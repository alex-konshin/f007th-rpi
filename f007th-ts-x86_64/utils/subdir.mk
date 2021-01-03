################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../utils/HTTPD.cpp \
../utils/Logger.cpp \
../utils/Utils.cpp 

OBJS += \
./utils/HTTPD.o \
./utils/Logger.o \
./utils/Utils.o 

CPP_DEPS += \
./utils/HTTPD.d \
./utils/Logger.d \
./utils/Utils.d 


# Each subdirectory must supply rules for building sources it contributes
utils/%.o: ../utils/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: Cross G++ Compiler'
	g++ -std=c++11 -DINCLUDE_HTTPD -DINCLUDE_POLLSTER -DUSE_GPIO_TS -O1 -Wall -c -fmessage-length=0 -pthread -U_FORTIFY_SOURCE -fno-stack-protector -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


