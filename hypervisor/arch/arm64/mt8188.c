/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) MediaTek Inc., 2025-2026
 *
 * MT8188 (MT8370, MT8390) register blocks shared between cells
 *
 * Authors:
 *  Felix Freimann <felix.freimann@mediatek.com>
 *  Aary Patil <aary.patil@mediatek.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/utils.h>
#include <asm/mediatek.h>

#include <jailhouse/cell-config.h>

/* 225 EINTs, one bit each in 8 registers */
#define EINT_REGS(offset, access)	MTK_REGS(offset, 4, 8, access, 1, 0)
/* debounce of EINT 0-31, eight bits each in 8 registers */
#define EINT_DBNC_REGS(offset, access)	MTK_REGS(offset, 4, 8, access, 8, 0)

static const struct mtk_reg_group eint_regs[] = {
	EINT_REGS(0x000, MTK_RO),	/* STA */
	EINT_REGS(0x040, MTK_WO),	/* ACK */
	EINT_REGS(0x080, MTK_RO),	/* MASK */
	EINT_REGS(0x0c0, MTK_WO),	/* MASK_SET */
	EINT_REGS(0x100, MTK_WO),	/* MASK_CLR */
	EINT_REGS(0x140, MTK_RO),	/* SENS */
	EINT_REGS(0x180, MTK_WO),	/* SENS_SET */
	EINT_REGS(0x1c0, MTK_WO),	/* SENS_CLR */
	EINT_REGS(0x200, MTK_RO),	/* SOFT */
	EINT_REGS(0x240, MTK_WO),	/* SOFT_SET */
	EINT_REGS(0x280, MTK_WO),	/* SOFT_CLR */
	EINT_REGS(0x300, MTK_RO),	/* POL */
	EINT_REGS(0x340, MTK_WO),	/* POL_SET */
	EINT_REGS(0x380, MTK_WO),	/* POL_CLR */
	EINT_REGS(0x400, MTK_RW),	/* D0EN */
	EINT_DBNC_REGS(0x500, MTK_RO),	/* DBNC_CTRL */
	EINT_DBNC_REGS(0x600, MTK_WO),	/* DBNC_SET */
	EINT_DBNC_REGS(0x700, MTK_WO),	/* DBNC_CLR */
	EINT_REGS(0x800, MTK_RO),	/* EVENT */
	EINT_REGS(0x880, MTK_WO),	/* EVENT_CLR */
	EINT_REGS(0xa00, MTK_RO),	/* RAW_STA */
};

const struct mtk_block mt8188_eint = {
	.name = "EINT",
	.type = JAILHOUSE_VENDOR_MT8188_EINT,
	.kind = MTK_EINT,
	.size = 0x1000,
	.groups = eint_regs,
	.num_groups = ARRAY_SIZE(eint_regs),
	.irq = 235 + 32,
	.status = 0x000,
	.num_regs = 8,
	.disable = 0x0c0,
};

/*
 * 178 pins, one bit each in 6 registers, or four in 23 registers. Their
 * configuration (pull, drive, ...) is in four IOCFG windows.
 */
#define GPIO_REGS(offset, access)	MTK_REGS(offset, 0x10, 6, access, 1, 0)
#define GPIO_MODE_REGS(offset, access)	\
	MTK_REGS(offset, 0x10, 23, access, 4, 0)

static const struct mtk_reg_group gpio_regs[] = {
	GPIO_REGS(0x000, MTK_RW),		/* DIR */
	GPIO_REGS(0x004, MTK_WO),		/* DIR_SET */
	GPIO_REGS(0x008, MTK_WO),		/* DIR_CLR */
	GPIO_REGS(0x100, MTK_RW),		/* DOUT */
	GPIO_REGS(0x104, MTK_WO),		/* DOUT_SET */
	GPIO_REGS(0x108, MTK_WO),		/* DOUT_CLR */
	GPIO_REGS(0x200, MTK_RO),		/* DIN */
	GPIO_MODE_REGS(0x300, MTK_RW),		/* MODE */
	GPIO_MODE_REGS(0x304, MTK_WO),		/* MODE_SET */
	GPIO_MODE_REGS(0x308, MTK_WO),		/* MODE_CLR */
	GPIO_MODE_REGS(0x30c, MTK_WO),		/* MODE_MOD */
};

const struct mtk_block mt8188_gpio = {
	.name = "GPIO",
	.type = JAILHOUSE_VENDOR_MT8188_GPIO,
	.kind = MTK_GPIO,
	.size = 0x1000,
	.groups = gpio_regs,
	.num_groups = ARRAY_SIZE(gpio_regs),
	.windows = mt8188_iocfg,
	.num_windows = ARRAY_SIZE(mt8188_iocfg),
};

/* infracfg_ao clock gates: 5 groups of 32 with set, clear and status */
#define CG_REGS(offset, stride, count, access, first_gate)		\
	MTK_REGS(offset, stride, count, access, 1, first_gate)

static const struct mtk_reg_group clk_regs[] = {
	CG_REGS(0x080, 8, 2, MTK_WO, 0),	/* CG0_SET, CG1_SET */
	CG_REGS(0x084, 8, 2, MTK_WO, 0),	/* CG0_CLR, CG1_CLR */
	CG_REGS(0x090, 4, 2, MTK_RW, 0),	/* CG0_STA, CG1_STA */
	CG_REGS(0x0a4, 4, 1, MTK_WO, 64),	/* CG2_SET */
	CG_REGS(0x0a8, 4, 1, MTK_WO, 64),	/* CG2_CLR */
	CG_REGS(0x0ac, 4, 1, MTK_RW, 64),	/* CG2_STA */
	CG_REGS(0x0c0, 4, 1, MTK_WO, 96),	/* CG3_SET */
	CG_REGS(0x0c4, 4, 1, MTK_WO, 96),	/* CG3_CLR */
	CG_REGS(0x0c8, 4, 1, MTK_RW, 96),	/* CG3_STA */
	CG_REGS(0x0e0, 4, 1, MTK_WO, 128),	/* CG4_SET */
	CG_REGS(0x0e4, 4, 1, MTK_WO, 128),	/* CG4_CLR */
	CG_REGS(0x0e8, 4, 1, MTK_RW, 128),	/* CG4_STA */
};

const struct mtk_block mt8188_clk = {
	.name = "clock gates",
	.type = JAILHOUSE_VENDOR_MT8188_CLK,
	.kind = MTK_CLK,
	.size = 0x400,
	.groups = clk_regs,
	.num_groups = ARRAY_SIZE(clk_regs),
	.root_owns_rest = true,
};
