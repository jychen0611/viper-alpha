#include "viper.h"

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Virtual Ethernet Switch Driver");

/* Customize the number of port in this switch */
static int num_port = 2;
module_param(num_port, int, 0444);
MODULE_PARM_DESC(num_port, "Number of port in this virtual switch.");

/* Customize the number of PC */
static int num_pc = 2;
module_param(num_pc, int, 0444);
MODULE_PARM_DESC(num_pc, "Number of PC.");

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
    /* Remove duplicate enrty */
    struct forward_table *tmp = NULL;
    bool del = false;
    list_for_each_entry (tmp, &viper->fd_list, fd_link) {
        if (ether_addr_equal(fd->mac, tmp->mac)) {
            del = true;
            break;
        }
    }
    if (del) {
        list_del(&tmp->fd_link);
        kfree(tmp);
    }
    /* Add new entry to tail */
    list_add_tail(&fd->fd_link, &viper->fd_list);
}

/* Query Port ID from forwarding table */
static inline int fd_query(char *mac)
{
    struct forward_table *tmp = NULL;
    list_for_each_entry (tmp, &viper->fd_list, fd_link) {
        if (ether_addr_equal(mac, tmp->mac)) {
            pr_info("viper: [HIT] %pM\n", mac);
            return tmp->port_id;
        }
    }
    /* If doesn't find the match entry, then return -1 */
    pr_info("viper: [MISS] %pM\n", mac);
    return -1;
}

/* The packet reception function */
static int viper_rx(struct net_device *dev)
{
    struct viper_if *vif = ndev_get_viper_if(dev);
    if (list_empty(&vif->rx_list)) {
        pr_err("viper: No packet in rx_queue\n");
        return -1;
    }
    /* Handle packet and put into socket buffer */
    struct viper_rx_pkt *pkt =
        list_first_entry(&vif->rx_list, struct viper_rx_pkt, rx_link);

    /* socket buffer will be sended to protocol stack or forwarding */
    struct sk_buff *skb;
    /* Put raw packet into socket buffer */
    skb = dev_alloc_skb(pkt->datalen + 2);
    if (!skb) {
        pr_err(
            "viper: Ran out of memory allocating socket buffer, packet "
            "dropped\n");
        list_del(&pkt->rx_link);
        kfree(pkt);
        return -ENOMEM;
    }
    memcpy(eth_hdr(skb)->h_source, pkt->src, ETH_ALEN);
    memcpy(eth_hdr(skb)->h_dest, pkt->dst, ETH_ALEN);
    skb_reserve(skb, 2); /* align IP address on 16B boundary */
    memcpy(skb_put(skb, pkt->datalen), pkt->data, pkt->datalen);
    bool from_port = pkt->from_port;

    list_del(&pkt->rx_link);
    kfree(pkt);

    /* Port forwarding the packet */
    char *src_addr = eth_hdr(skb)->h_source;
    char *dst_addr = eth_hdr(skb)->h_dest;
    if (vif->type == PORT) {
        if (!from_port) {
            /* Update the forwarding table */
            struct timespec64 t;
            ktime_get_real_ts64(&t);
            fd_insert(eth_hdr(skb)->h_source, ndev_get_viper_if(dev)->port_id,
                      t);

            int target_port = -1;
            if (!is_broadcast_ether_addr(dst_addr))
                target_port = fd_query(dst_addr);
            if (target_port != -1) {
                /* Forward to target Port */
                struct viper_if *dest = NULL;
                list_for_each_entry (dest, &viper->port_list, port_link) {
                    if (dest->port_id == target_port) {
                        pr_info("viper: [Forwarding] %s --> %s\n",
                                vif->ndev->name, dest->ndev->name);
                        viper_xmit(skb, dest, true);
                        break;
                    }
                }
            } else {
                /* Broadcast to other Ports */
                struct viper_if *dest = NULL;
                list_for_each_entry (dest, &viper->port_list, port_link) {
                    if (dest->port_id != vif->port_id) {
                        pr_info("viper: [Forwarding] %s --> %s\n",
                                vif->ndev->name, dest->ndev->name);
                        viper_xmit(skb, dest, true);
                    }
                }
            }
        } else {
            /* Transmit to destination PC */
            struct viper_if *dest = NULL;
            if (is_broadcast_ether_addr(dst_addr)) {
                list_for_each_entry (dest, &vif->pc_list, pc_link) {
                    /* Broadcast to all PCs */
                    pr_info("viper: [Egress] %s --> %s\n", vif->ndev->name,
                            dest->ndev->name);
                    viper_xmit(skb, dest, true);
                }
            } else {
                bool send = false;
                list_for_each_entry (dest, &vif->pc_list, pc_link) {
                    if (ether_addr_equal(dst_addr, dest->ndev->dev_addr)) {
                        pr_info("viper: [Egress] %s --> %s\n", vif->ndev->name,
                                dest->ndev->name);
                        viper_xmit(skb, dest, true);
                        send = true;
                        break;
                    }
                }
                if (!send) {
                    /* Drop it */
                    pr_info("viper: %s drop packet\n", vif->ndev->name);
                }
            }
        }
        /* Don't forget to cleanup skb */
        kfree_skb(skb);
        return 0;
    }
    /* PC receive the packet */
    pr_info("viper: %s received packet from %pM\n", vif->ndev->name, src_addr);
    skb->dev = dev;
    skb->protocol = eth_type_trans(skb, dev);
    skb->ip_summed = CHECKSUM_UNNECESSARY; /* don't check it */

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 18, 0)
    return netif_rx_ni(skb);
