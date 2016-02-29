ccflags-y := -Ikernel/swap

ifeq ($(CONFIG_ARM), y)
    link = arm
endif

ifeq ($(CONFIG_X86), y)
    link = x86
endif

ccflags-y += -Ikernel/swap/kprobe/arch/$(link) \
             -Ikernel/swap/uprobe/arch/$(link)

obj-$(CONFIG_SWAP_DA) := swap_writer.o
swap_writer-y := swap_writer_module.o \
                 debugfs_writer.o \
                 event_filter.o

swap_writer-$(CONFIG_ARM) += kernel_operations_arm.o
swap_writer-$(CONFIG_X86) += kernel_operations_x86.o
