/*
 * Copyright (c) 2018 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 */

#ifndef _OPENBSD_COMPAT_H
#define _OPENBSD_COMPAT_H

#if defined(_MSC_VER)
#include "types.h"
#endif

#if defined(HAVE_ENDIAN_H)
#include <endian.h>
#endif

#if defined(__APPLE__) && !defined(HAVE_ENDIAN_H)
#include <libkern/OSByteOrder.h>
#define be16toh(x) OSSwapBigToHostInt16((x))
#define htobe16(x) OSSwapHostToBigInt16((x))
#define be32toh(x) OSSwapBigToHostInt32((x))
#endif /* __APPLE__ && !HAVE_ENDIAN_H */

#if defined(_WIN32) && !defined(HAVE_ENDIAN_H)
#include <winsock2.h>
#if !defined(_MSC_VER)
#include <sys/param.h>
#endif
#define be16toh(x) ntohs((x))
#define htobe16(x) htons((x))
#define be32toh(x) ntohl((x))
#endif /* _WIN32 && !HAVE_ENDIAN_H */

#if defined(__FreeBSD__) && !defined(HAVE_ENDIAN_H)
#include <sys/endian.h>
#endif

#include <stdlib.h>

#if !defined(HAVE_STRLCAT)
size_t strlcat(char *, const char *, size_t);
#endif

#if !defined(HAVE_STRLCPY)
size_t strlcpy(char *, const char *, size_t);
#endif

#if !defined(HAVE_RECALLOCARRAY)
void *recallocarray(void *, size_t, size_t, size_t);
#endif

#if !defined(HAVE_EXPLICIT_BZERO)
void explicit_bzero(void *, size_t);
#endif

#if !defined(HAVE_GETPAGESIZE)
int getpagesize(void);
#endif

#if !defined(HAVE_TIMINGSAFE_BCMP)
int timingsafe_bcmp(const void *, const void *, size_t);
#endif

#if !defined(HAVE_READPASSPHRASE)
#include "readpassphrase.h"
#else
#include <readpassphrase.h>
#endif

#include <openssl/opensslv.h>
#if OPENSSL_VERSION_NUMBER < 0x10100000L
#define EVP_PKEY_get0_EC_KEY(x) ((x)->pkey.ec)
#define EVP_PKEY_get0_RSA(x) ((x)->pkey.rsa)
#endif

#if !defined(HAVE_ERR_H)
#include "err.h"
#else
#include <err.h>
#endif

#if !defined(HAVE_GETOPT)
#include "getopt.h"
#else
#include <unistd.h>
#endif

#if !defined(HAVE_GETLINE)
#include <stdio.h>
ssize_t getline(char **, size_t *, FILE *);
#endif

#include "time.h"

#endif /* !_OPENBSD_COMPAT_H */
