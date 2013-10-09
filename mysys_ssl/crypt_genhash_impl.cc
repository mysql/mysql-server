/* Copyright (c) 2011, 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */


// First include (the generated) my_config.h, to get correct platform defines.
#include "my_config.h"

#ifdef HAVE_OPENSSL

#ifdef HAVE_YASSL
#include <sha.hpp>
#include <openssl/ssl.h>
#else
#include <openssl/sha.h>
#include <openssl/rand.h>
#endif

#include "crypt_genhash_impl.h"

/* Pre VS2010 compilers doesn't support stdint.h */
#ifdef HAVE_STDINT_H
#include <stdint.h>
#else
#ifndef uint32_t
typedef unsigned long uint32_t;
#endif
#ifndef uint8_t
typedef unsigned char uint8_t;
#endif
#endif // !HAVE_STDINT_H

#include <time.h>
#include <string.h>



#ifndef HAVE_YASSL
#define	DIGEST_CTX	SHA256_CTX
#define	DIGESTInit	SHA256_Init
#define	DIGESTUpdate	SHA256_Update
#define	DIGESTFinal	SHA256_Final
#define	DIGEST_LEN	SHA256_DIGEST_LENGTH
#else
#define DIGEST_CTX TaoCrypt::SHA256
#define DIGEST_LEN 32
void DIGESTInit(DIGEST_CTX *ctx)
{
  ctx->Init();
}

void DIGESTUpdate(DIGEST_CTX *ctx, const void *plaintext, int len)
{
  ctx->Update((const TaoCrypt::byte *)plaintext, len);
}

void DIGESTFinal(void *txt, DIGEST_CTX *ctx)
{
  ctx->Final((TaoCrypt::byte *)txt);
}

#endif // HAVE_YASSL

static const char crypt_alg_magic[] = "$5";

#ifndef MAX
#define MAX(a, b)  (((a) > (b)) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))
#endif


/**
  Size-bounded string copying and concatenation
  This is a replacement for STRLCPY(3)
*/

size_t
strlcat(char *dst, const char *src, size_t siz)
{
  char *d= dst;
  const char *s= src;
  size_t n= siz;
  size_t dlen;
  /* Find the end of dst and adjust bytes left but don't go past end */
  while (n-- != 0 && *d != '\0')
    d++;
  dlen= d - dst;
  n= siz - dlen;
  if (n == 0)
    return(dlen + siz);
  while (*s != '\0')
  {
    if (n != 1)
    {
      *d++= *s;
      n--;
    }
    s++;
  }
  *d= '\0';
  return(dlen + (s - src));       /* count does not include NUL */
}

static const int crypt_alg_magic_len = sizeof (crypt_alg_magic) - 1;

static unsigned char b64t[] =		/* 0 ... 63 => ascii - 64 */
	"./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

#define	b64_from_24bit(B2, B1, B0, N) \
{ \
	uint32_t w = ((B2) << 16) | ((B1) << 8) | (B0); \
	int n = (N); \
	while (--n >= 0 && ctbufflen > 0) { \
		*p++ = b64t[w & 0x3f]; \
		w >>= 6; \
		ctbufflen--; \
} \
}

#define	ROUNDS		"rounds="
#define	ROUNDSLEN	(sizeof (ROUNDS) - 1)

/**
  Get the integer value after rounds= where ever it occurs in the string.
  if the last char after the int is a , or $ that is fine anything else is an
  error.
*/
static uint getrounds(const char *s)
{
  const char *r;
  const char *p;
  char *e;
  long val;

  if (s == NULL)
    return (0);

  if ((r = strstr(s, ROUNDS)) == NULL)
  {
    return (0);
  }

  if (strncmp(r, ROUNDS, ROUNDSLEN) != 0)
  {
    return (0);
  }

  p= r + ROUNDSLEN;
  errno= 0;
  val= strtol(p, &e, 10);
  /*
    An error occurred or there is non-numeric stuff at the end
    which isn't one of the crypt(3c) special chars ',' or '$'
  */
  if (errno != 0 || val < 0 || !(*e == '\0' || *e == ',' || *e == '$'))
  {
    return (0);
  }

  return ((uint32_t) val);
}

/**
  Finds the interval which envelopes the user salt in a crypt password
  The crypt format is assumed to be $a$bbbb$cccccc\0 and the salt is found
  by counting the delimiters and marking begin and end.

   @param salt_being[in]  Pointer to start of crypt passwd
   @param salt_being[out] Pointer to first byte of the salt
   @param salt_end[in]    Pointer to the last byte in passwd
   @param salt_end[out]   Pointer to the byte immediatly following the salt ($)

   @return The size of the salt identified
*/

