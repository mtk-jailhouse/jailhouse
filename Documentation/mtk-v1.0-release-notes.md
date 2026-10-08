# mtk-v1.0 release notes

mtk-jailhouse 1.0 is the first release of the Jailhouse partitioning
hypervisor for MediaTek Genio EVKs with MT8188-based SoCs: the Genio 510 EVK
(MT8370) and the Genio 700 EVK (MT8390). Linux runs as the root cell. Zephyr
runs next to it in its own cells, on its own CPUs and with its own pins,
interrupts and clock gates; one set of cells can also use the audio front
end, and another links to Linux through shared memory for rpmsg.

- Tag `mtk-v1.0.0` on the release branch `mtk-v1.0`. Built at the tag, from
  a git clone or by the Yocto recipe, the hypervisor and the driver report
  the version `mtk-v1.0.0 (0-g<commit>)`; builds of later commits on the
  branch report `mtk-v1.0.0 (<n>-g<commit>)`. Without a release tag in the
  clone, they report `1.0 (<commit>)`.
- Tested on the Genio 510 EVK and the Genio 700 EVK at the tag
  `mtk-v1.0.0`.
- Based on upstream Jailhouse `master` at `e57d1eff` (2023-01-16, after
  v0.12), with 16 commits on top.

## What this release adds to upstream Jailhouse

### Hypervisor

- **MT8188 EINT, GPIO and clock gate units.** These blocks pack the bits of
  many pins into each register, so memory regions cannot divide them between
  cells. A cell owns individual pins, EINTs and clock gates through
  MediaTek-specific entries in its config, and the hypervisor mediates the
  accesses: each cell reads and writes only the fields of its own pins. The
  shared EINT interrupt goes to the cell that owns the pending pin, and an
  EINT is masked when its pin changes owner. The root cell cannot write the
  other registers of the EINT and GPIO blocks, as they may control pins. The
  pin configuration (IOCFG) blocks are not mediated; the root cell maps
  them. The units are built with `make CONFIG_SOC=mt8188`, and are active
  only if the root cell config has such entries. A hypervisor built without
  `CONFIG_SOC` has no such units and refuses cells with these entries.
- **Shared interrupts.** A unit can share one SPI between cells and choose
  the cell to deliver each occurrence to. The EINT unit uses this.
- **GICv3.**
  - An SGI that arrives while the cell is still handling the previous one
    with the same ID becomes pending again, as on hardware, instead of
    being dropped.
  - The pending state of a cell's SGIs is emulated in `GICR_ISPENDR0` and
    `GICR_ICPENDR0`. Zephyr SMP polls it, and deadlocked without this.
  - Redistributors no longer need to be in sequence with the CPU IDs. On
    MT8188, disabled cores keep their redistributors; the system config
    gives the size of the redistributor region.
- **Cortex-A78.** CPUs whose EL1 cannot run AArch32 can now be reset, so the
  A78 can be given to a cell and returned to Linux.
- **Secure monitor calls.** Trusted OS and Trusted Application calls (for
  example to OP-TEE) are forwarded to the secure world for the root cell
  only, as the secure world does not separate cells. SiP calls are forwarded
  only if the cell config lists their function ID. The hypervisor reports a
  denied SiP call once per cell and ID. The Genio root cells list the SiP
  calls of the BSP's Linux kernel.

### Linux driver

- Builds against Linux 6.2 and newer, including 6.6.
- `jailhouse disable` no longer leaks the PCI domain of the virtual PCI host.
  Before, a root cell with an ivshmem device lost that device after the
  first disable until Linux was rebooted.

### Cells

Each EVK has nine cells, built from one table per board,
`configs/arm64/genio-510-evk-cells.c` and `genio-700-evk-cells.c`. A cell is
one block of the table that lists what is particular to it: its CPUs,
memory regions, interrupts, virtual PCI devices, SiP calls, pins and clock
gates. Edit a block to adapt a cell, for example to give it other pins. CPUs
that the board does not have, and pins or interrupts that do not fit the
cell's entries, fail the build.

