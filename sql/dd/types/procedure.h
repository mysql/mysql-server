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

#ifndef DD__PROCEDURE_INCLUDED
#define DD__PROCEDURE_INCLUDED


#include "sql/dd/types/routine.h" // Routine

namespace dd {

class Procedure_impl;

///////////////////////////////////////////////////////////////////////////

class Procedure : virtual public Routine
{
public:
  typedef Procedure_impl Impl;

  virtual bool update_name_key(Name_key *key) const
  { return update_routine_name_key(key, schema_id(), name()); }

  static bool update_name_key(Name_key *key,
                              Object_id schema_id,
                              const String_type &name);

public:
  virtual ~Procedure()
  { };

public:

  /**
    Allocate a new object graph and invoke the copy contructor for
    each object. Only used in unit testing.

    @return pointer to dynamically allocated copy
  */
  virtual Procedure *clone() const = 0;
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__PROCEDURE_INCLUDED
