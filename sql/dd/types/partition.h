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

#ifndef DD__PARTITION_INCLUDED
#define DD__PARTITION_INCLUDED

#include "my_global.h"

#include "dd/types/entity_object.h"   // dd::Entity_object

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Index;
class Object_table;
class Object_type;
class Partition_index;
class Partition_value;
class Properties;
class Table;
template <typename I> class Iterator;
typedef Iterator<Partition_index>        Partition_index_iterator;
typedef Iterator<const Partition_index>  Partition_index_const_iterator;
typedef Iterator<Partition_value>        Partition_value_iterator;
typedef Iterator<const Partition_value>  Partition_value_const_iterator;

///////////////////////////////////////////////////////////////////////////

class Partition : virtual public Entity_object
{
public:
  static const Object_type &TYPE();
  static const Object_table &OBJECT_TABLE();

public:
  virtual ~Partition()
  { };

  /////////////////////////////////////////////////////////////////////////
  // Table.
  /////////////////////////////////////////////////////////////////////////

  const Table &table() const
  { return const_cast<Partition *> (this)->table(); }

  virtual Table &table() = 0;

  /////////////////////////////////////////////////////////////////////////
  // level.
  /////////////////////////////////////////////////////////////////////////

  virtual uint level() const = 0;
  virtual void set_level(uint level) = 0;

  /////////////////////////////////////////////////////////////////////////
  // number.
  /////////////////////////////////////////////////////////////////////////

  virtual uint number() const = 0;
  virtual void set_number(uint number) = 0;

  /////////////////////////////////////////////////////////////////////////
  // engine.
  /////////////////////////////////////////////////////////////////////////

  virtual const std::string &engine() const = 0;
  virtual void set_engine(const std::string &engine) = 0;

  /////////////////////////////////////////////////////////////////////////
  // Comment.
  /////////////////////////////////////////////////////////////////////////

  virtual const std::string &comment() const = 0;
  virtual void set_comment(const std::string &comment) = 0;

  /////////////////////////////////////////////////////////////////////////
  // Options.
  /////////////////////////////////////////////////////////////////////////

  const Properties &options() const
  { return const_cast<Partition *> (this)->options(); }

  virtual Properties &options() = 0;
  virtual bool set_options_raw(const std::string &options_raw) = 0;

  /////////////////////////////////////////////////////////////////////////
  // se_private_data.
  /////////////////////////////////////////////////////////////////////////

  const Properties &se_private_data() const
  { return const_cast<Partition *> (this)->se_private_data(); }

  virtual Properties &se_private_data() = 0;

  virtual bool set_se_private_data_raw(
                 const std::string &se_private_data_raw) = 0;

  virtual void set_se_private_data(const Properties &se_private_data)= 0;

  /////////////////////////////////////////////////////////////////////////
  // se_private_id.
  /////////////////////////////////////////////////////////////////////////

  virtual ulonglong se_private_id() const = 0;
  virtual void set_se_private_id(ulonglong se_private_id) = 0;

  /////////////////////////////////////////////////////////////////////////
  // Tablespace.
  /////////////////////////////////////////////////////////////////////////

  virtual Object_id tablespace_id() const = 0;
  virtual void set_tablespace_id(Object_id tablespace_id) = 0;

  /////////////////////////////////////////////////////////////////////////
  // Partition-value collection.
  /////////////////////////////////////////////////////////////////////////

  virtual Partition_value *add_value() = 0;

  virtual Partition_value_const_iterator *values() const = 0;

  virtual Partition_value_iterator *values() = 0;

  /////////////////////////////////////////////////////////////////////////
  // Partition-index collection.
  /////////////////////////////////////////////////////////////////////////

  virtual Partition_index *add_index(Index *idx) = 0;

  virtual Partition_index_const_iterator *indexes() const = 0;

  virtual Partition_index_iterator *indexes() = 0;

  /////////////////////////////////////////////////////////////////////////
  // Drop this partition from the collection.
  /////////////////////////////////////////////////////////////////////////

  virtual void drop() = 0;
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__PARTITION_INCLUDED
