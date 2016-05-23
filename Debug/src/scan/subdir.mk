################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/scan/AutoConnect.cpp \
../src/scan/Scanner.cpp 

OBJS += \
./src/scan/AutoConnect.o \
./src/scan/Scanner.o 

CPP_DEPS += \
./src/scan/AutoConnect.d \
./src/scan/Scanner.d 


# Each subdirectory must supply rules for building sources it contributes
src/scan/%.o: ../src/scan/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -std=c++1y -D__cplusplus=201402L -I../lib/include -I../include -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


