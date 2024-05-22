/* Copyright (c) 2013, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql/rpl_group_replication.h"

#include <stdlib.h>
#include <sys/types.h>
#include <atomic>

#include <mysql/components/my_service.h>
#include <mysql/components/services/group_replication_status_service.h>
#include <mysql/components/services/mysql_system_variable.h>
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "my_systime.h"
#include "my_time.h"
#include "mysql/binlog/event/binlog_event.h"  // mysql::binlog::event::max_log_event_size
#include "mysql/components/services/log_builtins.h"
#include "mysql/components/services/log_shared.h"
#include "mysql/my_loglevel.h"
#include "mysql/plugin.h"
#include "mysql/plugin_group_replication.h"
#include "mysql/service_mysql_alloc.h"
#include "mysqld_error.h"       // ER_*
#include "sql/clone_handler.h"  // is_data_dropped
#include "sql/log.h"
#include "sql/mysqld.h"              // mysqld_port
#include "sql/mysqld_thd_manager.h"  // Global_THD_manager
#include "sql/replication.h"         // Trans_context_info
#include "sql/rpl_channel_credentials.h"
#include "sql/rpl_channel_service_interface.h"
#include "sql/rpl_gtid.h"     // Gtid_mode::lock
#include "sql/rpl_replica.h"  // report_host
#include "sql/sql_class.h"    // THD
#include "sql/sql_lex.h"
#include "sql/sql_plugin.h"  // plugin_unlock
#include "sql/sql_plugin_ref.h"
#include "sql/ssl_init_callback.h"
#include "sql/system_variables.h"  // System_variables
#include "sql/tztime.h"            // my_tz_UTC
#include "string_with_len.h"

class THD;

std::atomic_flag start_stop_executing = ATOMIC_FLAG_INIT;

/*
  Struct to share server ssl variables
*/
void st_server_ssl_variables::init() {
  ssl_ca = nullptr;
  ssl_capath = nullptr;
  tls_version = nullptr;
  tls_ciphersuites = nullptr;
  ssl_cert = nullptr;
  ssl_cipher = nullptr;
  ssl_key = nullptr;
  ssl_crl = nullptr;
  ssl_crlpath = nullptr;
  ssl_fips_mode = 0;
}

void st_server_ssl_variables::deinit() {
  my_free(ssl_ca);
  my_free(ssl_capath);
  my_free(tls_version);
  my_free(tls_ciphersuites);
  my_free(ssl_cert);
  my_free(ssl_cipher);
  my_free(ssl_key);
  my_free(ssl_crl);
  my_free(ssl_crlpath);
  init();
}

namespace {
/**
  Static name of Group Replication plugin.
*/
LEX_CSTRING group_replication_plugin_name_str = {
    STRING_WITH_LEN("group_replication")};
}  // namespace

/*
  Group Replication plugin handler function accessors.
*/
int group_replication_init() { return initialize_channel_service_interface(); }

bool is_group_replication_plugin_loaded() {
  bool result = false;

  plugin_ref plugin =
      my_plugin_lock_by_name(nullptr, group_replication_plugin_name_str,
                             MYSQL_GROUP_REPLICATION_PLUGIN);
  if (plugin != nullptr) {
    plugin_unlock(nullptr, plugin);
    result = true;
  }

  return result;
}

