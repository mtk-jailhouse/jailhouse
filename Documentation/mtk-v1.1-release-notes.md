# mtk-v1.1 release notes

mtk-jailhouse 1.1 gives each Zephyr cell a watchdog of its own and lets
Linux restart the cells that fail, on the Genio 510 EVK (MT8370) and the
Genio 700 EVK (MT8390). It also names each cell after its file, and fixes
the mediation of a few EINT, GPIO and clock gate registers.

- Tag `mtk-v1.1.0` on the release branch `mtk-v1.1`. Built at the tag, the
  hypervisor and the driver report the version `mtk-v1.1.0 (0-g<commit>)`;
  builds of later commits on the branch report `mtk-v1.1.0 (<n>-g<commit>)`.
- Tested on the Genio 510 EVK and the Genio 700 EVK at the tag
  `mtk-v1.1.0`.
- Based on `mtk-v1.0.0`, with 23 commits on top.

## What this release adds

### A watchdog for each Zephyr cell

The MT8188 has one watchdog, the TOPRGU at `0x10007000`, and it stays
Linux's: Linux's driver and systemd keep using it as before (on the Genio
image, systemd feeds it every 15.5 s with a 31 s timeout). Each Zephyr cell
gets a watchdog of its own at the same address, with the hardware's
registers, so that a Zephyr watchdog driver works the same in a cell and on
bare metal.

- A cell arms its watchdog and kicks it through the TOPRGU registers
  `MODE`, `LENGTH` and `RESTART`, with the hardware's keys and its timeout
  range (2 to 31 s in steps of 1/64 s). When a cell misses its deadline, the
  hypervisor stops that cell, and only that cell: its state becomes
  `failed`, and the hypervisor console logs how late the expiry was
  detected.
- A cell that writes the chip reset (`SWRST`), or that calls PSCI
  `SYSTEM_RESET`, fails the same way. Neither resets the SoC. A Zephyr
  `sys_reboot(SYS_REBOOT_COLD)` therefore restarts only its cell.
- After a restart, `STA` tells the cell why its previous run ended: bit 31
  after a timeout, bit 30 after a chip reset or `SYSTEM_RESET`, 0 otherwise.
  The bits hold until the cell starts again, and across the destruction and
  creation of a cell of the same name.
- The deadlines are checked on a tick that the Jailhouse driver gives the
  hypervisor every 100 ms (module parameter `tick_ms`). An expiry is
  therefore detected at most one tick late. If the ticks stop while Linux
  runs, the hypervisor reports it on Linux's next watchdog kick.
- The cells of this release have the watchdog: `-zephyr`, `-zephyr-a78`,
  `-zephyr-smp`, `-zephyr-afe`, `-zephyr-afe-a78`, `-zephyr-afe-smp` and
  `-zephyr-rpmsg`. The watchdog is a fourth MediaTek unit, built with
  `CONFIG_SOC=mt8188` like the other three.
- If Linux hangs, systemd stops feeding the hardware watchdog, and it resets
  the SoC within 31 s, cells and all, as without the hypervisor.

### Restarting failed cells

`jailhouse-cell-restartd` restarts the cells that fail: on a watchdog
expiry, a chip reset, a fault, or a message the cell did not answer. The
driver signals a failure on the cell's `state` attribute, and the daemon
checks every 5 s in addition. It restarts each failed cell that its
configuration names:

1. runs the cell's `before` command, if any;
2. detaches the cell's remoteproc, if any, until the state reads
   `detached`, which the rpmsg cell needs;
3. destroys the cell, creates it, loads its images and starts it;
4. runs the `after` command, if any.

Restarts back off (0, 1, 3, 7, 15, then 30 s), and after `restart_limit`
restarts within `restart_window` seconds, a cell stays failed until it runs
again. The configuration file is `/etc/jailhouse/cell-restart.conf`; the
format and an example are in `Documentation/cell-restart.md`. The daemon
needs only `python3-core`. `make install` installs it in
`<libexecdir>/jailhouse`, and `tools/jailhouse-cell-restartd.service` is the
systemd unit for it.

### Cell names

