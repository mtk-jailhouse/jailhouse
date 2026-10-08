/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Cells for the Genio EVKs with MT8188-based SoCs (Genio 510, Genio 700)
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
 * The macros that build the cells of a board's table, genio-<board>-cells.c,
 * and what all of these cells share: the memory layout and what every cell
 * of a kind contains. Each cell goes into its own section, .cell.<file>,
 * which the build turns into <file>.cell.
 */

#ifndef _GENIO_EVK_H
#define _GENIO_EVK_H

#include <jailhouse/types.h>
#include <jailhouse/cell-config.h>

#include "mt8188.h"

/*
 * Memory that the BSP's Jailhouse overlay withholds from Linux: the
 * hypervisor's, and the window that inmates are loaded into
 */
#define GENIO_WITHHELD		0x6ac00000
#define GENIO_WITHHELD_END	0x6ba00000
#define GENIO_HV_MEM		0x6ac00000
#define GENIO_HV_MEM_SIZE	0x00400000
#define GENIO_INMATE_MEM	0x6b000000
#define GENIO_INMATE_MEM_SIZE	0x00800000

_Static_assert(GENIO_HV_MEM >= GENIO_WITHHELD &&
	       GENIO_HV_MEM + GENIO_HV_MEM_SIZE <= GENIO_INMATE_MEM &&
	       GENIO_INMATE_MEM + GENIO_INMATE_MEM_SIZE <= GENIO_WITHHELD_END,
	       "hypervisor and inmates must be in the withheld memory");

/* the address of the inmate window in its cell, where inmate images start */
#define GENIO_INMATE_ENTRY	0x8000

/*
 * A virtual PCI host with one bus, whose config space is emulated at
 * GENIO_MMCONFIG, carries an ivshmem device between the root cell and one
 * other cell. Its shared memory is in the 1 MiB that the overlay reserves
 * at GENIO_IVSHMEM: the state table, a read-write section, and an output
 * section for each of the two peers, writable by that peer only. INTx
 * reaches each peer as SPI vpci_irq_base of its cell.
 */
#define GENIO_MMCONFIG		0x6b800000
#define GENIO_IVSHMEM		0x6ba00000
#define GENIO_IVSHMEM_SIZE	0x00100000
#define GENIO_IVSHMEM_RW	(GENIO_IVSHMEM + 0x1000)
#define GENIO_IVSHMEM_OUT(peer)						\
	(GENIO_IVSHMEM_RW + 0xc0000 + (peer) * 0x4000)
#define GENIO_ROOT_VPCI_SPI	72
#define GENIO_INMATE_VPCI_SPI	74

/* the bus's config space, and the BARs that Linux places after it */
_Static_assert(GENIO_MMCONFIG >= GENIO_INMATE_MEM + GENIO_INMATE_MEM_SIZE &&
	       GENIO_MMCONFIG + 0x200000 <= GENIO_WITHHELD_END,
	       "the virtual PCI host must be in the withheld memory");
_Static_assert(GENIO_IVSHMEM >= GENIO_WITHHELD_END &&
	       GENIO_IVSHMEM_OUT(2) <= GENIO_IVSHMEM + GENIO_IVSHMEM_SIZE,
	       "the ivshmem memory must be in the reserved memory");

#define GENIO_IVSHMEM_REGION(start, size, region_flags)			\
	MT8188_REGION(start, (start) + (size), JAILHOUSE_MEM_READ |	\
		      (region_flags))
#define GENIO_IVSHMEM_REGIONS(peer, region_flags)			\
	GENIO_IVSHMEM_REGION(GENIO_IVSHMEM, 0x1000, region_flags),	\
	GENIO_IVSHMEM_REGION(GENIO_IVSHMEM_RW, 0xc0000,			\
			     JAILHOUSE_MEM_WRITE | (region_flags)),	\
	GENIO_IVSHMEM_REGION(GENIO_IVSHMEM_OUT(0), 0x4000,		\
			     ((peer) == 0 ? JAILHOUSE_MEM_WRITE : 0) |	\
			     (region_flags)),				\
	GENIO_IVSHMEM_REGION(GENIO_IVSHMEM_OUT(1), 0x4000,		\
			     ((peer) == 1 ? JAILHOUSE_MEM_WRITE : 0) |	\
			     (region_flags))

