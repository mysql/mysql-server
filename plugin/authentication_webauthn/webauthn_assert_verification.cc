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

#include "webauthn_assert_verification.h"

#include <string>

#include <base64.h>    /* base64_encode */
#include <mysql_com.h> /* CHALLENGE_LENGTH */
#include "common.h"    /* url_compatible_base64 */

/* default constructor */
webauthn_assert_verification::webauthn_assert_verification(
    const unsigned char *salt) {
  /* save salt */
  memcpy(m_salt, salt, CHALLENGE_LENGTH);
}

/**
  Set method to set client data hash. This method will read client data JSON
  replace the random value received from client with what server has generated.
  Then generate SHA256 hash of the JSON object and set the hash in library
  context.

  @param [in] cd         buffer holding client data JSON object
  @param [in] len        length of buffer holding client data JSON object
*/
void webauthn_assert_verification::set_client_data(const unsigned char *cd,
                                                   size_t len) {
  char base64_salt[BASE64_SALT_LENGTH] = {0};
  char url_compatible_salt[BASE64_SALT_LENGTH] = {0};

  /* convert salt to be base64 */
  base64_encode(m_salt, CHALLENGE_LENGTH, base64_salt);
  /* convert base64 encoded salt to be websafe base64 */
  url_compatible_base64(url_compatible_salt, BASE64_SALT_LENGTH, base64_salt);

  std::string client_data_json{reinterpret_cast<const char *>(cd)};

  const std::string key{"\"challenge\":\""};
  auto p = client_data_json.find(key);
  /*
    Replace the salt recieved from client with the one which server plugin
    has generated.
  */
  if (p != std::string::npos)
    client_data_json.replace(p + key.length(), strlen(url_compatible_salt),
                             url_compatible_salt);

  unsigned char clientdata_hash[EVP_MAX_MD_SIZE] = {0};
  unsigned int clientdata_hash_len = 0;
  generate_sha256(
      reinterpret_cast<const unsigned char *>(client_data_json.c_str()), len,
      clientdata_hash, clientdata_hash_len);
  assert_verification::set_client_data(clientdata_hash, clientdata_hash_len);
}
