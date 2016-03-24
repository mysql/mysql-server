/* Copyright (c) 2014, 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef DD__INDEX_INCLUDED
#define DD__INDEX_INCLUDED

#include "my_global.h"

#include "dd/types/entity_object.h"    // dd::Entity_object

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Column;
class Index_element;
class Object_table;
class Object_type;
class Properties;
class Table;
template <typename I> class Iterator;
typedef Iterator<Index_element>       Index_element_iterator;
typedef Iterator<const Index_element> Index_element_const_iterator;

///////////////////////////////////////////////////////////////////////////

class Index : virtual public Entity_object
{
public:
  static const Object_type &TYPE();
  static const Object_table &OBJECT_TABLE();

public:
  enum enum_index_type // similar to Keytype in sql_class.h but w/o FOREIGN_KEY
  {
    IT_PRIMARY= 1,
    IT_UNIQUE,
    IT_MULTIPLE,
    IT_FULLTEXT,
    IT_SPATIAL
  };

  enum enum_index_algorithm // similar to ha_key_alg
  {
    IA_SE_SPECIFIC= 1,
    IA_BTREE,
    IA_RTREE,
    IA_HASH,
    IA_FULLTEXT
  };

public:
  virtual ~Index()
  { };

  /////////////////////////////////////////////////////////////////////////
  // Table.
  /////////////////////////////////////////////////////////////////////////

  const Table &table() const
  { return const_cast<Index *> (this)->table(); }

  virtual Table &table() = 0;

  /////////////////////////////////////////////////////////////////////////
  // is_generated
  /////////////////////////////////////////////////////////////////////////

  virtual bool is_generated() const = 0;
  virtual void set_generated(bool generated) = 0;

  /////////////////////////////////////////////////////////////////////////
  // hidden.
  /////////////////////////////////////////////////////////////////////////

  virtual bool is_hidden() const = 0;
  virtual void set_hidden(bool hidden) = 0;

  /////////////////////////////////////////////////////////////////////////
  // comment.
  /////////////////////////////////////////////////////////////////////////

  virtual const std::string &comment() const = 0;
  virtual void set_comment(const std::string &comment) = 0;

  /////////////////////////////////////////////////////////////////////////
  // Options.
  /////////////////////////////////////////////////////////////////////////

  const Properties &options() const
  { return const_cast<Index *> (this)->options(); }

  virtual Properties &options() = 0;
  virtual bool set_options_raw(const std::string &options_raw) = 0;

  /////////////////////////////////////////////////////////////////////////
  // se_private_data.
  /////////////////////////////////////////////////////////////////////////

  const Properties &se_private_data() const
  { return const_cast<Index *> (this)->se_private_data(); }

  virtual Properties &se_private_data() = 0;
  virtual bool set_se_private_data_raw(const std::string &se_private_data_raw) = 0;
  virtual void set_se_private_data(const Properties &se_private_data)= 0;

  /////////////////////////////////////////////////////////////////////////
  // Tablespace.
  /////////////////////////////////////////////////////////////////////////

  virtual Object_id tablespace_id() const = 0;
  virtual void set_tablespace_id(Object_id tablespace_id) = 0;

  /////////////////////////////////////////////////////////////////////////
  // Engine.
  /////////////////////////////////////////////////////////////////////////

  virtual const std::string &engine() const = 0;
  virtual void set_engine(const std::string &engine) = 0;

  /////////////////////////////////////////////////////////////////////////
  // Index type.
  /////////////////////////////////////////////////////////////////////////

  virtual enum_index_type type() const = 0;
  virtual void set_type(enum_index_type type) = 0;

  /////////////////////////////////////////////////////////////////////////
  // Index algorithm.
  /////////////////////////////////////////////////////////////////////////

  virtual enum_index_algorithm algorithm() const = 0;
  virtual void set_algorithm(enum_index_algorithm algorithm) = 0;

  virtual bool is_algorithm_explicit() const = 0;
  virtual void set_algorithm_explicit(bool alg_expl) = 0;

  /////////////////////////////////////////////////////////////////////////
  // Index-element collection.
  /////////////////////////////////////////////////////////////////////////

  virtual Index_element *add_element(Column *c) = 0;

  virtual Index_element *add_element(const Index_element &e) = 0;

  virtual Index_element_const_iterator *elements() const = 0;

  virtual Index_element_iterator *elements() = 0;

  virtual Index_element_const_iterator *user_elements() const = 0;

  virtual Index_element_iterator *user_elements() = 0;

  virtual uint user_elements_count() const = 0;

  virtual void set_ordinal_position(uint ordinal_position) = 0;

  virtual uint ordinal_position() const = 0;

  /////////////////////////////////////////////////////////////////////////
  // Drop this index from the collection.
  /////////////////////////////////////////////////////////////////////////

  virtual void drop() = 0;


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
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__INDEX_INCLUDED