#else
    return netif_rx(skb);
#endif

    return 0;
}

/* Put packet into destnation interface rx_queue */
static int viper_xmit(struct sk_buff *skb,
                      struct viper_if *dest,
                      bool from_port)
{
    struct viper_rx_pkt *pkt = NULL;
    pkt = kmalloc(sizeof(struct viper_rx_pkt), GFP_KERNEL);
    if (!pkt) {
        pr_warn("Ran out of memory allocating packet pool\n");
        return -ENOMEM;
    }
    char *src = eth_hdr(skb)->h_source;
    char *dst = eth_hdr(skb)->h_dest;
    memcpy(pkt->src, src, ETH_ALEN);
    memcpy(pkt->dst, dst, ETH_ALEN);
    memcpy(pkt->data, skb->data, skb->len);
    pkt->datalen = skb->len;
    pkt->from_port = from_port;
    list_add_tail(&pkt->rx_link, &dest->rx_list);

    /* Directly send to rx_queue, simulate the rx interrupt */
    viper_rx(dest->ndev);
    return 0;
}

/* The packet transmission function (Callback for PC interface) */
static netdev_tx_t viper_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
    // unsigned char *src_addr = eth_hdr(skb)->h_source;
    unsigned char *dst_addr = eth_hdr(skb)->h_dest;
    pr_info("viper: %s Start Xmit to %pM\n", dev->name, dst_addr);

    struct viper_if *pcif = ndev_get_viper_if(dev);
    struct viper_if *dest = NULL;

    if (is_broadcast_ether_addr(dst_addr)) {
        /* Broadcast to PC */
        list_for_each_entry (dest, &pcif->port->pc_list, pc_link) {
            if (!ether_addr_equal(dest->ndev->dev_addr, pcif->ndev->dev_addr)) {
                pr_info("viper: [Broadcast] %s --> %s\n", pcif->ndev->name,
                        dest->ndev->name);
                viper_xmit(skb, dest, false);
            }
        }
        /* Broadcast to port */
        dest = pcif->port;
        pr_info("viper: [Broadcast] %s (ingress)--> %s\n", pcif->ndev->name,
                dest->ndev->name);
        viper_xmit(skb, dest, false);

    } else {
        /* Check if src and dst in the same LAN */
        bool same_port = false;
        /* Direct send packet to destnation PC */
        list_for_each_entry (dest, &pcif->port->pc_list, pc_link) {
            if (ether_addr_equal(dest->ndev->dev_addr, dst_addr)) {
                pr_info("viper: [Unicast] %s --> %s\n", pcif->ndev->name,
                        dest->ndev->name);
                viper_xmit(skb, dest, false);
                same_port = true;
                break;
            }
        }
        /* Forward to switch */
        if (!same_port) {
            dest = pcif->port;
            pr_info("viper: [Unicast] %s (ingress)--> %s\n", pcif->ndev->name,
                    dest->ndev->name);
            viper_xmit(skb, dest, false);
        }
    }


    /* Don't forget to cleanup skb, as its ownership moved to xmit callback. */
    dev_kfree_skb(skb);
    return NETDEV_TX_OK;
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
    /* Close the ARP functionality (because port doesn't need that) */
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
    pif->port_id = idx;
    pif->type = PORT;
    /* Initialize rx_queue */
    INIT_LIST_HEAD(&pif->rx_list);
    /* Initialize PC list */
    INIT_LIST_HEAD(&pif->pc_list);
    /* Add this new port interface to global port_list */
    list_add_tail(&pif->port_link, &viper->port_list);
    pr_info("viper: New PORT %d\n", pif->port_id);
    return 0;
}

