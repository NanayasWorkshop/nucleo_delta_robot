/*
 * Network Layer - Phase 3: TCP/UDP Sockets
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "network.h"
#include "packet.h"
#include <zephyr/kernel.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_core.h>
#include <zephyr/net/net_context.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/dhcpv4.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>
#include <zephyr/sys/printk.h>
#include <zephyr/posix/arpa/inet.h>
#include <string.h>
#include <errno.h>

/* Network interface */
static struct net_if *iface = NULL;

/* Network management event handler */
static struct net_mgmt_event_callback mgmt_cb;

/* Network ready flag */
static bool net_ready = false;

/* Sockets */
static int tcp_sock = -1;
static int udp_sock = -1;
static int tcp_client_sock = -1;

/* Master address (for sending feedback) */
static struct sockaddr_in master_addr;
static bool master_connected = false;

/* Thread stacks */
#define TCP_THREAD_STACK_SIZE 2048
#define UDP_THREAD_STACK_SIZE 2048

K_THREAD_STACK_DEFINE(tcp_thread_stack, TCP_THREAD_STACK_SIZE);
K_THREAD_STACK_DEFINE(udp_thread_stack, UDP_THREAD_STACK_SIZE);

static struct k_thread tcp_thread_data;
static struct k_thread udp_thread_data;

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

/* TCP server thread */
static void tcp_server_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	uint8_t rx_buffer[NETWORK_RX_BUFFER_SIZE];
	int ret;

	printk("[TCP] Server thread started\n");

	while (1) {
		/* Wait for client connection */
		struct sockaddr_in client_addr;
		socklen_t client_addr_len = sizeof(client_addr);

		printk("[TCP] Waiting for client connection on port %d...\n", TCP_LISTEN_PORT);

		tcp_client_sock = accept(tcp_sock, (struct sockaddr *)&client_addr,
					 &client_addr_len);

		if (tcp_client_sock < 0) {
			printk("[TCP] Accept failed: %d\n", errno);
			k_sleep(K_SECONDS(1));
			continue;
		}

		/* Save master address for sending feedback */
		memcpy(&master_addr, &client_addr, sizeof(master_addr));
		master_connected = true;

		char addr_str[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &client_addr.sin_addr, addr_str, sizeof(addr_str));
		printk("[TCP] Client connected from %s:%d\n", addr_str,
		       ntohs(client_addr.sin_port));

		/* Handle client connection */
		while (1) {
			ret = recv(tcp_client_sock, rx_buffer, sizeof(rx_buffer), 0);

			if (ret <= 0) {
				if (ret < 0) {
					printk("[TCP] Receive error: %d\n", errno);
				} else {
					printk("[TCP] Client disconnected\n");
				}
				break;
			}

			printk("[TCP] Received %d bytes\n", ret);

			/* Parse and handle packet */
			packet_parse_command(rx_buffer, ret);
		}

		/* Close client socket */
		close(tcp_client_sock);
		tcp_client_sock = -1;
		master_connected = false;
	}
}

/* UDP server thread */
static void udp_server_thread(void *p1, void *p2, void *p3)
{
	ARG_UNUSED(p1);
	ARG_UNUSED(p2);
	ARG_UNUSED(p3);

	uint8_t rx_buffer[NETWORK_RX_BUFFER_SIZE];
	struct sockaddr_in src_addr;
	socklen_t src_addr_len;
	int ret;

	printk("[UDP] Server thread started on port %d\n", UDP_LISTEN_PORT);

	while (1) {
		src_addr_len = sizeof(src_addr);
		ret = recvfrom(udp_sock, rx_buffer, sizeof(rx_buffer), 0,
			       (struct sockaddr *)&src_addr, &src_addr_len);

		if (ret <= 0) {
			if (ret < 0) {
				printk("[UDP] Receive error: %d\n", errno);
			}
			k_sleep(K_MSEC(10));
			continue;
		}

		printk("[UDP] Received %d bytes\n", ret);

		/* Save master address for sending feedback */
		if (!master_connected) {
			memcpy(&master_addr, &src_addr, sizeof(master_addr));
		}

		/* Parse and handle packet */
		packet_parse_command(rx_buffer, ret);
	}
}

int network_start_servers(void)
{
	int ret;
	struct sockaddr_in bind_addr;

	printk("\n[Phase 3] Starting TCP/UDP servers...\n");

	/* Create TCP socket */
	tcp_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (tcp_sock < 0) {
		printk("[TCP] Socket creation failed: %d\n", errno);
		return -errno;
	}

	/* Bind TCP socket */
	memset(&bind_addr, 0, sizeof(bind_addr));
	bind_addr.sin_family = AF_INET;
	bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	bind_addr.sin_port = htons(TCP_LISTEN_PORT);

	ret = bind(tcp_sock, (struct sockaddr *)&bind_addr, sizeof(bind_addr));
	if (ret < 0) {
		printk("[TCP] Bind failed: %d\n", errno);
		close(tcp_sock);
		return -errno;
	}

	/* Listen on TCP socket */
	ret = listen(tcp_sock, 1);
	if (ret < 0) {
		printk("[TCP] Listen failed: %d\n", errno);
		close(tcp_sock);
		return -errno;
	}

	printk("[TCP] Listening on port %d\n", TCP_LISTEN_PORT);

	/* Create UDP socket */
	udp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (udp_sock < 0) {
		printk("[UDP] Socket creation failed: %d\n", errno);
		close(tcp_sock);
		return -errno;
	}

	/* Bind UDP socket */
	bind_addr.sin_port = htons(UDP_LISTEN_PORT);
	ret = bind(udp_sock, (struct sockaddr *)&bind_addr, sizeof(bind_addr));
	if (ret < 0) {
		printk("[UDP] Bind failed: %d\n", errno);
		close(tcp_sock);
		close(udp_sock);
		return -errno;
	}

	printk("[UDP] Listening on port %d\n", UDP_LISTEN_PORT);

	/* Start TCP server thread */
	k_thread_create(&tcp_thread_data, tcp_thread_stack,
			K_THREAD_STACK_SIZEOF(tcp_thread_stack),
			tcp_server_thread, NULL, NULL, NULL,
			K_PRIO_COOP(7), 0, K_NO_WAIT);
	k_thread_name_set(&tcp_thread_data, "tcp_server");

	/* Start UDP server thread */
	k_thread_create(&udp_thread_data, udp_thread_stack,
			K_THREAD_STACK_SIZEOF(udp_thread_stack),
			udp_server_thread, NULL, NULL, NULL,
			K_PRIO_COOP(7), 0, K_NO_WAIT);
	k_thread_name_set(&udp_thread_data, "udp_server");

	printk("[Phase 3] TCP/UDP servers started successfully\n\n");

	return 0;
}

int network_send_udp(const uint8_t *data, size_t length)
{
	if (udp_sock < 0) {
		return -ENOTCONN;
	}

	/* Use master address if we know it */
	if (master_connected || master_addr.sin_addr.s_addr != 0) {
		return sendto(udp_sock, data, length, 0,
			      (struct sockaddr *)&master_addr,
			      sizeof(master_addr));
	}

	return -ENOTCONN;
}

int network_send_tcp(const uint8_t *data, size_t length)
{
	if (tcp_client_sock < 0 || !master_connected) {
		return -ENOTCONN;
	}

	return send(tcp_client_sock, data, length, 0);
}
