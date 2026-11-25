################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../ra/fsp/src/r_ioport/r_ioport.c 

C_DEPS += \
./ra/fsp/src/r_ioport/r_ioport.d 

OBJS += \
./ra/fsp/src/r_ioport/r_ioport.o 

SREC += \
BGK_SD_Check.srec 

MAP += \
BGK_SD_Check.map 


# Each subdirectory must supply rules for building sources it contributes
ra/fsp/src/r_ioport/%.o: ../ra/fsp/src/r_ioport/%.c
	$(file > $@.in,-mcpu=cortex-m33 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16 -O2 -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -fno-strict-aliasing -Wunused -Wuninitialized -Wall -Wextra -Wmissing-declarations -Wconversion -Wpointer-arith -Wshadow -Wlogical-op -Waggregate-return -Wfloat-equal -g -D_RENESAS_RA_ -D_RA_CORE=CM33 -D_RA_ORDINAL=1 -I"C:/Stereo_ADAS/BGK_SD_Check/ra_gen" -I"." -I"C:/Stereo_ADAS/BGK_SD_Check/ra_cfg/fsp_cfg/bsp" -I"C:/Stereo_ADAS/BGK_SD_Check/ra_cfg/fsp_cfg" -I"C:/Stereo_ADAS/BGK_SD_Check/src" -I"C:/Stereo_ADAS/BGK_SD_Check/ra/fsp/inc" -I"C:/Stereo_ADAS/BGK_SD_Check/ra/fsp/inc/api" -I"C:/Stereo_ADAS/BGK_SD_Check/ra/fsp/inc/instances" -I"C:/Stereo_ADAS/BGK_SD_Check/ra/arm/CMSIS_6/CMSIS/Core/Include" -std=c99 -Wno-stringop-overflow -Wno-format-truncation --param=min-pagesize=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -c -o "$@" -x c "$<")
	@echo Building file: $< && arm-none-eabi-gcc @"$@.in"

