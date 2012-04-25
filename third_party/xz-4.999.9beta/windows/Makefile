###############################################################################
#
# Makefile to build XZ Utils using MinGW
#
# Make flags to alter compilation:
#
#   DEBUG=1     Enable assertions. Don't use this for production builds!
#               You may also want to set CFLAGS="-g -O0" to disable
#               optimizations.
#
#   W64=1       Build for 64-bit Windows. Make sure that you have 64-bit
#               MinGW in PATH.
#
#   WINE=1      Shortcut to set CC, AR, and STRIP to use Wine to run Windows
#               versions of MinGW binaries.
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

ifdef W64
CC = x86_64-pc-mingw32-gcc
WINDRES = x86_64-pc-mingw32-windres
AR = x86_64-pc-mingw32-ar
STRIP = x86_64-pc-mingw32-strip
else
CC = mingw32-gcc
WINDRES = windres
AR = ar
STRIP = strip
endif

SED = sed
MKDIR = mkdir
CP = cp
RM = rm -f

CFLAGS = -g -Wall -Wextra -O2
# CFLAGS = -Wall -Wextra -O3 -fomit-frame-pointer -funroll-loops

ALL_CFLAGS = -std=gnu99 -mms-bitfields

ALL_CPPFLAGS = \
	-I. \
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

# This works with Wine too while using native GNU make, sed, and rm.
ifdef WINE
ifdef W64
CC := wine c:/MinGW64/bin/x86_64-pc-mingw32-gcc
WINDRES := wine c:/MinGW64/bin/x86_64-pc-mingw32-windres
AR := wine c:/MinGW64/bin/x86_64-pc-mingw32-ar
STRIP := wine c:/MinGW64/bin/x86_64-pc-mingw32-strip
else
CC := wine c:/MinGW/bin/gcc
WINDRES := wine c:/MinGW/bin/windres
AR := wine c:/MinGW/bin/ar
STRIP := wine c:/MinGW/bin/strip
endif
endif

ifdef DEBUG
# Use echo since it works for this purpose on both Windows and POSIX.
STRIP := echo Skipping strip
else
ALL_CPPFLAGS += -DNDEBUG
endif

ALL_CPPFLAGS += $(CPPFLAGS)
ALL_CFLAGS += $(CFLAGS)


################
# Common rules #
################

.PHONY: all clean pkg
all: liblzma xzdec xz
clean: liblzma-clean xzdec-clean xz-clean

