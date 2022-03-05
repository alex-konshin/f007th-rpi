################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../utils/HTTPD.cpp \
../utils/Logger.cpp \
../utils/MQTT.cpp \
../utils/Utils.cpp 

CPP_DEPS += \
./utils/HTTPD.d \
./utils/Logger.d \
./utils/MQTT.d \
./utils/Utils.d 

OBJS += \
./utils/HTTPD.o \
./utils/Logger.o \
./utils/MQTT.o \
./utils/Utils.o 


# Each subdirectory must supply rules for building sources it contributes
utils/%.o: ../utils/%.cpp utils/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: Cross G++ Compiler'
	g++ -std=c++11 -DUSE_GPIO_TS -DNDEBUG -DRPI -DINCLUDE_HTTPD -DINCLUDE_MQTT -DINCLUDE_POLLSTER -O2 -Wall -c -fmessage-length=0 -pthread -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


clean: clean-utils

clean-utils:
	-$(RM) ./utils/HTTPD.d ./utils/HTTPD.o ./utils/Logger.d ./utils/Logger.o ./utils/MQTT.d ./utils/MQTT.o ./utils/Utils.d ./utils/Utils.o

.PHONY: clean-utils

