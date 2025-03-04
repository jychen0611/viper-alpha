# viper: A Virtual Ether-Switch for Linux.

<img src="viper.png" width="20%"/>

## vscode c_cpp_properties

ctrl+shift+p --> c/c++ edit configuration --> c_cpp_properties.json

```shell
$ ls /usr/src
linux-headers-6.8.0-40-generic  linux-hwe-6.8-headers-6.8.0-40
linux-headers-6.8.0-49-generic  linux-hwe-6.8-headers-6.8.0-49
$ uname -r
6.8.0-49-generic
$ gcc --version
gcc (Ubuntu 11.4.0-1ubuntu1~22.04) 11.4.0
Copyright (C) 2021 Free Software Foundation, Inc.
This is free software; see the source for copying conditions.  There is NO
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
```
`c_cpp_properties.json`
```json
{
    "configurations": [
        {
            "name": "Linux",
            "includePath": [
                "${workspaceFolder}/**",
                "/usr/include",
                "/usr/local/include",
                "/usr/src/linux-headers-6.8.0-49-generic/include",
                "/usr/src/linux-headers-6.8.0-49-generic/arch/x86/include",
                "/usr/src/linux-headers-6.8.0-49-generic/arch/x86/include/generated",
                "/usr/lib/gcc"
            ],
            "defines": [
                "__GNUC__",
                "__KERNEL__",
                "MODULE"
            ],
            "compilerPath": "/usr/bin/gcc",
            "cStandard": "c99",
            "cppStandard": "c++98",
            "intelliSenseMode": "linux-gcc-x64"
        }
    ],
    "version": 4
}
```

## clang-format

```shell
$ sudo apt install clang-format
```
automatically format C and header files in the current directory
```shell
$ clang-format -i *.[ch]
```

