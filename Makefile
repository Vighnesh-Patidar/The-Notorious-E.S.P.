# --- Variables & Paths ---
SDK_ROOT = $(HOME)/esp/ESP8266_NONOS_SDK
TOOLCHAIN_PREFIX = xtensa-lx106-elf-
CC = $(TOOLCHAIN_PREFIX)gcc
CXX = $(TOOLCHAIN_PREFIX)g++
AR = $(TOOLCHAIN_PREFIX)ar
LD = $(TOOLCHAIN_PREFIX)gcc

# --- Compiler Flags ---
COMMON_FLAGS = -mlongcalls -mtext-section-literals -Os -I$(SDK_ROOT)/include -I$(SDK_ROOT)/driver_lib/include -Iinclude -I$(HOME)/esp/xtensa-lx106-elf/xtensa-lx106-elf/include
CFLAGS = $(COMMON_FLAGS)
CXXFLAGS = $(COMMON_FLAGS) -fno-exceptions -fno-rtti -std=c++11

# --- Linker Flags ---
LDFLAGS += -nostdlib
LDFLAGS += -Wl,--no-check-sections
LDFLAGS += -Wl,-static
LDFLAGS += -L$(SDK_ROOT)/lib
LDFLAGS += -lmain -lnet80211 -lwpa -llwip -lpp -lphy
LDFLAGS += -T$(SDK_ROOT)/ld/eagle.app.v6.ld
LDFLAGS += -lgcc
LDFLAGS += -lcrypto
LDFLAGS += -lmain -lnet80211 -lwpa -llwip -lpp -lphy -lcrypto -lgcc
LDFLAGS += -lc

# --- Source and Object Files ---
SRC = $(wildcard src/*.cpp)
OBJ = $(patsubst src/%.cpp, build/%.o, $(SRC))
TARGET = build/packet_inspector.elf

# --- Rules ---

# 1. Default target
all: $(TARGET)

# Directory creation rule
build/:
	mkdir -p build

# 2. Linking rule
$(TARGET): $(OBJ)
	$(LD) $(OBJ) $(LDFLAGS) -o $@

# 3. Pattern rule for compiling (with order-only prerequisite)
build/%.o: src/%.cpp | build/
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 4. Clean rule
clean:
	rm -rf build/

# 5. Flash rule
flash: $(TARGET)
	esptool --port /dev/ttyUSB0 elf2image --flash_size 4MB --flash_mode qio --flash_freq 40m $(TARGET)
	esptool --port /dev/ttyUSB0 write_flash --flash_size 4MB --flash_mode qio --flash_freq 40m \
		0x00000 build/packet_inspector.elf-0x00000.bin \
		0x10000 build/packet_inspector.elf-0x10000.bin \
		0x3FB000 $(SDK_ROOT)/bin/blank.bin \
		0x3FC000 $(SDK_ROOT)/bin/esp_init_data_default_v08.bin \
		0x3FD000 $(SDK_ROOT)/bin/blank.bin \
		0x3FE000 $(SDK_ROOT)/bin/blank.bin

# 6. Monitor rule
monitor:
	picocom -b 115200 /dev/ttyUSB0

.PHONY: all clean flash monitor
