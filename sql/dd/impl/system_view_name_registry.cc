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

#include "dd/impl/system_view_name_registry.h"

#include "dd/iterator.h"          // dd::Iterator

namespace dd {

///////////////////////////////////////////////////////////////////////////

/// List of information schema table names. They are represented by views.

static const char *system_view_names[]=
{
  "CHARACTER_SETS",
  "COLLATIONS",
  "COLLATION_CHARACTER_SET_APPLICABILITY",
  "COLUMNS",
  "COLUMN_PRIVILEGES",
  "ENGINES",
  "EVENTS",
  "FILES",
  "GLOBAL_STATUS",
  "SESSION_STATUS",
  "GLOBAL_VARIABLES",
  "SESSION_VARIABLES",
  "KEY_COLUMN_USAGE",
  "OPTIMIZER_TRACE",
  "PARAMETERS",
  "PARTITIONS",
  "PLUGINS",
  "PROCESSLIST",
  "PROFILING",
  "REFERENTIAL_CONSTRAINTS",
  "ROUTINES",
  "SCHEMATA",
  "SCHEMA_PRIVILEGES",
  "STATISTICS",
  "TABLES",
  "TABLESPACES",
  "TABLE_CONSTRAINTS",
  "TABLE_PRIVILEGES",
  "TRIGGERS",
  "USER_PRIVILEGES",
  "VIEWS",
  "INNODB_CMP",
  "INNODB_CMP_RESET",
  "INNODB_CMP_PER_INDEX",
  "INNODB_CMPMEM",
  "INNODB_CMPMEM_RESET",
  "INNODB_TRX",
  "INNODB_LOCKS",
  "INNODB_LOCK_WAITS",
  "INNODB_SYS_TABLES",
  "INNODB_SYS_INDEXES",
  "INNODB_SYS_COLUMNS",
  "INNODB_SYS_FIELDS",
  "INNODB_SYS_FOREIGN",
  "INNODB_SYS_FOREIGN_COLS",
  "INNODB_SYS_TABLESTATS",
  "INNODB_SYS_DATAFILES",
  "INNODB_SYS_TABLESPACES",
  "INNODB_BUFFER_PAGE",
  "INNODB_BUFFER_PAGE_LRU",
  "INNODB_BUFFER_POOL_STATS",
  "INNODB_METRICS",
  "INNODB_FT_CONFIG",
  "INNODB_FT_DEFAULT_STOPWORD",
  "INNODB_FT_INDEX_TABLE",
  "INNODB_FT_INDEX_CACHE",
  "INNODB_FT_DELETED",
  "INNODB_FT_BEING_DELETED",
  "INNODB_TEMP_TABLE_INFO",
  (const char *) 0
};

///////////////////////////////////////////////////////////////////////////

class System_view_name_iterator : public Iterator<const char>
{
public:
  System_view_name_iterator()
   :m_current_index(0)
  { }

public:
  virtual const char *next()
  {
    const char *item= system_view_names[m_current_index];

    if (!item)
      return 0;

    ++m_current_index;

    return item;
  }

private:
  int m_current_index;
};

///////////////////////////////////////////////////////////////////////////

Iterator<const char> *System_view_name_registry::names()
{ return new (std::nothrow) System_view_name_iterator(); }

///////////////////////////////////////////////////////////////////////////

}


