/* Copyright (c) 2014, 2016 Oracle and/or its affiliates. All rights reserved.

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

#ifndef DD__RAW_RECORD_INCLUDED
#define DD__RAW_RECORD_INCLUDED

#include "my_global.h"

#include "dd/object_id.h"      // dd::Object_id

#include <string>

struct TABLE;
class Field;
typedef long my_time_t;

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Properties;
class Transaction;

///////////////////////////////////////////////////////////////////////////

class Raw_record
{
public:
  Raw_record(TABLE *table);

public:
  bool update();
  bool drop();

public:
  bool store_pk_id(int field_no, Object_id id);
  bool store_ref_id(int field_no, Object_id id);
  bool store(int field_no, const std::string &s, bool is_null= false);
  bool store(int field_no, ulonglong ull, bool is_null= false);
  bool store(int field_no, longlong ll, bool is_null= false);

  bool store(int field_no, bool b, bool is_null= false)
  { return store(field_no, b ? 1ll : 0ll, is_null); }

  bool store(int field_no, uint v, bool is_null= false)
  { return store(field_no, (ulonglong) v, is_null); }

  bool store(int field_no, int v, bool is_null= false)
  { return store(field_no, (longlong) v, is_null); }

  bool store(int field_no, const Properties &p);

  bool store_time(int field_no, my_time_t val, bool is_null= false);

public:
  bool is_null(int field_no) const;

  longlong read_int(int field_no) const;
  longlong read_int(int field_no, longlong null_value) const
  { return is_null(field_no) ? null_value : read_int(field_no); }

  ulonglong read_uint(int field_no) const;
  ulonglong read_uint(int field_no, ulonglong null_value) const
  { return is_null(field_no) ? null_value : read_uint(field_no); }

  std::string read_str(int field_no) const;
  std::string read_str(int field_no, const std::string &null_value) const
  { return is_null(field_no) ? null_value : read_str(field_no); }

  Object_id read_ref_id(int field_no) const;

  bool read_bool(int field_no) const
  { return read_int(field_no) != 0; }

  my_time_t read_time(int field_no) const;

protected:
  void set_null(int field_no, bool is_null);

  Field *field(int field_no) const; // XXX: return non-const from const-operation

protected:
  TABLE *m_table;
};

///////////////////////////////////////////////////////////////////////////

class Raw_new_record : public Raw_record
{
public:
  Raw_new_record(TABLE *table);

  ~Raw_new_record()
  { finalize(); }

public:
   bool insert();

   Object_id get_insert_id() const;

   void finalize();
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__RAW_RECORD_INCLUDED
