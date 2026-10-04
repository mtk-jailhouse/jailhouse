/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Zephyr cell for Genio 510 EVK, on two Cortex-A55 cores
 *
 * Copyright (c) MediaTek Inc., 2026
 *
 * Authors:
 *  Aary Patil <aary.patil@mediatek.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#define GENIO_ZEPHYR_CPUS	((1 << 2) | (1 << 3))
#include "genio-evk-zephyr.h"
