/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Zephyr cell for Genio EVKs with MT8188-based SoCs
 *
 * Copyright (c) MediaTek Inc., 2025-2026
 *
 * Authors:
 *  Felix Freimann <felix.freimann@mediatek.com>
 *  Andrew Perepech <andrew.perepech@mediatek.com>
 *  Aary Patil <aary.patil@mediatek.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * The cell runs on CPU 3, a Cortex-A55, unless GENIO_ZEPHYR_CPUS selects
 * other CPUs. Its image is loaded to 0x8000, an 8 MiB window
 * at 0x6b000000. It owns UART1 for its console, and GPIO 38 and 40 with
 * their EINTs. With GENIO_AFE defined, it also shares the audio front end,
 * its clocks and its DMA memory with the root cell, and owns the eTDM pins.
 */

#include <jailhouse/types.h>
#include <jailhouse/cell-config.h>

#ifndef GENIO_ZEPHYR_CPUS
#define GENIO_ZEPHYR_CPUS	(1 << 3)
#endif

struct {
	struct jailhouse_cell_desc cell;
	__u64 cpus[1];
#ifdef GENIO_AFE
	struct jailhouse_memory mem_regions[10];
#else
	struct jailhouse_memory mem_regions[3];
#endif
	struct jailhouse_irqchip irqchips[1];
#ifdef GENIO_AFE
	__u32 smc_ids[1];
	struct jailhouse_vendor_resource vendor_resources[5];
#else
	struct jailhouse_vendor_resource vendor_resources[3];
#endif
} __attribute__((packed)) config = {
	.cell = {
		.signature = JAILHOUSE_CELL_DESC_SIGNATURE,
		.revision = JAILHOUSE_CONFIG_REVISION,
		.architecture = JAILHOUSE_ARM64,
		.name = "zephyr",
		.flags = JAILHOUSE_CELL_PASSIVE_COMMREG |
			 JAILHOUSE_CELL_VIRTUAL_CONSOLE_PERMITTED,

		.cpu_set_size = sizeof(config.cpus),
		.num_memory_regions = ARRAY_SIZE(config.mem_regions),
		.num_irqchips = ARRAY_SIZE(config.irqchips),
#ifdef GENIO_AFE
		.num_smc_ids = ARRAY_SIZE(config.smc_ids),
#endif
		.num_vendor_resources = ARRAY_SIZE(config.vendor_resources),

		.cpu_reset_address = 0x8000,

		.console = {
			.address = 0x11001200,
			.divider = 0x2a,	/* 38400 baud */
			.type = JAILHOUSE_CON_TYPE_8250,
			.flags = JAILHOUSE_CON_ACCESS_MMIO |
				 JAILHOUSE_CON_REGDIST_4,
			.gate_nr = 23,
			.clock_reg = 0x10001084,
		},
	},

	.cpus = {
		GENIO_ZEPHYR_CPUS,
	},

	.mem_regions = {
#ifdef GENIO_AFE
		/* topckgen */ {
			.phys_start = 0x10000000,
			.virt_start = 0x10000000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO | JAILHOUSE_MEM_ROOTSHARED,
		},
		/* infracfg_ao, after the clock gates */ {
			.phys_start = 0x10001400,
			.virt_start = 0x10001400,
			.size = 0x00000c00,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO | JAILHOUSE_MEM_IO_32 |
				JAILHOUSE_MEM_ROOTSHARED,
		},
		/* toprgu */ {
			.phys_start = 0x10007000,
			.virt_start = 0x10007000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO | JAILHOUSE_MEM_ROOTSHARED,
		},
		/* apmixedsys */ {
			.phys_start = 0x1000c000,
			.virt_start = 0x1000c000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO | JAILHOUSE_MEM_ROOTSHARED,
		},
		/* AFE */ {
			.phys_start = 0x10b10000,
			.virt_start = 0x10b10000,
			.size = 0x10000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO | JAILHOUSE_MEM_ROOTSHARED,
		},
		/* adsp_audio26m */ {
			.phys_start = 0x10b91000,
			.virt_start = 0x10b91000,
			.size = 0x1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO | JAILHOUSE_MEM_ROOTSHARED,
		},
		/* audio DMA */ {
			.phys_start = 0x61000000,
			.virt_start = 0x61000000,
			.size = 0x00800000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_ROOTSHARED,
		},
#endif
		/* UART1 */ {
			.phys_start = 0x11001200,
			.virt_start = 0x11001200,
			.size = 0x100,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO | JAILHOUSE_MEM_IO_32 |
				JAILHOUSE_MEM_ROOTSHARED,
		},
		/* RAM */ {
			.phys_start = 0x6b000000,
			.virt_start = 0x8000,
			.size = 0x00800000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE | JAILHOUSE_MEM_LOADABLE,
		},
		/* communication region */ {
			.virt_start = 0x80000000,
			.size = 0x00001000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_COMM_REGION,
		},
	},

	.irqchips = {
		/* GIC */ {
			.address = 0x0c000000,
			.pin_base = 160,
			.pin_bitmap = {
				1 << (142 + 32 - 160),	/* UART1 */
			},
		},
	},

#ifdef GENIO_AFE
	.smc_ids = {
		0xc2000517,	/* MTK_SIP_AUDIO_CONTROL */
	},
#endif

	/*
	 * EINTs reach the cell via the EINT interrupt (SPI 235), which stays
	 * with the root cell.
	 */
	.vendor_resources = {
		/* EINT 38, 40 */ {
			.type = JAILHOUSE_VENDOR_MT8188_EINT,
			.address = 0x1000b000,
			.base = 32,
			.bitmap = { 1 << (38 - 32) | 1 << (40 - 32) },
		},
		/* GPIO 33, 34 (UART1), 38, 40 */ {
			.type = JAILHOUSE_VENDOR_MT8188_GPIO,
			.address = 0x10005000,
			.base = 32,
			.bitmap = {
				1 << (33 - 32) | 1 << (34 - 32) |
				1 << (38 - 32) | 1 << (40 - 32),
			},
		},
		/* clock gate of UART1 */ {
			.type = JAILHOUSE_VENDOR_MT8188_CLK,
			.address = 0x10001000,
			.base = 0,
			.bitmap = { 1 << 23 },
		},
#ifdef GENIO_AFE
		/*
		 * eTDM pins: OUT1 4-6, 11; IN2 107-110; OUT2 114-117;
		 * IN1 125-128
		 */ {
			.type = JAILHOUSE_VENDOR_MT8188_GPIO,
			.address = 0x10005000,
			.base = 0,
			.bitmap = { 0x00000870, 0, 0, 0xe03c7800 },
		},
		{
			.type = JAILHOUSE_VENDOR_MT8188_GPIO,
			.address = 0x10005000,
			.base = 128,
			.bitmap = { 1 << (128 - 128) },
		},
#endif
	},
};