#define GENIO_IVSHMEM_DEVICE(peer, first_region)			\
	{								\
		.type = JAILHOUSE_PCI_TYPE_IVSHMEM,			\
		.domain = 1,						\
		.bdf = 0 << 3,						\
		.bar_mask = JAILHOUSE_IVSHMEM_BAR_MASK_INTX,		\
		.shmem_regions_start = (first_region),			\
		.shmem_dev_id = (peer),					\
		.shmem_peers = 2,					\
		.shmem_protocol = JAILHOUSE_SHMEM_PROTO_UNDEFINED,	\
	}

/*
 * The parameters of a cell in a table. Each takes a list:
 *  CPUS(cpu, ...)		the CPUs of the cell, by the board table's
 *				GENIO_CPU_* names
 *  REGIONS(region, ...)	further memory regions, e.g. shared devices
 *  IRQS(irq, ...)		further interrupts (GIC interrupt IDs)
 *  PCI(spi, device, ...)	virtual PCI devices, whose INTx is SPI spi
 *				of the cell; NO_PCI for none
 *  SMC_IDS(id, ...)		SiP calls that the cell may make
 *  RESOURCES(entry, ...)	pins and clock gates, as entries of
 *  GPIO_PINS(base, pin, ...)	GPIO pins (with their configuration),
 *  EINT_PINS(base, pin, ...)	EINTs and
 *  CLK_GATES(base, gate, ...)	clock gates of infracfg_ao. An entry spans
 *				128 of them from base, a multiple of 32 up to
 *				128.
 * A pin outside its entry fails the build with "negative width in
 * bit-field", CPUs and interrupts with a static assertion.
 */
#define CPUS(...)							\
	GENIO_FOLD(GENIO_CPU_BIT, GENIO_OR, 0, 0, __VA_ARGS__)
#define GENIO_CPU_BIT(a, b, cpu)	(1ULL << (cpu))
#define REGIONS(...)		(__VA_ARGS__)
#define IRQS(...)		(__VA_ARGS__)
#define PCI(spi, ...)		(spi, __VA_ARGS__)
#define NO_PCI			PCI(0)
#define SMC_IDS(...)		(__VA_ARGS__)
#define RESOURCES(...)		(__VA_ARGS__)

#define GENIO_BITMAP(base, ...)						\
	{								\
		GENIO_FOLD(GENIO_BIT, GENIO_OR, base, 0, __VA_ARGS__),	\
		GENIO_FOLD(GENIO_BIT, GENIO_OR, base, 1, __VA_ARGS__),	\
		GENIO_FOLD(GENIO_BIT, GENIO_OR, base, 2, __VA_ARGS__),	\
		GENIO_FOLD(GENIO_BIT, GENIO_OR, base, 3, __VA_ARGS__),	\
	}
#define GENIO_PINS(kind, block, from, ...)				\
	{								\
		.type = JAILHOUSE_VENDOR_MTK_##kind,			\
		.address = (block),					\
		.base = (from) + GENIO_CHECK((from) % 32 == 0 &&	\
			(from) <= 128 &&				\
			!GENIO_FOLD(GENIO_OUTSIDE, GENIO_OR, from, 128,	\
				    __VA_ARGS__)),			\
		.bitmap = GENIO_BITMAP(from, __VA_ARGS__),		\
	}
#define GPIO_PINS(from, ...)						\
	GENIO_PINS(GPIO, MT8188_GPIO, from, __VA_ARGS__)
#define EINT_PINS(from, ...)						\
	GENIO_PINS(EINT, MT8188_EINT, from, __VA_ARGS__)
#define CLK_GATES(from, ...)						\
	GENIO_PINS(CLK, MT8188_INFRA_CG, from, __VA_ARGS__)

/* memory region initializers */
#define GENIO_COMM_REGION						\
	{								\
		.virt_start = 0x80000000,				\
		.size = 0x00001000,					\
		.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |	\
			JAILHOUSE_MEM_COMM_REGION,			\
	}

#define GENIO_UART1_REGION						\
	MT8188_REGION(MT8188_UART1, MT8188_UART1 + MT8188_UART_SIZE,	\
		      JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |	\
		      JAILHOUSE_MEM_IO | JAILHOUSE_MEM_IO_32 |		\
		      JAILHOUSE_MEM_ROOTSHARED)

/* MMIO that a cell shares with the root cell */
#define GENIO_SHARED_MMIO(start, size)					\
	MT8188_REGION(start, (start) + (size),				\
		      JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |	\
		      JAILHOUSE_MEM_IO | JAILHOUSE_MEM_ROOTSHARED)

/*
 * The console descriptor is for inmates built with Jailhouse's inmate
 * library. Zephyr ignores it and runs UART1 at 115200 baud.
 */
