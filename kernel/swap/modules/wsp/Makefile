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

obj-y := swap_wsp.o
swap_wsp-y := wsp_module.o \
              wsp_msg.o \
              wsp_debugfs.o \
              wsp.o \
              wsp_res.o
