################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../GstPlayerBase.cpp \
../GstVideoServer.cpp \
../main.cpp 

OBJS += \
./GstPlayerBase.o \
./GstVideoServer.o \
./main.o 

CPP_DEPS += \
./GstPlayerBase.d \
./GstVideoServer.d \
./main.d 


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -O3 -Wall -I/usr/include/gstreamermm-0.10 -I/usr/include/giomm-2.4 -I/usr/include/libxml2 -I/usr/include/libxml++-2.6 -I/usr/lib/libxml++-2.6/include -I/usr/lib/sigc++-2.0/include -I/usr/lib/glibmm-2.4/include -I/usr/lib/glib-2.0/include -I/usr/include/sigc++-2.0 -I/usr/include/gstreamer-0.10 -I/usr/include/glib-2.0 -I/usr/include/glibmm-2.4 -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


