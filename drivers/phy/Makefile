#
# Makefile for the phy drivers.
#

obj-$(CONFIG_GENERIC_PHY)		+= phy-core.o
obj-$(CONFIG_PHY_EXYNOS_MIPI_VIDEO)	+= phy-exynos-mipi-video.o
obj-$(CONFIG_EXYNOS_PHY)	+= exynos-phy.o
obj-$(CONFIG_PHY_SAMSUNG_USB2)		+= phy-exynos-usb2.o
phy-exynos-usb2-y			+= phy-samsung-usb2.o
phy-exynos-usb2-$(CONFIG_PHY_EXYNOS4210_USB2)	+= phy-exynos4210-usb2.o
phy-exynos-usb2-$(CONFIG_PHY_EXYNOS4X12_USB2)	+= phy-exynos4x12-usb2.o
