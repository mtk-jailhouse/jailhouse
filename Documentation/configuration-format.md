Configuration Format
====================

Jailhouse takes its configuration as binary images: one system configuration
for `jailhouse enable`, and one cell configuration per `jailhouse cell create`.
They are built from C sources (`configs/<arch>/*.c`) that fill the structures
of `include/jailhouse/cell-config.h`. Every structure is packed, and every
value is little-endian. This document describes revision 16.

Both images begin with a signature, the architecture and the revision. The
driver refuses an image whose revision is not the one it was built with
(`JAILHOUSE_CONFIG_REVISION`), and so does `jailhouse config check`. A change
of layout or meaning increments the revision; existing images must then be
rebuilt.


Cell configuration
------------------

A cell configuration is a descriptor, `struct jailhouse_cell_desc` (140 bytes),
followed by a sequence of arrays whose lengths the descriptor gives:

| Offset | Size | Field | Meaning |
|---|---|---|---|
| 0 | 5 | `signature` | `"JHCLL"` |
| 5 | 1 | `architecture` | `JAILHOUSE_X86` (0), `JAILHOUSE_ARM` (1), `JAILHOUSE_ARM64` (2) |
| 6 | 2 | `revision` | 16 |
| 8 | 32 | `name` | The cell's name, NUL-terminated (at most 31 characters) |
| 40 | 4 | `id` | Set by the driver; 0 in the image |
| 44 | 4 | `flags` | `JAILHOUSE_CELL_*`, see below |
| 48 | 4 | `cpu_set_size` | Size of the CPU set in bytes |
| 52 | 4 | `num_memory_regions` | Number of memory regions |
| 56 | 4 | `num_cache_regions` | Number of cache regions (x86 CAT) |
| 60 | 4 | `num_irqchips` | Number of interrupt controller entries |
| 64 | 4 | `num_pio_regions` | Number of port I/O ranges (x86) |
| 68 | 4 | `num_pci_devices` | Number of PCI devices |
| 72 | 4 | `num_pci_caps` | Number of PCI capabilities |
| 76 | 4 | `num_stream_ids` | Number of IOMMU stream IDs |
| 80 | 4 | `num_smc_ids` | Number of SiP calls the cell may make (ARM) |
| 84 | 4 | `num_vendor_resources` | Number of vendor-specific resources |
| 88 | 4 | `vpci_irq_base` | First interrupt of the virtual PCI devices |
| 92 | 8 | `cpu_reset_address` | Where the cell's CPUs start |
| 100 | 8 | `msg_reply_timeout` | How long the cell may take to answer a message, in polls of its reply; 0 for no limit. A cell that does not answer in time fails |
| 108 | 32 | `console` | The cell's console, `struct jailhouse_console` |

Flags: `JAILHOUSE_CELL_PASSIVE_COMMREG` (the cell does not answer messages
on its communication region), `JAILHOUSE_CELL_TEST_DEVICE`,
`JAILHOUSE_CELL_AARCH32` (ARM64: the cell runs in AArch32),
`JAILHOUSE_CELL_VIRTUAL_CONSOLE_PERMITTED` and
`JAILHOUSE_CELL_VIRTUAL_CONSOLE_ACTIVE`.

The arrays follow the descriptor in this order, without padding:

| Array | Entry | Size of an entry |
|---|---|---|
| CPU set | A bitmap of CPU numbers | `cpu_set_size` in all |
| Memory regions | `struct jailhouse_memory` | 32 |
| Cache regions | `struct jailhouse_cache` | 12 |
| Interrupt controllers | `struct jailhouse_irqchip` | 32 |
| Port I/O ranges | `struct jailhouse_pio` | 4 |
| PCI devices | `struct jailhouse_pci_device` | 56 |
| PCI capabilities | `struct jailhouse_pci_capability` | 8 |
| Stream IDs | `union jailhouse_stream_id` | 4 |
| SiP calls | A function ID (`__u32`) | 4 |
| Vendor resources | `struct jailhouse_vendor_resource` | 32 |

`jailhouse_cell_config_size()` gives the size of the whole image, and the
`jailhouse_cell_*()` functions of the header find each array.

### Memory regions

