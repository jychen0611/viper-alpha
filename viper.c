#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/init.h>
#include <linux/skbuff.h>
#include <linux/interrupt.h>

/* Driver name */
#define DRIVER_NAME "viper"

/* Virtual network interface card (NIC) structure */
struct viper_eth_vif {
    struct net_device *netdev;
    struct napi_struct napi;
    /* Fix me: Add other information */
};

/* Open NIC */
static int viper_eth_open(struct net_device *netdev)
{
    printk(KERN_INFO "%s: Ethernet device opened\n", netdev->name);
    netif_start_queue(netdev);
    return 0;
}

/* Close NIC */
static int viper_eth_stop(struct net_device *netdev)
{
    printk(KERN_INFO "%s: Ethernet device stopped\n", netdev->name);
    netif_stop_queue(netdev);
    return 0;
}

/* Transmit packet */
static netdev_tx_t viper_eth_start_xmit(struct sk_buff *skb, struct net_device *netdev)
{
    /* Process the sent packets */
    printk(KERN_INFO "%s: Transmitting packet\n", netdev->name);

    /* Simulation as the transmission completed */
    dev_kfree_skb(skb); /* Release packet memory */
    netdev->stats.tx_packets++;
    netdev->stats.tx_bytes += skb->len;

    return NETDEV_TX_OK;
}

/* Process network card statistics */
static struct net_device_stats *viper_eth_get_stats(struct net_device *netdev)
{
    return &netdev->stats;
}

/* NIC operations */
static const struct net_device_ops viper_eth_netdev_ops = {
    .ndo_open = viper_eth_open,
    .ndo_stop = viper_eth_stop,
    .ndo_start_xmit = viper_eth_start_xmit,
    .ndo_get_stats = viper_eth_get_stats,
};

/* Initialize NIC */
static void viper_eth_setup(struct net_device *netdev)
{
    ether_setup(netdev); /* Initialize ethernet device */

    netdev->netdev_ops = &viper_eth_netdev_ops;
    netdev->flags |= IFF_NOARP;
    netdev->features |= NETIF_F_HW_CSUM;
}

/* Initialize module */
static int __init viper_eth_init(void)
{
    struct net_device *netdev;
    struct viper_eth_vif *vif;

    /* Allocate network device */
    //netdev = alloc_etherdev(sizeof(struct viper_eth_vif));
    netdev = alloc_netdev(0, DRIVER_NAME"%d", NET_NAME_UNKNOWN, viper_eth_setup);
    if (!netdev) {
        printk(KERN_ERR "Failed to allocate Ethernet device\n");
        return -ENOMEM;
    }

    /* Initialize virtual interface */
    vif = netdev_priv(netdev);
    vif->netdev = netdev;

    /* Set NIC */
    //viper_eth_setup(netdev);

    /* Register NIC */
    if (register_netdev(netdev)) {
        printk(KERN_ERR "Failed to register Ethernet device\n");
        free_netdev(netdev);
        return -ENODEV;
    }

    printk(KERN_INFO "Viper initialized\n");
    return 0;
}

/* Exit module */
static void __exit viper_eth_exit(void)
{
    struct viper_eth_vif *vif;
    struct net_device *netdev;

    netdev = vif->netdev;

    unregister_netdev(netdev); /* Unregister NIC */
    free_netdev(netdev);       /* Release NIC memory */
    printk(KERN_INFO "Viper exited\n");
}

module_init(viper_eth_init);
module_exit(viper_eth_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Elian Chen");
MODULE_DESCRIPTION("Virtual Ethernet Driver");
