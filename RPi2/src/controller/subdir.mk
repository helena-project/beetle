################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/controller/AccessControl.cpp \
../src/controller/ControllerClient.cpp \
../src/controller/NetworkDiscovery.cpp \
../src/controller/NetworkState.cpp 

OBJS += \
./src/controller/AccessControl.o \
./src/controller/ControllerClient.o \
./src/controller/NetworkDiscovery.o \
./src/controller/NetworkState.o 

CPP_DEPS += \
./src/controller/AccessControl.d \
./src/controller/ControllerClient.d \
./src/controller/NetworkDiscovery.d \
./src/controller/NetworkState.d 


# Each subdirectory must supply rules for building sources it contributes
src/controller/%.o: ../src/controller/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -std=c++1y -D__cplusplus=201402L -I../lib/include -I../include -O1 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


