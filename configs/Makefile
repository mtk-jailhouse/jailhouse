#
# Jailhouse, a Linux-based partitioning hypervisor
#
# Copyright (c) Siemens AG, 2013
#
# Authors:
#  Jan Kiszka <jan.kiszka@siemens.com>
#
# This work is licensed under the terms of the GNU GPL, version 2.  See
# the COPYING file in the top-level directory.
#

include $(ALWAYS_COMPAT_MK)

-include $(GEN_CONFIG_MK)

LINUXINCLUDE := -I$(src)/../hypervisor/arch/$(SRCARCH)/include \
		-I$(src)/../hypervisor/include \
		-I$(src)/../include
KBUILD_CFLAGS := -Werror -Wall -Wextra -D__LINUX_COMPILER_TYPES_H

ifneq ($(wildcard $(obj)/../include/jailhouse/config.h),)
KBUILD_CFLAGS += -include $(obj)/../include/jailhouse/config.h
endif

OBJCOPYFLAGS := -O binary --remove-section=.note.gnu.property

# A table, *-cells.c, defines several cells, each in a section
# .cell.<name> that becomes <name>.cell. The names are read from the
# table's lines that start with <KIND>_CELL("<name>", and checked against
# the sections of the compiled table.
TABLES = $(shell cd $(src); ls $(SRCARCH)/*-cells.c 2>/dev/null)
CONFIGS = $(filter-out $(TABLES),$(shell cd $(src); ls $(SRCARCH)/*.c))

table_cells = $(addprefix $(dir $(1)),$(addsuffix .cell,$(shell \
	sed -n 's/^[A-Z0-9_]*_CELL."\([^"]*\)".*/\1/p' $(src)/$(1))))

always-y := $(CONFIGS:.c=.cell)

targets += $(CONFIGS:.c=.o) $(CONFIGS:.c=.cell)

define table_rules
always-y += $(call table_cells,$(1)) $(1:.c=.sections)
targets += $(1:.c=.o) $(call table_cells,$(1)) $(1:.c=.sections)

$(addprefix $$(obj)/,$(call table_cells,$(1))): $$(obj)/$(1:.c=.o) FORCE
	$$(call if_changed,extract_cell)

$$(obj)/$(1:.c=.sections): $$(obj)/$(1:.c=.o) FORCE
	$$(call if_changed,check_sections,$(notdir $(call table_cells,$(1))))
endef
$(foreach table,$(TABLES),$(eval $(call table_rules,$(table))))

quiet_cmd_extract_cell = OBJCOPY $@
      cmd_extract_cell = $(OBJCOPY) -O binary \
	--only-section=.cell.$(basename $(@F)) $< $@

quiet_cmd_check_sections = CHECK   $@
      cmd_check_sections = \
	$(READELF) -SW $< | sed -n 's/.* \.cell\.\([^ ]*\) .*/\1.cell/p' | \
	LC_ALL=C sort > $@ && \
	echo $(sort $(2)) | tr ' ' '\n' | diff -u - $@

DTS = $(shell cd $(src); ls $(SRCARCH)/dts/*.dts 2>/dev/null)
always-y += $(DTS:.dts=.dtb)
targets += $(DTS:.dts=.dtb)

# prevent deleting intermediate files which would cause rebuilds
ifdef KEEP_INTERMEDIATES
.SECONDARY: $(addprefix $(obj)/,$(CONFIGS:.c=.o))
endif

$(obj)/%.cell: $(obj)/%.o FORCE
	$(call if_changed,objcopy)
