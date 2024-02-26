/* Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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

#include <cstring>

#include "keyring_operations_helper.h"

namespace keyring_operations_helper {

int read_secret(SERVICE_TYPE(keyring_reader_with_status) * keyring_reader,
                const char *secret_id, const char *auth_id,
                unsigned char **secret, size_t *secret_length,
                char **secret_type, PSI_memory_key psi_memory_key) {
  if (keyring_reader == nullptr || secret == nullptr ||
      secret_type == nullptr || secret_length == nullptr)
    return 0;

  *secret = nullptr;
  *secret_type = nullptr;
  *secret_length = 0;

  size_t secret_type_length = 0;
  my_h_keyring_reader_object reader_object = nullptr;
  bool retval = keyring_reader->init(secret_id, auth_id, &reader_object);

  /* Keyring error */
  if (retval == true) return -1;

  /* Key absent */
  if (reader_object == nullptr) return 0;

  auto cleanup_guard = create_scope_guard([&] {
    if (reader_object != nullptr) (void)keyring_reader->deinit(reader_object);
    reader_object = nullptr;
  });

  /* Fetch length */
  if (keyring_reader->fetch_length(reader_object, secret_length,
                                   &secret_type_length) != 0)
    return 0;

  if (*secret_length == 0 || secret_type_length == 0) return 0;

  /* Allocate required memory for key and secret_type */
  *secret = reinterpret_cast<unsigned char *>(
      my_malloc(psi_memory_key, *secret_length, MYF(MY_WME)));
  if (*secret == nullptr) return 0;
  memset(*secret, 0, *secret_length);

  *secret_type = reinterpret_cast<char *>(
      my_malloc(psi_memory_key, secret_type_length + 1, MYF(MY_WME)));
  if (*secret_type == nullptr) {
    my_free(*secret);
    *secret = nullptr;
    return 0;
  }
  memset(*secret_type, 0, secret_type_length + 1);

  if (keyring_reader->fetch(reader_object, *secret, *secret_length,
                            secret_length, *secret_type, secret_type_length,
                            &secret_type_length) != 0) {
    my_free(*secret);
    my_free(*secret_type);
    *secret = nullptr;
    *secret_type = nullptr;
    *secret_length = 0;
    return 0;
  }

  return 1;
}

}  // namespace keyring_operations_helper