## test
```shell
$ chmod +x ./tools/test.sh
$ sudo ./tools/test.sh
```
```shell
Installing Module viper.ko
1: lo: <LOOPBACK,UP,LOWER_UP> mtu 65536 qdisc noqueue state UNKNOWN mode DEFAULT group default qlen 1000
    link/loopback 00:00:00:00:00:00 brd 00:00:00:00:00:00
2: enp0s3: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc fq_codel state UP mode DEFAULT group default qlen 1000
    link/ether 08:00:27:de:40:b9 brd ff:ff:ff:ff:ff:ff
3: viperPORT0: <BROADCAST,MULTICAST,NOARP,UP,LOWER_UP> mtu 1500 qdisc fq_codel state UNKNOWN mode DEFAULT group default qlen 1000
    link/ether 00:00:00:00:00:00 brd ff:ff:ff:ff:ff:ff
4: viperPORT1: <BROADCAST,MULTICAST,NOARP,UP,LOWER_UP> mtu 1500 qdisc fq_codel state UNKNOWN mode DEFAULT group default qlen 1000
    link/ether 00:00:00:00:00:00 brd ff:ff:ff:ff:ff:ff
5: viperPC0: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc fq_codel state UNKNOWN mode DEFAULT group default qlen 1000
    link/ether 02:ab:cd:00:00:00 brd ff:ff:ff:ff:ff:ff
6: viperPC1: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc fq_codel state UNKNOWN mode DEFAULT group default qlen 1000
    link/ether 02:ab:cd:00:00:01 brd ff:ff:ff:ff:ff:ff

================================================================================
Ping Test: Port0 viperPC0 (10.0.0.1) <--> PORT1 viperPC1 (10.0.0.2)

(should succeed)
(be patient, it will take some time to route...)
================================================================================
PING 10.0.0.2 (10.0.0.2) 56(84) bytes of data.
64 bytes from 10.0.0.2: icmp_seq=1 ttl=64 time=0.132 ms

--- 10.0.0.2 ping statistics ---
1 packets transmitted, 1 received, 0% packet loss, time 0ms
rtt min/avg/max/mdev = 0.132/0.132/0.132/0.000 ms
Removing Module viper
Test Passed
```
```shell
$ sudo dmesg
```
```shell
[ 1604.668304] viper: loading out-of-tree module taints kernel.
[ 1604.668317] viper: module verification failed: signature and/or required key missing - tainting kernel
[ 1604.672793] viper: New PORT 0
[ 1604.673031] viper: New PORT 1
[ 1604.673265] viper: New PC0 which belongs to PORT0
[ 1604.674146] viper: New PC1 which belongs to PORT1
[ 1604.674184] viper: Virtual Ethernet switch driver loaded
[ 1604.697581] viper: viperPORT1 Ethernet device opened
[ 1604.702779] viper: viperPC0 Ethernet device opened

/* Broadcast */
[ 1604.709320] viper: viperPC0 Start Xmit to ff:ff:ff:ff:ff:ff
[ 1604.709328] viper: [Broadcast] viperPC0 (ingress)--> viperPORT0
[ 1604.709345] viper: [Forwarding] viperPORT0 --> viperPORT1
[ 1604.709349] viper: [Egress] viperPORT1 --> viperPC1
[ 1604.709358] viper: viperPC1 received packet from 02:ab:cd:00:00:00
[ 1604.714905] viper: viperPC1 Ethernet device opened
[ 1604.718744] viper: viperPORT0 Ethernet device opened

/* Multicast */
[ 1604.721533] viper: viperPC0 Start Xmit to 33:33:00:00:00:16
[ 1604.721545] viper: [Unicast] viperPC0 (ingress)--> viperPORT0
[ 1604.721571] viper: [MISS] 33:33:00:00:00:16
[ 1604.721574] viper: [Forwarding] viperPORT0 --> viperPORT1
[ 1604.721581] viper: viperPORT1 drop packet

/* Broadcast */
[ 1604.724324] viper: viperPC1 Start Xmit to ff:ff:ff:ff:ff:ff
[ 1604.724330] viper: [Broadcast] viperPC1 (ingress)--> viperPORT1
[ 1604.724334] viper: [Forwarding] viperPORT1 --> viperPORT0
[ 1604.724336] viper: [Egress] viperPORT0 --> viperPC0
[ 1604.724339] viper: viperPC0 received packet from 02:ab:cd:00:00:01

/* Multicast */
[ 1604.728293] viper: viperPC1 Start Xmit to 33:33:00:00:00:16
[ 1604.728305] viper: [Unicast] viperPC1 (ingress)--> viperPORT1
[ 1604.728314] viper: [MISS] 33:33:00:00:00:16
[ 1604.728317] viper: [Forwarding] viperPORT1 --> viperPORT0
[ 1604.728323] viper: viperPORT0 drop packet

/* Multicast */
[ 1604.878080] viper: viperPC1 Start Xmit to 33:33:00:00:00:16
[ 1604.878102] viper: [Unicast] viperPC1 (ingress)--> viperPORT1
[ 1604.878115] viper: [MISS] 33:33:00:00:00:16
[ 1604.878118] viper: [Forwarding] viperPORT1 --> viperPORT0
[ 1604.878126] viper: viperPORT0 drop packet

/* Multicast */
[ 1604.971567] viper: viperPC0 Start Xmit to 33:33:00:00:00:16
[ 1604.971594] viper: [Unicast] viperPC0 (ingress)--> viperPORT0
[ 1604.971605] viper: [MISS] 33:33:00:00:00:16
[ 1604.971606] viper: [Forwarding] viperPORT0 --> viperPORT1
[ 1604.971615] viper: viperPORT1 drop packet

/* Multicast */
[ 1605.091114] viper: viperPC0 Start Xmit to 33:33:ff:a7:a8:da
[ 1605.091152] viper: [Unicast] viperPC0 (ingress)--> viperPORT0
[ 1605.091230] viper: [MISS] 33:33:ff:a7:a8:da
[ 1605.091232] viper: [Forwarding] viperPORT0 --> viperPORT1
[ 1605.091237] viper: viperPORT1 drop packet

[ 1605.292763] viper: viperPC0 Ethernet device stopped
[ 1605.329783] viper: viperPC1 Ethernet device stopped
[ 1605.382481] viper: viperPC0 Ethernet device opened

/* Multicast */
[ 1605.386415] viper: viperPC0 Start Xmit to 33:33:00:00:00:16
[ 1605.386423] viper: [Unicast] viperPC0 (ingress)--> viperPORT0
[ 1605.386440] viper: [MISS] 33:33:00:00:00:16
[ 1605.386442] viper: [Forwarding] viperPORT0 --> viperPORT1
[ 1605.386445] viper: viperPORT1 drop packet

[ 1605.402614] viper: viperPC1 Ethernet device opened

/* Multicast */
[ 1605.406411] viper: viperPC1 Start Xmit to 33:33:00:00:00:16
[ 1605.406417] viper: [Unicast] viperPC1 (ingress)--> viperPORT1
[ 1605.406423] viper: [MISS] 33:33:00:00:00:16
[ 1605.406424] viper: [Forwarding] viperPORT1 --> viperPORT0
[ 1605.406427] viper: viperPORT0 drop packet

/* ARP request */
[ 1605.461442] viper: viperPC0 Start Xmit to ff:ff:ff:ff:ff:ff
[ 1605.461452] viper: [Broadcast] viperPC0 (ingress)--> viperPORT0
[ 1605.461458] viper: [Forwarding] viperPORT0 --> viperPORT1
[ 1605.461462] viper: [Egress] viperPORT1 --> viperPC1
[ 1605.461475] viper: viperPC1 received packet from 02:ab:cd:00:00:00

/* ARP respond */
[ 1605.461503] viper: viperPC1 Start Xmit to 02:ab:cd:00:00:00
[ 1605.461505] viper: [Unicast] viperPC1 (ingress)--> viperPORT1
[ 1605.461507] viper: [HIT] 02:ab:cd:00:00:00
[ 1605.461509] viper: [Forwarding] viperPORT1 --> viperPORT0
[ 1605.461513] viper: [Egress] viperPORT0 --> viperPC0
[ 1605.461515] viper: viperPC0 received packet from 02:ab:cd:00:00:01

/* Ping request */
[ 1605.461522] viper: viperPC0 Start Xmit to 02:ab:cd:00:00:01
[ 1605.461523] viper: [Unicast] viperPC0 (ingress)--> viperPORT0
[ 1605.461526] viper: [HIT] 02:ab:cd:00:00:01
[ 1605.461528] viper: [Forwarding] viperPORT0 --> viperPORT1
[ 1605.461530] viper: [Egress] viperPORT1 --> viperPC1
[ 1605.461532] viper: viperPC1 received packet from 02:ab:cd:00:00:00

/* Ping respond */
[ 1605.461550] viper: viperPC1 Start Xmit to 02:ab:cd:00:00:00
[ 1605.461551] viper: [Unicast] viperPC1 (ingress)--> viperPORT1
[ 1605.461553] viper: [HIT] 02:ab:cd:00:00:00
[ 1605.461554] viper: [Forwarding] viperPORT1 --> viperPORT0
[ 1605.461556] viper: [Egress] viperPORT0 --> viperPC0
[ 1605.461557] viper: viperPC0 received packet from 02:ab:cd:00:00:01

[ 1605.492992] viper: viperPC0 Ethernet device stopped
[ 1605.514582] viper: viperPORT0 Ethernet device stopped
[ 1605.527303] viper: viperPC1 Ethernet device stopped
[ 1605.553017] viper: viperPORT1 Ethernet device stopped

/* Forwarding table */
[ 1605.570626] viper: | MAC 02:ab:cd:00:00:00 | PORT 0 | TIME 1741074076.674521206 (sec) |
[ 1605.570644] viper: | MAC 02:ab:cd:00:00:01 | PORT 1 | TIME 1741074076.674548680 (sec) |
[ 1605.570650] viper: Virtual Ethernet switch driver unloaded
```