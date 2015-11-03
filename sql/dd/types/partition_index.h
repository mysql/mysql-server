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

#ifndef DD__PARTITION_INDEX_INCLUDED
#define DD__PARTITION_INDEX_INCLUDED

#include "my_global.h"

#include "dd/types/index.h"           // dd::Index
#include "dd/types/weak_object.h"     // dd::Weak_object

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Index;
class Object_type;
class Partition;
class Tablespace;

class Properties;

///////////////////////////////////////////////////////////////////////////

class Partition_index : virtual public Weak_object
{
public:
  static const Object_type &TYPE();
  static const Object_table &OBJECT_TABLE();

public:
  virtual ~Partition_index()
  { };

  /////////////////////////////////////////////////////////////////////////
  // Partition.
  /////////////////////////////////////////////////////////////////////////

  const Partition &partition() const
  { return const_cast<Partition_index *> (this)->partition(); }

  virtual Partition &partition() = 0;

  /////////////////////////////////////////////////////////////////////////
  // Index.
  /////////////////////////////////////////////////////////////////////////

  const Index &index() const
  { return const_cast<Partition_index *> (this)->index(); }

  virtual Index &index() = 0;

  const std::string &name() const
  { return index().name(); }

  /////////////////////////////////////////////////////////////////////////
  // Options.
  /////////////////////////////////////////////////////////////////////////

  const Properties &options() const
  { return const_cast<Partition_index *> (this)->options(); }

  virtual Properties &options() = 0;
  virtual bool set_options_raw(const std::string &options_raw) = 0;

  /////////////////////////////////////////////////////////////////////////
  // se_private_data.
  /////////////////////////////////////////////////////////////////////////

  const Properties &se_private_data() const
  { return const_cast<Partition_index *> (this)->se_private_data(); }

  virtual Properties &se_private_data() = 0;
  virtual bool set_se_private_data_raw(
                 const std::string &se_private_data_raw) = 0;

  virtual void set_se_private_data(const Properties &se_private_data)= 0;

  /////////////////////////////////////////////////////////////////////////
  // Tablespace.
  /////////////////////////////////////////////////////////////////////////

  virtual Object_id tablespace_id() const = 0;
  virtual void set_tablespace_id(Object_id tablespace_id) = 0;

  /////////////////////////////////////////////////////////////////////////
  // Drop this partition_index from the collection.
  /////////////////////////////////////////////////////////////////////////

  virtual void drop() = 0;
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__PARTITION_INDEX_INCLUDED
