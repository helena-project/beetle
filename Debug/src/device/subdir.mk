################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/device/BeetleMetaDevice.cpp \
../src/device/LEPeripheral.cpp \
../src/device/RemoteClientProxy.cpp \
../src/device/RemoteServerProxy.cpp \
../src/device/TCPConnection.cpp \
../src/device/VirtualDevice.cpp 

OBJS += \
./src/device/BeetleMetaDevice.o \
./src/device/LEPeripheral.o \
./src/device/RemoteClientProxy.o \
./src/device/RemoteServerProxy.o \
./src/device/TCPConnection.o \
./src/device/VirtualDevice.o 

CPP_DEPS += \
./src/device/BeetleMetaDevice.d \
./src/device/LEPeripheral.d \
./src/device/RemoteClientProxy.d \
./src/device/RemoteServerProxy.d \
./src/device/TCPConnection.d \
./src/device/VirtualDevice.d 


# Each subdirectory must supply rules for building sources it contributes
src/device/%.o: ../src/device/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -D__cplusplus=201103L -O0 -g3 -Wall -c -fmessage-length=0 -std=c++11 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


