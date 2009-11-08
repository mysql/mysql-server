/*
   Copyright (C) 2009 Sun Microsystems Inc.
   All rights reserved. Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "Ndbinfo.hpp"


#define DECLARE_NDBINFO_TABLE(var, num)  \
static const struct  {                   \
  Ndbinfo::Table::Members m;             \
  Ndbinfo::Column col[num];              \
} ndbinfo_##var 


DECLARE_NDBINFO_TABLE(TABLES,3) =
{ "tables", 3, 0, "",
  {
    {"table_id",  Ndbinfo::Number, ""},
    {"table_name",Ndbinfo::String, ""},
    {"comment",   Ndbinfo::String, ""},
  }};

DECLARE_NDBINFO_TABLE(COLUMNS,5) =
{ "columns", 5, 0, "",
  {
    {"table_id",    Ndbinfo::Number, ""},
    {"column_id",   Ndbinfo::Number, ""},
    {"column_name", Ndbinfo::String, ""},
    {"column_type", Ndbinfo::Number, ""},
    {"comment",     Ndbinfo::String, ""},
  }};

DECLARE_NDBINFO_TABLE(MEMUSAGE,7) =
{ "memusage", 7, 0, "",
  {
    {"node_id",          Ndbinfo::Number, ""},
    {"block_number",     Ndbinfo::Number, ""},
    {"block_instance",   Ndbinfo::Number, ""},
    {"resource_name",    Ndbinfo::String, ""},
    {"page_size",        Ndbinfo::Number, ""},
    {"pages_used",       Ndbinfo::Number, ""},
    {"pages_total",      Ndbinfo::Number, ""},
  }};

DECLARE_NDBINFO_TABLE(LOGDESTINATION,5) =
{ "logdestination", 5, 0, "",
  {
    {"node_id",          Ndbinfo::Number, ""},
    {"type",             Ndbinfo::String, ""},
    {"params",           Ndbinfo::String, ""},
    {"current_size",     Ndbinfo::Number, ""},
    {"max_size",         Ndbinfo::Number, ""},
  }
};

DECLARE_NDBINFO_TABLE(BACKUP_RECORDS,11) =
{ "backup_records", 11, 0, "",
  {
    {"node_id",          Ndbinfo::Number, ""},
    {"backup_record",    Ndbinfo::Number, ""},
    {"backup_id",        Ndbinfo::Number, ""},
    {"master_ref",       Ndbinfo::Number, ""},
    {"client_ref",       Ndbinfo::Number, ""},
    {"state",            Ndbinfo::Number, ""},
    {"bytes",            Ndbinfo::Number, ""},
    {"records",          Ndbinfo::Number, ""},
    {"log_bytes",        Ndbinfo::Number, ""},
    {"log_records",      Ndbinfo::Number, ""},
    {"error_code",       Ndbinfo::Number, ""},
  }
};

DECLARE_NDBINFO_TABLE(BACKUP_PARAMETERS,14) =
{ "backup_parameters", 14, 0, "",
  {
    {"node_id",                  Ndbinfo::Number, ""},
    {"current_disk_write_speed", Ndbinfo::Number, ""},
    {"bytes_written_this_period",Ndbinfo::Number, ""},
    {"overflow_disk_write",      Ndbinfo::Number, ""},
    {"reset_delay_used",         Ndbinfo::Number, ""},
    {"reset_disk_speed_time",    Ndbinfo::Number, ""},
    {"backup_pool_size",         Ndbinfo::Number, ""},
    {"backup_file_pool_size",    Ndbinfo::Number, ""},
    {"table_pool_size",          Ndbinfo::Number, ""},
    {"trigger_pool_size",        Ndbinfo::Number, ""},
    {"fragment_pool_size",       Ndbinfo::Number, ""},
    {"page_pool_size",           Ndbinfo::Number, ""},
    {"compressed_backup",        Ndbinfo::Number, ""},
    {"compressed_lcp",           Ndbinfo::Number, ""},
  }
};

DECLARE_NDBINFO_TABLE(POOLS,6) =
{ "pools", 6, 0, "",
  {
    {"node_id",                  Ndbinfo::Number, ""},
    {"block_number",             Ndbinfo::Number, ""},
    {"block_instance",           Ndbinfo::Number, ""},
    {"pool_name",                Ndbinfo::String, ""},
    {"free",                     Ndbinfo::Number, ""},
    {"size",                     Ndbinfo::Number, ""},
  }
};

DECLARE_NDBINFO_TABLE(TEST,5) =
{ "test", 5, 0, "",
  {
    {"node_id",                 Ndbinfo::Number, ""},
    {"block_number",            Ndbinfo::Number, ""},
    {"block_instance",          Ndbinfo::Number, ""},
    {"counter",                 Ndbinfo::Number, ""},
    {"counter2",                Ndbinfo::Number64, ""},
  }
};

DECLARE_NDBINFO_TABLE(TRP_STATUS, 3) =
{ "trp_status", 3, 0, "",
  {
    {"node_id",                 Ndbinfo::Number, ""},
    {"remote_node_id",          Ndbinfo::Number, ""},
    {"status",                  Ndbinfo::Number, ""},
  }
};

DECLARE_NDBINFO_TABLE(LOG_SPACE, 7) =
{ "log_space", 7, 0, "",
  {
    {"log_id",   Ndbinfo::Number, ""},
    {"log_type", Ndbinfo::Number, "0 = REDO, 1 = DD-UNDO"},
    {"log_part", Ndbinfo::Number, ""},
    {"node_id",  Ndbinfo::Number, ""},
    {"total",    Ndbinfo::Number64, "total allocated, Mb"},
    {"used",     Ndbinfo::Number64, "currently in use, Mb"},
    {"used_hi",  Ndbinfo::Number64, "in use high water mark, Mb"},
  }
};


#define DBINFOTBL(x) Ndbinfo::x##_TABLEID, (Ndbinfo::Table*)&ndbinfo_##x

static
struct ndbinfo_table_list_entry {
  Ndbinfo::TableId id;
  Ndbinfo::Table * table;
} ndbinfo_tables[] = {
  // NOTE! the tables must be added to the list in the same order
  // as they are in "enum TableId"
  DBINFOTBL(TABLES),
  DBINFOTBL(COLUMNS),
  DBINFOTBL(MEMUSAGE),
  DBINFOTBL(LOGDESTINATION),
  DBINFOTBL(BACKUP_RECORDS),
  DBINFOTBL(BACKUP_PARAMETERS),
  DBINFOTBL(POOLS),
  DBINFOTBL(TEST),
  DBINFOTBL(TRP_STATUS),
  DBINFOTBL(LOG_SPACE)
};

static int no_ndbinfo_tables =
  sizeof(ndbinfo_tables) / sizeof(ndbinfo_tables[0]);


int Ndbinfo::getNumTables(){
  return no_ndbinfo_tables;
}

const Ndbinfo::Table& Ndbinfo::getTable(int i)
{
  assert(i >= 0 && i < number_ndbinfo_tables);
  ndbinfo_table_list_entry& entry = ndbinfo_tables[i];
  assert(entry.id == i);
  return *entry.table;
}

const Ndbinfo::Table& Ndbinfo::getTable(Uint32 i)
{
  return getTable((int)i);
}