int group_replication_start(char **error_message, THD *thd) {
  LEX *lex = thd->lex;
  int result = 1;
  plugin_ref plugin = nullptr;

  if (start_stop_executing.test_and_set()) {
    std::string msg;
    msg.assign(
        "Another instance of START/STOP GROUP_REPLICATION command is "
        "executing.");
    *error_message =
        (char *)my_malloc(PSI_NOT_INSTRUMENTED, msg.size() + 1, MYF(0));
    strcpy(*error_message, msg.c_str());
    result = 8;
    return result;
  }
  plugin = my_plugin_lock_by_name(nullptr, group_replication_plugin_name_str,
                                  MYSQL_GROUP_REPLICATION_PLUGIN);
  if (plugin != nullptr) {
    /*
      We need to take Gtid_mode::lock because
      group_replication_handler->start function will (among other
      things) do the following:

       1. Call global_gtid_mode.get (the call stack is: this function
          calls plugin_handle->start, which is equal to
          plugin_group_replication_init (as declared at the end of
          plugin/group_replication/src/plugin.cc), which calls
          plugin_group_replication_start, which calls
          check_if_server_properly_configured, which calls
          get_server_startup_prerequirements, which calls
          global_gtid_mode.get).
       2. Set plugin-internal state that ensures that
          is_group_replication_running() returns true.

      In order to prevent a concurrent client from executing SET
      GTID_MODE=ON_PERMISSIVE between 1 and 2, we must hold
      Gtid_mode::lock.
    */
    Checkable_rwlock::Guard g(Gtid_mode::lock, Checkable_rwlock::READ_LOCK);
    st_mysql_group_replication *plugin_handle =
        (st_mysql_group_replication *)plugin_decl(plugin)->info;
    /*
      is_running check is required below before storing credentials.
      Check makes sure running instance of START GR is not impacted by
      temporary storage of credentials or if storing credential failed
      message is meaningful.
      e.g. of credential conflict blocked by below check
      1: START GROUP_REPLICATION;
         START GROUP_REPLICATION USER="abc" , PASSWORD="xyz";
      2: START GROUP_REPLICATION USER="abc" , PASSWORD="xyz";
         START GROUP_REPLICATION USER="user" , PASSWORD="pass";
     */
    if (plugin_handle->is_running()) {
      result = 2;
      goto err;
    }
    if (lex != nullptr &&
        (lex->replica_connection.user || lex->replica_connection.plugin_auth)) {
      if (Rpl_channel_credentials::get_instance().store_credentials(
              "group_replication_recovery", lex->replica_connection.user,
              lex->replica_connection.password,
              lex->replica_connection.plugin_auth)) {
        // Parallel START/STOP GR is blocked.
        // So by now START GR command should fail if running or stop should have
        // cleared credentials. Post UNINSTALL we should not reach here.
        /* purecov: begin inspected */
        assert(false);
        result = 2;
        goto err;
        /* purecov: end */
      }
    }

    result = plugin_handle->start(error_message);
    if (result)
      Rpl_channel_credentials::get_instance().delete_credentials(
          "group_replication_recovery");
  } else {
    LogErr(ERROR_LEVEL, ER_GROUP_REPLICATION_PLUGIN_NOT_INSTALLED);
  }
err:
  if (plugin != nullptr) plugin_unlock(nullptr, plugin);
  start_stop_executing.clear();
  return result;
}

int group_replication_stop(char **error_message) {
  int result = 1;
  plugin_ref plugin = nullptr;

  if (start_stop_executing.test_and_set()) {
    std::string msg;
    msg.assign(
        "Another instance of START/STOP GROUP_REPLICATION command is "
        "executing.");
    *error_message =
        (char *)my_malloc(PSI_NOT_INSTRUMENTED, msg.size() + 1, MYF(0));
    strcpy(*error_message, msg.c_str());
    result = 8;
    return result;
  }
  plugin = my_plugin_lock_by_name(nullptr, group_replication_plugin_name_str,
                                  MYSQL_GROUP_REPLICATION_PLUGIN);
  if (plugin != nullptr) {
    st_mysql_group_replication *plugin_handle =
        (st_mysql_group_replication *)plugin_decl(plugin)->info;
    result = plugin_handle->stop(error_message);
    plugin_unlock(nullptr, plugin);
  } else {
    LogErr(ERROR_LEVEL, ER_GROUP_REPLICATION_PLUGIN_NOT_INSTALLED);
  }
  start_stop_executing.clear();
  return result;
}

bool is_group_replication_running() {
  bool result = false;

  plugin_ref plugin =
      my_plugin_lock_by_name(nullptr, group_replication_plugin_name_str,
                             MYSQL_GROUP_REPLICATION_PLUGIN);
  if (plugin != nullptr) {
    st_mysql_group_replication *plugin_handle =
        (st_mysql_group_replication *)plugin_decl(plugin)->info;
    result = plugin_handle->is_running();
    plugin_unlock(nullptr, plugin);
  }

  return result;
}

