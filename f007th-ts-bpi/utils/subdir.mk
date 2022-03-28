################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../utils/Logger.cpp \
../utils/Utils.cpp 

CPP_DEPS += \
./utils/Logger.d \
./utils/Utils.d 

OBJS += \
./utils/Logger.o \
./utils/Utils.o 


# Each subdirectory must supply rules for building sources it contributes
utils/%.o: ../utils/%.cpp utils/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: Cross G++ Compiler'
	arm-linux-gnueabihf-g++ -std=c++11 -DBPiM3 -DNDEBUG -DUSE_GPIO_TS -O2 -Wall -c -fmessage-length=0 -pthread -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


clean: clean-utils

clean-utils:
	-$(RM) ./utils/Logger.d ./utils/Logger.o ./utils/Utils.d ./utils/Utils.o

.PHONY: clean-utils

