###############################################################################
#
# Makefile to build XZ Utils using DJGPP
#
# Make flags to alter compilation:
#
#   DEBUG=1     Enable assertions. Don't use this for production builds!
#               You may also want to set CFLAGS="-g -O0" to disable
#               optimizations.
#
# The usual CPPFLAGS and CFLAGS are supported too.
#
###############################################################################
#
# Author: Lasse Collin
#
# This file has been put into the public domain.
# You can do whatever you want with this file.
#
###############################################################################

CC = gcc
AR = ar
STRIP = strip
SED = sed
RM = rm -f

CFLAGS = -g -Wextra -Wfatal-errors -Wall -march=i386 -mtune=i686 -O2

# NOTE: -fgnu89-inline is needed on DJGPP 2.04 beta and GCC 4.3.2
# because time.h uses GNU-style "extern inline".
ALL_CFLAGS = -std=gnu99 -fgnu89-inline

ALL_CPPFLAGS = \
	-I. \
	-I../lib \
	-I../src/common \
	-I../src/liblzma/api \
	-I../src/liblzma/common \
	-I../src/liblzma/check \
	-I../src/liblzma/rangecoder \
	-I../src/liblzma/lz \
	-I../src/liblzma/lzma \
	-I../src/liblzma/delta \
	-I../src/liblzma/simple \
	-I../src/liblzma/subblock

ALL_CPPFLAGS += -DHAVE_CONFIG_H

ifdef DEBUG
STRIP := rem Skipping strip
else
ALL_CPPFLAGS += -DNDEBUG
endif

ALL_CPPFLAGS += $(CPPFLAGS)
ALL_CFLAGS += $(CFLAGS)


################
# Common rules #
################

.PHONY: all clean
all: liblzma.a getopt.a xzdec.exe lzmadec.exe xz.exe
clean: liblzma-clean getopt-clean xzdec-clean xz-clean


#############
# liblzma.a #
#############

LIBLZMA_SRCS_C = \
	../src/liblzma/common/alone_decoder.c \
	../src/liblzma/common/alone_encoder.c \
	../src/liblzma/common/auto_decoder.c \
	../src/liblzma/common/block_buffer_decoder.c \
	../src/liblzma/common/block_buffer_encoder.c \
	../src/liblzma/common/block_decoder.c \
	../src/liblzma/common/block_encoder.c \
	../src/liblzma/common/block_header_decoder.c \
	../src/liblzma/common/block_header_encoder.c \
	../src/liblzma/common/block_util.c \
	../src/liblzma/common/common.c \
	../src/liblzma/common/easy_buffer_encoder.c \
	../src/liblzma/common/easy_decoder_memusage.c \
	../src/liblzma/common/easy_encoder.c \
	../src/liblzma/common/easy_encoder_memusage.c \
	../src/liblzma/common/easy_preset.c \
	../src/liblzma/common/filter_common.c \
	../src/liblzma/common/filter_decoder.c \
	../src/liblzma/common/filter_encoder.c \
	../src/liblzma/common/filter_flags_decoder.c \
	../src/liblzma/common/filter_flags_encoder.c \
	../src/liblzma/common/index.c \
	../src/liblzma/common/index_decoder.c \
	../src/liblzma/common/index_encoder.c \
	../src/liblzma/common/index_hash.c \
	../src/liblzma/common/stream_buffer_decoder.c \
	../src/liblzma/common/stream_buffer_encoder.c \
	../src/liblzma/common/stream_decoder.c \
	../src/liblzma/common/stream_encoder.c \
	../src/liblzma/common/stream_flags_common.c \
	../src/liblzma/common/stream_flags_decoder.c \
	../src/liblzma/common/stream_flags_encoder.c \
	../src/liblzma/common/vli_decoder.c \
	../src/liblzma/common/vli_encoder.c \
	../src/liblzma/common/vli_size.c \
	../src/liblzma/check/check.c \
	../src/liblzma/check/crc32_table.c \
	../src/liblzma/check/crc64_table.c \
	../src/liblzma/check/sha256.c \
	../src/liblzma/rangecoder/price_table.c \
	../src/liblzma/lz/lz_decoder.c \
	../src/liblzma/lz/lz_encoder.c \
	../src/liblzma/lz/lz_encoder_mf.c \
	../src/liblzma/lzma/fastpos_table.c \
	../src/liblzma/lzma/lzma2_decoder.c \
	../src/liblzma/lzma/lzma2_encoder.c \
	../src/liblzma/lzma/lzma_decoder.c \
	../src/liblzma/lzma/lzma_encoder.c \
	../src/liblzma/lzma/lzma_encoder_optimum_fast.c \
	../src/liblzma/lzma/lzma_encoder_optimum_normal.c \
	../src/liblzma/lzma/lzma_encoder_presets.c \
	../src/liblzma/delta/delta_common.c \
	../src/liblzma/delta/delta_decoder.c \
	../src/liblzma/delta/delta_encoder.c \
	../src/liblzma/simple/arm.c \
	../src/liblzma/simple/armthumb.c \
	../src/liblzma/simple/ia64.c \
	../src/liblzma/simple/powerpc.c \
	../src/liblzma/simple/simple_coder.c \
	../src/liblzma/simple/simple_decoder.c \
	../src/liblzma/simple/simple_encoder.c \
	../src/liblzma/simple/sparc.c \
	../src/liblzma/simple/x86.c