#define GENIO_UART1_CONSOLE						\
	{								\
		.address = MT8188_UART1,				\
		.divider = 0x2a,	/* 38400 baud */		\
		.type = JAILHOUSE_CON_TYPE_8250,			\
		.flags = JAILHOUSE_CON_ACCESS_MMIO |			\
			 JAILHOUSE_CON_REGDIST_4,			\
		.gate_nr = MT8188_UART1_CLK_GATE,			\
		.clock_reg = MT8188_INFRA_CG0_CLR,			\
	}

/*
 * Lists for the tables:
 * GENIO_UART1_RESOURCES: the pins (GPIO 33, 34) and the clock gate of
 *   UART1, which an inmate with the UART1 console needs.
 * GENIO_HEADER_PINS: GPIO 38 and 40 with their EINTs, which both EVKs
 *   route to the 40-pin header, e.g. for the Zephyr samples.
 * GENIO_AFE_REGIONS: the audio front end, the clocks and resets that its
 *   driver programs, and its DMA memory, all shared with the root cell.
 * GENIO_ETDM_PINS: the eTDM pins, OUT1 4-6, 11; IN2 107-110; OUT2 114-117;
 *   IN1 125-128.
 * GENIO_RPMSG_REGIONS: the memory of the ivshmem device, for its peer 1.
 *   They must be the first regions of the cell, which has the device as
 *   PCI(GENIO_INMATE_VPCI_SPI, GENIO_IVSHMEM_DEVICE(1, 0)).
 */
#define GENIO_UART1_RESOURCES						\
	GPIO_PINS(32, 33, 34),						\
	CLK_GATES(0, MT8188_UART1_CLK_GATE)

#define GENIO_HEADER_PINS						\
	EINT_PINS(32, 38, 40),						\
	GPIO_PINS(32, 38, 40)

#define GENIO_AFE_REGIONS						\
	GENIO_SHARED_MMIO(MT8188_TOPCKGEN, 0x1000),			\
	/* infracfg_ao, after the clock gates */			\
	MT8188_REGION(MT8188_INFRA_CG + MT8188_INFRA_CG_SIZE,		\
		      MT8188_INFRACFG_AO + MT8188_INFRACFG_AO_SIZE,	\
		      JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |	\
		      JAILHOUSE_MEM_IO | JAILHOUSE_MEM_IO_32 |		\
		      JAILHOUSE_MEM_ROOTSHARED),			\
	GENIO_SHARED_MMIO(MT8188_TOPRGU, 0x1000),			\
	GENIO_SHARED_MMIO(MT8188_APMIXEDSYS, 0x1000),			\
	GENIO_SHARED_MMIO(MT8188_AFE, MT8188_AFE_SIZE),			\
	GENIO_SHARED_MMIO(MT8188_ADSP_AUDIO26M, 0x1000),		\
	MT8188_REGION(MT8188_AUDIO_DMA,					\
		      MT8188_AUDIO_DMA + MT8188_AUDIO_DMA_SIZE,		\
		      JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |	\
		      JAILHOUSE_MEM_ROOTSHARED)

#define GENIO_RPMSG_REGIONS						\
	GENIO_IVSHMEM_REGIONS(1, JAILHOUSE_MEM_ROOTSHARED)

#define GENIO_ETDM_PINS							\
	GPIO_PINS(0, 4, 5, 6, 11, 107, 108, 109, 110, 114, 115, 116, 117, \
		  125, 126, 127),					\
	GPIO_PINS(128, 128)

#define GENIO_CELL_SECTION(file)					\
	__attribute__((used, section(".cell." file)))

#define GENIO_CHECK_CPUS(file, cpu_set)					\
	_Static_assert((cpu_set) != 0 &&				\
		       ((cpu_set) & ~(GENIO_BOARD_CPUS)) == 0,		\
		       file ": the CPUs must be CPUs of the board")

/* a whole block, owned by the root cell until handed to other cells */
#define GENIO_ALL_PINS(kind, block, first)				\
	{								\
		.type = JAILHOUSE_VENDOR_MTK_##kind,			\
		.address = (block),					\
		.base = (first),					\
		.bitmap = {						\
			0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, \
		},							\
	}

#define GENIO_ALL_IRQS(first)						\
	{								\
		.address = MT8188_GICD,					\
		.pin_base = (first),					\
		.pin_bitmap = {						\
			0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, \
		},							\
	}

