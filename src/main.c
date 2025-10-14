/*
 * Segment Controller Firmware - Phase 1: Basic Bringup
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/version.h>

int main(void)
{
	printk("Segment Controller Firmware Starting...\n");
	printk("Board: %s\n", CONFIG_BOARD);
	printk("Zephyr Version: %s\n", KERNEL_VERSION_STRING);

	printk("\nPhase 1: Basic Bringup - SUCCESS\n");
	printk("Ready for Phase 2: Ethernet Networking\n");

	while (1) {
		k_sleep(K_SECONDS(5));
		printk("Heartbeat: System running\n");
	}

	return 0;
}
