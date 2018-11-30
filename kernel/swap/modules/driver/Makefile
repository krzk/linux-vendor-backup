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
obj-y := swap_driver.o
swap_driver-y := swap_driver_module.o \
                     device_driver.o \
                     driver_to_buffer.o \
                     driver_debugfs.o

ifeq ($(CONFIG_CONNECTOR),y)
	swap_driver-y += us_interaction.o
endif
