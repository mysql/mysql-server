#ifndef PLUGIN_TABLE_INCLUDED
#define PLUGIN_TABLE_INCLUDED

/*
   Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; version 2 of
   the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "sql/sql_list.h"      // List

/**
  Class to hold information regarding a table to be created on
  behalf of a plugin. The class stores the name, definition, options
  and optional tablespace of the table. The definition should not contain the
  'CREATE TABLE name' prefix.

  @note The data members are not owned by the class, and will not
        be deleted when this instance is deleted.
*/
class Plugin_table
{
private:
  const char *m_schema_name;
  const char *m_table_name;
  const char *m_table_definition;
  const char *m_table_options;
  const char *m_tablespace_name;

public:
  Plugin_table(const char *schema_name,
               const char *table_name,
               const char *definition,
               const char *options,
               const char *tablespace_name)
  : m_schema_name(schema_name),
    m_table_name(table_name),
    m_table_definition(definition),
    m_table_options(options),
    m_tablespace_name(tablespace_name)
  { }

  const char *get_schema_name() const
  { return m_schema_name; }

  const char *get_name() const
  { return m_table_name; }

  const char *get_table_definition() const
  { return m_table_definition; }

  const char *get_table_options() const
  { return m_table_options; }

  const char *get_tablespace_name() const
  { return m_tablespace_name; }
};

/**
  Class to hold information regarding a predefined tablespace
  created by a storage engine. The class stores the name, options,
  se_private_data, comment and engine of the tablespace. A list of
  of the tablespace files is also stored.

  @note The data members are not owned by the class, and will not
        be deleted when this instance is deleted.
*/
class Plugin_tablespace
{
public:
  class Plugin_tablespace_file
  {
  private:
    const char *m_name;
    const char *m_se_private_data;
  public:
    Plugin_tablespace_file(const char *name, const char *se_private_data):
      m_name(name),
      m_se_private_data(se_private_data)
    { }

    const char *get_name() const
    { return m_name; }

    const char *get_se_private_data() const
    { return m_se_private_data; }
  };

private:
  const char *m_name;
  const char *m_options;
  const char *m_se_private_data;
  const char *m_comment;
  const char *m_engine;
  List<const Plugin_tablespace_file> m_files;

public:
  Plugin_tablespace(const char *name, const char *options,
                    const char *se_private_data, const char *comment,
                    const char *engine):
    m_name(name),
    m_options(options),
    m_se_private_data(se_private_data),
    m_comment(comment),
    m_engine(engine)
  { }

  void add_file(const Plugin_tablespace_file *file)
  { m_files.push_back(file); }

  const char *get_name() const
  { return m_name; }

  const char *get_options() const
  { return m_options; }

  const char *get_se_private_data() const
  { return m_se_private_data; }

  const char *get_comment() const
  { return m_comment; }

  const char *get_engine() const
  { return m_engine; }

  const List<const Plugin_tablespace_file> &get_files() const
  { return m_files; }
};

#endif  // PLUGIN_TABLE_INCLUDED