int extract_user_salt(char **salt_begin,
                      char **salt_end)
{
  char *it= *salt_begin;
  int delimiter_count= 0;
  while(it != *salt_end)
  {
    if (*it == '$')
    {
      ++delimiter_count;
      if (delimiter_count == 2)
      {
        *salt_begin= it + 1;
      }
      if (delimiter_count == 3)
        break;
    }
    ++it;
  }
  *salt_end= it;
  return *salt_end - *salt_begin;
}

const char *sha256_find_digest(char *pass)
{
  int sz= strlen(pass);
  return pass + sz - SHA256_HASH_LENGTH;
}

/*
 * Portions of the below code come from crypt_bsdmd5.so (bsdmd5.c) :
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dknet.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $FreeBSD: crypt.c,v 1.5 1996/10/14 08:34:02 phk Exp $
 *
 */

/*
 * The below code implements the specification from:
 *
 * From http://people.redhat.com/drepper/SHA-crypt.txt
 *
 * Portions of the code taken from inspired by or verified against the
 * source in the above document which is licensed as:
 *
 * "Released into the Public Domain by Ulrich Drepper <drepper@redhat.com>."
 */
 
/*
  Due to a Solaris namespace bug DS is a reserved word. To work around this
  DS is undefined.
*/
#undef DS

