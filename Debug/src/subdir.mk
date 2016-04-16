################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/Beetle.cpp \
../src/CLI.cpp \
../src/Device.cpp \
../src/Handle.cpp \
../src/Router.cpp \
../src/Scanner.cpp \
../src/UUID.cpp 

OBJS += \
./src/Beetle.o \
./src/CLI.o \
./src/Device.o \
./src/Handle.o \
./src/Router.o \
./src/Scanner.o \
./src/UUID.o 

CPP_DEPS += \
./src/Beetle.d \
./src/CLI.d \
./src/Device.d \
./src/Handle.d \
./src/Router.d \
./src/Scanner.d \
./src/UUID.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o: ../src/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -D__cplusplus=201103L -O0 -g3 -Wall -c -fmessage-length=0 -std=c++11 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


