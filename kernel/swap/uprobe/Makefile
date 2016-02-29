ccflags-y := -Ikernel/swap

ifeq ($(CONFIG_ARM), y)
    link = arm
endif

ifeq ($(CONFIG_X86), y)
    link = x86
endif

ccflags-y += -Ikernel/swap/kprobe/arch/$(link) \
             -Ikernel/swap/uprobe/arch/$(link)

obj-$(CONFIG_SWAP_DA) := swap_uprobe.o
swap_uprobe-y := swap_uprobes.o

### ARM
swap_uprobe-$(CONFIG_ARM) += arch/arm/swap-asm/swap_uprobes.o \
                             arch/arm/swap-asm/trampoline_thumb.o


### X86
swap_uprobe-$(CONFIG_X86) += arch/x86/swap-asm/swap_uprobes.o
