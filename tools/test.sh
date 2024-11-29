#!/usr/bin/env bash

return_val=0

function check_kmod() {
    local mod_name=$1
    lsmod | grep -w $mod_name > /dev/null
    if [ $? -eq 0 ]; then
        # Module found
        return 0
    fi
    return 1
}

function insert_kmod() {
    local mod_name=$1
    local num_port=$2
    local num_pc=$3
    local noko_name=$(echo $mod_name |sed s/.ko//)
    check_kmod $noko_name
    ret=$?
    if [ $ret -eq 0 ] ; then
        sudo rmmod $noko_name > /dev/null
    fi
    echo "Installing Module $mod_name"
    sudo insmod $mod_name $num_port $num_pc
    return $(check_kmod $noko_name)
} 

function remove_kmod() {
    local mod_name=$1
    check_kmod $mod_name
    ret=$?
    if [ $ret -eq 1 ] ; then
        return 0
    fi
    echo "Removing Module $mod_name"
    sudo rmmod $mod_name > /dev/null
    return 0
}

insert_kmod viper.ko num_port=2 num_pc=2
if [ $? -ne 0 ]; then
    return_val=1
fi

if [ $return_val -eq 0 ]; then
    # avoid device or resource busy error
    sleep 0.5
    sudo ip link
    # create network namespaces for each port
    #sudo ip netns add ns0
    #sudo ip netns add ns1

    #sudo ip link set viper0 netns ns0
    #sudo ip link set viper1 netns ns1
    #sudo ip netns exec ns0 ip link set viper0 up
    #sudo ip netns exec ns1 ip link set viper1 up

    #sudo ip netns exec ns0 ip addr add 10.0.0.1/24 dev viper0
    #sudo ip netns exec ns1 ip addr add 10.0.0.2/24 dev viper1

    echo
    echo "================================================================================"
    echo "Ping Test: Port0 viper0 (10.0.0.1) <--> Port1 viper1 (10.0.0.2)"
    echo
    echo "(should succeed)"
    echo "(be patient, it will take some time to route...)"
    echo "================================================================================"
    #sudo ip netns exec ns0 ping -c 1 10.0.0.2
fi

remove_kmod viper
#sudo ip netns del ns0
#sudo ip netns del ns1

if [ $return_val -eq 0 ]; then
    echo "Test Passed"
    exit 0
fi
echo "Failed (code: $return_val)"
exit $return_val
