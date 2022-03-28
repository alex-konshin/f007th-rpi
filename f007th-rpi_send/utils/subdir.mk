################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../utils/HTTPD.cpp \
../utils/Logger.cpp \
../utils/Utils.cpp 

CPP_DEPS += \
./utils/HTTPD.d \
./utils/Logger.d \
./utils/Utils.d 

OBJS += \
./utils/HTTPD.o \
./utils/Logger.o \
./utils/Utils.o 


# Each subdirectory must supply rules for building sources it contributes
utils/%.o: ../utils/%.cpp utils/subdir.mk
	@echo 'Building file: $<'
	@echo 'Invoking: Cross G++ Compiler'
	g++ -std=c++11 -DRPI -DNDEBUG -DINCLUDE_HTTPD -DINCLUDE_POLLSTER -O2 -Wall -c -fpermissive  -fmessage-length=0 -pthread -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


clean: clean-utils

clean-utils:
	-$(RM) ./utils/HTTPD.d ./utils/HTTPD.o ./utils/Logger.d ./utils/Logger.o ./utils/Utils.d ./utils/Utils.o

.PHONY: clean-utils

