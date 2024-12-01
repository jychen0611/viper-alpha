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
    /* Point to forwarding table */
    struct list_head fd_list;
};

enum viper_type {PORT, PC};
/* Port/PC interface pointed to by netdev_priv() */
struct viper_if {
    /* Point to the net device */
    struct net_device *ndev;
    struct net_device_stats stats;
    /* Port ID */
    u32 port_id;
    /* RX queue */
    struct list_head rx_list;
    /* Interface type */
    enum viper_type type;
    union{
        /* Port interface */
        struct{
            /* Link from viper_content */
            struct list_head port_list;
        };
        /* PC interface */
        struct{
            /* Link from viper_content */
            struct list_head pc_list;
        };
    };
};

/* Used for rx_queue */
struct viper_rx_pkt {
    int datalen;
    u8 data[ETH_DATA_LEN];
    struct list_head rx_list;
    /* Examine the source of packet */
    bool from_port;
};

struct forward_table {
    /* MAC address */
    char mac[ETH_ALEN];
    /* Port ID */
    int port_id;
    /* Time */
    struct timespec64 time;
    /* Link from viper_content */
    struct list_head fd_list;
};

static struct viper_content *viper = NULL;

/* Retrieve viper interface from net device */
static inline struct viper_if *ndev_get_viper_if(struct net_device *ndev)
{
    /* Port interface pointed to by netdev_priv() */
    return (struct viper_if *) netdev_priv(ndev);
}

/* Insert forwarding table */
static inline void fd_insert(char *mac, int id, struct timespec64 t)
{
    struct forward_table *fd =
        kmalloc(sizeof(struct forward_table), GFP_KERNEL);
    memcpy(fd->mac, mac, ETH_ALEN);
    fd->port_id = id;
    fd->time.tv_sec = t.tv_sec;
    fd->time.tv_nsec = t.tv_nsec;
    list_add_tail(&fd->fd_list, &viper->fd_list);
}


static netdev_tx_t viper_start_xmit(struct sk_buff *skb, struct net_device *dev);
/* The packet reception function */
static int viper_rx(struct net_device *dev)
{
    struct viper_if *vif = ndev_get_viper_if(dev);
    if(list_empty(&vif->rx_list)){
        pr_err("viper: No packet in rx_queue\n");
        return -1;
    }

    struct viper_rx_pkt *pkt = list_first_entry(&vif->rx_list, struct viper_rx_pkt, rx_list);
    
    /* socket buffer will be sended to protocol stack */
    struct sk_buff *skb;
    /* socket buffer will be transmitted to another STA */
    struct sk_buff *skb1 = NULL;
    /* Put raw packet into socket buffer */
    skb = dev_alloc_skb(pkt->datalen + 2);
    if (!skb) {
        pr_err("viper: Ran out of memory allocating socket buffer\n");
        return -ENOMEM;
    }
    skb_reserve(skb, 2); /* align IP address on 16B boundary */
    memcpy(skb_put(skb, pkt->datalen), pkt->data, pkt->datalen);
    bool from_port = pkt->from_port;
    list_del(&pkt->rx_list);
    kfree(pkt);
    

    
    unsigned char *src_addr = eth_hdr(skb)->h_source;
    unsigned char *dst_addr = eth_hdr(skb)->h_dest;
    if(vif->type == PORT){
        
        if(!from_port){
            struct timespec64 t;
            ktime_get_real_ts64(&t);
            fd_insert(src_addr, ndev_get_viper_if(dev)->port_id, t); 
        }
        if (is_broadcast_ether_addr(dst_addr)) {
            pr_info("viper: is_broadcast_ether_addr\n");
            skb1 = skb_copy(skb, GFP_KERNEL);
        }
        /* Receiving a unicast packet */
        else {
            /* The packet is not intended for the AP itself. Instead, it is
             * sent to the destination STA and not passed to the protocol stack.
             */
            if (!ether_addr_equal(dst_addr, vif->ndev->dev_addr)) {
                skb1 = skb;
                skb = NULL;
            }
        }

        if (skb1) {
            pr_info("viper: %s forward:\n", vif->ndev->name);
            //vwifi_ndo_start_xmit(skb1, vif->ndev);
        }

        /* Nothing to pass to protocol stack */
        if (!skb)
            return 0;
    }
    /* FIXME: Forward the packet to the correct device */
    skb->dev = dev;
    skb->protocol = eth_type_trans(skb, dev);
    skb->ip_summed = CHECKSUM_UNNECESSARY; /* don't check it */

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 18, 0)
    return netif_rx_ni(skb);
