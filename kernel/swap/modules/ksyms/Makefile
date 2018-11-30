ccflags-y := -Ikernel/swap/modules

ifeq ($(CONFIG_ARM), y)
    link = arm
endif

ifeq ($(CONFIG_ARM64), y)
    link = arm64
endif

ifeq ($(CONFIG_X86), y)
    link = x86
endif

ccflags-y += -Ikernel/swap/modules/kprobe/arch/$(link) \
             -Ikernel/swap/modules/uprobe/arch/$(link)

obj-y := swap_ksyms.o
swap_ksyms-y := ksyms_module.o

ifeq ($(CONFIG_KALLSYMS),y)
	swap_ksyms-y += ksyms.o
else
	swap_ksyms-y += no_ksyms.o
endif