pkg: all
	$(RM) -r pkg
	$(MKDIR) -p pkg/lib pkg/include/lzma
	$(CP) liblzma.dll xz-dynamic.exe xz.exe xzdec-dynamic.exe xzdec.exe lzmadec-dynamic.exe lzmadec.exe pkg
	$(CP) liblzma.a liblzma.def liblzma_static.lib pkg/lib
	$(CP) ../src/liblzma/api/lzma.h pkg/include
	$(CP) ../src/liblzma/api/lzma/*.h pkg/include/lzma

%.o: %.rc
	$(WINDRES) $(ALL_CPPFLAGS) $< $@


###############
# liblzma.dll #
###############

.PHONY: liblzma
liblzma: liblzma.dll liblzma_static.lib

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
	../src/liblzma/common/filter_buffer_decoder.c \
	../src/liblzma/common/filter_buffer_encoder.c \
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

LIBLZMA_SRCS_ASM =

ifdef W64
LIBLZMA_SRCS_C += \
	../src/liblzma/check/crc32_fast.c \
	../src/liblzma/check/crc64_fast.c
else
LIBLZMA_SRCS_ASM += \
	../src/liblzma/check/crc32_x86.S \
	../src/liblzma/check/crc64_x86.S
endif

LIBLZMA_OBJS_C = $(LIBLZMA_SRCS_C:.c=.o)
LIBLZMA_OBJS_ASM = $(LIBLZMA_SRCS_ASM:.S=.o)
LIBLZMA_OBJS = \
	$(LIBLZMA_OBJS_C) \
	$(LIBLZMA_OBJS_ASM) \
	../src/liblzma/liblzma_w32res.o

LIBLZMA_OBJS_STATIC_C = $(LIBLZMA_SRCS_C:.c=-static.o)
LIBLZMA_OBJS_STATIC_ASM = $(LIBLZMA_SRCS_ASM:.S=-static.o)
LIBLZMA_OBJS_STATIC = $(LIBLZMA_OBJS_STATIC_C) $(LIBLZMA_OBJS_STATIC_ASM)

# The sed is needed to remove ordinals from the .def file. I'm not going
# to track the ordinal numbers, so people should link against liblzma.dll
# only by using symbol names.
liblzma.dll: $(LIBLZMA_OBJS)
	$(CC) $(ALL_CFLAGS) -shared -o liblzma.dll $(LIBLZMA_OBJS) -Wl,--out-implib,liblzma.a,--output-def,liblzma.def.in
	$(SED) 's/ \+@ *[0-9]\+//' liblzma.def.in > liblzma.def
	$(RM) liblzma.def.in
	$(STRIP) --strip-unneeded liblzma.a
	$(STRIP) --strip-all liblzma.dll

$(LIBLZMA_OBJS_C): %.o: %.c
	$(CC) -DDLL_EXPORT $(ALL_CPPFLAGS) $(ALL_CFLAGS) -c -o $@ $<

$(LIBLZMA_OBJS_ASM): %.o: %.S
	$(CC) -DDLL_EXPORT $(ALL_CPPFLAGS) $(ALL_CFLAGS) -c -o $@ $<

liblzma_static.lib: $(LIBLZMA_OBJS_STATIC)
	$(RM) $@
	$(AR) rcs $@ $(LIBLZMA_OBJS_STATIC)
	$(STRIP) --strip-unneeded $@

$(LIBLZMA_OBJS_STATIC_C): %-static.o: %.c
	$(CC) $(ALL_CPPFLAGS) $(ALL_CFLAGS) -c -o $@ $<

$(LIBLZMA_OBJS_STATIC_ASM): %-static.o: %.S
	$(CC) $(ALL_CPPFLAGS) $(ALL_CFLAGS) -c -o $@ $<

.PHONY: liblzma-clean
liblzma-clean:
	-$(RM) $(LIBLZMA_OBJS) $(LIBLZMA_OBJS_STATIC) liblzma.def.in liblzma.def liblzma.a liblzma.dll liblzma_static.lib


###########################
# xzdec.exe & lzmadec.exe #
###########################

.PHONY: xzdec
xzdec: xzdec-dynamic.exe lzmadec-dynamic.exe xzdec.exe lzmadec.exe

XZDEC_SRCS = ../src/xzdec/xzdec.c

xzdec-dynamic.exe: liblzma.dll $(XZDEC_SRCS) ../src/xzdec/xzdec_w32res.o
	$(CC) $(ALL_CPPFLAGS) $(ALL_CFLAGS) $(XZDEC_SRCS) ../src/xzdec/xzdec_w32res.o -o $@ liblzma.a
	$(STRIP) --strip-all $@

lzmadec-dynamic.exe: liblzma.dll $(XZDEC_SRCS) ../src/xzdec/lzmadec_w32res.o
	$(CC) $(ALL_CPPFLAGS) -DLZMADEC $(ALL_CFLAGS) $(XZDEC_SRCS) ../src/xzdec/lzmadec_w32res.o -o $@ liblzma.a
	$(STRIP) --strip-all $@

xzdec.exe: liblzma_static.lib $(XZDEC_SRCS) ../src/xzdec/xzdec_w32res.o
	$(CC) -DLZMA_API_STATIC $(ALL_CPPFLAGS) $(ALL_CFLAGS) $(XZDEC_SRCS) ../src/xzdec/xzdec_w32res.o -o $@ liblzma_static.lib
	$(STRIP) --strip-all $@

lzmadec.exe: liblzma_static.lib $(XZDEC_SRCS) ../src/xzdec/lzmadec_w32res.o
	$(CC) -DLZMA_API_STATIC $(ALL_CPPFLAGS) -DLZMADEC $(ALL_CFLAGS) $(XZDEC_SRCS) ../src/xzdec/lzmadec_w32res.o -o $@ liblzma_static.lib
	$(STRIP) --strip-all $@

.PHONY: xzdec-clean
xzdec-clean:
	-$(RM) xzdec-dynamic.exe lzmadec-dynamic.exe xzdec.exe lzmadec.exe ../src/xzdec/xzdec_w32res.o ../src/xzdec/lzmadec_w32res.o


##########
# xz.exe #
##########

.PHONY: xz
xz: xz-dynamic.exe xz.exe

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
XZ_OBJS_STATIC = $(XZ_SRCS:.c=-static.o)

$(XZ_OBJS): %.o: %.c
	$(CC) $(ALL_CPPFLAGS) $(ALL_CFLAGS) -c -o $@ $<

xz-dynamic.exe: liblzma.dll $(XZ_OBJS) ../src/xz/xz_w32res.o
	$(CC) $(ALL_CFLAGS) $(XZ_OBJS) ../src/xz/xz_w32res.o -o $@ liblzma.a
	$(STRIP) --strip-all $@

$(XZ_OBJS_STATIC): %-static.o: %.c
	$(CC) -DLZMA_API_STATIC $(ALL_CPPFLAGS) $(ALL_CFLAGS) -c -o $@ $<

xz.exe: liblzma_static.lib $(XZ_OBJS_STATIC) ../src/xz/xz_w32res.o
	$(CC) $(ALL_CFLAGS) $(XZ_OBJS_STATIC) ../src/xz/xz_w32res.o -o $@ liblzma_static.lib
	$(STRIP) --strip-all $@

.PHONY: xz-clean
xz-clean:
	-$(RM) $(XZ_OBJS) $(XZ_OBJS_STATIC) ../src/xz/xz_w32res.o xz-dynamic.exe xz.exe
