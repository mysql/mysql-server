/* Copyright (c) 2020, Oracle and/or its affiliates.

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

#ifndef NDB_UTIL_NDBXFRM_WRITEFILE_H
#define NDB_UTIL_NDBXFRM_WRITEFILE_H

#include "portlib/ndb_file.h"
#include "util/ndb_openssl_evp.h"
#include "util/ndb_zlib.h"
#include "util/ndbxfrm_buffer.h"
#include "util/ndbxfrm_iterator.h"

class ndbxfrm_writefile
{
public:
  using byte = unsigned char;
  ndbxfrm_writefile();
  bool eof() const { return m_eof; }
  bool is_open() const;
  int open(ndb_file& file, bool compress, byte* pwd, size_t pwd_len, int kdf_iter_count);
  int close(bool no_flush);
  bool is_encrypted() const { return m_encrypted; }
  int write_forward(ndbxfrm_input_iterator* in);
  int write_trailer(ndbxfrm_output_iterator* out);
private:
  int flush_payload();

bool m_eof;
bool m_file_eof;
  ndb_file* m_file;
  off_t m_payload_start;
  off_t m_payload_end;

  size_t m_file_block_size;
  enum { FF_UNKNOWN, FF_RAW, FF_AZ31, FF_NDBXFRM1 } m_file_format;
  bool m_compressed;
  bool m_encrypted;
  ndb_zlib zlib;
  ndb_openssl_evp openssl_evp;
  ndb_openssl_evp::operation openssl_evp_op;
  unsigned long m_crc32;
  unsigned long m_data_size;

  ndbxfrm_buffer m_decrypted_buffer;
  ndbxfrm_buffer m_file_buffer;
};

#endif
