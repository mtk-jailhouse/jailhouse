/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) 2026 MediaTek
 *
 * Common part for sharing EINT between root and other inmate cells and
 * control concurrent access to shared registers.
 *
 * Authors:
 *   Felix Freimann <felix.freimann@mediatek.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <asm/mtk-common.h>
#include <jailhouse/cell.h>
#include <jailhouse/control.h>
#include <jailhouse/printk.h>


access_descr_t get_access_descr (struct mmio_access*        mmio,
                                 const access_descr_map_t*  access_descr_map,
                                 size_t                     access_descr_map_size)
{                                
    if ((mmio->address & 0x00000003) != 0)
    {
        return (ACCESS_DESCR_EMPTY);
    }

    while (access_descr_map_size > 0)
    {
        if ((mmio->address >= access_descr_map->reg_start) &&
            (mmio->address <  access_descr_map->reg_end))
        {
            return (access_descr_map->access_descr [(mmio->address - access_descr_map->reg_start) >> 2]);
        }

        access_descr_map++;
        access_descr_map_size--;
    }

    return (ACCESS_DESCR_EMPTY);
}

/* Checks a cell that takes the entries in bitmap of a block at block_phys. */
int mtk_check_cell (struct cell*     cell,
                    unsigned long    block_phys,
                    size_t           block_size,
                    const u32*       bitmap,
                    const u32*       root_bitmap,
                    size_t           bitmap_size)
{
    const struct jailhouse_memory* mem;
    unsigned int                   n;


    /* A cell must not map a block whose accesses are mediated. */
    for_each_mem_region (mem, cell->config, n)
    {
        if ((mem->phys_start < (block_phys + block_size)) &&
            (block_phys < (mem->phys_start + mem->size)))
        {
            return (-EINVAL);
        }
    }

    /* A cell may only take what the root cell still owns. */
    if (cell != &root_cell)
    {
        for (n = 0; n < bitmap_size; n++)
        {
            if ((bitmap [n] & ~(root_bitmap [n])) != 0)
            {
                return (-EBUSY);
            }
        }
    }

    return (0);
}

/* Refuses a cell config with vendor entries that no engine knows. */
int mtk_check_vendor_types (const struct jailhouse_cell_desc*  config)
{
    const struct jailhouse_vendor_resource* vendor;
    unsigned int                            cnt;


    FOR_EACH_VENDOR (vendor, config, cnt)
    {
        if ((vendor->type != JAILHOUSE_VENDOR_MTK_EINT) &&
            (vendor->type != JAILHOUSE_VENDOR_MTK_GPIO) &&
            (vendor->type != JAILHOUSE_VENDOR_MTK_CLK)  &&
            (vendor->type != JAILHOUSE_VENDOR_MTK_WDT))
        {
            return (-EINVAL);
        }
    }

    return (0);
}

/*
 * Checks where a pin or clock gate entry starts: the 128 entries of its bitmap
 * begin at a multiple of 32 and end within the bitmap_bits that the unit
 * tracks.
 */
int mtk_check_vendor_base (const struct jailhouse_cell_desc*       config,
                           const struct jailhouse_vendor_resource* vendor,
                           size_t                                  bitmap_bits)
{
    const size_t entry_bits = sizeof (vendor->bitmap) * 8;


    if (((vendor->base % (sizeof (vendor->bitmap [0]) * 8)) == 0) &&
        ((vendor->base + entry_bits) <= bitmap_bits))
    {
        return (0);
    }

    printk ("ERROR: Cell \"%s\": vendor entry of type %u starts at %u, "
            "which is not a multiple of 32 up to %u\n", config->name,
            vendor->type, vendor->base, (unsigned int) (bitmap_bits - entry_bits));
    return (-EINVAL);
}

