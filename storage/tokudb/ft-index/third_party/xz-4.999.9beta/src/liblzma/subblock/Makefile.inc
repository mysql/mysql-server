##
## Author: Lasse Collin
##
## This file has been put into the public domain.
## You can do whatever you want with this file.
##

if COND_ENCODER_SUBBLOCK
liblzma_la_SOURCES += \
	subblock/subblock_encoder.c \
	subblock/subblock_encoder.h
endif

if COND_DECODER_SUBBLOCK
liblzma_la_SOURCES += \
	subblock/subblock_decoder.c \
	subblock/subblock_decoder.h \
	subblock/subblock_decoder_helper.c \
	subblock/subblock_decoder_helper.h
endif
