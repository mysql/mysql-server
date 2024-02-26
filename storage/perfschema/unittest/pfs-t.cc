/* Copyright (c) 2008, 2023, Oracle and/or its affiliates.

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

#include "storage/perfschema/unittest/pfs_unit_test_conf.h"

#include <memory.h>
#include <mysql/psi/psi_file.h>
#include <string.h>

#include "m_ctype.h"
#include "my_io.h"
#include "my_thread.h"
#include "storage/perfschema/pfs_buffer_container.h"
#include "storage/perfschema/pfs_global.h"
#include "storage/perfschema/pfs_instr.h"
#include "storage/perfschema/pfs_instr_class.h"
#include "storage/perfschema/pfs_server.h"
#include "storage/perfschema/terminology_use_previous.cc"
#include "storage/perfschema/unittest/stub_digest.h"
#include "storage/perfschema/unittest/stub_pfs_defaults.h"
#include "storage/perfschema/unittest/stub_pfs_plugin_table.h"
#include "storage/perfschema/unittest/stub_pfs_tls_channel.h"
#include "storage/perfschema/unittest/stub_print_error.h"
#include "storage/perfschema/unittest/stub_server_telemetry.h"
#include "unittest/mytap/tap.h"

/* test helpers, to simulate the setup */

static void setup_thread(PSI_thread *t, bool enabled) {
  PFS_thread *t2 = (PFS_thread *)t;
  t2->m_enabled = enabled;
}

/* test helpers, to inspect data */

static PFS_file *lookup_file_by_name(const char *name) {
  PFS_file *pfs;
  size_t len = strlen(name);
  size_t dirlen;
  const char *filename;
  size_t filename_length;

  PFS_file_iterator it = global_file_container.iterate();
  pfs = it.scan_next();

  while (pfs != nullptr) {
    /*
      When a file "foo" is instrumented, the name is normalized
      to "/path/to/current/directory/foo", so we remove the
      directory name here to find it back.
    */
    dirlen = dirname_length(pfs->m_file_name.ptr());
    filename = pfs->m_file_name.ptr() + dirlen;
    filename_length = pfs->m_file_name.length() - dirlen;
    if ((len == filename_length) &&
        (strncmp(name, filename, filename_length) == 0))
      return pfs;

    pfs = it.scan_next();
  }

  return nullptr;
}

/* tests */

static void test_bootstrap() {
  void *psi;
  void *psi_2;
  PSI_thread_bootstrap *thread_boot;
  PSI_mutex_bootstrap *mutex_boot;
  PSI_rwlock_bootstrap *rwlock_boot;
  PSI_cond_bootstrap *cond_boot;
  PSI_file_bootstrap *file_boot;
  PSI_socket_bootstrap *socket_boot;
  PSI_table_bootstrap *table_boot;
  PSI_mdl_bootstrap *mdl_boot;
  PSI_idle_bootstrap *idle_boot;
  PSI_stage_bootstrap *stage_boot;
  PSI_statement_bootstrap *statement_boot;
  PSI_transaction_bootstrap *transaction_boot;
  PSI_memory_bootstrap *memory_boot;
  PSI_error_bootstrap *error_boot;
  PSI_data_lock_bootstrap *data_lock_boot;
  PSI_system_bootstrap *system_boot;
  PSI_tls_channel_bootstrap *tls_channel_boot;
  PFS_global_param param;

  diag("test_bootstrap");

  memset(&param, 0xFF, sizeof(param));
  param.m_enabled = true;
  param.m_mutex_class_sizing = 0;
  param.m_rwlock_class_sizing = 0;
  param.m_cond_class_sizing = 0;
  param.m_thread_class_sizing = 0;
  param.m_table_share_sizing = 0;
  param.m_file_class_sizing = 0;
  param.m_socket_class_sizing = 0;
  param.m_mutex_sizing = 0;
  param.m_rwlock_sizing = 0;
  param.m_cond_sizing = 0;
  param.m_thread_sizing = 0;
  param.m_table_sizing = 0;
  param.m_file_sizing = 0;
  param.m_file_handle_sizing = 0;
  param.m_socket_sizing = 0;
  param.m_events_waits_history_sizing = 0;
  param.m_events_waits_history_long_sizing = 0;
  param.m_setup_actor_sizing = 0;
  param.m_setup_object_sizing = 0;
  param.m_user_sizing = 0;
  param.m_account_sizing = 0;
  param.m_host_sizing = 0;
  param.m_stage_class_sizing = 0;
  param.m_events_stages_history_sizing = 0;
  param.m_events_stages_history_long_sizing = 0;
  param.m_statement_class_sizing = 0;
  param.m_events_statements_history_sizing = 0;
  param.m_events_statements_history_long_sizing = 0;
  param.m_events_transactions_history_sizing = 0;
  param.m_events_transactions_history_long_sizing = 0;
  param.m_digest_sizing = 0;
  param.m_session_connect_attrs_sizing = 0;
  param.m_program_sizing = 0;
  param.m_statement_stack_sizing = 0;
  param.m_memory_class_sizing = 0;
  param.m_metadata_lock_sizing = 0;
  param.m_max_digest_length = 0;
  param.m_max_sql_text_length = 0;
  param.m_error_sizing = 0;
  param.m_consumer_events_stages_current_enabled = false;
  param.m_consumer_events_stages_history_enabled = false;
  param.m_consumer_events_stages_history_long_enabled = false;
  param.m_consumer_events_statements_cpu_enabled = false;
  param.m_consumer_events_statements_current_enabled = false;
  param.m_consumer_events_statements_history_enabled = false;
  param.m_consumer_events_statements_history_long_enabled = false;
  param.m_consumer_events_transactions_current_enabled = false;
  param.m_consumer_events_transactions_history_enabled = false;
  param.m_consumer_events_transactions_history_long_enabled = false;
  param.m_consumer_events_waits_current_enabled = false;
  param.m_consumer_events_waits_history_enabled = false;
  param.m_consumer_events_waits_history_long_enabled = false;
  param.m_consumer_global_instrumentation_enabled = false;
  param.m_consumer_thread_instrumentation_enabled = false;
  param.m_consumer_statement_digest_enabled = false;

  param.m_hints.m_table_definition_cache = 100;
  param.m_hints.m_table_open_cache = 100;
  param.m_hints.m_max_connections = 100;
  param.m_hints.m_open_files_limit = 100;
  param.m_hints.m_max_prepared_stmt_count = 100;

  pre_initialize_performance_schema();
  initialize_performance_schema(
      &param, &thread_boot, &mutex_boot, &rwlock_boot, &cond_boot, &file_boot,
      &socket_boot, &table_boot, &mdl_boot, &idle_boot, &stage_boot,
      &statement_boot, &transaction_boot, &memory_boot, &error_boot,
      &data_lock_boot, &system_boot, &tls_channel_boot);
  ok(thread_boot != nullptr, "thread_boot");
  ok(mutex_boot != nullptr, "mutex_boot");
  ok(rwlock_boot != nullptr, "rwlock_boot");
  ok(cond_boot != nullptr, "cond_boot");
  ok(file_boot != nullptr, "file_boot");
  ok(socket_boot != nullptr, "socket_boot");
  ok(table_boot != nullptr, "table_boot");
  ok(mdl_boot != nullptr, "mdl_boot");
  ok(idle_boot != nullptr, "idle_boot");
  ok(stage_boot != nullptr, "stage_boot");
  ok(statement_boot != nullptr, "statement_boot");
  ok(transaction_boot != nullptr, "transaction_boot");
  ok(memory_boot != nullptr, "memory_boot");
  ok(error_boot != nullptr, "error_boot");
  ok(data_lock_boot != nullptr, "data_lock_boot");
  ok(tls_channel_boot != nullptr, "tls_channel_boot");

  ok(thread_boot->get_interface != nullptr, "thread_boot->get_interface");
  ok(mutex_boot->get_interface != nullptr, "mutex_boot->get_interface");
  ok(rwlock_boot->get_interface != nullptr, "rwlock_boot->get_interface");
  ok(cond_boot->get_interface != nullptr, "cond_boot->get_interface");
  ok(file_boot->get_interface != nullptr, "file_boot->get_interface");
  ok(socket_boot->get_interface != nullptr, "socket_boot->get_interface");
  ok(table_boot->get_interface != nullptr, "table_boot->get_interface");
  ok(mdl_boot->get_interface != nullptr, "mdl_boot->get_interface");
  ok(idle_boot->get_interface != nullptr, "idle_boot->get_interface");
  ok(stage_boot->get_interface != nullptr, "stage_boot->get_interface");
  ok(statement_boot->get_interface != nullptr, "statement_boot->get_interface");
  ok(transaction_boot->get_interface != nullptr,
     "transaction_boot->get_interface");
  ok(memory_boot->get_interface != nullptr, "memory_boot->get_interface");
  ok(error_boot->get_interface != nullptr, "error_boot->get_interface");
  ok(data_lock_boot->get_interface != nullptr, "data_lock_boot->get_interface");
  ok(tls_channel_boot->get_interface != nullptr,
     "tls_channel_boot->get_interface");

  psi = thread_boot->get_interface(0);
  ok(psi == nullptr, "no thread version 0");
  psi = thread_boot->get_interface(PSI_THREAD_VERSION_1);
  ok(psi == nullptr, "no thread version 1");
  psi = thread_boot->get_interface(PSI_THREAD_VERSION_2);
  ok(psi == nullptr, "no thread version 2");

  psi = mutex_boot->get_interface(0);
  ok(psi == nullptr, "no mutex version 0");
  psi = mutex_boot->get_interface(PSI_MUTEX_VERSION_1);
  ok(psi != nullptr, "mutex version 1");

  psi = rwlock_boot->get_interface(0);
  ok(psi == nullptr, "no rwlock version 0");
  psi = rwlock_boot->get_interface(PSI_RWLOCK_VERSION_1);
  ok(psi == nullptr, "no rwlock version 1");
  psi = rwlock_boot->get_interface(PSI_RWLOCK_VERSION_2);
  ok(psi != nullptr, "rwlock version 2");

  psi = cond_boot->get_interface(0);
  ok(psi == nullptr, "no cond version 0");
  psi = cond_boot->get_interface(PSI_COND_VERSION_1);
  ok(psi != nullptr, "cond version 1");

  psi = file_boot->get_interface(0);
  ok(psi == nullptr, "no file version 0");
  psi = file_boot->get_interface(PSI_FILE_VERSION_2);
  ok(psi != nullptr, "file version 2");

  psi = socket_boot->get_interface(0);
  ok(psi == nullptr, "no socket version 0");
  psi = socket_boot->get_interface(PSI_SOCKET_VERSION_1);
  ok(psi != nullptr, "socket version 1");

  psi = table_boot->get_interface(0);
  ok(psi == nullptr, "no table version 0");
  psi = table_boot->get_interface(PSI_TABLE_VERSION_1);
  ok(psi != nullptr, "table version 1");

  psi = mdl_boot->get_interface(0);
  ok(psi == nullptr, "no mdl version 0");
  psi = mdl_boot->get_interface(PSI_MDL_VERSION_1);
  ok(psi != nullptr, "mdl version 1");
  psi = mdl_boot->get_interface(PSI_MDL_VERSION_2);
  ok(psi != nullptr, "mdl version 2");

  psi = idle_boot->get_interface(0);
  ok(psi == nullptr, "no idle version 0");
  psi = idle_boot->get_interface(PSI_IDLE_VERSION_1);
  ok(psi != nullptr, "idle version 1");

  psi = stage_boot->get_interface(0);
  ok(psi == nullptr, "no stage version 0");
  psi = stage_boot->get_interface(PSI_STAGE_VERSION_1);
  ok(psi != nullptr, "stage version 1");

  psi = statement_boot->get_interface(0);
  ok(psi == nullptr, "no statement version 0");
  psi = statement_boot->get_interface(PSI_STATEMENT_VERSION_1);
  ok(psi == nullptr, "no statement version 1");
  psi = statement_boot->get_interface(PSI_STATEMENT_VERSION_2);
  ok(psi == nullptr, "no statement version 2");
  psi = statement_boot->get_interface(PSI_STATEMENT_VERSION_3);
  ok(psi == nullptr, "no statement version 3");
  psi = statement_boot->get_interface(PSI_STATEMENT_VERSION_4);
  ok(psi == nullptr, "no statement version 4");
  psi = statement_boot->get_interface(PSI_STATEMENT_VERSION_5);
  ok(psi != nullptr, "statement version 5");

  psi = transaction_boot->get_interface(0);
  ok(psi == nullptr, "no transaction version 0");
  psi = transaction_boot->get_interface(PSI_TRANSACTION_VERSION_1);
  ok(psi != nullptr, "transaction version 1");

  psi = memory_boot->get_interface(0);
  ok(psi == nullptr, "no memory version 0");
  psi = memory_boot->get_interface(PSI_MEMORY_VERSION_1);
  ok(psi == nullptr, "memory version 1");
  psi = memory_boot->get_interface(PSI_MEMORY_VERSION_2);
  ok(psi != nullptr, "memory version 2");

  psi = error_boot->get_interface(0);
  ok(psi == nullptr, "no error version 0");
  psi = error_boot->get_interface(PSI_ERROR_VERSION_1);
  ok(psi != nullptr, "error version 1");

  psi = data_lock_boot->get_interface(0);
  ok(psi == nullptr, "no data_lock version 0");
  psi = data_lock_boot->get_interface(PSI_DATA_LOCK_VERSION_1);
  ok(psi != nullptr, "data_lock version 1");
  psi_2 = data_lock_boot->get_interface(PSI_DATA_LOCK_VERSION_2);
  ok(psi_2 == nullptr, "data_lock version 2");

  psi = tls_channel_boot->get_interface(0);
  ok(psi == nullptr, "no tls channel version 0");
  psi = tls_channel_boot->get_interface(PSI_TLS_CHANNEL_VERSION_1);
  ok(psi != nullptr, "tls channel version 1");

  shutdown_performance_schema();
}

