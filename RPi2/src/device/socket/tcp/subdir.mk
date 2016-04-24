################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/device/socket/tcp/TCPClientProxy.cpp \
../src/device/socket/tcp/TCPServerProxy.cpp 

OBJS += \
./src/device/socket/tcp/TCPClientProxy.o \
./src/device/socket/tcp/TCPServerProxy.o 

CPP_DEPS += \
./src/device/socket/tcp/TCPClientProxy.d \
./src/device/socket/tcp/TCPServerProxy.d 


# Each subdirectory must supply rules for building sources it contributes
src/device/socket/tcp/%.o: ../src/device/socket/tcp/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -std=c++1y -D__cplusplus=201402L -I../lib/include -I../include -O1 -Wall -c -fmessage-length=0 -m32 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


