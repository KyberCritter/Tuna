# SPDX-License-Identifier: MIT
# Primary entry point: GNU Make + host C compiler + devkitPro. No Python/PowerShell.
.DEFAULT_GOAL := all
CC ?= cc
CFLAGS ?= -O2 -g -std=c11 -Wall -Wextra -Werror
DEVKITPRO ?= /opt/devkitpro
DEVKITARM ?= $(DEVKITPRO)/devkitARM
DEVKITPPC ?= $(DEVKITPRO)/devkitPPC
export DEVKITPRO DEVKITARM DEVKITPPC

ifeq ($(OS),Windows_NT)
EXEEXT := .exe
endif
INCLUDES := -Iinclude -Ithird_party/minimp3
SHARED := $(wildcard shared/*.c)
HOST := $(SHARED) host/convert.c
HEADERS := $(wildcard include/*.h) third_party/minimp3/minimp3.h
HOST_TEST := build/host/tests$(EXEEXT)
CONVERTER := build/host/tuna-convert$(EXEEXT)
BUILD_TOOL := build/host/tuna-build$(EXEEXT)
ARM_NM := $(DEVKITARM)/bin/arm-none-eabi-nm
LICENSE_FILES := $(wildcard LICENSES/*)
LEGAL_FILES := LICENSE THIRD-PARTY-NOTICES.md $(LICENSE_FILES) \
	LICENSES/minimp3-CC0.txt LICENSES/libgba.txt LICENSES/libogc.txt
DIST_FILES := tuna.mb tuna-convert$(EXEEXT) jingle.tuna README.md $(LEGAL_FILES) \
	tuna-gamecube.dol apps/tuna/boot.dol apps/tuna/meta.xml \
	$(addprefix apps/tuna/,$(LEGAL_FILES))

.PHONY: all host test gba embed gamecube wii package
all: package
host: test $(CONVERTER) build/host/jingle.tuna

build/host:
	mkdir -p $@
$(HOST_TEST): $(HOST) tests/test.c $(HEADERS) Makefile | build/host
	$(CC) $(CFLAGS) $(INCLUDES) $(HOST) tests/test.c -lm -o $@
$(CONVERTER): $(HOST) host/main.c $(HEADERS) Makefile | build/host
	$(CC) $(CFLAGS) $(INCLUDES) $(HOST) host/main.c -lm -o $@
$(BUILD_TOOL): tools/build_tool.c tools/sha256.c tools/sha256.h Makefile | build/host
	$(CC) $(CFLAGS) -Itools tools/build_tool.c tools/sha256.c -o $@
build/host/jingle.tuna: $(CONVERTER)
	./$(CONVERTER) --jingle $@
test: $(HOST_TEST) $(BUILD_TOOL)
	cd build/host && ./tests$(EXEEXT) && ./tuna-build$(EXEEXT) selftest

gba: host
	+$(MAKE) -C gba
	$(ARM_NM) -n build/gba/tuna.elf > build/gba/symbols.txt
	./$(BUILD_TOOL) check-gba build/gba/tuna.mb build/gba/symbols.txt
embed: gba
	mkdir -p build/generated
	./$(BUILD_TOOL) embed build/gba/tuna.mb build/generated/player_image.c build/generated/player_image.h
gamecube: embed
	+$(MAKE) -C sender PLATFORM=gamecube
wii: embed
	+$(MAKE) -C sender PLATFORM=wii

package: host gamecube wii
	mkdir -p dist/apps/tuna/LICENSES dist/LICENSES
	cp build/gba/tuna.mb $(CONVERTER) build/host/jingle.tuna README.md LICENSE THIRD-PARTY-NOTICES.md dist/
	cp $(LICENSE_FILES) dist/LICENSES/
	cp third_party/minimp3/LICENSE dist/LICENSES/minimp3-CC0.txt
	cp $(DEVKITPRO)/libgba/libgba_license.txt dist/LICENSES/libgba.txt
	cp $(DEVKITPRO)/libogc/LICENSE dist/LICENSES/libogc.txt
	cp dist/LICENSE dist/THIRD-PARTY-NOTICES.md dist/apps/tuna/
	cp $(addprefix dist/,$(filter LICENSES/%,$(LEGAL_FILES))) dist/apps/tuna/LICENSES/
	cp build/gamecube/tuna.dol dist/tuna-gamecube.dol
	cp build/wii/tuna.dol dist/apps/tuna/boot.dol
	cp assets/meta.xml dist/apps/tuna/meta.xml
	cd dist && ../$(BUILD_TOOL) sha256 $(DIST_FILES) > sha256.txt
	@echo "Build and tests complete: dist/ (emulator/hardware acceptance is separate)."
