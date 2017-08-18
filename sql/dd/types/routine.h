/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef DD__ROUTINE_INCLUDED
#define DD__ROUTINE_INCLUDED

#include "my_inttypes.h"
#include "sql/dd/impl/raw/object_keys.h"  // IWYU pragma: keep
#include "sql/dd/types/entity_object.h"   // dd::Entity_object
#include "sql/dd/types/view.h"            // dd::Column::enum_security_type

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Object_type;
class Void_key;
class Parameter;
class Properties;
class Routine_name_key;

namespace tables {
  class Routines;
}


///////////////////////////////////////////////////////////////////////////

/**
  Abstract base class for functions and procedures.

  @note This class may be inherited along different paths
        for some subclasses due to the diamond shaped
        inheritance hierarchy; thus, direct subclasses
        must inherit this class virtually.
*/

class Routine : virtual public Entity_object
{
public:
  static const Object_type &TYPE();
  static const Entity_object_table &OBJECT_TABLE();

  typedef Routine cache_partition_type;
  typedef tables::Routines cache_partition_table_type;
  typedef Primary_id_key id_key_type;
  typedef Routine_name_key name_key_type;
  typedef Void_key aux_key_type;
  typedef Collection<Parameter *> Parameter_collection;

  // We need a set of functions to update a preallocated key.
  virtual bool update_id_key(id_key_type *key) const
  { return update_id_key(key, id()); }

  static bool update_id_key(id_key_type *key, Object_id id);

  virtual bool update_name_key(name_key_type *key) const
  { return update_routine_name_key(key, schema_id(), name()); }

  virtual bool update_routine_name_key(name_key_type *key,
                                       Object_id schema_id,
                                       const String_type &name) const = 0;

  virtual bool update_aux_key(aux_key_type*) const
  { return true; }

public:
  enum enum_routine_type
  {
    RT_FUNCTION = 1,
    RT_PROCEDURE
  };

  enum enum_sql_data_access
  {
    SDA_CONTAINS_SQL = 1,
    SDA_NO_SQL,
    SDA_READS_SQL_DATA,
    SDA_MODIFIES_SQL_DATA
  };

public:
  virtual ~Routine()
  { };

public:
  /////////////////////////////////////////////////////////////////////////
  // schema.
  /////////////////////////////////////////////////////////////////////////

  virtual Object_id schema_id() const = 0;
  virtual void set_schema_id(Object_id schema_id) = 0;

  /////////////////////////////////////////////////////////////////////////
  // routine type.
  /////////////////////////////////////////////////////////////////////////

  virtual enum_routine_type type() const = 0;

  /////////////////////////////////////////////////////////////////////////
  // definition/utf8.
  /////////////////////////////////////////////////////////////////////////

  virtual const String_type &definition() const = 0;
  virtual void set_definition(const String_type &definition) = 0;

  virtual const String_type &definition_utf8() const = 0;
  virtual void set_definition_utf8(const String_type &definition_utf8) = 0;

  /////////////////////////////////////////////////////////////////////////
  // parameter_str
  /////////////////////////////////////////////////////////////////////////

  virtual const String_type &parameter_str() const = 0;
  virtual void set_parameter_str(const String_type &parameter_str) = 0;

  /////////////////////////////////////////////////////////////////////////
  // is_deterministic.
  /////////////////////////////////////////////////////////////////////////

  virtual bool is_deterministic() const = 0;
  virtual void set_deterministic(bool deterministic) = 0;

  /////////////////////////////////////////////////////////////////////////
  // sql data access.
  /////////////////////////////////////////////////////////////////////////

  virtual enum_sql_data_access sql_data_access() const = 0;
  virtual void set_sql_data_access(enum_sql_data_access sda) = 0;

  /////////////////////////////////////////////////////////////////////////
  // security type.
  /////////////////////////////////////////////////////////////////////////

  virtual View::enum_security_type security_type() const = 0;
  virtual void set_security_type(View::enum_security_type st) = 0;

  /////////////////////////////////////////////////////////////////////////
  // sql_mode
  /////////////////////////////////////////////////////////////////////////

  virtual ulonglong sql_mode() const = 0;
  virtual void set_sql_mode(ulonglong sm) = 0;

  /////////////////////////////////////////////////////////////////////////
  // definer.
  /////////////////////////////////////////////////////////////////////////

  virtual const String_type &definer_user() const = 0;
  virtual const String_type &definer_host() const = 0;
  virtual void set_definer(const String_type &username,
                           const String_type &hostname) = 0;

  /////////////////////////////////////////////////////////////////////////
  // collations.
  /////////////////////////////////////////////////////////////////////////

  virtual Object_id client_collation_id() const = 0;
  virtual void set_client_collation_id(Object_id client_collation_id) = 0;

  virtual Object_id connection_collation_id() const = 0;
  virtual void set_connection_collation_id(
                 Object_id connection_collation_id) = 0;

  virtual Object_id schema_collation_id() const = 0;
  virtual void set_schema_collation_id(Object_id schema_collation_id) = 0;

  /////////////////////////////////////////////////////////////////////////
  // created.
  /////////////////////////////////////////////////////////////////////////

  virtual ulonglong created(bool convert_time) const = 0;
  virtual void set_created(ulonglong created) = 0;

  /////////////////////////////////////////////////////////////////////////
  // last altered.
  /////////////////////////////////////////////////////////////////////////

  virtual ulonglong last_altered(bool convert_time) const = 0;
  virtual void set_last_altered(ulonglong last_altered) = 0;

  /////////////////////////////////////////////////////////////////////////
  // comment.
  /////////////////////////////////////////////////////////////////////////

  virtual const String_type &comment() const = 0;
  virtual void set_comment(const String_type &comment) = 0;

  /////////////////////////////////////////////////////////////////////////
  // parameter collection.
  /////////////////////////////////////////////////////////////////////////

  virtual Parameter *add_parameter() = 0;

  virtual const Parameter_collection &parameters() const = 0;

  /**
    Allocate a new object graph and invoke the copy contructor for
    each object. Only used in unit testing.

    @return pointer to dynamically allocated copy
  */
  virtual Routine *clone() const = 0;
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__ROUTINE_INCLUDED
