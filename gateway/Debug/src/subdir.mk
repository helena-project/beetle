################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/Beetle.cpp \
../src/BeetleConfig.cpp \
../src/CLI.cpp \
../src/Device.cpp \
../src/HCI.cpp \
../src/Handle.cpp \
../src/Router.cpp \
../src/StaticTopo.cpp \
../src/UUID.cpp \
../src/main.cpp 

OBJS += \
./src/Beetle.o \
./src/BeetleConfig.o \
./src/CLI.o \
./src/Device.o \
./src/HCI.o \
./src/Handle.o \
./src/Router.o \
./src/StaticTopo.o \
./src/UUID.o \
./src/main.o 

CPP_DEPS += \
./src/Beetle.d \
./src/BeetleConfig.d \
./src/CLI.d \
./src/Device.d \
./src/HCI.d \
./src/Handle.d \
./src/Router.d \
./src/StaticTopo.d \
./src/UUID.d \
./src/main.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -std=c++1y -D__cplusplus=201402L -I../lib/include -I../include -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


