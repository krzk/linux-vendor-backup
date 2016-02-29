#
## Makefile for the TUI_Module
#
## add TUI_Module

ifeq ($(CONFIG_SOC_EXYNOS3250),y)
PLATFORM := exynos3250
endif

obj-y += TlcTui.o

TlcTui-objs := main.o \
	tlcTui.o \
	trustedui.o \
	platforms/$(PLATFORM)/tui-hal.o

# Release mode by default
ccflags-y := -DNDEBUG
ccflags-y += -Wno-declaration-after-statement

ccflags-y += -I$(KERNEL_API_MODULE) -I$(M) -I$(M)/inc
ccflags-y += -Idrivers/gud/gud-$(PLATFORM)/MobiCoreKernelApi/public
ccflags-y += -Idrivers/gud/gud-$(PLATFORM)/MobiCoreKernelApi/include
ccflags-y += -Idrivers/gud/TlcTui/
ccflags-y += -Idrivers/gud/TlcTui/inc
ccflags-y += -Idrivers/gud/TlcTui/public
