/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) 2026 MediaTek
 *
 * The watchdog of MediaTek SoCs (TOPRGU). The root cell keeps the hardware
 * watchdog: its accesses pass through, except that it cannot change the
 * subsystem resets that other cells own. Each other cell with a watchdog
 * entry gets a watchdog of its own at the same address, with the hardware's
 * register interface. Its deadline is kept as a counter value and checked on
 * each "Tick" hypercall of the root cell; at the deadline, the cell fails. A
 * cell's chip reset (SWRST) fails the cell as well, never the SoC, and so does
 * its PSCI SYSTEM_RESET. After a restart, the cell's STA tells why its
 * previous run failed.
 *
 * The registers are those of the MT8390 datasheet, AP registers, TOPRGU
 * (pp. 2833-2837).
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <asm/mtk-common-wdt.h>
#include <asm/processor.h>
#include <asm/spinlock.h>
#include <asm/sysregs.h>
#include <jailhouse/control.h>
#include <jailhouse/paging.h>
#include <jailhouse/printk.h>
#include <jailhouse/string.h>

#define WDT_MODE		0x00
#define  WDT_MODE_EN		(1 << 0)
#define  WDT_MODE_BITS		0x000001ff
#define  WDT_MODE_KEY		0x22000000
#define WDT_LENGTH		0x04
#define  WDT_LENGTH_KEY		0x8
#define  WDT_LENGTH_KEY_MASK	0x1f
#define  WDT_LENGTH_TIMEOUT	0xffe0	/* in 1/64 s, bits 15:5 */
#define WDT_RESTART		0x08
#define  WDT_RESTART_KEY	0x1971
#define WDT_STA			0x0c	/* read-only */
#define  WDT_STA_HW_WDT_RST	(1U << 31)	/* reset by a timeout */
#define  WDT_STA_SW_WDT_RST	(1U << 30)	/* reset by SWRST (or PSCI) */
#define WDT_SWRST		0x14
#define  WDT_SWRST_KEY		0x1209
#define WDT_SWSYSRST		0x18
#define  WDT_SWSYSRST_KEY	0x88000000
#define WDT_KEY_MASK		0xff000000

/*
 * Only the root cell reaches the other registers, among them the reset pulse
 * width (WDT_INTERNAL, 0x10) and the lock bits of the subsystem resets
 * (WDT_SWSYSRST_LOCK, 0x70): another cell reads 0, and its writes are
 * dropped.
 */

static const wdt_config_descr_t *config_descr;
static void *virt_addr;
static unsigned long block_phys;
static spinlock_t lock;
static u64 counter_freq;

/*
 * the root cell's ticks, for the check on its own kicks: their period, as the
 * root cell passes it, else the last interval
 */
static u64 last_tick, tick_period, tick_interval;
static bool tick_missing_reported;

/*
 * Why a destroyed cell had failed, by its name, as WDT_STA bits: the next
 * cell of that name reads them after it starts. Taken when that cell is
 * created; a cause that finds no free entry is reported and dropped.
 */
static struct {
	char name[JAILHOUSE_CELL_NAME_MAXLEN + 1];
	u32 cause;
} causes[8];

static u64 counter_now(void)
{
	u64 now;

	isb();
	arm_read_sysreg(cntpct_el0, now);
	return now;
}

static u64 period_of(struct cell *cell)
{
	return ((cell->arch.wdt.length & WDT_LENGTH_TIMEOUT) >> 5) *
		counter_freq / 64;
}

static void record_cause(struct cell *cell)
{
	unsigned int n, free = ARRAY_SIZE(causes);

	for (n = 0; n < ARRAY_SIZE(causes); n++) {
		if (strcmp(causes[n].name, cell->config->name) == 0)
			break;
		if (causes[n].name[0] == 0 && free == ARRAY_SIZE(causes))
			free = n;
	}
	if (n == ARRAY_SIZE(causes))
		n = free;
	if (n == ARRAY_SIZE(causes)) {
		printk("WDT: no room to keep why cell \"%s\" failed, the next "
		       "cell of that name reads STA 0\n", cell->config->name);
		return;
	}

	memcpy(causes[n].name, cell->config->name, sizeof(causes[n].name) - 1);
	causes[n].name[sizeof(causes[n].name) - 1] = 0;
	causes[n].cause = cell->arch.wdt.cause;
}

