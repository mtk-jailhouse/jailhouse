/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) MediaTek Inc., 2025-2026
 *
 * MediaTek register blocks whose pins or clock gates are distributed among
 * cells: EINT, GPIO and its pin configuration, and clock gates
 *
 * Authors:
 *  Felix Freimann <felix.freimann@mediatek.com>
 *  Aary Patil <aary.patil@mediatek.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <jailhouse/control.h>
#include <jailhouse/mmio.h>
#include <jailhouse/paging.h>
#include <jailhouse/printk.h>
#include <jailhouse/string.h>
#include <jailhouse/unit.h>
#include <asm/irqchip.h>
#include <asm/mediatek.h>
#include <asm/spinlock.h>

#define for_each_vendor_resource(res, config, counter)			\
	for ((res) = jailhouse_cell_vendor_resources(config), (counter) = 0; \
	     (counter) < (config)->num_vendor_resources;		\
	     (res)++, (counter)++)

#define for_each_window(ws, state)					\
	for ((ws) = (state)->windows;					\
	     (ws) < &(state)->windows[(state)->num_windows]; (ws)++)

#define NUM_KINDS	ARRAY_SIZE(root_cell.arch.mtk_bitmap)
#define BITMAP_WORDS	ARRAY_SIZE(root_cell.arch.mtk_bitmap[0])

static const struct mtk_block *const blocks[] = {
	&mt8188_eint, &mt8188_gpio, &mt8188_clk,
};

struct mtk_state;

/* a mapped window: the block's registers if window is NULL */
struct mtk_window_state {
	struct mtk_state *state;
	const struct mtk_window *window;
	unsigned long phys;
	unsigned long size;
	void *base;
	bool write_reported;
};

/* the active block of each kind */
static struct mtk_state {
	const struct mtk_block *block;
	spinlock_t lock;
	struct mtk_window_state windows[1 + MTK_MAX_WINDOWS];
	unsigned int num_windows;
} states[NUM_KINDS];

static int get_config_bitmap(const struct jailhouse_cell_desc *config,
			     const struct mtk_block *block, unsigned long *phys,
			     u32 *bitmap)
{
	const struct jailhouse_vendor_resource *res;
	unsigned int n, pos;

	memset(bitmap, 0, BITMAP_WORDS * sizeof(u32));

	for_each_vendor_resource(res, config, n) {
		if (res->type != block->type)
			continue;
		if ((*phys && res->address != *phys) || res->base % 32 != 0 ||
		    res->base / 32 + ARRAY_SIZE(res->bitmap) > BITMAP_WORDS)
			return trace_error(-EINVAL);
		*phys = res->address;
		for (pos = 0; pos < ARRAY_SIZE(res->bitmap); pos++)
			bitmap[res->base / 32 + pos] |= res->bitmap[pos];
	}

	return 0;
}

static bool config_maps(const struct jailhouse_cell_desc *config,
			unsigned long start, unsigned long size)
{
	const struct jailhouse_memory *mem;
	unsigned int n;

	for_each_mem_region(mem, config, n)
		if (mem->phys_start < start + size &&
		    start < mem->phys_start + mem->size)
			return true;

	return false;
}

static void disable_pins(struct mtk_state *state, const u32 *bitmap)
{
	const struct mtk_block *block = state->block;
	unsigned int n;

	if (!block->disable)
		return;

	for (n = 0; n < block->num_regs; n++)
		if (bitmap[n])
			mmio_write32(state->windows[0].base + block->disable +
				     n * 4, bitmap[n]);
}

static const struct mtk_reg_group *find_group(const struct mtk_block *block,
					      unsigned long offset,
					      unsigned int *n)
{
	const struct mtk_reg_group *group;
	unsigned int i;

	for (i = 0; i < block->num_groups; i++) {
		group = &block->groups[i];
		if (offset < group->offset ||
		    (offset - group->offset) % group->stride != 0)
			continue;
		*n = (offset - group->offset) / group->stride;
		if (*n < group->count)
			return group;
	}

	return NULL;
}

/* returns the mask of the fields in register n of group owned per bitmap */
static u32 owned_fields(const struct mtk_reg_group *group, unsigned int n,
			const u32 *bitmap)
{
	unsigned int fields = 32 / group->width;
	unsigned int pin = group->first_pin + n * fields;
	u32 field = (1U << group->width) - 1;
	u32 owned, mask = 0;
	unsigned int i;

	if (pin / 32 >= BITMAP_WORDS)
		return 0;

	owned = bitmap[pin / 32] >> (pin % 32);
	if (group->width == 1)
		return owned;

	for (i = 0; i < fields; i++)
		if (owned & (1U << i))
			mask |= field << (i * group->width);

	return mask;
}