| Cell | CPUs | Contents |
|---|---|---|
| `genio-<board>-evk` | all | Root cell (Linux), with a virtual PCI host and one ivshmem device |
| `genio-<board>-evk-uart-demo` | 3 | Jailhouse's `uart-demo` inmate on UART1 |
| `genio-<board>-evk-zephyr` | 3 (Cortex-A55) | Zephyr with UART1 (its pins and clock gate), and GPIO 38 and 40 with their EINTs |
| `genio-<board>-evk-zephyr-a78` | 5 on the Genio 510, 7 on the Genio 700 (Cortex-A78) | As `-zephyr` |
| `genio-<board>-evk-zephyr-smp` | 2 and 3 | As `-zephyr`, for Zephyr SMP |
| `genio-<board>-evk-zephyr-afe`, `-afe-a78`, `-afe-smp` | as above | As the three cells above, plus the audio front end and its eTDM pins |
| `genio-<board>-evk-zephyr-rpmsg` | 3 | As `-zephyr`, plus the second peer of the root cell's ivshmem device, for rpmsg with Linux |

Zephyr images are loaded at `0x8000` of the cell's 8 MiB memory window at
physical `0x6b000000`. The ivshmem device appears in Linux as PCI device
`0001:00:00.0`; its shared memory is the 1 MiB at `0x6ba00000` that the BSP's
Jailhouse device tree overlay reserves.

### rpmsg with Linux

The `-zephyr-rpmsg` cells exchange rpmsg messages with Linux. Linux binds the
ivshmem device to the BSP's remoteproc driver, `mtk-jh-rproc`, which appears
as the remoteproc named `0001:00:00.0` once Jailhouse is enabled. The driver
never loads or starts the remote: the cell is started with `jailhouse cell
start`, and Linux then attaches to it. The rpmsg sample of
mtk-zephyr/samples shows the exchange: its Zephyr application runs in the
cell, and its Linux program attaches and echoes messages through
`/dev/rpmsg0`.

- Start the cell first, then attach Linux:
  `echo start > /sys/class/remoteproc/remoteproc<n>/state`, which the
  sample's Linux program does. The state then reads `attached`.
- Attach once per cell start. Writing `start` again does not fail, but takes
  another reference, and each `detach` drops only one.
- Before `jailhouse cell destroy`, detach Linux:
  `echo detach > /sys/class/remoteproc/remoteproc<n>/state`, until the state
  reads `detached`. The driver does not implement `stop`.

### Config format

The cell config format is revision 15 (upstream: 14). It adds the size of
the redistributor region, the list of SiP calls a cell may make, and
MediaTek pin and clock gate entries. Cells built from upstream Jailhouse or
from earlier MediaTek trees do not load; build them from this release.

## Tested setup

- IoT Yocto `rity-scarthgap-v26.0` with Linux 6.6.137 and
  `rity-bringup-image`, with the Jailhouse recipe of the
  `meta-mediatek-experimental` layer, branch `scarthgap`. The recipe builds
  this release from the branch `mtk-v1.0` with `CONFIG_SOC=mt8188` and
  installs the board's nine cells. The layer's Jailhouse device tree
  overlay withholds the inmate memory window from Linux and reserves the
  ivshmem memory.
- Zephyr images built with Zephyr SDK 1.0.1 from the tag
  `mtk-genio-v1.0.0` of mtk-zephyr, which marks the validated mtk-zephyr
  commit. This release pairs with it and with the tag `mtk-genio-v1.0.0` of
  mtk-zephyr/samples, whose `west.yml` pins mtk-zephyr to that tag, so
  `west init -m https://github.com/mtk-zephyr/samples --mr mtk-genio-v1.0.0`
  and `west update` reproduce the pairing. The rpmsg sample's Zephyr
  application is built with the same Zephyr tree.
