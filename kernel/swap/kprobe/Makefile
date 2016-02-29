ccflags-y := -Ikernel/swap

ifeq ($(CONFIG_ARM), y)
    link = arm
endif

ifeq ($(CONFIG_X86), y)
    link = x86
endif

ccflags-y += -Ikernel/swap/kprobe/arch/$(link) \
             -Ikernel/swap/uprobe/arch/$(link)

obj-$(CONFIG_SWAP_DA) := swap_kprobe.o
swap_kprobe-y := swap_kprobes.o \
                 swap_kprobes_deps.o \
                 swap_slots.o

### ARM
swap_kprobe-$(CONFIG_ARM) += arch/arm/swap-asm/swap_kprobes.o \
                             arch/arm/swap-asm/trampoline_arm.o
ifeq ($(CONFIG_STRICT_MEMORY_RWX), y)
swap_kprobe-$(CONFIG_ARM) += arch/arm/swap-asm/memory_rwx.o
endif #ifeq ($(CONFIG_STRICT_MEMORY_RWX), y)


### X86
swap_kprobe-$(CONFIG_X86) += arch/x86/swap-asm/swap_kprobes.o
