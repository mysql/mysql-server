/* Copyright (c) 2023, 2026, Oracle and/or its affiliates.

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

#include "common.h"

/**
  Helper method to convert base64 string to url safe base64.
  Using standard Base64 in URL requires replacing of '+', '/' and '='
  characters.
  '+' will be replaced with '-'
  '/' will be replaced with '_'
  '=' is used for padding which will be removed.
  @param [in] url_compatible_str url safe base64 string
  @param [in] len length of base64 string
  @param [in] base64_str  base64 string
*/
void url_compatible_base64(char *url_compatible_str, size_t len,
                           char *base64_str) {
  for (size_t i = 0; i < len; i++) {
    switch (base64_str[i]) {
      case '+':
        url_compatible_str[i] = '-';
        break;
      case '/':
        url_compatible_str[i] = '_';
        break;
      case '=':
        url_compatible_str[i] = 0;
        return;
      default:
        url_compatible_str[i] = base64_str[i];
    }
  }
}

bool generate_sha256(const unsigned char *in_key, unsigned int in_key_length,
                     unsigned char *hash, unsigned int &hash_length) {
  bool ret = true;
  EVP_MD_CTX *context = EVP_MD_CTX_create();
  if (context != nullptr) {
    if (EVP_DigestInit_ex(context, md_sha256 ? md_sha256 : EVP_sha256(),
                          nullptr)) {
      if (EVP_DigestUpdate(context, in_key, in_key_length)) {
        if (EVP_DigestFinal_ex(context, hash, &hash_length)) {
          ret = false;
        }
      }
    }
    EVP_MD_CTX_destroy(context);
  }
  return ret;
}
