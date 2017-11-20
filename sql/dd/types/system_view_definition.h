/* Copyright (c) 2017 Oracle and/or its affiliates. All rights reserved.

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

#ifndef DD__SYSTEM_VIEW_DEFINITION_INCLUDED
#define DD__SYSTEM_VIEW_DEFINITION_INCLUDED

#include "sql/dd/string_type.h"                // dd::String_type

namespace dd {
namespace system_views {

/*
  The purpose of this interface is to prepare the DDL statements
  necessary to create a I_S table.
*/

class System_view_definition
{
public:
  virtual ~System_view_definition()
  { };

  /**
    Build CREATE VIEW DDL statement for the system view.

    @return String_type containing the DDL statement for the target view.
  */
  virtual String_type build_ddl_create_view() const= 0;
};

}
}

#endif	// DD__SYSTEM_VIEW_DEFINITION_INCLUDED
