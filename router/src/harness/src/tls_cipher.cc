/*
  Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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

#include "mysql/harness/tls_cipher.h"

#include <array>
#include <memory>
#include <system_error>

#include <openssl/evp.h>

#include "mysql/harness/stdx/expected.h"

template <class>
class Deleter;

template <>
class Deleter<EVP_CIPHER_CTX> {
 public:
  void operator()(EVP_CIPHER_CTX *p) { EVP_CIPHER_CTX_free(p); }
};

using TlsCipherCtx = std::unique_ptr<EVP_CIPHER_CTX, Deleter<EVP_CIPHER_CTX>>;

/**
 * Transforms an arbitrary long key into a fixed length AES key.
 *
 * AES keys are of fixed length. This routine takes an arbitrary long key
 * iterates over it in AES key length increment and XORs the bytes with the
 * AES key buffer being prepared.
 * The bytes from the last incomplete iteration are XORed to the start
 * of the key until their depletion.
 * Needed since crypto function routines expect a fixed length key.
 *
 * @param[in] key_begin           pointer to start of they key
 * @param[in] key_end             pointer to one past the end of the key
 * @param[out] rkey_begin         pointer to start of the real key
 * @param[out] rkey_end           pointer to one past the end of the real key
 */
static void create_key(const uint8_t *key_begin, const uint8_t *key_end,
                       uint8_t *rkey_begin, uint8_t *rkey_end) {
  const uint8_t *key_cur;
  uint8_t *rkey_cur;

  std::fill(rkey_begin, rkey_end, 0);

  for (rkey_cur = rkey_begin, key_cur = key_begin; key_cur < key_end;
       ++key_cur, ++rkey_cur) {
    if (rkey_cur == rkey_end) {
      rkey_cur = rkey_begin;
    }

    *rkey_cur ^= *key_cur;
  }
}

stdx::expected<size_t, std::error_code> TlsCipher::encrypt(
    const uint8_t *src, size_t src_size, uint8_t *dst, const uint8_t *key,
    size_t key_size, const uint8_t *iv, bool padding) const {
  if (cipher_ == nullptr) {
    return stdx::unexpected(make_error_code(std::errc::invalid_argument));
  }

  TlsCipherCtx cipher_ctx(EVP_CIPHER_CTX_new());

  auto *ctx = cipher_ctx.get();

  const auto cipher_key_size = EVP_CIPHER_key_length(cipher_);

  if (!ctx || cipher_key_size > EVP_MAX_KEY_LENGTH ||
      (EVP_CIPHER_iv_length(cipher_) > 0 && iv == nullptr)) {
    return stdx::unexpected(make_error_code(std::errc::invalid_argument));
  }

  std::array<uint8_t, EVP_MAX_KEY_LENGTH> rkey;

  // transform a key into a cipher-key
  create_key(key, key + key_size, rkey.data(), rkey.data() + cipher_key_size);

  int updated_len{};
  int final_len{};

  if (1 != EVP_EncryptInit(ctx, cipher_, rkey.data(), iv) ||
      1 != EVP_CIPHER_CTX_set_padding(ctx, padding) ||
      1 != EVP_EncryptUpdate(ctx, dst, &updated_len, src, src_size) ||
      1 != EVP_EncryptFinal(ctx, dst + updated_len, &final_len)) {
    return stdx::unexpected(make_error_code(std::errc::invalid_argument));
  }

  return updated_len + final_len;
}

stdx::expected<size_t, std::error_code> TlsCipher::decrypt(
    const uint8_t *src, size_t src_size, uint8_t *dst, const uint8_t *key,
    size_t key_size, const uint8_t *iv, bool padding) const {
  if (cipher_ == nullptr) {
    return stdx::unexpected(make_error_code(std::errc::invalid_argument));
  }

  TlsCipherCtx cipher_ctx(EVP_CIPHER_CTX_new());

  auto *ctx = cipher_ctx.get();

  const auto cipher_key_size = EVP_CIPHER_key_length(cipher_);

  if (!ctx || cipher_key_size > EVP_MAX_KEY_LENGTH ||
      (EVP_CIPHER_iv_length(cipher_) > 0 && iv == nullptr)) {
    return stdx::unexpected(make_error_code(std::errc::invalid_argument));
  }

  std::array<uint8_t, EVP_MAX_KEY_LENGTH> rkey;

  create_key(key, key + key_size, rkey.data(), rkey.data() + cipher_key_size);

  int updated_len{};
  int final_len{};

  if (1 != EVP_DecryptInit(ctx, cipher_, rkey.data(), iv) ||
      1 != EVP_CIPHER_CTX_set_padding(ctx, padding) ||
      1 != EVP_DecryptUpdate(ctx, dst, &updated_len, src, src_size) ||
      1 != EVP_DecryptFinal(ctx, dst + updated_len, &final_len)) {
    return stdx::unexpected(make_error_code(std::errc::invalid_argument));
  }

  return updated_len + final_len;
}

size_t TlsCipher::size(size_t source_length) const {
  const size_t block_size = EVP_CIPHER_block_size(cipher_);

  return block_size > 1 ? block_size * (source_length / block_size) + block_size
                        : source_length;
}
