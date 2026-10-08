/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) 2026 MediaTek
 *
 * Common part for sharing GPIO between root and other inmate cells and
 * control concurrent access to shared registers.
 *
 * Authors:
 *   Felix Freimann <felix.freimann@mediatek.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <asm/irqchip.h>
#include <asm/mtk-common-gpio.h>
#include <asm/spinlock.h>
#include <jailhouse/cell.h>
#include <jailhouse/control.h>
#include <jailhouse/entry.h>
#include <jailhouse/paging.h>
#include <jailhouse/percpu.h>
#include <jailhouse/printk.h>


static spinlock_t lock;

static void* virt_addr = NULL;

/* The block's address, as the root cell names it. */
static unsigned long block_phys = 0;

static bool write_reported = false;

static const gpio_config_descr_t*  config_descr = NULL;


static void handle_ro_access (struct mmio_access*  mmio,
                              access_descr_t       access_descr);
static void handle_wo_access (struct mmio_access*  mmio,
                              access_descr_t       access_descr);
static void handle_rw_access (struct mmio_access*  mmio,
                              access_descr_t       access_descr);

static enum mmio_result gpio_handle_access (void*                arg,
                                            struct mmio_access*  mmio);

static int get_phys_addr (struct cell*    cell,
                          unsigned long*  phys_addr);


static void handle_ro_access (struct mmio_access*  mmio,
                              access_descr_t       access_descr)
{
	if (! (mmio->is_write))
	{
        u32 bitmap = config_descr->addr_to_bitmap (mmio, access_descr);


        if (bitmap != 0)
        {
            mmio_perform_access (virt_addr, mmio);

      		/* Only allow the bits for which access is allowed. */
	    	mmio->value &= bitmap;
        }
        else
        {
            mmio->value = 0;
        }
    }
}

static void handle_wo_access (struct mmio_access*  mmio,
                              access_descr_t       access_descr)
{
	if (mmio->is_write)
	{
        u32 bitmap = config_descr->addr_to_bitmap (mmio, access_descr);


		/* Only allow the bits for which access is allowed. */
        if ((bitmap != 0)                            &&
            ((mmio->value & bitmap) == mmio->value))
        {
            mmio_perform_access (virt_addr, mmio);
        }
    }
}

static void handle_rw_access (struct mmio_access*  mmio,
                              access_descr_t       access_descr)
{
    if (! (mmio->is_write))
    {
        handle_ro_access (mmio, access_descr);
    }
    else
    {
    	u32 bitmap = config_descr->addr_to_bitmap (mmio, access_descr);


        if ((bitmap != 0)                            &&
            ((mmio->value & bitmap) == mmio->value))
        {
            struct mmio_access curr_mmio;


            curr_mmio.address  = mmio->address;
            curr_mmio.size     = mmio->size;
            curr_mmio.is_write = false;

    	    /* Perform a read-update-write operation. This must be done inside a lock     */
            /* since the read-update-write operation must be atomic. In addition, this    */
            /* MMIO write operation could be executed from multiple cells simmultanously. */
	        spin_lock (&lock);
	        mmio_perform_access (virt_addr, &curr_mmio);

            mmio->value |= (curr_mmio.value & ~ (bitmap));

            mmio_perform_access (virt_addr, mmio);
        	spin_unlock (&lock);
        }
    }
}

static enum mmio_result gpio_handle_access (void*                arg,
									  	    struct mmio_access*  mmio)
{
    access_descr_t access_descr;


	/* Registers with entries take 32-bit accesses only. */
	if ((mmio->size != 4) || ((mmio->address & 0x3) != 0))
	{
		struct mmio_access word = *mmio;


		word.address &= ~0x3UL;
		if (get_access_type (get_access_descr (&word, config_descr->access_descr_map, config_descr->access_descr_map_size)) != ACCESS_ROOT)
		{
			return (MMIO_ERROR);
		}
	}

    access_descr = get_access_descr (mmio, config_descr->access_descr_map, config_descr->access_descr_map_size);


	switch (get_access_type (access_descr))
	{
        case ACCESS_RO:
            handle_ro_access (mmio, access_descr);
            break;

        case ACCESS_WO:
            handle_wo_access (mmio, access_descr);
            break;

        case ACCESS_RW:
            handle_rw_access (mmio, access_descr);
            break;

        default:
	    	/* The remaing GPIO accesses shall only be performed by the root cell. */
            if (this_cell () == &root_cell)
			{
				/* The root cell may read them, but not write them: */
				/* they may control pins of other cells.            */
				if (! (mmio->is_write))
				{
					mmio_perform_access (virt_addr, mmio);
				}
				else if (! write_reported)
				{
					printk ("GPIO: root cell writes to undescribed registers ignored (0x%lx)\n",
					        block_phys + mmio->address);
					write_reported = true;
				}
			}
			else
			{
				if (! (mmio->is_write))
				{
					/* Simply return '0' for any read operation. */
					mmio->value = 0;
				}
			}
			break;
	}

	return (MMIO_HANDLED);
}

static int get_phys_addr (struct cell*    cell,
                          unsigned long*  phys_addr)
{
	unsigned int                   cnt;
	const struct jailhouse_vendor_resource* vendor;


    (*phys_addr) = 0;

    FOR_EACH_VENDOR (vendor, cell->config, cnt)
	{
		if (vendor->type != JAILHOUSE_VENDOR_MTK_GPIO)
		{
			continue;
		}

		if (((*phys_addr) != 0)                         &&
            (vendor->address != (*phys_addr)))
		{
			return (-EINVAL);
		}

        (*phys_addr) = vendor->address;
	}

    return (0);
}

