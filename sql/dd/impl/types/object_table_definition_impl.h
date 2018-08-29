/* Copyright (c) 2014, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef DD__OBJECT_TABLE_DEFINITION_IMPL_INCLUDED
#define DD__OBJECT_TABLE_DEFINITION_IMPL_INCLUDED

#include <map>
#include <memory>
#include <vector>

#include "m_string.h"  // my_stpcpy
#include "my_dbug.h"
#include "sql/dd/properties.h"                     // dd::tables::DD_properties
#include "sql/dd/string_type.h"                    // dd::String_type
#include "sql/dd/types/object_table_definition.h"  // dd::Object_table_definition
#include "sql/dd/types/table.h"                    // dd::Table
#include "sql/mysqld.h"                            // lower_case_table_names

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Object_table_definition_impl : public Object_table_definition {
 public:
  typedef std::map<String_type, int> Element_numbers;
  typedef std::map<int, String_type> Element_definitions;

 private:
  enum class Label {
    NAME,
    FIELDS,
    INDEXES,
    FOREIGN_KEYS,
    OPTIONS,
    LABEL,
    POSITION,
    DEFINITION,
    ELEMENT
  };

  static const char *key(Label label) {
    switch (label) {
      case Label::NAME:
        return "name";
      case Label::FIELDS:
        return "fields";
      case Label::INDEXES:
        return "indexes";
      case Label::FOREIGN_KEYS:
        return "foreign_keys";
      case Label::OPTIONS:
        return "options";
      case Label::LABEL:
        return "lbl";
      case Label::POSITION:
        return "pos";
      case Label::DEFINITION:
        return "def";
      case Label::ELEMENT:
        return "elem";
      default:
        DBUG_ASSERT(false);
        return "";
    }
  }

  String_type m_schema_name;
  String_type m_table_name;

  String_type m_ddl_statement;

  Element_numbers m_field_numbers;
  Element_definitions m_field_definitions;

  Element_numbers m_index_numbers;
  Element_definitions m_index_definitions;

  Element_numbers m_foreign_key_numbers;
  Element_definitions m_foreign_key_definitions;

  Element_numbers m_option_numbers;
  Element_definitions m_option_definitions;

  std::vector<String_type> m_dml_statements;

  void add_element(int element_number, const String_type &element_name,
                   const String_type &element_definition,
                   Element_numbers *element_numbers,
                   Element_definitions *element_definitions) {
    DBUG_ASSERT(element_numbers != nullptr && element_definitions != nullptr &&
                element_numbers->find(element_name) == element_numbers->end() &&
                element_definitions->find(element_number) ==
                    element_definitions->end());

    (*element_numbers)[element_name] = element_number;
    (*element_definitions)[element_number] = element_definition;
  }

  int element_number(const String_type &element_name,
                     const Element_numbers &element_numbers) const {
    DBUG_ASSERT(element_numbers.find(element_name) != element_numbers.end());
    return element_numbers.find(element_name)->second;
  }

  void get_element_properties(dd::Properties *properties,
                              const Element_numbers &element_numbers,
                              const Element_definitions &element_defs) const {
    DBUG_ASSERT(properties != nullptr);
    DBUG_ASSERT(element_numbers.size() == element_defs.size());
    int count = 0;
    for (auto it : element_numbers) {
      std::unique_ptr<Properties> element(Properties::parse_properties(""));
      Element_definitions::const_iterator element_def =
          element_defs.find(it.second);
      DBUG_ASSERT(element_def != element_defs.end());
      element->set(key(Label::LABEL), it.first);
      element->set_int32(key(Label::POSITION), it.second);
      element->set(key(Label::DEFINITION), element_def->second);

      Stringstream_type ss;
      ss << "elem" << count++;
      properties->set(ss.str(), element->raw_string());
    }
  }

  bool set_element_properties(const String_type &prop_str,
                              Element_numbers *element_numbers,
                              Element_definitions *element_defs) {
    DBUG_ASSERT(element_numbers != nullptr);
    DBUG_ASSERT(element_defs != nullptr);
    std::unique_ptr<Properties> properties(
        Properties::parse_properties(prop_str));
    for (Properties::Iterator it = properties->begin(); it != properties->end();
         ++it) {
      String_type label;
      int pos = 0;
      String_type def;
      std::unique_ptr<Properties> element(
          Properties::parse_properties(it->second));
      if (element->get(key(Label::LABEL), label) ||
          element->get_int32(key(Label::POSITION), &pos) ||
          element->get(key(Label::DEFINITION), def))
        return true;

      add_element(pos, label, def, element_numbers, element_defs);
    }
    return false;
  }

 public:
  Object_table_definition_impl() {}

  Object_table_definition_impl(const String_type &schema_name,
                               const String_type &table_name,
                               const String_type &ddl_statement)
      : m_schema_name(schema_name),
        m_table_name(table_name),
        m_ddl_statement(ddl_statement) {}

  virtual ~Object_table_definition_impl() {}

  /**
    Get the collation which is used for names related to the file
    system (e.g. a schema name or table name). This collation is
    case sensitive or not, depending on the setting of lower_case-
    table_names.

    @return Pointer to CHARSET_INFO.
   */

  static const CHARSET_INFO *fs_name_collation() {
    if (lower_case_table_names == 0) return &my_charset_utf8_bin;
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

  static const char *fs_name_case(const String_type &src, char *buf) {
    const char *tmp_name = src.c_str();
    if (lower_case_table_names == 2) {
      // Lower case table names == 2 is tested on OSX.
      /* purecov: begin tested */
      my_stpcpy(buf, tmp_name);
      my_casedn_str(fs_name_collation(), buf);
      tmp_name = buf;
      /* purecov: end */
    }
    return tmp_name;
  }

  const String_type &get_table_name() const { return m_table_name; }

  void set_table_name(const String_type &name) { m_table_name = name; }

  void set_schema_name(const String_type &name) { m_schema_name = name; }

  void add_field(int field_number, const String_type &field_name,
                 const String_type field_definition) {
    add_element(field_number, field_name, field_definition, &m_field_numbers,
                &m_field_definitions);
  }

  virtual void add_index(int index_number, const String_type &index_name,
                         const String_type &index_definition) {
    add_element(index_number, index_name, index_definition, &m_index_numbers,
                &m_index_definitions);
  }

  virtual void add_foreign_key(int foreign_key_number,
                               const String_type &foreign_key_name,
                               const String_type &foreign_key_definition) {
    add_element(foreign_key_number, foreign_key_name, foreign_key_definition,
                &m_foreign_key_numbers, &m_foreign_key_definitions);
  }

  virtual void add_option(int option_number, const String_type &option_name,
                          const String_type &option_definition) {
    add_element(option_number, option_name, option_definition,
                &m_option_numbers, &m_option_definitions);
  }

  virtual void add_populate_statement(const String_type &statement) {
    m_dml_statements.push_back(statement);
  }

  virtual int field_number(const String_type &field_name) const {
    return element_number(field_name, m_field_numbers);
  }

  virtual int index_number(const String_type &index_name) const {
    return element_number(index_name, m_index_numbers);
  }

  virtual int option_number(const String_type &option_name) const {
    return element_number(option_name, m_option_numbers);
  }

  virtual String_type get_ddl() const {
    /*
      If a DDL statement has been assigned, we return it. Otherwise, we
      create one based on the element maps.
    */
    if (!m_ddl_statement.empty()) return m_ddl_statement;

    Stringstream_type ss;
    ss << "CREATE TABLE ";

    // Output schema name if non-empty.
    if (!m_schema_name.empty()) ss << m_schema_name << ".";

    ss << m_table_name + "(\n";

    // Output fields
    for (auto field : m_field_definitions) {
      if (field != *m_field_definitions.begin()) ss << ",\n";
      ss << "  " << field.second;
    }

    // Output indexes
    for (auto index : m_index_definitions) ss << ",\n  " << index.second;

    // Output foreign keys
    for (auto key : m_foreign_key_definitions) ss << ",\n  " << key.second;

    ss << "\n)";

    // Output options
    for (auto option : m_option_definitions) ss << " " << option.second;

    return ss.str();
  }

  virtual const std::vector<String_type> &get_dml() const {
    return m_dml_statements;
  }

  virtual void store_into_properties(Properties *table_def_properties) const {
    DBUG_ASSERT(table_def_properties != nullptr);
    table_def_properties->set(key(Label::NAME), m_table_name);

    Properties *field_props = Properties::parse_properties("");
    get_element_properties(field_props, m_field_numbers, m_field_definitions);
    table_def_properties->set(key(Label::FIELDS), field_props->raw_string());
    delete field_props;

    Properties *index_props = Properties::parse_properties("");
    get_element_properties(index_props, m_index_numbers, m_index_definitions);
    table_def_properties->set(key(Label::INDEXES), index_props->raw_string());
    delete index_props;

    Properties *fk_props = Properties::parse_properties("");
    get_element_properties(fk_props, m_foreign_key_numbers,
                           m_foreign_key_definitions);
    table_def_properties->set(key(Label::FOREIGN_KEYS), fk_props->raw_string());
    delete fk_props;

    Properties *option_props = Properties::parse_properties("");
    get_element_properties(option_props, m_option_numbers,
                           m_option_definitions);
    table_def_properties->set(key(Label::OPTIONS), option_props->raw_string());
    delete option_props;
  }

  virtual bool restore_from_string(const String_type &ddl_statement) {
    m_ddl_statement = ddl_statement;
    return false;
  }

  virtual bool restore_from_properties(const Properties &table_def_properties) {
    String_type property_str;
    if (table_def_properties.get(key(Label::NAME), m_table_name) ||
        table_def_properties.get(key(Label::FIELDS), property_str) ||
        set_element_properties(property_str, &m_field_numbers,
                               &m_field_definitions) ||
        table_def_properties.get(key(Label::INDEXES), property_str) ||
        set_element_properties(property_str, &m_index_numbers,
                               &m_index_definitions) ||
        table_def_properties.get(key(Label::FOREIGN_KEYS), property_str) ||
        set_element_properties(property_str, &m_foreign_key_numbers,
                               &m_foreign_key_definitions) ||
        table_def_properties.get(key(Label::OPTIONS), property_str) ||
        set_element_properties(property_str, &m_option_numbers,
                               &m_option_definitions))
      return true;

    return false;
  }
};

///////////////////////////////////////////////////////////////////////////

}  // namespace dd

#endif  // DD__OBJECT_TABLE_DEFINITION_IMPL_INCLUDED
