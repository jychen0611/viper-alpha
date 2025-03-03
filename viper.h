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

/* Global structure to maintain all interface */
struct viper_content {
    /* Point to the list which contain all port interface */
    struct list_head port_list;
    /* Point to forwarding table */
    struct list_head fd_list;
};

/* Define type of interface */
enum viper_type { PORT, PC };

/* Port/PC interface pointed to by netdev_priv() */
struct viper_if {
    /* Point to the net device */
    struct net_device *ndev;
    /* Port ID */
    u32 port_id;
    /* RX queue */
    struct list_head rx_list;
    /* Interface type */
    enum viper_type type;
    union {
        /* Port interface */
        struct {
            /* Link from viper_content */
            struct list_head port_link;
            /* Point to the PCs which belong to this port */
            struct list_head pc_list;
        };
        /* PC interface */
        struct {
            /* Link from viper_content */
            struct list_head pc_link;
            /* Point to the port */
            struct viper_if *port;
        };
    };
};

/* Used for rx_queue */
struct viper_rx_pkt {
    /* Source MAC address */
    char src[ETH_ALEN];
    /* Destination MAC address */
    char dst[ETH_ALEN];
    /* Data */
    int datalen;
    u8 data[ETH_DATA_LEN];
    /* Examine the source type of packet */
    bool from_port;
    /* Link from viper_if */
    struct list_head rx_link;
};

struct forward_table {
    /* MAC address */
    char mac[ETH_ALEN];
    /* Port ID */
    int port_id;
    /* Time */
    struct timespec64 time;
    /* Link from viper_content */
    struct list_head fd_link;
};


/* Retrieve viper interface from net device */
static struct viper_if *ndev_get_viper_if(struct net_device *ndev);

/* Insert forwarding table */
static void fd_insert(char *mac, int id, struct timespec64 t);

/* Query Port ID from forwarding table */
static int fd_query(char *mac);

/* The packet reception function */
static int viper_rx(struct net_device *dev);

/* Put packet into destnation interface rx_queue */
static int viper_xmit(struct sk_buff *skb,
                      struct viper_if *dest,
                      bool from_port);

/* The packet transmission function (Callback for PC interface) */
static netdev_tx_t viper_start_xmit(struct sk_buff *skb,
                                    struct net_device *dev);

/* Open NIC */
static int viper_eth_open(struct net_device *netdev);

/* Close NIC */
static int viper_eth_stop(struct net_device *netdev);

/* Construct the port interface */
static int viper_add_port(int idx);

/* Construct the PC interface */
static int viper_add_pc(int idx);