#
# Makefile for the sensor drivers.
#

# Each configuration option enables a list of files.
obj-$(CONFIG_SENSORS_SSP)		+= ssp_dev.o ssp_i2c.o ssp_data.o ssp_sysfs.o \
						ssp_input.o ssp_firmware.o ssp_debug.o \
						ssp_iio_ring.o ssp_iio_trigger.o

obj-$(CONFIG_SENSORS_SSP_ICM20610)	+= factory/accel_icm20610.o factory/gyro_icm20610.o
obj-$(CONFIG_SENSORS_SSP_YAS532)	+= factory/magnetic_yas532.o
obj-$(CONFIG_SENSORS_SSP_TMG399X)	+= factory/light_tmg399x.o factory/prox_tmg399x.o \
						factory/gesture_tmg399x.o
obj-$(CONFIG_SENSORS_SSP_BMP182)	+= factory/pressure_bmp182.o
obj-$(CONFIG_SENSORS_SSP_MOBEAM)	+= factory/barcode_emul_tmg3992.o
obj-$(CONFIG_SENSORS_SSP_SENSORHUB)	+= ssp_sensorhub.o ssp_misc.o
obj-$(CONFIG_SENSORS_SSP_BBD)		+= ssp_bbd.o factory/mcu_bcm4773.o
obj-$(CONFIG_SENSORS_SSP_BBD_VER1)	+= bbdpl1/
obj-$(CONFIG_SENSORS_SSP_BBD_VER2)	+= bbdpl2/
obj-$(CONFIG_GPS_BCM47531)		+= gps/sec_gps_bcm47531.o
