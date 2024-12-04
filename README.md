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
[  870.238038] viper: loading out-of-tree module taints kernel.
[  870.238049] viper: module verification failed: signature and/or required key missing - tainting kernel
[  870.242607] viper: New PORT 0
[  870.242801] viper: New PORT 1
[  870.242972] viper: New PC0 which belongs to PORT0
[  870.243117] viper: New PC1 which belongs to PORT1
[  870.243119] viper: Virtual Ethernet switch driver loaded
[  870.279184] viper: viperPORT1 Ethernet device opened
[  870.286367] viper: viperPORT0 Ethernet device opened
[  870.294951] viper: viperPC1 Ethernet device opened
[  870.297205] viper: viperPC0 Ethernet device opened
[  870.308190] viper: viperPC1 Start Xmit to ff:ff:ff:ff:ff:ff
[  870.308198] viper: [Broadcast] viperPC1 (ingress)--> viperPORT1
[  870.308204] viper: [Forwarding] viperPORT1 --> viperPORT0
[  870.308219] viper: [Egress] viperPORT0 --> viperPC0
[  870.308222] viper: viperPC0 received packet from 02:ab:cd:00:00:01
[  870.308316] viper: viperPC0 Start Xmit to ff:ff:ff:ff:ff:ff
[  870.308319] viper: [Broadcast] viperPC0 (ingress)--> viperPORT0
[  870.308321] viper: [Forwarding] viperPORT0 --> viperPORT1
[  870.308323] viper: [Egress] viperPORT1 --> viperPC1
[  870.308325] viper: viperPC1 received packet from 02:ab:cd:00:00:00
[  870.312663] viper: viperPC0 Start Xmit to 33:33:00:00:00:16
[  870.312670] viper: [Unicast] viperPC0 (ingress)--> viperPORT0
[  870.312675] viper: [MISS] 33:33:00:00:00:16
[  870.312677] viper: [Forwarding] viperPORT0 --> viperPORT1
[  870.312680] viper: viperPORT1 drop packet
[  870.312695] viper: viperPC1 Start Xmit to 33:33:00:00:00:16
[  870.312696] viper: [Unicast] viperPC1 (ingress)--> viperPORT1
[  870.312698] viper: [MISS] 33:33:00:00:00:16
[  870.312698] viper: [Forwarding] viperPORT1 --> viperPORT0
[  870.312700] viper: viperPORT0 drop packet
[  870.464692] viper: viperPC0 Start Xmit to 33:33:00:00:00:16
[  870.464703] viper: [Unicast] viperPC0 (ingress)--> viperPORT0
[  870.464710] viper: [MISS] 33:33:00:00:00:16
[  870.464712] viper: [Forwarding] viperPORT0 --> viperPORT1
[  870.464717] viper: viperPORT1 drop packet
[  870.536969] viper: viperPC1 Start Xmit to 33:33:ff:8c:85:e3
[  870.536973] viper: [Unicast] viperPC1 (ingress)--> viperPORT1
[  870.536976] viper: [MISS] 33:33:ff:8c:85:e3
[  870.536977] viper: [Forwarding] viperPORT1 --> viperPORT0
[  870.536980] viper: viperPORT0 drop packet
[  870.861042] viper: viperPC0 Ethernet device stopped
[  870.902814] viper: viperPC1 Ethernet device stopped
[  870.954356] viper: viperPC0 Ethernet device opened
[  870.957234] viper: viperPC0 Start Xmit to 33:33:00:00:00:16
[  870.957245] viper: [Unicast] viperPC0 (ingress)--> viperPORT0
[  870.957254] viper: [MISS] 33:33:00:00:00:16
[  870.957257] viper: [Forwarding] viperPORT0 --> viperPORT1
[  870.957264] viper: viperPORT1 drop packet
[  870.974933] viper: viperPC1 Ethernet device opened
[  870.976151] viper: viperPC0 Start Xmit to 33:33:ff:00:00:00
[  870.976161] viper: [Unicast] viperPC0 (ingress)--> viperPORT0
[  870.976168] viper: [MISS] 33:33:ff:00:00:00
[  870.976171] viper: [Forwarding] viperPORT0 --> viperPORT1
[  870.976175] viper: viperPORT1 drop packet
[  870.978168] viper: viperPC1 Start Xmit to 33:33:00:00:00:16
[  870.978178] viper: [Unicast] viperPC1 (ingress)--> viperPORT1
[  870.978185] viper: [MISS] 33:33:00:00:00:16
[  870.978187] viper: [Forwarding] viperPORT1 --> viperPORT0
[  870.978192] viper: viperPORT0 drop packet
[  871.020656] viper: viperPC0 Start Xmit to ff:ff:ff:ff:ff:ff
[  871.020665] viper: [Broadcast] viperPC0 (ingress)--> viperPORT0
[  871.020670] viper: [Forwarding] viperPORT0 --> viperPORT1
[  871.020679] viper: [Egress] viperPORT1 --> viperPC1
[  871.020682] viper: viperPC1 received packet from 02:ab:cd:00:00:00
[  871.020709] viper: viperPC1 Start Xmit to 02:ab:cd:00:00:00
[  871.020710] viper: [Unicast] viperPC1 (ingress)--> viperPORT1
[  871.020712] viper: [HIT] 02:ab:cd:00:00:00
[  871.020713] viper: [Forwarding] viperPORT1 --> viperPORT0
[  871.020715] viper: [Egress] viperPORT0 --> viperPC0
[  871.020716] viper: viperPC0 received packet from 02:ab:cd:00:00:01
[  871.020721] viper: viperPC0 Start Xmit to 02:ab:cd:00:00:01
[  871.020722] viper: [Unicast] viperPC0 (ingress)--> viperPORT0
[  871.020723] viper: [HIT] 02:ab:cd:00:00:01
[  871.020724] viper: [Forwarding] viperPORT0 --> viperPORT1
[  871.020726] viper: [Egress] viperPORT1 --> viperPC1
[  871.020727] viper: viperPC1 received packet from 02:ab:cd:00:00:00
[  871.020742] viper: viperPC1 Start Xmit to 02:ab:cd:00:00:00
[  871.020743] viper: [Unicast] viperPC1 (ingress)--> viperPORT1
[  871.020745] viper: [HIT] 02:ab:cd:00:00:00
[  871.020746] viper: [Forwarding] viperPORT1 --> viperPORT0
[  871.020747] viper: [Egress] viperPORT0 --> viperPC0
[  871.020748] viper: viperPC0 received packet from 02:ab:cd:00:00:01
[  871.051152] viper: viperPC0 Ethernet device stopped
[  871.060182] viper: viperPORT0 Ethernet device stopped
[  871.080201] viper: viperPC1 Ethernet device stopped
[  871.087123] viper: viperPORT1 Ethernet device stopped
[  871.097673] viper: | MAC 02:ab:cd:00:00:00 | PORT 0 | TIME 1733323532.356214732 (sec) |
[  871.097685] viper: | MAC 02:ab:cd:00:00:01 | PORT 1 | TIME 1733323532.356236282 (sec) |
[  871.097689] viper: Virtual Ethernet switch driver unloaded
```