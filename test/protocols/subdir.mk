################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../protocols/AcuRite00592TXR.cpp \
../protocols/AmbientWeatherF007TH.cpp \
../protocols/AuriolHG02832.cpp \
../protocols/DS18B20.cpp \
../protocols/LaCrosseTX7.cpp \
../protocols/Protocol.cpp \
../protocols/WH2.cpp 

OBJS += \
./protocols/AcuRite00592TXR.o \
./protocols/AmbientWeatherF007TH.o \
./protocols/AuriolHG02832.o \
./protocols/DS18B20.o \
./protocols/LaCrosseTX7.o \
./protocols/Protocol.o \
./protocols/WH2.o 

CPP_DEPS += \
./protocols/AcuRite00592TXR.d \
./protocols/AmbientWeatherF007TH.d \
./protocols/AuriolHG02832.d \
./protocols/DS18B20.d \
./protocols/LaCrosseTX7.d \
./protocols/Protocol.d \
./protocols/WH2.d 


# Each subdirectory must supply rules for building sources it contributes
protocols/%.o: ../protocols/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: Cross G++ Compiler'
	arm-linux-gnueabihf-g++ -std=c++0x -std=c++11 -DTEST_DECODING -DINCLUDE_HTTPD -DRPI -DINCLUDE_MQTT -DINCLUDE_POLLSTER -O2 -Wall -c -fmessage-length=0 -pthread -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


