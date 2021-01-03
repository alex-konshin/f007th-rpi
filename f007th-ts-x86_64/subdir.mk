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
	g++ -std=c++11 -DINCLUDE_HTTPD -DNDEBUG -DINCLUDE_POLLSTER -DUSE_GPIO_TS -O2 -Wall -c -fmessage-length=0 -pthread -U_FORTIFY_SOURCE -fno-stack-protector -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


