################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../f007th_send.cpp 

OBJS += \
./f007th_send.o 

CPP_DEPS += \
./f007th_send.d 


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: Cross G++ Compiler'
	C:\Windows\Sysnative\wsl.exe g++ -std=c++0x -std=c++11 -DTEST_DECODING -DINCLUDE_HTTPD -DINCLUDE_MQTT -DRPI -DINCLUDE_POLLSTER -O0 -g3 -Wall -c -fmessage-length=0 -pthread -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