bool is_group_replication_cloning() {
  bool result = false;

  plugin_ref plugin =
      my_plugin_lock_by_name(nullptr, group_replication_plugin_name_str,
                             MYSQL_GROUP_REPLICATION_PLUGIN);
  if (plugin != nullptr) {
    st_mysql_group_replication *plugin_handle =
        (st_mysql_group_replication *)plugin_decl(plugin)->info;
    result = plugin_handle->is_cloning();
    plugin_unlock(nullptr, plugin);
  }

  return result;
}

int set_group_replication_retrieved_certification_info(
    View_change_log_event *view_change_event) {
  int result = 1;

  plugin_ref plugin =
      my_plugin_lock_by_name(nullptr, group_replication_plugin_name_str,
                             MYSQL_GROUP_REPLICATION_PLUGIN);
  if (plugin != nullptr) {
    st_mysql_group_replication *plugin_handle =
        (st_mysql_group_replication *)plugin_decl(plugin)->info;
    result = plugin_handle->set_retrieved_certification_info(view_change_event);
    plugin_unlock(nullptr, plugin);
  }

  return result;
}

bool get_group_replication_connection_status_info(
    const GROUP_REPLICATION_CONNECTION_STATUS_CALLBACKS &callbacks) {
  bool result = true;

  plugin_ref plugin =
      my_plugin_lock_by_name(nullptr, group_replication_plugin_name_str,
                             MYSQL_GROUP_REPLICATION_PLUGIN);
  if (plugin != nullptr) {
    st_mysql_group_replication *plugin_handle =
        (st_mysql_group_replication *)plugin_decl(plugin)->info;
    result = plugin_handle->get_connection_status_info(callbacks);
    plugin_unlock(nullptr, plugin);
  }

  return result;
}

bool get_group_replication_group_members_info(
    unsigned int index,
    const GROUP_REPLICATION_GROUP_MEMBERS_CALLBACKS &callbacks) {
  bool result = true;

  plugin_ref plugin =
      my_plugin_lock_by_name(nullptr, group_replication_plugin_name_str,
                             MYSQL_GROUP_REPLICATION_PLUGIN);
  if (plugin != nullptr) {
    st_mysql_group_replication *plugin_handle =
        (st_mysql_group_replication *)plugin_decl(plugin)->info;
    result = plugin_handle->get_group_members_info(index, callbacks);
    plugin_unlock(nullptr, plugin);
  }

  return result;
}

bool get_group_replication_group_member_stats_info(
    unsigned int index,
    const GROUP_REPLICATION_GROUP_MEMBER_STATS_CALLBACKS &callbacks) {
  bool result = true;

  plugin_ref plugin =
      my_plugin_lock_by_name(nullptr, group_replication_plugin_name_str,
                             MYSQL_GROUP_REPLICATION_PLUGIN);
  if (plugin != nullptr) {
    st_mysql_group_replication *plugin_handle =
        (st_mysql_group_replication *)plugin_decl(plugin)->info;
    result = plugin_handle->get_group_member_stats_info(index, callbacks);
    plugin_unlock(nullptr, plugin);
  }

  return result;
}

unsigned int get_group_replication_members_number_info() {
  unsigned int result = 0;

  plugin_ref plugin =
      my_plugin_lock_by_name(nullptr, group_replication_plugin_name_str,
                             MYSQL_GROUP_REPLICATION_PLUGIN);
  if (plugin != nullptr) {
    st_mysql_group_replication *plugin_handle =
        (st_mysql_group_replication *)plugin_decl(plugin)->info;
    result = plugin_handle->get_members_number_info();
    plugin_unlock(nullptr, plugin);
  }

  return result;
}

/** helper function to @ref get_server_parameters */
inline char *my_strdup_nullable(OptionalString from) {
  return from.c_str() == nullptr
             ? nullptr
             : my_strdup(PSI_INSTRUMENT_ME, from.c_str(), MYF(0));
}

