################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../Logger.cpp \
../MQTT.cpp \
../RFReceiver.cpp \
../SensorsData.cpp \
../Utils.cpp \
../f007th_send.cpp 

OBJS += \
./Logger.o \
./MQTT.o \
./RFReceiver.o \
./SensorsData.o \
./Utils.o \
./f007th_send.o 

CPP_DEPS += \
./Logger.d \
./MQTT.d \
./RFReceiver.d \
./SensorsData.d \
./Utils.d \
./f007th_send.d 


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: Cross G++ Compiler'
	C:\Windows\Sysnative\wsl.exe g++ -DTEST_DECODING -DINCLUDE_HTTPD -DINCLUDE_MQTT -DRPI -O0 -g3 -Wall -c -fmessage-length=0 -pthread -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


