################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../F007TH.cpp \
../Logger.cpp \
../RFReceiver.cpp 

OBJS += \
./F007TH.o \
./Logger.o \
./RFReceiver.o 

CPP_DEPS += \
./F007TH.d \
./Logger.d \
./RFReceiver.d 


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: Cross G++ Compiler'
	arm-linux-gnueabihf-g++ -DRPi -DNDEBUG -O2 -Wall -c -fmessage-length=0 -pthread -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


