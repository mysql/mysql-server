/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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

#ifndef UTIL_NDB_ZLIB
#define UTIL_NDB_ZLIB

#include "zlib.h"

#include "util/ndbxfrm_iterator.h"

class ndb_zlib {
 public:
  using byte = unsigned char;
  using input_iterator = ndbxfrm_input_iterator;
  using output_iterator = ndbxfrm_output_iterator;

  static constexpr size_t MEMORY_NEED = 275256;

  ndb_zlib();
  ~ndb_zlib();

  void reset();
  int set_memory(void *mem, size_t size);
  int set_pkcs_padding() {
    pkcs_padded = true;
    return 0;
  }
  size_t get_random_access_block_size() const { return 0; }

  int deflate_init();
  int deflate(output_iterator *out, input_iterator *in);
  int deflate_end();

  int inflate_init();
  int inflate(output_iterator *out, input_iterator *in);
  int inflate_end();

  ndb_off_t get_input_position() const { return file.total_in; }
  ndb_off_t get_output_position() const { return file.total_out; }

 private:
  // RFC1950 ZLIB Compressed Data Format Specification version 3.3
  // RFC1951 DEFLATE Compressed Data Format Specification version 1.3
  static constexpr int level = Z_DEFAULT_COMPRESSION;
  static constexpr int method = Z_DEFLATED;
  // From zconf.h 32K LZ77 window (MAX_WBITS 15)
  static constexpr int windowBits = 15;
  // raw (no header), no checksum
  static constexpr int zlib_windowBits = -windowBits;
  static constexpr int memLevel = 8;
  static constexpr int strategy = Z_DEFAULT_STRATEGY;

  static void *alloc(void *opaque, unsigned items, unsigned size);
  static void free(void *opaque, void *address);

  byte *mem_begin;
  byte *mem_top;
  byte *mem_end;

  enum operation_mode { NO_OP, DEFLATE, INFLATE };
  operation_mode m_op_mode;
  bool pkcs_padded;
  byte padding;
  unsigned padding_left;
  z_stream file;
};

#endif
