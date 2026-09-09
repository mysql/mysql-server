/* Copyright (c) 2015, 2026, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef DD_UDT_TYPE_INCLUDED
#define DD_UDT_TYPE_INCLUDED

#include <sys/types.h>
#include <memory>  // std:unique_ptr
#include <string>

#include "my_inttypes.h"
#include "sql/dd/string_type.h"

class THD;
namespace dd {
class Schema;
}  // namespace dd

namespace dd {
class UDT_Type;

namespace cache {
class Dictionary_client;
}

bool udt_type_exists(dd::cache::Dictionary_client *client,
                     const char *schema_name, const char *name, bool *exists);

bool create_udt_type(THD *thd, const dd::Schema &sch_obj,
                     const dd::String_type &type_name);

bool drop_udt_type(THD *thd, const dd::UDT_Type &type_def);

}  // namespace dd
#endif  // DD_UDT_TYPE_INCLUDED
