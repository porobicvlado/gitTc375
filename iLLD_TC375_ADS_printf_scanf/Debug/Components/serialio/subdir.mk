################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
"../Components/serialio/serialio.c" 

COMPILED_SRCS += \
"Components/serialio/serialio.src" 

C_DEPS += \
"./Components/serialio/serialio.d" 

OBJS += \
"Components/serialio/serialio.o" 


# Each subdirectory must supply rules for building sources it contributes
"Components/serialio/serialio.src":"../Components/serialio/serialio.c" "Components/serialio/subdir.mk"
	cctc -cs --dep-file="$*.d" --misrac-version=2012 -D__CPU__=tc37x "-fC:/Nastava_ESUV/35_Aurix/05_sw_projekti/gitzAllExamplesAurix/AURIX_code_examples/code_examples/iLLD_TC375_ADS_printf_scanf/Debug/TASKING_C_C___Compiler-Include_paths__-I_.opt" --iso=99 --c++14 --language=+volatile --exceptions --anachronisms --fp-model=3 -O0 --tradeoff=4 --compact-max-size=200 -g -Wc-w544 -Wc-w557 -Ctc37x -Y0 -N0 -Z0 -o "$@" "$<"
"Components/serialio/serialio.o":"Components/serialio/serialio.src" "Components/serialio/subdir.mk"
	astc -Og -Os --no-warnings= --error-limit=42 -o  "$@" "$<"

clean: clean-Components-2f-serialio

clean-Components-2f-serialio:
	-$(RM) ./Components/serialio/serialio.d ./Components/serialio/serialio.o ./Components/serialio/serialio.src

.PHONY: clean-Components-2f-serialio

