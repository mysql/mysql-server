/* Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#include "mysys/my_kdf.h"

#include <openssl/evp.h>
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
#include <openssl/kdf.h>
#endif

#include <assert.h>
#include <cstring>
#include <memory>

const int max_kdf_iterations_size{65535};
const int min_kdf_iterations_size{1000};

/*
  return 0 on success and 1 on failure
*/
int create_kdf_key(const unsigned char *key, const unsigned int key_length,
                   unsigned char *rkey, unsigned int rkey_size,
                   vector<string> *kdf_options) {
  assert(kdf_options != nullptr);
  int nkdf_options = kdf_options->size();
  assert(nkdf_options > 0);
  if (nkdf_options < 1) {
    return 1;
  }

  string kdf_name = (*kdf_options)[0];
  std::unique_ptr<Key_derivation_function> kdf_function;

  if (kdf_name == "hkdf") {
#if OPENSSL_VERSION_NUMBER >= 0x10100000L
    kdf_function = std::make_unique<Key_hkdf_function>(kdf_options);
#else
    return 1;
#endif
  }
  if (kdf_name == "pbkdf2_hmac") {
    kdf_function = std::make_unique<Key_pbkdf2_hmac_function>(kdf_options);
  }
  if (kdf_function->validate_options()) {
    return 1;
  }
  return kdf_function->derive_key(key, key_length, rkey, rkey_size);
}

#if OPENSSL_VERSION_NUMBER >= 0x10100000L
Key_hkdf_function::Key_hkdf_function(vector<string> *kdf_options) {
  kdf_options_ = {kdf_options};
}

/*
  0 success
  1 error
*/
int Key_hkdf_function::validate_options() {
  int nkdf_options = kdf_options_->size();
  if (nkdf_options > 1) {
    salt_ = (*kdf_options_)[1];
  }
  if (nkdf_options > 2) {
    info_ = (*kdf_options_)[2];
  }
  options_valid_ = true;
  return 0;
}

int Key_hkdf_function::derive_key(const unsigned char *key,
                                  const unsigned int key_length,
                                  unsigned char *rkey, unsigned int key_size) {
  if (!options_valid_) return 1;

  EVP_PKEY_CTX *pctx{nullptr};

  /* Set initial key  */
  memset(rkey, 0, key_size);
  pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, nullptr);
  if (!pctx) {
    return 1;
  }
  if (EVP_PKEY_derive_init(pctx) <= 0) {
    EVP_PKEY_CTX_free(pctx);
  }
  if (EVP_PKEY_CTX_set_hkdf_md(pctx, EVP_sha512()) <= 0) {
    EVP_PKEY_CTX_free(pctx);
    return 1;
  }
  if (!salt_.empty()) {
    if (EVP_PKEY_CTX_set1_hkdf_salt(pctx,
                                    (const unsigned char *)(salt_.c_str()),
                                    salt_.length()) <= 0) {
      EVP_PKEY_CTX_free(pctx);
      return 1;
    }
  }
  if (!info_.empty()) {
    if (EVP_PKEY_CTX_add1_hkdf_info(pctx,
                                    (const unsigned char *)(info_.c_str()),
                                    info_.length()) <= 0) {
      EVP_PKEY_CTX_free(pctx);
      return 1;
    }
  }
  if (EVP_PKEY_CTX_set1_hkdf_key(pctx, key, key_length) <= 0) {
    EVP_PKEY_CTX_free(pctx);
    return 1;
  }
  size_t len{key_size};
  if (EVP_PKEY_derive(pctx, static_cast<unsigned char *>(rkey),
                      static_cast<size_t *>(&len)) <= 0) {
    EVP_PKEY_CTX_free(pctx);
    return 1;
  }
  if (len != key_size) {
    return 1;
  }
  if (pctx) {
    EVP_PKEY_CTX_free(pctx);
  }
  return 0;
}
#endif

Key_pbkdf2_hmac_function::Key_pbkdf2_hmac_function(
    vector<string> *kdf_options) {
  kdf_options_ = {kdf_options};
}

/*
  0 success
  1 error
*/
int Key_pbkdf2_hmac_function::validate_options() {
  int nkdf_options = kdf_options_->size();
  iterations_ = {min_kdf_iterations_size};

  if (nkdf_options > 1) {
    salt_ = (*kdf_options_)[1];
  }
  if (nkdf_options > 2) {
    string sIterations = (*kdf_options_)[2];
    iterations_ = atoi(sIterations.c_str());
  }
  if (iterations_ < min_kdf_iterations_size ||
      iterations_ > max_kdf_iterations_size) {
    return 1;
  }
  options_valid_ = true;
  return 0;
}

/*
  0 success
  1 error
*/
int Key_pbkdf2_hmac_function::derive_key(const unsigned char *key,
                                         const unsigned int key_length,
                                         unsigned char *rkey,
                                         unsigned int key_size) {
  if (!options_valid_) return 1;
  int res{0};
  res = PKCS5_PBKDF2_HMAC((const char *)key, key_length,
                          (const unsigned char *)salt_.c_str(), salt_.length(),
                          iterations_, EVP_sha512(), key_size, rkey);
  return res ? 0 : 1;
}
