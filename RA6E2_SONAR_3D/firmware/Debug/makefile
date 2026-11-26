################################################################################
# Automatically-generated file. Do not edit!
################################################################################

-include ../makefile.init

-include makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include src/subdir.mk
-include ra_gen/subdir.mk
-include ra/fsp/src/r_ioport/subdir.mk
-include ra/fsp/src/bsp/mcu/ra6e2/subdir.mk
-include ra/fsp/src/bsp/mcu/all/subdir.mk
-include ra/fsp/src/bsp/cmsis/Device/RENESAS/Source/subdir.mk
-include ra/board/ra6e2_bgk/subdir.mk
-include subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(JMP_DEPS)),)
-include $(JMP_DEPS)
endif
ifneq ($(strip $(FSY_DEPS)),)
-include $(FSY_DEPS)
endif
ifneq ($(strip $(C_UPPER_DEPS)),)
-include $(C_UPPER_DEPS)
endif
ifneq ($(strip $(SRC_UPPER_DEPS)),)
-include $(SRC_UPPER_DEPS)
endif
ifneq ($(strip $(P_UPPER_DEPS)),)
-include $(P_UPPER_DEPS)
endif
ifneq ($(strip $(S_DEPS)),)
-include $(S_DEPS)
endif
ifneq ($(strip $(PP_UPPER_DEPS)),)
-include $(PP_UPPER_DEPS)
endif
ifneq ($(strip $(P_DEPS)),)
-include $(P_DEPS)
endif
ifneq ($(strip $(FSY_UPPER_DEPS)),)
-include $(FSY_UPPER_DEPS)
endif
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
ifneq ($(strip $(SRC_DEPS)),)
-include $(SRC_DEPS)
endif
ifneq ($(strip $(JMP_UPPER_DEPS)),)
-include $(JMP_UPPER_DEPS)
endif
ifneq ($(strip $(PP_DEPS)),)
-include $(PP_DEPS)
endif
ifneq ($(strip $(SX_DEPS)),)
-include $(SX_DEPS)
endif
ifneq ($(strip $(ASM_UPPER_DEPS)),)
-include $(ASM_UPPER_DEPS)
endif
ifneq ($(strip $(SX_UPPER_DEPS)),)
-include $(SX_UPPER_DEPS)
endif
ifneq ($(strip $(ASM_DEPS)),)
-include $(ASM_DEPS)
endif
ifneq ($(strip $(S_UPPER_DEPS)),)
-include $(S_UPPER_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 
SECONDARY_FLASH += \
RA6E2_SONAR_3D.srec \
RA6E2_SONAR_3D.hex \

SECONDARY_SIZE += \
RA6E2_SONAR_3D.siz \


# All Target
all: RA6E2_SONAR_3D.elf secondary-outputs

# Tool invocations
RA6E2_SONAR_3D.elf: $(OBJS) $(USER_OBJS) $(LINKER_SCRIPT)
	$(file > RA6E2_SONAR_3D.elf.in,-mcpu=cortex-m33 -mthumb -mfloat-abi=hard -mfpu=fpv5-sp-d16 -O2 -fmessage-length=0 -fsigned-char -ffunction-sections -fdata-sections -fno-strict-aliasing -Wunused -Wuninitialized -Wall -Wextra -Wmissing-declarations -Wconversion -Wpointer-arith -Wshadow -Wlogical-op -Waggregate-return -Wfloat-equal -g -T "fsp.ld" -Xlinker --gc-sections -L "C:\\Renesas_Final\\RA6E2_SONAR_3D\\firmware/script" -Wl,-Map,"RA6E2_SONAR_3D.map" --specs=nano.specs -o "RA6E2_SONAR_3D.elf" -Wl,--start-group $(OBJS) $(USER_OBJS) $(LIBS) -lm -Wl,--end-group)
	@echo Building target: $@ && arm-none-eabi-gcc @"RA6E2_SONAR_3D.elf.in"
RA6E2_SONAR_3D.srec: RA6E2_SONAR_3D.elf
	arm-none-eabi-objcopy -O srec "RA6E2_SONAR_3D.elf" "RA6E2_SONAR_3D.srec"
RA6E2_SONAR_3D.hex: RA6E2_SONAR_3D.elf
	arm-none-eabi-objcopy -O ihex "RA6E2_SONAR_3D.elf" "RA6E2_SONAR_3D.hex"
RA6E2_SONAR_3D.siz: RA6E2_SONAR_3D.elf
	arm-none-eabi-size --format=berkeley "RA6E2_SONAR_3D.elf"
# Other Targets
clean:
	-$(RM) $(OBJS) $(C_DEPS) RA6E2_SONAR_3D.elf RA6E2_SONAR_3D.srec RA6E2_SONAR_3D.hex RA6E2_SONAR_3D.map RA6E2_SONAR_3D.siz
	-@echo ' '

secondary-outputs: $(SECONDARY_FLASH) $(SECONDARY_SIZE)

.PHONY: all clean dependents

-include ../makefile.targets
