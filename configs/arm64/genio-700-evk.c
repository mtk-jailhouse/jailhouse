/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Root cell for Genio 700 EVK (MT8390: 6x Cortex-A55, 2x Cortex-A78, 8 GiB)
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

#define GENIO_NAME	"genio-700-evk"
#define GENIO_CPUS	0b11111111	/* A55: 0-5, A78: 6-7 */
#define GENIO_DRAM_END	0x240000000

#include "genio-evk.h"
