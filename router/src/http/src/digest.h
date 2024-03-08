/*
  Copyright (c) 2018, 2024, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/
#ifndef MYSQLROUTER_DIGEST_INCLUDED
#define MYSQLROUTER_DIGEST_INCLUDED

#include <algorithm>
#include <limits>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <openssl/evp.h>

#include "openssl_version.h"

/**
 * message digest.
 *
 * Wrapper around Digest functions of openssl (EVP_MD_...)
 *
 * - MD5
 * - SHA1
 * - SHA224
 * - SHA256
 * - SHA384
 * - SHA512
 *
 * @see openssl's EVP_sha512()
 */
class Digest {
 public:
  enum class Type { Md5, Sha1, Sha224, Sha256, Sha384, Sha512 };

  /**
   * constructor.
   *
   * initializes the digest function.
   */
  Digest(Type type) : type_{type}, ctx_ {
#if defined(OPENSSL_VERSION_NUMBER) && \
    (OPENSSL_VERSION_NUMBER >= ROUTER_OPENSSL_VERSION(1, 1, 0))
    EVP_MD_CTX_new(), &EVP_MD_CTX_free
#else
    EVP_MD_CTX_create(), &EVP_MD_CTX_destroy
#endif
  }
  { reinit(); }

  /**
   * initialize or reinitialize the message digest functions.
   *
   * Allows reused of the Digest function without reallocating memory.
   */
  void reinit() {
#if defined(OPENSSL_VERSION_NUMBER) && \
    (OPENSSL_VERSION_NUMBER >= ROUTER_OPENSSL_VERSION(1, 1, 0))
    EVP_MD_CTX_reset(ctx_.get());
#else
    EVP_MD_CTX_cleanup(ctx_.get());
#endif
    EVP_DigestInit(ctx_.get(), Digest::get_evp_md(type_));
  }

  /**
   * update Digest.
   *
   * @param data data to update digest function with
   */
  void update(const std::span<uint8_t> &data) {
    EVP_DigestUpdate(ctx_.get(), data.data(), data.size());
  }

  void update(const std::string_view &data) {
    EVP_DigestUpdate(ctx_.get(), data.data(), data.length());
  }

  /**
   * finalize the digest and get digest value.
   *
   * @param out vector to place the digest value in
   */
  void finalize(std::vector<uint8_t> &out) { finalize_impl(out); }

  /**
   * finalize the digest and get digest value.
   *
   * @param out string to place the digest value in
   */
  void finalize(std::string &out) { finalize_impl(out); }

  /**
   * get size of the digest value.
   *
   * @param type type of message digest
   * @returns size of message digest
   */
  static size_t digest_size(Type type) {
    if (auto *dg = Digest::get_evp_md(type)) {
      return EVP_MD_size(dg);
    } else {
      // compiler should ensure this can't happen
      throw std::invalid_argument("type wasn't part of Type");
    }
  }

 private:
  template <typename Container>
  void finalize_impl(Container &out) {
    // if cap is too large, limit it to uint::max and let narrowing handle the
    // rest
    unsigned int out_len{static_cast<unsigned int>(std::min(
        out.capacity(),
        static_cast<size_t>(std::numeric_limits<unsigned int>::max())))};

    EVP_DigestFinal_ex(ctx_.get(), reinterpret_cast<uint8_t *>(&out[0]),
                       &out_len);
    out.resize(out_len);
  }

  static const EVP_MD *get_evp_md(Type type) noexcept {
    switch (type) {
      case Type::Md5:
        return EVP_md5();
      case Type::Sha1:
        return EVP_sha1();
      case Type::Sha224:
        return EVP_sha224();
      case Type::Sha256:
        return EVP_sha256();
      case Type::Sha384:
        return EVP_sha384();
      case Type::Sha512:
        return EVP_sha512();
    }

    return nullptr;
  }
  Type type_;
  std::unique_ptr<EVP_MD_CTX, decltype(
#if defined(OPENSSL_VERSION_NUMBER) && \
    (OPENSSL_VERSION_NUMBER >= ROUTER_OPENSSL_VERSION(1, 1, 0))
                                  &EVP_MD_CTX_free
#else
                                  &EVP_MD_CTX_destroy
#endif
                                  )>
      ctx_;
};

#endif
