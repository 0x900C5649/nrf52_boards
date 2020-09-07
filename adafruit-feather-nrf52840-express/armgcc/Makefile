#SETUP MAIN DIR
ROOT_DIR := ../../..
CONFIG_DIR := $(ROOT_DIR)/config

#INCLUDE common make
include $(CONFIG_DIR)/Makefile.common

#SET PROJECT VARIABLES
PROJECT_NAME	 	?= nrf52840
TARGETS		  		?= nrf52840_xxaa
OUTPUT_DIRECTORY 	?= $(PROJ_DIR)
VERSION		  		?= 1
BOARD			 	:= adafruit-feather-nrf52840-express
SD_REQ				?= 0x00
APPLICATION_NAME    ?= $(PROJECT_NAME)_$(VERSION)

#SETUP OTHER DIRS
SDK_ROOT ?= $(ROOT_DIR)/nrf_sdks/nRF5_SDK_16.0.0_98a08e2
PROJ_DIR ?= $(ROOT_DIR)/build/$(BOARD)
BOARD_DIRECTORY ?= $(ROOT_DIR)/boards/$(BOARD)

$(mkdir -p $(PROJ_DIR))	

SRC_DIR ?= $(ROOT_DIR)/src
INCL_DIR ?= $(SRC_DIR)

#directory for dependencies
LIB_DIR ?= $(ROOT_DIR)/lib

LD_SCRIPT ?= $(BOARD_DIRECTORY)/armgcc/nrf52_gcc.ld

$(OUTPUT_DIRECTORY)/nrf52840_xxaa.out: \
  LINKER_SCRIPT  := $(LD_SCRIPT)

# Source files common to all targets
SRC_FILES += \
  $(SDK_ROOT)/modules/nrfx/mdk/gcc_startup_nrf52840.S \
  $(SDK_ROOT)/components/boards/boards.c \
  $(SDK_ROOT)/modules/nrfx/mdk/system_nrf52.c \


# Include folders common to all targets
INC_FOLDERS += 	$(BOARD_DIRECTORY)/board_config \
			   	$(SDK_ROOT)/config/nrf52840/config \
			   	$(SDK_ROOT)/components/boards \
  				$(SDK_ROOT)/modules/nrfx/mdk \

# Libraries common to all targets
LIB_FILES += \

# DEFINITIONS
DEFINES += -DCUSTOM_BOARD_INC=$(BOARD).h \
		   -DFLOAT_ABI_HARD \
		   -DNRF52840_XXAA \
		   -DCONFIG_GPIO_AS_PINRESET \
		   -DBOARD=$(BOARD) \


# Optimization flags
OPT ?= -O0 -g3
#-O3 -g3
# Uncomment the line below to enable link time optimization
#OPT += -flto

# C flags common to all targets
CFLAGS += $(OPT)
CFLAGS += $(DEFINES)
CFLAGS += -mcpu=cortex-m4
CFLAGS += -mthumb -mabi=aapcs
CFLAGS += -Wall -Werror -Wno-unused-function
CFLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16
# keep every function in a separate section, this allows linker to discard unused ones
CFLAGS += -ffunction-sections -fdata-sections -fno-strict-aliasing
CFLAGS += -fno-builtin -fshort-enums

# C++ flags common to all targets
CXXFLAGS += $(OPT)
CXXFLAGS += $(DEFINES)

# Assembler flags common to all targets
ASMFLAGS += $(OPT)
ASMFLAGS += $(DEFINES)
ASMFLAGS += -g3
ASMFLAGS += -mcpu=cortex-m4
ASMFLAGS += -mthumb -mabi=aapcs
ASMFLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16

# Linker flags
LDFLAGS += $(OPT)
LDFLAGS += $(DEFINES)
LDFLAGS += -mthumb -mabi=aapcs -L$(SDK_ROOT)/modules/nrfx/mdk -T$(LINKER_SCRIPT)
LDFLAGS += -mcpu=cortex-m4
LDFLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16
# let linker dump unused sections
LDFLAGS += -Wl,--gc-sections
# use newlib in nano version
LDFLAGS += --specs=nano.specs

nrf52840_xxaa: CFLAGS += -D__HEAP_SIZE=8192
nrf52840_xxaa: CFLAGS += -D__STACK_SIZE=8192
nrf52840_xxaa: ASMFLAGS += -D__HEAP_SIZE=8192
nrf52840_xxaa: ASMFLAGS += -D__STACK_SIZE=8192

# Add standard libraries at the very end of the linker input, after all objects
# that may need symbols provided by these libraries.
LIB_FILES += -lc -lnosys -lm

.PHONY: default help

# Default target - first one defined
default: nrf52840_xxaa

# Print all targets that can be built
help:
	@echo following targets are available:
	@echo		nrf52840_xxaa
	@echo	   flash_softdevice
	@echo		sdk_config - starting external tool for editing sdk_config.h
	@echo		flash	  - flashing binary

TEMPLATE_PATH := $(SDK_ROOT)/components/toolchain/gcc


include $(TEMPLATE_PATH)/Makefile.common

$(foreach target, $(TARGETS), $(call define_target, $(target)))

.PHONY: erase pkg flash

pkg: default
	@echo "creating dfu package"
	nrfutil pkg generate --hw-version 52 --application-version $(VERSION) --application $(OUTPUT_DIRECTORY)/nrf52840_xxaa.hex --sd-req $(SD_REQ) $(OUTPUT_DIRECTORY)/$(APPLICATION_NAME).zip

flash: pkg
	@echo "flashing application"
	nrfutil dfu usb-serial -p /dev/ttyACM0 -pkg $(OUTPUT_DIRECTORY)/$(APPLICATION_NAME).zip



SDK_CONFIG_FILE := $(SDK_ROOT)/config/nrf52840/config/sdk_config.h
#SDK_CONFIG_FILE := $(PROJ_DIR)/config/sdk_config.h
CMSIS_CONFIG_TOOL := $(SDK_ROOT)/external_tools/cmsisconfig/CMSIS_Configuration_Wizard.jar
sdk_config:
	java -jar $(CMSIS_CONFIG_TOOL) $(SDK_CONFIG_FILE)
