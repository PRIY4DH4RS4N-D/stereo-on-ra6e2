################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../ra/fsp/src/bsp/mcu/ra6e2/bsp_linker.c 

C_DEPS += \
./ra/fsp/src/bsp/mcu/ra6e2/bsp_linker.d 

OBJS += \
./ra/fsp/src/bsp/mcu/ra6e2/bsp_linker.o 

SREC += \
SD_Card_tesr.srec 

MAP += \
SD_Card_tesr.map 


# Each subdirectory must supply rules for building sources it contributes
ra/fsp/src/bsp/mcu/ra6e2/%.o: ../ra/fsp/src/bsp/mcu/ra6e2/%.c
	$(file > $@.in,-mcpu=cortex-m33 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16 -O2 -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -fno-strict-aliasing -Wunused -Wuninitialized -Wall -Wextra -Wmissing-declarations -Wconversion -Wpointer-arith -Wshadow -Wlogical-op -Waggregate-return -Wfloat-equal -g -D_RENESAS_RA_ -D_RA_CORE=CM33 -D_RA_ORDINAL=1 -I"C:/Renesas_Final/RA6E2_Ultrasonic_HCSR04/ra_gen" -I"." -I"C:/Renesas_Final/RA6E2_Ultrasonic_HCSR04/ra_cfg/fsp_cfg/bsp" -I"C:/Renesas_Final/RA6E2_Ultrasonic_HCSR04/ra_cfg/fsp_cfg" -I"C:/Renesas_Final/RA6E2_Ultrasonic_HCSR04/src" -I"C:/Renesas_Final/RA6E2_Ultrasonic_HCSR04/ra/fsp/inc" -I"C:/Renesas_Final/RA6E2_Ultrasonic_HCSR04/ra/fsp/inc/api" -I"C:/Renesas_Final/RA6E2_Ultrasonic_HCSR04/ra/fsp/inc/instances" -I"C:/Renesas_Final/RA6E2_Ultrasonic_HCSR04/ra/arm/CMSIS_6/CMSIS/Core/Include" -std=c99 -Wno-stringop-overflow -Wno-format-truncation --param=min-pagesize=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -c -o "$@" -x c "$<")
	@echo Building file: $< && arm-none-eabi-gcc @"$@.in"