/* Construct the PC interface */
static int viper_add_pc(int idx)
{
    struct net_device *ndev = NULL;
    /* Allocate net device context */
    ndev = alloc_netdev(sizeof(struct viper_if), PC_NAME, NET_NAME_ENUM,
                        ether_setup);
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
    mac[3] = (idx >> 16) & 0xFF;
    mac[4] = (idx >> 8) & 0xFF;
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

    /* Fulfill the PC interface */
    struct viper_if *pcif = ndev_get_viper_if(ndev);
    pcif->ndev = ndev;
    pcif->type = PC;
    /* Initialize rx_queue */
    INIT_LIST_HEAD(&pcif->rx_list);
    /* Assign PC to the corresponding Port */
    if (num_port != 0)
        pcif->port_id = idx % num_port;
    /* Add PC to the corresponding Port */
    struct viper_if *port = NULL;
    list_for_each_entry (port, &viper->port_list, port_link) {
        if (port->port_id == pcif->port_id) {
            pcif->port = port;
            list_add_tail(&pcif->pc_link, &port->pc_list);
            break;
        }
    }
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
    for (int i = 0; i < num_pc; ++i) {
        viper_add_pc(i);
    }
    /* Initialize the forwarding table header */
    INIT_LIST_HEAD(&viper->fd_list);

    pr_info("viper: Virtual Ethernet switch driver loaded\n");
    return 0;
}

/* Exit viper */
static void __exit viper_switch_exit(void)
{
    /* Free Port interface in switch */
    struct viper_if *pif = NULL, *safe1 = NULL;
    list_for_each_entry_safe (pif, safe1, &viper->port_list, port_link) {
        /* Free RX queue */
        struct viper_rx_pkt *pkt = NULL, *safe = NULL;
        list_for_each_entry_safe (pkt, safe, &pif->rx_list, rx_link) {
            list_del(&pkt->rx_link);
            kfree(pkt);
        }
        /* Free PC list in corresponding Port */
        struct viper_if *pcif = NULL, *safe2 = NULL;
        list_for_each_entry_safe (pcif, safe2, &pif->pc_list, pc_link) {
            struct viper_rx_pkt *pkt = NULL, *safe = NULL;
            list_for_each_entry_safe (pkt, safe, &pcif->rx_list, rx_link) {
                list_del(&pkt->rx_link);
                kfree(pkt);
            }
            unregister_netdev(pcif->ndev);
            free_netdev(pcif->ndev);
        }

        unregister_netdev(pif->ndev);
        free_netdev(pif->ndev);
    }
    /* Free forwarding table */
    struct forward_table *fd = NULL, *safe3 = NULL;
    list_for_each_entry_safe (fd, safe3, &viper->fd_list, fd_link) {
        pr_info("viper: | MAC %pM | PORT %d | TIME %lld.%09ld (sec) |\n",
                fd->mac, fd->port_id, (s64) fd->time.tv_sec, fd->time.tv_nsec);
        kfree(fd);
    }
    kfree(viper);

    pr_info("viper: Virtual Ethernet switch driver unloaded\n");
}

module_init(viper_switch_init);
module_exit(viper_switch_exit);