/*
  Not a test, helper for testing pfs.cc
*/
static void load_perfschema(
    PSI_thread_service_t **thread_service, PSI_mutex_service_t **mutex_service,
    PSI_rwlock_service_t **rwlock_service, PSI_cond_service_t **cond_service,
    PSI_file_service_t **file_service, PSI_socket_service_t **socket_service,
    PSI_table_service_t **table_service, PSI_mdl_service_t **mdl_service,
    PSI_idle_service_t **idle_service, PSI_stage_service_t **stage_service,
    PSI_statement_service_t **statement_service,
    PSI_transaction_service_t **transaction_service,
    PSI_memory_service_t **memory_service, PSI_error_service_t **error_service,
    PSI_data_lock_service_t **data_lock_service,
    PSI_system_service_t **system_service,
    PSI_tls_channel_service_t **tls_channel_service) {
  PSI_thread_bootstrap *thread_boot;
  PSI_mutex_bootstrap *mutex_boot;
  PSI_rwlock_bootstrap *rwlock_boot;
  PSI_cond_bootstrap *cond_boot;
  PSI_file_bootstrap *file_boot;
  PSI_socket_bootstrap *socket_boot;
  PSI_table_bootstrap *table_boot;
  PSI_mdl_bootstrap *mdl_boot;
  PSI_idle_bootstrap *idle_boot;
  PSI_stage_bootstrap *stage_boot;
  PSI_statement_bootstrap *statement_boot;
  PSI_transaction_bootstrap *transaction_boot;
  PSI_memory_bootstrap *memory_boot;
  PSI_error_bootstrap *error_boot;
  PSI_data_lock_bootstrap *data_lock_boot;
  PSI_system_bootstrap *system_boot;
  PSI_tls_channel_bootstrap *tls_channel_boot;
  PFS_global_param param;

  memset(&param, 0xFF, sizeof(param));
  param.m_enabled = true;
  param.m_mutex_class_sizing = 10;
  param.m_rwlock_class_sizing = 10;
  param.m_cond_class_sizing = 10;
  param.m_thread_class_sizing = 10;
  param.m_table_share_sizing = 10;
  param.m_file_class_sizing = 10;
  param.m_socket_class_sizing = 10;
  param.m_mutex_sizing = 10;
  param.m_rwlock_sizing = 10;
  param.m_cond_sizing = 10;
  param.m_thread_sizing = 10;
  param.m_table_sizing = 10;
  param.m_file_sizing = 10;
  param.m_file_handle_sizing = 50;
  param.m_socket_sizing = 10;
  param.m_events_waits_history_sizing = 10;
  param.m_events_waits_history_long_sizing = 10;
  param.m_setup_actor_sizing = 0;
  param.m_setup_object_sizing = 0;
  param.m_user_sizing = 0;
  param.m_account_sizing = 0;
  param.m_host_sizing = 0;
  param.m_stage_class_sizing = 0;
  param.m_events_stages_history_sizing = 0;
  param.m_events_stages_history_long_sizing = 0;
  param.m_statement_class_sizing = 0;
  param.m_events_statements_history_sizing = 0;
  param.m_events_statements_history_long_sizing = 0;
  param.m_events_transactions_history_sizing = 0;
  param.m_events_transactions_history_long_sizing = 0;
  param.m_digest_sizing = 0;
  param.m_session_connect_attrs_sizing = 0;
  param.m_program_sizing = 0;
  param.m_statement_stack_sizing = 10;
  param.m_memory_class_sizing = 10;
  param.m_metadata_lock_sizing = 10;
  param.m_max_digest_length = 0;
  param.m_max_sql_text_length = 1000;
  param.m_error_sizing = 0;
  param.m_consumer_events_stages_current_enabled = false;
  param.m_consumer_events_stages_history_enabled = false;
  param.m_consumer_events_stages_history_long_enabled = false;
  param.m_consumer_events_statements_cpu_enabled = false;
  param.m_consumer_events_statements_current_enabled = false;
  param.m_consumer_events_statements_history_enabled = false;
  param.m_consumer_events_statements_history_long_enabled = false;
  param.m_consumer_events_transactions_current_enabled = false;
  param.m_consumer_events_transactions_history_enabled = false;
  param.m_consumer_events_transactions_history_long_enabled = false;
  param.m_consumer_events_waits_current_enabled = false;
  param.m_consumer_events_waits_history_enabled = false;
  param.m_consumer_events_waits_history_long_enabled = false;
  param.m_consumer_global_instrumentation_enabled = false;
  param.m_consumer_thread_instrumentation_enabled = false;
  param.m_consumer_statement_digest_enabled = false;

  param.m_hints.m_table_definition_cache = 100;
  param.m_hints.m_table_open_cache = 100;
  param.m_hints.m_max_connections = 100;
  param.m_hints.m_open_files_limit = 100;
  param.m_hints.m_max_prepared_stmt_count = 100;

  pre_initialize_performance_schema();
  /* test_bootstrap() covered this, assuming it just works */
  initialize_performance_schema(
      &param, &thread_boot, &mutex_boot, &rwlock_boot, &cond_boot, &file_boot,
      &socket_boot, &table_boot, &mdl_boot, &idle_boot, &stage_boot,
      &statement_boot, &transaction_boot, &memory_boot, &error_boot,
      &data_lock_boot, &system_boot, &tls_channel_boot);
  *thread_service = (PSI_thread_service_t *)thread_boot->get_interface(
      PSI_CURRENT_THREAD_VERSION);
  *mutex_service =
      (PSI_mutex_service_t *)mutex_boot->get_interface(PSI_MUTEX_VERSION_1);
  *rwlock_service =
      (PSI_rwlock_service_t *)rwlock_boot->get_interface(PSI_RWLOCK_VERSION_2);
  *cond_service =
      (PSI_cond_service_t *)cond_boot->get_interface(PSI_COND_VERSION_1);
  *file_service =
      (PSI_file_service_t *)file_boot->get_interface(PSI_FILE_VERSION_2);
  *socket_service =
      (PSI_socket_service_t *)socket_boot->get_interface(PSI_SOCKET_VERSION_1);
  *table_service =
      (PSI_table_service_t *)table_boot->get_interface(PSI_TABLE_VERSION_1);
  *mdl_service =
      (PSI_mdl_service_t *)mdl_boot->get_interface(PSI_CURRENT_MDL_VERSION);
  *idle_service =
      (PSI_idle_service_t *)idle_boot->get_interface(PSI_IDLE_VERSION_1);
  *stage_service =
      (PSI_stage_service_t *)stage_boot->get_interface(PSI_SOCKET_VERSION_1);
  *statement_service = (PSI_statement_service_t *)statement_boot->get_interface(
      PSI_CURRENT_STATEMENT_VERSION);
  *system_service =
      (PSI_system_service_t *)system_boot->get_interface(PSI_SYSTEM_VERSION_1);
  *transaction_service =
      (PSI_transaction_service_t *)transaction_boot->get_interface(
          PSI_TRANSACTION_VERSION_1);
  *memory_service =
      (PSI_memory_service_t *)memory_boot->get_interface(PSI_MEMORY_VERSION_2);
  *error_service =
      (PSI_error_service_t *)error_boot->get_interface(PSI_ERROR_VERSION_1);
  *data_lock_service = (PSI_data_lock_service_t *)data_lock_boot->get_interface(
      PSI_DATA_LOCK_VERSION_1);
  *tls_channel_service =
      (PSI_tls_channel_service_t *)tls_channel_boot->get_interface(
          PSI_TLS_CHANNEL_VERSION_1);

  /* Reset every consumer to a known state */
  flag_global_instrumentation = true;
  flag_thread_instrumentation = true;
}

