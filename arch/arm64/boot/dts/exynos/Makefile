dtb-$(CONFIG_SOC_EXYNOS8895) += exynos8895-smdk8895.dtb
dtb-$(CONFIG_SOC_EXYNOS9810) += exynos9810.dtb

ifeq ($(CONFIG_GALILEO),y)
ifeq ($(CONFIG_GALILEO_SMALL),y)
ifeq ($(CONFIG_GALILEO_LTE),y)
dtb-y += exynos9110-tizen_galileo_small_lte-r04.dtb \
         exynos9110-tizen_galileo_small_lte-r05.dtb \
         exynos9110-tizen_galileo_small_lte-r06.dtb \
         exynos9110-tizen_galileo_small_lte-r07.dtb \
         exynos9110-tizen_galileo_small_lte-r08.dtb \
         exynos9110-tizen_galileo_small_lte-r09.dtb \
         exynos9110-tizen_galileo_small_lte-r10.dtb \
	 exynos9110-tizen_galileo_small_lte-r11.dtb
else
dtb-y += exynos9110-tizen_galileo_small_bt-r06.dtb \
         exynos9110-tizen_galileo_small_bt-r07.dtb \
         exynos9110-tizen_galileo_small_bt-r08.dtb \
         exynos9110-tizen_galileo_small_bt-r09.dtb \
         exynos9110-tizen_galileo_small_bt-r10.dtb \
	 exynos9110-tizen_galileo_small_bt-r11.dtb
endif
else
ifeq ($(CONFIG_GALILEO_LTE),y)
dtb-y += exynos9110-tizen_galileo_large_lte-r04.dtb \
         exynos9110-tizen_galileo_large_lte-r05.dtb \
         exynos9110-tizen_galileo_large_lte-r06.dtb \
         exynos9110-tizen_galileo_large_lte-r07.dtb \
         exynos9110-tizen_galileo_large_lte-r08.dtb \
         exynos9110-tizen_galileo_large_lte-r09.dtb \
         exynos9110-tizen_galileo_large_lte-r10.dtb \
	 exynos9110-tizen_galileo_large_lte-r11.dtb
else
dtb-y += exynos9110-tizen_galileo_large_bt-r06.dtb \
         exynos9110-tizen_galileo_large_bt-r07.dtb \
         exynos9110-tizen_galileo_large_bt-r08.dtb \
         exynos9110-tizen_galileo_large_bt-r09.dtb \
         exynos9110-tizen_galileo_large_bt-r10.dtb \
	 exynos9110-tizen_galileo_large_bt-r11.dtb
endif
endif
else ifeq ($(CONFIG_SOC_EXYNOS9110),y)
dtb-y += exynos9110-smdk9110.dtb \
         exynos9110-maestro9110.dtb \
         exynos9110-smdk9110-lowmem.dtb \
         exynos9110-maestro9110-lowmem.dtb \
         exynos9110-maestro9110-rev02.dtb
else
dtb-$(CONFIG_ARCH_EXYNOS) += exynos7-espresso.dtb
endif

dtbo-$(CONFIG_SOC_EXYNOS9810) += \
	exynos9810-universal9810.dtbo \
	exynos9810-universal9810-2.dtbo \
	exynos9810-universal9810-rev02.dtbo \
	exynos9810-universal9810-2-rev02.dtbo

always		:= $(dtb-y) $(dtbo-y)
subdir-y	:= $(dts-dirs)
clean-files	:= *.dtb *.dtbo