/*
 * The root cell, Linux, of the board: GENIO_BOARD_CPUS, and DRAM up to
 * GENIO_BOARD_DRAM_END except what the overlay withholds. It owns all MMIO,
 * interrupts, pins and clock gates, may make the SiP calls smc_ids, and is
 * peer 0 of the ivshmem device.
 *
 * Not mapped, but mediated by the hypervisor: the GIC, the clock gates,
 * GPIO, EINT and the pin configuration.
 */
#define GENIO_ROOT_LOW_REGIONS						\
	(MT8188_MMIO_REGIONS,						\
	 MT8188_BSP_MEM_REGIONS(GENIO_HV_MEM),				\
	 MT8188_RAM(GENIO_INMATE_MEM,					\
		    GENIO_INMATE_MEM + GENIO_INMATE_MEM_SIZE))
#define GENIO_ROOT_MEM_REGIONS						\
	(GENIO_UNLIST(GENIO_ROOT_LOW_REGIONS),				\
	 GENIO_IVSHMEM_REGIONS(0, 0),					\
	 MT8188_RAM(GENIO_IVSHMEM + GENIO_IVSHMEM_SIZE,			\
		    GENIO_BOARD_DRAM_END))
#define GENIO_ROOT_PCI_DEVICES						\
	(GENIO_IVSHMEM_DEVICE(0, GENIO_COUNT(struct jailhouse_memory,	\
					     GENIO_ROOT_LOW_REGIONS)))
#define GENIO_ROOT_IRQCHIPS						\
	(GENIO_ALL_IRQS(32), GENIO_ALL_IRQS(160), GENIO_ALL_IRQS(288),	\
	 GENIO_ALL_IRQS(416), GENIO_ALL_IRQS(544), GENIO_ALL_IRQS(672),	\
	 GENIO_ALL_IRQS(800))
#define GENIO_ROOT_RESOURCES						\
	(GENIO_ALL_PINS(EINT, MT8188_EINT, 0),				\
	 GENIO_ALL_PINS(EINT, MT8188_EINT, 128),			\
	 GENIO_ALL_PINS(GPIO, MT8188_GPIO, 0),				\
	 GENIO_ALL_PINS(GPIO, MT8188_GPIO, 128),			\
	 GENIO_ALL_PINS(CLK, MT8188_INFRA_CG, 0),			\
	 GENIO_ALL_PINS(CLK, MT8188_INFRA_CG, 128))

#define GENIO_ROOT_CELL(file, smc_ids)					\
	GENIO_ROOT_CELL_(GENIO_ID, file, GENIO_ROOT_MEM_REGIONS,	\
			 GENIO_ROOT_IRQCHIPS, GENIO_ROOT_PCI_DEVICES,	\
			 smc_ids, GENIO_ROOT_RESOURCES)
#define GENIO_ROOT_CELL_(id, file, regions, irqchip_list, pci, smc, res) \
static const struct {							\
	struct jailhouse_system header;					\
	__u64 cpus[1];							\
	struct jailhouse_memory						\
		mem_regions[GENIO_COUNT(struct jailhouse_memory,	\
					regions)];			\
	struct jailhouse_irqchip					\
		irqchips[GENIO_COUNT(struct jailhouse_irqchip,		\
				     irqchip_list)];			\
	struct jailhouse_pci_device					\
		pci_devices[GENIO_COUNT(struct jailhouse_pci_device,	\
					pci)];				\
	__u32 smc_ids[GENIO_COUNT(__u32, smc)];				\
	struct jailhouse_vendor_resource				\
		vendor_resources[GENIO_COUNT(				\
			struct jailhouse_vendor_resource, res)];	\
} __attribute__((packed)) id GENIO_CELL_SECTION(file) = {		\
	.header = {							\
		.signature = JAILHOUSE_SYSTEM_SIGNATURE,		\
		.revision = JAILHOUSE_CONFIG_REVISION,			\
		.architecture = JAILHOUSE_ARM64,			\
		.flags = JAILHOUSE_SYS_VIRTUAL_DEBUG_CONSOLE,		\
		.hypervisor_memory = {					\
			.phys_start = GENIO_HV_MEM,			\
			.size = GENIO_HV_MEM_SIZE,			\
		},							\
		.debug_console = {					\
			.address = MT8188_UART0,			\
			.size = MT8188_UART_SIZE,			\
			.type = JAILHOUSE_CON_TYPE_8250,		\
			.flags = JAILHOUSE_CON_ACCESS_MMIO |		\
				 JAILHOUSE_CON_REGDIST_4,		\
		},							\
		.platform_info = {					\
			.pci_mmconfig_base = GENIO_MMCONFIG,		\
			.pci_mmconfig_end_bus = 0,			\
			.pci_is_virtual = 1,				\
			.pci_domain = 1,				\
			.arm = {					\
				.gic_version = 3,			\
				.gicd_base = MT8188_GICD,		\
				.gicr_base = MT8188_GICR,		\
				.gicr_size = MT8188_GICR_SIZE,		\
				.maintenance_irq =			\
					MT8188_GIC_MAINTENANCE_IRQ,	\
			},						\
		},							\
		.root_cell = {						\
			.name = file,					\
			.cpu_set_size = sizeof(id.cpus),		\
			.num_memory_regions = ARRAY_SIZE(id.mem_regions), \
			.num_irqchips = ARRAY_SIZE(id.irqchips),	\
			.num_pci_devices = ARRAY_SIZE(id.pci_devices),	\
			.num_smc_ids = ARRAY_SIZE(id.smc_ids),		\
			.num_vendor_resources =				\
				ARRAY_SIZE(id.vendor_resources),	\
			.vpci_irq_base = GENIO_ROOT_VPCI_SPI,		\
		},							\
	},								\
	.cpus = {							\
		GENIO_BOARD_CPUS,					\
	},								\
	.mem_regions = {						\
		GENIO_UNPAREN regions					\
	},								\
	.irqchips = {							\
		GENIO_UNPAREN irqchip_list				\
	},								\
	.pci_devices = {						\
		GENIO_UNPAREN pci					\
	},								\
	.smc_ids = {							\
		GENIO_UNPAREN smc					\
	},								\
	.vendor_resources = {						\
		GENIO_UNPAREN res					\
	},								\
}