static void test_bad_registration() {
  PSI_thread_service_t *thread_service;
  PSI_mutex_service_t *mutex_service;
  PSI_rwlock_service_t *rwlock_service;
  PSI_cond_service_t *cond_service;
  PSI_file_service_t *file_service;
  PSI_socket_service_t *socket_service;
  PSI_table_service_t *table_service;
  PSI_mdl_service_t *mdl_service;
  PSI_idle_service_t *idle_service;
  PSI_stage_service_t *stage_service;
  PSI_statement_service_t *statement_service;
  PSI_transaction_service_t *transaction_service;
  PSI_memory_service_t *memory_service;
  PSI_error_service_t *error_service;
  PSI_data_lock_service_t *data_lock_service;
  PSI_system_service_t *system_service;
  PSI_tls_channel_service_t *tls_channel_service;

  diag("test_bad_registration");

  load_perfschema(&thread_service, &mutex_service, &rwlock_service,
                  &cond_service, &file_service, &socket_service, &table_service,
                  &mdl_service, &idle_service, &stage_service,
                  &statement_service, &transaction_service, &memory_service,
                  &error_service, &data_lock_service, &system_service,
                  &tls_channel_service);
  /*
    Test that length('wait/synch/mutex/' (17) + category + '/' (1)) < 32
    --> category can be up to 13 chars for a mutex.
  */

  PSI_mutex_key dummy_mutex_key = 9999;
  PSI_mutex_info bad_mutex_1[] = {{&dummy_mutex_key, "X", 0, 0, ""}};

  mutex_service->register_mutex("/", bad_mutex_1, 1);
  ok(dummy_mutex_key == 0, "zero key");
  dummy_mutex_key = 9999;
  mutex_service->register_mutex("a/", bad_mutex_1, 1);
  ok(dummy_mutex_key == 0, "zero key");
  dummy_mutex_key = 9999;
  mutex_service->register_mutex("/b", bad_mutex_1, 1);
  ok(dummy_mutex_key == 0, "zero key");
  dummy_mutex_key = 9999;
  mutex_service->register_mutex("a/b", bad_mutex_1, 1);
  ok(dummy_mutex_key == 0, "zero key");
  dummy_mutex_key = 9999;
  mutex_service->register_mutex("12345678901234", bad_mutex_1, 1);
  ok(dummy_mutex_key == 0, "zero key");
  dummy_mutex_key = 9999;
  mutex_service->register_mutex("1234567890123", bad_mutex_1, 1);
  ok(dummy_mutex_key == 1, "assigned key");

  /*
    Test that length('wait/synch/mutex/' (17) + category + '/' (1) + name) <=
    128
    --> category + name can be up to 110 chars for a mutex.
  */

  dummy_mutex_key = 9999;
  PSI_mutex_info bad_mutex_2[] = {
      {&dummy_mutex_key,
       /* 110 chars name */
       "12345678901234567890123456789012345678901234567890"
       "12345678901234567890123456789012345678901234567890"
       "1234567890",
       0, 0, ""}};

  mutex_service->register_mutex("X", bad_mutex_2, 1);
  ok(dummy_mutex_key == 0, "zero key");

  dummy_mutex_key = 9999;
  PSI_mutex_info bad_mutex_3[] = {
      {&dummy_mutex_key,
       /* 109 chars name */
       "12345678901234567890123456789012345678901234567890"
       "12345678901234567890123456789012345678901234567890"
       "123456789",
       0, 0, ""}};

  mutex_service->register_mutex("XX", bad_mutex_3, 1);
  ok(dummy_mutex_key == 0, "zero key");

  mutex_service->register_mutex("X", bad_mutex_3, 1);
  ok(dummy_mutex_key == 2, "assigned key");

  /*
    Test that length('wait/synch/rwlock/' (18) + category + '/' (1)) < 32
    --> category can be up to 12 chars for a rwlock.
  */

  PSI_rwlock_key dummy_rwlock_key = 9999;
  PSI_rwlock_info bad_rwlock_1[] = {{&dummy_rwlock_key, "X", 0, 0, ""}};

  rwlock_service->register_rwlock("/", bad_rwlock_1, 1);
  ok(dummy_rwlock_key == 0, "zero key");
  dummy_rwlock_key = 9999;
  rwlock_service->register_rwlock("a/", bad_rwlock_1, 1);
  ok(dummy_rwlock_key == 0, "zero key");
  dummy_rwlock_key = 9999;
  rwlock_service->register_rwlock("/b", bad_rwlock_1, 1);
  ok(dummy_rwlock_key == 0, "zero key");
  dummy_rwlock_key = 9999;
  rwlock_service->register_rwlock("a/b", bad_rwlock_1, 1);
  ok(dummy_rwlock_key == 0, "zero key");
  dummy_rwlock_key = 9999;
  rwlock_service->register_rwlock("1234567890123", bad_rwlock_1, 1);
  ok(dummy_rwlock_key == 0, "zero key");
  dummy_rwlock_key = 9999;
  rwlock_service->register_rwlock("123456789012", bad_rwlock_1, 1);
  ok(dummy_rwlock_key == 1, "assigned key");

  /*
    Test that length('wait/synch/rwlock/' (18) + category + '/' (1) + name) <=
    128
    --> category + name can be up to 109 chars for a rwlock.
  */

  dummy_rwlock_key = 9999;
  PSI_rwlock_info bad_rwlock_2[] = {
      {&dummy_rwlock_key,
       /* 109 chars name */
       "12345678901234567890123456789012345678901234567890"
       "12345678901234567890123456789012345678901234567890"
       "123456789",
       0, 0, ""}};

  rwlock_service->register_rwlock("X", bad_rwlock_2, 1);
  ok(dummy_rwlock_key == 0, "zero key");

  dummy_rwlock_key = 9999;
  PSI_rwlock_info bad_rwlock_2_sx[] = {
      {&dummy_rwlock_key,
       /* 109 chars name */
       "12345678901234567890123456789012345678901234567890"
       "12345678901234567890123456789012345678901234567890"
       "123456789",
       PSI_FLAG_RWLOCK_SX, 0, ""}};

  rwlock_service->register_rwlock("Y", bad_rwlock_2_sx, 1);
  ok(dummy_rwlock_key == 0, "zero key SX");

  dummy_rwlock_key = 9999;
  PSI_rwlock_info bad_rwlock_3[] = {
      {&dummy_rwlock_key,
       /* 108 chars name */
       "12345678901234567890123456789012345678901234567890"
       "12345678901234567890123456789012345678901234567890"
       "12345678",
       0, 0, ""}};

  rwlock_service->register_rwlock("XX", bad_rwlock_3, 1);
  ok(dummy_rwlock_key == 0, "zero key");

  rwlock_service->register_rwlock("X", bad_rwlock_3, 1);
  ok(dummy_rwlock_key == 2, "assigned key");

  dummy_rwlock_key = 9999;
  PSI_rwlock_info bad_rwlock_3_sx[] = {
      {&dummy_rwlock_key,
       /* 108 chars name */
       "12345678901234567890123456789012345678901234567890"
       "12345678901234567890123456789012345678901234567890"
       "12345678",
       PSI_FLAG_RWLOCK_SX, 0, ""}};

  rwlock_service->register_rwlock("YY", bad_rwlock_3_sx, 1);
  ok(dummy_rwlock_key == 0, "zero key SX");

  rwlock_service->register_rwlock("Y", bad_rwlock_3_sx, 1);
  ok(dummy_rwlock_key == 3, "assigned key SX");

  /*
    Test that length('wait/synch/cond/' (16) + category + '/' (1)) < 32
    --> category can be up to 14 chars for a cond.
  */

  PSI_cond_key dummy_cond_key = 9999;
  PSI_cond_info bad_cond_1[] = {{&dummy_cond_key, "X", 0, 0, ""}};

  cond_service->register_cond("/", bad_cond_1, 1);
  ok(dummy_cond_key == 0, "zero key");
  dummy_cond_key = 9999;
  cond_service->register_cond("a/", bad_cond_1, 1);
  ok(dummy_cond_key == 0, "zero key");
  dummy_cond_key = 9999;
  cond_service->register_cond("/b", bad_cond_1, 1);
  ok(dummy_cond_key == 0, "zero key");
  dummy_cond_key = 9999;
  cond_service->register_cond("a/b", bad_cond_1, 1);
  ok(dummy_cond_key == 0, "zero key");
  dummy_cond_key = 9999;
  cond_service->register_cond("123456789012345", bad_cond_1, 1);
  ok(dummy_cond_key == 0, "zero key");
  dummy_cond_key = 9999;
  cond_service->register_cond("12345678901234", bad_cond_1, 1);
  ok(dummy_cond_key == 1, "assigned key");

  /*
    Test that length('wait/synch/cond/' (16) + category + '/' (1) + name) <= 128
    --> category + name can be up to 111 chars for a cond.
  */

  dummy_cond_key = 9999;
  PSI_cond_info bad_cond_2[] = {
      {&dummy_cond_key,
       /* 111 chars name */
       "12345678901234567890123456789012345678901234567890"
       "12345678901234567890123456789012345678901234567890"
       "12345678901",
       0, 0, ""}};

  cond_service->register_cond("X", bad_cond_2, 1);
  ok(dummy_cond_key == 0, "zero key");

  dummy_cond_key = 9999;
  PSI_cond_info bad_cond_3[] = {
      {&dummy_cond_key,
       /* 110 chars name */
       "12345678901234567890123456789012345678901234567890"
       "12345678901234567890123456789012345678901234567890"
       "1234567890",
       0, 0, ""}};

  cond_service->register_cond("XX", bad_cond_3, 1);
  ok(dummy_cond_key == 0, "zero key");

  cond_service->register_cond("X", bad_cond_3, 1);
  ok(dummy_cond_key == 2, "assigned key");

  /*
    Test that length('thread/' (7) + category + '/' (1)) < 32
    --> category can be up to 23 chars for a thread.
  */

  PSI_thread_key dummy_thread_key = 9999;
  PSI_thread_info bad_thread_1[] = {{&dummy_thread_key, "X", "X", 0, 0, ""}};

  thread_service->register_thread("/", bad_thread_1, 1);
  ok(dummy_thread_key == 0, "zero key");
  dummy_thread_key = 9999;
  thread_service->register_thread("a/", bad_thread_1, 1);
  ok(dummy_thread_key == 0, "zero key");
  dummy_thread_key = 9999;
  thread_service->register_thread("/b", bad_thread_1, 1);
  ok(dummy_thread_key == 0, "zero key");
  dummy_thread_key = 9999;
  thread_service->register_thread("a/b", bad_thread_1, 1);
  ok(dummy_thread_key == 0, "zero key");
  dummy_thread_key = 9999;
  thread_service->register_thread("123456789012345678901234", bad_thread_1, 1);
  ok(dummy_thread_key == 0, "zero key");
  dummy_thread_key = 9999;
  thread_service->register_thread("12345678901234567890123", bad_thread_1, 1);
  ok(dummy_thread_key == 1, "assigned key");

  /*
    Test that length('thread/' (7) + category + '/' (1) + name) <= 128
    --> category + name can be up to 120 chars for a thread.
  */

  dummy_thread_key = 9999;
  PSI_thread_info bad_thread_2[] = {
      {&dummy_thread_key,
       /* 120 chars name */
       "12345678901234567890123456789012345678901234567890"
       "12345678901234567890123456789012345678901234567890"
       "12345678901234567890",
       "BAD", 0, 0, ""}};

  thread_service->register_thread("X", bad_thread_2, 1);
  ok(dummy_thread_key == 0, "zero key");

  dummy_thread_key = 9999;
  PSI_thread_info bad_thread_3[] = {
      {&dummy_thread_key,
       /* 119 chars name */
       "12345678901234567890123456789012345678901234567890"
       "12345678901234567890123456789012345678901234567890"
       "1234567890123456789",
       "OK", 0, 0, ""}};

  thread_service->register_thread("XX", bad_thread_3, 1);
  ok(dummy_thread_key == 0, "zero key");

  thread_service->register_thread("X", bad_thread_3, 1);
  ok(dummy_thread_key == 2, "assigned key");

  /*
    Test that length('wait/io/file/' (13) + category + '/' (1)) < 32
    --> category can be up to 17 chars for a file.
  */

  PSI_file_key dummy_file_key = 9999;
  PSI_file_info bad_file_1[] = {{&dummy_file_key, "X", 0, 0, ""}};

  file_service->register_file("/", bad_file_1, 1);
  ok(dummy_file_key == 0, "zero key");
  dummy_file_key = 9999;
  file_service->register_file("a/", bad_file_1, 1);
  ok(dummy_file_key == 0, "zero key");
  dummy_file_key = 9999;
  file_service->register_file("/b", bad_file_1, 1);
  ok(dummy_file_key == 0, "zero key");
  dummy_file_key = 9999;
  file_service->register_file("a/b", bad_file_1, 1);
  ok(dummy_file_key == 0, "zero key");
  dummy_file_key = 9999;
  file_service->register_file("123456789012345678", bad_file_1, 1);
  ok(dummy_file_key == 0, "zero key");
  dummy_file_key = 9999;
  file_service->register_file("12345678901234567", bad_file_1, 1);
  ok(dummy_file_key == 1, "assigned key");

  /*
    Test that length('wait/io/file/' (13) + category + '/' (1) + name) <= 128
    --> category + name can be up to 114 chars for a file.
  */

  dummy_file_key = 9999;
  PSI_file_info bad_file_2[] = {
      {&dummy_file_key,
       /* 114 chars name */
       "12345678901234567890123456789012345678901234567890"
       "12345678901234567890123456789012345678901234567890"
       "12345678901234",
       0, 0, ""}};

  file_service->register_file("X", bad_file_2, 1);
  ok(dummy_file_key == 0, "zero key");

  dummy_file_key = 9999;
  PSI_file_info bad_file_3[] = {
      {&dummy_file_key,
       /* 113 chars name */
       "12345678901234567890123456789012345678901234567890"
       "12345678901234567890123456789012345678901234567890"
       "1234567890123",
       0, 0, ""}};

  file_service->register_file("XX", bad_file_3, 1);
  ok(dummy_file_key == 0, "zero key");

  file_service->register_file("X", bad_file_3, 1);
  ok(dummy_file_key == 2, "assigned key");

  /*
     Test that length('wait/io/socket/' (15) + category + '/' (1)) < 32
     --> category can be up to 15 chars for a socket.
   */

  PSI_socket_key dummy_socket_key = 9999;
  PSI_socket_info bad_socket_1[] = {{&dummy_socket_key, "X", 0, 0, ""}};

  socket_service->register_socket("/", bad_socket_1, 1);
  ok(dummy_socket_key == 0, "zero key");
  dummy_socket_key = 9999;
  socket_service->register_socket("a/", bad_socket_1, 1);
  ok(dummy_socket_key == 0, "zero key");
  dummy_socket_key = 9999;
  socket_service->register_socket("/b", bad_socket_1, 1);
  ok(dummy_socket_key == 0, "zero key");
  dummy_socket_key = 9999;
  socket_service->register_socket("a/b", bad_socket_1, 1);
  ok(dummy_socket_key == 0, "zero key");
  dummy_socket_key = 9999;
  socket_service->register_socket("1234567890123456", bad_socket_1, 1);
  ok(dummy_socket_key == 0, "zero key");
  dummy_socket_key = 9999;
  socket_service->register_socket("123456789012345", bad_socket_1, 1);
  ok(dummy_socket_key == 1, "assigned key");

  /*
    Test that length('wait/io/socket/' (15) + category + '/' (1) + name) <= 128
    --> category + name can be up to 112 chars for a socket.
  */

  dummy_socket_key = 9999;
  PSI_socket_info bad_socket_2[] = {
      {&dummy_socket_key,
       /* 112 chars name */
       "12345678901234567890123456789012345678901234567890"
       "12345678901234567890123456789012345678901234567890"
       "123456789012",
       0, 0, ""}};

  socket_service->register_socket("X", bad_socket_2, 1);
  ok(dummy_socket_key == 0, "zero key");

  dummy_socket_key = 9999;
  PSI_socket_info bad_socket_3[] = {
      {&dummy_socket_key,
       /* 111 chars name */
       "12345678901234567890123456789012345678901234567890"
       "12345678901234567890123456789012345678901234567890"
       "12345678901",
       0, 0, ""}};

  socket_service->register_socket("XX", bad_socket_3, 1);
  ok(dummy_socket_key == 0, "zero key");

  socket_service->register_socket("X", bad_socket_3, 1);
  ok(dummy_socket_key == 2, "assigned key");

  shutdown_performance_schema();
}

