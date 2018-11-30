#
# Makefile for nanohub
#

ccflags-$(CONFIG_CONTEXTHUB_DEBUG) := -DDEBUG
obj-$(CONFIG_NANOHUB) += nanohub.o
obj-$(CONFIG_NANOHUB) := main.o comms.o
obj-$(CONFIG_NANOHUB_SPI) += spi.o bl.o
obj-$(CONFIG_NANOHUB_I2C) += i2c.o bl.o
obj-$(CONFIG_CONTEXTHUB_DRV) += chub_dev.o
obj-$(CONFIG_CONTEXTHUB_IPC) += chub.o chub_ipc.o chub_log.o chub_dbg.o
