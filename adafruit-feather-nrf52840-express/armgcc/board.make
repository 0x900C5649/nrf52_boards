LD_SCRIPT ?= $(BOARD_DIRECORY)/nrf52_gcc.ld

$(OUTPUT_DIRECTORY)/nrf52840_xxaa.out: \
  LINKER_SCRIPT  := $(LD_SCRIPT)

SRC_FILES += \

INC_FOLDERS += $(BOARD_DIRECTORY)/board_config \

# DEFINITIONS
BOARD_DEFINES += -DBOARD_CUSTOM \
          		 -DFLOAT_ABI_HARD \
                 -DNRF52840_XXAA \

# C flags common to all targets
CFLAGS += $(BOARD_DEFINES)
CFLAGS += -mcpu=cortex-m4
CFLAGS += -mthumb -mabi=aapcs
CFLAGS += -mfloat-abi=hard -mfpu=fpv4-sp-d16
# keep every function in a separate section, this allows linker to discard unused ones
CFLAGS += -ffunction-sections -fdata-sections -fno-strict-aliasing
CFLAGS += -fno-builtin -fshort-enums

# C++ flags common to all targets
CXXFLAGS += $(DEFINES)

# Assembler flags common to all targets
ASMFLAGS += $(OPT)
ASMFLAGS += $(DEFINES)
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

LIB_FILES += -lc -lnosys -lm

.PHONY: pkg flash default

default: nrf52840_xxaa

flash: pkg
	@echo "flashing application"
	nrfutil dfu usb-serial -p /dev/ttyACM0 -pkg $(PKG_OUT)

pkg: nrf52840_xxaa
	@echo "creating dfu package"	
	nrfutil pkg generate --hw-version 52 --application-version $(VERSION) --application $(APPLICATION) $(SD-REQ) $(PKG_OUT)