static void test_init_disabled() {
  PSI_thread_service_t *thread_service;
  PSI_mutex_service_t *mutex_service;
  PSI_rwlock_service_t *rwlock_service;
  PSI_cond_service_t *cond_service;
  PSI_file_service_t *file_service;
  PSI_socket_service_t *socket_service;
  PSI_table_service_t *table_service;
  PSI_mdl_service_t *mdl_service;
  PSI_idle_service_t *idle_service;
  PSI_stage_service_t *stage_service;
  PSI_statement_service_t *statement_service;
  PSI_transaction_service_t *transaction_service;
  PSI_memory_service_t *memory_service;
  PSI_error_service_t *error_service;
  PSI_data_lock_service_t *data_lock_service;
  PSI_system_service_t *system_service;
  PSI_tls_channel_service_t *tls_channel_service;

  diag("test_init_disabled");

  load_perfschema(&thread_service, &mutex_service, &rwlock_service,
                  &cond_service, &file_service, &socket_service, &table_service,
                  &mdl_service, &idle_service, &stage_service,
                  &statement_service, &transaction_service, &memory_service,
                  &error_service, &data_lock_service, &system_service,
                  &tls_channel_service);

  PSI_mutex_key mutex_key_A;
  PSI_mutex_info all_mutex[] = {{&mutex_key_A, "M-A", 0, 0, ""}};

  PSI_rwlock_key rwlock_key_A;
  PSI_rwlock_info all_rwlock[] = {{&rwlock_key_A, "RW-A", 0, 0, ""}};

  PSI_cond_key cond_key_A;
  PSI_cond_info all_cond[] = {{&cond_key_A, "C-A", 0, 0, ""}};

  PSI_file_key file_key_A;
  PSI_file_info all_file[] = {{&file_key_A, "F-A", 0, 0, ""}};

  PSI_socket_key socket_key_A;
  PSI_socket_info all_socket[] = {{&socket_key_A, "S-A", 0, 0, ""}};

  PSI_thread_key thread_key_1;
  PSI_thread_info all_thread[] = {{&thread_key_1, "T-1", "T-1", 0, 0, ""}};

  mutex_service->register_mutex("test", all_mutex, 1);
  rwlock_service->register_rwlock("test", all_rwlock, 1);
  cond_service->register_cond("test", all_cond, 1);
  file_service->register_file("test", all_file, 1);
  socket_service->register_socket("test", all_socket, 1);
  thread_service->register_thread("test", all_thread, 1);

  PFS_mutex_class *mutex_class_A;
  PFS_rwlock_class *rwlock_class_A;
  PFS_cond_class *cond_class_A;
  PFS_file_class *file_class_A;
  PFS_socket_class *socket_class_A;
  PSI_mutex *mutex_A1;
  PSI_rwlock *rwlock_A1;
  PSI_cond *cond_A1;
  PFS_file *file_A1;
  PSI_socket *socket_A1;
  PSI_thread *thread_1;

  /* Preparation */

  thread_1 = thread_service->new_thread(thread_key_1, 12, nullptr, 0);
  ok(thread_1 != nullptr, "T-1");
  thread_service->set_thread_id(thread_1, 1);

  mutex_class_A = find_mutex_class(mutex_key_A);
  ok(mutex_class_A != nullptr, "mutex class A");

  rwlock_class_A = find_rwlock_class(rwlock_key_A);
  ok(rwlock_class_A != nullptr, "rwlock class A");

  cond_class_A = find_cond_class(cond_key_A);
  ok(cond_class_A != nullptr, "cond class A");

  file_class_A = find_file_class(file_key_A);
  ok(file_class_A != nullptr, "file class A");

  socket_class_A = find_socket_class(socket_key_A);
  ok(socket_class_A != nullptr, "socket class A");

  /*
    Pretend thread T-1 is running, and disabled, with thread_instrumentation.
    Disabled instruments are still created so they can be enabled later.
  */

  /* ------------------------------------------------------------------------ */

  thread_service->set_thread(thread_1);
  setup_thread(thread_1, false);

  /* disabled M-A + disabled T-1: instrumentation */

  mutex_class_A->m_enabled = false;
  mutex_A1 = mutex_service->init_mutex(mutex_key_A, nullptr);
  ok(mutex_A1 != nullptr, "mutex_A1 disabled, instrumented");

  /* enabled M-A + disabled T-1: instrumentation (for later) */

  mutex_class_A->m_enabled = true;
  mutex_A1 = mutex_service->init_mutex(mutex_key_A, nullptr);
  ok(mutex_A1 != nullptr, "mutex_A1 enabled, instrumented");

  /* broken key + disabled T-1: no instrumentation */

  mutex_class_A->m_enabled = true;
  mutex_A1 = mutex_service->init_mutex(0, nullptr);
  ok(mutex_A1 == nullptr, "mutex key 0 not instrumented");
  mutex_A1 = mutex_service->init_mutex(99, nullptr);
  ok(mutex_A1 == nullptr, "broken mutex key not instrumented");

  /* disabled RW-A + disabled T-1: no instrumentation */

  rwlock_class_A->m_enabled = false;
  rwlock_A1 = rwlock_service->init_rwlock(rwlock_key_A, nullptr);
  ok(rwlock_A1 != nullptr, "rwlock_A1 disabled, instrumented");

  /* enabled RW-A + disabled T-1: instrumentation (for later) */

  rwlock_class_A->m_enabled = true;
  rwlock_A1 = rwlock_service->init_rwlock(rwlock_key_A, nullptr);
  ok(rwlock_A1 != nullptr, "rwlock_A1 enabled, instrumented");

  /* broken key + disabled T-1: no instrumentation */

  rwlock_class_A->m_enabled = true;
  rwlock_A1 = rwlock_service->init_rwlock(0, nullptr);
  ok(rwlock_A1 == nullptr, "rwlock key 0 not instrumented");
  rwlock_A1 = rwlock_service->init_rwlock(99, nullptr);
  ok(rwlock_A1 == nullptr, "broken rwlock key not instrumented");

  /* disabled C-A + disabled T-1: no instrumentation */

  cond_class_A->m_enabled = false;
  cond_A1 = cond_service->init_cond(cond_key_A, nullptr);
  ok(cond_A1 != nullptr, "cond_A1 disabled, instrumented");

  /* enabled C-A + disabled T-1: instrumentation (for later) */

  cond_class_A->m_enabled = true;
  cond_A1 = cond_service->init_cond(cond_key_A, nullptr);
  ok(cond_A1 != nullptr, "cond_A1 enabled, instrumented");

  /* broken key + disabled T-1: no instrumentation */

  cond_class_A->m_enabled = true;
  cond_A1 = cond_service->init_cond(0, nullptr);
  ok(cond_A1 == nullptr, "cond key 0 not instrumented");
  cond_A1 = cond_service->init_cond(99, nullptr);
  ok(cond_A1 == nullptr, "broken cond key not instrumented");

  /* disabled F-A + disabled T-1: no instrumentation */

  file_class_A->m_enabled = false;
  file_service->create_file(file_key_A, "foo", (File)12);
  file_A1 = lookup_file_by_name("foo");
  ok(file_A1 == nullptr, "file_A1 disabled, not instrumented");

  /* enabled F-A + disabled T-1: no instrumentation */

  file_class_A->m_enabled = true;
  file_service->create_file(file_key_A, "foo", (File)12);
  file_A1 = lookup_file_by_name("foo");
  ok(file_A1 == nullptr, "file_A1 enabled, not instrumented");

  /* broken key + disabled T-1: no instrumentation */

  file_class_A->m_enabled = true;
  file_service->create_file(0, "foo", (File)12);
  file_A1 = lookup_file_by_name("foo");
  ok(file_A1 == nullptr, "file_A1 not instrumented");
  file_service->create_file(99, "foo", (File)12);
  file_A1 = lookup_file_by_name("foo");
  ok(file_A1 == nullptr, "file_A1 not instrumented");

  /* disabled S-A + disabled T-1: no instrumentation */

  socket_class_A->m_enabled = false;
  socket_A1 = socket_service->init_socket(socket_key_A, nullptr, nullptr, 0);
  ok(socket_A1 != nullptr, "socket_A1 disabled, instrumented");

  /* enabled S-A + disabled T-1: instrumentation (for later) */

  socket_class_A->m_enabled = true;
  socket_A1 = socket_service->init_socket(socket_key_A, nullptr, nullptr, 0);
  ok(socket_A1 != nullptr, "socket_A1 enabled, instrumented");

  /* broken key + disabled T-1: no instrumentation */

  socket_class_A->m_enabled = true;
  socket_A1 = socket_service->init_socket(0, nullptr, nullptr, 0);
  ok(socket_A1 == nullptr, "socket key 0 not instrumented");
  socket_A1 = socket_service->init_socket(99, nullptr, nullptr, 0);
  ok(socket_A1 == nullptr, "broken socket key not instrumented");

  /* Pretend thread T-1 is enabled */
  /* ----------------------------- */

  setup_thread(thread_1, true);

  /* disabled M-A + enabled T-1: no instrumentation */

  mutex_class_A->m_enabled = false;
  mutex_A1 = mutex_service->init_mutex(mutex_key_A, nullptr);
  ok(mutex_A1 != nullptr, "mutex_A1 disabled, instrumented");

  /* enabled M-A + enabled T-1: instrumentation */

  mutex_class_A->m_enabled = true;
  mutex_A1 = mutex_service->init_mutex(mutex_key_A, nullptr);
  ok(mutex_A1 != nullptr, "mutex_A1 enabled, instrumented");
  mutex_service->destroy_mutex(mutex_A1);

  /* broken key + enabled T-1: no instrumentation */

  mutex_class_A->m_enabled = true;
  mutex_A1 = mutex_service->init_mutex(0, nullptr);
  ok(mutex_A1 == nullptr, "mutex_A1 not instrumented");
  mutex_A1 = mutex_service->init_mutex(99, nullptr);
  ok(mutex_A1 == nullptr, "mutex_A1 not instrumented");

  /* disabled RW-A + enabled T-1: no instrumentation */

  rwlock_class_A->m_enabled = false;
  rwlock_A1 = rwlock_service->init_rwlock(rwlock_key_A, nullptr);
  ok(rwlock_A1 != nullptr, "rwlock_A1 disabled, instrumented");

  /* enabled RW-A + enabled T-1: instrumentation */

  rwlock_class_A->m_enabled = true;
  rwlock_A1 = rwlock_service->init_rwlock(rwlock_key_A, nullptr);
  ok(rwlock_A1 != nullptr, "rwlock_A1 enabled, instrumented");
  rwlock_service->destroy_rwlock(rwlock_A1);

  /* broken key + enabled T-1: no instrumentation */

  rwlock_class_A->m_enabled = true;
  rwlock_A1 = rwlock_service->init_rwlock(0, nullptr);
  ok(rwlock_A1 == nullptr, "rwlock_A1 not instrumented");
  rwlock_A1 = rwlock_service->init_rwlock(99, nullptr);
  ok(rwlock_A1 == nullptr, "rwlock_A1 not instrumented");

  /* disabled C-A + enabled T-1: no instrumentation */

  cond_class_A->m_enabled = false;
  cond_A1 = cond_service->init_cond(cond_key_A, nullptr);
  ok(cond_A1 != nullptr, "cond_A1 disabled, instrumented");

  /* enabled C-A + enabled T-1: instrumentation */

  cond_class_A->m_enabled = true;
  cond_A1 = cond_service->init_cond(cond_key_A, nullptr);
  ok(cond_A1 != nullptr, "cond_A1 enabled, instrumented");
  cond_service->destroy_cond(cond_A1);

  /* broken key + enabled T-1: no instrumentation */

  cond_class_A->m_enabled = true;
  cond_A1 = cond_service->init_cond(0, nullptr);
  ok(cond_A1 == nullptr, "cond_A1 not instrumented");
  cond_A1 = cond_service->init_cond(99, nullptr);
  ok(cond_A1 == nullptr, "cond_A1 not instrumented");

  /* disabled F-A + enabled T-1: no instrumentation */

  file_class_A->m_enabled = false;
  file_service->create_file(file_key_A, "foo", (File)12);
  file_A1 = lookup_file_by_name("foo");
  ok(file_A1 == nullptr, "file_A1 not instrumented");

  /* enabled F-A + open failed + enabled T-1: no instrumentation */

  file_class_A->m_enabled = true;
  file_service->create_file(file_key_A, "foo", (File)-1);
  file_A1 = lookup_file_by_name("foo");
  ok(file_A1 == nullptr, "file_A1 not instrumented");

  /* enabled F-A + out-of-descriptors + enabled T-1: no instrumentation */

  file_class_A->m_enabled = true;
  file_service->create_file(file_key_A, "foo", (File)65000);
  file_A1 = lookup_file_by_name("foo");
  ok(file_A1 == nullptr, "file_A1 not instrumented");
  ok(file_handle_lost == 1, "lost a file handle");
  file_handle_lost = 0;

  /* enabled F-A + enabled T-1: instrumentation */

  file_class_A->m_enabled = true;
  file_service->create_file(file_key_A, "foo-instrumented", (File)12);
  file_A1 = lookup_file_by_name("foo-instrumented");
  ok(file_A1 != nullptr, "file_A1 instrumented");

  /* broken key + enabled T-1: no instrumentation */

  file_class_A->m_enabled = true;
  file_service->create_file(0, "foo", (File)12);
  file_A1 = lookup_file_by_name("foo");
  ok(file_A1 == nullptr, "file key 0 not instrumented");
  file_service->create_file(99, "foo", (File)12);
  file_A1 = lookup_file_by_name("foo");
  ok(file_A1 == nullptr, "broken file key not instrumented");

  /* disabled S-A + enabled T-1: no instrumentation */

  socket_class_A->m_enabled = false;
  ok(socket_A1 == nullptr, "socket_A1 not instrumented");

  /* enabled S-A + enabled T-1: instrumentation */

  socket_class_A->m_enabled = true;
  socket_A1 = socket_service->init_socket(socket_key_A, nullptr, nullptr, 0);
  ok(socket_A1 != nullptr, "socket_A1 instrumented");
  socket_service->destroy_socket(socket_A1);

  /* broken key + enabled T-1: no instrumentation */

  socket_class_A->m_enabled = true;
  socket_A1 = socket_service->init_socket(0, nullptr, nullptr, 0);
  ok(socket_A1 == nullptr, "socket_A1 not instrumented");
  socket_A1 = socket_service->init_socket(99, nullptr, nullptr, 0);
  ok(socket_A1 == nullptr, "socket_A1 not instrumented");

  /* Pretend the running thread is not instrumented */
  /* ---------------------------------------------- */

  thread_service->delete_current_thread();

  /* disabled M-A + unknown thread: no instrumentation */

  mutex_class_A->m_enabled = false;
  mutex_A1 = mutex_service->init_mutex(mutex_key_A, nullptr);
  ok(mutex_A1 != nullptr, "mutex_A1 disabled, instrumented");

  /* enabled M-A + unknown thread: instrumentation (for later) */

  mutex_class_A->m_enabled = true;
  mutex_A1 = mutex_service->init_mutex(mutex_key_A, nullptr);
  ok(mutex_A1 != nullptr, "mutex_A1 enabled, instrumented");

  /* broken key + unknown thread: no instrumentation */

  mutex_class_A->m_enabled = true;
  mutex_A1 = mutex_service->init_mutex(0, nullptr);
  ok(mutex_A1 == nullptr, "mutex key 0 not instrumented");
  mutex_A1 = mutex_service->init_mutex(99, nullptr);
  ok(mutex_A1 == nullptr, "broken mutex key not instrumented");

  /* disabled RW-A + unknown thread: no instrumentation */

  rwlock_class_A->m_enabled = false;
  rwlock_A1 = rwlock_service->init_rwlock(rwlock_key_A, nullptr);
  ok(rwlock_A1 != nullptr, "rwlock_A1 disabled, instrumented");

  /* enabled RW-A + unknown thread: instrumentation (for later) */

  rwlock_class_A->m_enabled = true;
  rwlock_A1 = rwlock_service->init_rwlock(rwlock_key_A, nullptr);
  ok(rwlock_A1 != nullptr, "rwlock_A1 enabled, instrumented");

  /* broken key + unknown thread: no instrumentation */

  rwlock_class_A->m_enabled = true;
  rwlock_A1 = rwlock_service->init_rwlock(0, nullptr);
  ok(rwlock_A1 == nullptr, "rwlock key 0 not instrumented");
  rwlock_A1 = rwlock_service->init_rwlock(99, nullptr);
  ok(rwlock_A1 == nullptr, "broken rwlock key not instrumented");

  /* disabled C-A + unknown thread: no instrumentation */

  cond_class_A->m_enabled = false;
  cond_A1 = cond_service->init_cond(cond_key_A, nullptr);
  ok(cond_A1 != nullptr, "cond_A1 disabled, instrumented");

  /* enabled C-A + unknown thread: instrumentation (for later) */

  cond_class_A->m_enabled = true;
  cond_A1 = cond_service->init_cond(cond_key_A, nullptr);
  ok(cond_A1 != nullptr, "cond_A1 enabled, instrumented");

  /* broken key + unknown thread: no instrumentation */

  cond_class_A->m_enabled = true;
  cond_A1 = cond_service->init_cond(0, nullptr);
  ok(cond_A1 == nullptr, "cond key 0 not instrumented");
  cond_A1 = cond_service->init_cond(99, nullptr);
  ok(cond_A1 == nullptr, "broken cond key not instrumented");

  /* disabled F-A + unknown thread: no instrumentation */

  file_class_A->m_enabled = false;
  file_service->create_file(file_key_A, "foo", (File)12);
  file_A1 = lookup_file_by_name("foo");
  ok(file_A1 == nullptr, "file_A1 not instrumented");

  /* enabled F-A + unknown thread: no instrumentation */

  file_class_A->m_enabled = true;
  file_service->create_file(file_key_A, "foo", (File)12);
  file_A1 = lookup_file_by_name("foo");
  ok(file_A1 == nullptr, "file_A1 not instrumented");

  /* broken key + unknown thread: no instrumentation */

  file_class_A->m_enabled = true;
  file_service->create_file(0, "foo", (File)12);
  file_A1 = lookup_file_by_name("foo");
  ok(file_A1 == nullptr, "not instrumented");
  file_service->create_file(99, "foo", (File)12);
  file_A1 = lookup_file_by_name("foo");
  ok(file_A1 == nullptr, "not instrumented");

  /* disabled S-A + unknown thread: no instrumentation */

  socket_class_A->m_enabled = false;
  socket_A1 = socket_service->init_socket(socket_key_A, nullptr, nullptr, 0);
  ok(socket_A1 != nullptr, "socket_A1 disabled, instrumented");

  /* enabled S-A + unknown thread: instrumentation (for later) */

  socket_class_A->m_enabled = true;
  socket_A1 = socket_service->init_socket(socket_key_A, nullptr, nullptr, 0);
  ok(socket_A1 != nullptr, "socket_A1 enabled, instrumented");

  /* broken key + unknown thread: no instrumentation */

  socket_class_A->m_enabled = true;
  socket_A1 = socket_service->init_socket(0, nullptr, nullptr, 0);
  ok(socket_A1 == nullptr, "socket key 0 not instrumented");
  socket_A1 = socket_service->init_socket(99, nullptr, nullptr, 0);
  ok(socket_A1 == nullptr, "broken socket key not instrumented");

  shutdown_performance_schema();
}

