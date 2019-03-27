#!/bin/bash
set -e

#
# This script is part of ProGDN RVI.
# It is intended to allow progdn-rvi catch and redirect packets.
# NOTICE: Changes are active until reboot.
#

#
# TODO: Define own values for the following parameters
#

# Option "mark" from progdn-rvi.conf
MARK=123
# Option "table" from progdn-rvi.conf
TABLE=100
# Default outbound interface
DEFAULT_OUTBOUND_INTERFACE=/proc/sys/net/ipv4/conf/enp0s3


if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root" 
   exit 1
fi

iptables -t mangle -I PREROUTING -m mark --mark ${MARK} -j CONNMARK --save-mark
iptables -t mangle -I OUTPUT -m connmark --mark ${MARK} -j CONNMARK --restore-mark
ip rule add fwmark ${MARK} lookup ${TABLE}
ip route add local 0.0.0.0/0 dev lo table ${TABLE}
echo 1 > ${DEFAULT_OUTBOUND_INTERFACE}/route_localnet 

echo "Done"
