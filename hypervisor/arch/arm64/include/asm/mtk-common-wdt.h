/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) 2026 MediaTek
 *
 * Common part for the watchdog (TOPRGU) of MediaTek SoCs: passthrough for the
 * root cell, an emulated watchdog of its own for each other cell.
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <asm/mtk-common.h>
#include <jailhouse/cell.h>
#include <jailhouse/mmio.h>


typedef struct
{
    /* Size of the register block. */
    size_t  reg_size;

    /* The subsystem resets in SWSYSRST. */
    u32     swsysrst_mask;
}   wdt_config_descr_t;


extern int mtk_wdt_cell_init (struct cell*  cell);
extern void mtk_wdt_cell_exit (struct cell*  cell);
extern void mtk_wdt_cell_reset (struct cell*  cell);

extern unsigned int mtk_wdt_mmio_count_regions (struct cell*  cell);

extern int mtk_wdt_init (const wdt_config_descr_t*  wdt_config_descr);
extern void mtk_wdt_shutdown (void);