/*
  Server methods exported to plugin through
  include/mysql/group_replication_priv.h
*/
void get_server_parameters(char **hostname, uint *port, char **uuid,
                           unsigned int *out_server_version,
                           uint *out_admin_port) {
  /*
    use startup option report-host and report-port when provided,
    as value provided by glob_hostname, which used gethostname() function
    internally to determine hostname, will not always provide correct
    network interface, especially in case of multiple network interfaces.
  */
  if (report_host)
    *hostname = report_host;
  else
    *hostname = glob_hostname;

  if (report_port)
    *port = report_port;
  else
    *port = mysqld_port;

  *uuid = server_uuid;

  // Convert server version to hex

  ulong major = 0, minor = 0, patch = 0;
  char *pos = server_version, *end_pos;
  // extract each server decimal number, e.g., for 5.9.30 -> 5, 9 and 30
  major = strtoul(pos, &end_pos, 10);
  pos = end_pos + 1;
  minor = strtoul(pos, &end_pos, 10);
  pos = end_pos + 1;
  patch = strtoul(pos, &end_pos, 10);

  /*
    Convert to a equivalent hex representation.
    5.9.30 -> 0x050930
    version= 0 x 16^5 + 5 x 16^4 + 0 x 16^3 + 9 x 16^2 + 3 x 16^1 + 0 x 16^0
  */
  int v1 = patch / 10;
  int v0 = patch - v1 * 10;
  int v3 = minor / 10;
  int v2 = minor - v3 * 10;
  int v5 = major / 10;
  int v4 = major - v5 * 10;

  *out_server_version =
      v0 + v1 * 16 + v2 * 256 + v3 * 4096 + v4 * 65536 + v5 * 1048576;

  *out_admin_port = mysqld_admin_port;

  return;
}

void get_server_main_ssl_parameters(
    st_server_ssl_variables *server_ssl_variables) {
  OptionalString ca, capath, cert, cipher, ciphersuites, key, crl, crlpath,
      version;

  server_main_callback.read_parameters(&ca, &capath, &version, &cert, &cipher,
                                       &ciphersuites, &key, &crl, &crlpath,
                                       nullptr, nullptr);

  server_ssl_variables->ssl_ca = my_strdup_nullable(ca);
  server_ssl_variables->ssl_capath = my_strdup_nullable(capath);
  server_ssl_variables->tls_version = my_strdup_nullable(version);
  server_ssl_variables->tls_ciphersuites = my_strdup_nullable(ciphersuites);
  server_ssl_variables->ssl_cert = my_strdup_nullable(cert);
  server_ssl_variables->ssl_cipher = my_strdup_nullable(cipher);
  server_ssl_variables->ssl_key = my_strdup_nullable(key);
  server_ssl_variables->ssl_crl = my_strdup_nullable(crl);
  server_ssl_variables->ssl_crlpath = my_strdup_nullable(crlpath);
  server_ssl_variables->ssl_fips_mode = opt_ssl_fips_mode;

  return;
}

void get_server_admin_ssl_parameters(
    st_server_ssl_variables *server_ssl_variables) {
  OptionalString ca, capath, cert, cipher, ciphersuites, key, crl, crlpath,
      version;

  server_admin_callback.read_parameters(&ca, &capath, &version, &cert, &cipher,
                                        &ciphersuites, &key, &crl, &crlpath,
                                        nullptr, nullptr);

  server_ssl_variables->ssl_ca = my_strdup_nullable(ca);
  server_ssl_variables->ssl_capath = my_strdup_nullable(capath);
  server_ssl_variables->tls_version = my_strdup_nullable(version);
  server_ssl_variables->tls_ciphersuites = my_strdup_nullable(ciphersuites);
  server_ssl_variables->ssl_cert = my_strdup_nullable(cert);
  server_ssl_variables->ssl_cipher = my_strdup_nullable(cipher);
  server_ssl_variables->ssl_key = my_strdup_nullable(key);
  server_ssl_variables->ssl_crl = my_strdup_nullable(crl);
  server_ssl_variables->ssl_crlpath = my_strdup_nullable(crlpath);
  server_ssl_variables->ssl_fips_mode = opt_ssl_fips_mode;

  return;
}

ulong get_server_id() { return server_id; }

ulong get_auto_increment_increment() {
  return global_system_variables.auto_increment_increment;
}

ulong get_auto_increment_offset() {
  return global_system_variables.auto_increment_offset;
}