- **Genio 510 EVK and Genio 700 EVK, at `mtk-v1.0.0`, built by the Yocto
  recipe with `CONFIG_SOC=mt8188`:** all 50 checks of the regression pass on
  each board:
  - enable, disable and re-enable, with a clean hypervisor console;
  - the root cell's pins and EINTs, and the uart-demo cell;
  - Zephyr in the A55 cell with 10 create/start/destroy cycles, and the
    memory window, GPIO/EINT loopback, timer and UART samples;
  - the same in the A78 cell (CPU 5 on the Genio 510, CPU 7 on the
    Genio 700), with the core back in Linux after 5 cycles;
  - the SMP cell: PSCI `CPU_ON` and its refusal outside the cell, and
    Zephyr's `synchronization` sample with a thread on each CPU;
  - SGI pending state (`GICR_ISPENDR0`/`ICPENDR0`);
  - ten audio samples in the AFE cell and three each in the AFE cells on the
    A78 and on two A55 cores, over eTDM loopback wires;
  - pin isolation: cells that map a mediated block or claim another cell's
    pin are refused, and pins return to Linux when their cell is destroyed.
    As the pin configuration is not mediated, its two checks are reversed:
    Linux's writes reach the pin configuration of a cell's pins.

  Beyond the regression, on each board:
  - three more disable/enable cycles, each with the ivshmem device back in
    Linux and no kernel warning;
  - `hello_world` in the `-zephyr-rpmsg` cell, with the hypervisor reporting
    the shared memory connection to the root cell;
  - the rpmsg sample in the `-zephyr-rpmsg` cell: Linux and Zephyr echo
    messages in both directions, and Linux detaches before the cell is
    destroyed;
  - a hypervisor built without `CONFIG_SOC` refuses the root cell:
    `jailhouse enable` fails with "Invalid argument", and Linux keeps
    running.

## Known limitations

- **Pin configuration.** The hypervisor does not mediate the pin
  configuration (IOCFG) blocks of the GPIO controller; the root cell maps
  them. Linux can therefore change the pull, drive and input settings of
  every pin, including those of the cells' pins, the eTDM pins of the AFE
  cells among them.
- **EINT debounce.** The EINT unit does not describe the debounce and event
  registers, so Linux's writes to them are ignored: Linux cannot set up
  hardware debounce or the event registers of its EINTs.
- **Audio power domain.** The audio power domain is off after boot, and only
  Linux can switch it on. Before starting an AFE cell, run
  `echo on > /sys/devices/platform/soc/10b10000.afe/power/control`. While an
  AFE cell runs, Linux must keep the domain on and must not use the eTDM
  ports.
- **Linux power management of cell devices.** The root cell can still write
  the registers of the clock gate block that do not belong to a pin, as
  Linux needs them. The bus protection of the power domains in that block
  therefore stays with Linux, and keeping Linux's power management away from
  the devices of a cell, such as the audio front end, relies on Linux.
- **AFE interrupt.** The AFE cells do not get the audio front end's
  interrupt (SPI 822); the Zephyr driver polls. Jailhouse returns a destroyed
  cell's interrupts to Linux disabled, so Linux audio would lose its
  interrupt after such a cell.
- **Audio reset.** The AFE cells can write the whole reset block (`toprgu`,
  `0x10007000`) so that Zephyr can reset the audio subsystem. Such a cell
  can therefore reset other subsystems and reach the watchdog; trust AFE
  cells accordingly.
- **rpmsg teardown.** The BSP's remoteproc driver does not notice when the
  `-zephyr-rpmsg` cell is destroyed. If Linux is still attached, it keeps
  the destroyed cell's rings, and a new cell waits for Linux forever; detach
  until the state reads `detached` and start the cell again. Always detach
  before `jailhouse cell destroy` (see rpmsg with Linux).
- **SDEI.** Jailhouse's SDEI mode, which takes management events through the
  firmware, is untested; the Genio firmware does not provide SDEI.
- **GICv2.** The two SGI fixes are for GICv3 only. GICv2 has the same flaws;
  the Genio SoCs use GICv3.
- **SoCs.** Only MT8188-based SoCs are supported. The hypervisor's tables
  of mediated blocks are specific to MT8188.
