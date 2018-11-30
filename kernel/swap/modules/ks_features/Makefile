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
obj-y := swap_ks_features.o
swap_ks_features-y := ks_features.o \
                      ks_features_data.o \
                      ks_map.o \
                      file_ops.o \
                      ksf_msg.o