void set_auto_increment_increment(ulong auto_increment_increment) {
  global_system_variables.auto_increment_increment = auto_increment_increment;
}

void set_auto_increment_offset(ulong auto_increment_offset) {
  global_system_variables.auto_increment_offset = auto_increment_offset;
}

void get_server_startup_prerequirements(Trans_context_info &requirements) {
  requirements.binlog_enabled = opt_bin_log;
  requirements.binlog_format = global_system_variables.binlog_format;
  requirements.binlog_checksum_options = binlog_checksum_options;
  requirements.gtid_mode = global_gtid_mode.get();
  requirements.log_replica_updates = opt_log_replica_updates;
  requirements.parallel_applier_type = mts_parallel_option;
  requirements.parallel_applier_workers = opt_mts_replica_parallel_workers;
  requirements.parallel_applier_preserve_commit_order =
      opt_replica_preserve_commit_order;
  requirements.lower_case_table_names = lower_case_table_names;
  requirements.default_table_encryption =
      global_system_variables.default_table_encryption;
}

bool get_server_encoded_gtid_executed(uchar **encoded_gtid_executed,
                                      size_t *length) {
  Checkable_rwlock::Guard g(*global_tsid_lock, Checkable_rwlock::WRITE_LOCK);

  assert(global_gtid_mode.get() != Gtid_mode::OFF);

  const Gtid_set *executed_gtids = gtid_state->get_executed_gtids();
  *length = executed_gtids->get_encoded_length();
  *encoded_gtid_executed =
      (uchar *)my_malloc(key_memory_Gtid_set_to_string, *length, MYF(MY_WME));
  if (*encoded_gtid_executed == nullptr) return true;

  executed_gtids->encode(*encoded_gtid_executed);
  return false;
}

#if !defined(NDEBUG)
char *encoded_gtid_set_to_string(uchar *encoded_gtid_set, size_t length) {
  /* No tsid_lock because this is a completely local object. */
  Tsid_map tsid_map(nullptr);
  Gtid_set set(&tsid_map);

  if (set.add_gtid_encoding(encoded_gtid_set, length) != RETURN_STATUS_OK)
    return nullptr;

  char *buf;
  set.to_string(&buf);
  return buf;
}
#endif

void global_thd_manager_add_thd(THD *thd) {
  Global_THD_manager::get_instance()->add_thd(thd);
}

void global_thd_manager_remove_thd(THD *thd) {
  Global_THD_manager::get_instance()->remove_thd(thd);
}

bool is_gtid_committed(const Gtid &gtid) {
  Checkable_rwlock::Guard g(*global_tsid_lock, Checkable_rwlock::READ_LOCK);

  assert(global_gtid_mode.get() != Gtid_mode::OFF);

  gtid_state->lock_sidno(gtid.sidno);
  bool result = gtid_state->is_executed(gtid);
  gtid_state->unlock_sidno(gtid.sidno);

  return result;
}

bool wait_for_gtid_set_committed(const char *gtid_set_text, double timeout,
                                 bool update_thd_status) {
  THD *thd = current_thd;
  Gtid_set wait_for_gtid_set(global_tsid_map, nullptr);

  global_tsid_lock->rdlock();

  if (wait_for_gtid_set.add_gtid_text(gtid_set_text) != RETURN_STATUS_OK) {
    global_tsid_lock->unlock();
    return true;
  }

  /*
    If the current session owns a GTID that is part of the waiting
    set then that GTID will not reach GTID_EXECUTED while the session
    is waiting.
  */
  if (thd->owned_gtid.sidno > 0 &&
      wait_for_gtid_set.contains_gtid(thd->owned_gtid)) {
    global_tsid_lock->unlock();
    return true;
  }

  gtid_state->begin_gtid_wait();
  bool result = gtid_state->wait_for_gtid_set(thd, &wait_for_gtid_set, timeout,
                                              update_thd_status);
  gtid_state->end_gtid_wait();

  global_tsid_lock->unlock();

  return result;
}

unsigned long get_replica_max_allowed_packet() {
  return replica_max_allowed_packet;
}

unsigned long get_max_replica_max_allowed_packet() {
  return mysql::binlog::event::max_log_event_size;
}

