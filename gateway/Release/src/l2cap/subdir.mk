################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/l2cap/L2CAPServer.cpp 

OBJS += \
./src/l2cap/L2CAPServer.o 

CPP_DEPS += \
./src/l2cap/L2CAPServer.d 


# Each subdirectory must supply rules for building sources it contributes
src/l2cap/%.o: ../src/l2cap/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -std=c++1y -D__cplusplus=201402L -I../lib/include -I../include -O3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


