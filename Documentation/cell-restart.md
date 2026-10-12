Restarting Failed Cells
=======================

A non-root cell fails when the hypervisor stops it: on a fault, when it does
not answer a message in time, or when a unit fails it, such as the MediaTek
watchdog unit when the cell's watchdog expires or the cell asks for a reset.
A failed cell keeps its resources until the root cell destroys it.

`tools/jailhouse-cell-restartd` restarts failed cells. It waits on the
`state` attribute of each cell (see [1]), which the driver signals when a cell
fails, and checks the cells every 5 s anyway. It restarts each failed cell
that its configuration names, with `jailhouse cell destroy`, `create`, `load`
and `start`. It needs only the core modules of Python 3 (Yocto:
`python3-core`). `make install` puts it into `<libexecdir>/jailhouse`;
`tools/jailhouse-cell-restartd.service` is a systemd unit for it.


The driver ticks the hypervisor every `tick_ms` milliseconds (a parameter of
`jailhouse.ko`, 100 by default, writable in
`/sys/module/jailhouse/parameters/tick_ms`), and the hypervisor checks the
cells' watchdogs on each tick. `tick_ms=0` stops the tick, for tests only: the
cells' watchdogs are then not checked, and the kernel log says so.


Configuration
-------------

The daemon reads `/etc/jailhouse/cell-restart.conf`, or the file named on its
command line. Each section names a cell by its name, not by its file:

    [DEFAULT]
    restart_limit = 5
    restart_window = 300

    [zephyr]
    config = /usr/share/jailhouse/cells/genio-510-evk-zephyr.cell
    image = /usr/share/zephyr/zephyr.bin 0x8000

    [zephyr-rpmsg]
    config = /usr/share/jailhouse/cells/genio-510-evk-zephyr-rpmsg.cell
    image = /usr/share/zephyr/rpmsg.bin 0x8000
    remoteproc = 0001:00:00.0

| Key | Meaning |
|---|---|
| `config` | The cell's configuration file. |
| `image` | The image and its load address (default 0); one per line for several images. |
| `remoteproc` | Before the cell is destroyed, the remoteproc of this name is detached until its state is "detached". Each `start` on an attached remoteproc takes a reference, which a `detach` returns. |
| `before`, `after` | Shell commands, run before the restart and after the cell has started again, for example to stop and start the applications that use the cell. |
| `restart_limit`, `restart_window` | After `restart_limit` restarts within `restart_window` seconds (default 5 and 300), the cell stays failed until it runs again. Restarts within the window back off: 0, 1, 3, 7, 15 and at most 30 seconds after the failure. |

When a restart stops after the cell was destroyed, the daemon tries again,
whether the cell exists or not.

A cell restarted after its watchdog expired or after it asked for a reset can
tell so from its watchdog's status register (MediaTek: `WDT_STA` bits 31 and
30).

[1] Documentation/sysfs-entries.txt
