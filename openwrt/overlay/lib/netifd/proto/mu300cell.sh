#!/bin/sh
# netifd protocol for the MU300 modem: attach with AT commands (mobile-data) and configure sipa_eth0.
# /etc/config/network:  config interface 'wan' / option proto 'mu300cell' / option apn 'internet'
[ -n "$INCLUDE_ONLY" ] || {
	. /lib/functions.sh
	. ../netifd-proto.sh
	init_proto "$@"
}

proto_mu300cell_init_config() {
	available=1
	no_device=1
	proto_config_add_string "apn"
	proto_config_add_boolean "peerdns"
	proto_config_add_array "dns:list(ipaddr)"
}

proto_mu300cell_setup() {
	local config="$1"
	local apn peerdns out ifname ip prefix dns1 dns2
	json_get_vars apn peerdns

	out=$(MU300_NETIFD=1 /opt/mu300/bin/mobile-data up $apn 2>/tmp/mu300cell.err)
	if [ $? = 3 ]; then
		logger -t mu300cell "$(cat /tmp/mu300cell.err)"
		proto_notify_error "$config" NO_MODEM
		proto_block_restart "$config"
		return 1
	fi
	ip=$(echo "$out" | sed -n 's/^IP=//p')
	if [ -z "$ip" ]; then
		logger -t mu300cell "attach failed: $(cat /tmp/mu300cell.err)"
		proto_notify_error "$config" ATTACH_FAILED
		sleep 20
		proto_setup_failed "$config"
		return 1
	fi
	ifname=$(echo "$out" | sed -n 's/^IFACE=//p')
	prefix=$(echo "$out" | sed -n 's/^PREFIX=//p')
	dns1=$(echo "$out" | sed -n 's/^DNS1=//p')
	dns2=$(echo "$out" | sed -n 's/^DNS2=//p')

	ip link set "$ifname" up
	proto_init_update "$ifname" 1
	proto_add_ipv4_address "$ip" "${prefix:-32}"
	proto_add_ipv4_route "0.0.0.0" 0
	if [ "${peerdns:-1}" != 0 ]; then
		[ -n "$dns1" ] && proto_add_dns_server "$dns1"
		[ -n "$dns2" ] && proto_add_dns_server "$dns2"
	fi
	proto_send_update "$config"
	[ -w /sys/class/leds/sc27xx:blue/brightness ] && echo 255 > /sys/class/leds/sc27xx:blue/brightness
	logger -t mu300cell "connected: $ip/${prefix:-32} on $ifname"
}

proto_mu300cell_teardown() {
	local config="$1"
	/opt/mu300/bin/mobile-data down >/dev/null 2>&1
	proto_kill_command "$config"
}

[ -n "$INCLUDE_ONLY" ] || add_protocol mu300cell
