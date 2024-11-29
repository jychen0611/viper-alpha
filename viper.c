#include <linux/etherdevice.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/version.h>

#define PORT_NAME "viperPORT%d"
#define PC_NAME "viperPC%d"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Elian Chen");
MODULE_DESCRIPTION("Virtual Ethernet Switch Driver");

/* Customize the number of port in this switch */
static int num_port = 2;
module_param(num_port, int, 0444);
MODULE_PARM_DESC(num_port, "Number of port in this virtual switch.");
u32 port_id = 0;
/* Customize the number of PC */
static int num_pc = 2;
module_param(num_pc, int, 0444);
MODULE_PARM_DESC(num_pc, "Number of PC.");

/* Global structure to maintain all interface */
struct viper_content {
    /* Point to the list which contain all port interface */
    struct list_head port_list;
    /* Point to the list which contain all PC interface */
    struct list_head pc_list;
};
/* Port interface pointed to by netdev_priv() */
struct port_if {
    /* Point to the net device */
    struct net_device *ndev;
    struct net_device_stats stats;
    /* ID */
    u32 port_id;
    /* Link from viper_content */
    struct list_head port_list;
};
/* PC interface pointed to by netdev_priv() */
struct pc_if {
    /* Point to the net device */
    struct net_device *ndev;
    struct net_device_stats stats;
    /* Port ID */
    u32 port_id;
    /* Link from viper_content */
    struct list_head pc_list;
};

static struct viper_content *viper = NULL;

/* Retrieve port interface from net device */
static inline struct port_if *ndev_get_viper_pif(struct net_device *ndev)
{
    /* Port interface pointed to by netdev_priv() */
    return (struct port_if *) netdev_priv(ndev);
}
/* Retrieve PC interface from net device */
static inline struct pc_if *ndev_get_viper_pcif(struct net_device *ndev)
{
    /* Port interface pointed to by netdev_priv() */
    return (struct pc_if *) netdev_priv(ndev);
}


// The packet reception function (switching logic with VLAN support)
static int viper_rx(struct sk_buff *skb, struct net_device *dev)
{
    /* FIXME: Forward the packet to the correct device */
    skb->dev = dev;
    skb->protocol = eth_type_trans(skb, dev);
    skb->ip_summed = CHECKSUM_UNNECESSARY; /* don't check it */
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 18, 0)
    return netif_rx_ni(skb);
#else
    return netif_rx(skb);
#endif
    /* FIXME: free skb (kfree_skb(skb);)*/
}

// The packet transmission function (transmitting logic with VLAN support)
static netdev_tx_t viper_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
    unsigned char *src_addr = eth_hdr(skb)->h_source;
    unsigned char *dst_addr = eth_hdr(skb)->h_dest;
    pr_info("viper: Transmission from %pM to %pM\n", dev->dev_addr, dst_addr);

    struct port_if *dest = NULL;
    /*
        if (is_broadcast_ether_addr(dst_addr)) {
            list_for_each_entry (dest, &viper->port_list, port_list) {
                if (!ether_addr_equal(dest->ndev->dev_addr, src_addr)) {
                    viper_rx(skb, dest->ndev);
                }
            }
        } else {
            list_for_each_entry (dest, &viper->port_list, port_list) {
                if (ether_addr_equal(dest->ndev->dev_addr, dst_addr)) {
                    viper_rx(skb, dest->ndev);
                }
            }
        }
    */

    // viper_rx(skb, skb->dev);
    return 0;
    // return dev_queue_xmit(skb);
}

/* Open NIC */
static int viper_eth_open(struct net_device *netdev)
{
    pr_info("viper: %s Ethernet device opened\n", netdev->name);
    netif_start_queue(netdev);
    return 0;
}

/* Close NIC */
static int viper_eth_stop(struct net_device *netdev)
{
    pr_info("viper: %s Ethernet device stopped\n", netdev->name);
    netif_stop_queue(netdev);
    return 0;
}


/* Net device operations */
static const struct net_device_ops viper_netdev_ops = {
    .ndo_start_xmit = viper_start_xmit,
    .ndo_open = viper_eth_open,
    .ndo_stop = viper_eth_stop,
    /* For simplicity, not implementing stats gathering here */
    .ndo_get_stats = NULL,
};

