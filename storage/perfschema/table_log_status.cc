/* Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

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

/**
  @file storage/perfschema/table_log_status.cc
  Table log_status (implementation).
*/

#include "storage/perfschema/table_log_status.h"

#include "mysql/plugin.h"
#include "sql/current_thd.h"
#include "sql/debug_sync.h"
#include "sql/field.h"
#include "sql/log_resource.h"
#include "sql/plugin_table.h"
#include "sql/rpl_msr.h"  // channel_map
#include "storage/perfschema/pfs_instr.h"
#include "storage/perfschema/pfs_instr_class.h"
#include "storage/perfschema/table_helper.h"

THR_LOCK table_log_status::m_table_lock;

Plugin_table table_log_status::m_table_def(
    /* Schema name */
    "performance_schema",
    /* Name */
    "log_status",
    /* Definition */
    "  SERVER_UUID CHAR(36) collate utf8mb4_bin not null,\n"
    "  LOCAL JSON not null,\n"
    "  REPLICATION JSON not null,\n"
    "  STORAGE_ENGINES JSON not null\n",
    /* Options */
    " ENGINE=PERFORMANCE_SCHEMA",
    /* Tablespace */
    nullptr);

PFS_engine_table_share table_log_status::m_share = {
    &pfs_readonly_acl,
    table_log_status::create,
    NULL,                            /* write_row */
    NULL,                            /* delete_all_rows */
    table_log_status::get_row_count, /* records */
    sizeof(PFS_simple_index),        /* ref length */
    &m_table_lock,
    &m_table_def,
    true, /* perpetual */
    PFS_engine_table_proxy(),
    {0},
    false /* m_in_purgatory */
};

PFS_engine_table *table_log_status::create(PFS_engine_table_share *) {
  return new table_log_status();
}

table_log_status::table_log_status()
    : PFS_engine_table(&m_share, &m_pos), m_pos(0), m_next_pos(0) {}

table_log_status::~table_log_status() {}

void table_log_status::reset_position(void) {
  m_pos.m_index = 0;
  m_next_pos.m_index = 0;
}

ha_rows table_log_status::get_row_count() { return 1; }

int table_log_status::rnd_next(void) {
  int res = HA_ERR_END_OF_FILE;

  for (m_pos.set_at(&m_next_pos); m_pos.m_index < 1 && res != 0; m_pos.next()) {
    res = make_row();
    m_next_pos.set_after(&m_pos);
  }

  return res;
}

int table_log_status::rnd_pos(const void *pos MY_ATTRIBUTE((unused))) {
  int res = HA_ERR_RECORD_DELETED;

  set_position(pos);

  if (m_pos.m_index == 1) {
    res = make_row();
  }

  return res;
}

struct st_register_hton_arg {
  std::list<Log_resource *> *resources;
  Json_dom *json;
};

static bool iter_storage_engines_register(THD *, plugin_ref plugin, void *arg) {
  st_register_hton_arg *vargs = (st_register_hton_arg *)arg;
  handlerton *hton = plugin_data<handlerton *>(plugin);
  bool result = false;

  DBUG_ASSERT(plugin_state(plugin) == PLUGIN_IS_READY);

  /* The storage engine must implement all three functions to be supported */
  if (hton->lock_hton_log && hton->unlock_hton_log &&
      hton->collect_hton_log_info) {
    Log_resource *resource;
    resource = Log_resource_factory::get_wrapper(hton, vargs->json);
    if (!(result = !resource)) {
      vargs->resources->push_back(resource);
    }
  }
  return result;
}

