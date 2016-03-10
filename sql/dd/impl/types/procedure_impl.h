/* Copyright (c) 2016 Oracle and/or its affiliates. All rights reserved.

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

#ifndef DD__PROCEDURE_IMPL_INCLUDED
#define DD__PROCEDURE_IMPL_INCLUDED

#include "my_global.h"

#include "dd/impl/types/routine_impl.h"        // dd::Routine_impl
#include "dd/types/object_type.h"              // dd::Object_type
#include "dd/types/procedure.h"                // dd::Procedure

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Procedure_impl : virtual public Routine_impl,
                       virtual public Procedure
{
public:
  Procedure_impl()
  { }

  virtual ~Procedure_impl()
  { }

public:

  virtual bool update_routine_name_key(name_key_type *key,
                                       Object_id schema_id,
                                       const std::string &name) const;

  virtual void debug_print(std::string &outb) const;

private:
  Procedure_impl(const Procedure_impl &src);
  Procedure_impl *clone() const
  {
    return new Procedure_impl(*this);
  }
};

///////////////////////////////////////////////////////////////////////////

class Procedure_type : public Object_type
{
public:
  virtual void register_tables(Open_dictionary_tables_ctx *otx) const;

  virtual Weak_object *create_object() const
  { return new (std::nothrow) Procedure_impl(); }
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__PROCEDURE_IMPL_INCLUDED
