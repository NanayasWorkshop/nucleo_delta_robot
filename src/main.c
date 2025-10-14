/*
 * Segment Controller Firmware - Phase 2: Ethernet Networking
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/version.h>
#include "network.h"

int main(void)
{
	int ret;
	char ip_addr[32];

	printk("\n");
	printk("========================================\n");
	printk("  Segment Controller Firmware\n");
	printk("========================================\n");
	printk("Board: %s\n", CONFIG_BOARD);
	printk("Zephyr Version: %s\n", KERNEL_VERSION_STRING);
	printk("========================================\n\n");

	printk("[Phase 1] Basic Bringup - SUCCESS\n");

	/* Phase 2: Initialize networking */
	ret = network_init();
	if (ret < 0) {
		printk("ERROR: Network initialization failed: %d\n", ret);
		printk("Phase 2 FAILED - stopping here\n");
		return ret;
	}

	printk("[Phase 2] Network initialization started\n");
	printk("Waiting for DHCP to assign IP address...\n\n");

	/* Main loop */
	while (1) {
		k_sleep(K_SECONDS(5));

		if (network_is_ready()) {
			/* Network is up, print IP periodically */
			if (network_get_ip_address(ip_addr, sizeof(ip_addr)) == 0) {
				printk("Heartbeat: System running | IP: %s\n", ip_addr);
			}
		} else {
			/* Still waiting for network */
			printk("Heartbeat: Waiting for network...\n");
		}
	}

	return 0;
}