int table_log_status::make_row() {
  DBUG_ENTER("table_log_status::make_row");
  THD *thd = current_thd;

  /* Report an error if THD has no BACKUP_ADMIN privilege */
  Security_context *sctx = thd->security_context();
  if (!sctx->has_global_grant(STRING_WITH_LEN("BACKUP_ADMIN")).first) {
    my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0), "BACKUP_ADMIN");
    DBUG_RETURN(HA_ERR_RECORD_DELETED);
  }

  /* Lock instance to collect log information */
  mysql_mutex_lock(&LOCK_collect_instance_log);
  bool error = false;

  Json_object json_local;             // LOCAL field
  Json_object json_replication;       // REPLICATION field
  Json_array json_replication_array;  // JSON array for REPLICATION field
  Json_object json_storage_engines;   // STORAGE_ENGINES field

  /* To block replication channels creation/removal */
  channel_map.wrlock();

  /* List of resources to be locked/collected/unlocked */
  std::list<Log_resource *> resources;
  std::list<Log_resource *>::iterator it;
  std::list<Log_resource *>::reverse_iterator rit;

  /*
    Add resources to lock/collect/unlock list.

    Each resource will to be added to the list will be wrapped with a new
    object that will be deleted once finishing this process.

    Note: the order the resources are added to the resource list is also the
    order they will lock theirs resources.
  */

  /*
    Add existing channels Master_info to the resources list, so that they can
    be blocked and their data collected in later steps.
  */
  for (uint mi_index = 0; mi_index < channel_map.get_max_channels();
       mi_index++) {
    Master_info *mi = channel_map.get_mi_at_pos(mi_index);
    if (Master_info::is_configured(mi))  // channel is configured
    {
      Log_resource *res;
      res = Log_resource_factory::get_wrapper(mi, &json_replication_array);
      if ((error = DBUG_EVALUATE_IF("log_status_oom_mi", 1, !res))) {
        char errfmt[] =
            "failed to allocate memory to collect "
            "information from replication channel '%s'";
        char errbuf[sizeof(errfmt) + CHANNEL_NAME_LENGTH];
        sprintf(errbuf, errfmt, mi->get_channel());
        my_error(ER_UNABLE_TO_COLLECT_LOG_STATUS, MYF(0), "REPLICATION",
                 errbuf);
        /* To please valgrind */
        DBUG_EXECUTE_IF("log_status_oom_mi", resources.push_back(res););
        goto end;
      }
      resources.push_back(res);
    }
  }

  /*
    Add binary log to the resources list, so that it can be blocked and its
    data collected in later steps.
  */
  {
    Log_resource *res;
    res = Log_resource_factory::get_wrapper(&mysql_bin_log, &json_local);
    if ((error = DBUG_EVALUATE_IF("log_status_oom_binlog", 1, !res))) {
      my_error(ER_UNABLE_TO_COLLECT_LOG_STATUS, MYF(0), "LOCAL",
               "failed to allocate memory to collect "
               "binary log information");
      /* To please valgrind */
      DBUG_EXECUTE_IF("log_status_oom_binlog", resources.push_back(res););
      goto end;
    }
    resources.push_back(res);
  }

  /*
    Add Gtid_state to the resources list, so that it can be blocked and its
    data (GTID_EXECUTED) collected in later steps.
  */
  {
    Log_resource *res;
    res = Log_resource_factory::get_wrapper(gtid_state, &json_local);
    if ((error = DBUG_EVALUATE_IF("log_status_oom_gtid", 1, !res))) {
      my_error(ER_UNABLE_TO_COLLECT_LOG_STATUS, MYF(0), "LOCAL",
               "failed to allocate memory to collect "
               "gtid_executed information");
      /* To please valgrind */
      DBUG_EXECUTE_IF("log_status_oom_gtid", resources.push_back(res););
      goto end;
    }
    resources.push_back(res);
  }

  /* To block storage engines logs, collect their logs information */
  /*
    Add storage engine's handlertons to the resources list, so that they can be
    blocked and their data collected in later steps.
  */
  {
    st_register_hton_arg args = {&resources, &json_storage_engines};
    error = plugin_foreach(thd, iter_storage_engines_register,
                           MYSQL_STORAGE_ENGINE_PLUGIN, &args);
    if (error || DBUG_EVALUATE_IF("log_status_oom_se", 1, 0)) {
      my_error(ER_UNABLE_TO_COLLECT_LOG_STATUS, MYF(0), "STORAGE_ENGINE",
               "failed to allocate memory to collect "
               "storage engines information");
      goto end;
    }
  }

  /* Lock all resources */
  for (it = resources.begin(); it != resources.end(); ++it) {
    (*it)->lock();
  }

  DBUG_SIGNAL_WAIT_FOR(thd, "pause_collecting_instance_logs_info",
                       "reached_collecting_instance_logs_info",
                       "continue_collecting_instance_logs_info");

  /* Collect all resources information (up to hitting some error) */
  for (it = resources.begin(); it != resources.end(); ++it)
    if ((error = DBUG_EVALUATE_IF("log_status_oom_collecting", 1,
                                  (*it)->collect_info()))) {
      my_error(ER_UNABLE_TO_COLLECT_LOG_STATUS, MYF(0),
               (*it)->get_json() == &json_storage_engines
                   ? "STORAGE_ENGINES"
                   : (*it)->get_json() == &json_local ? "LOCAL" : "REPLICATION",
               "failed to allocate memory to collect information");
      goto err_unlock;
    }

err_unlock:
  /* Unlock all resources */
  for (rit = resources.rbegin(); rit != resources.rend(); ++rit) {
    (*rit)->unlock();
  }

end:
  /* Delete all wrappers */
  while (!resources.empty()) {
    Log_resource *wrapper = resources.back();
    resources.pop_back();
    delete wrapper;
  }

  /* To allow replication channels creation/removal/admin */
  channel_map.unlock();

  /* Unlock instance after collecting log information */
  mysql_mutex_unlock(&LOCK_collect_instance_log);

  if (!error) {
    /* Populate m_row */
    if ((error = DBUG_EVALUATE_IF("log_status_oom_replication", 1,
                                  json_replication.add_clone(
                                      "channels", &json_replication_array)))) {
      my_error(ER_UNABLE_TO_COLLECT_LOG_STATUS, MYF(0), "REPLICATION",
               "failed to allocate memory to collect information");
    } else {
      memcpy(m_row.server_uuid, server_uuid, UUID_LENGTH);
      m_row.w_local = Json_wrapper(json_local.clone());
      m_row.w_replication = Json_wrapper(json_replication.clone());
      m_row.w_storage_engines = Json_wrapper(json_storage_engines.clone());
    }
  }

  DBUG_RETURN(error ? HA_ERR_RECORD_DELETED : 0);
}

int table_log_status::read_row_values(TABLE *table MY_ATTRIBUTE((unused)),
                                      unsigned char *buf MY_ATTRIBUTE((unused)),
                                      Field **fields MY_ATTRIBUTE((unused)),
                                      bool read_all MY_ATTRIBUTE((unused))) {
  Field *f;

  DBUG_ASSERT(table->s->null_bytes == 0);
  buf[0] = 0;

  for (; (f = *fields); fields++) {
    if (read_all || bitmap_is_set(table->read_set, f->field_index)) {
      switch (f->field_index) {
        case 0: /*server_uuid*/
          set_field_char_utf8(f, m_row.server_uuid, UUID_LENGTH);
          break;
        case 1: /*local*/
          set_field_json(f, &m_row.w_local);
          break;
        case 2: /*replication*/
          set_field_json(f, &m_row.w_replication);
          break;
        case 3: /*storage_engines*/
          set_field_json(f, &m_row.w_storage_engines);
          break;
        default:
          DBUG_ASSERT(false);
      }
    }
  }
  m_row.cleanup();

  return 0;
}
