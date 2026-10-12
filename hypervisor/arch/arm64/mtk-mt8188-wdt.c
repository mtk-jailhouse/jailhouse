/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) 2026 MediaTek
 *
 * MT8188 watchdog: the TOPRGU block and its subsystem resets.
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <asm/mtk-common-wdt.h>
#include <jailhouse/unit.h>

static const wdt_config_descr_t mt8188_wdt_config_descr = {
	/* the page; the registers take its first 0x100 bytes */
	.reg_size = 0x1000,
	/* 24 subsystem resets, MT8188_TOPRGU_SW_RST_NUM */
	.swsysrst_mask = 0x00ffffff,
};

static int mt8188_wdt_init(void)
{
	return mtk_wdt_init(&mt8188_wdt_config_descr);
}

static void mt8188_wdt_shutdown(void)
{
	mtk_wdt_shutdown();
}

static unsigned int mt8188_wdt_mmio_count_regions(struct cell *cell)
{
	return mtk_wdt_mmio_count_regions(cell);
}

static int mt8188_wdt_cell_init(struct cell *cell)
{
	return mtk_wdt_cell_init(cell);
}

static void mt8188_wdt_cell_exit(struct cell *cell)
{
	mtk_wdt_cell_exit(cell);
}

static void mt8188_wdt_cell_reset(struct cell *cell)
{
	mtk_wdt_cell_reset(cell);
}

DEFINE_UNIT_WITH_CELL_RESET(mt8188_wdt, "mt8188_wdt");
