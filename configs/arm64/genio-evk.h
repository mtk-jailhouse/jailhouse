/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Root cell for Genio EVKs with MT8188-based SoCs
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
 * The including board config defines GENIO_NAME, GENIO_CPUS and
 * GENIO_DRAM_END. Linux has to reserve the memory of the hypervisor
 * (0x6ac00000, 4 MiB) and of the inmate (0x6b000000, 8 MiB).
 */

#include <jailhouse/types.h>
#include <jailhouse/cell-config.h>

struct {
	struct jailhouse_system header;
	__u64 cpus[1];
	struct jailhouse_memory mem_regions[22];
	struct jailhouse_irqchip irqchips[7];
	__u32 smc_ids[12];
	struct jailhouse_vendor_resource vendor_resources[6];
} __attribute__((packed)) config = {
	.header = {
		.signature = JAILHOUSE_SYSTEM_SIGNATURE,
		.revision = JAILHOUSE_CONFIG_REVISION,
		.architecture = JAILHOUSE_ARM64,
		.flags = JAILHOUSE_SYS_VIRTUAL_DEBUG_CONSOLE,
		.hypervisor_memory = {
			.phys_start = 0x6ac00000,
			.size = 0x00400000,
		},
		.debug_console = {
			.address = 0x11001100,
			.size = 0x100,
			.type = JAILHOUSE_CON_TYPE_8250,
			.flags = JAILHOUSE_CON_ACCESS_MMIO |
				 JAILHOUSE_CON_REGDIST_4,
		},
		.platform_info = {
			.arm = {
				.gic_version = 3,
				.gicd_base = 0x0c000000,
				.gicr_base = 0x0c040000,
				.gicr_size = 0x00200000,
				.maintenance_irq = 25,
			},
		},
		.root_cell = {
			.name = GENIO_NAME,

			.cpu_set_size = sizeof(config.cpus),
			.num_memory_regions = ARRAY_SIZE(config.mem_regions),
			.num_irqchips = ARRAY_SIZE(config.irqchips),
			.num_smc_ids = ARRAY_SIZE(config.smc_ids),
			.num_vendor_resources =
				ARRAY_SIZE(config.vendor_resources),
		},
	},

	.cpus = {
		GENIO_CPUS,
	},

	/*
	 * Not mapped, but mediated by the hypervisor: GIC, clock gates
	 * (0x10001000), GPIO (0x10005000), EINT (0x1000b000) and the pin
	 * configuration (0x11c00000, 0x11e10000, 0x11e20000, 0x11ea0000).
	 */
	.mem_regions = {
		/* MMIO */ {
			.phys_start = 0x00000000,
			.virt_start = 0x00000000,
			.size = 0x0c000000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* MMIO */ {
			.phys_start = 0x0c240000,
			.virt_start = 0x0c240000,
			.size = 0x03dc1000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* MMIO: infracfg_ao, after the clock gates */ {
			.phys_start = 0x10001400,
			.virt_start = 0x10001400,
			.size = 0x00000c00,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO | JAILHOUSE_MEM_IO_32,
		},
		/* MMIO */ {
			.phys_start = 0x10002000,
			.virt_start = 0x10002000,
			.size = 0x00003000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* MMIO */ {
			.phys_start = 0x10006000,
			.virt_start = 0x10006000,
			.size = 0x00005000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* MMIO */ {
			.phys_start = 0x1000c000,
			.virt_start = 0x1000c000,
			.size = 0x01bf4000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* MMIO */ {
			.phys_start = 0x11c01000,
			.virt_start = 0x11c01000,
			.size = 0x0020f000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* MMIO */ {
			.phys_start = 0x11e11000,
			.virt_start = 0x11e11000,
			.size = 0x0000f000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* MMIO */ {
			.phys_start = 0x11e21000,
			.virt_start = 0x11e21000,
			.size = 0x0007f000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* MMIO */ {
			.phys_start = 0x11ea1000,
			.virt_start = 0x11ea1000,
			.size = 0x0e15f000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_IO,
		},
		/* RAM */ {
			.phys_start = 0x20000000,
			.virt_start = 0x20000000,
			.size = 0x20000000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE,
		},
		/* RAM, incl. OP-TEE */ {
			.phys_start = 0x40000000,
			.virt_start = 0x40000000,
			.size = 0x10000000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE,
		},
		/* SCP */ {
			.phys_start = 0x50000000,
			.virt_start = 0x50000000,
			.size = 0x02900000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* RAM, incl. secure monitor */ {
			.phys_start = 0x52900000,
			.virt_start = 0x52900000,
			.size = 0x02700000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE,
		},
		/* APU */ {
			.phys_start = 0x55000000,
			.virt_start = 0x55000000,
			.size = 0x01400000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* RAM */ {
			.phys_start = 0x56400000,
			.virt_start = 0x56400000,
			.size = 0x00c00000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE,
		},
		/* VPU */ {
			.phys_start = 0x57000000,
			.virt_start = 0x57000000,
			.size = 0x01400000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* RAM */ {
			.phys_start = 0x58400000,
			.virt_start = 0x58400000,
			.size = 0x07c00000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE,
		},
		/* ADSP and audio DMA */ {
			.phys_start = 0x60000000,
			.virt_start = 0x60000000,
			.size = 0x01800000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE,
		},
		/* RAM */ {
			.phys_start = 0x61800000,
			.virt_start = 0x61800000,
			.size = 0x09400000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE,
		},
		/* RAM: inmate */ {
			.phys_start = 0x6b000000,
			.virt_start = 0x6b000000,
			.size = 0x00800000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE,
		},
		/* RAM */ {
			.phys_start = 0x6b800000,
			.virt_start = 0x6b800000,
			.size = GENIO_DRAM_END - 0x6b800000,
			.flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
				JAILHOUSE_MEM_EXECUTE,
		},
	},

	.irqchips = {
		/* GIC */ {
			.address = 0x0c000000,
			.pin_base = 32,
			.pin_bitmap = {
				0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
			},
		},
		/* GIC */ {
			.address = 0x0c000000,
			.pin_base = 160,
			.pin_bitmap = {
				0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
			},
		},
		/* GIC */ {
			.address = 0x0c000000,
			.pin_base = 288,
			.pin_bitmap = {
				0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
			},
		},
		/* GIC */ {
			.address = 0x0c000000,
			.pin_base = 416,
			.pin_bitmap = {
				0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
			},
		},
		/* GIC */ {
			.address = 0x0c000000,
			.pin_base = 544,
			.pin_bitmap = {
				0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
			},
		},
		/* GIC */ {
			.address = 0x0c000000,
			.pin_base = 672,
			.pin_bitmap = {
				0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
			},
		},
		/* GIC */ {
			.address = 0x0c000000,
			.pin_base = 800,
			.pin_bitmap = {
				0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
			},
		},
	},

	/* SiP calls of Linux */
	.smc_ids = {
		0xc2000202,	/* MTK_SIP_KERNEL_TIME_SYNC */
		0xc2000205,	/* MTK_SIP_KERNEL_DFD */
		0xc2000273,	/* MTK_SIP_KERNEL_MSDC */
		0xc2000506,	/* MTK_SIP_VCORE_CONTROL */
		0xc2000507,	/* MTK_SIP_MTK_LPM_CONTROL */
		0xc2000514,	/* MTK_SIP_IOMMU_CONTROL */
		0xc2000517,	/* MTK_SIP_AUDIO_CONTROL */
		0xc200051c,	/* MTK_SIP_DISP_CONTROL */
		0xc200051e,	/* MTK_SIP_APUSYS_CONTROL */
		0xc2000523,	/* MTK_SIP_DP_CONTROL */
		0xc2000526,	/* MTK_SIP_KERNEL_GIC_OP */
		0xc2000529,	/* MTK_SIP_PLAT_BINFO */
	},

	/* all pins and clock gates, until handed to other cells */
	.vendor_resources = {
		{
			.type = JAILHOUSE_VENDOR_MT8188_EINT,
			.address = 0x1000b000,
			.base = 0,
			.bitmap = {
				0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
			},
		},
		{
			.type = JAILHOUSE_VENDOR_MT8188_EINT,
			.address = 0x1000b000,
			.base = 128,
			.bitmap = {
				0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
			},
		},
		{
			.type = JAILHOUSE_VENDOR_MT8188_GPIO,
			.address = 0x10005000,
			.base = 0,
			.bitmap = {
				0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
			},
		},
		{
			.type = JAILHOUSE_VENDOR_MT8188_GPIO,
			.address = 0x10005000,
			.base = 128,
			.bitmap = {
				0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
			},
		},
		{
			.type = JAILHOUSE_VENDOR_MT8188_CLK,
			.address = 0x10001000,
			.base = 0,
			.bitmap = {
				0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
			},
		},
		{
			.type = JAILHOUSE_VENDOR_MT8188_CLK,
			.address = 0x10001000,
			.base = 128,
			.bitmap = {
				0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
			},
		},
	},
};