/* ARGSUSED4 */
extern "C"
char *
my_crypt_genhash(char *ctbuffer,
                   size_t ctbufflen,
                   const char *plaintext,
                   int plaintext_len,
                   const char *switchsalt,
                   const char **params)
{
  int salt_len, i;
  char *salt;
  unsigned char A[DIGEST_LEN];
  unsigned char B[DIGEST_LEN];
  unsigned char DP[DIGEST_LEN];
  unsigned char DS[DIGEST_LEN];
  DIGEST_CTX ctxA, ctxB, ctxC, ctxDP, ctxDS;
  int rounds = ROUNDS_DEFAULT;
  int srounds = 0;
  bool custom_rounds= false;
  char *p;
  char *P, *Pp;
  char *S, *Sp;

  /* Refine the salt */
  salt = (char *)switchsalt;

  /* skip our magic string */
  if (strncmp((char *)salt, crypt_alg_magic, crypt_alg_magic_len) == 0)
  {
          salt += crypt_alg_magic_len + 1;
  }

  srounds = getrounds(salt);
  if (srounds != 0) {
          rounds = MAX(ROUNDS_MIN, MIN(srounds, ROUNDS_MAX));
          custom_rounds= true;
          p = strchr(salt, '$');
          if (p != NULL)
                  salt = p + 1;
  }

  salt_len = MIN(strcspn(salt, "$"), CRYPT_SALT_LENGTH);
  //plaintext_len = strlen(plaintext);

  /* 1. */
  DIGESTInit(&ctxA);

  /* 2. The password first, since that is what is most unknown */
  DIGESTUpdate(&ctxA, plaintext, plaintext_len);

  /* 3. Then the raw salt */
  DIGESTUpdate(&ctxA, salt, salt_len);

  /* 4. - 8. */
  DIGESTInit(&ctxB);
  DIGESTUpdate(&ctxB, plaintext, plaintext_len);
  DIGESTUpdate(&ctxB, salt, salt_len);
  DIGESTUpdate(&ctxB, plaintext, plaintext_len);
  DIGESTFinal(B, &ctxB);

  /* 9. - 10. */
  for (i= plaintext_len; i > MIXCHARS; i -= MIXCHARS)
    DIGESTUpdate(&ctxA, B, MIXCHARS);
  DIGESTUpdate(&ctxA, B, i);

  /* 11. */
  for (i= plaintext_len; i > 0; i >>= 1) {
    if ((i & 1) != 0)
    {
      DIGESTUpdate(&ctxA, B, MIXCHARS);
    }
    else
    {
      DIGESTUpdate(&ctxA, plaintext, plaintext_len);
    }
  }

  /* 12. */
  DIGESTFinal(A, &ctxA);

  /* 13. - 15. */
  DIGESTInit(&ctxDP);
  for (i= 0; i < plaintext_len; i++)
          DIGESTUpdate(&ctxDP, plaintext, plaintext_len);
  DIGESTFinal(DP, &ctxDP);

  /* 16. */
  Pp= P= (char *)alloca(plaintext_len);
  for (i= plaintext_len; i >= MIXCHARS; i -= MIXCHARS)
  {
          Pp= (char *)(memcpy(Pp, DP, MIXCHARS)) + MIXCHARS;
  }
  (void) memcpy(Pp, DP, i);

  /* 17. - 19. */
  DIGESTInit(&ctxDS);
  for (i= 0; i < 16 + (uint8_t)A[0]; i++)
          DIGESTUpdate(&ctxDS, salt, salt_len);
  DIGESTFinal(DS, &ctxDS);

  /* 20. */
  Sp= S= (char *)alloca(salt_len);
  for (i= salt_len; i >= MIXCHARS; i -= MIXCHARS)
  {
          Sp= (char *)(memcpy(Sp, DS, MIXCHARS)) + MIXCHARS;
  }
  (void) memcpy(Sp, DS, i);

  /*  21. */
  for (i= 0; i < rounds; i++)
  {
  DIGESTInit(&ctxC);

    if ((i & 1) != 0)
    {
      DIGESTUpdate(&ctxC, P, plaintext_len);
    }
    else
    {
      if (i == 0)
        DIGESTUpdate(&ctxC, A, MIXCHARS);
      else
        DIGESTUpdate(&ctxC, DP, MIXCHARS);
    }

    if (i % 3 != 0) {
      DIGESTUpdate(&ctxC, S, salt_len);
    }

    if (i % 7 != 0) {
      DIGESTUpdate(&ctxC, P, plaintext_len);
    }

    if ((i & 1) != 0)
    {
      if (i == 0)
        DIGESTUpdate(&ctxC, A, MIXCHARS);
      else
        DIGESTUpdate(&ctxC, DP, MIXCHARS);
    }
    else
    {
        DIGESTUpdate(&ctxC, P, plaintext_len);
    }
    DIGESTFinal(DP, &ctxC);
  }

  /* 22. Now make the output string */
  if (custom_rounds)
  {
    (void) snprintf(ctbuffer, ctbufflen,
                    "%s$rounds=%zu$", crypt_alg_magic, (size_t)rounds);
  }
  else
  {
    (void) snprintf(ctbuffer, ctbufflen,
                    "%s$", crypt_alg_magic);
  }
  (void) strncat(ctbuffer, (const char *)salt, salt_len);
  (void) strlcat(ctbuffer, "$", ctbufflen);

  p= ctbuffer + strlen(ctbuffer);
  ctbufflen -= strlen(ctbuffer);

  b64_from_24bit(DP[ 0], DP[10], DP[20], 4);
  b64_from_24bit(DP[21], DP[ 1], DP[11], 4);
  b64_from_24bit(DP[12], DP[22], DP[ 2], 4);
  b64_from_24bit(DP[ 3], DP[13], DP[23], 4);
  b64_from_24bit(DP[24], DP[ 4], DP[14], 4);
  b64_from_24bit(DP[15], DP[25], DP[ 5], 4);
  b64_from_24bit(DP[ 6], DP[16], DP[26], 4);
  b64_from_24bit(DP[27], DP[ 7], DP[17], 4);
  b64_from_24bit(DP[18], DP[28], DP[ 8], 4);
  b64_from_24bit(DP[ 9], DP[19], DP[29], 4);
  b64_from_24bit(0, DP[31], DP[30], 3);
  *p= '\0';

  (void) memset(A, 0, sizeof (A));
  (void) memset(B, 0, sizeof (B));
  (void) memset(DP, 0, sizeof (DP));
  (void) memset(DS, 0, sizeof (DS));

  return (ctbuffer);
}


/**
  Generate a random string using ASCII characters but avoid seperator character.
  Stdlib rand and srand are used to produce pseudo random numbers between 
  with about 7 bit worth of entropty between 1-127.
*/
extern "C"
void generate_user_salt(char *buffer, int buffer_len)
{
  char *end= buffer + buffer_len - 1;
#ifdef HAVE_YASSL
  yaSSL::RAND_bytes((unsigned char *) buffer, buffer_len);
#else
  RAND_bytes((unsigned char *) buffer, buffer_len);
#endif
      
  /* Sequence must be a legal UTF8 string */
  for (; buffer < end; buffer++)
  { 
    *buffer &= 0x7f;
    if (*buffer == '\0' || *buffer == '$')
      *buffer= *buffer + 1;
  }
  /* Make sure the buffer is terminated properly */
  *end= '\0';
}

void xor_string(char *to, int to_len, char *pattern, int pattern_len)
{
  int loop= 0;
  while(loop <= to_len)
  {
    *(to + loop) ^= *(pattern + loop % pattern_len);
    ++loop;
  }
}

#endif // HAVE_OPENSSL
