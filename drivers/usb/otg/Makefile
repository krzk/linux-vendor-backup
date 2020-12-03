#
# OTG infrastructure and transceiver drivers
#

# infrastructure
obj-$(CONFIG_USB_OTG_UTILS)	+= otg.o

obj-$(CONFIG_MICROUSBIC_INTR)	+= microusbic/
obj-$(CONFIG_FSA9480_MICROUSB)	+= fsa9480_i2c.o
#obj-$(CONFIG_MAX14577_MICROUSB)	+= MD_MAX14577.o
obj-$(CONFIG_MAX14577_MICROUSB)	+= MD_MAX14577.o battery_monitor.o charger_max14577.o fuelgauge_max17040.o sleep_i2c1.o sleep_i2c2.o sleep_madc.o 

# transceiver drivers
obj-$(CONFIG_USB_GPIO_VBUS)	+= gpio_vbus.o
obj-$(CONFIG_ISP1301_OMAP)	+= isp1301_omap.o
obj-$(CONFIG_TWL4030_USB)	+= twl4030-usb.o
obj-$(CONFIG_ISP1301_HOST)	+= isp1301_host.o
obj-$(CONFIG_NOP_USB_XCEIV)	+= nop-usb-xceiv.o
obj-$(CONFIG_CPCAP_USB)		+= cpcap-usb.o

ccflags-$(CONFIG_USB_DEBUG)	+= -DDEBUG
ccflags-$(CONFIG_USB_GADGET_DEBUG) += -DDEBUG

