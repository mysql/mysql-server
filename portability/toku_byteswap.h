/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "Copyright (c) 2012-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#ifndef TOKU_BYTESWAP_H
#define TOKU_BYTESWAP_H

#include <config.h>

#if defined(HAVE_BYTESWAP_H)
# include <byteswap.h>
#elif defined(HAVE_SYS_ENDIAN_H)
# include <sys/endian.h>
# define bswap_64 bswap64
#elif defined(HAVE_LIBKERN_OSBYTEORDER_H)
# include <libkern/OSByteOrder.h>
# define bswap_64 OSSwapInt64
#endif

#endif /* TOKU_BYTESWAP_H */
