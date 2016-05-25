################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/control/NetworkReporter.cpp 

OBJS += \
./src/control/NetworkReporter.o 

CPP_DEPS += \
./src/control/NetworkReporter.d 


# Each subdirectory must supply rules for building sources it contributes
src/control/%.o: ../src/control/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -D__cplusplus=201103L -I"/home/james/workspace/Beetle/include" -O0 -g3 -Wall -c -fmessage-length=0 -std=c++11 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


