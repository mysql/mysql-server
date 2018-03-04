/* Copyright (c) 2007, 2017, Oracle and/or its affiliates. All rights reserved.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2.0,
 as published by the Free Software Foundation.

 This program is also distributed with certain software (including
 but not limited to OpenSSL) that is licensed under separate terms,
 as designated in a particular file or component or in included license
 documentation.  The authors of MySQL hereby grant you an additional
 permission to link the program and your derivative works with the
 separately licensed software that they have included with MySQL.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef included_sha2_h
#define included_sha2_h

/**
  @file include/sha2.h
*/

#if defined(HAVE_YASSL) || defined(HAVE_OPENSSL)

#include <stddef.h>

#  ifndef HAVE_YASSL
#    include <openssl/sha.h>

#  else

#include <sha.hpp>

#    ifdef __cplusplus
extern "C" {
#    endif

#ifndef SHA512_DIGEST_LENGTH
#define SHA512_DIGEST_LENGTH TaoCrypt::SHA512::DIGEST_SIZE
#endif

#ifndef SHA384_DIGEST_LENGTH
#define SHA384_DIGEST_LENGTH TaoCrypt::SHA384::DIGEST_SIZE
#endif

#ifndef SHA256_DIGEST_LENGTH
#define SHA256_DIGEST_LENGTH TaoCrypt::SHA256::DIGEST_SIZE
#endif

#ifndef SHA224_DIGEST_LENGTH
#define SHA224_DIGEST_LENGTH TaoCrypt::SHA224::DIGEST_SIZE
#endif

#define GEN_YASSL_SHA2_BRIDGE(size) \
unsigned char* SHA##size(const unsigned char *input_ptr, size_t input_length, \
               char unsigned *output_ptr);

GEN_YASSL_SHA2_BRIDGE(512)
GEN_YASSL_SHA2_BRIDGE(384)
GEN_YASSL_SHA2_BRIDGE(256)
GEN_YASSL_SHA2_BRIDGE(224)

#undef GEN_YASSL_SHA2_BRIDGE

#    ifdef __cplusplus
}
#    endif

#  endif /* HAVE_YASSL */

#endif /* HAVE_OPENSSL || HAVE_YASSL */
#endif /* included_sha2_h */
