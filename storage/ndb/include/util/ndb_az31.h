/* Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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

#ifndef NDB_UTIL_NDB_AZ31_H
#define NDB_UTIL_NDB_AZ31_H

#include <stdint.h>

#include "util/ndbxfrm_iterator.h"

class ndb_az31
{
public:
  using byte = unsigned char;
  using output_iterator = ndbxfrm_output_iterator;
  using input_iterator = ndbxfrm_input_iterator;
  using input_reverse_iterator = ndbxfrm_input_reverse_iterator;

  ndb_az31(): m_have_data_size(false), m_have_data_crc32(false) {}

  /* -1 fail, 0 ok, 1 need more space/input */
  int set_data_size(Uint64 data_size) { m_data_size = data_size; m_have_data_size = true; return 0; }
  int set_data_crc32(Uint32 data_crc32) { m_data_crc32 = data_crc32; m_have_data_crc32 = true; return 0; }
  static int write_header(output_iterator* out);
  int write_trailer(output_iterator* out, int pad_len,
                    output_iterator* extra = nullptr) const;
  size_t get_trailer_size() const { return 12; }

  static int detect_header(const input_iterator* in);
  static int read_header(input_iterator* in);
  int read_trailer(input_reverse_iterator* in);
  int get_data_size(Uint64* data_size) const { if (!m_have_data_size) return -1; *data_size = m_data_size; return 0; }
  int get_data_crc32(Uint32* data_crc32) const { if (!m_have_data_crc32) return -1; *data_crc32 = m_data_crc32; return 0; }
private:
  static const byte header[512];
  bool m_have_data_size;
  uint64_t m_data_size;
  bool m_have_data_crc32;
  uint32_t m_data_crc32;
};

#endif
