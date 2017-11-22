#
# Copyright (c) 2013, 2016, Oracle and/or its affiliates. All rights reserved.
#

INCLUDE_DIRECTORIES(.)

SET(LZMA_SOURCES Alloc.c LzFind.c LzmaDec.c LzmaEnc.c LzmaLib.c)

ADD_LIBRARY(lzma_lib STATIC ${LZMA_SOURCES})
SET_PROPERTY(TARGET lzma_lib PROPERTY COMPILE_DEFINITIONS _7ZIP_ST)