/* returns the mask of the fields in reg of window owned per bitmap */
static u32 owned_pin_fields(const struct mtk_window *window,
			    const struct mtk_field_reg *reg, const u32 *bitmap)
{
	const struct mtk_pin_field *field = &window->fields[reg->first];
	u32 granted = 0, denied = 0, mask;
	unsigned int n;

	for (n = 0; n < reg->count; n++, field++) {
		mask = ((1U << field->width) - 1) << field->shift;
		if (bitmap[field->pin / 32] & (1U << (field->pin % 32)))
			granted |= mask;
		else
			denied |= mask;
	}

	/* a field shared by several pins needs all of them */
	return granted & ~denied;
}

/*
 * Returns the access type of the register at offset and, in mask, the bits
 * of it that the current cell owns, or -1 if no cell owns any part of it.
 */
static int lookup_reg(const struct mtk_window_state *ws, unsigned long offset,
		      u32 *mask)
{
	const struct mtk_block *block = ws->state->block;
	const u32 *bitmap = this_cell()->arch.mtk_bitmap[block->kind];
	const struct mtk_reg_group *group;
	unsigned int n;

	if (!ws->window) {
		group = find_group(block, offset, &n);
		if (!group)
			return -1;
		*mask = owned_fields(group, n, bitmap);
		return group->access;
	}

	for (n = 0; n < ws->window->num_regs; n++)
		if (ws->window->regs[n].offset == offset) {
			*mask = owned_pin_fields(ws->window,
						 &ws->window->regs[n], bitmap);
			return MTK_RW;
		}

	return -1;
}

/* an access of up to 32 bits */
static void mediate(struct mtk_window_state *ws, struct mmio_access *mmio)
{
	unsigned long offset = mmio->address & ~3UL;
	unsigned int shift = (mmio->address & 3) * 8;
	u32 mask, value;
	int access;

	access = lookup_reg(ws, offset, &mask);
	if (access < 0) {
		/*
		 * Registers without pins are reserved to the root cell, which
		 * may only read them if they might still control pins.
		 */
		if (this_cell() != &root_cell) {
			if (!mmio->is_write)
				mmio->value = 0;
		} else if (!mmio->is_write ||
			   ws->state->block->root_owns_rest) {
			mmio_perform_access(ws->base, mmio);
		} else if (!ws->write_reported) {
			printk("%s: root cell writes to undescribed registers ignored (0x%lx)\n",
			       ws->state->block->name, ws->phys + offset);
			ws->write_reported = true;
		}
		return;
	}

	/* only the bytes accessed */
	if (mmio->size < 4)
		mask &= ((1U << (mmio->size * 8)) - 1) << shift;

	if (!mmio->is_write) {
		if (access == MTK_WO)
			mmio->value = 0;
		else
			mmio->value = (mmio_read32(ws->base + offset) & mask) >>
				shift;
		return;
	}

	if (access == MTK_RO)
		return;

	value = (mmio->value << shift) & mask;
	if (access == MTK_WO && !value)
		return;

	/* serialize with read-modify-write of other cells */
	spin_lock(&ws->state->lock);
	if (access == MTK_RW)
		value |= mmio_read32(ws->base + offset) & ~mask;
	mmio_write32(ws->base + offset, value);
	spin_unlock(&ws->state->lock);
}

static enum mmio_result mtk_handle_access(void *arg, struct mmio_access *mmio)
{
	struct mmio_access word = *mmio;
	u64 value;

	if (mmio->address % mmio->size != 0)
		return MMIO_ERROR;

	if (mmio->size <= 4) {
		mediate(arg, mmio);
		return MMIO_HANDLED;
	}

	/* a 64-bit access covers two registers */
	word.size = 4;
	word.value = mmio->value & 0xffffffff;
	mediate(arg, &word);
	value = word.value & 0xffffffff;

	word.address += 4;
	word.value = (u64)mmio->value >> 32;
	mediate(arg, &word);

	if (!mmio->is_write)
		mmio->value = value | (u64)word.value << 32;
	return MMIO_HANDLED;
}

static struct cell *mtk_irq_target(u16 irq_id)
{
	const struct mtk_block *block;
	struct cell *cell;
	unsigned int kind, n;
	u32 status;

	for (kind = 0; kind < NUM_KINDS; kind++) {
		block = states[kind].block;
		if (!block || block->irq != irq_id)
			continue;

		for (n = 0; n < block->num_regs; n++) {
			status = mmio_read32(states[kind].windows[0].base +
					     block->status + n * 4);
			if (!status)
				continue;
			for_each_cell(cell)
				if (cell->arch.mtk_bitmap[kind][n] & status)
					return cell;
		}
	}

	return NULL;
}

