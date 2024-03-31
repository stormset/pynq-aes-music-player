SHELL = /bin/bash

CROSS_COMPILE ?= arm-linux-gnueabihf
CC = $(CROSS_COMPILE)-gcc
CXX = $(CROSS_COMPILE)-g++
CFLAGS = -Wall -Wextra -lm
CXXFLAGS = -Wall -Wextra -lm -std=c++17

SYSROOT_PATH ?= /home/$(shell whoami)/aes-music/images/linux/rootfs
DOWNLOAD_DIR ?= $(ROOT)/external/libsndfile

# external library URLs needed for MP3 format support
URL_ALSALIB     := https://www.alsa-project.org/files/pub/lib/alsa-lib-1.2.9.tar.bz2
URL_LAME        := https://sourceforge.net/projects/lame/files/lame/3.100/lame-3.100.tar.gz
URL_MPG123      := https://sourceforge.net/projects/mpg123/files/mpg123/1.32.5/mpg123-1.32.5.tar.bz2
URL_SNDFILE  := https://github.com/libsndfile/libsndfile/archive/refs/tags/1.2.2.tar.gz

EXTERNAL_LIBRARY_TARGETS = alsalib lame mpg123 sndfile
EXTERNAL_LIBRARY_SOURCES = $(addprefix $(DOWNLOAD_DIR)/,$(EXTERNAL_LIBRARY_TARGETS))

SNDFILE_EXTRA_STEP            = autoreconf -vif &&
SNDFILE_EXTRA_CONFIGURE_ARGS ?= --enable-shared --with-sysroot=$(SYSROOT_PATH) LDFLAGS="-L$(shell readlink -f $(OUTPUT_DIR)/lib)" CFLAGS="-I$(shell readlink -f $(OUTPUT_DIR)/include)"

.PHONY: $(EXTERNAL_LIBRARY_TARGETS)

.SECONDEXPANSION:

libsndfile: $(EXTERNAL_LIBRARY_TARGETS)

$(EXTERNAL_LIBRARY_TARGETS): $(OUTPUT_DIR) $(DOWNLOAD_DIR)/$$@
	pushd . && cd $(DOWNLOAD_DIR)/$@ && $($(shell basename $@ | tr a-z A-Z)_EXTRA_STEP) ./configure --prefix='' --host=$(CROSS_COMPILE) $($(shell basename $@ | tr a-z A-Z)_EXTRA_CONFIGURE_ARGS) && popd
	make -C $(DOWNLOAD_DIR)/$@
	make -C $(DOWNLOAD_DIR)/$@ DESTDIR=$(shell readlink -f $(OUTPUT_DIR)) install

$(EXTERNAL_LIBRARY_SOURCES):
	mkdir -p $(DOWNLOAD_DIR)

	wget $(URL_$(shell basename $@ | tr a-z A-Z)) -O $@.tar
	mkdir -p $@
	tar -xf $@.tar -C $@ --strip-components=1
	rm -f $@.tar

