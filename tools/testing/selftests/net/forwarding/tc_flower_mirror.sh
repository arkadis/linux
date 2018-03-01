#!/bin/bash
# SPDX-License-Identifier: GPL-2.0

NUM_NETIFS=4
source lib.sh
source tc_common.sh

tcflags="skip_hw"

h1_create()
{
	simple_if_init $h1 192.0.1.1/24
}

h1_destroy()
{
	simple_if_fini $h1 192.0.1.1/24
}

h2_create()
{
	simple_if_init $h2 192.0.1.2/24
	tc qdisc add dev $h2 handle ffff: ingress
	ip link set $h2 address $rp1mac
}

h2_destroy()
{
	tc qdisc del dev $h2 handle ffff: ingress
	simple_if_fini $h2 192.0.1.2/24
}

router_create()
{
	ip link set dev $rp1 up
	ip link set dev $rp2 up

	ip address add 192.0.1.2/24 dev $rp1
	tc qdisc add dev $rp1 handle ffff: ingress

	ip address add 192.0.2.2/24 dev $rp2
}

router_destroy()
{
	ip address del 192.0.2.2/24 dev $rp2

	tc qdisc del dev $rp1 handle ffff: ingress
	ip address del 192.0.1.2/24 dev $rp1

	ip link set dev $rp2 down
	ip link set dev $rp1 down
}

setup_prepare()
{
	h1=${NETIFS[p1]}
	rp1=${NETIFS[p2]}

	rp2=${NETIFS[p3]}
	h2=${NETIFS[p4]}

	rp1mac=$(mac_get $rp1)

	vrf_prepare

	h1_create
	h2_create

	router_create
}

cleanup()
{
	pre_cleanup

	router_destroy

	h2_destroy
	h1_destroy

	vrf_cleanup
}

mirror_test()
{
	RET=0

	tc filter add dev $rp1 ingress protocol ip pref 1 flower \
		$tcflags action mirred egress mirror dev $rp2

	tc filter add dev $h2 ingress protocol ip pref 1 flower \
		$tcflags action drop

	ping_test $h1 192.0.1.2

	tc_check_packets "dev $h2 ingress" 1 10
	check_err $? "Matched on a wrong filter"

	tc filter del dev $h2 ingress protocol ip pref 1 flower \
		$tcflags action drop

	tc filter del dev $rp1 ingress protocol ip pref 1 flower \
		$tcflags action mirred egress mirror dev $rp2

	log_test "mirror ($tcflags)"
}

trap cleanup EXIT

setup_prepare
setup_wait
mirror_test

tc_offload_check
if [[ $? -ne 0 ]]; then
	log_info "Could not test offloaded functionality"
else
	tcflags="skip_sw"
	mirror_test
fi

exit $EXIT_STATUS
