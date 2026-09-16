/* Copyright (c) 2026, Oracle and/or its affiliates.

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

#ifndef NDB_UTIL_NDBXFRM_PADDING_H
#define NDB_UTIL_NDBXFRM_PADDING_H

#include "ndb_types.h"
#include "util/ndbxfrm_buffer.h"
#include "util/ndbxfrm_iterator.h"

class ndb_pkcs7_padding {
  using byte = uint8_t;
  byte padding = 0;
  byte remaining = 0;

 public:
  static constexpr byte cipher_block_size = 16;

  void set_padding(ndb_off_t data_size);
  int pad(ndbxfrm_output_iterator *out);
  int pad(ndbxfrm_buffer *buf);

  int unpad(ndbxfrm_input_iterator *in);
  int unpad(ndbxfrm_buffer *buf);
  int unpad_reverse(ndbxfrm_input_reverse_iterator *in);
  int unpad_reverse(ndbxfrm_buffer *buf);
  bool check_and_clear_padding(ndb_off_t data_size);
};

#endif
