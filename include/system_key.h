/* Copyright (c) 2018 Percona LLC and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; version 2 of
   the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA */

#ifndef SYSTEM_KEY_INCLUDED
#define SYSTEM_KEY_INCLUDED

#include "my_inttypes.h"

#define PERCONA_BINLOG_KEY_NAME "percona_binlog"
#define PERCONA_INNODB_KEY_NAME "percona_innodb"
#define PERCONA_REDO_KEY_NAME "percona_redo"

bool is_valid_percona_system_key(const char *key_name, size_t *key_length);

/**
  A convenience function that extracts key's data and key's version from system
  key.
  @param key[in]              system key to parse
  @param key_length[in]       system key's length
  @param key_version[out]     on success - extracted key's version
  @param key_data[out]        on success - extracted key's data - The caller of
  this function must free this memory
  @param key_data_length[out] on success - extracted key's data length

  @return key_data on success, NULL on failure
*/
extern uchar *parse_system_key(const uchar *key, const size_t key_length,
                               uint *key_version, uchar **key_data,
                               size_t *key_data_length) noexcept;
#endif  // SYSTEM_KEY_INCLUDED
