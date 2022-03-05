################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../common/Config.cpp \
../common/ConfigParser.cpp \
../common/Receiver.cpp \
../common/SensorsData.cpp 

CPP_DEPS += \
./common/Config.d \
./common/ConfigParser.d \
./common/Receiver.d \
./common/SensorsData.d 

OBJS += \
./common/Config.o \
./common/ConfigParser.o \
./common/Receiver.o \
./common/SensorsData.o 


# Each subdirectory must supply rules for building sources it contributes
common/%.o: ../common/%.cpp common/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: Cross G++ Compiler'
	C:\Windows\Sysnative\wsl.exe g++ -std=c++0x -std=c++11 -DTEST_DECODING -DINCLUDE_HTTPD -DINCLUDE_MQTT -DRPI -DINCLUDE_POLLSTER -O0 -g3 -Wall -c -fmessage-length=0 -pthread -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


clean: clean-common

clean-common:
	-$(RM) ./common/Config.d ./common/Config.o ./common/ConfigParser.d ./common/ConfigParser.o ./common/Receiver.d ./common/Receiver.o ./common/SensorsData.d ./common/SensorsData.o

.PHONY: clean-common