/* The uart-demo inmate, loaded to 0, prints to UART1 */
#define GENIO_UART_DEMO_CELL(file, cpu_set)				\
	GENIO_UART_DEMO_CELL_(GENIO_ID, file, cpu_set)
#define GENIO_UART_DEMO_CELL_(id, file, cpu_set)			\
GENIO_CHECK_CPUS(file, cpu_set);					\
static const struct {							\
	struct jailhouse_cell_desc cell;				\
	__u64 cpus[1];							\
	struct jailhouse_memory mem_regions[3];				\
	struct jailhouse_vendor_resource vendor_resources[1];		\
} __attribute__((packed)) id GENIO_CELL_SECTION(file) = {		\
	.cell = {							\
		.signature = JAILHOUSE_CELL_DESC_SIGNATURE,		\
		.revision = JAILHOUSE_CONFIG_REVISION,			\
		.architecture = JAILHOUSE_ARM64,			\
		.name = "uart-demo",					\
		.flags = JAILHOUSE_CELL_PASSIVE_COMMREG |		\
			 JAILHOUSE_CELL_VIRTUAL_CONSOLE_PERMITTED,	\
		.cpu_set_size = sizeof(id.cpus),			\
		.num_memory_regions = ARRAY_SIZE(id.mem_regions),	\
		.num_vendor_resources = ARRAY_SIZE(id.vendor_resources), \
		.console = GENIO_UART1_CONSOLE,				\
	},								\
	.cpus = {							\
		cpu_set,						\
	},								\
	.mem_regions = {						\
		GENIO_UART1_REGION,					\
		{							\
			.phys_start = GENIO_INMATE_MEM,			\
			.virt_start = 0,				\
			.size = 0x00010000,				\
			.flags = JAILHOUSE_MEM_READ |			\
				 JAILHOUSE_MEM_WRITE |			\
				 JAILHOUSE_MEM_EXECUTE |		\
				 JAILHOUSE_MEM_LOADABLE,		\
		},							\
		GENIO_COMM_REGION,					\
	},								\
	.vendor_resources = {						\
		CLK_GATES(0, MT8188_UART1_CLK_GATE),			\
	},								\
}

/*
 * An inmate cell, named cell_name. Its image is loaded to
 * GENIO_INMATE_ENTRY in the inmate window, where the lowest CPU of the
 * cell starts; the others wait for PSCI CPU_ON, by SMC. The cell always
 * has UART1 for its console, with its interrupt, and the interrupt of its
 * virtual PCI devices; its other memory regions (first), interrupts, PCI
 * devices, SiP calls, pins and clock gates come from the table. All its
 * interrupts must be within 128 of the lowest, rounded down to 32.
 */
