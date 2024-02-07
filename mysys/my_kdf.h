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

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <openssl/conf.h>
#include <string>
#include <vector>

using std::string;
using std::vector;

/**
  Creates the required size of key using supplied key and KDF options.

  KDF: key derivation function (KDF) is a cryptographic algorithm that derives
  one or more secret keys from a secret value such as a main key, a password, or
  a passphrase using a pseudorandom function (which typically uses a
  cryptographic hash function or block cipher)

  @param key Input key
  @param key_length Input key length
  @param [out] rkey output key
  @param rkey_size output key length
  @param kdf_options  KDF function options

  @return 0 on success and 1 on failure
*/
int create_kdf_key(const unsigned char *key, const unsigned int key_length,
                   unsigned char *rkey, unsigned int rkey_size,
                   vector<string> *kdf_options);

class Key_derivation_function {
 protected:
  vector<string> *kdf_options_{nullptr};
  bool options_valid_{false};

 public:
  virtual ~Key_derivation_function() {}
  virtual int derive_key(const unsigned char *key,
                         const unsigned int key_length, unsigned char *rkey,
                         unsigned int key_size) = 0;
  virtual int validate_options() = 0;
};

#if OPENSSL_VERSION_NUMBER >= 0x10100000L

/** Class to implement KDF method hkdf. */
class Key_hkdf_function : public Key_derivation_function {
  string salt_;
  string info_;

 public:
  /**
     hkdf Constructor.

     @param kdf_options options

     kdf_options has following KDF options:

     1. KDF function name

     2. KDF salt: The salt. Salts prevent attacks based on dictionaries of
     common passwords and attacks based on rainbow tables. It is a public value
     that can be safely stored along with the encryption key.

     3. KDF info: The context and application specific information.
  */
  Key_hkdf_function(vector<string> *kdf_options);
  virtual ~Key_hkdf_function() override {}
  int derive_key(const unsigned char *key, const unsigned int key_length,
                 unsigned char *rkey, unsigned int key_size) override;
  int validate_options() override;
};
#endif

/** Class to implement KDF method pbkdf2_hmac. */
class Key_pbkdf2_hmac_function : public Key_derivation_function {
  string salt_;
  int iterations_{0};

 public:
  /**
     pbkdf2_hmac Constructor.

     @param kdf_options options

     kdf_options has following KDF options:

     1. KDF function name

     2. KDF salt: The salt. Salts prevent attacks based on dictionaries of
     common passwords and attacks based on rainbow tables. It is a public value
     that can be safely stored along with the encryption key.

     3. KDF info: The iteration count.
     This provides the ability to tune the algorithm.
     It is better to use the highest count possible for the maximum resistance
     to brute-force attacks.
  */
  Key_pbkdf2_hmac_function(vector<string> *kdf_options);
  virtual ~Key_pbkdf2_hmac_function() override {}
  int derive_key(const unsigned char *key, const unsigned int key_length,
                 unsigned char *rkey, unsigned int key_size) override;
  int validate_options() override;
};
