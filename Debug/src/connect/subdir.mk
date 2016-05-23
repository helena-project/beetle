################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/connect/AutoConnect.cpp 

OBJS += \
./src/connect/AutoConnect.o 

CPP_DEPS += \
./src/connect/AutoConnect.d 


# Each subdirectory must supply rules for building sources it contributes
src/connect/%.o: ../src/connect/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -D__cplusplus=201103L -O0 -g3 -Wall -c -fmessage-length=0 -std=c++11 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


