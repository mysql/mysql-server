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

#ifndef DD__TABLESPACE_FILES_INCLUDED
#define DD__TABLESPACE_FILES_INCLUDED

#include "my_global.h"

#include "dd/types/weak_object.h"   // dd::Weak_object

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Object_type;
class Object_table;
class Properties;
class Tablespace;

///////////////////////////////////////////////////////////////////////////

class Tablespace_file : virtual public Weak_object
{
public:
  static const Object_type &TYPE();
  static const Object_table &OBJECT_TABLE();

public:
  virtual ~Tablespace_file()
  { };

  /////////////////////////////////////////////////////////////////////////
  // ordinal_position.
  /////////////////////////////////////////////////////////////////////////

  virtual uint ordinal_position() const = 0;

  /////////////////////////////////////////////////////////////////////////
  // filename.
  /////////////////////////////////////////////////////////////////////////

  virtual const std::string &filename() const = 0;
  virtual void set_filename(const std::string &filename) = 0;

  /////////////////////////////////////////////////////////////////////////
  // se_private_data.
  /////////////////////////////////////////////////////////////////////////

  const Properties &se_private_data() const
  { return const_cast<Tablespace_file *> (this)->se_private_data(); }

  virtual Properties &se_private_data() = 0;
  virtual bool set_se_private_data_raw(const std::string &se_private_data_raw) = 0;

  /////////////////////////////////////////////////////////////////////////
  // tablespace.
  /////////////////////////////////////////////////////////////////////////

  const Tablespace &tablespace() const
  { return const_cast<Tablespace_file *> (this)->tablespace(); }

  virtual Tablespace &tablespace() = 0;

  /////////////////////////////////////////////////////////////////////////
  // Drop this tablespace-file from the collection.
  /////////////////////////////////////////////////////////////////////////

  virtual void drop() = 0;
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__TABLESPACE_FILES_INCLUDED