static void test_locker_disabled() {
  PSI_thread_service_t *thread_service;
  PSI_mutex_service_t *mutex_service;
  PSI_rwlock_service_t *rwlock_service;
  PSI_cond_service_t *cond_service;
  PSI_file_service_t *file_service;
  PSI_socket_service_t *socket_service;
  PSI_table_service_t *table_service;
  PSI_mdl_service_t *mdl_service;
  PSI_idle_service_t *idle_service;
  PSI_stage_service_t *stage_service;
  PSI_statement_service_t *statement_service;
  PSI_transaction_service_t *transaction_service;
  PSI_memory_service_t *memory_service;
  PSI_error_service_t *error_service;
  PSI_data_lock_service_t *data_lock_service;
  PSI_system_service_t *system_service;
  PSI_tls_channel_service_t *tls_channel_service;

  diag("test_locker_disabled");

  load_perfschema(&thread_service, &mutex_service, &rwlock_service,
                  &cond_service, &file_service, &socket_service, &table_service,
                  &mdl_service, &idle_service, &stage_service,
                  &statement_service, &transaction_service, &memory_service,
                  &error_service, &data_lock_service, &system_service,
                  &tls_channel_service);

  PSI_mutex_key mutex_key_A;
  PSI_mutex_info all_mutex[] = {{&mutex_key_A, "M-A", 0, 0, ""}};

  PSI_rwlock_key rwlock_key_A;
  PSI_rwlock_info all_rwlock[] = {{&rwlock_key_A, "RW-A", 0, 0, ""}};

  PSI_cond_key cond_key_A;
  PSI_cond_info all_cond[] = {{&cond_key_A, "C-A", 0, 0, ""}};

  PSI_file_key file_key_A;
  PSI_file_info all_file[] = {{&file_key_A, "F-A", 0, 0, ""}};

  PSI_socket_key socket_key_A;
  PSI_socket_info all_socket[] = {{&socket_key_A, "S-A", 0, 0, ""}};

  PSI_thread_key thread_key_1;
  PSI_thread_info all_thread[] = {{&thread_key_1, "T-1", "T-1", 0, 0, ""}};

  mutex_service->register_mutex("test", all_mutex, 1);
  rwlock_service->register_rwlock("test", all_rwlock, 1);
  cond_service->register_cond("test", all_cond, 1);
  file_service->register_file("test", all_file, 1);
  socket_service->register_socket("test", all_socket, 1);
  thread_service->register_thread("test", all_thread, 1);

  PFS_mutex_class *mutex_class_A;
  PFS_rwlock_class *rwlock_class_A;
  PFS_cond_class *cond_class_A;
  PFS_file_class *file_class_A;
  PFS_socket_class *socket_class_A;
  PSI_mutex *mutex_A1;
  PSI_rwlock *rwlock_A1;
  PSI_cond *cond_A1;
  PSI_file *file_A1;
  PSI_socket *socket_A1;
  PSI_thread *thread_1;

  /* Preparation */

  thread_1 = thread_service->new_thread(thread_key_1, 12, nullptr, 0);
  ok(thread_1 != nullptr, "T-1");
  thread_service->set_thread_id(thread_1, 1);

  mutex_class_A = find_mutex_class(mutex_key_A);
  ok(mutex_class_A != nullptr, "mutex info A");

  rwlock_class_A = find_rwlock_class(rwlock_key_A);
  ok(rwlock_class_A != nullptr, "rwlock info A");

  cond_class_A = find_cond_class(cond_key_A);
  ok(cond_class_A != nullptr, "cond info A");

  file_class_A = find_file_class(file_key_A);
  ok(file_class_A != nullptr, "file info A");

  socket_class_A = find_socket_class(socket_key_A);
  ok(socket_class_A != nullptr, "socket info A");

  /* Pretend thread T-1 is running, and enabled */
  /* ------------------------------------------ */

  thread_service->set_thread(thread_1);
  setup_thread(thread_1, true);

  /* Enable all instruments, instantiate objects */

  mutex_class_A->m_enabled = true;
  mutex_A1 = mutex_service->init_mutex(mutex_key_A, nullptr);
  ok(mutex_A1 != nullptr, "instrumented");

  rwlock_class_A->m_enabled = true;
  rwlock_A1 = rwlock_service->init_rwlock(rwlock_key_A, nullptr);
  ok(rwlock_A1 != nullptr, "instrumented");

  cond_class_A->m_enabled = true;
  cond_A1 = cond_service->init_cond(cond_key_A, nullptr);
  ok(cond_A1 != nullptr, "instrumented");

  file_class_A->m_enabled = true;
  file_service->create_file(file_key_A, "foo", (File)12);
  file_A1 = (PSI_file *)lookup_file_by_name("foo");
  ok(file_A1 != nullptr, "instrumented");

  socket_class_A->m_enabled = true;
  socket_A1 = socket_service->init_socket(socket_key_A, nullptr, nullptr, 0);
  ok(socket_A1 != nullptr, "instrumented");

  /* Socket lockers require a thread owner */
  socket_service->set_socket_thread_owner(socket_A1);

  PSI_mutex_locker *mutex_locker;
  PSI_mutex_locker_state mutex_state;
  PSI_rwlock_locker *rwlock_locker;
  PSI_rwlock_locker_state rwlock_state;
  PSI_cond_locker *cond_locker;
  PSI_cond_locker_state cond_state;
  PSI_file_locker *file_locker;
  PSI_file_locker_state file_state;
  PSI_socket_locker *socket_locker;
  PSI_socket_locker_state socket_state;

  /* Pretend thread T-1 is disabled */
  /* ------------------------------ */

  setup_thread(thread_1, false);
  flag_events_waits_current = true;
  mutex_class_A->m_enabled = true;
  rwlock_class_A->m_enabled = true;
  cond_class_A->m_enabled = true;
  file_class_A->m_enabled = true;
  socket_class_A->m_enabled = true;

  mutex_locker = mutex_service->start_mutex_wait(&mutex_state, mutex_A1,
                                                 PSI_MUTEX_LOCK, "foo.cc", 12);
  ok(mutex_locker == nullptr, "no locker (T-1 disabled)");
  rwlock_locker = rwlock_service->start_rwlock_rdwait(
      &rwlock_state, rwlock_A1, PSI_RWLOCK_READLOCK, "foo.cc", 12);
  ok(rwlock_locker == nullptr, "no locker (T-1 disabled)");
  cond_locker = cond_service->start_cond_wait(&cond_state, cond_A1, mutex_A1,
                                              PSI_COND_WAIT, "foo.cc", 12);
  ok(cond_locker == nullptr, "no locker (T-1 disabled)");
  file_locker = file_service->get_thread_file_name_locker(
      &file_state, file_key_A, PSI_FILE_OPEN, "xxx", nullptr);
  ok(file_locker == nullptr, "no locker (T-1 disabled)");
  file_locker = file_service->get_thread_file_stream_locker(
      &file_state, file_A1, PSI_FILE_READ);
  ok(file_locker == nullptr, "no locker (T-1 disabled)");
  file_locker = file_service->get_thread_file_descriptor_locker(
      &file_state, (File)12, PSI_FILE_READ);
  ok(file_locker == nullptr, "no locker (T-1 disabled)");
  socket_locker = socket_service->start_socket_wait(
      &socket_state, socket_A1, PSI_SOCKET_SEND, 12, "foo.cc", 12);
  ok(socket_locker == nullptr, "no locker (T-1 disabled)");

  /* Pretend the global consumer is disabled */
  /* --------------------------------------- */

  setup_thread(thread_1, true);
  flag_global_instrumentation = false;
  mutex_class_A->m_enabled = true;
  rwlock_class_A->m_enabled = true;
  cond_class_A->m_enabled = true;
  file_class_A->m_enabled = true;
  socket_class_A->m_enabled = true;
  update_instruments_derived_flags();

  ok(mutex_A1->m_enabled == false, "mutex_A1 disabled");
  ok(rwlock_A1->m_enabled == false, "rwlock_A1 disabled");
  ok(cond_A1->m_enabled == false, "cond_A1 disabled");

  file_locker = file_service->get_thread_file_name_locker(
      &file_state, file_key_A, PSI_FILE_OPEN, "xxx", nullptr);
  ok(file_locker == nullptr, "no locker (global disabled)");

  file_locker = file_service->get_thread_file_stream_locker(
      &file_state, file_A1, PSI_FILE_READ);
  ok(file_locker == nullptr, "no locker (global disabled)");

  file_locker = file_service->get_thread_file_descriptor_locker(
      &file_state, (File)12, PSI_FILE_READ);
  ok(file_locker == nullptr, "no locker (global disabled)");

  ok(socket_A1->m_enabled == false, "socket_A1 disabled");

  /* Pretend the mode is global, counted only */
  /* ---------------------------------------- */

  setup_thread(thread_1, true);
  flag_global_instrumentation = true;
  flag_thread_instrumentation = false;
  mutex_class_A->m_enabled = true;
  mutex_class_A->m_timed = false;
  rwlock_class_A->m_enabled = true;
  rwlock_class_A->m_timed = false;
  cond_class_A->m_enabled = true;
  cond_class_A->m_timed = false;
  file_class_A->m_enabled = true;
  file_class_A->m_timed = false;
  socket_class_A->m_enabled = true;
  socket_class_A->m_timed = false;
  update_instruments_derived_flags();

  mutex_locker = mutex_service->start_mutex_wait(&mutex_state, mutex_A1,
                                                 PSI_MUTEX_LOCK, "foo.cc", 12);
  ok(mutex_locker == nullptr, "no locker (global counted)");
  rwlock_locker = rwlock_service->start_rwlock_rdwait(
      &rwlock_state, rwlock_A1, PSI_RWLOCK_READLOCK, "foo.cc", 12);
  ok(rwlock_locker == nullptr, "no locker (global counted)");
  cond_locker = cond_service->start_cond_wait(&cond_state, cond_A1, mutex_A1,
                                              PSI_COND_WAIT, "foo.cc", 12);
  ok(cond_locker == nullptr, "no locker (global counted)");
  file_locker = file_service->get_thread_file_name_locker(
      &file_state, file_key_A, PSI_FILE_OPEN, "xxx", nullptr);
  ok(file_locker != nullptr, "locker (global counted)");
  file_service->start_file_wait(file_locker, 10, __FILE__, __LINE__);
  file_service->end_file_wait(file_locker, 10);
  file_locker = file_service->get_thread_file_stream_locker(
      &file_state, file_A1, PSI_FILE_READ);
  ok(file_locker != nullptr, "locker (global counted)");
  file_service->start_file_wait(file_locker, 10, __FILE__, __LINE__);
  file_service->end_file_wait(file_locker, 10);
  file_locker = file_service->get_thread_file_descriptor_locker(
      &file_state, (File)12, PSI_FILE_READ);
  ok(file_locker != nullptr, "locker (global counted)");
  file_service->start_file_wait(file_locker, 10, __FILE__, __LINE__);
  file_service->end_file_wait(file_locker, 10);
  /* The null locker shortcut applies only to socket ops with no byte count */
  socket_locker = socket_service->start_socket_wait(
      &socket_state, socket_A1, PSI_SOCKET_BIND, 0, "foo.cc", 12);
  ok(socket_locker == nullptr, "no locker (global counted)");

  /* TODO */

  /* Pretend the instrument is disabled */
  /* ---------------------------------- */

  setup_thread(thread_1, true);
  flag_global_instrumentation = true;
  flag_events_waits_current = true;
  mutex_class_A->m_enabled = false;
  rwlock_class_A->m_enabled = false;
  cond_class_A->m_enabled = false;
  file_class_A->m_enabled = false;
  socket_class_A->m_enabled = false;
  update_instruments_derived_flags();

  ok(mutex_A1->m_enabled == false, "mutex_A1 disabled");

  ok(rwlock_A1->m_enabled == false, "rwlock_A1 disabled");

  ok(cond_A1->m_enabled == false, "cond_A1 disabled");

  file_locker = file_service->get_thread_file_name_locker(
      &file_state, file_key_A, PSI_FILE_OPEN, "xxx", nullptr);
  ok(file_locker == nullptr, "no locker");

  file_locker = file_service->get_thread_file_stream_locker(
      &file_state, file_A1, PSI_FILE_READ);
  ok(file_locker == nullptr, "no locker");

  file_locker = file_service->get_thread_file_descriptor_locker(
      &file_state, (File)12, PSI_FILE_READ);
  ok(file_locker == nullptr, "no locker");

  ok(socket_A1->m_enabled == false, "socket_A1 disabled");

  /* Pretend everything is enabled and timed */
  /* --------------------------------------- */

  setup_thread(thread_1, true);
  flag_global_instrumentation = true;
  flag_thread_instrumentation = true;
  flag_events_waits_current = true;
  mutex_class_A->m_enabled = true;
  mutex_class_A->m_timed = true;
  rwlock_class_A->m_enabled = true;
  rwlock_class_A->m_timed = true;
  cond_class_A->m_enabled = true;
  cond_class_A->m_timed = true;
  file_class_A->m_enabled = true;
  file_class_A->m_timed = true;
  socket_class_A->m_enabled = true;
  socket_class_A->m_timed = true;
  update_instruments_derived_flags();

  mutex_locker = mutex_service->start_mutex_wait(
      &mutex_state, mutex_A1, PSI_MUTEX_LOCK, __FILE__, __LINE__);
  ok(mutex_locker != nullptr, "locker");
  mutex_service->end_mutex_wait(mutex_locker, 0);
  rwlock_locker = rwlock_service->start_rwlock_rdwait(
      &rwlock_state, rwlock_A1, PSI_RWLOCK_READLOCK, __FILE__, __LINE__);
  ok(rwlock_locker != nullptr, "locker");
  rwlock_service->end_rwlock_rdwait(rwlock_locker, 0);
  cond_locker = cond_service->start_cond_wait(
      &cond_state, cond_A1, mutex_A1, PSI_COND_WAIT, __FILE__, __LINE__);
  ok(cond_locker != nullptr, "locker");
  cond_service->end_cond_wait(cond_locker, 0);
  file_locker = file_service->get_thread_file_name_locker(
      &file_state, file_key_A, PSI_FILE_STREAM_OPEN, "xxx", nullptr);
  ok(file_locker != nullptr, "locker");
  file_service->start_file_open_wait(file_locker, __FILE__, __LINE__);
  file_service->end_file_open_wait(file_locker, nullptr);
  file_locker = file_service->get_thread_file_stream_locker(
      &file_state, file_A1, PSI_FILE_READ);
  ok(file_locker != nullptr, "locker");
  file_service->start_file_wait(file_locker, 10, __FILE__, __LINE__);
  file_service->end_file_wait(file_locker, 10);
  file_locker = file_service->get_thread_file_descriptor_locker(
      &file_state, (File)12, PSI_FILE_READ);
  ok(file_locker != nullptr, "locker");
  file_service->start_file_wait(file_locker, 10, __FILE__, __LINE__);
  file_service->end_file_wait(file_locker, 10);
  socket_locker = socket_service->start_socket_wait(
      &socket_state, socket_A1, PSI_SOCKET_SEND, 12, "foo.cc", 12);
  ok(socket_locker != nullptr, "locker");
  socket_service->end_socket_wait(socket_locker, 10);

  /* Pretend the socket does not have a thread owner */
  /* ---------------------------------------------- */

  socket_class_A->m_enabled = true;
  socket_A1 = socket_service->init_socket(socket_key_A, nullptr, nullptr, 0);
  ok(socket_A1 != nullptr, "instrumented");
  /* Socket thread owner has not been set */
  socket_locker = socket_service->start_socket_wait(
      &socket_state, socket_A1, PSI_SOCKET_SEND, 12, "foo.cc", 12);
  ok(socket_locker != nullptr, "locker (owner not used)");
  socket_service->end_socket_wait(socket_locker, 10);

  /* Pretend the running thread is not instrumented */
  /* ---------------------------------------------- */

  thread_service->delete_current_thread();
  flag_events_waits_current = true;
  mutex_class_A->m_enabled = true;
  rwlock_class_A->m_enabled = true;
  cond_class_A->m_enabled = true;
  file_class_A->m_enabled = true;
  socket_class_A->m_enabled = true;
  update_instruments_derived_flags();

  mutex_locker = mutex_service->start_mutex_wait(&mutex_state, mutex_A1,
                                                 PSI_MUTEX_LOCK, "foo.cc", 12);
  ok(mutex_locker == nullptr, "no locker");
  rwlock_locker = rwlock_service->start_rwlock_rdwait(
      &rwlock_state, rwlock_A1, PSI_RWLOCK_READLOCK, "foo.cc", 12);
  ok(rwlock_locker == nullptr, "no locker");
  cond_locker = cond_service->start_cond_wait(&cond_state, cond_A1, mutex_A1,
                                              PSI_COND_WAIT, "foo.cc", 12);
  ok(cond_locker == nullptr, "no locker");
  file_locker = file_service->get_thread_file_name_locker(
      &file_state, file_key_A, PSI_FILE_OPEN, "xxx", nullptr);
  ok(file_locker == nullptr, "no locker");
  file_locker = file_service->get_thread_file_stream_locker(
      &file_state, file_A1, PSI_FILE_READ);
  ok(file_locker == nullptr, "no locker");
  file_locker = file_service->get_thread_file_descriptor_locker(
      &file_state, (File)12, PSI_FILE_READ);
  ok(file_locker == nullptr, "no locker");
  socket_locker = socket_service->start_socket_wait(
      &socket_state, socket_A1, PSI_SOCKET_SEND, 12, "foo.cc", 12);
  ok(socket_locker == nullptr, "no locker");

  shutdown_performance_schema();
}