Each cell is named after its file without the board prefix:
`genio-510-evk-zephyr-afe.cell` is the cell `zephyr-afe`. In 1.0 every
Zephyr cell was named `zephyr`. The build checks that each name matches its
file.

### Config format: revision 16

The cell config format is revision 16 (1.0: 15). It adds the vendor
resource type of the watchdog. `Documentation/configuration-format.md` now
describes the whole format.

## Changes in behaviour

1. **Revision 16.** The cells of 1.0 do not load into 1.1, nor 1.1's into
   1.0. Rebuild the cells from this release; the recipe does. To give a cell
   of your own a watchdog, add a watchdog entry, as the Genio cells do
   (`WDT()` in `configs/arm64/genio-evk.h`).
2. **The root cell no longer maps the TOPRGU page.** Its accesses pass
   through the hypervisor unchanged, except that a write to the subsystem
   resets (`SWSYSRST`) keeps the value of the resets that a cell owns.
3. **The AFE cells no longer map the TOPRGU.** In 1.0 they mapped the whole
   block to reset the audio subsystem, and could reach every subsystem reset
   and the watchdog. In 1.1 their watchdog entry permits the audio reset
   (`AUDIO_SW_RST`) only; a write to any other reset is ignored and reported
   once. While an AFE cell exists, Linux's writes keep the audio reset's
   value.
4. **Cell names.** Scripts that address a cell by the name `zephyr` must use
   the cell's own name, or read it from the cell file: offset 8, 32 bytes,
   NUL-padded. The `tools/genio-inmate.sh` of mtk-zephyr/samples at its 1.1
   tag reads it, and serves the cells of both releases.
5. **Mediated registers.** A write to an EINT, GPIO or clock gate register
   that also carries bits of another cell now changes the writer's bits and
   leaves the others. In 1.0 it was dropped whole, so Linux lost the changes
   to its own EINTs in a word that a cell shares.
6. **The root cell's sub-word accesses.** An 8- or 16-bit or unaligned
   access of the root cell to a mediated register now reads 0 and is
   ignored, reported once per block. In 1.0 it stopped the CPU.
7. **Pin and gate entries.** A vendor entry for EINTs, GPIO pins or clock
   gates must start at a multiple of 32. The hypervisor refuses a cell whose
   entry does not, with a message naming the entry. In 1.0, such an entry
   was accepted but described the wrong pins. The Genio cells always started
   at multiples of 32.
8. **`CONFIG_SOC`** also accepts `mt8370` and `mt8390`, for the same units
   as `mt8188`.

## Fixes

- **CG0 status.** A cell's read of the clock gate group 0 status register
  (`0x90`) was masked with the wrong gates, so it never saw its own, and its
  read-modify-write was refused.
- **EINTs 160 and above.** Their domain enable and raw status registers were
  not mediated: a cell's writes were dropped and its reads returned 0.
- **A cell loaded again in place.** A cell loaded and started again without
  being destroyed could execute its previous image, as the CPU's instruction
  cache kept it. The hypervisor now invalidates the instruction cache of
  each CPU it resets into a cell. Seen on the Genio 700; upstream Jailhouse
  has the same gap.

## Tested setup

- IoT Yocto `rity-scarthgap-v26.0` with Linux 6.6.137 and
  `rity-bringup-image`, with the Jailhouse recipe of the
  `meta-mediatek-experimental` layer, branch `scarthgap`. The recipe builds
  this release from the branch `mtk-v1.1` with `CONFIG_SOC=mt8188`, installs
  the board's nine cells, and installs the restart daemon with its systemd
  unit.
- Zephyr images built with Zephyr SDK 1.0.1 from the tag
  `mtk-genio-v1.1.0` of mtk-zephyr, which adds the TOPRGU watchdog driver
  (snippet `mtk-wdt`), the reset cause through `hwinfo`, and a UART driver
  that clears the state that Linux may leave in UART1. This release pairs
  with that tag and with the tag `mtk-genio-v1.1.0` of
  mtk-zephyr/samples, which adds the watchdog `expiry` sample and the
  cell-name support of `tools/genio-inmate.sh`.
