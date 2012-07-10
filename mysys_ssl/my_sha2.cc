/* Copyright (c) 2007, 2012, Oracle and/or its affiliates. All rights reserved.

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
  A compatibility layer to our built-in SSL implementation, to mimic the
  oft-used external library, OpenSSL.
*/

#include <my_global.h>
#include <sha2.h>

#ifdef HAVE_YASSL

/*
  If TaoCrypt::SHA512 or ::SHA384 are not defined (but ::SHA256 is), it's
  probably that neither of config.h's SIZEOF_LONG or SIZEOF_LONG_LONG are
  64 bits long.  At present, both OpenSSL and YaSSL require 64-bit integers
  for SHA-512.  (The SIZEOF_* definitions come from autoconf's config.h .)
*/

#  define GEN_YASSL_SHA2_BRIDGE(size) \
unsigned char* SHA##size(const unsigned char *input_ptr, size_t input_length, \
               char unsigned *output_ptr) {                         \
  TaoCrypt::SHA##size hasher;                                       \
                                                                    \
  hasher.Update(input_ptr, input_length);                           \
  hasher.Final(output_ptr);                                         \
  return(output_ptr);                                               \
}


/**
  @fn SHA512
  @fn SHA384
  @fn SHA256
  @fn SHA224

  Instantiate an hash object, fill in the cleartext value, compute the digest,
  and extract the result from the object.
  
  (Generate the functions.  See similar .h code for the prototypes.)
*/
#  ifndef OPENSSL_NO_SHA512
GEN_YASSL_SHA2_BRIDGE(512);
GEN_YASSL_SHA2_BRIDGE(384);
#  else
#    warning Some SHA2 functionality is missing.  See OPENSSL_NO_SHA512.
#  endif
GEN_YASSL_SHA2_BRIDGE(256);
GEN_YASSL_SHA2_BRIDGE(224);

#  undef GEN_YASSL_SHA2_BRIDGE

#endif /* HAVE_YASSL */