#else
    return netif_rx(skb);
#endif

    return 0;
    /* FIXME: free skb (kfree_skb(skb);)*/
}

/* Put packet into destnation interface rx_queue */
static int viper_xmit(struct sk_buff *skb, struct viper_if *dest, bool from_port)
{
    struct viper_rx_pkt *pkt = NULL;
    pkt = kmalloc(sizeof(struct viper_rx_pkt), GFP_KERNEL);
    if (!pkt) {
        pr_warn("Ran out of memory allocating packet pool\n");
        return -ENOMEM;
    }
    memcpy(pkt->data, skb->data, skb->len);
    pkt->datalen = skb->len;
    pkt->from_port = from_port;
    list_add_tail(&pkt->rx_list, &dest->rx_list);

    /* Directly send to rx_queue, simulate the rx interrupt */
    viper_rx(dest->ndev);
    return 0;
}

/* The packet transmission function */
static netdev_tx_t viper_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
    unsigned char *src_addr = eth_hdr(skb)->h_source;
    unsigned char *dst_addr = eth_hdr(skb)->h_dest;
    pr_info("viper: Transmission from %pM to %pM\n", dev->dev_addr, dst_addr);

    struct viper_if *cur = ndev_get_viper_if(dev);
    struct viper_if *dest = NULL;
    if(cur->type == PORT){

    }else if(cur->type == PC){
        if (is_broadcast_ether_addr(dst_addr)) {
        
        }else{
            bool same_port = false;
            list_for_each_entry (dest, &viper->pc_list, pc_list) {
                if (dest->port_id == cur->port_id && ether_addr_equal(dest->ndev->dev_addr, dst_addr)) {
                    viper_xmit(skb, dest, false);
                    same_port = true;
                    break;
                }
            }
            if(!same_port){
                list_for_each_entry (dest, &viper->port_list, port_list) {
                    if (dest->port_id == cur->port_id) {
                        viper_xmit(skb, dest, false);
                        break;
                    }
                }
            }
        }
    }   

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
    ndev = alloc_netdev(sizeof(struct viper_if), PORT_NAME, NET_NAME_ENUM,
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
    struct viper_if *pif = ndev_get_viper_if(ndev);
    pif->ndev = ndev;
    pif->port_id = port_id++;
    pif->type = PORT;
    /* Initialize rx_queue */
    INIT_LIST_HEAD(&pif->rx_list);
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
        alloc_netdev(sizeof(struct viper_if), PC_NAME, NET_NAME_ENUM, ether_setup);
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
    struct viper_if *pcif = ndev_get_viper_if(ndev);
    pcif->ndev = ndev;
    if (num_port != 0)
        pcif->port_id = idx % num_port;
    pcif->type = PC;
    /* Initialize rx_queue */
    INIT_LIST_HEAD(&pcif->rx_list);
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
    INIT_LIST_HEAD(&viper->fd_list);

    pr_info("viper: Virtual Ethernet switch driver loaded\n");
    return 0;
}

/* Exit viper */
static void __exit viper_switch_exit(void)
{
    struct viper_if *pif = NULL, *safe1 = NULL;
    list_for_each_entry_safe (pif, safe1, &viper->port_list, port_list) {
        struct viper_rx_pkt *pkt = NULL, *safe = NULL;
        list_for_each_entry_safe (pkt, safe, &pif->rx_list, rx_list) {
            list_del(&pkt->rx_list);
            kfree(pkt);
        }
        unregister_netdev(pif->ndev);
        free_netdev(pif->ndev);
    }
    struct viper_if *pcif = NULL, *safe2 = NULL;
    list_for_each_entry_safe (pcif, safe2, &viper->pc_list, pc_list) {
        struct viper_rx_pkt *pkt = NULL, *safe = NULL;
        list_for_each_entry_safe (pkt, safe, &pcif->rx_list, rx_list) {
            list_del(&pkt->rx_list);
            kfree(pkt);
        }
        unregister_netdev(pcif->ndev);
        free_netdev(pcif->ndev);
    }
    struct forward_table *fd = NULL, *safe3 = NULL;
    list_for_each_entry_safe (fd, safe3, &viper->fd_list, fd_list) {
        pr_info("viper: | MAC %pM | PORT %d | TIME %lld.%09ld (sec) |\n",
                fd->mac, fd->port_id, (s64) fd->time.tv_sec, fd->time.tv_nsec);
        kfree(fd);
    }
    kfree(viper);

    pr_info("viper: Virtual Ethernet switch driver unloaded\n");
}

module_init(viper_switch_init);
module_exit(viper_switch_exit);
