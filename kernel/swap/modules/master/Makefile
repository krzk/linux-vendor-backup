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

obj-y := swap_master.o
swap_master-y := master_module.o \
                 swap_debugfs.o \
                 swap_initializer.o \
                 swap_deps.o \
                 wait.o
