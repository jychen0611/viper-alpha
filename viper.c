#include <linux/etherdevice.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/version.h>

static struct net_device *veth0, *veth1;
static struct net_device_stats veth_stats0, veth_stats1;

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
static int veth_rx(struct sk_buff *skb, struct net_device *dev)
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
        printk(KERN_WARNING "viper: MAC address not found in FDB for VLAN %u,
    dropping packet.\n", vlan_id); dev_kfree_skb(skb); return NET_RX_DROP;
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
static netdev_tx_t veth_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
    unsigned char *mac_src = eth_hdr(skb)->h_source;
    u16 vlan_id = 0;

    // Check if the packet has a VLAN tag and extract the VLAN ID
    if (skb_vlan_tag_present(skb)) {
        vlan_id = skb_vlan_tag_get(skb);
    }

    // Learn the source MAC address and VLAN ID
    fdb_add(mac_src, dev, vlan_id);

    // Simply forward the packet to the other device (veth0 or veth1)
    if (dev == veth0) {
        skb->dev = veth1;
    } else {
        skb->dev = veth0;
    }
    veth_rx(skb, skb->dev);
    return 0;
    // return dev_queue_xmit(skb);
}

/* Open NIC */
static int viper_eth_open(struct net_device *netdev)
{
    printk(KERN_INFO "viper: %s Ethernet device opened\n", netdev->name);
    netif_start_queue(netdev);
    return 0;
}

/* Close NIC */
static int viper_eth_stop(struct net_device *netdev)
{
    printk(KERN_INFO "viper: %s Ethernet device stopped\n", netdev->name);
    netif_stop_queue(netdev);
    return 0;
}


// Network device operations
static const struct net_device_ops veth_netdev_ops = {
    .ndo_start_xmit = veth_start_xmit,
    .ndo_open = viper_eth_open,
    .ndo_stop = viper_eth_stop,
    .ndo_get_stats =
        NULL,  // For simplicity, not implementing stats gathering here
};

// Initialize virtual Ethernet devices with VLAN support
static int __init veth_switch_init(void)
{
    veth0 = alloc_netdev(0, "veth0", NET_NAME_UNKNOWN, ether_setup);
    veth1 = alloc_netdev(0, "veth1", NET_NAME_UNKNOWN, ether_setup);

    if (!veth0 || !veth1) {
        printk(KERN_ERR "viper: Failed to allocate network devices\n");
        return -ENOMEM;
    }

    // Set up the network devices
    veth0->netdev_ops = &veth_netdev_ops;
    veth1->netdev_ops = &veth_netdev_ops;

    // Register devices with the kernel
    if (register_netdev(veth0)) {
        printk(KERN_ERR "viper: Failed to register veth0\n");
        return -ENODEV;
    }
    if (register_netdev(veth1)) {
        printk(KERN_ERR "viper: Failed to register veth1\n");
        unregister_netdev(veth0);
        return -ENODEV;
    }

    printk(KERN_INFO
           "viper: Virtual Ethernet switch driver with VLAN support loaded\n");
    return 0;
}

// Cleanup the virtual devices
static void __exit veth_switch_exit(void)
{
    unregister_netdev(veth0);
    unregister_netdev(veth1);
    free_netdev(veth0);
    free_netdev(veth1);

    printk(KERN_INFO "viper: Virtual Ethernet switch driver unloaded\n");
}

module_init(veth_switch_init);
module_exit(veth_switch_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Elian");
MODULE_DESCRIPTION("Virtual Ethernet Switch Driver");