static void test_file_instrumentation_leak() {
  PSI_thread_service_t *thread_service;
  PSI_mutex_service_t *mutex_service;
  PSI_rwlock_service_t *rwlock_service;
  PSI_cond_service_t *cond_service;
  PSI_file_service_t *file_service;
  PSI_socket_service_t *socket_service;
  PSI_table_service_t *table_service;
  PSI_mdl_service_t *mdl_service;
  PSI_idle_service_t *idle_service;
  PSI_stage_service_t *stage_service;
  PSI_statement_service_t *statement_service;
  PSI_transaction_service_t *transaction_service;
  PSI_memory_service_t *memory_service;
  PSI_error_service_t *error_service;
  PSI_data_lock_service_t *data_lock_service;
  PSI_system_service_t *system_service;
  PSI_tls_channel_service_t *tls_channel_service;

  diag("test_file_instrumentation_leak");

  load_perfschema(&thread_service, &mutex_service, &rwlock_service,
                  &cond_service, &file_service, &socket_service, &table_service,
                  &mdl_service, &idle_service, &stage_service,
                  &statement_service, &transaction_service, &memory_service,
                  &error_service, &data_lock_service, &system_service,
                  &tls_channel_service);

  PSI_file_key file_key_A;
  PSI_file_key file_key_B;
  PSI_file_info all_file[] = {{&file_key_A, "F-A", 0, 0, ""},
                              {&file_key_B, "F-B", 0, 0, ""}};

  PSI_thread_key thread_key_1;
  PSI_thread_info all_thread[] = {{&thread_key_1, "T-1", "T-1", 0, 0, ""}};

  file_service->register_file("test", all_file, 2);
  thread_service->register_thread("test", all_thread, 1);

  PFS_file_class *file_class_A;
  PFS_file_class *file_class_B;
  PSI_file_locker_state file_state;
  PSI_thread *thread_1;

  /* Preparation */

  thread_1 = thread_service->new_thread(thread_key_1, 12, nullptr, 0);
  ok(thread_1 != nullptr, "T-1");
  thread_service->set_thread_id(thread_1, 1);

  file_class_A = find_file_class(file_key_A);
  ok(file_class_A != nullptr, "file info A");

  file_class_B = find_file_class(file_key_B);
  ok(file_class_B != nullptr, "file info B");

  thread_service->set_thread(thread_1);

  /* Pretend everything is enabled */
  /* ----------------------------- */

  setup_thread(thread_1, true);
  flag_events_waits_current = true;
  file_class_A->m_enabled = true;
  file_class_B->m_enabled = true;

  PSI_file_locker *file_locker;

  /* Simulate OPEN + READ of 100 bytes + CLOSE on descriptor 12 */

  file_locker = file_service->get_thread_file_name_locker(
      &file_state, file_key_A, PSI_FILE_OPEN, "AAA", nullptr);
  ok(file_locker != nullptr, "locker");
  file_service->start_file_open_wait(file_locker, __FILE__, __LINE__);
  file_service->end_file_open_wait_and_bind_to_descriptor(file_locker, 12);

  file_locker = file_service->get_thread_file_descriptor_locker(
      &file_state, (File)12, PSI_FILE_READ);
  ok(file_locker != nullptr, "locker");
  file_service->start_file_wait(file_locker, 100, __FILE__, __LINE__);
  file_service->end_file_wait(file_locker, 100);

  file_locker = file_service->get_thread_file_descriptor_locker(
      &file_state, (File)12, PSI_FILE_CLOSE);
  ok(file_locker != nullptr, "locker");
  file_service->start_file_wait(file_locker, 0, __FILE__, __LINE__);
  file_service->end_file_wait(file_locker, 0);

  /* Simulate uninstrumented-OPEN + WRITE on descriptor 24 */

  file_locker = file_service->get_thread_file_descriptor_locker(
      &file_state, (File)24, PSI_FILE_WRITE);
  ok(file_locker == nullptr, "no locker, since the open was not instrumented");

  /*
    Simulate uninstrumented-OPEN + WRITE on descriptor 12 :
    the instrumentation should not leak (don't charge the file io on unknown B
    to "AAA")
  */

  file_locker = file_service->get_thread_file_descriptor_locker(
      &file_state, (File)12, PSI_FILE_WRITE);
  ok(file_locker == nullptr, "no locker, no leak");

  shutdown_performance_schema();
}

#ifdef LATER
static void test_enabled() {
  PSI *psi;

  diag("test_enabled");

  psi = load_perfschema();

  PSI_mutex_key mutex_key_A;
  PSI_mutex_key mutex_key_B;
  PSI_mutex_info all_mutex[] = {{&mutex_key_A, "M-A", 0, 0, ""},
                                {&mutex_key_B, "M-B", 0, 0, ""}};

  PSI_rwlock_key rwlock_key_A;
  PSI_rwlock_key rwlock_key_B;
  PSI_rwlock_info all_rwlock[] = {{&rwlock_key_A, "RW-A", 0, 0, ""},
                                  {&rwlock_key_B, "RW-B", 0, 0, ""}};

  PSI_cond_key cond_key_A;
  PSI_cond_key cond_key_B;
  PSI_cond_info all_cond[] = {{&cond_key_A, "C-A", 0, 0, ""},
                              {&cond_key_B, "C-B", 0, 0, ""}};

  shutdown_performance_schema();
}
#endif

static void test_event_name_index() {
  PSI_thread_service_t *thread_service;
  PSI_mutex_service_t *mutex_service;
  PSI_rwlock_service_t *rwlock_service;
  PSI_cond_service_t *cond_service;
  PSI_file_service_t *file_service;
  PSI_socket_service_t *socket_service;
  PSI_table_service_t *table_service;
  PSI_mdl_service_t *mdl_service;
  PSI_idle_service_t *idle_service;
  PSI_stage_service_t *stage_service;
  PSI_statement_service_t *statement_service;
  PSI_transaction_service_t *transaction_service;
  PSI_memory_service_t *memory_service;
  PSI_error_service_t *error_service;
  PSI_data_lock_service_t *data_lock_service;
  PSI_tls_channel_service_t *tls_channel_service;

  PSI_thread_bootstrap *thread_boot;
  PSI_mutex_bootstrap *mutex_boot;
  PSI_rwlock_bootstrap *rwlock_boot;
  PSI_cond_bootstrap *cond_boot;
  PSI_file_bootstrap *file_boot;
  PSI_socket_bootstrap *socket_boot;
  PSI_table_bootstrap *table_boot;
  PSI_mdl_bootstrap *mdl_boot;
  PSI_idle_bootstrap *idle_boot;
  PSI_stage_bootstrap *stage_boot;
  PSI_statement_bootstrap *statement_boot;
  PSI_transaction_bootstrap *transaction_boot;
  PSI_memory_bootstrap *memory_boot;
  PSI_error_bootstrap *error_boot;
  PSI_data_lock_bootstrap *data_lock_boot;
  PSI_system_bootstrap *system_boot;
  PSI_tls_channel_bootstrap *tls_channel_boot;
  PFS_global_param param;

  diag("test_event_name_index");

  memset(&param, 0xFF, sizeof(param));
  param.m_enabled = true;

  /* NOTE: Need to add 4 to each index: table io, table lock, idle, metadata
   * lock */

  /* Per mutex info waits should be at [0..9] */
  param.m_mutex_class_sizing = 10;
  /* Per rwlock info waits should be at [10..29] */
  param.m_rwlock_class_sizing = 20;
  /* Per cond info waits should be at [30..69] */
  param.m_cond_class_sizing = 40;
  /* Per file info waits should be at [70..149] */
  param.m_file_class_sizing = 80;
  /* Per socket info waits should be at [150..309] */
  param.m_socket_class_sizing = 160;
  /* Per table info waits should be at [310] */
  param.m_table_share_sizing = 320;

  param.m_thread_class_sizing = 0;
  param.m_user_sizing = 0;
  param.m_account_sizing = 0;
  param.m_host_sizing = 0;
  param.m_stage_class_sizing = 0;
  param.m_events_stages_history_sizing = 0;
  param.m_events_stages_history_long_sizing = 0;
  param.m_statement_class_sizing = 0;
  param.m_events_statements_history_sizing = 0;
  param.m_events_statements_history_long_sizing = 0;
  param.m_events_transactions_history_sizing = 0;
  param.m_events_transactions_history_long_sizing = 0;
  param.m_digest_sizing = 0;
  param.m_session_connect_attrs_sizing = 0;
  param.m_program_sizing = 0;
  param.m_statement_stack_sizing = 10;
  param.m_memory_class_sizing = 12;
  param.m_metadata_lock_sizing = 10;
  param.m_max_digest_length = 0;
  param.m_max_sql_text_length = 1000;
  param.m_error_sizing = 0;
  param.m_consumer_events_stages_current_enabled = false;
  param.m_consumer_events_stages_history_enabled = false;
  param.m_consumer_events_stages_history_long_enabled = false;
  param.m_consumer_events_statements_cpu_enabled = false;
  param.m_consumer_events_statements_current_enabled = false;
  param.m_consumer_events_statements_history_enabled = false;
  param.m_consumer_events_statements_history_long_enabled = false;
  param.m_consumer_events_transactions_current_enabled = false;
  param.m_consumer_events_transactions_history_enabled = false;
  param.m_consumer_events_transactions_history_long_enabled = false;
  param.m_consumer_events_waits_current_enabled = false;
  param.m_consumer_events_waits_history_enabled = false;
  param.m_consumer_events_waits_history_long_enabled = false;
  param.m_consumer_global_instrumentation_enabled = false;
  param.m_consumer_thread_instrumentation_enabled = false;
  param.m_consumer_statement_digest_enabled = false;

  param.m_mutex_sizing = 0;
  param.m_rwlock_sizing = 0;
  param.m_cond_sizing = 0;
  param.m_thread_sizing = 0;
  param.m_table_sizing = 0;
  param.m_file_sizing = 0;
  param.m_file_handle_sizing = 0;
  param.m_socket_sizing = 0;
  param.m_events_waits_history_sizing = 0;
  param.m_events_waits_history_long_sizing = 0;
  param.m_setup_actor_sizing = 0;
  param.m_setup_object_sizing = 0;

  param.m_hints.m_table_definition_cache = 100;
  param.m_hints.m_table_open_cache = 100;
  param.m_hints.m_max_connections = 100;
  param.m_hints.m_open_files_limit = 100;
  param.m_hints.m_max_prepared_stmt_count = 100;

  pre_initialize_performance_schema();
  initialize_performance_schema(
      &param, &thread_boot, &mutex_boot, &rwlock_boot, &cond_boot, &file_boot,
      &socket_boot, &table_boot, &mdl_boot, &idle_boot, &stage_boot,
      &statement_boot, &transaction_boot, &memory_boot, &error_boot,
      &data_lock_boot, &system_boot, &tls_channel_boot);
  ok(thread_boot != nullptr, "thread_bootstrap");
  ok(mutex_boot != nullptr, "mutex_bootstrap");
  ok(rwlock_boot != nullptr, "rwlock_bootstrap");
  ok(cond_boot != nullptr, "cond_bootstrap");
  ok(file_boot != nullptr, "file_bootstrap");
  ok(socket_boot != nullptr, "socket_bootstrap");
  ok(table_boot != nullptr, "table_bootstrap");
  ok(mdl_boot != nullptr, "mdl_bootstrap");
  ok(idle_boot != nullptr, "idle_bootstrap");
  ok(stage_boot != nullptr, "stage_bootstrap");
  ok(statement_boot != nullptr, "statement_bootstrap");
  ok(transaction_boot != nullptr, "transaction_bootstrap");
  ok(memory_boot != nullptr, "memory_bootstrap");
  ok(error_boot != nullptr, "error_bootstrap");
  ok(data_lock_boot != nullptr, "data_lock_bootstrap");
  ok(tls_channel_boot != nullptr, "tls_channel_bootstrap");

  thread_service = (PSI_thread_service_t *)thread_boot->get_interface(
      PSI_CURRENT_THREAD_VERSION);
  ok(thread_service != nullptr, "thread_service");
  mutex_service =
      (PSI_mutex_service_t *)mutex_boot->get_interface(PSI_MUTEX_VERSION_1);
  ok(mutex_service != nullptr, "mutex_service");
  rwlock_service =
      (PSI_rwlock_service_t *)rwlock_boot->get_interface(PSI_RWLOCK_VERSION_2);
  ok(rwlock_service != nullptr, "rwlock_service");
  cond_service =
      (PSI_cond_service_t *)cond_boot->get_interface(PSI_COND_VERSION_1);
  ok(cond_service != nullptr, "cond_service");
  file_service =
      (PSI_file_service_t *)file_boot->get_interface(PSI_FILE_VERSION_2);
  ok(file_service != nullptr, "file_service");
  socket_service =
      (PSI_socket_service_t *)socket_boot->get_interface(PSI_SOCKET_VERSION_1);
  ok(socket_service != nullptr, "socket_service");
  table_service =
      (PSI_table_service_t *)table_boot->get_interface(PSI_TABLE_VERSION_1);
  ok(table_service != nullptr, "table_service");
  mdl_service =
      (PSI_mdl_service_t *)mdl_boot->get_interface(PSI_CURRENT_MDL_VERSION);
  ok(mdl_service != nullptr, "mdl_service");
  idle_service =
      (PSI_idle_service_t *)idle_boot->get_interface(PSI_IDLE_VERSION_1);
  ok(idle_service != nullptr, "idle_service");
  stage_service =
      (PSI_stage_service_t *)stage_boot->get_interface(PSI_STAGE_VERSION_1);
  ok(stage_service != nullptr, "stage_service");
  statement_service = (PSI_statement_service_t *)statement_boot->get_interface(
      PSI_CURRENT_STATEMENT_VERSION);
  ok(statement_service != nullptr, "statement_service");
  transaction_service =
      (PSI_transaction_service_t *)transaction_boot->get_interface(
          PSI_TRANSACTION_VERSION_1);
  ok(transaction_service != nullptr, "transaction_service");
  memory_service =
      (PSI_memory_service_t *)memory_boot->get_interface(PSI_MEMORY_VERSION_2);
  ok(memory_service != nullptr, "memory_service");
  error_service =
      (PSI_error_service_t *)error_boot->get_interface(PSI_MEMORY_VERSION_1);
  ok(error_service != nullptr, "error_service");
  data_lock_service = (PSI_data_lock_service_t *)data_lock_boot->get_interface(
      PSI_DATA_LOCK_VERSION_1);
  ok(data_lock_service != nullptr, "data_lock_service");
  tls_channel_service =
      (PSI_tls_channel_service_t *)tls_channel_boot->get_interface(
          PSI_TLS_CHANNEL_VERSION_1);
  ok(tls_channel_service != nullptr, "tls_channel_service");

  PFS_mutex_class *mutex_class;
  PSI_mutex_key dummy_mutex_key_1;
  PSI_mutex_key dummy_mutex_key_2;
  PSI_mutex_info dummy_mutexes[] = {{&dummy_mutex_key_1, "M-1", 0, 0, ""},
                                    {&dummy_mutex_key_2, "M-2", 0, 0, ""}};

  mutex_service->register_mutex("X", dummy_mutexes, 2);
  mutex_class = find_mutex_class(dummy_mutex_key_1);
  ok(mutex_class != nullptr, "mutex class 1");
  ok(mutex_class->m_event_name_index == 4, "index 4");
  mutex_class = find_mutex_class(dummy_mutex_key_2);
  ok(mutex_class != nullptr, "mutex class 2");
  ok(mutex_class->m_event_name_index == 5, "index 5");

  PFS_rwlock_class *rwlock_class;
  PSI_rwlock_key dummy_rwlock_key_1;
  PSI_rwlock_key dummy_rwlock_key_2;
  PSI_rwlock_info dummy_rwlocks[] = {{&dummy_rwlock_key_1, "RW-1", 0, 0, ""},
                                     {&dummy_rwlock_key_2, "RW-2", 0, 0, ""}};

  rwlock_service->register_rwlock("X", dummy_rwlocks, 2);
  rwlock_class = find_rwlock_class(dummy_rwlock_key_1);
  ok(rwlock_class != nullptr, "rwlock class 1");
  ok(rwlock_class->m_event_name_index == 14, "index 14");
  rwlock_class = find_rwlock_class(dummy_rwlock_key_2);
  ok(rwlock_class != nullptr, "rwlock class 2");
  ok(rwlock_class->m_event_name_index == 15, "index 15");

  PFS_cond_class *cond_class;
  PSI_cond_key dummy_cond_key_1;
  PSI_cond_key dummy_cond_key_2;
  PSI_cond_info dummy_conds[] = {{&dummy_cond_key_1, "C-1", 0, 0, ""},
                                 {&dummy_cond_key_2, "C-2", 0, 0, ""}};

  cond_service->register_cond("X", dummy_conds, 2);
  cond_class = find_cond_class(dummy_cond_key_1);
  ok(cond_class != nullptr, "cond class 1");
  ok(cond_class->m_event_name_index == 34, "index 34");
  cond_class = find_cond_class(dummy_cond_key_2);
  ok(cond_class != nullptr, "cond class 2");
  ok(cond_class->m_event_name_index == 35, "index 35");

  PFS_file_class *file_class;
  PSI_file_key dummy_file_key_1;
  PSI_file_key dummy_file_key_2;
  PSI_file_info dummy_files[] = {{&dummy_file_key_1, "F-1", 0, 0, ""},
                                 {&dummy_file_key_2, "F-2", 0, 0, ""}};

  file_service->register_file("X", dummy_files, 2);
  file_class = find_file_class(dummy_file_key_1);
  ok(file_class != nullptr, "file class 1");
  ok(file_class->m_event_name_index == 74, "index 74");
  file_class = find_file_class(dummy_file_key_2);
  ok(file_class != nullptr, "file class 2");
  ok(file_class->m_event_name_index == 75, "index 75");

  PFS_socket_class *socket_class;
  PSI_socket_key dummy_socket_key_1;
  PSI_socket_key dummy_socket_key_2;
  PSI_socket_info dummy_sockets[] = {{&dummy_socket_key_1, "S-1", 0, 0, ""},
                                     {&dummy_socket_key_2, "S-2", 0, 0, ""}};

  socket_service->register_socket("X", dummy_sockets, 2);
  socket_class = find_socket_class(dummy_socket_key_1);
  ok(socket_class != nullptr, "socket class 1");
  ok(socket_class->m_event_name_index == 154, "index 154");
  socket_class = find_socket_class(dummy_socket_key_2);
  ok(socket_class != nullptr, "socket class 2");
  ok(socket_class->m_event_name_index == 155, "index 155");

  ok(global_table_io_class.m_event_name_index == 0, "index 0");
  ok(global_table_lock_class.m_event_name_index == 1, "index 1");
  ok(wait_class_max = 314, "314 event names");  // 4 global classes

  shutdown_performance_schema();
}

