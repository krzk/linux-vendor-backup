#
# Makefile for the SWAP
#
extra_cflags := -I$(M) -I$(M)/kprobe/arch/$(LINKNAME)/ -I$(M)/uprobe/arch/$(LINKNAME)/
extra_cflags += $(MCFLAGS)
export extra_cflags

obj-$(CONFIG_SWAP_DA) += \
         master/ \
         buffer/ \
         ksyms/ \
         driver/ \
         writer/ \
         kprobe/ \
         uprobe/ \
         us_manager/ \
         ks_features/ \
         sampler/ \
         energy/ \
         parser/ \
         retprobe/ \
         webprobe/ \
         fbiprobe/ \
         wsp/ \
         nsp/ \
         task_ctx/
         # uihv/
         # preload/
         # loader/

obj-$(CONFIG_SWAP_TD_HOOKS) += td_hooks.o
obj-$(CONFIG_SWAP_US_HOOKS) += us_hooks.o
obj-$(CONFIG_SWAP_SYSHOOK) += syshook.o
obj-$(CONFIG_SWAP_CONTEXT_HOOKS) += context_hooks.o
obj-$(CONFIG_SWAP_ENERGY_HOOKS) += energy_hooks.o
obj-$(CONFIG_SWAP_FOPS_HOOKS) += fops.o
