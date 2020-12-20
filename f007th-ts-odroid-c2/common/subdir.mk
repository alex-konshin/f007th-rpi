################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../common/RFReceiver.cpp \
../common/SensorsData.cpp 

OBJS += \
./common/RFReceiver.o \
./common/SensorsData.o 

CPP_DEPS += \
./common/RFReceiver.d \
./common/SensorsData.d 


# Each subdirectory must supply rules for building sources it contributes
common/%.o: ../common/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: Cross G++ Compiler'
	aarch64-linux-gnu-g++ -std=c++11 -DODROIDC2 -DNDEBUG -DUSE_GPIO_TS -O2 -Wall -c -fmessage-length=0 -pthread -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


