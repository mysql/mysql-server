/* Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */


/**
  @file

  @brief
  Wrapper functions for OpenSSL, YaSSL implementations. Also provides a
  Compatibility layer to make available YaSSL's SHA1 implementation.
*/

#include <my_global.h>
#include <sha1.h>

#if defined(HAVE_YASSL)
#include "sha.hpp"

/**
  Compute SHA1 message digest using YaSSL.

  @param digest [out]  Computed SHA1 digest
  @param buf    [in]   Message to be computed
  @param len    [in]   Length of the message

  @return              void
*/
void mysql_sha1_yassl(uint8 *digest, const char *buf, size_t len)
{
  TaoCrypt::SHA hasher;
  hasher.Update((const TaoCrypt::byte *) buf, (TaoCrypt::word32)len);
  hasher.Final ((TaoCrypt::byte *) digest);
}

/**
  Compute SHA1 message digest for two messages in order to
  emulate sha1(msg1, msg2) using YaSSL.

  @param digest [out]  Computed SHA1 digest
  @param buf1   [in]   First message
  @param len1   [in]   Length of first message
  @param buf2   [in]   Second message
  @param len2   [in]   Length of second message

  @return              void
*/
void mysql_sha1_multi_yassl(uint8 *digest, const char *buf1, int len1,
                            const char *buf2, int len2)
{
  TaoCrypt::SHA hasher;
  hasher.Update((const TaoCrypt::byte *) buf1, len1);
  hasher.Update((const TaoCrypt::byte *) buf2, len2);
  hasher.Final((TaoCrypt::byte *) digest);
}

#elif defined(HAVE_OPENSSL)
#include <openssl/sha.h>

int mysql_sha1_reset(SHA_CTX *context)
{
    return SHA1_Init(context);
}


int mysql_sha1_input(SHA_CTX *context, const uint8 *message_array,
                     unsigned length)
{
    return SHA1_Update(context, message_array, length);
}


int mysql_sha1_result(SHA_CTX *context,
                      uint8 Message_Digest[SHA1_HASH_SIZE])
{
    return SHA1_Final(Message_Digest, context);
}

#endif /* HAVE_YASSL */

/**
  Wrapper function to compute SHA1 message digest.

  @param digest [out]  Computed SHA1 digest
  @param buf    [in]   Message to be computed
  @param len    [in]   Length of the message

  @return              void
*/
void compute_sha1_hash(uint8 *digest, const char *buf, size_t len)
{
#if defined(HAVE_YASSL)
  mysql_sha1_yassl(digest, buf, len);
#elif defined(HAVE_OPENSSL)
  SHA_CTX sha1_context;

  mysql_sha1_reset(&sha1_context);
  mysql_sha1_input(&sha1_context, (const uint8 *) buf, len);
  mysql_sha1_result(&sha1_context, digest);
#endif /* HAVE_YASSL */
}


/**
  Wrapper function to compute SHA1 message digest for
  two messages in order to emulate sha1(msg1, msg2).

  @param digest [out]  Computed SHA1 digest
  @param buf1   [in]   First message
  @param len1   [in]   Length of first message
  @param buf2   [in]   Second message
  @param len2   [in]   Length of second message

  @return              void
*/
void compute_sha1_hash_multi(uint8 *digest, const char *buf1, int len1,
                             const char *buf2, int len2)
{
#if defined(HAVE_YASSL)
  mysql_sha1_multi_yassl(digest, buf1, len1, buf2, len2);
#elif defined(HAVE_OPENSSL)
  SHA_CTX sha1_context;

  mysql_sha1_reset(&sha1_context);
  mysql_sha1_input(&sha1_context, (const uint8 *) buf1, len1);
  mysql_sha1_input(&sha1_context, (const uint8 *) buf2, len2);
  mysql_sha1_result(&sha1_context, digest);
#endif /* HAVE_YASSL */
}

