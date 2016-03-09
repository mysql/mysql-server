/* Copyright (c) 2014, 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef DD__OBJECT_TABLE_DEFINITION_IMPL_INCLUDED
#define	DD__OBJECT_TABLE_DEFINITION_IMPL_INCLUDED

#include "my_global.h"
#include "m_string.h"                         // my_stpcpy
#include "mysqld.h"                           // lower_case_table_names
#include "table.h"                            // MYSQL_TABLESPACE_NAME

#include "dd/impl/system_registry.h"          // System_tablespaces
#include "dd/types/object_table_definition.h" // dd::Object_table_definition
#include "dd/types/table.h"                   // dd::Table

#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Object_table_definition_impl: public Object_table_definition
{
private:
  typedef std::map<std::string, int> Field_numbers;
  typedef std::map<int, std::string> Field_definitions;
  typedef std::vector<std::string> Indexes;
  typedef std::vector<std::string> Foreign_keys;
  typedef std::vector<std::string> Options;

private:
  std::string m_table_name;
  std::string m_type_name;

  Field_numbers m_field_numbers;
  Field_definitions m_field_definitions;
  Indexes m_indexes;
  Foreign_keys m_foreign_keys;
  Foreign_keys m_cyclic_foreign_keys;
  Options m_options;
  std::vector<std::string> m_populate_statements;

  uint m_dd_version;

public:
  Object_table_definition_impl(): m_dd_version(0)
  { }

  virtual ~Object_table_definition_impl()
  { }


  /**
    Get the collation which is used for names related to the file
    system (e.g. a schema name or table name). This collation is
    case sensitive or not, depending on the setting of lower_case-
    table_names.

    @return Pointer to CHARSET_INFO.
   */

  static const CHARSET_INFO *fs_name_collation()
  {
     if (lower_case_table_names == 0)
       return &my_charset_utf8_bin;
     return &my_charset_utf8_tolower_ci;
  }


  /**
    Convert to lowercase if lower_case_table_names == 2. This is needed
    e.g when reconstructing name keys from a dictionary object in order
    to remove the object.

    @param          src  String to possibly convert to lowercase.
    @param [in,out] buf  Buffer for storing lowercase'd string. Supplied
                         by the caller.

    @retval  A pointer to the src string if l_c_t_n != 2
    @retval  A pointer to the buf supplied by the caller, into which
             the src string has been copied and lowercase'd, if l_c_t_n == 2
   */

  static const char *fs_name_case(const std::string &src, char *buf)
  {
    const char *tmp_name= src.c_str();
    if (lower_case_table_names == 2)
    {
      // Lower case table names == 2 is tested on OSX.
      /* purecov: begin tested */
      my_stpcpy(buf, tmp_name);
      my_casedn_str(fs_name_collation(), buf);
      tmp_name= buf;
      /* purecov: end */
    }
    return tmp_name;
  }

  virtual uint dd_version() const
  { return m_dd_version; }

  virtual void dd_version(uint version)
  { m_dd_version= version; }

  virtual const std::string &table_name() const
  { return m_table_name; }

  virtual const std::string &type_name() const
  { return m_type_name; }

  virtual void table_name(const std::string &name)
  { m_table_name= name; }

  virtual void type_name(const std::string &type)
  { m_type_name= type; }

  virtual void add_field(int field_number, const std::string &field_name,
                         const std::string field_definition)
  {
    DBUG_ASSERT(
      m_field_numbers.find(field_name) == m_field_numbers.end() &&
      m_field_definitions.find(field_number) == m_field_definitions.end());

    m_field_numbers[field_name]= field_number;
    m_field_definitions[field_number]= field_definition;
  }

  virtual void add_index(const std::string &index)
  { m_indexes.push_back(index); }

  virtual void add_foreign_key(const std::string &foreign_key)
  { m_foreign_keys.push_back(foreign_key); }

  virtual void add_cyclic_foreign_key(const std::string &foreign_key)
  { m_cyclic_foreign_keys.push_back(foreign_key); }

  virtual void add_option(const std::string &option)
  { m_options.push_back(option); }

  virtual void add_populate_statement(const std::string &statement)
  { m_populate_statements.push_back(statement); }

  virtual int field_number(const std::string &field_name) const
  {
    DBUG_ASSERT(m_field_numbers.find(field_name) != m_field_numbers.end());
    return m_field_numbers.find(field_name)->second;
  }

  virtual std::string build_ddl_create_table() const
  {
    std::stringstream ss;
    ss << "CREATE TABLE " + m_table_name + "(\n";

    // Output fields
    for (Field_definitions::const_iterator field= m_field_definitions.begin();
         field != m_field_definitions.end(); ++field)
    {
      if (field != m_field_definitions.begin())
        ss << ",\n";
      ss << "  " << field->second;
    }

    // Output indexes
    for (Indexes::const_iterator index= m_indexes.begin();
         index != m_indexes.end(); ++index)
      ss << ",\n  " << *index;

    // Output foreign keys
    for (Foreign_keys::const_iterator key= m_foreign_keys.begin();
         key != m_foreign_keys.end(); ++key)
      ss << ",\n  " << *key;

    ss << "\n)";

    // Output options
    for (Options::const_iterator option= m_options.begin();
         option != m_options.end(); ++option)
      ss << " " << *option;

    // Optionally output tablespace clause
    if (System_tablespaces::instance()->find(MYSQL_TABLESPACE_NAME.str))
      ss << " " << "TABLESPACE=" << MYSQL_TABLESPACE_NAME.str;

    return ss.str();
  }

  virtual std::string build_ddl_add_cyclic_foreign_keys() const
  {
    std::stringstream ss;
    ss << "ALTER TABLE " + m_table_name + "\n";

    // Output cyclic foreign keys
    for (Foreign_keys::const_iterator key= m_cyclic_foreign_keys.begin();
         key != m_cyclic_foreign_keys.end(); ++key)
    {
      if (key != m_cyclic_foreign_keys.begin())
        ss << ",\n";
      ss << "  ADD " << *key;
    }

    ss << "\n";

    return ss.str();
  }

  virtual const std::vector<std::string> &dml_populate_statements() const
  { return m_populate_statements; }
};

///////////////////////////////////////////////////////////////////////////

}

#endif	// DD__OBJECT_TABLE_DEFINITION_IMPL_INCLUDED

