/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef DD__COLUMN_STATISTIC_INCLUDED
#define DD__COLUMN_STATISTIC_INCLUDED

#include "my_alloc.h"                     // MEM_ROOT
#include "sql/dd/sdi_fwd.h"               // RJ_Document

class THD;

namespace histograms {
  class Histogram;
}

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Entity_object_table;
class Item_name_key;
class Object_type;
class Primary_id_key;
class Void_key;

namespace tables {
  class Column_statistics;
}

///////////////////////////////////////////////////////////////////////////

class Column_statistics : virtual public Entity_object
{
protected:
  /// MEM_ROOT on which the histogram data is allocated.
  MEM_ROOT m_mem_root;
public:
  static const Object_type &TYPE();
  static const Entity_object_table &OBJECT_TABLE();

  typedef Column_statistics cache_partition_type;
  typedef tables::Column_statistics cache_partition_table_type;
  typedef Primary_id_key id_key_type;
  typedef Item_name_key name_key_type;
  typedef Void_key aux_key_type;

  // We need a set of functions to update a preallocated key.
  bool update_id_key(id_key_type *key) const
  { return update_id_key(key, id()); }

  static bool update_id_key(id_key_type *key, Object_id id);

  bool update_name_key(name_key_type *key) const
  { return update_name_key(key, name()); }

  static bool update_name_key(name_key_type *key, const String_type &name);

  bool update_aux_key(aux_key_type*) const
  { return true; }

  virtual ~Column_statistics()
  { };

  virtual const String_type &schema_name() const = 0;
  virtual void set_schema_name(const String_type &schema_name) = 0;

  virtual const String_type &table_name() const = 0;
  virtual void set_table_name(const String_type &table_name) = 0;

  virtual const String_type &column_name() const = 0;
  virtual void set_column_name(const String_type &column_name) = 0;

  virtual const histograms::Histogram *histogram() const = 0;
  virtual void set_histogram(const histograms::Histogram *histogram) = 0;

  /**
    Converts *this into json.

    Converts all member variables that are to be included in the sdi
    into json by transforming them appropriately and passing them to
    the rapidjson writer provided.

    @param wctx opaque context for data needed by serialization
    @param w rapidjson writer which will perform conversion to json

  */
  virtual void serialize(Sdi_wcontext *wctx, Sdi_writer *w) const = 0;


  /**
    Re-establishes the state of *this by reading sdi information from
    the rapidjson DOM subobject provided.

    Cross-references encountered within this object are tracked in
    sdictx, so that they can be updated when the entire object graph
    has been established.

    @param rctx stores book-keeping information for the
    deserialization process
    @param val subobject of rapidjson DOM containing json
    representation of this object
  */
  virtual bool deserialize(Sdi_rcontext *rctx, const RJ_Value &val) = 0;

  /*
    Create a unique name for a column statistic object based on the triplet
    SCHEMA_NAME TABLE_NAME COLUMN_NAME separated with the 'Unit Separator'
    character.
  */
  static String_type create_name(const String_type &schema_name,
                                 const String_type &table_name,
                                 const String_type &column_name);

  String_type create_name() const
  {
    return Column_statistics::create_name(schema_name(), table_name(),
                                          column_name());
  }

  static void create_mdl_key(const String_type &schema_name,
                             const String_type &table_name,
                             const String_type &column_name,
                             MDL_key *key);

  void create_mdl_key(MDL_key *key) const
  {
    Column_statistics::create_mdl_key(schema_name(), table_name(),
                                      column_name(), key);
  }

  virtual Column_statistics *clone() const = 0;
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__COLUMN_STATISTIC_INCLUDED
