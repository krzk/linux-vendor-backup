ccflags-y += -Idrivers/media/platform/exynos4-is

exynos-iss-objs := exynos5-mdev.o

ifeq ($(CONFIG_VIDEO_SAMSUNG_EXYNOS5_FIMC_IS),y)
exynos-iss-objs += fimc-is-core.o fimc-is-isp.o fimc-is-scaler.o
exynos-iss-objs += fimc-is-pipeline.o fimc-is-interface.o fimc-is-sensor.o fimc-is-backend.o fimc-is-fw.o
endif

obj-$(CONFIG_VIDEO_SAMSUNG_EXYNOS5_CAMERA) += exynos-iss.o
