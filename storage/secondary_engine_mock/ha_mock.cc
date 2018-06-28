/* Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "storage/secondary_engine_mock/ha_mock.h"

#include <map>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

#include "lex_string.h"
#include "my_alloc.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "mysql/plugin.h"
#include "mysqld_error.h"
#include "sql/handler.h"
#include "sql/sql_const.h"
#include "sql/table.h"
#include "thr_lock.h"

class THD;

namespace dd {
class Table;
}

namespace {

struct MockShare {
  THR_LOCK lock;
  MockShare() { thr_lock_init(&lock); }
  ~MockShare() { thr_lock_delete(&lock); }

  // Not copyable. The THR_LOCK object must stay where it is in memory
  // after it has been initialized.
  MockShare(const MockShare &) = delete;
  MockShare &operator=(const MockShare &) = delete;
};

// Map from (db_name, table_name) to the MockShare with table state.
using LoadedTables = std::map<std::pair<std::string, std::string>, MockShare>;
LoadedTables *loaded_tables{nullptr};

}  // namespace

namespace mock {

ha_mock::ha_mock(handlerton *hton, TABLE_SHARE *table_share)
    : handler(hton, table_share) {}

int ha_mock::open(const char *, int, unsigned int, const dd::Table *) {
  auto it = loaded_tables->find(
      std::make_pair(table_share->db.str, table_share->table_name.str));
  if (it == loaded_tables->end()) {
    // The table has not been loaded into the secondary storage engine yet.
    my_error(ER_NO_SUCH_TABLE, MYF(0), table_share->db.str,
             table_share->table_name.str);
    return HA_ERR_GENERIC;
  }
  thr_lock_data_init(&it->second.lock, &m_lock, nullptr);
  return 0;
}

THR_LOCK_DATA **ha_mock::store_lock(THD *, THR_LOCK_DATA **to,
                                    thr_lock_type lock_type) {
  if (lock_type != TL_IGNORE && m_lock.type == TL_UNLOCK)
    m_lock.type = lock_type;
  *to++ = &m_lock;
  return to;
}

int ha_mock::load_table(const TABLE &table) {
  DBUG_ASSERT(table.file != nullptr);
  loaded_tables->emplace(
      std::piecewise_construct,
      std::make_tuple(table.s->db.str, table.s->table_name.str),
      std::make_tuple());
  return 0;
}

int ha_mock::unload_table(const char *db_name, const char *table_name) {
  loaded_tables->erase(std::make_pair(db_name, table_name));
  return 0;
}

}  // namespace mock

static handler *Create(handlerton *hton, TABLE_SHARE *table_share, bool,
                       MEM_ROOT *mem_root) {
  return new (mem_root) mock::ha_mock(hton, table_share);
}

static int Init(MYSQL_PLUGIN p) {
  loaded_tables = new LoadedTables();

  handlerton *hton = static_cast<handlerton *>(p);
  hton->create = Create;
  hton->state = SHOW_OPTION_YES;
  hton->flags = HTON_SUPPORTS_SECONDARY;
  hton->db_type = DB_TYPE_UNKNOWN;
  return 0;
}

static int Deinit(MYSQL_PLUGIN) {
  delete loaded_tables;
  loaded_tables = nullptr;
  return 0;
}

static st_mysql_storage_engine mock_storage_engine{
    MYSQL_HANDLERTON_INTERFACE_VERSION};

mysql_declare_plugin(mock){
    MYSQL_STORAGE_ENGINE_PLUGIN,
    &mock_storage_engine,
    "MOCK",
    "MySQL",
    "Mock storage engine",
    PLUGIN_LICENSE_GPL,
    Init,
    nullptr,
    Deinit,
    0x0001,
    nullptr,
    nullptr,
    nullptr,
    0,
} mysql_declare_plugin_end;
