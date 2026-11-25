################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../ra/board/ra6e2_bgk/board_init.c \
../ra/board/ra6e2_bgk/board_leds.c 

C_DEPS += \
./ra/board/ra6e2_bgk/board_init.d \
./ra/board/ra6e2_bgk/board_leds.d 

OBJS += \
./ra/board/ra6e2_bgk/board_init.o \
./ra/board/ra6e2_bgk/board_leds.o 

SREC += \
ADAS_Stereo.srec 

MAP += \
ADAS_Stereo.map 


# Each subdirectory must supply rules for building sources it contributes
ra/board/ra6e2_bgk/%.o: ../ra/board/ra6e2_bgk/%.c
	$(file > $@.in,-mcpu=cortex-m33 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16 -O2 -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -fno-strict-aliasing -Wunused -Wuninitialized -Wall -Wextra -Wmissing-declarations -Wconversion -Wpointer-arith -Wshadow -Wlogical-op -Waggregate-return -Wfloat-equal -g -D_RENESAS_RA_ -D_RA_CORE=CM33 -D_RA_ORDINAL=1 -I"C:/Stereo_ADAS/ra_gen" -I"." -I"C:/Stereo_ADAS/ra_cfg/fsp_cfg/bsp" -I"C:/Stereo_ADAS/ra_cfg/fsp_cfg" -I"C:/Stereo_ADAS/src" -I"C:/Stereo_ADAS/ra/fsp/inc" -I"C:/Stereo_ADAS/ra/fsp/inc/api" -I"C:/Stereo_ADAS/ra/fsp/inc/instances" -I"C:/Stereo_ADAS/ra/arm/CMSIS_6/CMSIS/Core/Include" -std=c99 -Wno-stringop-overflow -Wno-format-truncation --param=min-pagesize=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -c -o "$@" -x c "$<")
	@echo Building file: $< && arm-none-eabi-gcc @"$@.in"

