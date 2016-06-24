################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/device/socket/IPCApplication.cpp \
../src/device/socket/LEDevice.cpp \
../src/device/socket/SeqPacketConnection.cpp \
../src/device/socket/TCPConnection.cpp 

OBJS += \
./src/device/socket/IPCApplication.o \
./src/device/socket/LEDevice.o \
./src/device/socket/SeqPacketConnection.o \
./src/device/socket/TCPConnection.o 

CPP_DEPS += \
./src/device/socket/IPCApplication.d \
./src/device/socket/LEDevice.d \
./src/device/socket/SeqPacketConnection.d \
./src/device/socket/TCPConnection.d 


# Each subdirectory must supply rules for building sources it contributes
src/device/socket/%.o: ../src/device/socket/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -std=c++1y -D__cplusplus=201402L -I../lib/include -I../include -O3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