static u32 take_cause(struct cell *cell)
{
	unsigned int n;
	u32 cause;

	for (n = 0; n < ARRAY_SIZE(causes); n++)
		if (strcmp(causes[n].name, cell->config->name) == 0) {
			cause = causes[n].cause;
			memset(&causes[n], 0, sizeof(causes[n]));
			return cause;
		}
	return 0;
}

/* Releases the subsystem resets that the cell holds asserted. */
static void release_swsysrst(struct cell *cell)
{
	u32 value = mmio_read32(virt_addr + WDT_SWSYSRST);

	if (value & cell->arch.wdt.swsysrst_bits)
		mmio_write32(virt_addr + WDT_SWSYSRST, WDT_SWSYSRST_KEY |
			     (value & config_descr->swsysrst_mask &
			      ~cell->arch.wdt.swsysrst_bits));
}

/* On the root cell's kick: are the cells' watchdogs still checked? */
static void check_ticks(void)
{
	u64 now = counter_now(), limit;

	spin_lock(&lock);
	limit = 10 * (tick_period ? tick_period : tick_interval);
	if (limit < counter_freq)
		limit = counter_freq;
	if (!tick_missing_reported && now - last_tick > limit) {
		printk("WDT: no tick from the root cell for %llu ms, the cells' "
		       "watchdogs are not checked\n",
		       (now - last_tick) * 1000 / counter_freq);
		tick_missing_reported = true;
	}
	spin_unlock(&lock);
}

static void wdt_tick(unsigned int period_ms)
{
	u64 now = counter_now();
	const char *reason;
	struct cell *cell;
	u32 cause;

	spin_lock(&lock);

	tick_period = (u64)period_ms * counter_freq / 1000;
	tick_interval = now - last_tick;
	last_tick = now;
	if (tick_missing_reported) {
		printk("WDT: ticks from the root cell resumed\n");
		tick_missing_reported = false;
	}

	for_each_non_root_cell(cell) {
		/* A cell that failed otherwise, e.g. on a fault, stays so. */
		if (!cell->arch.wdt.present ||
		    cell->comm_page.comm_region.cell_state ==
		    JAILHOUSE_CELL_FAILED)
			continue;

		if (cell->arch.wdt.restart) {
			cause = WDT_STA_SW_WDT_RST;
			reason = cell->arch.wdt.restart;
		} else if ((cell->arch.wdt.mode & WDT_MODE_EN) &&
			   (s64)(now - cell->arch.wdt.deadline) >= 0) {
			cause = WDT_STA_HW_WDT_RST;
			reason = "watchdog timeout";
		} else {
			continue;
		}

		cell->arch.wdt.mode &= ~WDT_MODE_EN;
		cell->arch.wdt.restart = NULL;
		cell->arch.wdt.cause = cause;
		if (cause == WDT_STA_HW_WDT_RST)
			printk("WDT: cell \"%s\" missed its deadline, detected "
			       "%llu ms late\n", cell->config->name,
			       (now - cell->arch.wdt.deadline) * 1000 /
			       counter_freq);

		/* The cell's CPUs may be waiting for the lock. */
		spin_unlock(&lock);
		cell_fail(cell, reason);
		spin_lock(&lock);
	}

	spin_unlock(&lock);
}

static enum mmio_result root_access(struct mmio_access *mmio)
{
	u32 others;

	if (mmio->is_write && mmio->address == WDT_RESTART &&
	    mmio->value == WDT_RESTART_KEY)
		check_ticks();

	if (mmio->is_write && mmio->address == WDT_SWSYSRST &&
	    mmio->size == 4) {
		spin_lock(&lock);
		/* The resets that other cells own keep their value. */
		others = config_descr->swsysrst_mask &
			~root_cell.arch.wdt.swsysrst_bits;
		mmio->value = (mmio->value & ~others) |
			(mmio_read32(virt_addr + WDT_SWSYSRST) & others);
		mmio_perform_access(virt_addr, mmio);
		spin_unlock(&lock);
	} else {
		mmio_perform_access(virt_addr, mmio);
	}

	return MMIO_HANDLED;
}

static enum mmio_result cell_access(struct cell *cell,
				    struct mmio_access *mmio)
{
	u32 value = mmio->value, bits;
	u64 now;

	/* The registers take 32-bit accesses only. */
	if (mmio->size != 4 || (mmio->address & 0x3) != 0)
		return MMIO_ERROR;

	now = counter_now();
	spin_lock(&lock);

