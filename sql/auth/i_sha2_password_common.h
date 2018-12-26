/* Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef I_SHA2_PASSWORD_COMMON_INCLUDED
#define I_SHA2_PASSWORD_COMMON_INCLUDED

#include <openssl/evp.h>
#if !defined(HAVE_YASSL)
#include <openssl/ossl_typ.h>
#endif /* !HAVE_YASSL */
#include "sha2.h" /* SHA256_DIGEST_LENGTH */

#include <string>

/**
  @file sql/auth/i_sha2_password_common.h
  Classes for caching_sha2_authentication plugin
*/

/**
  @defgroup auth_caching_sha2_auth caching_sha2_authentication information
  @{
*/
namespace sha2_password {
/* Digest length for caching_sha2_authentication plugin */
const unsigned int CACHING_SHA2_DIGEST_LENGTH= SHA256_DIGEST_LENGTH;

/**
  Supported digest information
*/

typedef enum Digest_info
{
  SHA256_DIGEST= 0,
  DIGEST_LAST
} Digest_info;

/**
  Interface for cryptographic digest generation
*/

class Generate_digest
{
 public:
  virtual bool update_digest(const void *src, unsigned int length)= 0;
  virtual bool retrieve_digest(unsigned char *digest, unsigned int length)= 0;
  virtual void scrub()= 0;
  virtual ~Generate_digest() {}
};

#ifndef HAVE_YASSL
#define DIGEST_CTX EVP_MD_CTX
#else
#define DIGEST_CTX TaoCrypt::SHA256
#endif /* !HAVE_YASSL */

/**
  SHA256 digest generator
  @sa Generate_digest
  @sa Digest_info
*/

class SHA256_digest : public Generate_digest
{
 public:
  SHA256_digest();
  ~SHA256_digest();

  bool update_digest(const void *src, unsigned int length);
  bool retrieve_digest(unsigned char *digest, unsigned int length);
  void scrub();
  bool all_ok() { return m_ok; }

 private:
  void init();
  void deinit();

 private:
  /** Digest output buffer */
  unsigned char m_digest[CACHING_SHA2_DIGEST_LENGTH];
  /** Digest context */
  DIGEST_CTX *md_context;
  /** Status */
  bool m_ok;
};

/**
  Scramble generator
  Responsible for generating scramble of following format:
  XOR(SHA2(m_src), SHA2(SHA2(SHA2(m_src)), m_rnd))
  @sa SHA256_digest
  @sa Digest_info
*/

class Generate_scramble
{
public:
  Generate_scramble(const std::string source, const std::string rnd,
                    Digest_info digest_type= SHA256_DIGEST);

  ~Generate_scramble();

  bool scramble(unsigned char *scramble, unsigned int scramble_length);

 private:
  /** plaintext source string */
  std::string m_src;
  /** random string */
  std::string m_rnd;
  /** Type of digest */
  Digest_info m_digest_type;
  /** Digest generator class */
  Generate_digest *m_digest_generator;
  /** length of the digest */
  unsigned int m_digest_length;
};

}  // namespace sha2_password

/** @} (end of auth_caching_sha2_auth) */

#endif  // !I_SHA2_PASSWORD_INCLUDED