static void test_memory_instruments() {
  PSI_thread_service_t *thread_service;
  PSI_mutex_service_t *mutex_service;
  PSI_rwlock_service_t *rwlock_service;
  PSI_cond_service_t *cond_service;
  PSI_file_service_t *file_service;
  PSI_socket_service_t *socket_service;
  PSI_table_service_t *table_service;
  PSI_mdl_service_t *mdl_service;
  PSI_idle_service_t *idle_service;
  PSI_stage_service_t *stage_service;
  PSI_statement_service_t *statement_service;
  PSI_transaction_service_t *transaction_service;
  PSI_memory_service_t *memory_service;
  PSI_error_service_t *error_service;
  PSI_data_lock_service_t *data_lock_service;
  PSI_system_service_t *system_service;
  PSI_tls_channel_service_t *tls_channel_service;
  PSI_thread *owner;

  diag("test_memory_instruments");

  load_perfschema(&thread_service, &mutex_service, &rwlock_service,
                  &cond_service, &file_service, &socket_service, &table_service,
                  &mdl_service, &idle_service, &stage_service,
                  &statement_service, &transaction_service, &memory_service,
                  &error_service, &data_lock_service, &system_service,
                  &tls_channel_service);

  PSI_memory_key memory_key_A;
  PSI_memory_info all_memory[] = {{&memory_key_A, "M-A", 0, 0, ""}};

  PSI_thread_key thread_key_1;
  PSI_thread_info all_thread[] = {{&thread_key_1, "T-1", "T-1", 0, 0, ""}};

  memory_service->register_memory("test", all_memory, 1);
  thread_service->register_thread("test", all_thread, 1);

  PFS_memory_class *memory_class_A;
  PSI_thread *thread_1;
  PSI_memory_key key;

  /* Preparation */

  thread_1 = thread_service->new_thread(thread_key_1, 12, nullptr, 0);
  ok(thread_1 != nullptr, "T-1");
  thread_service->set_thread_id(thread_1, 1);

  memory_class_A = find_memory_class(memory_key_A);
  ok(memory_class_A != nullptr, "memory info A");

  /* Pretend thread T-1 is running, and enabled */
  /* ------------------------------------------ */

  thread_service->set_thread(thread_1);
  setup_thread(thread_1, true);

  /* Enable all instruments */

  memory_class_A->m_enabled = true;

  /* for coverage, need to print stats collected. */

  key = memory_service->memory_alloc(memory_key_A, 100, &owner);
  ok(key == memory_key_A, "alloc memory info A");
  key = memory_service->memory_realloc(memory_key_A, 100, 200, &owner);
  ok(key == memory_key_A, "realloc memory info A");
  key = memory_service->memory_realloc(memory_key_A, 200, 300, &owner);
  ok(key == memory_key_A, "realloc up memory info A");
  key = memory_service->memory_realloc(memory_key_A, 300, 50, &owner);
  ok(key == memory_key_A, "realloc down memory info A");
  memory_service->memory_free(memory_key_A, 50, owner);

  /* Use global instrumentation only */
  /* ------------------------------- */

  flag_thread_instrumentation = false;

  key = memory_service->memory_alloc(memory_key_A, 100, &owner);
  ok(key == memory_key_A, "alloc memory info A");
  key = memory_service->memory_realloc(memory_key_A, 100, 200, &owner);
  ok(key == memory_key_A, "realloc memory info A");
  key = memory_service->memory_realloc(memory_key_A, 200, 300, &owner);
  ok(key == memory_key_A, "realloc up memory info A");
  key = memory_service->memory_realloc(memory_key_A, 300, 50, &owner);
  ok(key == memory_key_A, "realloc down memory info A");
  memory_service->memory_free(memory_key_A, 50, owner);

  /* Garbage, for robustness */
  /* ----------------------- */

  key = memory_service->memory_alloc(9999, 100, &owner);
  ok(key == PSI_NOT_INSTRUMENTED, "alloc with unknown key");
  key = memory_service->memory_realloc(PSI_NOT_INSTRUMENTED, 100, 200, &owner);
  ok(key == PSI_NOT_INSTRUMENTED, "realloc with unknown key");
  memory_service->memory_free(PSI_NOT_INSTRUMENTED, 200, owner);

  shutdown_performance_schema();
}

static void test_leaks() {
  PSI_thread_bootstrap *thread_boot;
  PSI_mutex_bootstrap *mutex_boot;
  PSI_rwlock_bootstrap *rwlock_boot;
  PSI_cond_bootstrap *cond_boot;
  PSI_file_bootstrap *file_boot;
  PSI_socket_bootstrap *socket_boot;
  PSI_table_bootstrap *table_boot;
  PSI_mdl_bootstrap *mdl_boot;
  PSI_idle_bootstrap *idle_boot;
  PSI_stage_bootstrap *stage_boot;
  PSI_statement_bootstrap *statement_boot;
  PSI_transaction_bootstrap *transaction_boot;
  PSI_memory_bootstrap *memory_boot;
  PSI_data_lock_bootstrap *data_lock_boot;
  PSI_error_bootstrap *error_boot;
  PSI_system_bootstrap *system_boot;
  PSI_tls_channel_bootstrap *tls_channel_boot;
  PFS_global_param param;

  /* Allocate everything, to make sure cleanup does not forget anything. */

  memset(&param, 0xFF, sizeof(param));
  param.m_enabled = true;
  param.m_mutex_class_sizing = 10;
  param.m_rwlock_class_sizing = 10;
  param.m_cond_class_sizing = 10;
  param.m_thread_class_sizing = 10;
  param.m_table_share_sizing = 10;
  param.m_file_class_sizing = 10;
  param.m_socket_class_sizing = 10;
  param.m_mutex_sizing = 1000;
  param.m_rwlock_sizing = 1000;
  param.m_cond_sizing = 1000;
  param.m_thread_sizing = 1000;
  param.m_table_sizing = 1000;
  param.m_file_sizing = 1000;
  param.m_file_handle_sizing = 1000;
  param.m_socket_sizing = 1000;
  param.m_events_waits_history_sizing = 10;
  param.m_events_waits_history_long_sizing = 1000;
  param.m_setup_actor_sizing = 1000;
  param.m_setup_object_sizing = 1000;
  param.m_host_sizing = 1000;
  param.m_user_sizing = 1000;
  param.m_account_sizing = 1000;
  param.m_stage_class_sizing = 10;
  param.m_events_stages_history_sizing = 10;
  param.m_events_stages_history_long_sizing = 1000;
  param.m_statement_class_sizing = 10;
  param.m_events_statements_history_sizing = 10;
  param.m_events_statements_history_long_sizing = 1000;
  param.m_session_connect_attrs_sizing = 1000;
  param.m_memory_class_sizing = 10;
  param.m_metadata_lock_sizing = 1000;
  param.m_digest_sizing = 1000;
  param.m_program_sizing = 1000;
  param.m_statement_stack_sizing = 10;
  param.m_max_digest_length = 1000;
  param.m_max_sql_text_length = 1000;
  param.m_error_sizing = 1000;
  param.m_consumer_events_stages_current_enabled = false;
  param.m_consumer_events_stages_history_enabled = false;
  param.m_consumer_events_stages_history_long_enabled = false;
  param.m_consumer_events_statements_cpu_enabled = false;
  param.m_consumer_events_statements_current_enabled = false;
  param.m_consumer_events_statements_history_enabled = false;
  param.m_consumer_events_statements_history_long_enabled = false;
  param.m_consumer_events_transactions_current_enabled = false;
  param.m_consumer_events_transactions_history_enabled = false;
  param.m_consumer_events_transactions_history_long_enabled = false;
  param.m_consumer_events_waits_current_enabled = false;
  param.m_consumer_events_waits_history_enabled = false;
  param.m_consumer_events_waits_history_long_enabled = false;
  param.m_consumer_global_instrumentation_enabled = false;
  param.m_consumer_thread_instrumentation_enabled = false;
  param.m_consumer_statement_digest_enabled = false;

  param.m_hints.m_table_definition_cache = 100;
  param.m_hints.m_table_open_cache = 100;
  param.m_hints.m_max_connections = 100;
  param.m_hints.m_open_files_limit = 100;
  param.m_hints.m_max_prepared_stmt_count = 100;

  pre_initialize_performance_schema();
  initialize_performance_schema(
      &param, &thread_boot, &mutex_boot, &rwlock_boot, &cond_boot, &file_boot,
      &socket_boot, &table_boot, &mdl_boot, &idle_boot, &stage_boot,
      &statement_boot, &transaction_boot, &memory_boot, &error_boot,
      &data_lock_boot, &system_boot, &tls_channel_boot);
  ok(thread_boot != nullptr, "thread bootstrap");
  ok(mutex_boot != nullptr, "mutex bootstrap");
  ok(rwlock_boot != nullptr, "rwlock bootstrap");
  ok(cond_boot != nullptr, "cond bootstrap");
  ok(file_boot != nullptr, "file bootstrap");
  ok(socket_boot != nullptr, "socket bootstrap");
  ok(table_boot != nullptr, "table bootstrap");
  ok(mdl_boot != nullptr, "mdl bootstrap");
  ok(idle_boot != nullptr, "idle bootstrap");
  ok(stage_boot != nullptr, "stage bootstrap");
  ok(statement_boot != nullptr, "statement bootstrap");
  ok(transaction_boot != nullptr, "transaction bootstrap");
  ok(memory_boot != nullptr, "memory bootstrap");
  ok(error_boot != nullptr, "error bootstrap");
  shutdown_performance_schema();

  /* Leaks will be reported with valgrind */
}