	switch (mmio->address) {
	case WDT_MODE:
		if (!mmio->is_write) {
			mmio->value = cell->arch.wdt.mode;
		} else if ((value & WDT_KEY_MASK) == WDT_MODE_KEY) {
			if ((value & WDT_MODE_EN) &&
			    !(cell->arch.wdt.mode & WDT_MODE_EN))
				cell->arch.wdt.deadline = now + period_of(cell);
			cell->arch.wdt.mode = value & WDT_MODE_BITS;
		}
		break;
	case WDT_LENGTH:
		if (!mmio->is_write)
			mmio->value = cell->arch.wdt.length;
		else if ((value & WDT_LENGTH_KEY_MASK) == WDT_LENGTH_KEY)
			cell->arch.wdt.length = value & WDT_LENGTH_TIMEOUT;
		break;
	case WDT_RESTART:
		if (!mmio->is_write)
			mmio->value = 0;
		else if (value == WDT_RESTART_KEY &&
			 (cell->arch.wdt.mode & WDT_MODE_EN))
			cell->arch.wdt.deadline = now + period_of(cell);
		break;
	case WDT_SWRST:
		/* A chip reset restarts the cell: it fails on the next tick. */
		if (!mmio->is_write)
			mmio->value = 0;
		else if (value == WDT_SWRST_KEY)
			cell->arch.wdt.restart = "chip reset requested";
		break;
	case WDT_SWSYSRST:
		if (!mmio->is_write) {
			mmio->value = mmio_read32(virt_addr + WDT_SWSYSRST) &
				cell->arch.wdt.swsysrst_bits;
			break;
		}
		if ((value & WDT_KEY_MASK) != WDT_SWSYSRST_KEY)
			break;
		bits = value & config_descr->swsysrst_mask;
		if ((bits & ~cell->arch.wdt.swsysrst_bits) &&
		    !cell->arch.wdt.swsysrst_reported) {
			printk("WDT: cell \"%s\" may not reset subsystems %x, "
			       "ignored\n", cell->config->name,
			       bits & ~cell->arch.wdt.swsysrst_bits);
			cell->arch.wdt.swsysrst_reported = true;
		}
		mmio_write32(virt_addr + WDT_SWSYSRST, WDT_SWSYSRST_KEY |
			     (mmio_read32(virt_addr + WDT_SWSYSRST) &
			      config_descr->swsysrst_mask &
			      ~cell->arch.wdt.swsysrst_bits) |
			     (bits & cell->arch.wdt.swsysrst_bits));
		break;
	case WDT_STA:
		if (!mmio->is_write)
			mmio->value = cell->arch.wdt.sta;
		break;
	default:
		if (!mmio->is_write)
			mmio->value = 0;
		break;
	}

	spin_unlock(&lock);
	return MMIO_HANDLED;
}

static enum mmio_result wdt_handle_access(void *arg, struct mmio_access *mmio)
{
	struct cell *cell = this_cell();

	if (cell == &root_cell)
		return root_access(mmio);
	return cell_access(cell, mmio);
}

/* A cell's PSCI SYSTEM_RESET: restarted like its chip reset (SWRST). */
static bool wdt_system_reset(struct cell *cell)
{
	if (!cell->arch.wdt.present)
		return false;

	spin_lock(&lock);
	cell->arch.wdt.restart = "system reset requested";
	spin_unlock(&lock);
	return true;
}

/* The block that the root cell's watchdog entry names, or 0. */
static int get_phys_addr(struct cell *cell, unsigned long *phys_addr)
{
	const struct jailhouse_vendor_resource *vendor;
	unsigned int cnt;

	*phys_addr = 0;

	FOR_EACH_VENDOR(vendor, cell->config, cnt) {
		if (vendor->type != JAILHOUSE_VENDOR_MTK_WDT)
			continue;
		if (*phys_addr != 0)
			return -EINVAL;
		*phys_addr = vendor->address;
	}

	return 0;
}

/* Disarms the cell's watchdog and releases its subsystem resets. */
static void reset_watchdog(struct cell *cell)
{
	release_swsysrst(cell);
	cell->arch.wdt.mode = 0;
	cell->arch.wdt.length = WDT_LENGTH_TIMEOUT;
	cell->arch.wdt.deadline = 0;
	cell->arch.wdt.restart = NULL;
	cell->arch.wdt.swsysrst_reported = false;
}

