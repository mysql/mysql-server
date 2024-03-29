/*
   Copyright (c) 2019, 2023, Oracle and/or its affiliates.

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

#ifndef Ndb_sql_record_layout_H
#define Ndb_sql_record_layout_H

#include <assert.h>
#include <string>

#include "NdbApi.hpp"
#include "storage/ndb/plugin/ndb_require.h"

class Ndb_record_layout {
 public:
  NdbDictionary::RecordSpecification *const record_specs;
  unsigned int record_size;

  Ndb_record_layout(int number_of_columns);
  ~Ndb_record_layout();

  void clear(); /* Reset object, allowing it to be reused */

  void addColumn(const NdbDictionary::Column *);

  void initRowBuffer(char *data) const;
  void setNull(int idx, char *data) const;
  void setNotNull(int idx, char *data) const;
  void setValue(int idx, unsigned short, char *data) const;
  void setValue(int idx, std::string, char *data) const;
  void setValue(int idx, unsigned int *, char *data) const;

  /* Encode value into a buffer for NdbScanFilter */
  void packValue(int idx, std::string, char *data) const;

  bool isNull(const char *data, int idx) const;

  /* Getters for nullable columns return false if the stored value is null. */
  bool getValue(const char *data, int idx, unsigned short *value) const;
  bool getValue(const char *data, int idx, size_t *length,
                const char **str) const;
  bool getValue(const char *data, int idx, unsigned int *value) const;

 private:
  static constexpr unsigned MAX_NULLABLE_COLUMNS = 32;

  const unsigned int m_columns;
  unsigned int m_seq;
  unsigned m_nullable_columns = 0;
};

inline void Ndb_record_layout::setNull(int idx, char *data) const {
  ndbcluster::ndbrequire(record_specs[idx].column->getNullable());
  assert(record_specs[idx].nullbit_byte_offset < MAX_NULLABLE_COLUMNS / 8);
  assert(record_specs[idx].nullbit_bit_in_byte < 8);
  *(data + record_specs[idx].nullbit_byte_offset) |=
      (char)(1 << record_specs[idx].nullbit_bit_in_byte);
}

inline void Ndb_record_layout::setNotNull(int idx, char *data) const {
  if (!record_specs[idx].column->getNullable()) return;
  assert(record_specs[idx].nullbit_byte_offset < MAX_NULLABLE_COLUMNS / 8);
  assert(record_specs[idx].nullbit_bit_in_byte < 8);
  *(data + record_specs[idx].nullbit_byte_offset) &=
      (char)(0xFF ^ (1 << record_specs[idx].nullbit_bit_in_byte));
}

#endif
