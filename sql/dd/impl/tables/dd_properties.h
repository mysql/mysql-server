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

#ifndef DD_TABLES__DD_PROPERTIES_INCLUDED
#define DD_TABLES__DD_PROPERTIES_INCLUDED

#include <sys/types.h>
#include <string>

#include "sql/dd/impl/properties_impl.h"            // dd::Properties_impl
#include "sql/dd/impl/types/object_table_impl.h"
#include "sql/dd/string_type.h"

class THD;

namespace dd {
namespace tables {

// The version of the current DD schema
static const uint TARGET_DD_VERSION= 1;

// The version of the current server IS schema
static const uint TARGET_I_S_VERSION= 1;

// The version of the current server PS schema
static const uint TARGET_P_S_VERSION= 1;

// Unknown version of the current server PS schema. It is used for tests.
static const uint UNKNOWN_P_S_VERSION= -1;

///////////////////////////////////////////////////////////////////////////

class DD_properties : public Object_table_impl
{
public:
  DD_properties();

  enum enum_fields
  {
    FIELD_PROPERTIES
  };

  static const DD_properties &instance();

  static const String_type &table_name()
  {
    static String_type s_table_name("dd_properties");
    return s_table_name;
  }

  virtual const String_type &name() const
  { return DD_properties::table_name(); }


public:

  /*
    Methods that relate to store 'DD_version' perperty
    in DD_properties table.
  */

  /**
    The DD version is always 0 for DD_properties table.

    @returns 0 always
  */
  uint default_dd_version(THD*) const
  { return 0; }

  /**
    The DD version required by the current server binaries.

    @returns TARGET_DD_VERSION
  */
  static uint get_target_dd_version()
  { return TARGET_DD_VERSION; }

  /**
    The DD version stored in mysql.dd_properties.

    @param thd         - Thread context.
    @param[out] exists - Will be marked 'false' if dd_properties
                         tables is not present. Otherwise true.

    @returns TARGET_DD_VERSION
  */
  uint get_actual_dd_version(THD *thd,  bool *exists) const;


  /*
    Following are methods that relate to store 'DD_version'
    property in DD_properties table.
  */

  /**
    The IS version required by the current server binaries.

    @returns TARGET_I_S_VERSION
  */
  static uint get_target_I_S_version()
  { return TARGET_I_S_VERSION; }

  /**
    The IS version stored in mysql.dd_properties.

    @param thd - Thread context.

    @returns TARGET_I_S_VERSION
  */
  uint get_actual_I_S_version(THD *thd) const;

  /**
    The PS version required by the current server binaries.

    @returns TARGET_P_S_VERSION
  */
  static uint get_target_P_S_version()
  { return TARGET_P_S_VERSION; }

  /**
    The PS version stored in mysql.dd_properties.

    @param thd - Thread context.

    @returns actual PS version
  */
  uint get_actual_P_S_version(THD *thd) const;

  /**
    Store IS version in mysql.dd_properties.

    @param thd     - Thread context.
    @param version - The version number to stored.

    @returns false on success otherwise true.
  */
  bool set_I_S_version(THD *thd, uint version);

  /**
    Store PS version in mysql.dd_properties.

    @param thd     - Thread context.
    @param version - The version number to stored.

    @returns false on success otherwise true.
  */
  bool set_P_S_version(THD *thd, uint version);

  /**
    Get the dd::Properties raw string of all versions.
    This is used when creating mysql.dd_properties table
    and while upgrading.

    @returns String containing all versions.
  */
  static String_type get_target_versions()
  {
    dd::Properties_impl p;
    p.set_uint32("DD_version", get_target_dd_version());
    p.set_uint32("IS_version", get_target_I_S_version());
    p.set_uint32("PS_version", get_target_P_S_version());
    return p.raw_string();
  }

private:

  /**
    Get the value of given property key stored in dd_properties
    table.

    @param thd         - Thread context.
    @param key         - The key representing property stored.
    @param[out] exists - Will be marked 'false' if dd_properties
                         tables is not present. Otherwise true.

    @returns uint value stored for the key.
  */
  uint get_property(THD *thd, String_type key, bool *exists) const;

  /**
    Set the value of property key in dd_properties table.

    @param thd    - Thread context.
    @param key    - The key representing property.
    @param value  - The value to be stored for 'key'.

    @returns false on success otherwise true.
  */
  bool set_property(THD *thd, String_type key, uint value);

};

///////////////////////////////////////////////////////////////////////////

}
}

#endif // DD_TABLES__DD_PROPERTIES_INCLUDED
