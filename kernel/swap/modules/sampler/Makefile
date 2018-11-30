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
obj-y := swap_sampler.o
swap_sampler-y := swap_sampler_module.o

ifdef CONFIG_HIGH_RES_TIMERS
    swap_sampler-y += sampler_hrtimer.o
else
    swap_sampler-y += sampler_timer.o
endif