#define GENIO_INMATE_CELL(file, cell_name, cpu_set, regions, irqs, pci, smc, \
			  res)						\
	GENIO_INMATE_CELL_(GENIO_ID, file, cell_name, cpu_set, regions,	\
			   (MT8188_UART1_IRQ,				\
			    GENIO_PCI_IRQ(GENIO_FIRST pci)		\
			    GENIO_COMMA_IF_ANY irqs GENIO_UNPAREN irqs), \
			   pci, smc, res)
/* the INTx of the cell's PCI devices, or the UART's again (no-op) */
#define GENIO_PCI_IRQ(spi)	((spi) ? (spi) + 32 : MT8188_UART1_IRQ)
#define GENIO_IRQ_BASE(irqs)						\
	(GENIO_FOLD(GENIO_IRQ, GENIO_MIN, 0, 0, GENIO_UNPAREN irqs) & ~31)
#define GENIO_IRQ(a, b, irq)	(irq)
#define GENIO_IRQ_BITS(irqs, w)						\
	GENIO_FOLD(GENIO_BIT, GENIO_OR, GENIO_IRQ_BASE(irqs), w,	\
		   GENIO_UNPAREN irqs)
#define GENIO_INMATE_CELL_(id, file, cell_name, cpu_set, regions, irqs, pci, \
			   smc, res)					\
GENIO_CHECK_CPUS(file, cpu_set);					\
_Static_assert(!GENIO_FOLD(GENIO_OUTSIDE, GENIO_OR,			\
			   GENIO_IRQ_BASE(irqs), 128, GENIO_UNPAREN irqs), \
	       file ": the interrupts must be within 128 of the lowest"); \
static const struct {							\
	struct jailhouse_cell_desc cell;				\
	__u64 cpus[1];							\
	struct jailhouse_memory						\
		mem_regions[GENIO_COUNT(struct jailhouse_memory,	\
					regions) + 3];			\
	struct jailhouse_irqchip irqchips[1];				\
	struct jailhouse_pci_device					\
		pci_devices[GENIO_COUNT(struct jailhouse_pci_device,	\
					(GENIO_REST pci))];		\
	__u32 smc_ids[GENIO_COUNT(__u32, smc)];				\
	struct jailhouse_vendor_resource				\
		vendor_resources[GENIO_COUNT(				\
			struct jailhouse_vendor_resource, res)];	\
} __attribute__((packed)) id GENIO_CELL_SECTION(file) = {		\
	.cell = {							\
		.signature = JAILHOUSE_CELL_DESC_SIGNATURE,		\
		.revision = JAILHOUSE_CONFIG_REVISION,			\
		.architecture = JAILHOUSE_ARM64,			\
		.name = cell_name,					\
		.flags = JAILHOUSE_CELL_PASSIVE_COMMREG |		\
			 JAILHOUSE_CELL_VIRTUAL_CONSOLE_PERMITTED,	\
		.cpu_set_size = sizeof(id.cpus),			\
		.num_memory_regions = ARRAY_SIZE(id.mem_regions),	\
		.num_irqchips = ARRAY_SIZE(id.irqchips),		\
		.num_pci_devices = ARRAY_SIZE(id.pci_devices),		\
		.num_smc_ids = ARRAY_SIZE(id.smc_ids),			\
		.num_vendor_resources = ARRAY_SIZE(id.vendor_resources), \
		.vpci_irq_base = GENIO_FIRST pci,			\
		.cpu_reset_address = GENIO_INMATE_ENTRY,		\
		.console = GENIO_UART1_CONSOLE,				\
	},								\
	.cpus = {							\
		cpu_set,						\
	},								\
	.mem_regions = {						\
		GENIO_UNPAREN regions					\
		GENIO_COMMA_IF_ANY regions				\
		GENIO_UART1_REGION,					\
		{							\
			.phys_start = GENIO_INMATE_MEM,			\
			.virt_start = GENIO_INMATE_ENTRY,		\
			.size = GENIO_INMATE_MEM_SIZE,			\
			.flags = JAILHOUSE_MEM_READ |			\
				 JAILHOUSE_MEM_WRITE |			\
				 JAILHOUSE_MEM_EXECUTE |		\
				 JAILHOUSE_MEM_LOADABLE,		\
		},							\
		GENIO_COMM_REGION,					\
	},								\
	.irqchips = {							\
		{							\
			.address = MT8188_GICD,				\
			.pin_base = GENIO_IRQ_BASE(irqs),		\
			.pin_bitmap = {					\
				GENIO_IRQ_BITS(irqs, 0),		\
				GENIO_IRQ_BITS(irqs, 1),		\
				GENIO_IRQ_BITS(irqs, 2),		\
				GENIO_IRQ_BITS(irqs, 3),		\
			},						\
		},							\
	},								\
	.pci_devices = {						\
		GENIO_REST pci						\
	},								\
	.smc_ids = {							\
		GENIO_UNPAREN smc					\
	},								\
	.vendor_resources = {						\
		GENIO_UNPAREN res					\
	},								\
}

