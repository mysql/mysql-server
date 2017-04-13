/* Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef DD_TABLES__VERSION_INCLUDED
#define DD_TABLES__VERSION_INCLUDED

#include <sys/types.h>
#include <string>

#include "dd/impl/types/object_table_impl.h"

class THD;

namespace dd {
namespace tables {

///////////////////////////////////////////////////////////////////////////

// The version of the current DD schema
static const uint TARGET_DD_VERSION= 1;

///////////////////////////////////////////////////////////////////////////

class Version : public Object_table_impl
{
public:
  // The version table always uses version == 0.
  uint default_dd_version(THD*) const
  { return 0; }

  static const Version &instance();

  static const String_type &table_name()
  {
    static String_type s_table_name("version");
    return s_table_name;
  }

public:
  enum enum_fields
  {
    FIELD_VERSION
  };

public:
  Version();

  virtual const String_type &name() const
  { return Version::table_name(); }

  static uint get_target_dd_version()
  { return TARGET_DD_VERSION; }

  uint get_actual_dd_version(THD *thd) const;
};

///////////////////////////////////////////////////////////////////////////

}
}

#endif // DD_TABLES__VERSION_INCLUDED
