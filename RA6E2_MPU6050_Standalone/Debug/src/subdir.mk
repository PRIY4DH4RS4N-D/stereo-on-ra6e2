################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/hal_entry.c \
../src/hal_warmstart.c \
../src/i2c_bus.c \
../src/i2c_hw_iic0.c \
../src/i2c_hw_sci9.c \
../src/mpu6050.c \
../src/mpu_sensor.c \
../src/timer_measurement.c \
../src/uart_logger.c 

C_DEPS += \
./src/hal_entry.d \
./src/hal_warmstart.d \
./src/i2c_bus.d \
./src/i2c_hw_iic0.d \
./src/i2c_hw_sci9.d \
./src/mpu6050.d \
./src/mpu_sensor.d \
./src/timer_measurement.d \
./src/uart_logger.d 

OBJS += \
./src/hal_entry.o \
./src/hal_warmstart.o \
./src/i2c_bus.o \
./src/i2c_hw_iic0.o \
./src/i2c_hw_sci9.o \
./src/mpu6050.o \
./src/mpu_sensor.o \
./src/timer_measurement.o \
./src/uart_logger.o 

SREC += \
RA6E2_MPU6050_Standalone.srec 

MAP += \
RA6E2_MPU6050_Standalone.map 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.c
	$(file > $@.in,-mcpu=cortex-m33 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16 -O2 -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -fno-strict-aliasing -Wunused -Wuninitialized -Wall -Wextra -Wmissing-declarations -Wconversion -Wpointer-arith -Wshadow -Wlogical-op -Waggregate-return -Wfloat-equal -g -D_RENESAS_RA_ -D_RA_CORE=CM33 -D_RA_ORDINAL=1 -I"C:/Renesas_Final/RA6E2_MPU6050_Standalone/ra_gen" -I"." -I"C:/Renesas_Final/RA6E2_MPU6050_Standalone/ra_cfg/fsp_cfg/bsp" -I"C:/Renesas_Final/RA6E2_MPU6050_Standalone/ra_cfg/fsp_cfg" -I"C:/Renesas_Final/RA6E2_MPU6050_Standalone/src" -I"C:/Renesas_Final/RA6E2_MPU6050_Standalone/ra/fsp/inc" -I"C:/Renesas_Final/RA6E2_MPU6050_Standalone/ra/fsp/inc/api" -I"C:/Renesas_Final/RA6E2_MPU6050_Standalone/ra/fsp/inc/instances" -I"C:/Renesas_Final/RA6E2_MPU6050_Standalone/ra/arm/CMSIS_6/CMSIS/Core/Include" -std=c99 -Wno-stringop-overflow -Wno-format-truncation --param=min-pagesize=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -c -o "$@" -x c "$<")
	@echo Building file: $< && arm-none-eabi-gcc @"$@.in"