/*
 * Preprocessor helpers: lists, folds and checks used by the macros above.
 * A list is a parenthesized, comma-separated sequence, e.g. (a, b, c),
 * possibly empty. GENIO_FOLD() applies m(a, b, x) to each x of a non-empty
 * list of up to 32 elements and combines the results with j().
 */

#define GENIO_UNPAREN(...)	__VA_ARGS__
#define GENIO_UNLIST(list)	GENIO_UNPAREN list
#define GENIO_COUNT(type, list)						\
	(sizeof((type[]){ GENIO_UNPAREN list }) / sizeof(type))
#define GENIO_FIRST(x, ...)	x
#define GENIO_REST(x, ...)	__VA_ARGS__
/* a comma after a non-empty list */
#define GENIO_COMMA_IF_ANY(...)	__VA_OPT__(,)

#define GENIO_OR(x, y)		((x) | (y))
#define GENIO_MIN(x, y)		((x) < (y) ? (x) : (y))
/* the bit of n in word w of a bitmap from base */
#define GENIO_BIT(base, w, n)						\
	((n) >= (base) + 32 * (w) && (n) < (base) + 32 * (w) + 32 ?	\
	 1U << ((n) % 32) : 0)
/* whether n is outside of size from base */
#define GENIO_OUTSIDE(base, size, n)					\
	((n) < (base) || (n) >= (base) + (size))
/* 0, or a build error unless ok */
#define GENIO_CHECK(ok)		((int)sizeof(struct { int: -!(ok); }))

/* a new identifier on each use */
#define GENIO_CAT(a, b)		GENIO_CAT_(a, b)
#define GENIO_CAT_(a, b)	a##b
#define GENIO_ID		GENIO_CAT(genio_cell_, __COUNTER__)

#define GENIO_FOLD(m, j, a, b, ...)					\
	GENIO_FE_N(__VA_ARGS__,						\
		   GENIO_FE_32, GENIO_FE_31, GENIO_FE_30, GENIO_FE_29,	\
		   GENIO_FE_28, GENIO_FE_27, GENIO_FE_26, GENIO_FE_25,	\
		   GENIO_FE_24, GENIO_FE_23, GENIO_FE_22, GENIO_FE_21,	\
		   GENIO_FE_20, GENIO_FE_19, GENIO_FE_18, GENIO_FE_17,	\
		   GENIO_FE_16, GENIO_FE_15, GENIO_FE_14, GENIO_FE_13,	\
		   GENIO_FE_12, GENIO_FE_11, GENIO_FE_10, GENIO_FE_9,	\
		   GENIO_FE_8, GENIO_FE_7, GENIO_FE_6, GENIO_FE_5,	\
		   GENIO_FE_4, GENIO_FE_3, GENIO_FE_2, GENIO_FE_1)	\
		(m, j, a, b, __VA_ARGS__)
#define GENIO_FE_N(							\
	_1, _2, _3, _4, _5, _6, _7, _8, _9, _10,			\
	_11, _12, _13, _14, _15, _16, _17, _18, _19, _20,		\
	_21, _22, _23, _24, _25, _26, _27, _28, _29, _30,		\
	_31, _32,							\
	N, ...) N
#define GENIO_FE_1(m, j, a, b, x)		m(a, b, x)
#define GENIO_FE_2(m, j, a, b, x, ...)					\
	j(m(a, b, x), GENIO_FE_1(m, j, a, b, __VA_ARGS__))
#define GENIO_FE_3(m, j, a, b, x, ...)					\
	j(m(a, b, x), GENIO_FE_2(m, j, a, b, __VA_ARGS__))
#define GENIO_FE_4(m, j, a, b, x, ...)					\
	j(m(a, b, x), GENIO_FE_3(m, j, a, b, __VA_ARGS__))
#define GENIO_FE_5(m, j, a, b, x, ...)					\
	j(m(a, b, x), GENIO_FE_4(m, j, a, b, __VA_ARGS__))
#define GENIO_FE_6(m, j, a, b, x, ...)					\
	j(m(a, b, x), GENIO_FE_5(m, j, a, b, __VA_ARGS__))
