/* Copyright (c) 2016, 2023, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/** @file storage/temptable/src/plugin.cc
Glue code for registering the TempTable plugin at MySQL. */

#include "mysql/plugin.h"

#include "sql/handler.h"
#include "sql/table.h"
#include "storage/temptable/include/temptable/allocator.h"
#include "storage/temptable/include/temptable/handler.h"

struct MEM_ROOT;

static handler *create_handler(handlerton *hton, TABLE_SHARE *table_share, bool,
                               MEM_ROOT *mem_root) {
  return new (mem_root) temptable::Handler(hton, table_share);
}

static int close_connection(handlerton *hton, THD *thd) {
  (void)hton;
  temptable::kv_store_shards_debug_dump();
  temptable::shared_block_pool_release(thd);
  return 0;
}

static st_mysql_storage_engine temptable_storage_engine = {
    MYSQL_HANDLERTON_INTERFACE_VERSION};

static int init(void *p) {
  handlerton *h = static_cast<handlerton *>(p);

  h->state = SHOW_OPTION_YES;
  h->db_type = DB_TYPE_TEMPTABLE;
  h->create = create_handler;
  h->flags = HTON_ALTER_NOT_SUPPORTED | HTON_CAN_RECREATE | HTON_HIDDEN |
             HTON_NOT_USER_SELECTABLE | HTON_NO_PARTITION |
             HTON_NO_BINLOG_ROW_OPT | HTON_SUPPORTS_EXTENDED_KEYS;
  h->close_connection = close_connection;

  temptable::Allocator<uint8_t>::init();

  return 0;
}

// clang-format off
mysql_declare_plugin(temptable) {
  MYSQL_STORAGE_ENGINE_PLUGIN,
  &temptable_storage_engine,
  "TempTable",
  PLUGIN_AUTHOR_ORACLE,
  "InnoDB temporary storage engine",
  PLUGIN_LICENSE_GPL,
  init,
  /* check uninstall */
  nullptr,
  /* destroy */
  nullptr,
  /* 1.0 */
  0x0100,
  /* status variables */
  nullptr,
  /* system variables */
  nullptr,
  /* config options */
  nullptr,
  /* flags */
  0,
}
mysql_declare_plugin_end;
// clang-format on
