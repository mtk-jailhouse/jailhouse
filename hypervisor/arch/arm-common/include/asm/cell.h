/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013-2016
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#ifndef _JAILHOUSE_ASM_CELL_H
#define _JAILHOUSE_ASM_CELL_H

#include <jailhouse/paging.h>

struct pvu_tlb_entry;

struct arch_cell {
	struct paging_structures mm;

	u32 irq_bitmap[1024/32];

	/* pins owned in the MediaTek EINT, GPIO and clock gate blocks */
	u32 eint_bitmap [256 / 32];
	u32 gpio_bitmap [256 / 32];
	u32 clk_bitmap  [256 / 32];

	struct {
		u8 ent_count;
		struct pvu_tlb_entry *entries;
	} iommu_pvu; /**< ARM PVU specific fields. */

	/* denied SiP calls reported so far */
	u32 denied_sip_ids[8];
	unsigned int num_denied_sip_ids;
};

#endif /* !_JAILHOUSE_ASM_CELL_H */
