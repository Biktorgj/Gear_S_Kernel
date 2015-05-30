#
# Makefile for the sensor drivers.
#

# Each configuration option enables a list of files.
obj-$(CONFIG_SENSORS_SSP)		+= ssp_dev.o ssp_spi.o ssp_data.o ssp_sysfs.o\
						ssp_input.o ssp_firmware.o ssp_debug.o
obj-$(CONFIG_SENSORS_SYSFS)		+= sensors_core.o
obj-$(CONFIG_SENSORS_SSP_MPU6500)	+= factory/accel_mpu6500.o factory/gyro_mpu6500.o
obj-$(CONFIG_SENSORS_SSP_MPU9250)	+= factory/accel_mpu9250.o factory/gyro_mpu9250.o factory/magnetic_mpu9250.o
obj-$(CONFIG_SENSORS_SSP_ICM20628)	+= factory/accel_icm20628.o factory/gyro_icm20628.o
obj-$(CONFIG_SENSORS_SSP_AK09911)	+= factory/magnetic_ak09911.o
obj-$(CONFIG_SENSORS_SSP_LPS25H)	+= factory/pressure_lps25h.o
obj-$(CONFIG_SENSORS_SSP_AL3320)	+= factory/light_al3320.o
obj-$(CONFIG_SENSORS_SSP_ADPD142)	+= factory/hrm_adpd142.o
obj-$(CONFIG_SENSORS_SSP_UVIS25)	+= factory/uv_uvis25.o
obj-$(CONFIG_SENSORS_SSP_STM32F401)	+= factory/mcu_stm32f401.o
obj-$(CONFIG_SENSORS_SSP_SENSORHUB)	+= ssp_sensorhub.o