const char *temp_filename1 = "MLfd=12";
const char *temp_filename2 = "MLfd=13";

/* Simulated my_create_temp_file() */
File my_create_temp_file(const char **filename) {
  *filename = temp_filename1;
  return 12;
}

/* Simulated my_close() */
int my_close(File fd [[maybe_unused]], bool success) {
  return (success ? 0 : 1);
}

/* Simulated my_delete() */
int my_delete(const char *filename [[maybe_unused]], bool success) {
  return (success ? 0 : 1);
}

/* Simulated my_rename() */
int my_rename(const char *from [[maybe_unused]],
              const char *to [[maybe_unused]], bool success) {
  return (success ? 0 : 1);
}

static void test_file_operations() {
  PSI_thread_service_t *thread_service;
  PSI_mutex_service_t *mutex_service;
  PSI_rwlock_service_t *rwlock_service;
  PSI_cond_service_t *cond_service;
  PSI_file_service_t *file_service;
  PSI_socket_service_t *socket_service;
  PSI_table_service_t *table_service;
  PSI_mdl_service_t *mdl_service;
  PSI_idle_service_t *idle_service;
  PSI_stage_service_t *stage_service;
  PSI_statement_service_t *statement_service;
  PSI_transaction_service_t *transaction_service;
  PSI_memory_service_t *memory_service;
  PSI_error_service_t *error_service;
  PSI_data_lock_service_t *data_lock_service;
  PSI_system_service_t *system_service;
  PSI_tls_channel_service_t *tls_channel_service;

  diag("test_file_operations SETUP");

  load_perfschema(&thread_service, &mutex_service, &rwlock_service,
                  &cond_service, &file_service, &socket_service, &table_service,
                  &mdl_service, &idle_service, &stage_service,
                  &statement_service, &transaction_service, &memory_service,
                  &error_service, &data_lock_service, &system_service,
                  &tls_channel_service);

  PFS_file_class *file_class;
  PSI_thread *thread_A, *thread_B;
  PSI_file_locker *locker_A, *locker_B;
  PSI_file_locker_state state_A, state_B;
  File fd1, fd2;
  const char *filename1, *filename2;
  int rc = 0;

  PSI_file_key file_key;
  PSI_file_info all_file[] = {{&file_key, "File Class", 0, 0, ""}};
  PSI_thread_key thread_key;
  PSI_thread_info all_thread[] = {
      {&thread_key, "Thread Class", "OS NAME", 0, 0, ""}};

  file_service->register_file("test", all_file, 1);
  thread_service->register_thread("test", all_thread, 1);

  /* Create Thread A and B to simulate operations from different threads. */
  thread_A = thread_service->new_thread(thread_key, 12, nullptr, 0);
  ok(thread_A != nullptr, "Thread A");
  thread_service->set_thread_id(thread_A, 1);

  thread_B = thread_service->new_thread(thread_key, 12, nullptr, 0);
  ok(thread_B != nullptr, "Thread B");
  thread_service->set_thread_id(thread_B, 1);

  file_class = find_file_class(file_key);
  ok(file_class != nullptr, "File Class");

  flag_global_instrumentation = true;
  flag_thread_instrumentation = true;
  file_class->m_enabled = true;
  file_class->m_timed = true;
  update_instruments_derived_flags();

  setup_thread(thread_A, true);
  setup_thread(thread_B, true);
  flag_events_waits_current = true;
  file_class->m_enabled = true;

  /*
    TEST 1: Simulate race of mysql_file_close() on Thread A and
            mysql_file_create_temp() on Thread B
  */
  diag("test_file_operations TEST 1");

  /* THREAD A */
  thread_service->set_thread(thread_A);
  /* Create a temporary file */
  locker_A = file_service->get_thread_file_name_locker(
      &state_A, file_key, PSI_FILE_CREATE, nullptr, &locker_A);
  ok(locker_A != nullptr, "locker A");
  file_service->start_file_open_wait(locker_A, __FILE__, __LINE__);
  /* Returns filename with embedded FD */
  fd1 = my_create_temp_file(&filename1);
  file_service->end_temp_file_open_wait_and_bind_to_descriptor(locker_A, fd1,
                                                               filename1);
  /* THREAD A */
  /* Start mysql_file_close */
  locker_A = file_service->get_thread_file_descriptor_locker(&state_A, fd1,
                                                             PSI_FILE_CLOSE);
  ok(locker_A != nullptr, "locker A");
  file_service->start_file_close_wait(locker_A, __FILE__, __LINE__);
  rc = my_close(fd1, true); /* successful close, FD released */

  /* THREAD B */
  thread_service->set_thread(thread_B);
  /* Create a temporary file with the same FD before Thread A completes
     mysql_file_close()
  */
  locker_B = file_service->get_thread_file_name_locker(
      &state_B, file_key, PSI_FILE_CREATE, nullptr, &locker_B);
  ok(locker_B != nullptr, "locker B");
  file_service->start_file_open_wait(locker_B, __FILE__, __LINE__);
  /* Returns same FD and filename as Thread A */
  fd2 = my_create_temp_file(&filename2);
  file_service->end_temp_file_open_wait_and_bind_to_descriptor(locker_B, fd2,
                                                               filename2);
  /* THREAD A */
  thread_service->set_thread(thread_A);
  /* Complete mysql_file_close() */
  file_service->end_file_close_wait(locker_A, rc);

  /* THREAD B */
  /* Close the file and clean up */
  locker_B = file_service->get_thread_file_descriptor_locker(&state_B, fd2,
                                                             PSI_FILE_CLOSE);
  ok(locker_B != nullptr, "locker A");
  file_service->start_file_close_wait(locker_B, __FILE__, __LINE__);
  rc = my_close(fd2, true); /* successful close, FD released */
  file_service->end_file_close_wait(locker_B, rc);

  /*
    TEST 2: Disable file instrumentation after a file has been created and
            before it is closed. Re-enable the instrumentation, then create the
            and close the file again.
  */
  diag("test_file_operations TEST 2");

  /* Create a temporary file */
  thread_service->set_thread(thread_A);
  locker_A = file_service->get_thread_file_name_locker(
      &state_A, file_key, PSI_FILE_CREATE, nullptr, &locker_A);
  ok(locker_A != nullptr, "locker A");
  file_service->start_file_open_wait(locker_A, __FILE__, __LINE__);
  /* Returns filename with embedded FD */
  fd1 = my_create_temp_file(&filename1);
  file_service->end_temp_file_open_wait_and_bind_to_descriptor(locker_A, fd1,
                                                               filename1);
  /* Disable file instrumentation */
  file_class->m_enabled = false;
  update_instruments_derived_flags();

  /* mysql_file_close() */
  locker_A = file_service->get_thread_file_descriptor_locker(&state_A, fd1,
                                                             PSI_FILE_CLOSE);
  /* File instrumentation should be deleted for temporary files. */
  ok(locker_A == nullptr, "locker A is NULL");
  rc = my_close(fd1, true); /* successful close, FD released */

  /* Re-enable the file instrumentation */
  file_class->m_enabled = true;
  update_instruments_derived_flags();

  /* Open the same temporary file with the same FD */
  locker_A = file_service->get_thread_file_name_locker(
      &state_A, file_key, PSI_FILE_CREATE, nullptr, &locker_A);
  ok(locker_A != nullptr, "locker A");
  file_service->start_file_open_wait(locker_A, __FILE__, __LINE__);
  /* Returns filename with embedded FD */
  fd1 = my_create_temp_file(&filename1);
  file_service->end_temp_file_open_wait_and_bind_to_descriptor(locker_A, fd1,
                                                               filename1);
  /* mysql_file_close() */
  locker_A = file_service->get_thread_file_descriptor_locker(&state_A, fd1,
                                                             PSI_FILE_CLOSE);
  ok(locker_A != nullptr, "locker A");
  file_service->start_file_close_wait(locker_A, __FILE__, __LINE__);
  rc = my_close(fd1, true); /* successful close, FD released */
  /* Checks for correct open count */
  file_service->end_file_close_wait(locker_A, rc);

  /*
    TEST 3: Disable file instrumentation after a file has been created and
            before it is deleted. Re-enable the instrumentation, then create
            and delete the file again.
  */
  diag("test_file_operations TEST 3");

  /* Create a temporary file */
  thread_service->set_thread(thread_A);
  locker_A = file_service->get_thread_file_name_locker(
      &state_A, file_key, PSI_FILE_CREATE, nullptr, &locker_A);
  ok(locker_A != nullptr, "locker A");
  file_service->start_file_open_wait(locker_A, __FILE__, __LINE__);
  /* Returns filename with embedded FD */
  fd1 = my_create_temp_file(&filename1);
  file_service->end_temp_file_open_wait_and_bind_to_descriptor(locker_A, fd1,
                                                               filename1);
  /* Disable file instrumentation */
  file_class->m_enabled = false;
  update_instruments_derived_flags();

  /* mysql_file_delete() */
  locker_A = file_service->get_thread_file_name_locker(
      &state_A, file_key, PSI_FILE_DELETE, temp_filename1, &locker_A);
  /* Locker should be NULL if instrumentation disabled. */
  ok(locker_A == nullptr, "locker A");
  rc = my_delete(temp_filename1, true); /* successful delete */

  /* Re-enable the file instrumentation */
  file_class->m_enabled = true;
  update_instruments_derived_flags();

  /* Open the same temporary file with the same FD */
  locker_A = file_service->get_thread_file_name_locker(
      &state_A, file_key, PSI_FILE_CREATE, nullptr, &locker_A);
  ok(locker_A != nullptr, "locker A");
  file_service->start_file_open_wait(locker_A, __FILE__, __LINE__);
  /* Returns filename with embedded FD */
  fd1 = my_create_temp_file(&filename1);
  file_service->end_temp_file_open_wait_and_bind_to_descriptor(locker_A, fd1,
                                                               filename1);

  /* mysql_file_delete() */
  locker_A = file_service->get_thread_file_name_locker(
      &state_A, file_key, PSI_FILE_DELETE, temp_filename1, &locker_A);
  ok(locker_A != nullptr, "locker A");
  file_service->start_file_close_wait(locker_A, __FILE__, __LINE__);
  rc = my_delete(temp_filename1, true); /* successful delete */
  file_service->end_file_close_wait(locker_A, rc);

  /*
    TEST 4: Disable file instrumentation after a file has been created and
            before it is renamed. Re-enable the instrumentation, then delete,
            create and delete the file again.
  */
  diag("test_file_operations TEST 4");

  /* Create a temporary file */
  thread_service->set_thread(thread_A);
  locker_A = file_service->get_thread_file_name_locker(
      &state_A, file_key, PSI_FILE_CREATE, nullptr, &locker_A);
  ok(locker_A != nullptr, "locker A");
  file_service->start_file_open_wait(locker_A, __FILE__, __LINE__);
  /* Returns filename with embedded FD */
  fd1 = my_create_temp_file(&filename1);
  file_service->end_temp_file_open_wait_and_bind_to_descriptor(locker_A, fd1,
                                                               filename1);
  /* Disable file instrumentation */
  file_class->m_enabled = false;
  update_instruments_derived_flags();

  /* mysql_file_rename() */
  locker_A = file_service->get_thread_file_name_locker(
      &state_A, file_key, PSI_FILE_RENAME, temp_filename1, &locker_A);
  /* Locker should be NULL if file instrumentation disabled. */
  ok(locker_A == nullptr, "locker A");
  rc = my_rename(temp_filename1, temp_filename2, true); /* success */

  /* Re-enable the file instrumentation */
  file_class->m_enabled = true;
  update_instruments_derived_flags();

  /* mysql_file_delete() */
  locker_A = file_service->get_thread_file_name_locker(
      &state_A, file_key, PSI_FILE_DELETE, temp_filename2, &locker_A);
  ok(locker_A != nullptr, "locker A");
  file_service->start_file_close_wait(locker_A, __FILE__, __LINE__);
  rc = my_delete(temp_filename2, true); /* success */
  file_service->end_file_close_wait(locker_A, rc);

  /* Open the original file with the same FD */
  locker_A = file_service->get_thread_file_name_locker(
      &state_A, file_key, PSI_FILE_CREATE, nullptr, &locker_A);
  ok(locker_A != nullptr, "locker A");
  file_service->start_file_open_wait(locker_A, __FILE__, __LINE__);
  /* Returns filename with embedded FD */
  fd1 = my_create_temp_file(&filename1);
  file_service->end_temp_file_open_wait_and_bind_to_descriptor(locker_A, fd1,
                                                               filename1);
  /* mysql_file_delete() */
  locker_A = file_service->get_thread_file_name_locker(
      &state_A, file_key, PSI_FILE_DELETE, temp_filename1, &locker_A);
  ok(locker_A != nullptr, "locker A");
  file_service->start_file_close_wait(locker_A, __FILE__, __LINE__);
  rc = my_delete(temp_filename1, true); /* successful delete */
  file_service->end_file_close_wait(locker_A, rc);

  thread_service->delete_thread(thread_A);
  thread_service->delete_thread(thread_B);
  shutdown_performance_schema();
}

/**
  Verify two properties of the maps defined in
  terminology_use_previous.cc:

  - Key and value should be different (or else it's a typo).

  - The same key should not appear in multiple versions (limitation
    of the framework.)
*/
static void test_terminology_use_previous() {
  for (auto &class_map : version_vector) {
    for (auto &str_map_pair : class_map) {
      for (auto &str_pair : str_map_pair.second) {
        // Key and value should be different.
        ok(str_pair.first != str_pair.second, "key and value are different");

        // Key should not appear in any other version. Currently,
        // there is nothing to check - the break statement will
        // execute in the first iteration - because there is only one
        // version.  This will be relevant if we extend the range of
        // terminology_use_previous to more than two values.
        for (auto &class_map2 : version_vector) {
          if (class_map2 == class_map) break;  // Only check older versions
#ifndef NDEBUG
          const auto &str_map_pair2 = class_map2.find(str_map_pair.first);
          if (str_map_pair2 != class_map2.end()) {
            const auto &str_map2 = str_map_pair2->second;
            const auto &pair2 = str_map2.find(str_pair.first);
            assert(pair2 == str_map2.end());
          }
#endif
        }
      }
    }
  }
}

static void do_all_tests() {
  /* system charset needed by pfs_statements_digest */
  system_charset_info = &my_charset_latin1;

  /* Using initialize_performance_schema(), no partial init needed. */
  test_bootstrap();
  test_bad_registration();
  test_init_disabled();
  test_locker_disabled();
  test_file_instrumentation_leak();
  test_event_name_index();
  test_memory_instruments();
  test_leaks();
  test_file_operations();
  test_terminology_use_previous();
}

int main(int, char **) {
  plan(417);

  MY_INIT("pfs-t");
  do_all_tests();
  my_end(0);
  return (exit_status());
}
