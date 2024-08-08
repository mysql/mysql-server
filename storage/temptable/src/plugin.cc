/* Copyright (c) 2016, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

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
#include <atomic>

#include "include/mysql/components/services/bits/psi_metric_bits.h"
#include "include/mysql/psi/mysql_metric.h"
#include "mysql/status_var.h"
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

namespace temptable {
/** Status variable that counts the memory limit breaches */
std::atomic_uint64_t count_hit_max_ram{};

/* Set the TempTable_count_hit_max_ram status var value */
static int show_count_hit_max_ram_var(THD *, SHOW_VAR *var, char *buff) {
  var->type = SHOW_LONGLONG;
  var->value = buff;
  var->scope = SHOW_SCOPE_GLOBAL;

  ulonglong *value = reinterpret_cast<ulonglong *>(buff);
  *value = count_hit_max_ram.load();

  return (0);
}

static void get_count_hit_max_ram(void * /* measurement_context */,
                                  measurement_delivery_callback_t delivery,
                                  void *delivery_context) {
  assert(delivery != nullptr);
  const auto measurement = static_cast<int64_t>(count_hit_max_ram.load());
  const auto value =
      std::clamp(measurement, std::numeric_limits<int64_t>::min(),
                 std::numeric_limits<int64_t>::max());
  delivery->value_int64(delivery_context, value);
}

/* Metrics of TempTable Storage engine */
static PSI_metric_info_v1 metrics[] = {
    {.m_metric = "count_hit_max_ram",
     .m_unit = "",
     .m_description = "The number of times internal temp tables exceeded the "
                      "memory limit of engine",
     .m_metric_type = MetricOTELType::ASYNC_COUNTER,
     .m_num_type = MetricNumType::METRIC_INTEGER,
     .m_flags = 0,
     .m_key = 0,
     .m_measurement_callback = get_count_hit_max_ram,
     .m_measurement_context = nullptr}};

/* Add the OTEL metrics for TempTable engine */
static PSI_meter_info_v1 meter[] = {
    {.m_meter = "mysql.TempTable",
     .m_description = "MySql TempTable metrics",
     .m_frequency = 10,
     .m_flags = 0,
     .m_key = 0,
     .m_metrics = metrics,
     .m_metrics_size = std::size(metrics)},
};

/* Structure for TempTable engine specific status variables */
static SHOW_VAR status_variables[] = {
    {.name = "TempTable_count_hit_max_ram",
     .value = (char *)&show_count_hit_max_ram_var,
     .type = SHOW_FUNC,
     .scope = SHOW_SCOPE_GLOBAL},
    {.name = NullS,
     .value = NullS,
     .type = SHOW_FUNC,
     .scope = SHOW_SCOPE_GLOBAL}};
}  // namespace temptable

/* Initialize the TempTable engine */
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
  mysql_meter_register(temptable::meter, std::size(temptable::meter));
  return 0;
}

/* De initialize the TempTable engine */
static int deinit(MYSQL_PLUGIN plugin_info [[maybe_unused]]) {
  temptable::count_hit_max_ram.store(0);
  mysql_meter_unregister(temptable::meter, std::size(temptable::meter));
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
  deinit,
  /* 1.0 */
  0x0100,
  /* status variables */
  temptable::status_variables,
  /* system variables */
  nullptr,
  /* config options */
  nullptr,
  /* flags */
  0,
}
mysql_declare_plugin_end;
// clang-format on