static int mtk_cell_init(struct cell *cell)
{
	u32 bitmaps[NUM_KINDS][BITMAP_WORDS];
	const struct jailhouse_vendor_resource *res;
	const struct mtk_window_state *ws;
	struct mtk_state *state;
	unsigned int kind, n;
	unsigned long phys;
	int err;

	/* Validate everything before changing any state. */
	for_each_vendor_resource(res, cell->config, n) {
		for (kind = 0; kind < NUM_KINDS; kind++)
			if (states[kind].block &&
			    states[kind].block->type == res->type)
				break;
		if (kind == NUM_KINDS)
			return trace_error(-EINVAL);
	}

	for (kind = 0; kind < NUM_KINDS; kind++) {
		state = &states[kind];
		if (!state->block)
			continue;

		phys = state->windows[0].phys;
		err = get_config_bitmap(cell->config, state->block, &phys,
					bitmaps[kind]);
		if (err)
			return err;

		for_each_window(ws, state)
			if (config_maps(cell->config, ws->phys, ws->size))
				return trace_error(-EINVAL);

		if (cell == &root_cell)
			continue;

		for (n = 0; n < BITMAP_WORDS; n++)
			if (bitmaps[kind][n] &
			    ~root_cell.arch.mtk_bitmap[kind][n])
				return trace_error(-EBUSY);
	}

	for (kind = 0; kind < NUM_KINDS; kind++) {
		state = &states[kind];
		if (!state->block)
			continue;

		memcpy(cell->arch.mtk_bitmap[kind], bitmaps[kind],
		       sizeof(bitmaps[kind]));

		if (cell != &root_cell) {
			for (n = 0; n < BITMAP_WORDS; n++)
				if (bitmaps[kind][n])
					break;
			if (n == BITMAP_WORDS)
				continue;

			for (n = 0; n < BITMAP_WORDS; n++)
				root_cell.arch.mtk_bitmap[kind][n] &=
					~bitmaps[kind][n];
			disable_pins(state, bitmaps[kind]);
		}

		for_each_window(ws, state)
			mmio_region_register(cell, ws->phys, ws->size,
					     mtk_handle_access, (void *)ws);
	}

	return 0;
}

static void mtk_cell_exit(struct cell *cell)
{
	u32 root_bitmap[BITMAP_WORDS];
	struct mtk_state *state;
	unsigned int kind, n;
	unsigned long phys;

	if (cell == &root_cell)
		return;

	for (kind = 0; kind < NUM_KINDS; kind++) {
		state = &states[kind];
		if (!state->block)
			continue;

		disable_pins(state, cell->arch.mtk_bitmap[kind]);

		/* return the pins that the root cell owns per its config */
		phys = state->windows[0].phys;
		get_config_bitmap(root_cell.config, state->block, &phys,
				  root_bitmap);
		for (n = 0; n < BITMAP_WORDS; n++)
			root_cell.arch.mtk_bitmap[kind][n] |=
				cell->arch.mtk_bitmap[kind][n] & root_bitmap[n];
	}
}

static unsigned int mtk_mmio_count_regions(struct cell *cell)
{
	const struct jailhouse_vendor_resource *res;
	unsigned int count = 0, i, n;

	/* called before mtk_init for the root cell, so go by the config */
	for (i = 0; i < ARRAY_SIZE(blocks); i++)
		for_each_vendor_resource(res, cell->config, n)
			if (res->type == blocks[i]->type) {
				count += 1 + blocks[i]->num_windows;
				break;
			}

	return count;
}

static int map_window(struct mtk_state *state, const struct mtk_window *window,
		      unsigned long phys, unsigned long size)
{
	struct mtk_window_state *ws = &state->windows[state->num_windows];

	ws->base = paging_map_device(phys, size);
	if (!ws->base)
		return -ENOMEM;

	ws->state = state;
	ws->window = window;
	ws->phys = phys;
	ws->size = size;
	state->num_windows++;

	return 0;
}

static int mtk_init(void)
{
	u32 bitmap[BITMAP_WORDS];
	const struct mtk_window *window;
	const struct mtk_block *block;
	struct mtk_state *state;
	unsigned long phys;
	unsigned int i, n;
	int err;

	for (i = 0; i < ARRAY_SIZE(blocks); i++) {
		block = blocks[i];

		phys = 0;
		err = get_config_bitmap(root_cell.config, block, &phys, bitmap);
		if (err)
			return err;
		if (!phys)
			continue;

		state = &states[block->kind];
		if (state->block || block->num_windows > MTK_MAX_WINDOWS)
			return trace_error(-EINVAL);
		state->block = block;

		err = map_window(state, NULL, phys, block->size);
		for (n = 0; n < block->num_windows && !err; n++) {
			window = &block->windows[n];
			err = map_window(state, window, window->phys,
					 window->size);
		}
		if (err)
			return err;

		if (block->irq) {
			err = irqchip_register_shared_irq(block->irq,
							  mtk_irq_target);
			if (err)
				return err;
		}
	}

	return mtk_cell_init(&root_cell);
}

DEFINE_UNIT_SHUTDOWN_STUB(mtk);
DEFINE_UNIT(mtk, "MediaTek");
