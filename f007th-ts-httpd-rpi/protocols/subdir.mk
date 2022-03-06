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
../protocols/Nexus.cpp \
../protocols/Protocol.cpp \
../protocols/TFATwinPlus.cpp \
../protocols/WH2.cpp 

CPP_DEPS += \
./protocols/AcuRite00592TXR.d \
./protocols/AmbientWeatherF007TH.d \
./protocols/AuriolHG02832.d \
./protocols/DS18B20.d \
./protocols/LaCrosseTX7.d \
./protocols/Nexus.d \
./protocols/Protocol.d \
./protocols/TFATwinPlus.d \
./protocols/WH2.d 

OBJS += \
./protocols/AcuRite00592TXR.o \
./protocols/AmbientWeatherF007TH.o \
./protocols/AuriolHG02832.o \
./protocols/DS18B20.o \
./protocols/LaCrosseTX7.o \
./protocols/Nexus.o \
./protocols/Protocol.o \
./protocols/TFATwinPlus.o \
./protocols/WH2.o 


# Each subdirectory must supply rules for building sources it contributes
protocols/%.o: ../protocols/%.cpp protocols/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: Cross G++ Compiler'
	g++ -std=c++11 -DUSE_GPIO_TS -DNDEBUG -DRPI -DINCLUDE_HTTPD -DINCLUDE_POLLSTER -O2 -Wall -c -fpermissive -fmessage-length=0 -pthread -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


clean: clean-protocols

clean-protocols:
	-$(RM) ./protocols/AcuRite00592TXR.d ./protocols/AcuRite00592TXR.o ./protocols/AmbientWeatherF007TH.d ./protocols/AmbientWeatherF007TH.o ./protocols/AuriolHG02832.d ./protocols/AuriolHG02832.o ./protocols/DS18B20.d ./protocols/DS18B20.o ./protocols/LaCrosseTX7.d ./protocols/LaCrosseTX7.o ./protocols/Nexus.d ./protocols/Nexus.o ./protocols/Protocol.d ./protocols/Protocol.o ./protocols/TFATwinPlus.d ./protocols/TFATwinPlus.o ./protocols/WH2.d ./protocols/WH2.o

.PHONY: clean-protocols

