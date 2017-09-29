/* Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef DD__RAW_RECORD_SET_INCLUDED
#define DD__RAW_RECORD_SET_INCLUDED

#include <stddef.h>

#include "sql/dd/impl/raw/raw_record.h"   // dd::Raw_record

struct TABLE;

namespace dd {

///////////////////////////////////////////////////////////////////////////

struct Raw_key;

///////////////////////////////////////////////////////////////////////////

class Raw_record_set : private Raw_record
{
public:
  ~Raw_record_set();

  Raw_record *current_record()
  { return m_current_record; }

  bool next(Raw_record *&r);

private:
  // Note: The 'key' supplied will be freed by Raw_record_set
  Raw_record_set(TABLE *table, Raw_key *key)
   :Raw_record(table),
    m_key(key),
    m_current_record(NULL)
  { }

  bool open();

  friend class Raw_table;

private:
  // Raw_record_set owns m_key.
  Raw_key *m_key;

  Raw_record *m_current_record;
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__RAW_RECORD_SET_INCLUDED
