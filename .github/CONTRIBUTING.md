# Contributing to MediaTek Jailhouse

Contributions follow upstream Jailhouse's rules in
[CONTRIBUTING.md](../CONTRIBUTING.md): the coding style of
[Documentation/coding-style.txt](../Documentation/coding-style.txt), one logical
change per commit, tested changes, and a sign-off under the Developer's
Certificate of Origin. This page lists what is particular to this repository.
Changes go here, not to the upstream mailing list.

## Branches and pull requests

- Base your work on `mtk-genio-dev`, the default branch, and open a pull
  request against it.
- A pull request needs one approval. It is merged by rebase or squash, so the
  history stays linear.
- A fix for a release lands on `mtk-genio-dev` first; the maintainers then add
  it to the release branch.
- `master`, `mtk-genio-dev` and the release branches `mtk-v*` are protected:
  no force-push and no deletion.
  A release branch cannot be deleted and takes changes only as new commits.

## Commits

Use upstream's commit format:

```
area: subarea: Summary in the imperative

Why the change is needed and what it does, wrapped at 72 columns.

Fixes: 123456789abc ("Subject of the commit that introduced the bug")
Co-authored-by: Co Author <co.author@example.com>
Signed-off-by: Your Name <your.name@example.com>
```

- **Summary:** prefix it with the area and subarea of the files you change, as
  `git log --oneline -- <file>` shows them, for example `arm-common: gic-v3:`,
  `configs: arm64:` or `driver: pci:`. Start with a capital letter and end
  without a period.
- **Body:** explain why, not only what. Keep refactoring, fixes and new
  features in separate commits.
- **Signed-off-by:** required on every commit (`git commit -s`), with your real
  name. It certifies the Developer's Certificate of Origin in upstream's
  CONTRIBUTING.md.
- **Co-authored-by:** one line for each co-author.
- **Fixes:** recommended for bug fixes, with a 12-character hash. Leave it out
  if you are not sure which commit introduced the bug.

## Every commit builds

Each commit must build on its own, without warnings. The hypervisor, cells,
inmates and tools build with `-Werror`; the driver does not, but it must
build without warnings against the BSP kernel as well. Build against a
Genio BSP kernel (IoT Yocto v26.0, Linux 6.6) that has the
Jailhouse patches of the IoT Yocto layer:

```bash
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- KDIR=/path/to/kernel/build CONFIG_SOC=mt8188
```

Without `CONFIG_SOC`, the hypervisor is built without the MediaTek units, as
upstream's; a change outside the units must build that way too.

To build every commit of your branch:

```bash
git rebase --exec 'make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- KDIR=/path/to/kernel/build CONFIG_SOC=mt8188' mtk-genio-dev
```

## Cells

The cells of each board are built from one table,
`configs/arm64/genio-<board>-evk-cells.c`, with the macros of
`configs/arm64/genio-evk.h`. The SoC's addresses and values are in
`configs/arm64/mt8188.h`.

- **Change a cell** by editing its block in the table: CPUs, memory regions of
  shared devices, interrupts, virtual PCI devices, SiP calls, pins and clock
  gates. The cell keeps its file name. The header comments describe each list.
- **Add a cell** with a new block. Start it in the first column with
  `GENIO_<kind>_CELL("genio-<board>-evk-<name>",`: the build takes the cell
  names from these lines.
- **Keep both boards alike.** Change both tables unless the change is specific
  to one board.
- **Keep facts in the headers.** Addresses, SoC values and the memory layout
  belong in `mt8188.h` and `genio-evk.h`; a table only selects from them.
- **Keep the SMP cells in step with Zephyr.** The CPUs of the `-smp` cells must
  be the cpu nodes of mtk-zephyr's `boards/mediatek/common/genio-evk-smp.dtsi`.
- **Check the cells.** The build checks CPUs, pins and interrupts. Check each
  board's cells against its root cell and each other; the check must report no
  problems:

  ```bash
  PYTHONPATH=. tools/jailhouse-config-check configs/arm64/genio-510-evk.cell configs/arm64/genio-510-evk-*.cell
  PYTHONPATH=. tools/jailhouse-config-check configs/arm64/genio-700-evk.cell configs/arm64/genio-700-evk-*.cell
  ```

  On a board with Jailhouse installed, the same check is `jailhouse config check`.
- **Change the format with care.** A change to the cell configuration format
  needs a new `JAILHOUSE_CONFIG_REVISION` and the matching change in
  pyjailhouse.

## New files

Give each new file a copyright and license header. GPL-2.0 is the default; see
[LICENSING.md](../LICENSING.md).
