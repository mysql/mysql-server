/*
   Copyright (c) 2022, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

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
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/x509v3.h>

static RSA *RSA_gen(unsigned int bits) {
  bool success = false;
  BIGNUM *exponent = nullptr;
  RSA *rsa = RSA_new();

  if (rsa) {
    exponent = BN_new();
    if (exponent)
      if (BN_set_word(exponent, RSA_F4))
        if (RSA_generate_key_ex(rsa, bits, exponent, nullptr)) success = true;
  }

  BN_free(exponent);

  if (!success) {
    RSA_free(rsa);
    rsa = nullptr;
  }

  return rsa;
}

/* Public exported functions */

EVP_PKEY *EVP_RSA_gen(unsigned int bits) {
  RSA *rsa = RSA_gen(bits);
  if (rsa) {
    EVP_PKEY *evp_pkey = EVP_PKEY_new();
    if (EVP_PKEY_assign_RSA(evp_pkey, rsa)) return evp_pkey;
    EVP_PKEY_free(evp_pkey);
  }
  return nullptr;
}

EVP_PKEY *EVP_EC_generate(const char *curve) {
  int nid = EC_curve_nist2nid(curve);
  if (nid != NID_undef) {
    EC_KEY *ec_key = EC_KEY_new_by_curve_name(nid);
    if (ec_key) {
      if (EC_KEY_generate_key(ec_key)) {
        EVP_PKEY *evp_pkey = EVP_PKEY_new();
        if (EVP_PKEY_assign_EC_KEY(evp_pkey, ec_key)) return evp_pkey;
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

/* Stub functions to allow NodeCertificate.cpp to compile with old OpenSSL */
#if OPENSSL_VERSION_NUMBER < NDB_TLS_MINIMUM_OPENSSL

const ASN1_INTEGER *X509_get0_serialNumber(const X509 *x) {
  return X509_get_serialNumber(const_cast<X509 *>(x));
}

EVP_PKEY *X509_get0_pubkey(X509 *x) {
  EVP_PKEY *key = X509_get_pubkey(x);
  if (key) EVP_PKEY_free(key);
  return key;
}

EVP_PKEY *X509_REQ_get0_pubkey(X509_REQ *csr) {
  EVP_PKEY *key = X509_REQ_get_pubkey(csr);
  if (key) EVP_PKEY_free(key);
  return key;
}

int X509_get_signature_info(X509 *, int *, int *, int *, uint32_t *) {
  return 0;
}

X509_EXTENSION *X509V3_EXT_conf_nid(LHASH_OF(CONF_VALUE) * conf,
                                    X509V3_CTX *ctx, int ext_nid,
                                    const char *value) {
  return X509V3_EXT_conf_nid(conf, ctx, ext_nid, const_cast<char *>(value));
}

int EVP_PKEY_up_ref(EVP_PKEY *) { return 0; }

int X509_up_ref(X509 *) { return 0; }

#ifdef __NEED_STUB_ASN1_FUNCTIONS
const ASN1_TIME *X509_get0_notBefore(const X509 *x) {
  return X509_get_notBefore(x);
}

const ASN1_TIME *X509_get0_notAfter(const X509 *x) {
  return X509_get_notAfter(x);
}

int ASN1_TIME_to_tm(const ASN1_TIME *, struct tm *) { return 0; }

int ASN1_INTEGER_get_uint64(uint64_t *v, const ASN1_INTEGER *) {
  *v = 0;
  return 0;
}

#endif  // __NEED_STUB_ASN1_FUNCTIONS

#endif
