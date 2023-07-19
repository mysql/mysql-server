/***************************************************************************
 *                                  _   _ ____  _
 *  Project                     ___| | | |  _ \| |
 *                             / __| | | | |_) | |
 *                            | (__| |_| |  _ <| |___
 *                             \___|\___/|_| \_\_____|
 *
 * Copyright (C) Daniel Stenberg, <daniel@haxx.se>, et al.
 *
 * This software is licensed as described in the file COPYING, which
 * you should have received as part of this distribution. The terms
 * are also available at https://curl.se/docs/copyright.html.
 *
 * You may opt to use, copy, modify, merge, publish, distribute and/or sell
 * copies of the Software, and permit persons to whom the Software is
 * furnished to do so, under the terms of the COPYING file.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 * SPDX-License-Identifier: curl
 *
 ***************************************************************************/

#include "curl_setup.h"

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_ARC4RANDOM
/* Some platforms might have the prototype missing (ubuntu + libressl) */
uint32_t arc4random(void);
#endif

#include <curl/curl.h>
#include "vtls/vtls.h"
#include "sendf.h"
#include "timeval.h"
#include "rand.h"

/* The last 3 #include files should be in this order */
#include "curl_printf.h"
#include "curl_memory.h"
#include "memdebug.h"

#ifdef WIN32

#if defined(__MINGW32__) && !defined(__MINGW64_VERSION_MAJOR)
#  define HAVE_MINGW_ORIGINAL
#endif

#if defined(_WIN32_WINNT) && _WIN32_WINNT >= 0x600 && \
  !defined(HAVE_MINGW_ORIGINAL)
#  define HAVE_WIN_BCRYPTGENRANDOM
#  include <bcrypt.h>
#  ifdef _MSC_VER
#    pragma comment(lib, "bcrypt.lib")
#  endif
#  ifndef BCRYPT_USE_SYSTEM_PREFERRED_RNG
#  define BCRYPT_USE_SYSTEM_PREFERRED_RNG 0x00000002
#  endif
#  ifndef STATUS_SUCCESS
#  define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#  endif
#elif defined(USE_WIN32_CRYPTO)
#  include <wincrypt.h>
#  ifdef _MSC_VER
#    pragma comment(lib, "advapi32.lib")
#  endif
#endif

CURLcode Curl_win32_random(unsigned char *entropy, size_t length)
{
  memset(entropy, 0, length);

#if defined(HAVE_WIN_BCRYPTGENRANDOM)
  if(BCryptGenRandom(NULL, entropy, (ULONG)length,
                     BCRYPT_USE_SYSTEM_PREFERRED_RNG) != STATUS_SUCCESS)
    return CURLE_FAILED_INIT;

  return CURLE_OK;
#elif defined(USE_WIN32_CRYPTO)
  {
    HCRYPTPROV hCryptProv = 0;

    if(!CryptAcquireContext(&hCryptProv, NULL, NULL, PROV_RSA_FULL,
                            CRYPT_VERIFYCONTEXT | CRYPT_SILENT))
      return CURLE_FAILED_INIT;

    if(!CryptGenRandom(hCryptProv, (DWORD)length, entropy)) {
      CryptReleaseContext(hCryptProv, 0UL);
      return CURLE_FAILED_INIT;
    }

    CryptReleaseContext(hCryptProv, 0UL);
  }
  return CURLE_OK;
#else
  return CURLE_NOT_BUILT_IN;
#endif
}
#endif

