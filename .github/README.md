# MediaTek Jailhouse

MediaTek's [Jailhouse](https://github.com/siemens/jailhouse) partitioning
hypervisor for the Genio 510 and Genio 700 EVKs. Jailhouse splits one SoC into
isolated cells: Linux keeps running in the root cell, and a real-time operating
system such as Zephyr runs next to it, on CPU cores, memory and devices of its
own.

This repository is based on upstream Jailhouse `master` at
[`e57d1eff`](https://github.com/siemens/jailhouse/commit/e57d1eff6d55aeed5f977fe4e2acfb6ccbdd7560)
and adds the Genio support. Development happens here; changes are not
submitted upstream.

**[User guide (PDF)](https://github.com/mtk-zephyr/genio-docs/releases/latest/download/genio-jailhouse-zephyr-guide.pdf):**
build the IoT Yocto image with Jailhouse, flash the board, build Zephyr and run
it in a cell, step by step.

## Supported boards

| Board | SoC | CPUs (Linux numbering) |
|---|---|---|
| Genio 510 EVK | MT8370 | Cortex-A55: 0-3; Cortex-A78: 4, 5 |
| Genio 700 EVK | MT8390 | Cortex-A55: 0-5; Cortex-A78: 6, 7 |

Both SoCs use the MT8188 units. Linux and the cells share the GPIO, external
interrupt (EINT) and clock gate registers; the hypervisor lets each cell read
and change only the fields of the pins and clock gates it owns. The pin
configuration registers belong to Linux.

## Branches and releases

| Branch | Role |
|---|---|
| `mtk-genio-dev` | Default branch. All development lands here. |
| `mtk-v1.0` | Release branch, cut from `mtk-genio-dev`. It takes fixes as new commits only. Later releases get branches of their own (`mtk-v1.1`, ...). |
| `master` | Mirror of upstream Jailhouse `master`. |

Build products from a release branch. IoT Yocto builds Jailhouse with the
`jailhouse` recipe of the `meta-mediatek-experimental` layer, which builds this
repository from the release branch `mtk-v1.0` with `CONFIG_SOC=mt8188` and
installs the cells of the machine it builds for; the user guide covers the
build. The release notes of each release branch list what the release was
tested with and its known limitations, for example [`Documentation/mtk-v1.0-release-notes.md`](https://github.com/mtk-jailhouse/jailhouse/blob/mtk-v1.0/Documentation/mtk-v1.0-release-notes.md)
on `mtk-v1.0`.

The hypervisor, driver, tools and cells of a release belong together. The cell
configuration format is revision 15; cells built from other trees do not load.

To build outside IoT Yocto, run `make` against the Genio BSP kernel.
`CONFIG_SOC=mt8188` selects the hypervisor's MediaTek units for the SoCs of both
boards; without it, the hypervisor has no such units and refuses the Genio cells.
No `config.h` is needed:

```bash
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- KDIR=/path/to/kernel/build CONFIG_SOC=mt8188
```

The kernel needs the Jailhouse patches that the IoT Yocto layer applies, and the
board needs the BSP's Jailhouse device-tree overlay, which reserves the memory
of the hypervisor and the cells. The user guide covers both.

## Compatibility

| Release | IoT Yocto (BSP) | Linux | [mtk-zephyr](https://github.com/mtk-zephyr/mtk-zephyr) | [Genio samples](https://github.com/mtk-zephyr/samples) |
|---|---|---|---|---|
| `mtk-v1.0` | v26.0 (`rity-scarthgap-v26.0`) | 6.6.137 | `mtk-genio-v1.0.0` | `mtk-genio-v1.0.0` |

The samples tag's `west.yml` pins the mtk-zephyr tag, so
`west init -m https://github.com/mtk-zephyr/samples --mr mtk-genio-v1.0.0` and
`west update` reproduce the pairing.

## Cells

`make` builds the cells of both boards in `configs/arm64/`; the IoT Yocto image
installs them in `/usr/share/jailhouse/cells/`. In the table, `<board>` is `510`
or `700`.

| Cell file | CPUs, Genio 510 | CPUs, Genio 700 | Runs | Devices |
|---|---|---|---|---|
| `genio-<board>-evk.cell` | 0-5 | 0-7 | Linux (root cell) | All, except those of the other cells; a virtual PCI host with an ivshmem device |
| `genio-<board>-evk-uart-demo.cell` | 3 | 3 | Jailhouse's UART demo | UART1 |
| `genio-<board>-evk-zephyr.cell` | 3 | 3 | Zephyr on a Cortex-A55 | UART1, GPIO 38 and 40 |
| `genio-<board>-evk-zephyr-a78.cell` | 5 | 7 | Zephyr on a Cortex-A78 | As `-zephyr` |
| `genio-<board>-evk-zephyr-smp.cell` | 2, 3 | 2, 3 | Zephyr on two Cortex-A55 (SMP) | As `-zephyr` |
| `genio-<board>-evk-zephyr-afe.cell` | 3 | 3 | Zephyr with audio | As `-zephyr`, plus the audio front end, shared with Linux, and the eTDM pins |
| `genio-<board>-evk-zephyr-afe-a78.cell` | 5 | 7 | Zephyr with audio on a Cortex-A78 | As `-zephyr-afe` |
| `genio-<board>-evk-zephyr-afe-smp.cell` | 2, 3 | 2, 3 | Zephyr with audio on two Cortex-A55 | As `-zephyr-afe` |
| `genio-<board>-evk-zephyr-rpmsg.cell` | 3 | 3 | Zephyr with an ivshmem device shared with Linux, for RPMsg | As `-zephyr`, plus the ivshmem device, shared with the root cell |

The uart-demo and Zephyr cells load into the same 8 MiB of memory and use
UART1, so only one of them exists at a time. Zephyr images start at address
`0x8000` of their cell.

Each board's cells come from one table, `configs/arm64/genio-<board>-evk-cells.c`.
To adapt a cell, edit its CPUs, shared devices, interrupts, SiP calls, pins or
clock gates there and rebuild; the cell keeps its file name. See
[CONTRIBUTING.md](CONTRIBUTING.md) for how to check the result.

## License

Jailhouse is licensed under the GNU General Public License version 2; see
[COPYING](../COPYING) and [LICENSING.md](../LICENSING.md). Upstream's
[README](../README.md) and [Documentation](../Documentation) describe Jailhouse
in general.

## Related

- [User guide](https://github.com/mtk-zephyr/genio-docs): running Zephyr
  alongside Linux with Jailhouse on Genio 510 and Genio 700 EVKs
- [mtk-zephyr](https://github.com/mtk-zephyr/mtk-zephyr): Zephyr for MediaTek
  Genio, and the [Genio samples](https://github.com/mtk-zephyr/samples)
- [IoT Yocto](https://genio.mediatek.com/doc/iot-yocto/latest/sw/yocto/get-started.html):
  the Linux BSP for Genio