void mtk_wdt_cell_reset(struct cell *cell)
{
	if (!cell->arch.wdt.present)
		return;

	spin_lock(&lock);
	reset_watchdog(cell);
	/*
	 * When the cell starts, rather than becomes loadable, STA takes the
	 * cause of its last failure until the next start: the hardware's bits
	 * stay until power-on, a cell's run is the closest equivalent.
	 */
	if (!cell->loadable) {
		cell->arch.wdt.sta = cell->arch.wdt.cause;
		cell->arch.wdt.cause = 0;
	}
	spin_unlock(&lock);
}

int mtk_wdt_cell_init(struct cell *cell)
{
	const struct jailhouse_vendor_resource *vendor;
	unsigned int cnt;
	int err;

	if (config_descr == NULL)
		return -EINVAL;

	if (mtk_check_vendor_types(cell->config) != 0)
		return -EINVAL;

	/* One entry at most, naming the root cell's block; base is reserved. */
	FOR_EACH_VENDOR(vendor, cell->config, cnt) {
		if (vendor->type != JAILHOUSE_VENDOR_MTK_WDT)
			continue;
		if (cell->arch.wdt.present || block_phys == 0 ||
		    vendor->address != block_phys || vendor->base != 0)
			return -EINVAL;
		cell->arch.wdt.present = true;
		cell->arch.wdt.swsysrst_bits =
			vendor->bitmap[0] & config_descr->swsysrst_mask;
	}

	if (block_phys == 0)
		return 0;

	err = mtk_check_cell(cell, block_phys, config_descr->reg_size,
			     &cell->arch.wdt.swsysrst_bits,
			     &root_cell.arch.wdt.swsysrst_bits, 1);
	if (err)
		return err;

	if (!cell->arch.wdt.present)
		return 0;

	mmio_region_register(cell, block_phys, config_descr->reg_size,
			     wdt_handle_access, NULL);

	if (cell != &root_cell) {
		spin_lock(&lock);
		root_cell.arch.wdt.swsysrst_bits &=
			~cell->arch.wdt.swsysrst_bits;
		cell->arch.wdt.cause = take_cause(cell);
		cell->arch.wdt.sta = 0;
		reset_watchdog(cell);
		spin_unlock(&lock);
	}

	return 0;
}

void mtk_wdt_cell_exit(struct cell *cell)
{
	const struct jailhouse_vendor_resource *vendor;
	unsigned int cnt;

	if (cell == &root_cell || !cell->arch.wdt.present)
		return;

	spin_lock(&lock);
	release_swsysrst(cell);
	if (cell->arch.wdt.cause)
		record_cause(cell);

	/* Return the resets to the root cell, as far as they were its own. */
	root_cell.arch.wdt.swsysrst_bits |= cell->arch.wdt.swsysrst_bits;
	FOR_EACH_VENDOR(vendor, root_cell.config, cnt)
		if (vendor->type == JAILHOUSE_VENDOR_MTK_WDT)
			root_cell.arch.wdt.swsysrst_bits &= vendor->bitmap[0];

	cell->arch.wdt.present = false;
	spin_unlock(&lock);
}

unsigned int mtk_wdt_mmio_count_regions(struct cell *cell)
{
	return 1;
}

int mtk_wdt_init(const wdt_config_descr_t *wdt_config_descr)
{
	unsigned long phys_addr;
	int err;

	if (wdt_config_descr == NULL)
		return -EINVAL;

	/* The cells' vendor entries are handled here. */
	vendor_resources_handled = true;

	config_descr = wdt_config_descr;
	arm_read_sysreg(cntfrq_el0, counter_freq);

	err = get_phys_addr(&root_cell, &phys_addr);
	if (err || phys_addr == 0)
		return err;

	virt_addr = paging_map_device(phys_addr, config_descr->reg_size);
	if (virt_addr == NULL)
		return -ENOMEM;
	block_phys = phys_addr;
	last_tick = counter_now();

	err = mtk_wdt_cell_init(&root_cell);
	if (!err)
		err = tick_register_handler(wdt_tick);
	if (!err)
		err = system_reset_register_handler(wdt_system_reset);
	if (err) {
		paging_unmap_device(phys_addr, virt_addr,
				    config_descr->reg_size);
		virt_addr = NULL;
		block_phys = 0;
	}

	return err;
}

void mtk_wdt_shutdown(void)
{
	if (virt_addr != NULL) {
		paging_unmap_device(block_phys, virt_addr,
				    config_descr->reg_size);
		virt_addr = NULL;
	}
}
