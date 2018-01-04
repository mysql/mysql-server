/* Copyright (c) 2012, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   Without limiting anything contained in the foregoing, this file,
   which is part of C Driver for MySQL (Connector/C), is also subject to the
   Universal FOSS Exception, version 1.0, a copy of which can be found at
   http://oss.oracle.com/licenses/universal-foss-exception.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */


/**
  @file mysys_ssl/my_sha1.cc
  Wrapper functions for OpenSSL, YaSSL implementations. Also provides a
  Compatibility layer to make available YaSSL's SHA1 implementation.
*/

#include "my_inttypes.h"
#include "sha1.h"

#if defined(HAVE_YASSL)
#include <sha.hpp>

/**
  Compute SHA1 message digest using YaSSL.

  @param [out] digest   Computed SHA1 digest
  @param [in] buf       Message to be computed
  @param [in] len       Length of the message
*/
static void mysql_sha1_yassl(uint8 *digest, const char *buf, size_t len)
{
  TaoCrypt::SHA hasher;
  hasher.Update((const TaoCrypt::byte *) buf, (TaoCrypt::word32)len);
  hasher.Final ((TaoCrypt::byte *) digest);
}

/**
  Compute SHA1 message digest for two messages in order to
  emulate sha1(msg1, msg2) using YaSSL.

  @param [out] digest  Computed SHA1 digest
  @param [in] buf1     First message
  @param [in] len1     Length of first message
  @param [in] buf2     Second message
  @param [in] len2     Length of second message
*/
static void mysql_sha1_multi_yassl(uint8 *digest, const char *buf1, int len1,
                                   const char *buf2, int len2)
{
  TaoCrypt::SHA hasher;
  hasher.Update((const TaoCrypt::byte *) buf1, len1);
  hasher.Update((const TaoCrypt::byte *) buf2, len2);
  hasher.Final((TaoCrypt::byte *) digest);
}

#elif defined(HAVE_OPENSSL)
#include <openssl/sha.h>

static int mysql_sha1_reset(SHA_CTX *context)
{
    return SHA1_Init(context);
}


static int mysql_sha1_input(SHA_CTX *context, const uint8 *message_array,
                            unsigned length)
{
    return SHA1_Update(context, message_array, length);
}


static int mysql_sha1_result(SHA_CTX *context,
                             uint8 Message_Digest[SHA1_HASH_SIZE])
{
    return SHA1_Final(Message_Digest, context);
}

#endif /* HAVE_YASSL */

/**
  Wrapper function to compute SHA1 message digest.

  @param [out] digest  Computed SHA1 digest
  @param [in] buf      Message to be computed
  @param [in] len      Length of the message
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

  @param [out] digest  Computed SHA1 digest
  @param [in] buf1     First message
  @param [in] len1     Length of first message
  @param [in] buf2     Second message
  @param [in] len2     Length of second message
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

