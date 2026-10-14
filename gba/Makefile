ifeq ($(strip $(DEVKITARM)),)
$(error Set DEVKITARM and DEVKITPRO using devkitPro's environment)
endif
include $(DEVKITARM)/gba_rules

ROOT := $(abspath ..)
OUT := $(ROOT)/build/gba
CC := $(DEVKITARM)/bin/arm-none-eabi-gcc
OBJCOPY := $(DEVKITARM)/bin/arm-none-eabi-objcopy
CFLAGS := -std=gnu11 -Os -g -Wall -Wextra -Werror -mcpu=arm7tdmi -mthumb -mthumb-interwork -ffunction-sections -fdata-sections -I$(ROOT)/include -I$(DEVKITPRO)/libgba/include
SOURCES := main.c display.c audio.c ../shared/format.c ../shared/adpcm.c ../shared/protocol.c ../shared/player.c
OBJECTS := $(addprefix $(OUT)/,$(notdir $(SOURCES:.c=.o)))
VPATH := .:../shared

.PHONY: all
all: $(OUT)/tuna.mb
$(OUT):
	mkdir -p $@
$(OUT)/%.o: %.c $(wildcard $(ROOT)/include/*.h) $(wildcard *.h) Makefile | $(OUT)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@
$(OUT)/tuna.elf: $(OBJECTS) memory.ld
	$(CC) -mcpu=arm7tdmi -mthumb -mthumb-interwork -specs=gba_mb.specs -Wl,--gc-sections,-Map,$(OUT)/tuna.map -Wl,-T,memory.ld $(OBJECTS) -L$(DEVKITPRO)/libgba/lib -lgba -o $@
$(OUT)/tuna.mb: $(OUT)/tuna.elf
	$(OBJCOPY) -O binary $< $@
	$(DEVKITPRO)/tools/bin/gbafix $@ -tTUNA -cTUNA -m01
-include $(OBJECTS:.o=.d)