/* Construct the port interface */
static int viper_add_port(int idx)
{
    struct net_device *ndev = NULL;
    /* Allocate net device context */
    ndev = alloc_netdev(sizeof(struct port_if), PORT_NAME, NET_NAME_ENUM,
                        ether_setup);
    if (!ndev) {
        pr_err("viper: Couldn't allocate space for nedv");
        return -ENOMEM;
    }
    /* Assign viper operations */
    ndev->netdev_ops = &viper_netdev_ops;
    /* Close the ARP functionality */
    ndev->flags |= IFF_NOARP;
    /* FIXME: Support VLAN */


    /* Register net device with the kernel */
    if (register_netdev(ndev)) {
        pr_err("viper: Failed to register net device\n");
        free_netdev(ndev);
        return -ENODEV;
    }

    /* Fulfill the port interface */
    struct port_if *pif = ndev_get_viper_pif(ndev);
    pif->ndev = ndev;
    pif->port_id = port_id++;
    list_add_tail(&pif->port_list, &viper->port_list);
    pr_info("viper: New PORT %d\n", pif->port_id);
    return 0;
}

/* Construct the PC interface */
static int viper_add_pc(int idx)
{
    struct net_device *ndev = NULL;
    /* Allocate net device context */
    ndev =
        alloc_netdev(sizeof(struct pc_if), PC_NAME, NET_NAME_ENUM, ether_setup);
    if (!ndev) {
        pr_err("viper: Couldn't allocate space for nedv");
        return -ENOMEM;
    }
    /* Assign viper operations */
    ndev->netdev_ops = &viper_netdev_ops;

    /* Asign MAC address.
     * The first byte is '0x02' to avoid being a multicast
     * address.
     */
    char mac[ETH_ALEN];
    /* The first 3~24 bits are vendor identifier */
    mac[0] =
        0x02; /* Locally administered address, unicast (binary: 00000010) */
    mac[1] = 0xab;
    mac[2] = 0xcd;
    /* Assign uniqueness MAC address by idx */
    mac[3] = idx & (0xFF << 16);
    mac[4] = idx & (0xFF << 8);
    mac[5] = idx & 0xFF;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
    eth_hw_addr_set(ndev, mac);
#else
    memcpy(ndev->dev_addr, mac, ETH_ALEN);
#endif


    /* Register net device with the kernel */
    if (register_netdev(ndev)) {
        pr_err("viper: Failed to register net device\n");
        free_netdev(ndev);
        return -ENODEV;
    }

    /* Fulfill the port interface */
    struct pc_if *pcif = ndev_get_viper_pcif(ndev);
    pcif->ndev = ndev;
    if (num_port != 0)
        pcif->port_id = idx % num_port;
    list_add_tail(&pcif->pc_list, &viper->pc_list);
    pr_info("viper: New PC%d which belongs to PORT%d\n", idx, pcif->port_id);
    return 0;
}

/* Initialize viper */
static int __init viper_switch_init(void)
{
    /* Initialize global content */
    viper = kmalloc(sizeof(struct viper_content), GFP_KERNEL);
    if (!viper) {
        pr_err("viper: Couldn't allocate space for viper_content\n");
        return -ENOMEM;
    }
    /* Initialize the port interface */
    INIT_LIST_HEAD(&viper->port_list);
    for (int i = 0; i < num_port; ++i) {
        viper_add_port(i);
    }
    /* Initialize the PC interface */
    INIT_LIST_HEAD(&viper->pc_list);
    for (int i = 0; i < num_pc; ++i) {
        viper_add_pc(i);
    }


    pr_info("viper: Virtual Ethernet switch driver loaded\n");
    return 0;
}

/* Exit viper */
static void __exit viper_switch_exit(void)
{
    struct port_if *pif = NULL, *safe1 = NULL;
    list_for_each_entry_safe (pif, safe1, &viper->port_list, port_list) {
        unregister_netdev(pif->ndev);
        free_netdev(pif->ndev);
    }
    struct pc_if *pcif = NULL, *safe2 = NULL;
    list_for_each_entry_safe (pcif, safe2, &viper->pc_list, pc_list) {
        unregister_netdev(pcif->ndev);
        free_netdev(pcif->ndev);
    }
    kfree(viper);

    pr_info("viper: Virtual Ethernet switch driver unloaded\n");
}

module_init(viper_switch_init);
module_exit(viper_switch_exit);
