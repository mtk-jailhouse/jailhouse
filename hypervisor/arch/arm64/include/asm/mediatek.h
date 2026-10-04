/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) MediaTek Inc., 2025-2026
 *
 * Authors:
 *  Felix Freimann <felix.freimann@mediatek.com>
 *  Aary Patil <aary.patil@mediatek.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#ifndef _JAILHOUSE_ASM_MEDIATEK_H
#define _JAILHOUSE_ASM_MEDIATEK_H

#include <jailhouse/types.h>

/* index into arch_cell.mtk_bitmap */
#define MTK_EINT		0
#define MTK_GPIO		1
#define MTK_CLK			2

/*
 * Access types of mediated registers: reads return owned fields only, writes
 * affect owned fields only.
 */
#define MTK_RO			0
#define MTK_WO			1
#define MTK_RW			2

/*
 * Register n of a group is at offset + n * stride. It holds 32 / width
 * fields of width bits, one per pin, starting with pin first_pin + n *
 * (32 / width).
 */
struct mtk_reg_group {
	u16 offset;
	u16 stride;
	u8 count;
	u8 access;
	u8 width;
	u16 first_pin;
};

#define MTK_REGS(_offset, _stride, _count, _access, _width, _first_pin)	\
	{								\
		.offset = _offset,					\
		.stride = _stride,					\
		.count = _count,					\
		.access = _access,					\
		.width = _width,					\
		.first_pin = _first_pin,				\
	}

/* A field of register at offset, owned by the cell that owns pin. */
struct mtk_pin_field {
	u8 pin;
	u8 shift;
	u8 width;
};

/* fields[first] to fields[first + count - 1] are in the register at offset */
struct mtk_field_reg {
	u16 offset;
	u16 first;
	u8 count;
};

/*
 * An additional register window of a block, at a fixed address, with
 * irregularly laid out fields, e.g. the pin configuration of the GPIO block.
 * A field shared by several pins is listed for each of them and belongs to
 * a cell only if it owns all of them.
 */
struct mtk_window {
	unsigned long phys;
	unsigned long size;
	const struct mtk_field_reg *regs;
	unsigned int num_regs;
	const struct mtk_pin_field *fields;
};

#define MTK_MAX_WINDOWS		4

/*
 * A register block whose pins (or clock gates) are distributed among cells
 * by config entries of the given type, plus optional windows. Registers
 * outside of the groups and fields are reserved to the root cell.
 */
struct mtk_block {
	const char *name;
	u32 type;
	unsigned int kind;
	unsigned long size;
	const struct mtk_reg_group *groups;
	unsigned int num_groups;
	const struct mtk_window *windows;
	unsigned int num_windows;
	/*
	 * The root cell may write the registers not described here, as they
	 * do not control pins. Otherwise, it may only read them.
	 */
	bool root_owns_rest;
	/*
	 * Optional shared IRQ, delivered to the cell owning a pin that is set
	 * in the status registers. Optional disable registers, pins are
	 * written to when they change their owner. Both have one bit per pin,
	 * starting with pin 0, in num_regs registers.
	 */
	u16 irq;
	u16 status;
	u16 disable;
	u8 num_regs;
};

extern const struct mtk_block mt8188_eint, mt8188_gpio, mt8188_clk;
extern const struct mtk_window mt8188_iocfg[4];

#endif /* !_JAILHOUSE_ASM_MEDIATEK_H */
