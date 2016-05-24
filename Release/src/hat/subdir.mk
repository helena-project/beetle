################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/hat/BlockAllocator.cpp \
../src/hat/SingleAllocator.cpp 

OBJS += \
./src/hat/BlockAllocator.o \
./src/hat/SingleAllocator.o 

CPP_DEPS += \
./src/hat/BlockAllocator.d \
./src/hat/SingleAllocator.d 


# Each subdirectory must supply rules for building sources it contributes
src/hat/%.o: ../src/hat/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -std=c++1y -D__cplusplus=201402L -I../lib/include -I../include -O3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


