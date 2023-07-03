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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef MYSQLROUTER_ROUTING_OPENSSL_DIGEST_H
#define MYSQLROUTER_ROUTING_OPENSSL_DIGEST_H

#include <memory>  // unique_ptr
#include <string>
#include <string_view>

#include <openssl/evp.h>
#include <openssl/opensslv.h>

namespace openssl {
class DigestFunc {
 public:
  DigestFunc(const EVP_MD *func) : func_{func} {}

  int size() const { return EVP_MD_size(func_); }

  const EVP_MD *native_func() const { return func_; }

 private:
  const EVP_MD *func_;
};

class DigestCtx {
 public:
  class Deleter {
   public:
    void operator()(EVP_MD_CTX *ctx) {
#if OPENSSL_VERSION_NUMBER >= 0x1010000fL
      EVP_MD_CTX_free(ctx);
#else
      EVP_MD_CTX_destroy(ctx);
#endif
    }
  };

  DigestCtx(const EVP_MD *func) : digest_func_(func) {}
  DigestCtx(const DigestFunc &func) : digest_func_(func.native_func()) {}

  // reinit digest-ctx with the same digest-function after finalize()
  bool init() { return init(digest_func_); }

  // init digest-ctx with the digest-function.
  bool init(const EVP_MD *digest_func) {
    if (digest_func == nullptr) return false;

    auto res = EVP_DigestInit_ex(ctx_.get(), digest_func, nullptr);
    if (res) digest_func_ = digest_func;

    return res;
  }

  bool init(DigestFunc func) { return init(func.native_func()); }

  template <class T>
  bool update(const T &data) {
    return EVP_DigestUpdate(ctx_.get(), data.data(), data.size());
  }

  template <class T>
  bool finalize(T &out) {
    unsigned int written;

    return EVP_DigestFinal_ex(
        ctx_.get(), reinterpret_cast<unsigned char *>(out.data()), &written);
  }

 private:
  const EVP_MD *digest_func_{};

  std::unique_ptr<EVP_MD_CTX, Deleter> ctx_ {
#if OPENSSL_VERSION_NUMBER >= 0x1010000fL
    EVP_MD_CTX_new()
#else
    EVP_MD_CTX_create()
#endif
  };
};
}  // namespace openssl

#endif
