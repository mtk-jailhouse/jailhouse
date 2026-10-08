/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * The cells of the Genio 700 EVK (MT8390)
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
 * Each GENIO_<kind>_CELL("<file>", ...) block builds <file>.cell. A block
 * lists what is particular to its cell: CPUs, further memory regions such
 * as shared devices, interrupts, virtual PCI devices, SiP calls, and the
 * pins and clock gates it owns; genio-evk.h adds what all cells of the
 * kind share and describes the lists. The build reads the cell names from
 * the lines that start with GENIO_<kind>_CELL(", so begin each block like
 * that, in the first column.
 *
 * To adapt a cell, edit its lists and rebuild; its file name stays. For
 * example, to give a Zephyr cell GPIO 0 and 1 instead of 38 and 40,
 * replace GENIO_HEADER_PINS with EINT_PINS(0, 0, 1) and GPIO_PINS(0, 0, 1);
 * keep GENIO_UART1_RESOURCES, the pins and the clock gate of the console.
 * To share a device with a cell, add its MMIO to REGIONS() and its
 * interrupt to IRQS(). The build checks CPUs, pins and interrupts;
 * "jailhouse config check" checks the cells against each other.
 */

#include "genio-evk.h"

/*
 * The cores, by Linux CPU number: 6 Cortex-A55 and 2 Cortex-A78. The
 * -a78 cells take the second A78.
 */
#define GENIO_CPU_A55_0		0
#define GENIO_CPU_A55_1		1
#define GENIO_CPU_A55_2		2
#define GENIO_CPU_A55_3		3
#define GENIO_CPU_A55_4		4
#define GENIO_CPU_A55_5		5
#define GENIO_CPU_A78_0		6
#define GENIO_CPU_A78_1		7
#define GENIO_BOARD_CPUS						\
	CPUS(GENIO_CPU_A55_0, GENIO_CPU_A55_1, GENIO_CPU_A55_2,		\
	     GENIO_CPU_A55_3, GENIO_CPU_A55_4, GENIO_CPU_A55_5,		\
	     GENIO_CPU_A78_0, GENIO_CPU_A78_1)
/* 8 GiB of DRAM */
#define GENIO_BOARD_DRAM_END	0x240000000

GENIO_ROOT_CELL("genio-700-evk",
	SMC_IDS(MT8188_LINUX_SIP_CALLS));

GENIO_UART_DEMO_CELL("genio-700-evk-uart-demo", CPUS(GENIO_CPU_A55_3));

GENIO_INMATE_CELL("genio-700-evk-zephyr", "zephyr",
	CPUS(GENIO_CPU_A55_3),
	REGIONS(),
	IRQS(),
	NO_PCI,
	SMC_IDS(),
	RESOURCES(GENIO_UART1_RESOURCES, GENIO_HEADER_PINS));

GENIO_INMATE_CELL("genio-700-evk-zephyr-a78", "zephyr",
	CPUS(GENIO_CPU_A78_1),
	REGIONS(),
	IRQS(),
	NO_PCI,
	SMC_IDS(),
	RESOURCES(GENIO_UART1_RESOURCES, GENIO_HEADER_PINS));

/*
 * Zephyr SMP (-smp, -afe-smp): the CPUs must be the cpu nodes of
 * mtk-zephyr's boards/mediatek/common/genio-evk-smp.dtsi. An SMP image on
 * other CPUs either spins before its console exists (no cpu node for its
 * boot CPU) or panics when CPU_ON is denied.
 */
GENIO_INMATE_CELL("genio-700-evk-zephyr-smp", "zephyr",
	CPUS(GENIO_CPU_A55_2, GENIO_CPU_A55_3),
	REGIONS(),
	IRQS(),
	NO_PCI,
	SMC_IDS(),
	RESOURCES(GENIO_UART1_RESOURCES, GENIO_HEADER_PINS));

GENIO_INMATE_CELL("genio-700-evk-zephyr-afe", "zephyr",
	CPUS(GENIO_CPU_A55_3),
	REGIONS(GENIO_AFE_REGIONS),
	IRQS(),
	NO_PCI,
	SMC_IDS(MT8188_SIP_AUDIO_CONTROL),
	RESOURCES(GENIO_UART1_RESOURCES, GENIO_HEADER_PINS,
		  GENIO_ETDM_PINS));

GENIO_INMATE_CELL("genio-700-evk-zephyr-afe-a78", "zephyr",
	CPUS(GENIO_CPU_A78_1),
	REGIONS(GENIO_AFE_REGIONS),
	IRQS(),
	NO_PCI,
	SMC_IDS(MT8188_SIP_AUDIO_CONTROL),
	RESOURCES(GENIO_UART1_RESOURCES, GENIO_HEADER_PINS,
		  GENIO_ETDM_PINS));

GENIO_INMATE_CELL("genio-700-evk-zephyr-afe-smp", "zephyr",
	CPUS(GENIO_CPU_A55_2, GENIO_CPU_A55_3),
	REGIONS(GENIO_AFE_REGIONS),
	IRQS(),
	NO_PCI,
	SMC_IDS(MT8188_SIP_AUDIO_CONTROL),
	RESOURCES(GENIO_UART1_RESOURCES, GENIO_HEADER_PINS,
		  GENIO_ETDM_PINS));

GENIO_INMATE_CELL("genio-700-evk-zephyr-rpmsg", "zephyr",
	CPUS(GENIO_CPU_A55_3),
	REGIONS(GENIO_RPMSG_REGIONS),
	IRQS(),
	PCI(GENIO_INMATE_VPCI_SPI, GENIO_IVSHMEM_DEVICE(1, 0)),
	SMC_IDS(),
	RESOURCES(GENIO_UART1_RESOURCES, GENIO_HEADER_PINS));
