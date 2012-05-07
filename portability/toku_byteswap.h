/* -*- mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#ident "Copyright (c) 2012 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#ifndef TOKU_BYTESWAP_H
#define TOKU_BYTESWAP_H

#include <config.h>

#if defined(HAVE_BYTESWAP_H)
# include <byteswap.h>
#elif defined(HAVE_LIBKERN_OSBYTEORDER_H)
# include <libkern/OSByteOrder.h>
# define bswap_64 OSSwapInt64
#endif

#endif /* TOKU_BYTESWAP_H */
