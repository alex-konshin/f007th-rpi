################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../Logger.cpp \
../RFReceiver.cpp \
../SensorsData.cpp \
../f007th_send.cpp 

OBJS += \
./Logger.o \
./RFReceiver.o \
./SensorsData.o \
./f007th_send.o 

CPP_DEPS += \
./Logger.d \
./RFReceiver.d \
./SensorsData.d \
./f007th_send.d 


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: Cross G++ Compiler'
	arm-linux-gnueabihf-g++ -std=c++11 -DRPi -DNDEBUG -DINCLUDE_HTTPD -O0 -Wall -c -fmessage-length=0 -pthread -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