static CURLcode randit(struct Curl_easy *data, unsigned int *rnd)
{
  unsigned int r;
  CURLcode result = CURLE_OK;
  static unsigned int randseed;
  static bool seeded = FALSE;

#ifdef CURLDEBUG
  char *force_entropy = getenv("CURL_ENTROPY");
  if(force_entropy) {
    if(!seeded) {
      unsigned int seed = 0;
      size_t elen = strlen(force_entropy);
      size_t clen = sizeof(seed);
      size_t min = elen < clen ? elen : clen;
      memcpy((char *)&seed, force_entropy, min);
      randseed = ntohl(seed);
      seeded = TRUE;
    }
    else
      randseed++;
    *rnd = randseed;
    return CURLE_OK;
  }
#endif

  /* data may be NULL! */
  result = Curl_ssl_random(data, (unsigned char *)rnd, sizeof(*rnd));
  if(result != CURLE_NOT_BUILT_IN)
    /* only if there is no random function in the TLS backend do the non crypto
       version, otherwise return result */
    return result;

  /* ---- non-cryptographic version following ---- */

#ifdef WIN32
  if(!seeded) {
    result = Curl_win32_random((unsigned char *)rnd, sizeof(*rnd));
    if(result != CURLE_NOT_BUILT_IN)
      return result;
  }
#endif

#ifdef HAVE_ARC4RANDOM
  *rnd = (unsigned int)arc4random();
  return CURLE_OK;
#endif

#if defined(RANDOM_FILE) && !defined(WIN32)
  if(!seeded) {
    /* if there's a random file to read a seed from, use it */
    int fd = open(RANDOM_FILE, O_RDONLY);
    if(fd > -1) {
      /* read random data into the randseed variable */
      ssize_t nread = read(fd, &randseed, sizeof(randseed));
      if(nread == sizeof(randseed))
        seeded = TRUE;
      close(fd);
    }
  }
#endif

  if(!seeded) {
    struct curltime now = Curl_now();
    infof(data, "WARNING: using weak random seed");
    randseed += (unsigned int)now.tv_usec + (unsigned int)now.tv_sec;
    randseed = randseed * 1103515245 + 12345;
    randseed = randseed * 1103515245 + 12345;
    randseed = randseed * 1103515245 + 12345;
    seeded = TRUE;
  }

  /* Return an unsigned 32-bit pseudo-random number. */
  r = randseed = randseed * 1103515245 + 12345;
  *rnd = (r << 16) | ((r >> 16) & 0xFFFF);
  return CURLE_OK;
}

/*
 * Curl_rand() stores 'num' number of random unsigned characters in the buffer
 * 'rnd' points to.
 *
 * If libcurl is built without TLS support or with a TLS backend that lacks a
 * proper random API (rustls, Gskit or mbedTLS), this function will use "weak"
 * random.
 *
 * When built *with* TLS support and a backend that offers strong random, it
 * will return error if it cannot provide strong random values.
 *
 * NOTE: 'data' may be passed in as NULL when coming from external API without
 * easy handle!
 *
 */

CURLcode Curl_rand(struct Curl_easy *data, unsigned char *rnd, size_t num)
{
  CURLcode result = CURLE_BAD_FUNCTION_ARGUMENT;

  DEBUGASSERT(num > 0);

  while(num) {
    unsigned int r;
    size_t left = num < sizeof(unsigned int) ? num : sizeof(unsigned int);

    result = randit(data, &r);
    if(result)
      return result;

    while(left) {
      *rnd++ = (unsigned char)(r & 0xFF);
      r >>= 8;
      --num;
      --left;
    }
  }

  return result;
}

/*
 * Curl_rand_hex() fills the 'rnd' buffer with a given 'num' size with random
 * hexadecimal digits PLUS a null-terminating byte. It must be an odd number
 * size.
 */

CURLcode Curl_rand_hex(struct Curl_easy *data, unsigned char *rnd,
                       size_t num)
{
  CURLcode result = CURLE_BAD_FUNCTION_ARGUMENT;
  const char *hex = "0123456789abcdef";
  unsigned char buffer[128];
  unsigned char *bufp = buffer;
  DEBUGASSERT(num > 1);

#ifdef __clang_analyzer__
  /* This silences a scan-build warning about accessing this buffer with
     uninitialized memory. */
  memset(buffer, 0, sizeof(buffer));
#endif

  if((num/2 >= sizeof(buffer)) || !(num&1))
    /* make sure it fits in the local buffer and that it is an odd number! */
    return CURLE_BAD_FUNCTION_ARGUMENT;

  num--; /* save one for null-termination */

  result = Curl_rand(data, buffer, num/2);
  if(result)
    return result;

  while(num) {
    /* clang-tidy warns on this line without this comment: */
    /* NOLINTNEXTLINE(clang-analyzer-core.UndefinedBinaryOperatorResult) */
    *rnd++ = hex[(*bufp & 0xF0)>>4];
    *rnd++ = hex[*bufp & 0x0F];
    bufp++;
    num -= 2;
  }
  *rnd = 0;

  return result;
}
