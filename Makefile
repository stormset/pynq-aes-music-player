SHELL = /bin/bash

CROSS_COMPILE ?= arm-linux-gnueabihf
CC = $(CROSS_COMPILE)-gcc
CXX = $(CROSS_COMPILE)-g++
CFLAGS = -Wall -Wextra -lm
CXXFLAGS = -Wall -Wextra -lm -std=c++17

OUTPUT_DIR ?= outputs
INCLUDE_DIR ?= $(OUTPUT_DIR)/include
SYSROOT_PATH ?= /home/$(shell whoami)/aes-music/images/linux/rootfs # sysroot of petalinux

ROOT = $(shell pwd)

.PHONY: $(EXTERNAL_LIBRARY_TARGETS)

.SECONDEXPANSION:

all: libsndfile libaxidma app

include libaxidma/library.mk
include external/libsndfile/libsndfile.mk
include app/component.mk

$(OUTPUT_DIR):
	@mkdir -p $(OUTPUT_DIR)

toolchains:
	sudo apt install autoconf autogen automake build-essential libtool pkg-config	

