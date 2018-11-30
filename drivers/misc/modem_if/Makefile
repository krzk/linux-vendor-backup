# Makefile of modem_if

EXTRA_CFLAGS += -Idrivers/misc/modem_v1
EXTRA_CFLAGS += -lnet/core/

obj-y += modem_main.o modem_io_device.o
obj-y += modem_net_flowcontrol_device.o modem_utils.o modem_dump.o

obj-$(CONFIG_SOC_EXYNOS7580) += modem_modemctl_device_sh310ap.o
obj-$(CONFIG_SOC_EXYNOS7890) += modem_modemctl_device_sh333ap.o
obj-$(CONFIG_SOC_EXYNOS8890) += modem_modemctl_device_sh333ap.o
obj-$(CONFIG_SOC_EXYNOS7880) += modem_modemctl_device_sh325ap.o
obj-$(CONFIG_SOC_EXYNOS7570) += modem_modemctl_device_sh325ap.o
obj-$(CONFIG_SOC_EXYNOS8895) += modem_modemctl_device_sh333ap.o
obj-$(CONFIG_SOC_EXYNOS7872) += modem_modemctl_device_sh333ap.o
obj-$(CONFIG_SOC_EXYNOS9810) += modem_modemctl_device_sh333ap.o
obj-$(CONFIG_SOC_EXYNOS9610) += modem_modemctl_device_sh333ap.o
obj-$(CONFIG_SOC_EXYNOS9110) += modem_modemctl_device_sh333ap.o

obj-$(CONFIG_LINK_DEVICE_SHMEM)	+= modem_link_device_shmem.o modem_notifier.o

# Check whether or not memory-type interface
ifeq ($(CONFIG_LINK_DEVICE_SHMEM),y)
LINK_DEVICE_MEMORY_INTERFACE=y
endif

ifdef LINK_DEVICE_MEMORY_INTERFACE
obj-y += modem_link_device_memory.o
endif
