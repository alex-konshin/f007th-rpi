################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../Logger.cpp \
../RFReceiver.cpp \
../f007th_send.cpp 

OBJS += \
./Logger.o \
./RFReceiver.o \
./f007th_send.o 

CPP_DEPS += \
./Logger.d \
./RFReceiver.d \
./f007th_send.d 


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: Cross G++ Compiler'
	g++ -DTEST_DECODING -DINCLUDE_HTTPD -DRPI -O2 -Wall -c -fmessage-length=0 -pthread -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


