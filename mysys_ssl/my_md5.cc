<<<<<<< HEAD
/* Copyright (c) 2012, 2018, Oracle and/or its affiliates. All rights reserved.
=======
/* Copyright (c) 2012, 2023, Oracle and/or its affiliates.
>>>>>>> upstream/cluster-7.6

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
<<<<<<< HEAD
  @file mysys_ssl/my_md5.cc
  Wrapper functions for OpenSSL and wolfSSL.
=======
  @file

  @brief
  Wrapper functions for OpenSSL.
>>>>>>> upstream/cluster-7.6
*/

#include "my_md5.h"

<<<<<<< HEAD
#include <openssl/crypto.h>
=======
#if defined(HAVE_OPENSSL)
>>>>>>> upstream/cluster-7.6
#include <openssl/md5.h>

static void my_md5_hash(unsigned char *digest, unsigned const char *buf,
                        int len) {
  MD5_CTX ctx;
  MD5_Init(&ctx);
  MD5_Update(&ctx, buf, len);
  MD5_Final(digest, &ctx);
}

<<<<<<< HEAD
=======
#endif /* HAVE_OPENSSL */

>>>>>>> upstream/cluster-7.6
/**
    Wrapper function to compute MD5 message digest.

    @param [out] digest Computed MD5 digest
    @param [in] buf     Message to be computed
    @param [in] len     Length of the message
    @return             0 when MD5 hash function called successfully
                        1 when MD5 hash function doesn't called because of fips
   mode (ON/STRICT)
*/
<<<<<<< HEAD
int compute_md5_hash(char *digest, const char *buf, int len) {
  int retval = 0;
  int fips_mode = 0;
#if !defined(HAVE_WOLFSSL)
  fips_mode = FIPS_mode();
#endif /* HAVE_WOLFSSL */
  /* If fips mode is ON/STRICT restricted method calls will result into abort,
   * skipping call. */
  if (fips_mode == 0) {
    my_md5_hash((unsigned char *)digest, (unsigned const char *)buf, len);
  } else {
    retval = 1;
  }
  return retval;
=======
void compute_md5_hash(char *digest, const char *buf, int len)
{
#if defined(HAVE_OPENSSL)
  my_md5_hash((unsigned char*)digest, (unsigned const char*)buf, len);
#endif /* HAVE_OPENSSL */
>>>>>>> upstream/cluster-7.6
}
