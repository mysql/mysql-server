/*
  Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

#ifndef MYSQL_HARNESS_TLS_CIPHER_H_INCLUDED
#define MYSQL_HARNESS_TLS_CIPHER_H_INCLUDED

#include <system_error>

#include <openssl/evp.h>

#include "mysql/harness/stdx/expected.h"

class TlsCipher {
 public:
  using nid_type = int;

  TlsCipher(const EVP_CIPHER *cipher) : cipher_{cipher} {}

  stdx::expected<size_t, std::error_code> encrypt(
      const uint8_t *src, size_t src_size, uint8_t *dst, const uint8_t *key,
      size_t key_size, const uint8_t *iv, bool padding = true) const;

  stdx::expected<size_t, std::error_code> decrypt(
      const uint8_t *src, size_t src_size, uint8_t *dst, const uint8_t *key,
      size_t key_size, const uint8_t *iv, bool padding = true) const;

  size_t size(size_t source_length) const;

 private:
  const EVP_CIPHER *cipher_;
};

#endif
