#
# Makefile for the i2c core.
#

obj-$(CONFIG_I2C_BOARDINFO)	+= i2c-boardinfo.o
obj-$(CONFIG_I2C)		+= i2c-core.o
obj-$(CONFIG_I2C_CHARDEV)	+= i2c-dev.o
obj-y				+= busses/ chips/ algos/
obj-$(CONFIG_GPIO_I2C_BH1750FVI)	+= bh1750fvi.o

ifeq ($(CONFIG_I2C_DEBUG_CORE),y)
EXTRA_CFLAGS += -DDEBUG
endif