bool is_server_restarting_after_clone() { return clone_startup; }

bool is_server_data_dropped() { return Clone_handler::is_data_dropped(); }

std::string get_group_replication_group_name() {
  std::string group_name{""};
  auto set_channel_name_lambda = [](void *const, const char &, size_t) {};
  auto set_source_uuid_lambda = [](void *const, const char &, size_t) {};
  auto set_service_state_lambda = [](void *const, bool) {};
  auto set_group_name_lambda = [](void *const context, const char &value,
                                  size_t length) {
    std::string *group_name_ptr = static_cast<std::string *>(context);
    const size_t max = UUID_LENGTH;
    length = std::min(length, max);
    group_name_ptr->assign(&value, length);
  };

  const GROUP_REPLICATION_CONNECTION_STATUS_CALLBACKS callbacks = {
      &group_name,
      set_channel_name_lambda,
      set_group_name_lambda,
      set_source_uuid_lambda,
      set_service_state_lambda,
  };

  // Query plugin and let callbacks do their job.
  if (get_group_replication_connection_status_info(callbacks)) {
    DBUG_PRINT("info", ("Group Replication stats not available!"));
  }
  return group_name;
}

bool get_group_replication_view_change_uuid(std::string &uuid) {
  my_h_service component_sys_variable_reader_service_handler = nullptr;
  SERVICE_TYPE(mysql_system_variable_reader)
  *component_sys_variable_reader_service = nullptr;
  srv_registry->acquire("mysql_system_variable_reader",
                        &component_sys_variable_reader_service_handler);

  char *var_value = nullptr;
  // uuid length + sizeof('\0')
  constexpr size_t var_buffer_capacity = UUID_LENGTH + 1;
  size_t var_len = var_buffer_capacity;

  bool error = false;

  if (nullptr == component_sys_variable_reader_service_handler) {
    error = true; /* purecov: inspected */
    goto end;     /* purecov: inspected */
  }

  component_sys_variable_reader_service =
      reinterpret_cast<SERVICE_TYPE(mysql_system_variable_reader) *>(
          component_sys_variable_reader_service_handler);

  if ((var_value = new char[var_len]) == nullptr) {
    error = true; /* purecov: inspected */
    goto end;     /* purecov: inspected */
  }

  if (!component_sys_variable_reader_service->get(
          nullptr, "GLOBAL", "mysql_server",
          "group_replication_view_change_uuid",
          reinterpret_cast<void **>(&var_value), &var_len)) {
    uuid.assign(var_value, var_len);
  } else if (var_len != var_buffer_capacity) {
    // Should never happen: no enough space for UUID in the buffer
    assert(false);
    error = true;
    goto end;
  } else {
    // The variable does not exist, thence we use its default value.
    uuid.assign("AUTOMATIC");
  }

end:
  srv_registry->release(component_sys_variable_reader_service_handler);
  delete[] var_value;
  return error;
}

bool is_group_replication_member_secondary() {
  bool is_a_secondary = false;

  my_h_service gr_status_service_handler = nullptr;
  SERVICE_TYPE(group_replication_status_service_v1) *gr_status_service =
      nullptr;
  srv_registry->acquire("group_replication_status_service_v1",
                        &gr_status_service_handler);
  if (nullptr != gr_status_service_handler) {
    gr_status_service =
        reinterpret_cast<SERVICE_TYPE(group_replication_status_service_v1) *>(
            gr_status_service_handler);
    if (gr_status_service
            ->is_group_in_single_primary_mode_and_im_a_secondary()) {
      is_a_secondary = true;
    }
  }

  srv_registry->release(gr_status_service_handler);
  return is_a_secondary;
}

void microseconds_to_datetime_str(uint64_t microseconds_since_epoch,
                                  char *datetime_str, uint decimal_precision) {
  my_timeval time_value;
  my_micro_time_to_timeval(microseconds_since_epoch, &time_value);

  MYSQL_TIME mysql_time;
  my_tz_UTC->gmt_sec_to_TIME(&mysql_time, time_value);
  my_datetime_to_str(mysql_time, datetime_str, decimal_precision);
}
