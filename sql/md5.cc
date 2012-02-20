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
  Wrapper functions for OpenSSL, YaSSL and MySQL's MD5
  implementations. Also provides a Compatibility layer
  to make available YaSSL's MD5 implementation.
*/

#include <my_global.h>
#include <my_md5.h>

#ifdef HAVE_YASSL

#include "md5.hpp"

/**
  Compute MD5 message digest.

  @param digest [out]  Computed MD5 digest
  @param buf    [in]   Message to be computed
  @param len    [in]   Length of the message

  @return              void
*/
void my_md5_hash(char *digest, const char *buf, int len)
{
  TaoCrypt::MD5 hasher;
  hasher.Update((TaoCrypt::byte *) buf, len);
  hasher.Final((TaoCrypt::byte *) digest);
}
#endif /* HAVE_YASSL */


/**
    Wrapper function to compute MD5 message digest.

    @param digest [out]  Computed MD5 digest
    @param buf    [in]   Message to be computed
    @param len    [in]   Length of the message

    @return              void
*/
void compute_md5_hash(char *digest, const char *buf, int len)
{
#ifdef HAVE_YASSL
  my_md5_hash(digest, buf, len);
#else
  MY_MD5_HASH((unsigned char *) digest, (unsigned char const *) buf, len);
#endif /* HAVE_YASSL */
}

