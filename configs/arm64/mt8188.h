/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * MT8188 (MT8370, MT8390): addresses and interrupts of the SoC, and the
 * memory map of MediaTek's board support package for it
 *
 * Copyright (c) MediaTek Inc., 2025-2026
 *
 * Authors:
 *  Felix Freimann <felix.freimann@mediatek.com>
 *  Aary Patil <aary.patil@mediatek.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * Interrupts are GIC interrupt IDs (SPI number + 32).
 */

#ifndef _MT8188_H
#define _MT8188_H

/* GIC-600; disabled cores keep their redistributors */
#define MT8188_GICD			0x0c000000
#define MT8188_GICR			0x0c040000
#define MT8188_GICR_SIZE		0x00200000
#define MT8188_GIC_MAINTENANCE_IRQ	25

/* 8250-compatible UARTs, register spacing 4 */
#define MT8188_UART0			0x11001100
#define MT8188_UART1			0x11001200
#define MT8188_UART_SIZE		0x100
#define MT8188_UART1_IRQ		(142 + 32)
#define MT8188_UART1_CLK_GATE		23

/*
 * Register blocks shared between cells and mediated by the hypervisor:
 * the clock gates at the start of infracfg_ao, GPIO and EINT. The pin
 * configuration (IOCFG) blocks of the GPIO controller are not mediated:
 * the root cell maps them.
 */
#define MT8188_INFRA_CG			0x10001000
#define MT8188_INFRA_CG_SIZE		0x400
#define MT8188_INFRA_CG0_CLR		(MT8188_INFRA_CG + 0x84)
#define MT8188_GPIO			0x10005000
#define MT8188_EINT			0x1000b000
#define MT8188_EINT_IRQ			(235 + 32)

/* The audio front end, and the blocks its drivers program */
#define MT8188_TOPCKGEN			0x10000000
#define MT8188_INFRACFG_AO		0x10001000
#define MT8188_INFRACFG_AO_SIZE		0x1000
#define MT8188_TOPRGU			0x10007000
#define MT8188_APMIXEDSYS		0x1000c000
#define MT8188_AFE			0x10b10000
#define MT8188_AFE_SIZE			0x10000
#define MT8188_ADSP_AUDIO26M		0x10b91000
#define MT8188_AFE_IRQ			(822 + 32)

/* SiP calls (MTK_SIP_*) of Linux */
#define MT8188_SIP_KERNEL_TIME_SYNC	0xc2000202
#define MT8188_SIP_KERNEL_DFD		0xc2000205
#define MT8188_SIP_KERNEL_MSDC		0xc2000273
#define MT8188_SIP_VCORE_CONTROL	0xc2000506
#define MT8188_SIP_MTK_LPM_CONTROL	0xc2000507
#define MT8188_SIP_IOMMU_CONTROL	0xc2000514
#define MT8188_SIP_AUDIO_CONTROL	0xc2000517
#define MT8188_SIP_DISP_CONTROL		0xc200051c
#define MT8188_SIP_APUSYS_CONTROL	0xc200051e
#define MT8188_SIP_DP_CONTROL		0xc2000523
#define MT8188_SIP_KERNEL_GIC_OP	0xc2000526
#define MT8188_SIP_PLAT_BINFO		0xc2000529

/*
 * The BSP's memory map below 0x6ac00000: DRAM starts at 0x40000000; OP-TEE
 * and the secure monitor are inside the RAM ranges, the coprocessors' and
 * the audio DSP's memory is reserved for them.
 */
#define MT8188_DRAM			0x40000000
#define MT8188_SCP_MEM			0x50000000
#define MT8188_SCP_MEM_SIZE		0x02900000
#define MT8188_APU_MEM			0x55000000
#define MT8188_APU_MEM_SIZE		0x01400000
#define MT8188_VPU_MEM			0x57000000
#define MT8188_VPU_MEM_SIZE		0x01400000
#define MT8188_ADSP_MEM			0x60000000
#define MT8188_ADSP_MEM_SIZE		0x01800000
#define MT8188_AUDIO_DMA		0x61000000
#define MT8188_AUDIO_DMA_SIZE		0x00800000

