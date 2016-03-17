/* Copyright (c) 2014, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify it under
   the terms of the GNU General Public License as published by the Free Software
   Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef DD__TABLE_INCLUDED
#define DD__TABLE_INCLUDED

#include "my_global.h"

#include "dd/types/abstract_table.h"   // dd::Abstract_table

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Foreign_key;
class Index;
class Object_type;
class Partition;
typedef Iterator<Foreign_key>         Foreign_key_iterator;
typedef Iterator<const Foreign_key>   Foreign_key_const_iterator;
typedef Iterator<Index>               Index_iterator;
typedef Iterator<const Index>         Index_const_iterator;
typedef Iterator<Partition>           Partition_iterator;
typedef Iterator<const Partition>     Partition_const_iterator;

///////////////////////////////////////////////////////////////////////////

class Table : virtual public Abstract_table
{
public:
  static const Object_type &TYPE();

  // We need a set of functions to update a preallocated se private id key,
  // which requires special handling for table objects.
  virtual bool update_aux_key(aux_key_type *key) const
  { return update_aux_key(key, engine(), se_private_id()); }

  static bool update_aux_key(aux_key_type *key,
                             const std::string &engine,
                             Object_id se_private_id);

public:
  virtual ~Table()
  { };

public:
  /* Keep in sync with subpartition type for forward compatibility.*/
  enum enum_partition_type
  {
    PT_NONE= 0,
    PT_HASH,
    PT_KEY_51,
    PT_KEY_55,
    PT_LINEAR_HASH,
    PT_LINEAR_KEY_51,
    PT_LINEAR_KEY_55,
    PT_RANGE,
    PT_LIST,
    PT_RANGE_COLUMNS,
    PT_LIST_COLUMNS,
    PT_AUTO,
    PT_AUTO_LINEAR,
  };

  enum enum_subpartition_type
  {
    ST_NONE= 0,
    ST_HASH,
    ST_KEY_51,
    ST_KEY_55,
    ST_LINEAR_HASH,
    ST_LINEAR_KEY_51,
    ST_LINEAR_KEY_55
  };

  /* Also used for default subpartitioning. */
  enum enum_default_partitioning
  {
    DP_NONE= 0,
    DP_NO,
    DP_YES,
    DP_NUMBER
  };

public:
  /////////////////////////////////////////////////////////////////////////
  //collation.
  /////////////////////////////////////////////////////////////////////////

  virtual Object_id collation_id() const = 0;
  virtual void set_collation_id(Object_id collation_id) = 0;

  /////////////////////////////////////////////////////////////////////////
  // tablespace.
  /////////////////////////////////////////////////////////////////////////

  virtual Object_id tablespace_id() const = 0;
  virtual void set_tablespace_id(Object_id tablespace_id) = 0;

  /////////////////////////////////////////////////////////////////////////
  // engine.
  /////////////////////////////////////////////////////////////////////////

  virtual const std::string &engine() const = 0;
  virtual void set_engine(const std::string &engine) = 0;

  /////////////////////////////////////////////////////////////////////////
  // comment
  /////////////////////////////////////////////////////////////////////////

  virtual const std::string &comment() const = 0;
  virtual void set_comment(const std::string &comment) = 0;

  /////////////////////////////////////////////////////////////////////////
  // hidden.
  /////////////////////////////////////////////////////////////////////////

  virtual bool hidden() const = 0;
  virtual void set_hidden(bool hidden) = 0;

  /////////////////////////////////////////////////////////////////////////
  // se_private_data.
  /////////////////////////////////////////////////////////////////////////

  const Properties &se_private_data() const
  { return const_cast<Table *> (this)->se_private_data(); }

  virtual Properties &se_private_data() = 0;
  virtual bool set_se_private_data_raw(const std::string &se_private_data_raw) = 0;
  virtual void set_se_private_data(const Properties &se_private_data)= 0;

  /////////////////////////////////////////////////////////////////////////
  // se_private_id.
  /////////////////////////////////////////////////////////////////////////

  virtual ulonglong se_private_id() const = 0;
  virtual void set_se_private_id(ulonglong se_private_id) = 0;

  /////////////////////////////////////////////////////////////////////////
  // Partition related.
  /////////////////////////////////////////////////////////////////////////

  virtual enum_partition_type partition_type() const = 0;
  virtual void set_partition_type(enum_partition_type partition_type) = 0;

  virtual enum_default_partitioning default_partitioning() const = 0;
  virtual void set_default_partitioning(
    enum_default_partitioning default_partitioning) = 0;

  virtual const std::string &partition_expression() const = 0;
  virtual void set_partition_expression(
    const std::string &partition_expression) = 0;

  virtual enum_subpartition_type subpartition_type() const = 0;
  virtual void set_subpartition_type(
    enum_subpartition_type subpartition_type) = 0;

  virtual enum_default_partitioning default_subpartitioning() const = 0;
  virtual void set_default_subpartitioning(
    enum_default_partitioning default_subpartitioning) = 0;

  virtual const std::string &subpartition_expression() const = 0;
  virtual void set_subpartition_expression(
    const std::string &subpartition_expression) = 0;

  /** Dummy method to be able to use Partition and Table interchangeably
  in templates. */
  const Table &table() const
  { return *this; }
  Table &table()
  { return *this; }

  /////////////////////////////////////////////////////////////////////////
  //Index collection.
  /////////////////////////////////////////////////////////////////////////

  virtual Index *add_index() = 0;

  virtual Index *add_first_index() = 0;

  virtual Index_const_iterator *indexes() const = 0;

  virtual Index_iterator *indexes() = 0;

  virtual Index_const_iterator *user_indexes() const = 0;

  virtual Index_iterator *user_indexes() = 0;

  /////////////////////////////////////////////////////////////////////////
  // Foreign key collection.
  /////////////////////////////////////////////////////////////////////////

  virtual Foreign_key *add_foreign_key() = 0;

  virtual Foreign_key_const_iterator *foreign_keys() const = 0;

  virtual Foreign_key_iterator *foreign_keys() = 0;

  /////////////////////////////////////////////////////////////////////////
  // Partition collection.
  /////////////////////////////////////////////////////////////////////////

  virtual Partition *add_partition() = 0;

  virtual Partition_const_iterator *partitions() const = 0;

  virtual Partition_iterator *partitions() = 0;

  virtual const Partition *get_last_partition() const = 0;

  /**
    Allocate a new object graph and invoke the copy contructor for
    each object.

    @return pointer to dynamically allocated copy
  */
  virtual Table *clone() const = 0;

};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__TABLE_INCLUDED
