/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."
#ifndef ENDIAN_H
#define ENDIAN_H

#if defined(__BYTE_ORDER) || defined(__LITTLE_ENDIAN) || defined(__BIG_ENDIAN)
#error Standards are defined for some reason
#endif

//Windows does not exist for big endian machines, only little endian
#define __LITTLE_ENDIAN (0x01020304)
#define __BIG_ENDIAN    (0x04030201)
#define __BYTE_ORDER    (__LITTLE_ENDIAN)


#endif

