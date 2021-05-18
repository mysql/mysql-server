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

#include "util/ndb_az31.h"

const ndb_az31::byte ndb_az31::header[512] = {
  254,  3,  1, 16,  0,  0,  2,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  2,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,
};

int ndb_az31::write_header(output_iterator* out)
{
  if (out->size() < sizeof(header))
    return 1; // Need more space
  memcpy(out->begin(), header, sizeof(header));
  out->advance(sizeof(header));
  return 0;
}

int ndb_az31::write_trailer(output_iterator* out, int pad_len) const
{
  if (!m_have_data_size)
    return -1;
  if (!m_have_data_crc32)
    return -1;

  if (out->size() < 12)
    return 1; // Need more space
  byte* p = out->begin();
  //
  p[0] = m_data_crc32 & 0xff;
  p[1] = (m_data_crc32 & 0xff00) >> 8;
  p[2] = (m_data_crc32 & 0xff0000) >> 16;
  p[3] = (m_data_crc32 & 0xff000000) >> 24;
  //
  p[4] = m_data_size & 0xff;
  p[5] = (m_data_size & 0xff00) >> 8;
  p[6] = (m_data_size & 0xff0000) >> 16;
  p[7] = (m_data_size & 0xff000000) >> 24;
  //
  memcpy(p + 8, "DBDN", 4);
  memset(p + 12, 0, pad_len);
  out->advance(12 + pad_len);
  out->set_last();
  return 0;
}

int ndb_az31::detect_header(input_iterator* in)
{
  if (in->size() < 3)
    return in->last() ? -1 : 1;
  if (memcmp(in->cbegin(), header, 3) != 0)
    return -1;
  return 0;
}

int ndb_az31::read_header(input_iterator* in)
{
  if (in->size() < sizeof(header))
    return in->last() ? -1 : 1;
  if (memcmp(in->cbegin(), header, sizeof(header)) != 0)
    return -1;
  in->advance(sizeof(header));
  return 0;
}

int ndb_az31::read_trailer(input_reverse_iterator* in)
{
  const byte* pbeg = in->cend();
  const byte* pend = in->cbegin();
  while (pbeg < pend && pend[-1] == 0) pend--;
  if (pend - pbeg < 12)
    return -1;
  pend -= 12;
  if (memcmp(pend + 8, "DBDN", 4) != 0)
    return -1;
  m_data_size = Uint32(pend[4]) |
                (Uint32(pend[5]) << 8) |
                (Uint32(pend[6]) << 16) |
                (Uint32(pend[7]) << 24);
  m_data_crc32 = Uint32(pend[0]) |
                 (Uint32(pend[1]) << 8) |
                 (Uint32(pend[2]) << 16) |
                 (Uint32(pend[3]) << 24);
  in->advance(in->cbegin() - pend);
  return 0;
}