/* memory region initializers, identity-mapped */
#define MT8188_REGION(start, end, region_flags)				\
	{								\
		.phys_start = (start),					\
		.virt_start = (start),					\
		.size = (end) - (start),				\
		.flags = (region_flags),				\
	}
#define MT8188_MMIO(start, end)						\
	MT8188_REGION(start, end, JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE | \
		      JAILHOUSE_MEM_IO)
#define MT8188_RAM(start, end)						\
	MT8188_REGION(start, end, JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE | \
		      JAILHOUSE_MEM_EXECUTE)
#define MT8188_RESERVED(start, size)					\
	MT8188_REGION(start, (start) + (size),				\
		      JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE)

/*
 * All MMIO below 0x20000000 except the GIC and the mediated blocks. The
 * rest of infracfg_ao takes 32-bit accesses only.
 */
#define MT8188_MMIO_REGIONS						\
	MT8188_MMIO(0, MT8188_GICD),					\
	MT8188_MMIO(MT8188_GICR + MT8188_GICR_SIZE, MT8188_INFRA_CG),	\
	MT8188_REGION(MT8188_INFRA_CG + MT8188_INFRA_CG_SIZE,		\
		      MT8188_INFRACFG_AO + MT8188_INFRACFG_AO_SIZE,	\
		      JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |	\
		      JAILHOUSE_MEM_IO | JAILHOUSE_MEM_IO_32),		\
	MT8188_MMIO(MT8188_INFRACFG_AO + MT8188_INFRACFG_AO_SIZE,	\
		    MT8188_GPIO),					\
	MT8188_MMIO(MT8188_GPIO + 0x1000, MT8188_EINT),			\
	MT8188_MMIO(MT8188_EINT + 0x1000, 0x20000000)

/*
 * Memory up to end: the BSP's RAM, with OP-TEE and the secure monitor
 * inside, and its reservations for the coprocessors and the audio DSP,
 * whose last 8 MiB are the audio DMA buffers
 */
#define MT8188_BSP_MEM_REGIONS(end)					\
	MT8188_RAM(0x20000000, MT8188_DRAM),				\
	MT8188_RAM(MT8188_DRAM, MT8188_SCP_MEM),			\
	MT8188_RESERVED(MT8188_SCP_MEM, MT8188_SCP_MEM_SIZE),		\
	MT8188_RAM(MT8188_SCP_MEM + MT8188_SCP_MEM_SIZE, MT8188_APU_MEM), \
	MT8188_RESERVED(MT8188_APU_MEM, MT8188_APU_MEM_SIZE),		\
	MT8188_RAM(MT8188_APU_MEM + MT8188_APU_MEM_SIZE, MT8188_VPU_MEM), \
	MT8188_RESERVED(MT8188_VPU_MEM, MT8188_VPU_MEM_SIZE),		\
	MT8188_RAM(MT8188_VPU_MEM + MT8188_VPU_MEM_SIZE, MT8188_ADSP_MEM), \
	MT8188_RESERVED(MT8188_ADSP_MEM, MT8188_ADSP_MEM_SIZE),	\
	MT8188_RAM(MT8188_ADSP_MEM + MT8188_ADSP_MEM_SIZE, end)

/* SiP calls of Linux */
#define MT8188_LINUX_SIP_CALLS						\
	MT8188_SIP_KERNEL_TIME_SYNC,					\
	MT8188_SIP_KERNEL_DFD,						\
	MT8188_SIP_KERNEL_MSDC,						\
	MT8188_SIP_VCORE_CONTROL,					\
	MT8188_SIP_MTK_LPM_CONTROL,					\
	MT8188_SIP_IOMMU_CONTROL,					\
	MT8188_SIP_AUDIO_CONTROL,					\
	MT8188_SIP_DISP_CONTROL,					\
	MT8188_SIP_APUSYS_CONTROL,					\
	MT8188_SIP_DP_CONTROL,						\
	MT8188_SIP_KERNEL_GIC_OP,					\
	MT8188_SIP_PLAT_BINFO

#endif /* !_MT8188_H */
