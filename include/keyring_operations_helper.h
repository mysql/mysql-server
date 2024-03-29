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

#ifndef KEYRING_OPERATIONS_HELPER_INCLUDED
#define KEYRING_OPERATIONS_HELPER_INCLUDED

#include <mysql/components/service.h>
#include <mysql/components/services/keyring_reader_with_status.h>
#include <mysql/service_mysql_alloc.h>
#include "my_sys.h"
#include "scope_guard.h"

namespace keyring_operations_helper {

/**
  Read secret from keyring

  Note: Memory for secert and secret_type must be freed by the caller

  @param [in]  keyring_reader Handle to keyring_reader_with_status service
  @param [in]  secret_id      Identifier for secret data
  @param [in]  auth_id        Owner of secret data - nullptr for internal keys
  @param [out] secret         Output buffer for secret fetched from keyring
  @param [out] secret_length  Length of secret data
  @param [out] secret_type    Type of data
  @param [out] psi_memory_key Memory key to be used to allocate memory for
                              secret and secret_type

  @returns status of reading secret
    @retval -1 Keyring error
    @retval 0  Key absent
    @retval 1  Key present. Check output buffers.
*/
int read_secret(SERVICE_TYPE(keyring_reader_with_status) * keyring_reader,
                const char *secret_id, const char *auth_id,
                unsigned char **secret, size_t *secret_length,
                char **secret_type, PSI_memory_key psi_memory_key);
}  // namespace keyring_operations_helper

#endif /* KEYRING_OPERATIONS_HELPER_INCLUDED */