#define GENIO_FE_7(m, j, a, b, x, ...)					\
	j(m(a, b, x), GENIO_FE_6(m, j, a, b, __VA_ARGS__))
#define GENIO_FE_8(m, j, a, b, x, ...)					\
	j(m(a, b, x), GENIO_FE_7(m, j, a, b, __VA_ARGS__))
#define GENIO_FE_9(m, j, a, b, x, ...)					\
	j(m(a, b, x), GENIO_FE_8(m, j, a, b, __VA_ARGS__))
#define GENIO_FE_10(m, j, a, b, x, ...)					\
	j(m(a, b, x), GENIO_FE_9(m, j, a, b, __VA_ARGS__))
#define GENIO_FE_11(m, j, a, b, x, ...)					\
	j(m(a, b, x), GENIO_FE_10(m, j, a, b, __VA_ARGS__))
#define GENIO_FE_12(m, j, a, b, x, ...)					\
	j(m(a, b, x), GENIO_FE_11(m, j, a, b, __VA_ARGS__))
#define GENIO_FE_13(m, j, a, b, x, ...)					\
	j(m(a, b, x), GENIO_FE_12(m, j, a, b, __VA_ARGS__))
#define GENIO_FE_14(m, j, a, b, x, ...)					\
	j(m(a, b, x), GENIO_FE_13(m, j, a, b, __VA_ARGS__))
#define GENIO_FE_15(m, j, a, b, x, ...)					\
	j(m(a, b, x), GENIO_FE_14(m, j, a, b, __VA_ARGS__))
#define GENIO_FE_16(m, j, a, b, x, ...)					\
	j(m(a, b, x), GENIO_FE_15(m, j, a, b, __VA_ARGS__))
#define GENIO_FE_17(m, j, a, b, x, ...)					\
	j(m(a, b, x), GENIO_FE_16(m, j, a, b, __VA_ARGS__))
#define GENIO_FE_18(m, j, a, b, x, ...)					\
	j(m(a, b, x), GENIO_FE_17(m, j, a, b, __VA_ARGS__))
#define GENIO_FE_19(m, j, a, b, x, ...)					\
	j(m(a, b, x), GENIO_FE_18(m, j, a, b, __VA_ARGS__))
#define GENIO_FE_20(m, j, a, b, x, ...)					\
	j(m(a, b, x), GENIO_FE_19(m, j, a, b, __VA_ARGS__))
#define GENIO_FE_21(m, j, a, b, x, ...)					\
	j(m(a, b, x), GENIO_FE_20(m, j, a, b, __VA_ARGS__))
#define GENIO_FE_22(m, j, a, b, x, ...)					\
	j(m(a, b, x), GENIO_FE_21(m, j, a, b, __VA_ARGS__))
#define GENIO_FE_23(m, j, a, b, x, ...)					\
	j(m(a, b, x), GENIO_FE_22(m, j, a, b, __VA_ARGS__))
#define GENIO_FE_24(m, j, a, b, x, ...)					\
	j(m(a, b, x), GENIO_FE_23(m, j, a, b, __VA_ARGS__))
#define GENIO_FE_25(m, j, a, b, x, ...)					\
	j(m(a, b, x), GENIO_FE_24(m, j, a, b, __VA_ARGS__))
#define GENIO_FE_26(m, j, a, b, x, ...)					\
	j(m(a, b, x), GENIO_FE_25(m, j, a, b, __VA_ARGS__))
#define GENIO_FE_27(m, j, a, b, x, ...)					\
	j(m(a, b, x), GENIO_FE_26(m, j, a, b, __VA_ARGS__))
#define GENIO_FE_28(m, j, a, b, x, ...)					\
	j(m(a, b, x), GENIO_FE_27(m, j, a, b, __VA_ARGS__))
#define GENIO_FE_29(m, j, a, b, x, ...)					\
	j(m(a, b, x), GENIO_FE_28(m, j, a, b, __VA_ARGS__))
#define GENIO_FE_30(m, j, a, b, x, ...)					\
	j(m(a, b, x), GENIO_FE_29(m, j, a, b, __VA_ARGS__))
#define GENIO_FE_31(m, j, a, b, x, ...)					\
	j(m(a, b, x), GENIO_FE_30(m, j, a, b, __VA_ARGS__))
#define GENIO_FE_32(m, j, a, b, x, ...)					\
	j(m(a, b, x), GENIO_FE_31(m, j, a, b, __VA_ARGS__))

#endif /* !_GENIO_EVK_H */