LIBLZMA_SRCS_ASM = \
	../src/liblzma/check/crc32_x86.S \
	../src/liblzma/check/crc64_x86.S

LIBLZMA_OBJS_C = $(LIBLZMA_SRCS_C:.c=.o)
LIBLZMA_OBJS_ASM = $(LIBLZMA_SRCS_ASM:.S=.o)
LIBLZMA_OBJS = $(LIBLZMA_OBJS_C) $(LIBLZMA_OBJS_ASM)

$(LIBLZMA_OBJS_C): %.o: %.c
	$(CC) $(ALL_CPPFLAGS) $(ALL_CFLAGS) -c -o $@ $<

$(LIBLZMA_OBJS_ASM): %.o: %.S
	$(CC) $(ALL_CPPFLAGS) $(ALL_CFLAGS) -c -o $@ $<

liblzma.a: $(LIBLZMA_OBJS)
	$(RM) $@
	$(AR) rcs $@ $(LIBLZMA_OBJS)
	$(STRIP) --strip-unneeded $@

# Avoid too long command lines.
.PHONY: liblzma-clean $(LIBLZMA_OBJS:.o=-clean)
liblzma-clean: $(LIBLZMA_OBJS:.o=-clean)
	-$(RM) liblzma.a

$(LIBLZMA_OBJS:.o=-clean):
	-$(RM) $(@:-clean=.o)


###############
# getopt_long #
###############

GETOPT_SRCS = \
	../lib/getopt.c \
	../lib/getopt1.c

GETOPT_OBJS = $(GETOPT_SRCS:.c=.o)

GETOPT_H = ../lib/getopt.h

$(GETOPT_H): %.h: %.in.h
	$(SED) "" $< > $@

$(GETOPT_OBJS): %.o: %.c $(GETOPT_H)
	$(CC) $(ALL_CPPFLAGS) $(ALL_CFLAGS) -c -o $@ $<

getopt.a: $(GETOPT_OBJS)
	$(RM) $@
	$(AR) rcs $@ $(GETOPT_OBJS)
	$(STRIP) --strip-unneeded $@

getopt-clean:
	$(RM) $(GETOPT_H) $(GETOPT_OBJS) getopt.a


###########################
# xzdec.exe & lzmadec.exe #
###########################

XZDEC_SRCS = ../src/xzdec/xzdec.c

xzdec.exe: getopt.a liblzma.a $(XZDEC_SRCS)
	$(CC) $(ALL_CPPFLAGS) $(ALL_CFLAGS) $(XZDEC_SRCS) -o $@ getopt.a liblzma.a
	$(STRIP) --strip-all $@
	exe2coff $@
	$(RM) $@
	copy /b $(DJGPP:DJGPP.ENV=BIN\CWSDSTUB.EXE) + $(@:.exe=) $@
	$(RM) $(@:.exe=)

lzmadec.exe: getopt.a liblzma.a $(XZDEC_SRCS)
	$(CC) $(ALL_CPPFLAGS) -DLZMADEC $(ALL_CFLAGS) $(XZDEC_SRCS) -o $@ getopt.a liblzma.a
	$(STRIP) --strip-all $@
	exe2coff $@
	$(RM) $@
	copy /b $(DJGPP:DJGPP.ENV=BIN\CWSDSTUB.EXE) + $(@:.exe=) $@
	$(RM) $(@:.exe=)

.PHONY: xzdec-clean
xzdec-clean:
	-$(RM) xzdec.exe lzmadec.exe xzdec lzmadec


##########
# xz.exe #
##########

XZ_SRCS = \
	../src/xz/args.c \
	../src/xz/coder.c \
	../src/xz/file_io.c \
	../src/xz/hardware.c \
	../src/xz/main.c \
	../src/xz/message.c \
	../src/xz/options.c \
	../src/xz/signals.c \
	../src/xz/suffix.c \
	../src/xz/util.c

XZ_OBJS = $(XZ_SRCS:.c=.o)

$(XZ_OBJS): %.o: %.c
	$(CC) $(ALL_CPPFLAGS) $(ALL_CFLAGS) -c -o $@ $<

xz.exe: getopt.a liblzma.a $(XZ_OBJS)
	$(CC) $(ALL_CFLAGS) $(XZ_OBJS) -o $@ getopt.a liblzma.a
	$(STRIP) --strip-all $@
	exe2coff $@
	$(RM) $@
	copy /b $(DJGPP:DJGPP.ENV=BIN\CWSDSTUB.EXE) + $(@:.exe=) $@
	$(RM) $(@:.exe=)

# Avoid too long command lines.
.PHONY: xz-clean $(XZ_OBJS:.o=-clean)
xz-clean: $(XZ_OBJS:.o=-clean)
	-$(RM) xz.exe xz

$(XZ_OBJS:.o=-clean):
	-$(RM) $(@:-clean=.o)
