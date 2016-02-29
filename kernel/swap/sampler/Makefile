ccflags-y := -Ikernel/swap

ifeq ($(CONFIG_ARM), y)
    link = arm
endif

ifeq ($(CONFIG_X86), y)
    link = x86
endif

ccflags-y += -Ikernel/swap/kprobe/arch/$(link) \
             -Ikernel/swap/uprobe/arch/$(link)

obj-$(CONFIG_SWAP_DA) := swap_sampler.o
swap_sampler-y := swap_sampler_module.o

ifdef CONFIG_HIGH_RES_TIMERS
    swap_sampler-y += sampler_hrtimer.o
else
    swap_sampler-y += sampler_timer.o
endif
