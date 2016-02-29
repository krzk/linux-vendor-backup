ccflags-y := -Ikernel/swap

ifeq ($(CONFIG_ARM), y)
    link = arm
endif

ifeq ($(CONFIG_X86), y)
    link = x86
endif

ccflags-y += -Ikernel/swap/kprobe/arch/$(link) \
             -Ikernel/swap/uprobe/arch/$(link)

obj-$(CONFIG_SWAP_DA) := swap_us_manager.o
swap_us_manager-y := us_manager.o us_slot_manager.o helper.o debugfs_us_manager.o \
                     sspt/ip.o sspt/sspt_page.o sspt/sspt_file.o sspt/sspt_proc.o \
                     sspt/sspt_feature.o sspt/sspt_filter.o \
                     pf/proc_filters.o pf/pf_group.o \
                     img/img_proc.o img/img_file.o img/img_ip.o