`phys_start`, `virt_start` (the address the cell sees), `size` and `flags`,
64 bits each. Flags: `JAILHOUSE_MEM_READ`, `_WRITE`, `_EXECUTE`, `_DMA`, `_IO`
(a device), `_COMM_REGION` (the communication region, at `virt_start`;
`phys_start` is ignored), `_LOADABLE` (the root cell can load an image into it
while the cell is stopped), `_ROOTSHARED` (stays mapped in the root cell),
`_NO_HUGEPAGES`, and for subpage device regions `_IO_UNALIGNED` and the
access widths `_IO_8` to `_IO_64`.

### Interrupt controllers

`address` (the distributor, on ARM), `id`, `pin_base` and `pin_bitmap[4]`:
the cell owns the 128 interrupts from `pin_base` whose bits are set.

### SiP calls (ARM)

The function IDs of the SMCCC SiP service calls (owner 2) that the cell may
make. The hypervisor forwards these to the firmware; any other SiP call
returns NOT_SUPPORTED and is reported once per cell. The root cell lists the
SiP calls of its operating system as well. PSCI calls are emulated, and calls
to the trusted OS or trusted applications are forwarded for the root cell
only.

### Vendor resources

`struct jailhouse_vendor_resource` describes a resource that a vendor unit
of the hypervisor mediates:

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | `type` |
| 4 | 8 | `address` |
| 12 | 4 | `base` |
| 16 | 16 | `bitmap[4]` |

The SoC is the one that the hypervisor is built for (`CONFIG_SOC`). A
hypervisor without vendor units refuses a cell that has vendor resources.
The MediaTek types:

| Type | Name | `address` | `base`, `bitmap` |
|---|---|---|---|
| 1 | `JAILHOUSE_VENDOR_MTK_EINT` | The EINT block | External interrupts |
| 2 | `JAILHOUSE_VENDOR_MTK_GPIO` | The GPIO block | GPIO pins |
| 3 | `JAILHOUSE_VENDOR_MTK_CLK` | The clock gate block | Clock gates |
| 4 | `JAILHOUSE_VENDOR_MTK_WDT` | The watchdog (TOPRGU) | `base` 0; `bitmap[0]`: subsystem resets |

For types 1 to 3, the cell owns the 128 numbers from `base` whose bits are
set; `base` is a multiple of 32, and `base` + 128 is at most 256. A cell may
have several entries of a type, all naming the same block. The hypervisor
lets the cell change only the fields of what it owns, in registers it shares
with other cells, and the cell must not map that block with a memory region.

Type 4 gives the cell a watchdog of its own at `address`, with the
hardware's register interface. `bitmap[0]` holds the subsystem resets that
the cell may assert; the other words are 0. `base` is reserved and must be
0. A cell has at most one such entry.

The root cell lists each block with everything it owns at first, its whole
pins, gates and resets. Another cell can only take what the root cell still
owns, and gives it back when it is destroyed. The root cell's type-4 entry
names the hardware watchdog, which the root cell keeps.


System configuration
--------------------

`struct jailhouse_system` (500 bytes, with a root cell that has no arrays)
begins with the signature `"JHSYS"`, the architecture, the revision and
`flags` (`JAILHOUSE_SYS_VIRTUAL_DEBUG_CONSOLE`). Then:

- `hypervisor_memory` (offset 12): the hypervisor's memory, a memory region;
- `debug_console` (offset 44): the hypervisor's console;
- `platform_info` (offset 76): the PCI configuration space (`pci_mmconfig_base`,
  `pci_mmconfig_end_bus`, `pci_is_virtual`, `pci_domain`), up to eight IOMMU
  units, and the architecture's part. On ARM: `maintenance_irq`,
  `gic_version`, the GIC's `gicd_base`, `gicc_base`, `gich_base`, `gicv_base`,
  `gicr_base`, and `gicr_size` (offset 356): the size of the redistributor
  region. 0 means one redistributor per CPU, without gaps. Otherwise the
  region may hold redistributors without a CPU: the root cell reads their
  identification and type registers, other reads return 0, writes are
  ignored;
- `root_cell` (offset 360): the root cell's descriptor, followed by its
  arrays as in a cell configuration.


Revisions
---------

| Revision | Change |
|---|---|
| 14 | Upstream Jailhouse (`e57d1eff`) |
| 15 | `gicr_size`; the SiP call list (`num_smc_ids`); vendor resources (`num_vendor_resources`), types 1 to 3 |
| 16 | Vendor resource type 4, the MediaTek watchdog |
