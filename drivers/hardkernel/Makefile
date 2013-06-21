#ODROID U2 Fan
obj-$(CONFIG_ODROID_U2_FAN)		+= odroidu2_fan.o

#ODROID UMP Glue
obj-$(CONFIG_ODROID_UMP)	+= odroid_ump.o
ifeq ($(CONFIG_ODROID_UMP),y)
        EXTRA_CFLAGS += -Idrivers/gpu/arm/ump \
                        -Idrivers/gpu/arm/mali/common \
                        -Idrivers/gpu/arm/mali/linux
endif