int mtk_gpio_cell_init (struct cell*  cell)
{
	size_t                         pos;
	unsigned int                   cnt;
    unsigned long                  address = 0;
	const struct jailhouse_vendor_resource* vendor;


    if (config_descr == NULL)
    {
        return (-EINVAL);
    }

    if (mtk_check_vendor_types (cell->config) != 0)
    {
        return (-EINVAL);
    }

	FOR_EACH_VENDOR (vendor, cell->config, cnt)
	{
		if (vendor->type != JAILHOUSE_VENDOR_MTK_GPIO)
		{
			continue;
		}

		/* Verify that the GPIO entries in the cel description are valid. */
		if (((vendor->base % sizeof (vendor->bitmap [0])) != 0)                                       ||
		    ((vendor->base + (sizeof (vendor->bitmap) * 8)) > (sizeof (cell->arch.gpio_bitmap) * 8)))
		{
			return (-EINVAL);
		}

		/* Also only one GPIO address is supported. */
		if ((address != 0)                         &&
		    (vendor->address != address))
		{
			return (-EINVAL);
		}
	
        address = vendor->address;

		/* Copy the GPIO entries. */
		for (pos = 0; pos < ARRAY_SIZE (vendor->bitmap); pos++)
		{
			cell->arch.gpio_bitmap [(vendor->base / (sizeof (vendor->bitmap [0]) * 8)) + pos] |= vendor->bitmap [pos];
		}
	}

	/* The entries must name the block that the root cell names. */
	if ((address != 0) && (address != block_phys))
	{
		return (-EINVAL);
	}

	if (block_phys != 0)
	{
		int ret = mtk_check_cell (cell, block_phys, config_descr->reg_size,
		                          cell->arch.gpio_bitmap, root_cell.arch.gpio_bitmap,
		                          ARRAY_SIZE (cell->arch.gpio_bitmap));

		if (ret != 0)
		{
			return (ret);
		}

	   	/* Register handler. */
		mmio_region_register (cell, block_phys, config_descr->reg_size, gpio_handle_access, (void*) block_phys);
	}

    /* And lastly, remove the GPIO entries from the root cell. */
	if (cell != &root_cell)
	{
		for (pos = 0; pos < ARRAY_SIZE (cell->arch.gpio_bitmap); pos++)
		{
			root_cell.arch.gpio_bitmap [pos] &= ~(cell->arch.gpio_bitmap [pos]);
		}
    }

	return (0);
}

void mtk_gpio_cell_exit (struct cell*  cell)
{
	size_t                         pos;
	unsigned int                   cnt;
	const struct jailhouse_vendor_resource* vendor;


	if (cell != &root_cell)
	{
		/* Return the GPIO entries to the root cell. */
		for (pos = 0; pos < ARRAY_SIZE (cell->arch.gpio_bitmap); pos++)
		{
			root_cell.arch.gpio_bitmap [pos] |= cell->arch.gpio_bitmap [pos];
		}

		/* Mask out bits which were not part of the root cell. */
		FOR_EACH_VENDOR (vendor, root_cell.config, cnt)
		{
			if (vendor->type != JAILHOUSE_VENDOR_MTK_GPIO)
			{
				continue;
			}	

			for (pos = 0; pos < ARRAY_SIZE (vendor->bitmap); pos++)
			{
				root_cell.arch.gpio_bitmap [(vendor->base / (sizeof (vendor->bitmap [0]) * 8)) + pos] &= vendor->bitmap [pos];
			}
		}
	}
}

unsigned int mtk_gpio_mmio_count_regions (struct cell*  cell)
{
	/* Only one MMIO GPIO region handler per cell. */
	return (1);
}

int mtk_gpio_init (const gpio_config_descr_t*  gpio_config_descr)
{
	int           ret = 0;
    unsigned long phys_addr = 0;


    if (gpio_config_descr == NULL)
    {
        return (-EINVAL);
    }

    /* The cells' vendor entries are handled here, see mtk_check_vendor_types (). */
    vendor_resources_handled = true;

	if (virt_addr == NULL)
	{
		/* Create hypervisor paging for GPIO access. */
        /* This is only done once with the root cell. */
        ret = get_phys_addr (&root_cell, &phys_addr);
        block_phys = phys_addr;
		if (ret != 0)
		{
			return (ret);
		}

        if (phys_addr != 0)
        {
            virt_addr = paging_map_device (phys_addr, gpio_config_descr->reg_size);
            if (virt_addr == NULL)
            {
                ret = -ENOMEM;
            }
        }

        config_descr = gpio_config_descr;

    	ret = mtk_gpio_cell_init (&root_cell);

    	if (ret != 0)
	    {
		    /* Cleanup if cell initialization failed. */
    		if (virt_addr != NULL)
	    	{
                paging_unmap_device (phys_addr, virt_addr, gpio_config_descr->reg_size);

                virt_addr = NULL;
	    	}
    	}
    }

	return (ret);
}

void mtk_gpio_shutdown (void)
{
    unsigned long phys_addr;


    if (config_descr != NULL)
    {
    	if (virt_addr != NULL)
	    {
            get_phys_addr (&root_cell, &phys_addr);

            paging_unmap_device (phys_addr, virt_addr, config_descr->reg_size);

		    virt_addr = NULL;
	    }

        config_descr = NULL;
    }
}