################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/controller/AccessControl.cpp \
../src/controller/ControllerCLI.cpp \
../src/controller/ControllerClient.cpp \
../src/controller/NetworkDiscoveryClient.cpp \
../src/controller/NetworkStateClient.cpp 

OBJS += \
./src/controller/AccessControl.o \
./src/controller/ControllerCLI.o \
./src/controller/ControllerClient.o \
./src/controller/NetworkDiscoveryClient.o \
./src/controller/NetworkStateClient.o 

CPP_DEPS += \
./src/controller/AccessControl.d \
./src/controller/ControllerCLI.d \
./src/controller/ControllerClient.d \
./src/controller/NetworkDiscoveryClient.d \
./src/controller/NetworkStateClient.d 


# Each subdirectory must supply rules for building sources it contributes
src/controller/%.o: ../src/controller/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -std=c++1y -D__cplusplus=201402L -I../lib/include -I../include -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


