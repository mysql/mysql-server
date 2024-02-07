/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "NDBT_ResultRow.hpp"
#include <ndb_global.h>
#include <NdbOut.hpp>
#include <NdbSchemaCon.hpp>
#include "util/require.h"

NDBT_ResultRow::NDBT_ResultRow(const NdbDictionary::Table &tab,
                               char attrib_delimiter)
    : cols(0), names(NULL), data(NULL), m_ownData(false), m_table(tab) {
  ad[0] = attrib_delimiter;
  ad[1] = 0;

  if (tab.getObjectStatus() == NdbDictionary::Object::Retrieved) {
    cols = tab.getNoOfColumns();
    names = new char *[cols];
    data = new NdbRecAttr *[cols];

    for (int i = 0; i < cols; i++) {
      names[i] = new char[255];
      strcpy(names[i], tab.getColumn(i)->getName());
    }
  }
}

NDBT_ResultRow::~NDBT_ResultRow() {
  for (int i = 0; i < cols; i++) {
    delete[] names[i];
  }
  delete[] names;

  if (m_ownData) {
    for (int i = 0; i < cols; i++) delete data[i];
  }
  delete[] data;
}

NdbRecAttr *&NDBT_ResultRow::attributeStore(int i) { return data[i]; }

const NdbRecAttr *NDBT_ResultRow::attributeStore(int i) const {
  return data[i];
}

const NdbRecAttr *NDBT_ResultRow::attributeStore(const char *name) const {
  for (int i = 0; i < cols; i++) {
    if (strcmp(names[i], name) == 0) return data[i];
  }
  require(false);
  return 0;
}

NdbOut &NDBT_ResultRow::header(NdbOut &out) const {
  for (int i = 0; i < cols; i++) {
    out << names[i];
    if (i < cols - 1) out << ad;
  }
  return out;
}

BaseString NDBT_ResultRow::c_str() const {
  BaseString str;

  char buf[10];
  for (int i = 0; i < cols; i++) {
    if (data[i]->isNULL()) {
      sprintf(buf, "NULL");
      str.append(buf);
    } else {
      Uint32 *p = (Uint32 *)data[i]->aRef();
      Uint32 sizeInBytes = data[i]->get_size_in_bytes();
      for (Uint32 j = 0; j < sizeInBytes; j += (sizeof(Uint32))) {
        str.append("H'");
        if (j + 4 < sizeInBytes) {
          sprintf(buf, "%.8x", *p);
        } else {
          Uint32 tmp = 0;
          memcpy(&tmp, p, sizeInBytes - j);
          sprintf(buf, "%.8x", tmp);
        }
        p++;
        str.append(buf);
        if ((j + sizeof(Uint32)) < sizeInBytes) str.append(", ");
      }
    }
    str.append("\n");
  }
  str.append("*");

  // ndbout << "NDBT_ResultRow::c_str() = " << str.c_str() << endl;

  return str;
}

NdbOut &operator<<(NdbOut &ndbout, const NDBT_ResultRow &res) {
  if (res.cols != 0) {
    ndbout << *(res.data[0]);
    for (int i = 1; i < res.cols; i++) ndbout << res.ad << *(res.data[i]);
  }
  return ndbout;
}

NDBT_ResultRow *NDBT_ResultRow::clone() const {
  NDBT_ResultRow *row = new NDBT_ResultRow(m_table, ad[0]);
  row->m_ownData = true;
  Uint32 noOfColumns = m_table.getNoOfColumns();
  for (Uint32 i = 0; i < noOfColumns; i++) {
    row->data[i] = data[i]->clone();
  }

  return row;
}

bool NDBT_ResultRow::operator==(const NDBT_ResultRow &other) const {
  // quick and dirty
  return c_str() == other.c_str();
}
