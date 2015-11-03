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

#ifndef DD__NON_REPRESENTED_TABLES_INCLUDED
#define	DD__NON_REPRESENTED_TABLES_INCLUDED

#include "my_global.h"

#include "dd/impl/types/object_table_impl.h" // dd::Object_table_impl

#include <string>

namespace dd {
namespace tables {

///////////////////////////////////////////////////////////////////////////

class Innodb_table_stats: virtual public Object_table_impl
{
public:
  static const Innodb_table_stats &instance()
  {
    static Innodb_table_stats s_instance;
    return s_instance;
  }

  static const std::string &table_name()
  {
    static std::string s_table_name("innodb_table_stats");
    return s_table_name;
  }

  virtual bool hidden() const
  { return false; }

public:
  Innodb_table_stats()
  {
    m_target_def.table_name(table_name());

    m_target_def.add_field(0, "FIELD_DATABASE_NAME",
            "database_name VARCHAR(64) NOT NULL");
    m_target_def.add_field(1, "FIELD_TABLE_NAME",
            "table_name VARCHAR(64) NOT NULL");
    m_target_def.add_field(2, "FIELD_LAST_UPDATE",
            "last_update TIMESTAMP NOT NULL NOT NULL\n"
            "  DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP");
    m_target_def.add_field(3, "FIELD_N_ROWS",
            "n_rows BIGINT UNSIGNED NOT NULL");
    m_target_def.add_field(4, "FIELD_CLUSTERED_INDEX_SIZE",
            "clustered_index_size BIGINT UNSIGNED NOT NULL");
    m_target_def.add_field(5, "FIELD_SUM_OF_OTHER_INDEX_SIZES",
            "sum_of_other_index_sizes	BIGINT UNSIGNED NOT NULL");

    m_target_def.add_index("PRIMARY KEY (database_name, table_name)");

    m_target_def.add_option("ENGINE=INNODB");
    m_target_def.add_option("DEFAULT CHARSET=utf8");
    m_target_def.add_option("COLLATE=utf8_bin");
    m_target_def.add_option("STATS_PERSISTENT=0");
  }

  virtual const std::string &name() const
  { return Innodb_table_stats::table_name(); }
};

///////////////////////////////////////////////////////////////////////////

class Innodb_index_stats: virtual public Object_table_impl
{
public:
  static const Innodb_index_stats &instance()
  {
    static Innodb_index_stats s_instance;
    return s_instance;
  }

  static const std::string &table_name()
  {
    static std::string s_table_name("innodb_index_stats");
    return s_table_name;
  }

  virtual bool hidden() const
  { return false; }

public:
  Innodb_index_stats()
  {
    m_target_def.table_name(table_name());

    m_target_def.add_field(0, "FIELD_DATABASE_NAME",
            "database_name VARCHAR(64) NOT NULL");
    m_target_def.add_field(1, "FIELD_TABLE_NAME",
            "table_name VARCHAR(64) NOT NULL");
    m_target_def.add_field(2, "FIELD_INDEX_NAME",
            "index_name VARCHAR(64) NOT NULL");
    m_target_def.add_field(3, "FIELD_LAST_UPDATE",
            "last_update TIMESTAMP NOT NULL NOT NULL\n"
            "  DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP");
    /*
      There are at least: stat_name='size'
                          stat_name='n_leaf_pages'
                          stat_name='n_diff_pfx%'
    */
    m_target_def.add_field(4, "FIELD_STAT_NAME",
            "stat_name VARCHAR(64) NOT NULL");
    m_target_def.add_field(5, "FIELD_STAT_VALUE",
            "stat_value BIGINT UNSIGNED NOT NULL");
    m_target_def.add_field(6, "FIELD_SAMPLE_SIZE",
            "sample_size BIGINT UNSIGNED");
    m_target_def.add_field(7, "FIELD_STAT_DESCRIPTION",
            "stat_description VARCHAR(1024) NOT NULL");

    m_target_def.add_index("PRIMARY KEY (database_name, table_name, "
                           "index_name, stat_name)");

    m_target_def.add_option("ENGINE=INNODB");
    m_target_def.add_option("DEFAULT CHARSET=utf8");
    m_target_def.add_option("COLLATE=utf8_bin");
    m_target_def.add_option("STATS_PERSISTENT=0");
  }

  virtual const std::string &name() const
  { return Innodb_index_stats::table_name(); }
};

///////////////////////////////////////////////////////////////////////////

class Catalogs: virtual public Object_table_impl
{
public:
  static const Catalogs &instance()
  {
    static Catalogs s_instance;
    return s_instance;
  }

  static const std::string &table_name()
  {
    static std::string s_table_name("catalogs");
    return s_table_name;
  }

public:
  Catalogs()
  {
    m_target_def.table_name(table_name());

    m_target_def.add_field(0, "FIELD_ID",
            "id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT");
    m_target_def.add_field(1, "FIELD_NAME",
            "name VARCHAR(64) NOT NULL COLLATE " +
            std::string(Object_table_definition_impl::
                        fs_name_collation()->name));
    m_target_def.add_field(2, "FIELD_CREATED",
            "created TIMESTAMP NOT NULL\n"
            "  DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP");
    m_target_def.add_field(3, "FIELD_LAST_ALTERED",
            "last_altered TIMESTAMP NOT NULL DEFAULT NOW()");

    m_target_def.add_index("PRIMARY KEY (id)");
    m_target_def.add_index("UNIQUE KEY (name)");

    m_target_def.add_option("ENGINE=INNODB");
    m_target_def.add_option("DEFAULT CHARSET=utf8");
    m_target_def.add_option("COLLATE=utf8_bin");
    m_target_def.add_option("STATS_PERSISTENT=0");

    m_target_def.add_populate_statement(
      "INSERT INTO catalogs(id, name, created, last_altered) "
        "VALUES (1, 'def', now(), now())");
  }

  virtual const std::string &name() const
  { return Catalogs::table_name(); }
};

///////////////////////////////////////////////////////////////////////////

}
}

#endif // NON_REPRESENTED_TABLES_INCLUDED

