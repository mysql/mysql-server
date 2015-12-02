/* Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

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

#ifndef DD__ABSTRACT_TABLE_INCLUDED
#define DD__ABSTRACT_TABLE_INCLUDED

#include "my_global.h"

#include "dd/object_id.h"                 // dd::Object_id
#include "dd/types/dictionary_object.h"   // dd::Dictionary_object

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Column;
class Item_name_key;
class Object_type;
class Primary_id_key;
class Properties;
class Se_private_id_key;

template <typename I> class Iterator;
typedef Iterator<const Column> Column_const_iterator;
typedef Iterator<Column> Column_iterator;

namespace tables {
  class Tables;
}

///////////////////////////////////////////////////////////////////////////

class Abstract_table : public virtual Dictionary_object
{
public:
  static const Object_type &TYPE();
  static const Dictionary_object_table &OBJECT_TABLE();

  typedef Abstract_table cache_partition_type;
  typedef tables::Tables cache_partition_table_type;
  typedef Primary_id_key id_key_type;
  typedef Item_name_key name_key_type;
  typedef Se_private_id_key aux_key_type;

  // We need a set of functions to update a preallocated key.
  virtual bool update_id_key(id_key_type *key) const
  { return update_id_key(key, id()); }

  static bool update_id_key(id_key_type *key, Object_id id);

  virtual bool update_name_key(name_key_type *key) const
  { return update_name_key(key, schema_id(), name()); }

  static bool update_name_key(name_key_type *key, Object_id schema_id,
                              const std::string &name);

  virtual bool update_aux_key(aux_key_type *key) const
  { return true; }

public:
  virtual ~Abstract_table()
  { };

public:
  /////////////////////////////////////////////////////////////////////////
  // schema.
  /////////////////////////////////////////////////////////////////////////

  virtual Object_id schema_id() const = 0;
  virtual void set_schema_id(Object_id schema_id) = 0;

  /////////////////////////////////////////////////////////////////////////
  // mysql_version_id.
  /////////////////////////////////////////////////////////////////////////

  virtual uint mysql_version_id() const = 0;
  //virtual void set_mysql_version_id(uint mysql_version_id) = 0;

  /////////////////////////////////////////////////////////////////////////
  // options.
  /////////////////////////////////////////////////////////////////////////

  const Properties &options() const
  { return const_cast<Abstract_table *> (this)->options(); }

  virtual Properties &options() = 0;
  virtual bool set_options_raw(const std::string &options_raw) = 0;

  /////////////////////////////////////////////////////////////////////////
  // created.
  /////////////////////////////////////////////////////////////////////////

  virtual ulonglong created() const = 0;
  virtual void set_created(ulonglong created) = 0;

  /////////////////////////////////////////////////////////////////////////
  // last altered.
  /////////////////////////////////////////////////////////////////////////

  virtual ulonglong last_altered() const = 0;
  virtual void set_last_altered(ulonglong last_altered) = 0;

  /////////////////////////////////////////////////////////////////////////
  // enum_table_type.
  /////////////////////////////////////////////////////////////////////////

  enum enum_table_type
  {
    TT_BASE_TABLE= 1,
    TT_USER_VIEW,
    TT_SYSTEM_VIEW
  };

  virtual enum_table_type type() const = 0;

  /////////////////////////////////////////////////////////////////////////
  // Column collection.
  /////////////////////////////////////////////////////////////////////////

  virtual Column *add_column() = 0;

  virtual Column_const_iterator *columns() const = 0;

  virtual Column_iterator *columns() = 0;

  virtual Column_const_iterator *user_columns() const = 0;

  virtual Column_iterator *user_columns() = 0;

  virtual const Column *get_column(const std::string name) const = 0;

#ifndef DBUG_OFF
  /**
    Allocate a new object graph and invoke the copy contructor for
    each object. Only used in unit testing.

    @return pointer to dynamically allocated copy
  */
  virtual Abstract_table *clone() const = 0;
#endif /* !DBUG_OFF */
};

///////////////////////////////////////////////////////////////////////////
}

#endif // DD__ABSTRACT_TABLE_INCLUDED
