#include <linux/etherdevice.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/version.h>

#define PORT_NAME "viper%d"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Elian Chen");
MODULE_DESCRIPTION("Virtual Ethernet Switch Driver");

/* Customize the number of port in this switch */
static int num_port = 4;
module_param(num_port, int, 0444);
MODULE_PARM_DESC(num_port, "Number of port in this virtual switch.");


/* Global structure to maintain all interface */
struct viper_content{
    /* Point to the list which contain all port interface */
    struct list_head port_list;
};
/* Port interface pointed to by netdev_priv() */
struct port_if{
    /* Point to the net device */
    struct net_device *ndev;
    struct net_device_stats stats;
    /* Link from viper_content */
    struct list_head port_list;
};
static struct viper_content *viper = NULL; 

/* Retrieve port interface from net device */
static inline struct port_if* ndev_get_viper_pif(struct net_device *ndev)
{
    /* Port interface pointed to by netdev_priv() */
    return (struct port_if *) netdev_priv(ndev);
}

// Forwarding Database (FDB) to simulate MAC address learning
#define FDB_SIZE 256
static struct {
    unsigned char mac[ETH_ALEN];
    struct net_device *dev;
    u16 vlan_id;  // VLAN ID associated with the entry
} fdb[FDB_SIZE];

static int fdb_lookup(const unsigned char *mac, u16 vlan_id)
{
    for (int i = 0; i < FDB_SIZE; i++) {
        if (fdb[i].dev && memcmp(fdb[i].mac, mac, ETH_ALEN) == 0 &&
            fdb[i].vlan_id == vlan_id) {
            return i;
        }
    }
    return -1;
}

static void fdb_add(const unsigned char *mac,
                    struct net_device *dev,
                    u16 vlan_id)
{
    int i;
    for (i = 0; i < FDB_SIZE; i++) {
        if (fdb[i].dev == NULL) {
            memcpy(fdb[i].mac, mac, ETH_ALEN);
            fdb[i].dev = dev;
            fdb[i].vlan_id = vlan_id;
            return;
        }
    }
}

// The packet reception function (switching logic with VLAN support)
static int viper_rx(struct sk_buff *skb, struct net_device *dev)
{
    unsigned char *mac_dst = eth_hdr(skb)->h_dest;
    struct net_device *out_dev = NULL;
    u16 vlan_id = 0;

    // Check if the packet has a VLAN tag and extract the VLAN ID
    if (skb_vlan_tag_present(skb)) {
        vlan_id = skb_vlan_tag_get(skb);
    }

    // Look up the MAC address in the forwarding database (FDB) with VLAN ID
    int index = fdb_lookup(mac_dst, vlan_id);
    if (index >= 0) {
        out_dev = fdb[index].dev;
    }

    /*if (!out_dev) {
        pr_warn("viper: MAC address not found in FDB for VLAN %u, dropping packet.\n", vlan_id); dev_kfree_skb(skb); return NET_RX_DROP;
    }*/

    // Forward the packet to the correct device
    // skb->dev = out_dev;
    skb->dev = dev;
    skb->protocol = eth_type_trans(skb, dev);
    skb->ip_summed = CHECKSUM_UNNECESSARY; /* don't check it */
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 18, 0)
    return netif_rx_ni(skb);
#else
    return netif_rx(skb);
#endif
}

// The packet transmission function (transmitting logic with VLAN support)
static netdev_tx_t viper_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
    unsigned char *mac_src = eth_hdr(skb)->h_source;
    u16 vlan_id = 0;
    pr_info("viper: src| %pM\n", mac_src);
    // Check if the packet has a VLAN tag and extract the VLAN ID
    if (skb_vlan_tag_present(skb)) {
        vlan_id = skb_vlan_tag_get(skb);
    }

    // Learn the source MAC address and VLAN ID
    fdb_add(mac_src, dev, vlan_id);
    
    //viper_rx(skb, skb->dev);
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

static int viper_add_port(int idx){
    struct net_device *ndev = NULL;
    /* Allocate net device context */
    ndev = alloc_netdev(sizeof(struct port_if), PORT_NAME, NET_NAME_ENUM,
                        ether_setup);
    if (!ndev){
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
    mac[0] = 0x02; /* Locally administered address, unicast (binary: 00000010) */
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
        return -ENODEV;
    }

    /* Fulfill the port interface */
    struct port_if *pif = ndev_get_viper_pif(ndev);
    pif->ndev = ndev;
    list_add_tail(&pif->port_list, &viper->port_list);
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
    INIT_LIST_HEAD(&viper->port_list);
    for (int i=0;i<num_port;++i){
        viper_add_port(i);
    }

    pr_info("viper: Virtual Ethernet switch driver loaded\n");
    return 0;
}

/* Exit viper */
static void __exit viper_switch_exit(void)
{
    struct port_if *pif=NULL, *safe=NULL;
    list_for_each_entry_safe(pif, safe, &viper->port_list, port_list){
        unregister_netdev(pif->ndev);
        free_netdev(pif->ndev);
    }
    kfree(viper);

    pr_info("viper: Virtual Ethernet switch driver unloaded\n");
}

module_init(viper_switch_init);
module_exit(viper_switch_exit);

