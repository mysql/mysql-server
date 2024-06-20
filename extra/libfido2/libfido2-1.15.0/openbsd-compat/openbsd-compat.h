/*
 * Copyright (c) 2018-2021 Yubico AB. All rights reserved.
 * Use of this source code is governed by a BSD-style
 * license that can be found in the LICENSE file.
 * SPDX-License-Identifier: BSD-2-Clause
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
#define htobe32(x) OSSwapHostToBigInt32((x))
#define htole32(x) OSSwapHostToLittleInt32((x))
#define htole64(x) OSSwapHostToLittleInt64((x))
#endif /* __APPLE__ && !HAVE_ENDIAN_H */

#if defined(_WIN32) && !defined(HAVE_ENDIAN_H)
#include <stdint.h>
#include <winsock2.h>
#if !defined(_MSC_VER)
#include <sys/param.h>
#endif
#define be16toh(x) ntohs((x))
#define htobe16(x) htons((x))
#define be32toh(x) ntohl((x))
#define htobe32(x) htonl((x))
uint32_t htole32(uint32_t);
uint64_t htole64(uint64_t);
#endif /* _WIN32 && !HAVE_ENDIAN_H */

#if (defined(__FreeBSD__) || defined(__MidnightBSD__)) && !defined(HAVE_ENDIAN_H)
#include <sys/endian.h>
#endif

#include <stdlib.h>
#include <string.h>

#if !defined(HAVE_STRLCAT)
size_t strlcat(char *, const char *, size_t);
#endif

#if !defined(HAVE_STRLCPY)
size_t strlcpy(char *, const char *, size_t);
#endif

#if !defined(HAVE_STRSEP)
char *strsep(char **, const char *);
#endif

#if !defined(HAVE_RECALLOCARRAY)
void *recallocarray(void *, size_t, size_t, size_t);
#endif

#if !defined(HAVE_EXPLICIT_BZERO)
void explicit_bzero(void *, size_t);
#endif

#if !defined(HAVE_FREEZERO)
void freezero(void *, size_t);
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

#if defined(_MSC_VER)
#define strerror_r(e, b, l) strerror_s((b), (l), (e))
#endif

#include "time.h"

#if !defined(HAVE_POSIX_IOCTL)
#define IOCTL_REQ(x)	(x)
#else
#define IOCTL_REQ(x)	((int)(x))
#endif

#if !defined(HAVE_ASPRINTF)
int asprintf(char **, const char *, ...);
#endif

#endif /* !_OPENBSD_COMPAT_H */
