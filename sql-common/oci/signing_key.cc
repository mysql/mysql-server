/* Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA  */

#include "sql-common/oci/signing_key.h"
#include <openssl/crypto.h>
#include <iostream>
#include <memory>
#include "sql-common/oci/ssl.h"

namespace oci {
// custom unique_ptr deleter since OPENSSL_free is a macro
static void SSL_memory_deallocator(void *p) { OPENSSL_free(p); }

void log_error(const std::string &message) { std::cerr << message; }

Data Signing_Key::sign(const std::string &message) {
  return sign(message.c_str(), message.length());
}

Data Signing_Key::sign(const void *message, size_t length) {
  if (m_private_key == nullptr) return {};

  size_t slen = 0;

  /* Create the Message Digest Context */
  ssl::EVP_MD_CTX_ptr evp_md_ctx{EVP_MD_CTX_create()};
  if (!evp_md_ctx) return {};

  /* Initialise the DigestSign operation, using SHA-256 message digest. */
  if (1 != EVP_DigestSignInit(evp_md_ctx.get(), nullptr, EVP_sha256(), nullptr,
                              m_private_key.get()))
    return {};

  /* Call update with the message */
  if (1 != EVP_DigestSignUpdate(evp_md_ctx.get(), message, length)) return {};

  /* Finalise the DigestSign operation */
  /* First call EVP_DigestSignFinal with a nullptr sig parameter to obtain the
   * length of the signature. Length is returned in slen */
  if (1 != EVP_DigestSignFinal(evp_md_ctx.get(), nullptr, &slen)) return {};
  /* Allocate memory for the signature based on size in slen */
  std::unique_ptr<unsigned char, decltype(&SSL_memory_deallocator)> signature(
      static_cast<unsigned char *>(
          OPENSSL_malloc(sizeof(unsigned char) * slen)),
      &SSL_memory_deallocator);

  if (!signature) return {};

  /* Obtain the signature */
  if (1 != EVP_DigestSignFinal(evp_md_ctx.get(), signature.get(), &slen))
    return {};

  // Success.
  return {signature.get(), signature.get() + slen};
}

/**
 * Constructor.
 * Read the key from the file.
 */
Signing_Key::Signing_Key(const std::string &file_name)
    : m_private_key{EVP_PKEY_new()} {
  std::unique_ptr<FILE, decltype(&fclose)> fp(fopen(file_name.c_str(), "rb"),
                                              &fclose);
  if (!fp) {
    log_error("Cannot open signing key file " + file_name + "\n");
    return;
  }

  if (!m_private_key) {
    log_error("Cannot create private key");
    return;
  }

  auto key = m_private_key.release();
  key = PEM_read_PrivateKey(fp.get(), &key, nullptr, nullptr);
  if (key == nullptr) {
    log_error("Cannot read signing key file " + file_name);
    return;
  }
  m_private_key = ssl::EVP_PKEY_ptr{key};
}

/**
 * Constructor.
 * Read the key from the memory string.
 */
Signing_Key::Signing_Key(ssl::Key_Content key_content) {
  void *ptr;
  ptr = static_cast<void *>(const_cast<char *>(key_content.c_str()));
  oci::ssl::BIO_ptr key_bio{BIO_new_mem_buf(ptr, key_content.size())};
  if (!key_bio) return;

  m_private_key = ssl::EVP_PKEY_ptr{
      PEM_read_bio_PrivateKey(key_bio.get(), nullptr, nullptr, nullptr)};
  if (!m_private_key) {
    log_error("Error reading the private key " + key_content);
    return;
  }
}

/**
 * Constructor.
 * Generate a key.
 */
Signing_Key::Signing_Key() {
  // Generate a new RSA private key to be used for request signing.
#if OPENSSL_VERSION_NUMBER < 0x30000000L
  std::unique_ptr<RSA, decltype(&::RSA_free)> rsa(RSA_new(), ::RSA_free);
  std::unique_ptr<BIGNUM, decltype(&::BN_free)> bn(BN_new(), ::BN_free);

  if (1 == BN_set_word(bn.get(), RSA_F4)) {
    if (1 == RSA_generate_key_ex(rsa.get(), 2048, bn.get(), nullptr)) {
      m_private_key = ssl::EVP_PKEY_ptr{EVP_PKEY_new()};
      // Convert RSA to PKEY
      if (1 == EVP_PKEY_set1_RSA(m_private_key.get(), rsa.get())) {
        // Extract the public key from the private key.
        oci::ssl::BIO_ptr bio{BIO_new(BIO_s_mem())};
        if (PEM_write_bio_RSA_PUBKEY(bio.get(), rsa.get())) {
          size_t len = BIO_pending(bio.get());
          std::vector<char> read_buffer;
          read_buffer.resize(len + 1, '\0');
          BIO_read(bio.get(), read_buffer.data(), len);
          m_public_key = read_buffer.data();
        }
      }
    }
  }
#else  /* OPENSSL_VERSION_NUMBER */
  m_private_key = ssl::EVP_PKEY_ptr{EVP_RSA_gen(2048)};
  oci::ssl::BIO_ptr bio{BIO_new(BIO_s_mem())};
  if (PEM_write_bio_PUBKEY(bio.get(), m_private_key.get())) {
    size_t len = BIO_pending(bio.get());
    std::vector<char> read_buffer(len + 1, '\0');
    BIO_read(bio.get(), read_buffer.data(), len);
    m_public_key = read_buffer.data();
  }
#endif /* OPENSSL_VERSION_NUMBER */
}

}  // namespace oci
