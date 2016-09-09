MODULE_NAME := tzdev
ccflags-y := -DCONFIG_SECOS_NO_RPMB=1 -DCONFIG_PSCI=1

obj-$(CONFIG_ARTIK_TRUSTZONE_DRIVER) += $(MODULE_NAME).o

$(MODULE_NAME)-objs += tzdev_main.o
$(MODULE_NAME)-objs += tzlog.o
$(MODULE_NAME)-objs += sstransaction.o
$(MODULE_NAME)-objs += tzpage.o
$(MODULE_NAME)-objs += ss_core.o
$(MODULE_NAME)-objs += ss_dev.o
$(MODULE_NAME)-objs += ss_file.o
$(MODULE_NAME)-objs += ss_rpmb.o
$(MODULE_NAME)-objs += tzsys.o
$(MODULE_NAME)-objs += tzdev_smc.o
$(MODULE_NAME)-objs += tzlinkmgr.o
$(MODULE_NAME)-objs += tzwsm.o
$(MODULE_NAME)-objs += tzmem.o
$(MODULE_NAME)-objs += tzdev_plat.o
