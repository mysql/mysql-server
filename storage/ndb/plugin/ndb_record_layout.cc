/*
   Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "storage/ndb/plugin/ndb_record_layout.h"

#include "NdbApi.hpp"
#include "my_byteorder.h"
#include "my_dbug.h"
#include "storage/ndb/plugin/ndb_ndbapi_util.h"

Ndb_record_layout::Ndb_record_layout(int ncol)
    : record_specs(new NdbDictionary::RecordSpecification[ncol]),
      record_size(4),  // Use first four bytes for null bitmap
      m_columns(ncol),
      m_seq(0){};

Ndb_record_layout::~Ndb_record_layout() { delete[] record_specs; }

void Ndb_record_layout::clear() {
  record_size = 4;
  m_seq = 0;
}

/*
 * add a column to a Record
 */
void Ndb_record_layout::addColumn(const NdbDictionary::Column *column) {
  DBUG_ASSERT(m_seq < m_columns);

  /* Alignment */
  int align = column->getSizeInBytes();
  if (align > 8) align = 2;
  int offset = record_size % align;
  if (offset && (align == 2 || align == 4 || align == 8)) {
    record_size += (align - offset);
  }

  /* The current record size is the offset of this column */
  record_specs[m_seq].offset = record_size;

  /* Set nullbits in the record specification */
  if (column->getNullable()) {
    record_specs[m_seq].nullbit_byte_offset = m_columns / 8;
    record_specs[m_seq].nullbit_bit_in_byte = m_columns % 8;
  } else {
    record_specs[m_seq].nullbit_byte_offset = 0;
    record_specs[m_seq].nullbit_bit_in_byte = 0;
  }

  /* Set Column in record spec */
  record_specs[m_seq].column = column;

  /* Increment the counter and record size */
  m_seq += 1;
  record_size += column->getSizeInBytes();
};

bool Ndb_record_layout::isNull(const char *data, int idx) const {
  if (record_specs[idx].column->getNullable()) {
    return (*(data + record_specs[idx].nullbit_byte_offset) &
            (1 << record_specs[idx].nullbit_bit_in_byte));
  }
  return false;
}

void Ndb_record_layout::setValue(int idx, unsigned short value,
                                 char *data) const {
  DBUG_ASSERT(idx < (int)m_columns);
  DBUG_ASSERT(record_specs[idx].column->getSizeInBytes() == sizeof(short));

  setNotNull(idx, data);
  int2store((data + record_specs[idx].offset), value);
}

void Ndb_record_layout::setValue(int idx, std::string value, char *data) const {
  DBUG_ASSERT(idx < (int)m_columns);
  setNotNull(idx, data);
  ndb_pack_varchar(record_specs[idx].column, record_specs[idx].offset,
                   value.c_str(), value.length(), data);
}

void Ndb_record_layout::setValue(int idx, unsigned int *value,
                                 char *data) const {
  DBUG_ASSERT(idx < (int)m_columns);
  if (value) {
    setNotNull(idx, data);
    int4store((data + record_specs[idx].offset), *value);
  } else {
    setNull(idx, data);
  }
}

void Ndb_record_layout::packValue(int idx, std::string value,
                                  char *data) const {
  ndb_pack_varchar(record_specs[idx].column, 0, value.c_str(), value.length(),
                   data);
}

bool Ndb_record_layout::getValue(const char *data, int idx,
                                 unsigned short *value) const {
  DBUG_ASSERT(idx < (int)m_columns);
  if (isNull(data, idx)) return false;
  data += record_specs[idx].offset;
  *value = uint2korr(data);
  return true;
}

bool Ndb_record_layout::getValue(const char *data, int idx, size_t *length,
                                 const char **str) const {
  DBUG_ASSERT(idx < (int)m_columns);
  if (isNull(data, idx)) return false;
  ndb_unpack_varchar(record_specs[idx].column, record_specs[idx].offset, str,
                     length, data);
  return true;
}

bool Ndb_record_layout::getValue(const char *data, int idx,
                                 unsigned int *value) const {
  DBUG_ASSERT(idx < (int)m_columns);
  if (isNull(data, idx)) return false;
  data += record_specs[idx].offset;
  *value = uint4korr(data);
  return true;
}
