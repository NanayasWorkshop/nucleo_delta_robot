/*
 * Network Layer - Phase 2: Ethernet Networking
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "network.h"
#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_core.h>
#include <zephyr/net/net_context.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/dhcpv4.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/sys/printk.h>

/* Network interface */
static struct net_if *iface = NULL;

/* Network management event handler */
static struct net_mgmt_event_callback mgmt_cb;

/* Network ready flag */
static bool net_ready = false;

/* DHCP event handler */
static void dhcp_event_handler(struct net_mgmt_event_callback *cb,
			       uint64_t mgmt_event, struct net_if *iface_cb)
{
	if (mgmt_event == NET_EVENT_IPV4_DHCP_BOUND) {
		char addr_str[INET_ADDRSTRLEN];

		/* Get the assigned IP address */
		struct in_addr *addr = &iface_cb->config.dhcpv4.requested_ip;

		net_addr_ntop(AF_INET, addr, addr_str, sizeof(addr_str));

		printk("\n=== DHCP Success ===\n");
		printk("IP Address assigned: %s\n", addr_str);
		printk("Network is ready!\n");
		printk("====================\n\n");

		net_ready = true;
	} else if (mgmt_event == NET_EVENT_IPV4_ADDR_ADD) {
		printk("IPv4 address added to interface\n");
	}
}

int network_init(void)
{
	printk("\n[Phase 2] Initializing Network...\n");

	/* Get the default network interface */
	iface = net_if_get_default();
	if (!iface) {
		printk("ERROR: No network interface found\n");
		return -ENODEV;
	}

	printk("Network interface found: %s\n", net_if_get_device(iface)->name);

	/* Register DHCP event handler */
	net_mgmt_init_event_callback(&mgmt_cb, dhcp_event_handler,
				     NET_EVENT_IPV4_DHCP_BOUND |
				     NET_EVENT_IPV4_ADDR_ADD);
	net_mgmt_add_event_callback(&mgmt_cb);

	printk("DHCP event handler registered\n");

	/* Start DHCP */
	printk("Starting DHCP client...\n");
	printk("Waiting for IP address (this may take 10-30 seconds)...\n");

	net_dhcpv4_start(iface);

	return 0;
}

int network_get_ip_address(char *buf, size_t buflen)
{
	if (!iface || !net_ready) {
		return -EAGAIN;
	}

	struct in_addr *addr = &iface->config.dhcpv4.requested_ip;

	if (net_addr_ntop(AF_INET, addr, buf, buflen) == NULL) {
		return -EINVAL;
	}

	return 0;
}

bool network_is_ready(void)
{
	return net_ready;
}
