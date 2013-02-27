/* Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef TABLE_ID_INCLUDED
#define TABLE_ID_INCLUDED

#include "my_global.h"

/*
  Each table share has a table id, it is mainly used for row based replication.
  Meanwhile it is used as table's version too.
*/
class Table_id
{
private:
  /* In table map event and rows events, table id is 6 bytes.*/
  static const ulonglong TABLE_ID_MAX= (~0ULL >> 16);
  ulonglong m_id;

public:
  Table_id() : m_id(0) {}
  Table_id(ulonglong id) : m_id(id) {}

  ulonglong id() const { return m_id; }
  bool is_valid() const { return m_id <= TABLE_ID_MAX; }
  bool is_invalid() const { return m_id > TABLE_ID_MAX; }

  void operator=(const Table_id &tid) { m_id = tid.m_id; }
  void operator=(ulonglong id) { m_id = id; }

  bool operator==(const Table_id &tid) const { return m_id == tid.m_id; }
  bool operator!=(const Table_id &tid) const { return m_id != tid.m_id; }

  /* Support implicit type converting from Table_id to ulonglong */
  operator ulonglong() const { return m_id; }

  Table_id operator++(int)
  {
    Table_id id(m_id);

    /* m_id is reset to 0, when it exceeds the max value. */
    m_id = (m_id == TABLE_ID_MAX ? 0 : m_id + 1);

    DBUG_ASSERT(m_id <= TABLE_ID_MAX );
    return id;
  }
};

#endif
