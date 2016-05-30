################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/sync/OrderedThreadPool.cpp \
../src/sync/SocketSelect.cpp \
../src/sync/ThreadPool.cpp 

OBJS += \
./src/sync/OrderedThreadPool.o \
./src/sync/SocketSelect.o \
./src/sync/ThreadPool.o 

CPP_DEPS += \
./src/sync/OrderedThreadPool.d \
./src/sync/SocketSelect.d \
./src/sync/ThreadPool.d 


# Each subdirectory must supply rules for building sources it contributes
src/sync/%.o: ../src/sync/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -std=c++1y -D__cplusplus=201402L -I../lib/include -I../include -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


