/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Root cell for Genio 510 EVK (MT8370: 4x Cortex-A55, 2x Cortex-A78, 4 GiB)
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

#define GENIO_NAME	"genio-510-evk"
#define GENIO_CPUS	0b111111	/* A55: 0-3, A78: 4-5 */
#define GENIO_DRAM_END	0x140000000

#include "genio-evk.h"
