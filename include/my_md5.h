#ifndef MY_MD5_INCLUDED
#define MY_MD5_INCLUDED

/* Copyright (C) 2000 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/* See md5.c for explanation and copyright information.  */

/*
 * $FreeBSD: src/contrib/cvs/lib/md5.h,v 1.2 1999/12/11 15:10:02 peter Exp $
 */

#if defined(HAVE_YASSL) || defined(HAVE_OPENSSL)
/*
  Use MD5 implementation provided by the SSL libraries.
*/

#if defined(HAVE_YASSL)

#ifdef __cplusplus
extern "C" {
#endif

void my_md5_hash(char *digest, const char *buf, int len);

#ifdef __cplusplus
}
#endif


#else /* HAVE_YASSL */

#include <openssl/md5.h>

#define MY_MD5_HASH(digest, buf, len) \
do { \
  MD5_CTX ctx; \
  MD5_Init (&ctx); \
  MD5_Update (&ctx, buf, len); \
  MD5_Final (digest, &ctx); \
} while (0)

#endif /* HAVE_YASSL */

#else /* HAVE_YASSL || HAVE_OPENSSL */
/* Fallback to the MySQL's implementation. */

/* Unlike previous versions of this code, uint32 need not be exactly
   32 bits, merely 32 bits or more.  Choosing a data type which is 32
   bits instead of 64 is not important; speed is considerably more
   important.  ANSI guarantees that "unsigned long" will be big enough,
   and always using it seems to have few disadvantages.  */

#include "my_global.h"
typedef uint32 cvs_uint32;

typedef struct {
  cvs_uint32 buf[4];
  cvs_uint32 bits[2];
  unsigned char in[64];
} my_MD5Context;

#ifdef __cplusplus
extern "C" {
#endif

void my_MD5Init (my_MD5Context *context);
void my_MD5Update (my_MD5Context *context,
                   unsigned char const *buf, unsigned len);
void my_MD5Final (unsigned char digest[16],
                  my_MD5Context *context);

#ifdef __cplusplus
}
#endif


#define MY_MD5_HASH(digest,buf,len) \
do { \
  my_MD5Context ctx; \
  my_MD5Init (&ctx); \
  my_MD5Update (&ctx, buf, len); \
  my_MD5Final (digest, &ctx); \
} while (0)

#endif /* defined(HAVE_YASSL) || defined(HAVE_OPENSSL) */

#ifdef __cplusplus
extern "C" {
#endif

void compute_md5_hash(char *digest, const char *buf, int len);

#ifdef __cplusplus
}
#endif

#endif /* MY_MD5_INCLUDED */
