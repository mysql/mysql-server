/*
Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef X_SHA256_SCRAMBLE_GENERATOR_H_
#define X_SHA256_SCRAMBLE_GENERATOR_H_

#include <memory>
#include <string>

#include <openssl/evp.h>
#include "openssl/ossl_typ.h"
#include "sha2.h" /* SHA256_DIGEST_LENGTH */

namespace xcl {
namespace sha256_password {

/* Digest length for caching_sha2_authentication plugin */
const std::uint32_t CACHING_SHA2_DIGEST_LENGTH = SHA256_DIGEST_LENGTH;

/**
  Supported digest information
*/
enum class Digest_info { SHA256_DIGEST = 0, DIGEST_LAST };

/**
  Interface for cryptographic digest generation
*/
class Generate_digest {
 public:
  virtual bool update_digest(const void *src, const std::uint32_t length) = 0;
  virtual bool retrieve_digest(unsigned char *digest,
                               const std::uint32_t length) = 0;
  virtual void scrub() = 0;
  virtual ~Generate_digest() = default;
};

/**
  SHA256 digest generator
*/
class SHA256_digest : public Generate_digest {
 public:
  SHA256_digest();
  ~SHA256_digest();

  bool update_digest(const void *src, std::uint32_t length) override;
  bool retrieve_digest(unsigned char *digest, std::uint32_t length) override;
  void scrub() override;
  bool all_ok() const { return m_ok; }

 private:
  void init();
  void deinit();

 private:
  /** Digest output buffer */
  unsigned char m_digest[CACHING_SHA2_DIGEST_LENGTH];
  /** Digest context */
  EVP_MD_CTX *md_context;
  /** Status */
  bool m_ok;
};

/**
  Scramble generator
  Responsible for generating scramble of following format:
  XOR(SHA2(m_src), SHA2(SHA2(SHA2(m_src)), m_rnd))
*/
class Generate_scramble {
 public:
  Generate_scramble(const std::string &source, const std::string &rnd,
                    Digest_info digest_type = Digest_info::SHA256_DIGEST);

  bool scramble(unsigned char *out_scramble,
                const std::uint32_t scramble_length);

 private:
  /** plaintext source string */
  std::string m_src;
  /** random string */
  std::string m_rnd;
  /** Type of digest */
  const Digest_info m_digest_type;
  /** Digest generator class */
  std::unique_ptr<Generate_digest> m_digest_generator;
  /** length of the digest */
  std::uint32_t m_digest_length;
};

}  // namespace sha256_password

bool generate_sha256_scramble(unsigned char *out_scramble,
                              const std::size_t scramble_size, const char *src,
                              const std::size_t src_size, const char *salt,
                              const std::size_t salt_size);

}  // namespace xcl

#endif  // X_SHA256_SCRAMBLE_GENERATOR_H_
