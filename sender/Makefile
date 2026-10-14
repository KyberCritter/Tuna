PLATFORM ?= gamecube
ifeq ($(strip $(DEVKITPPC)),)
$(error Set DEVKITPPC and DEVKITPRO using devkitPro's environment)
endif
include $(DEVKITPPC)/$(PLATFORM)_rules
ROOT := $(abspath ..)
OUT := $(ROOT)/build/$(PLATFORM)
CC := $(DEVKITPPC)/bin/powerpc-eabi-gcc
CFLAGS := -std=gnu11 -O2 -g -Wall -Wextra -Werror $(MACHDEP) -ffunction-sections -fdata-sections -I$(ROOT)/include -I$(ROOT)/build/generated -I$(LIBOGC_INC) -I$(ROOT)/third_party/minimp3
SOURCES := main.c link.c ../host/convert.c ../shared/format.c ../shared/adpcm.c ../shared/protocol.c ../shared/player.c ../shared/resample.c ../shared/boot.c
OBJECTS := $(addprefix $(OUT)/,$(notdir $(SOURCES:.c=.o))) $(OUT)/player_image.o
VPATH := .:../host:../shared:../build/generated
LIBS := -lfat -logc -lm
ifeq ($(PLATFORM),wii)
LIBS := -lfat -lwiiuse -lbte -logc -lm
endif
.PHONY: all
all: $(OUT)/tuna.dol
$(OUT):
	mkdir -p $@
$(OUT)/%.o: %.c $(wildcard $(ROOT)/include/*.h) $(ROOT)/build/generated/player_image.h Makefile | $(OUT)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@
$(OUT)/tuna.elf: $(OBJECTS)
	$(CC) $(MACHDEP) -Wl,--gc-sections,-Map,$(OUT)/tuna.map $(OBJECTS) -L$(LIBOGC_LIB) $(LIBS) -o $@
$(OUT)/tuna.dol: $(OUT)/tuna.elf
	$(DEVKITPRO)/tools/bin/elf2dol $< $@
-include $(OBJECTS:.o=.d)