- **Genio 510 EVK and Genio 700 EVK, at `mtk-v1.1.0`:** all 52 checks of the
  regression pass on each board: the 50 of 1.0, plus a whole-word write to a
  register that a cell shares and a sub-word access of the root cell.
- Beyond the regression, on both boards unless noted:
  - **Expiry:**
    - a cell that stops kicking fails at its deadline, detected 0 to
      100 ms late;
    - so does a cell spinning with interrupts masked on both CPUs of the
      SMP cell, or after a fault inside the cell;
    - a stage-2 fault fails the cell at once;
    - of two cells with watchdogs, only the one that stops kicking fails;
    - with Linux's feeder stopped, the cells' watchdogs still work.
  - **Chip reset, `SYSTEM_RESET` and `STA`:** a cell's chip reset and its
    PSCI `SYSTEM_RESET` fail only the cell; `STA` reads the expected bits
    after each kind of end, in place and across destroy and create; the
    root cell's `SWSYSRST` keeps the AFE cell's audio reset.
  - **The tick:** stopping it is reported, and the armed cell fails when it
    resumes; changing `tick_ms` at runtime causes no false report (Genio
    700).
  - **Zephyr:**
    - the upstream watchdog tests (`wdt_basic_api`, `wdt_error_cases`) and
      the `task_wdt` sample, whose reboot restarts its cell;
    - `hwinfo` reports the reset cause after a timeout and after a reset.
  - **The daemon:**
    - restarts each of the seven Zephyr cells;
    - backs off and gives up as configured;
    - runs ten expiry and restart cycles without leaking hypervisor memory;
    - restarts the rpmsg cell, after which rpmsg echoes again.
  - **Linux:** Linux's watchdog keeps its settings across `jailhouse
    disable` and `enable`. On the Genio 700, a kernel crash with cells
    running resets the SoC through the hardware watchdog within 31 s, and
    `reboot` works.
  - **A cell loaded again in place** runs its new image every time (Genio
    700, where it failed before the fix).

## Known limitations

- **Expiry detection needs the Linux kernel.** The deadlines are checked on
  the driver's ticks. While the Linux kernel hangs, the cells' watchdogs are
  not checked; the hardware watchdog then resets the SoC within 31 s.
  Detection is at most one tick (100 ms) late.
- **`tick_ms=0`** stops the tick and with it every cell's watchdog; it is
  meant for tests, and the kernel log says so when it is set.
- **No pre-timeout.** A cell's watchdog resets the cell; it does not raise
  an interrupt before (Zephyr's watchdog callbacks are not supported).
- **`SYSTEM_RESET2`** is not supported: Zephyr's `sys_reboot(SYS_REBOOT_WARM)`
  fails in a cell, while a cold reboot restarts the cell.
- **The daemon and new cells.** The daemon watches a cell from its next
  wakeup on. The first failure of a cell created while the daemon waits can
  therefore take up to 5 s to be restarted; later failures are restarted at
  once.
- **UART1 on the Genio 700.** On the Genio 700, Linux has a driver for UART1
  (`ttyS1`), the cells' UART. Do not open `ttyS1` from Linux: doing so leaves
  UART1 with DMA and the high-speed mode set until the next boot. The
  Zephyr images of the paired release clear that state; other inmates do
  not. On a fresh boot, Linux also keeps UART1's clock off, so an inmate
  without a clock driver of its own, such as `uart-demo`, prints only after
  a Zephyr cell has run.
- **Kernel messages on the Genio 700.** Each time a cell gives a CPU back to
  Linux, the kernel logs `IRQ309: set affinity failed(-22)` and
  `IRQ321: set affinity failed(-22)`: it cannot move the PCIe MSIs to the
  onlined CPU. The messages are harmless and also appear in 1.0.
- **As in 1.0:**
  - the pin configuration (IOCFG) blocks are not mediated;
  - Linux cannot use EINT debounce;
  - the audio power domain must be switched on from Linux before an AFE
    cell starts;
  - Linux's power management of cell devices relies on Linux;
  - the AFE cells do not get the audio interrupt;
  - the rpmsg cell needs Linux to detach before it is destroyed (the
    daemon does this);
  - SDEI is untested;
  - the SGI fixes are for GICv3 only;
  - only MT8188-based SoCs are supported.
