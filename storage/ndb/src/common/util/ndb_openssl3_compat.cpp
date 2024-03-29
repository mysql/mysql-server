/*
   Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/


/*
  Enable NDB code to use OpenSSL 3 APIs unconditionally
  with any OpenSSL version starting from 1.0.2
*/

#include "util/ndb_openssl3_compat.h"

#include <openssl/ssl.h>

#if OPENSSL_VERSION_NUMBER < 0x30000000L && OPENSSL_VERSION_NUMBER > 0x10002000L

#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/x509v3.h>
#include <openssl/rand.h>
#include <openssl/evp.h>

static RSA * RSA_gen(unsigned int bits) {
  bool success = false;
  BIGNUM * exponent = nullptr;
  RSA * rsa = RSA_new();

  if (rsa) {
    exponent = BN_new();
    if(exponent)
      if(BN_set_word(exponent, RSA_F4))
        if(RSA_generate_key_ex(rsa, bits, exponent, 0))
          success = true;
  }

  BN_free(exponent);

  if(! success) {
    RSA_free(rsa);
    rsa = nullptr;
  }

  return rsa;
}

/* Public exported functions */

EVP_PKEY * EVP_RSA_gen(unsigned int bits) {
  RSA * rsa = RSA_gen(bits);
  if(rsa) {
    EVP_PKEY * evp_pkey = EVP_PKEY_new();
    if(EVP_PKEY_assign_RSA(evp_pkey, rsa))
      return evp_pkey;
    EVP_PKEY_free(evp_pkey);
  }
  return nullptr;
}

EVP_PKEY * EVP_EC_generate(const char * curve) {
  int nid = EC_curve_nist2nid(curve);
  if(nid != NID_undef) {
    EC_KEY * ec_key = EC_KEY_new_by_curve_name(nid);
    if(ec_key) {
      if(EC_KEY_generate_key(ec_key)) {
        EVP_PKEY * evp_pkey = EVP_PKEY_new();
        if(EVP_PKEY_assign_EC_KEY(evp_pkey, ec_key))
          return evp_pkey;
        EVP_PKEY_free(evp_pkey);
      }
      EC_KEY_free(ec_key);
    }
  }
  return nullptr;
}

int EVP_PKEY_eq(const EVP_PKEY *a, const EVP_PKEY *b) {
  return EVP_PKEY_cmp(a, b);
}

#endif

