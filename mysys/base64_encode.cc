/* Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA  */

#include "base64_encode.h"
#include <openssl/err.h>
#include <regex>
#include <sstream>
#include "encode_ptr.h"

#include <cassert>
#include <fstream>
#include <iomanip>
#include <ios>
#include <iostream>

namespace oci {

namespace ssl {
/**
 * BASE64 encode encrypted data.
 */
std::string base64_encode(const void *binary, size_t length) {
  std::unique_ptr<BIO, decltype(&BIO_free_all)> b64(BIO_new(BIO_f_base64()),
                                                    &BIO_free_all);
  BIO_set_flags(b64.get(), BIO_FLAGS_BASE64_NO_NL);
  auto *sink = BIO_new(BIO_s_mem());
  BIO_push(b64.get(), sink);
  BIO_write(b64.get(), binary, static_cast<int>(length));
  if (BIO_flush(b64.get()) != 1) return {};
  char *encoded;
  const size_t result_length = BIO_get_mem_data(sink, &encoded);
  return {encoded, result_length};
}

std::string base64_encode(const Data &data) {
  if (data.empty()) return {};
  return base64_encode(data.data(), data.size());
}

/**
 * BASE64 decode an encoded string.
 */
Data base64_decode(const std::string &encoded) {
  if (encoded.empty()) return {};
  std::unique_ptr<BIO, decltype(&BIO_free_all)> b64(BIO_new(BIO_f_base64()),
                                                    &BIO_free_all);
  BIO_set_flags(b64.get(), BIO_FLAGS_BASE64_NO_NL);
  auto *source = BIO_new_mem_buf(
      const_cast<void *>(static_cast<const void *>(encoded.c_str())),
      -1);  // read-only source
  BIO_push(b64.get(), source);

  // Each byte gets encoded into 3 characters. +1 for \0
  const auto maxlen = encoded.length() / 4 * 3 + 1;
  Data decoded(maxlen);
  assert(decoded.size() == maxlen);
  const int len = BIO_read(b64.get(), decoded.data(), static_cast<int>(maxlen));
  decoded.resize(len);
  return decoded;
}

std::string load_public_key_file(const std::string &public_key_file) {
  std::ifstream inFile;
  inFile.open(public_key_file);  // open the input file

  std::stringstream strStream;
  strStream << inFile.rdbuf();  // read the file
  return strStream.str();       // str holds the content of the file
}

/**
 * Create public key BIO from in-memory public key buffer
 */
EVP_PKEY_ptr load_public_key(const std::string &public_key_content) {
  void *ptr;
  ptr = static_cast<void *>(const_cast<char *>(public_key_content.c_str()));
  BIO_ptr bio{BIO_new_mem_buf(ptr, public_key_content.size())};
  if (!bio) return {nullptr};
  EVP_PKEY *result;

  std::cout << "BIO_new_mem_buf " << std::endl;
  result = PEM_read_bio_PUBKEY(bio.get(), &result, nullptr, nullptr);
  std::cout << "PEM_read_bio_PUBKEY" << std::endl;
  return EVP_PKEY_ptr{result};
}

/**
 * Verify a message signed by the private key pair of the provided public key.
 */
bool verify(const std::string &digest, const std::string &message,
            const std::string &public_key_content) {
  EVP_PKEY *pkey;
  EVP_MD_CTX *ctx;
  {
    FILE *f = fopen(public_key_content.c_str(), "rb");
    pkey = PEM_read_PUBKEY(f, nullptr, nullptr, nullptr);
    if (pkey == nullptr) return false;
  }
  ctx = EVP_MD_CTX_create();
  if (ctx == nullptr) {
    std::cerr << "Error: EVP_MD_CTX_create" << std::endl;
    return false;
  }

  auto digest_raw = base64_decode(digest);

  if (1 != EVP_DigestVerifyInit(ctx, nullptr, EVP_sha256(), nullptr, pkey)) {
    //  public_key.get());
    std::cout << "EVP_DigestVerifyInit" << std::endl;
    ERR_print_errors_fp(stderr);
    return false;
  }

  if (1 != EVP_DigestVerifyUpdate(ctx, message.c_str(), message.length())) {
    std::cout << "EVP_DigestVerifyUpdate" << std::endl;
    ERR_print_errors_fp(stderr);
    return false;
  }

  if (1 != EVP_DigestVerifyFinal(ctx, digest_raw.data(), digest_raw.size())) {
    std::cout << "EVP_DigestVerifyFinal" << std::endl;
    ERR_print_errors_fp(stderr);
    return false;
  }
  std::cerr << "Match!\n";
  return true;
}
}  // namespace ssl
}  // namespace oci
