/* Copyright (c) 2008, 2019, Oracle and/or its affiliates. All rights reserved.

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
#include "storage/perfschema/unittest/stub_pfs_defaults.h"
#include "storage/perfschema/unittest/stub_pfs_plugin_table.h"
#include "storage/perfschema/unittest/stub_print_error.h"
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

  while (pfs != NULL) {
    /*
      When a file "foo" is instrumented, the name is normalized
      to "/path/to/current/directory/foo", so we remove the
      directory name here to find it back.
    */
    dirlen = dirname_length(pfs->m_filename);
    filename = pfs->m_filename + dirlen;
    filename_length = pfs->m_filename_length - dirlen;
    if ((len == filename_length) &&
        (strncmp(name, filename, filename_length) == 0))
      return pfs;

    pfs = it.scan_next();
  }

  return NULL;
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
      &data_lock_boot, &system_boot);
  ok(thread_boot != NULL, "thread_boot");
  ok(mutex_boot != NULL, "mutex_boot");
  ok(rwlock_boot != NULL, "rwlock_boot");
  ok(cond_boot != NULL, "cond_boot");
  ok(file_boot != NULL, "file_boot");
  ok(socket_boot != NULL, "socket_boot");
  ok(table_boot != NULL, "table_boot");
  ok(mdl_boot != NULL, "mdl_boot");
  ok(idle_boot != NULL, "idle_boot");
  ok(stage_boot != NULL, "stage_boot");
  ok(statement_boot != NULL, "statement_boot");
  ok(transaction_boot != NULL, "transaction_boot");
  ok(memory_boot != NULL, "memory_boot");
  ok(error_boot != NULL, "error_boot");
  ok(data_lock_boot != NULL, "data_lock_boot");

  ok(thread_boot->get_interface != NULL, "thread_boot->get_interface");
  ok(mutex_boot->get_interface != NULL, "mutex_boot->get_interface");
  ok(rwlock_boot->get_interface != NULL, "rwlock_boot->get_interface");
  ok(cond_boot->get_interface != NULL, "cond_boot->get_interface");
  ok(file_boot->get_interface != NULL, "file_boot->get_interface");
  ok(socket_boot->get_interface != NULL, "socket_boot->get_interface");
  ok(table_boot->get_interface != NULL, "table_boot->get_interface");
  ok(mdl_boot->get_interface != NULL, "mdl_boot->get_interface");
  ok(idle_boot->get_interface != NULL, "idle_boot->get_interface");
  ok(stage_boot->get_interface != NULL, "stage_boot->get_interface");
  ok(statement_boot->get_interface != NULL, "statement_boot->get_interface");
  ok(transaction_boot->get_interface != NULL,
     "transaction_boot->get_interface");
  ok(memory_boot->get_interface != NULL, "memory_boot->get_interface");
  ok(error_boot->get_interface != NULL, "error_boot->get_interface");
  ok(data_lock_boot->get_interface != NULL, "data_lock_boot->get_interface");

  psi = thread_boot->get_interface(0);
  ok(psi == NULL, "no thread version 0");
  psi = thread_boot->get_interface(PSI_THREAD_VERSION_2);
  ok(psi != NULL, "thread version 1");

  psi = mutex_boot->get_interface(0);
  ok(psi == NULL, "no mutex version 0");
  psi = mutex_boot->get_interface(PSI_MUTEX_VERSION_1);
  ok(psi != NULL, "mutex version 1");

  psi = rwlock_boot->get_interface(0);
  ok(psi == NULL, "no rwlock version 0");
  psi = rwlock_boot->get_interface(PSI_RWLOCK_VERSION_1);
  ok(psi != NULL, "rwlock version 1");

  psi = cond_boot->get_interface(0);
  ok(psi == NULL, "no cond version 0");
  psi = cond_boot->get_interface(PSI_COND_VERSION_1);
  ok(psi != NULL, "cond version 1");

  psi = file_boot->get_interface(0);
  ok(psi == NULL, "no file version 0");
  psi = file_boot->get_interface(PSI_FILE_VERSION_1);
  ok(psi != NULL, "file version 1");

  psi = socket_boot->get_interface(0);
  ok(psi == NULL, "no socket version 0");
  psi = socket_boot->get_interface(PSI_SOCKET_VERSION_1);
  ok(psi != NULL, "socket version 1");

  psi = table_boot->get_interface(0);
  ok(psi == NULL, "no table version 0");
  psi = table_boot->get_interface(PSI_TABLE_VERSION_1);
  ok(psi != NULL, "table version 1");

  psi = mdl_boot->get_interface(0);
  ok(psi == NULL, "no mdl version 0");
  psi = mdl_boot->get_interface(PSI_MDL_VERSION_1);
  ok(psi != NULL, "mdl version 1");

  psi = idle_boot->get_interface(0);
  ok(psi == NULL, "no idle version 0");
  psi = idle_boot->get_interface(PSI_IDLE_VERSION_1);
  ok(psi != NULL, "idle version 1");

  psi = stage_boot->get_interface(0);
  ok(psi == NULL, "no stage version 0");
  psi = stage_boot->get_interface(PSI_STAGE_VERSION_1);
  ok(psi != NULL, "stage version 1");

  psi = statement_boot->get_interface(0);
  ok(psi == NULL, "no statement version 0");
  psi = statement_boot->get_interface(PSI_STATEMENT_VERSION_1);
  ok(psi == NULL, "no statement version 1");
  psi = statement_boot->get_interface(PSI_STATEMENT_VERSION_2);
  ok(psi != NULL, "statement version 2");

  psi = transaction_boot->get_interface(0);
  ok(psi == NULL, "no transaction version 0");
  psi = transaction_boot->get_interface(PSI_TRANSACTION_VERSION_1);
  ok(psi != NULL, "transaction version 1");

  psi = memory_boot->get_interface(0);
  ok(psi == NULL, "no memory version 0");
  psi = memory_boot->get_interface(PSI_MEMORY_VERSION_1);
  ok(psi != NULL, "memory version 1");

  psi = error_boot->get_interface(0);
  ok(psi == NULL, "no error version 0");
  psi = error_boot->get_interface(PSI_ERROR_VERSION_1);
  ok(psi != NULL, "error version 1");

  psi = data_lock_boot->get_interface(0);
  ok(psi == NULL, "no data_lock version 0");
  psi = data_lock_boot->get_interface(PSI_DATA_LOCK_VERSION_1);
  ok(psi != NULL, "data_lock version 1");
  psi_2 = data_lock_boot->get_interface(PSI_DATA_LOCK_VERSION_2);
  ok(psi_2 == NULL, "data_lock version 2");

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
    PSI_system_service_t **system_service) {
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
      &data_lock_boot, &system_boot);
  *thread_service =
      (PSI_thread_service_t *)thread_boot->get_interface(PSI_THREAD_VERSION_2);
  *mutex_service =
      (PSI_mutex_service_t *)mutex_boot->get_interface(PSI_MUTEX_VERSION_1);
  *rwlock_service =
      (PSI_rwlock_service_t *)rwlock_boot->get_interface(PSI_RWLOCK_VERSION_1);
  *cond_service =
      (PSI_cond_service_t *)cond_boot->get_interface(PSI_COND_VERSION_1);
  *file_service =
      (PSI_file_service_t *)file_boot->get_interface(PSI_FILE_VERSION_1);
  *socket_service =
      (PSI_socket_service_t *)socket_boot->get_interface(PSI_SOCKET_VERSION_1);
  *table_service =
      (PSI_table_service_t *)table_boot->get_interface(PSI_TABLE_VERSION_1);
  *mdl_service =
      (PSI_mdl_service_t *)mdl_boot->get_interface(PSI_MDL_VERSION_1);
  *idle_service =
      (PSI_idle_service_t *)idle_boot->get_interface(PSI_IDLE_VERSION_1);
  *stage_service =
      (PSI_stage_service_t *)stage_boot->get_interface(PSI_SOCKET_VERSION_1);
  *statement_service = (PSI_statement_service_t *)statement_boot->get_interface(
      PSI_STATEMENT_VERSION_2);
  *system_service =
      (PSI_system_service_t *)system_boot->get_interface(PSI_SYSTEM_VERSION_1);
  *transaction_service =
      (PSI_transaction_service_t *)transaction_boot->get_interface(
          PSI_TRANSACTION_VERSION_1);
  *memory_service =
      (PSI_memory_service_t *)memory_boot->get_interface(PSI_MEMORY_VERSION_1);
  *error_service =
      (PSI_error_service_t *)error_boot->get_interface(PSI_ERROR_VERSION_1);
  *data_lock_service = (PSI_data_lock_service_t *)data_lock_boot->get_interface(
      PSI_DATA_LOCK_VERSION_1);

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

  diag("test_bad_registration");

  load_perfschema(&thread_service, &mutex_service, &rwlock_service,
                  &cond_service, &file_service, &socket_service, &table_service,
                  &mdl_service, &idle_service, &stage_service,
                  &statement_service, &transaction_service, &memory_service,
                  &error_service, &data_lock_service, &system_service);

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
  PSI_thread_info bad_thread_1[] = {{&dummy_thread_key, "X", 0, 0, ""}};

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
       0, 0, ""}};

  thread_service->register_thread("X", bad_thread_2, 1);
  ok(dummy_thread_key == 0, "zero key");

  dummy_thread_key = 9999;
  PSI_thread_info bad_thread_3[] = {
      {&dummy_thread_key,
       /* 119 chars name */
       "12345678901234567890123456789012345678901234567890"
       "12345678901234567890123456789012345678901234567890"
       "1234567890123456789",
       0, 0, ""}};

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

  diag("test_init_disabled");

  load_perfschema(&thread_service, &mutex_service, &rwlock_service,
                  &cond_service, &file_service, &socket_service, &table_service,
                  &mdl_service, &idle_service, &stage_service,
                  &statement_service, &transaction_service, &memory_service,
                  &error_service, &data_lock_service, &system_service);

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
  PSI_thread_info all_thread[] = {{&thread_key_1, "T-1", 0, 0, ""}};

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

  thread_1 = thread_service->new_thread(thread_key_1, NULL, 0);
  ok(thread_1 != NULL, "T-1");
  thread_service->set_thread_id(thread_1, 1);

  mutex_class_A = find_mutex_class(mutex_key_A);
  ok(mutex_class_A != NULL, "mutex class A");

  rwlock_class_A = find_rwlock_class(rwlock_key_A);
  ok(rwlock_class_A != NULL, "rwlock class A");

  cond_class_A = find_cond_class(cond_key_A);
  ok(cond_class_A != NULL, "cond class A");

  file_class_A = find_file_class(file_key_A);
  ok(file_class_A != NULL, "file class A");

  socket_class_A = find_socket_class(socket_key_A);
  ok(socket_class_A != NULL, "socket class A");

  /*
    Pretend thread T-1 is running, and disabled, with thread_instrumentation.
    Disabled instruments are still created so they can be enabled later.
  */

  /* ------------------------------------------------------------------------ */

  thread_service->set_thread(thread_1);
  setup_thread(thread_1, false);

  /* disabled M-A + disabled T-1: instrumentation */

  mutex_class_A->m_enabled = false;
  mutex_A1 = mutex_service->init_mutex(mutex_key_A, NULL);
  ok(mutex_A1 != NULL, "mutex_A1 disabled, instrumented");

  /* enabled M-A + disabled T-1: instrumentation (for later) */

  mutex_class_A->m_enabled = true;
  mutex_A1 = mutex_service->init_mutex(mutex_key_A, NULL);
  ok(mutex_A1 != NULL, "mutex_A1 enabled, instrumented");

  /* broken key + disabled T-1: no instrumentation */

  mutex_class_A->m_enabled = true;
  mutex_A1 = mutex_service->init_mutex(0, NULL);
  ok(mutex_A1 == NULL, "mutex key 0 not instrumented");
  mutex_A1 = mutex_service->init_mutex(99, NULL);
  ok(mutex_A1 == NULL, "broken mutex key not instrumented");

  /* disabled RW-A + disabled T-1: no instrumentation */

  rwlock_class_A->m_enabled = false;
  rwlock_A1 = rwlock_service->init_rwlock(rwlock_key_A, NULL);
  ok(rwlock_A1 != NULL, "rwlock_A1 disabled, instrumented");

  /* enabled RW-A + disabled T-1: instrumentation (for later) */

  rwlock_class_A->m_enabled = true;
  rwlock_A1 = rwlock_service->init_rwlock(rwlock_key_A, NULL);
  ok(rwlock_A1 != NULL, "rwlock_A1 enabled, instrumented");

  /* broken key + disabled T-1: no instrumentation */

  rwlock_class_A->m_enabled = true;
  rwlock_A1 = rwlock_service->init_rwlock(0, NULL);
  ok(rwlock_A1 == NULL, "rwlock key 0 not instrumented");
  rwlock_A1 = rwlock_service->init_rwlock(99, NULL);
  ok(rwlock_A1 == NULL, "broken rwlock key not instrumented");

  /* disabled C-A + disabled T-1: no instrumentation */

  cond_class_A->m_enabled = false;
  cond_A1 = cond_service->init_cond(cond_key_A, NULL);
  ok(cond_A1 != NULL, "cond_A1 disabled, instrumented");

  /* enabled C-A + disabled T-1: instrumentation (for later) */

  cond_class_A->m_enabled = true;
  cond_A1 = cond_service->init_cond(cond_key_A, NULL);
  ok(cond_A1 != NULL, "cond_A1 enabled, instrumented");

  /* broken key + disabled T-1: no instrumentation */

  cond_class_A->m_enabled = true;
  cond_A1 = cond_service->init_cond(0, NULL);
  ok(cond_A1 == NULL, "cond key 0 not instrumented");
  cond_A1 = cond_service->init_cond(99, NULL);
  ok(cond_A1 == NULL, "broken cond key not instrumented");

  /* disabled F-A + disabled T-1: no instrumentation */

  file_class_A->m_enabled = false;
  file_service->create_file(file_key_A, "foo", (File)12);
  file_A1 = lookup_file_by_name("foo");
  ok(file_A1 == NULL, "file_A1 disabled, not instrumented");

  /* enabled F-A + disabled T-1: no instrumentation */

  file_class_A->m_enabled = true;
  file_service->create_file(file_key_A, "foo", (File)12);
  file_A1 = lookup_file_by_name("foo");
  ok(file_A1 == NULL, "file_A1 enabled, not instrumented");

  /* broken key + disabled T-1: no instrumentation */

  file_class_A->m_enabled = true;
  file_service->create_file(0, "foo", (File)12);
  file_A1 = lookup_file_by_name("foo");
  ok(file_A1 == NULL, "file_A1 not instrumented");
  file_service->create_file(99, "foo", (File)12);
  file_A1 = lookup_file_by_name("foo");
  ok(file_A1 == NULL, "file_A1 not instrumented");

  /* disabled S-A + disabled T-1: no instrumentation */

  socket_class_A->m_enabled = false;
  socket_A1 = socket_service->init_socket(socket_key_A, NULL, NULL, 0);
  ok(socket_A1 != NULL, "socket_A1 disabled, instrumented");

  /* enabled S-A + disabled T-1: instrumentation (for later) */

  socket_class_A->m_enabled = true;
  socket_A1 = socket_service->init_socket(socket_key_A, NULL, NULL, 0);
  ok(socket_A1 != NULL, "socket_A1 enabled, instrumented");

  /* broken key + disabled T-1: no instrumentation */

  socket_class_A->m_enabled = true;
  socket_A1 = socket_service->init_socket(0, NULL, NULL, 0);
  ok(socket_A1 == NULL, "socket key 0 not instrumented");
  socket_A1 = socket_service->init_socket(99, NULL, NULL, 0);
  ok(socket_A1 == NULL, "broken socket key not instrumented");

  /* Pretend thread T-1 is enabled */
  /* ----------------------------- */

  setup_thread(thread_1, true);

  /* disabled M-A + enabled T-1: no instrumentation */

  mutex_class_A->m_enabled = false;
  mutex_A1 = mutex_service->init_mutex(mutex_key_A, NULL);
  ok(mutex_A1 != NULL, "mutex_A1 disabled, instrumented");

  /* enabled M-A + enabled T-1: instrumentation */

  mutex_class_A->m_enabled = true;
  mutex_A1 = mutex_service->init_mutex(mutex_key_A, NULL);
  ok(mutex_A1 != NULL, "mutex_A1 enabled, instrumented");
  mutex_service->destroy_mutex(mutex_A1);

  /* broken key + enabled T-1: no instrumentation */

  mutex_class_A->m_enabled = true;
  mutex_A1 = mutex_service->init_mutex(0, NULL);
  ok(mutex_A1 == NULL, "mutex_A1 not instrumented");
  mutex_A1 = mutex_service->init_mutex(99, NULL);
  ok(mutex_A1 == NULL, "mutex_A1 not instrumented");

  /* disabled RW-A + enabled T-1: no instrumentation */

  rwlock_class_A->m_enabled = false;
  rwlock_A1 = rwlock_service->init_rwlock(rwlock_key_A, NULL);
  ok(rwlock_A1 != NULL, "rwlock_A1 disabled, instrumented");

  /* enabled RW-A + enabled T-1: instrumentation */

  rwlock_class_A->m_enabled = true;
  rwlock_A1 = rwlock_service->init_rwlock(rwlock_key_A, NULL);
  ok(rwlock_A1 != NULL, "rwlock_A1 enabled, instrumented");
  rwlock_service->destroy_rwlock(rwlock_A1);

  /* broken key + enabled T-1: no instrumentation */

  rwlock_class_A->m_enabled = true;
  rwlock_A1 = rwlock_service->init_rwlock(0, NULL);
  ok(rwlock_A1 == NULL, "rwlock_A1 not instrumented");
  rwlock_A1 = rwlock_service->init_rwlock(99, NULL);
  ok(rwlock_A1 == NULL, "rwlock_A1 not instrumented");

  /* disabled C-A + enabled T-1: no instrumentation */

  cond_class_A->m_enabled = false;
  cond_A1 = cond_service->init_cond(cond_key_A, NULL);
  ok(cond_A1 != NULL, "cond_A1 disabled, instrumented");

  /* enabled C-A + enabled T-1: instrumentation */

  cond_class_A->m_enabled = true;
  cond_A1 = cond_service->init_cond(cond_key_A, NULL);
  ok(cond_A1 != NULL, "cond_A1 enabled, instrumented");
  cond_service->destroy_cond(cond_A1);

  /* broken key + enabled T-1: no instrumentation */

  cond_class_A->m_enabled = true;
  cond_A1 = cond_service->init_cond(0, NULL);
  ok(cond_A1 == NULL, "cond_A1 not instrumented");
  cond_A1 = cond_service->init_cond(99, NULL);
  ok(cond_A1 == NULL, "cond_A1 not instrumented");

  /* disabled F-A + enabled T-1: no instrumentation */

  file_class_A->m_enabled = false;
  file_service->create_file(file_key_A, "foo", (File)12);
  file_A1 = lookup_file_by_name("foo");
  ok(file_A1 == NULL, "file_A1 not instrumented");

  /* enabled F-A + open failed + enabled T-1: no instrumentation */

  file_class_A->m_enabled = true;
  file_service->create_file(file_key_A, "foo", (File)-1);
  file_A1 = lookup_file_by_name("foo");
  ok(file_A1 == NULL, "file_A1 not instrumented");

  /* enabled F-A + out-of-descriptors + enabled T-1: no instrumentation */

  file_class_A->m_enabled = true;
  file_service->create_file(file_key_A, "foo", (File)65000);
  file_A1 = lookup_file_by_name("foo");
  ok(file_A1 == NULL, "file_A1 not instrumented");
  ok(file_handle_lost == 1, "lost a file handle");
  file_handle_lost = 0;

  /* enabled F-A + enabled T-1: instrumentation */

  file_class_A->m_enabled = true;
  file_service->create_file(file_key_A, "foo-instrumented", (File)12);
  file_A1 = lookup_file_by_name("foo-instrumented");
  ok(file_A1 != NULL, "file_A1 instrumented");

  /* broken key + enabled T-1: no instrumentation */

  file_class_A->m_enabled = true;
  file_service->create_file(0, "foo", (File)12);
  file_A1 = lookup_file_by_name("foo");
  ok(file_A1 == NULL, "file key 0 not instrumented");
  file_service->create_file(99, "foo", (File)12);
  file_A1 = lookup_file_by_name("foo");
  ok(file_A1 == NULL, "broken file key not instrumented");

  /* disabled S-A + enabled T-1: no instrumentation */

  socket_class_A->m_enabled = false;
  ok(socket_A1 == NULL, "socket_A1 not instrumented");

  /* enabled S-A + enabled T-1: instrumentation */

  socket_class_A->m_enabled = true;
  socket_A1 = socket_service->init_socket(socket_key_A, NULL, NULL, 0);
  ok(socket_A1 != NULL, "socket_A1 instrumented");
  socket_service->destroy_socket(socket_A1);

  /* broken key + enabled T-1: no instrumentation */

  socket_class_A->m_enabled = true;
  socket_A1 = socket_service->init_socket(0, NULL, NULL, 0);
  ok(socket_A1 == NULL, "socket_A1 not instrumented");
  socket_A1 = socket_service->init_socket(99, NULL, NULL, 0);
  ok(socket_A1 == NULL, "socket_A1 not instrumented");

  /* Pretend the running thread is not instrumented */
  /* ---------------------------------------------- */

  thread_service->delete_current_thread();

  /* disabled M-A + unknown thread: no instrumentation */

  mutex_class_A->m_enabled = false;
  mutex_A1 = mutex_service->init_mutex(mutex_key_A, NULL);
  ok(mutex_A1 != NULL, "mutex_A1 disabled, instrumented");

  /* enabled M-A + unknown thread: instrumentation (for later) */

  mutex_class_A->m_enabled = true;
  mutex_A1 = mutex_service->init_mutex(mutex_key_A, NULL);
  ok(mutex_A1 != NULL, "mutex_A1 enabled, instrumented");

  /* broken key + unknown thread: no instrumentation */

  mutex_class_A->m_enabled = true;
  mutex_A1 = mutex_service->init_mutex(0, NULL);
  ok(mutex_A1 == NULL, "mutex key 0 not instrumented");
  mutex_A1 = mutex_service->init_mutex(99, NULL);
  ok(mutex_A1 == NULL, "broken mutex key not instrumented");

  /* disabled RW-A + unknown thread: no instrumentation */

  rwlock_class_A->m_enabled = false;
  rwlock_A1 = rwlock_service->init_rwlock(rwlock_key_A, NULL);
  ok(rwlock_A1 != NULL, "rwlock_A1 disabled, instrumented");

  /* enabled RW-A + unknown thread: instrumentation (for later) */

  rwlock_class_A->m_enabled = true;
  rwlock_A1 = rwlock_service->init_rwlock(rwlock_key_A, NULL);
  ok(rwlock_A1 != NULL, "rwlock_A1 enabled, instrumented");

  /* broken key + unknown thread: no instrumentation */

  rwlock_class_A->m_enabled = true;
  rwlock_A1 = rwlock_service->init_rwlock(0, NULL);
  ok(rwlock_A1 == NULL, "rwlock key 0 not instrumented");
  rwlock_A1 = rwlock_service->init_rwlock(99, NULL);
  ok(rwlock_A1 == NULL, "broken rwlock key not instrumented");

  /* disabled C-A + unknown thread: no instrumentation */

  cond_class_A->m_enabled = false;
  cond_A1 = cond_service->init_cond(cond_key_A, NULL);
  ok(cond_A1 != NULL, "cond_A1 disabled, instrumented");

  /* enabled C-A + unknown thread: instrumentation (for later) */

  cond_class_A->m_enabled = true;
  cond_A1 = cond_service->init_cond(cond_key_A, NULL);
  ok(cond_A1 != NULL, "cond_A1 enabled, instrumented");

  /* broken key + unknown thread: no instrumentation */

  cond_class_A->m_enabled = true;
  cond_A1 = cond_service->init_cond(0, NULL);
  ok(cond_A1 == NULL, "cond key 0 not instrumented");
  cond_A1 = cond_service->init_cond(99, NULL);
  ok(cond_A1 == NULL, "broken cond key not instrumented");

  /* disabled F-A + unknown thread: no instrumentation */

  file_class_A->m_enabled = false;
  file_service->create_file(file_key_A, "foo", (File)12);
  file_A1 = lookup_file_by_name("foo");
  ok(file_A1 == NULL, "file_A1 not instrumented");

  /* enabled F-A + unknown thread: no instrumentation */

  file_class_A->m_enabled = true;
  file_service->create_file(file_key_A, "foo", (File)12);
  file_A1 = lookup_file_by_name("foo");
  ok(file_A1 == NULL, "file_A1 not instrumented");

  /* broken key + unknown thread: no instrumentation */

  file_class_A->m_enabled = true;
  file_service->create_file(0, "foo", (File)12);
  file_A1 = lookup_file_by_name("foo");
  ok(file_A1 == NULL, "not instrumented");
  file_service->create_file(99, "foo", (File)12);
  file_A1 = lookup_file_by_name("foo");
  ok(file_A1 == NULL, "not instrumented");

  /* disabled S-A + unknown thread: no instrumentation */

  socket_class_A->m_enabled = false;
  socket_A1 = socket_service->init_socket(socket_key_A, NULL, NULL, 0);
  ok(socket_A1 != NULL, "socket_A1 disabled, instrumented");

  /* enabled S-A + unknown thread: instrumentation (for later) */

  socket_class_A->m_enabled = true;
  socket_A1 = socket_service->init_socket(socket_key_A, NULL, NULL, 0);
  ok(socket_A1 != NULL, "socket_A1 enabled, instrumented");

  /* broken key + unknown thread: no instrumentation */

  socket_class_A->m_enabled = true;
  socket_A1 = socket_service->init_socket(0, NULL, NULL, 0);
  ok(socket_A1 == NULL, "socket key 0 not instrumented");
  socket_A1 = socket_service->init_socket(99, NULL, NULL, 0);
  ok(socket_A1 == NULL, "broken socket key not instrumented");

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

  diag("test_locker_disabled");

  load_perfschema(&thread_service, &mutex_service, &rwlock_service,
                  &cond_service, &file_service, &socket_service, &table_service,
                  &mdl_service, &idle_service, &stage_service,
                  &statement_service, &transaction_service, &memory_service,
                  &error_service, &data_lock_service, &system_service);

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
  PSI_thread_info all_thread[] = {{&thread_key_1, "T-1", 0, 0, ""}};

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

  thread_1 = thread_service->new_thread(thread_key_1, NULL, 0);
  ok(thread_1 != NULL, "T-1");
  thread_service->set_thread_id(thread_1, 1);

  mutex_class_A = find_mutex_class(mutex_key_A);
  ok(mutex_class_A != NULL, "mutex info A");

  rwlock_class_A = find_rwlock_class(rwlock_key_A);
  ok(rwlock_class_A != NULL, "rwlock info A");

  cond_class_A = find_cond_class(cond_key_A);
  ok(cond_class_A != NULL, "cond info A");

  file_class_A = find_file_class(file_key_A);
  ok(file_class_A != NULL, "file info A");

  socket_class_A = find_socket_class(socket_key_A);
  ok(socket_class_A != NULL, "socket info A");

  /* Pretend thread T-1 is running, and enabled */
  /* ------------------------------------------ */

  thread_service->set_thread(thread_1);
  setup_thread(thread_1, true);

  /* Enable all instruments, instantiate objects */

  mutex_class_A->m_enabled = true;
  mutex_A1 = mutex_service->init_mutex(mutex_key_A, NULL);
  ok(mutex_A1 != NULL, "instrumented");

  rwlock_class_A->m_enabled = true;
  rwlock_A1 = rwlock_service->init_rwlock(rwlock_key_A, NULL);
  ok(rwlock_A1 != NULL, "instrumented");

  cond_class_A->m_enabled = true;
  cond_A1 = cond_service->init_cond(cond_key_A, NULL);
  ok(cond_A1 != NULL, "instrumented");

  file_class_A->m_enabled = true;
  file_service->create_file(file_key_A, "foo", (File)12);
  file_A1 = (PSI_file *)lookup_file_by_name("foo");
  ok(file_A1 != NULL, "instrumented");

  socket_class_A->m_enabled = true;
  socket_A1 = socket_service->init_socket(socket_key_A, NULL, NULL, 0);
  ok(socket_A1 != NULL, "instrumented");

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
  ok(mutex_locker == NULL, "no locker (T-1 disabled)");
  rwlock_locker = rwlock_service->start_rwlock_rdwait(
      &rwlock_state, rwlock_A1, PSI_RWLOCK_READLOCK, "foo.cc", 12);
  ok(rwlock_locker == NULL, "no locker (T-1 disabled)");
  cond_locker = cond_service->start_cond_wait(&cond_state, cond_A1, mutex_A1,
                                              PSI_COND_WAIT, "foo.cc", 12);
  ok(cond_locker == NULL, "no locker (T-1 disabled)");
  file_locker = file_service->get_thread_file_name_locker(
      &file_state, file_key_A, PSI_FILE_OPEN, "xxx", NULL);
  ok(file_locker == NULL, "no locker (T-1 disabled)");
  file_locker = file_service->get_thread_file_stream_locker(
      &file_state, file_A1, PSI_FILE_READ);
  ok(file_locker == NULL, "no locker (T-1 disabled)");
  file_locker = file_service->get_thread_file_descriptor_locker(
      &file_state, (File)12, PSI_FILE_READ);
  ok(file_locker == NULL, "no locker (T-1 disabled)");
  socket_locker = socket_service->start_socket_wait(
      &socket_state, socket_A1, PSI_SOCKET_SEND, 12, "foo.cc", 12);
  ok(socket_locker == NULL, "no locker (T-1 disabled)");

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

  mutex_locker = mutex_service->start_mutex_wait(&mutex_state, mutex_A1,
                                                 PSI_MUTEX_LOCK, "foo.cc", 12);
  ok(mutex_locker == NULL, "no locker (global disabled)");
  rwlock_locker = rwlock_service->start_rwlock_rdwait(
      &rwlock_state, rwlock_A1, PSI_RWLOCK_READLOCK, "foo.cc", 12);
  ok(rwlock_locker == NULL, "no locker (global disabled)");
  cond_locker = cond_service->start_cond_wait(&cond_state, cond_A1, mutex_A1,
                                              PSI_COND_WAIT, "foo.cc", 12);
  ok(cond_locker == NULL, "no locker (global disabled)");
  file_locker = file_service->get_thread_file_name_locker(
      &file_state, file_key_A, PSI_FILE_OPEN, "xxx", NULL);
  ok(file_locker == NULL, "no locker (global disabled)");
  file_locker = file_service->get_thread_file_stream_locker(
      &file_state, file_A1, PSI_FILE_READ);
  ok(file_locker == NULL, "no locker (global disabled)");
  file_locker = file_service->get_thread_file_descriptor_locker(
      &file_state, (File)12, PSI_FILE_READ);
  ok(file_locker == NULL, "no locker (global disabled)");
  socket_locker = socket_service->start_socket_wait(
      &socket_state, socket_A1, PSI_SOCKET_SEND, 12, "foo.cc", 12);
  ok(socket_locker == NULL, "no locker (global disabled)");

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
  ok(mutex_locker == NULL, "no locker (global counted)");
  rwlock_locker = rwlock_service->start_rwlock_rdwait(
      &rwlock_state, rwlock_A1, PSI_RWLOCK_READLOCK, "foo.cc", 12);
  ok(rwlock_locker == NULL, "no locker (global counted)");
  cond_locker = cond_service->start_cond_wait(&cond_state, cond_A1, mutex_A1,
                                              PSI_COND_WAIT, "foo.cc", 12);
  ok(cond_locker == NULL, "no locker (global counted)");
  file_locker = file_service->get_thread_file_name_locker(
      &file_state, file_key_A, PSI_FILE_OPEN, "xxx", NULL);
  ok(file_locker != NULL, "locker (global counted)");
  file_service->start_file_wait(file_locker, 10, __FILE__, __LINE__);
  file_service->end_file_wait(file_locker, 10);
  file_locker = file_service->get_thread_file_stream_locker(
      &file_state, file_A1, PSI_FILE_READ);
  ok(file_locker != NULL, "locker (global counted)");
  file_service->start_file_wait(file_locker, 10, __FILE__, __LINE__);
  file_service->end_file_wait(file_locker, 10);
  file_locker = file_service->get_thread_file_descriptor_locker(
      &file_state, (File)12, PSI_FILE_READ);
  ok(file_locker != NULL, "locker (global counted)");
  file_service->start_file_wait(file_locker, 10, __FILE__, __LINE__);
  file_service->end_file_wait(file_locker, 10);
  /* The null locker shortcut applies only to socket ops with no byte count */
  socket_locker = socket_service->start_socket_wait(
      &socket_state, socket_A1, PSI_SOCKET_BIND, 0, "foo.cc", 12);
  ok(socket_locker == NULL, "no locker (global counted)");

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

  mutex_locker = mutex_service->start_mutex_wait(&mutex_state, mutex_A1,
                                                 PSI_MUTEX_LOCK, "foo.cc", 12);
  ok(mutex_locker == NULL, "no locker");
  rwlock_locker = rwlock_service->start_rwlock_rdwait(
      &rwlock_state, rwlock_A1, PSI_RWLOCK_READLOCK, "foo.cc", 12);
  ok(rwlock_locker == NULL, "no locker");
  cond_locker = cond_service->start_cond_wait(&cond_state, cond_A1, mutex_A1,
                                              PSI_COND_WAIT, "foo.cc", 12);
  ok(cond_locker == NULL, "no locker");
  file_locker = file_service->get_thread_file_name_locker(
      &file_state, file_key_A, PSI_FILE_OPEN, "xxx", NULL);
  ok(file_locker == NULL, "no locker");
  file_locker = file_service->get_thread_file_stream_locker(
      &file_state, file_A1, PSI_FILE_READ);
  ok(file_locker == NULL, "no locker");
  file_locker = file_service->get_thread_file_descriptor_locker(
      &file_state, (File)12, PSI_FILE_READ);
  ok(file_locker == NULL, "no locker");
  socket_locker = socket_service->start_socket_wait(
      &socket_state, socket_A1, PSI_SOCKET_SEND, 12, "foo.cc", 12);
  ok(socket_locker == NULL, "no locker");

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
  ok(mutex_locker != NULL, "locker");
  mutex_service->end_mutex_wait(mutex_locker, 0);
  rwlock_locker = rwlock_service->start_rwlock_rdwait(
      &rwlock_state, rwlock_A1, PSI_RWLOCK_READLOCK, __FILE__, __LINE__);
  ok(rwlock_locker != NULL, "locker");
  rwlock_service->end_rwlock_rdwait(rwlock_locker, 0);
  cond_locker = cond_service->start_cond_wait(
      &cond_state, cond_A1, mutex_A1, PSI_COND_WAIT, __FILE__, __LINE__);
  ok(cond_locker != NULL, "locker");
  cond_service->end_cond_wait(cond_locker, 0);
  file_locker = file_service->get_thread_file_name_locker(
      &file_state, file_key_A, PSI_FILE_STREAM_OPEN, "xxx", NULL);
  ok(file_locker != NULL, "locker");
  file_service->start_file_open_wait(file_locker, __FILE__, __LINE__);
  file_service->end_file_open_wait(file_locker, NULL);
  file_locker = file_service->get_thread_file_stream_locker(
      &file_state, file_A1, PSI_FILE_READ);
  ok(file_locker != NULL, "locker");
  file_service->start_file_wait(file_locker, 10, __FILE__, __LINE__);
  file_service->end_file_wait(file_locker, 10);
  file_locker = file_service->get_thread_file_descriptor_locker(
      &file_state, (File)12, PSI_FILE_READ);
  ok(file_locker != NULL, "locker");
  file_service->start_file_wait(file_locker, 10, __FILE__, __LINE__);
  file_service->end_file_wait(file_locker, 10);
  socket_locker = socket_service->start_socket_wait(
      &socket_state, socket_A1, PSI_SOCKET_SEND, 12, "foo.cc", 12);
  ok(socket_locker != NULL, "locker");
  socket_service->end_socket_wait(socket_locker, 10);

  /* Pretend the socket does not have a thread owner */
  /* ---------------------------------------------- */

  socket_class_A->m_enabled = true;
  socket_A1 = socket_service->init_socket(socket_key_A, NULL, NULL, 0);
  ok(socket_A1 != NULL, "instrumented");
  /* Socket thread owner has not been set */
  socket_locker = socket_service->start_socket_wait(
      &socket_state, socket_A1, PSI_SOCKET_SEND, 12, "foo.cc", 12);
  ok(socket_locker != NULL, "locker (owner not used)");
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
  ok(mutex_locker == NULL, "no locker");
  rwlock_locker = rwlock_service->start_rwlock_rdwait(
      &rwlock_state, rwlock_A1, PSI_RWLOCK_READLOCK, "foo.cc", 12);
  ok(rwlock_locker == NULL, "no locker");
  cond_locker = cond_service->start_cond_wait(&cond_state, cond_A1, mutex_A1,
                                              PSI_COND_WAIT, "foo.cc", 12);
  ok(cond_locker == NULL, "no locker");
  file_locker = file_service->get_thread_file_name_locker(
      &file_state, file_key_A, PSI_FILE_OPEN, "xxx", NULL);
  ok(file_locker == NULL, "no locker");
  file_locker = file_service->get_thread_file_stream_locker(
      &file_state, file_A1, PSI_FILE_READ);
  ok(file_locker == NULL, "no locker");
  file_locker = file_service->get_thread_file_descriptor_locker(
      &file_state, (File)12, PSI_FILE_READ);
  ok(file_locker == NULL, "no locker");
  socket_locker = socket_service->start_socket_wait(
      &socket_state, socket_A1, PSI_SOCKET_SEND, 12, "foo.cc", 12);
  ok(socket_locker == NULL, "no locker");

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

  diag("test_file_instrumentation_leak");

  load_perfschema(&thread_service, &mutex_service, &rwlock_service,
                  &cond_service, &file_service, &socket_service, &table_service,
                  &mdl_service, &idle_service, &stage_service,
                  &statement_service, &transaction_service, &memory_service,
                  &error_service, &data_lock_service, &system_service);

  PSI_file_key file_key_A;
  PSI_file_key file_key_B;
  PSI_file_info all_file[] = {{&file_key_A, "F-A", 0, 0, ""},
                              {&file_key_B, "F-B", 0, 0, ""}};

  PSI_thread_key thread_key_1;
  PSI_thread_info all_thread[] = {{&thread_key_1, "T-1", 0, 0, ""}};

  file_service->register_file("test", all_file, 2);
  thread_service->register_thread("test", all_thread, 1);

  PFS_file_class *file_class_A;
  PFS_file_class *file_class_B;
  PSI_file_locker_state file_state;
  PSI_thread *thread_1;

  /* Preparation */

  thread_1 = thread_service->new_thread(thread_key_1, NULL, 0);
  ok(thread_1 != NULL, "T-1");
  thread_service->set_thread_id(thread_1, 1);

  file_class_A = find_file_class(file_key_A);
  ok(file_class_A != NULL, "file info A");

  file_class_B = find_file_class(file_key_B);
  ok(file_class_B != NULL, "file info B");

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
      &file_state, file_key_A, PSI_FILE_OPEN, "AAA", NULL);
  ok(file_locker != NULL, "locker");
  file_service->start_file_open_wait(file_locker, __FILE__, __LINE__);
  file_service->end_file_open_wait_and_bind_to_descriptor(file_locker, 12);

  file_locker = file_service->get_thread_file_descriptor_locker(
      &file_state, (File)12, PSI_FILE_READ);
  ok(file_locker != NULL, "locker");
  file_service->start_file_wait(file_locker, 100, __FILE__, __LINE__);
  file_service->end_file_wait(file_locker, 100);

  file_locker = file_service->get_thread_file_descriptor_locker(
      &file_state, (File)12, PSI_FILE_CLOSE);
  ok(file_locker != NULL, "locker");
  file_service->start_file_wait(file_locker, 0, __FILE__, __LINE__);
  file_service->end_file_wait(file_locker, 0);

  /* Simulate uninstrumented-OPEN + WRITE on descriptor 24 */

  file_locker = file_service->get_thread_file_descriptor_locker(
      &file_state, (File)24, PSI_FILE_WRITE);
  ok(file_locker == NULL, "no locker, since the open was not instrumented");

  /*
    Simulate uninstrumented-OPEN + WRITE on descriptor 12 :
    the instrumentation should not leak (don't charge the file io on unknown B
    to "AAA")
  */

  file_locker = file_service->get_thread_file_descriptor_locker(
      &file_state, (File)12, PSI_FILE_WRITE);
  ok(file_locker == NULL, "no locker, no leak");

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
      &data_lock_boot, &system_boot);
  ok(thread_boot != NULL, "thread_bootstrap");
  ok(mutex_boot != NULL, "mutex_bootstrap");
  ok(rwlock_boot != NULL, "rwlock_bootstrap");
  ok(cond_boot != NULL, "cond_bootstrap");
  ok(file_boot != NULL, "file_bootstrap");
  ok(socket_boot != NULL, "socket_bootstrap");
  ok(table_boot != NULL, "table_bootstrap");
  ok(mdl_boot != NULL, "mdl_bootstrap");
  ok(idle_boot != NULL, "idle_bootstrap");
  ok(stage_boot != NULL, "stage_bootstrap");
  ok(statement_boot != NULL, "statement_bootstrap");
  ok(transaction_boot != NULL, "transaction_bootstrap");
  ok(memory_boot != NULL, "memory_bootstrap");
  ok(error_boot != NULL, "error_bootstrap");
  ok(data_lock_boot != NULL, "data_lock_bootstrap");

  thread_service =
      (PSI_thread_service_t *)thread_boot->get_interface(PSI_THREAD_VERSION_2);
  ok(thread_service != NULL, "thread_service");
  mutex_service =
      (PSI_mutex_service_t *)mutex_boot->get_interface(PSI_MUTEX_VERSION_1);
  ok(mutex_service != NULL, "mutex_service");
  rwlock_service =
      (PSI_rwlock_service_t *)rwlock_boot->get_interface(PSI_RWLOCK_VERSION_1);
  ok(rwlock_service != NULL, "rwlock_service");
  cond_service =
      (PSI_cond_service_t *)cond_boot->get_interface(PSI_COND_VERSION_1);
  ok(cond_service != NULL, "cond_service");
  file_service =
      (PSI_file_service_t *)file_boot->get_interface(PSI_FILE_VERSION_1);
  ok(file_service != NULL, "file_service");
  socket_service =
      (PSI_socket_service_t *)socket_boot->get_interface(PSI_SOCKET_VERSION_1);
  ok(socket_service != NULL, "socket_service");
  table_service =
      (PSI_table_service_t *)table_boot->get_interface(PSI_TABLE_VERSION_1);
  ok(table_service != NULL, "table_service");
  mdl_service = (PSI_mdl_service_t *)mdl_boot->get_interface(PSI_MDL_VERSION_1);
  ok(mdl_service != NULL, "mdl_service");
  idle_service =
      (PSI_idle_service_t *)idle_boot->get_interface(PSI_IDLE_VERSION_1);
  ok(idle_service != NULL, "idle_service");
  stage_service =
      (PSI_stage_service_t *)stage_boot->get_interface(PSI_STAGE_VERSION_1);
  ok(stage_service != NULL, "stage_service");
  statement_service = (PSI_statement_service_t *)statement_boot->get_interface(
      PSI_STATEMENT_VERSION_2);
  ok(statement_service != NULL, "statement_service");
  transaction_service =
      (PSI_transaction_service_t *)transaction_boot->get_interface(
          PSI_TRANSACTION_VERSION_1);
  ok(transaction_service != NULL, "transaction_service");
  memory_service =
      (PSI_memory_service_t *)memory_boot->get_interface(PSI_MEMORY_VERSION_1);
  ok(memory_service != NULL, "memory_service");
  error_service =
      (PSI_error_service_t *)error_boot->get_interface(PSI_MEMORY_VERSION_1);
  ok(error_service != NULL, "error_service");
  data_lock_service = (PSI_data_lock_service_t *)data_lock_boot->get_interface(
      PSI_DATA_LOCK_VERSION_1);
  ok(data_lock_service != NULL, "data_lock_service");

  PFS_mutex_class *mutex_class;
  PSI_mutex_key dummy_mutex_key_1;
  PSI_mutex_key dummy_mutex_key_2;
  PSI_mutex_info dummy_mutexes[] = {{&dummy_mutex_key_1, "M-1", 0, 0, ""},
                                    {&dummy_mutex_key_2, "M-2", 0, 0, ""}};

  mutex_service->register_mutex("X", dummy_mutexes, 2);
  mutex_class = find_mutex_class(dummy_mutex_key_1);
  ok(mutex_class != NULL, "mutex class 1");
  ok(mutex_class->m_event_name_index == 4, "index 4");
  mutex_class = find_mutex_class(dummy_mutex_key_2);
  ok(mutex_class != NULL, "mutex class 2");
  ok(mutex_class->m_event_name_index == 5, "index 5");

  PFS_rwlock_class *rwlock_class;
  PSI_rwlock_key dummy_rwlock_key_1;
  PSI_rwlock_key dummy_rwlock_key_2;
  PSI_rwlock_info dummy_rwlocks[] = {{&dummy_rwlock_key_1, "RW-1", 0, 0, ""},
                                     {&dummy_rwlock_key_2, "RW-2", 0, 0, ""}};

  rwlock_service->register_rwlock("X", dummy_rwlocks, 2);
  rwlock_class = find_rwlock_class(dummy_rwlock_key_1);
  ok(rwlock_class != NULL, "rwlock class 1");
  ok(rwlock_class->m_event_name_index == 14, "index 14");
  rwlock_class = find_rwlock_class(dummy_rwlock_key_2);
  ok(rwlock_class != NULL, "rwlock class 2");
  ok(rwlock_class->m_event_name_index == 15, "index 15");

  PFS_cond_class *cond_class;
  PSI_cond_key dummy_cond_key_1;
  PSI_cond_key dummy_cond_key_2;
  PSI_cond_info dummy_conds[] = {{&dummy_cond_key_1, "C-1", 0, 0, ""},
                                 {&dummy_cond_key_2, "C-2", 0, 0, ""}};

  cond_service->register_cond("X", dummy_conds, 2);
  cond_class = find_cond_class(dummy_cond_key_1);
  ok(cond_class != NULL, "cond class 1");
  ok(cond_class->m_event_name_index == 34, "index 34");
  cond_class = find_cond_class(dummy_cond_key_2);
  ok(cond_class != NULL, "cond class 2");
  ok(cond_class->m_event_name_index == 35, "index 35");

  PFS_file_class *file_class;
  PSI_file_key dummy_file_key_1;
  PSI_file_key dummy_file_key_2;
  PSI_file_info dummy_files[] = {{&dummy_file_key_1, "F-1", 0, 0, ""},
                                 {&dummy_file_key_2, "F-2", 0, 0, ""}};

  file_service->register_file("X", dummy_files, 2);
  file_class = find_file_class(dummy_file_key_1);
  ok(file_class != NULL, "file class 1");
  ok(file_class->m_event_name_index == 74, "index 74");
  file_class = find_file_class(dummy_file_key_2);
  ok(file_class != NULL, "file class 2");
  ok(file_class->m_event_name_index == 75, "index 75");

  PFS_socket_class *socket_class;
  PSI_socket_key dummy_socket_key_1;
  PSI_socket_key dummy_socket_key_2;
  PSI_socket_info dummy_sockets[] = {{&dummy_socket_key_1, "S-1", 0, 0, ""},
                                     {&dummy_socket_key_2, "S-2", 0, 0, ""}};

  socket_service->register_socket("X", dummy_sockets, 2);
  socket_class = find_socket_class(dummy_socket_key_1);
  ok(socket_class != NULL, "socket class 1");
  ok(socket_class->m_event_name_index == 154, "index 154");
  socket_class = find_socket_class(dummy_socket_key_2);
  ok(socket_class != NULL, "socket class 2");
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
  PSI_thread *owner;

  diag("test_memory_instruments");

  load_perfschema(&thread_service, &mutex_service, &rwlock_service,
                  &cond_service, &file_service, &socket_service, &table_service,
                  &mdl_service, &idle_service, &stage_service,
                  &statement_service, &transaction_service, &memory_service,
                  &error_service, &data_lock_service, &system_service);

  PSI_memory_key memory_key_A;
  PSI_memory_info all_memory[] = {{&memory_key_A, "M-A", 0, 0, ""}};

  PSI_thread_key thread_key_1;
  PSI_thread_info all_thread[] = {{&thread_key_1, "T-1", 0, 0, ""}};

  memory_service->register_memory("test", all_memory, 1);
  thread_service->register_thread("test", all_thread, 1);

  PFS_memory_class *memory_class_A;
  PSI_thread *thread_1;
  PSI_memory_key key;

  /* Preparation */

  thread_1 = thread_service->new_thread(thread_key_1, NULL, 0);
  ok(thread_1 != NULL, "T-1");
  thread_service->set_thread_id(thread_1, 1);

  memory_class_A = find_memory_class(memory_key_A);
  ok(memory_class_A != NULL, "memory info A");

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
      &data_lock_boot, &system_boot);
  ok(thread_boot != NULL, "thread bootstrap");
  ok(mutex_boot != NULL, "mutex bootstrap");
  ok(rwlock_boot != NULL, "rwlock bootstrap");
  ok(cond_boot != NULL, "cond bootstrap");
  ok(file_boot != NULL, "file bootstrap");
  ok(socket_boot != NULL, "socket bootstrap");
  ok(table_boot != NULL, "table bootstrap");
  ok(mdl_boot != NULL, "mdl bootstrap");
  ok(idle_boot != NULL, "idle bootstrap");
  ok(stage_boot != NULL, "stage bootstrap");
  ok(statement_boot != NULL, "statement bootstrap");
  ok(transaction_boot != NULL, "transaction bootstrap");
  ok(memory_boot != NULL, "memory bootstrap");
  ok(error_boot != NULL, "error bootstrap");
  shutdown_performance_schema();

  /* Leaks will be reported with valgrind */
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
}

int main(int, char **) {
  plan(330);

  MY_INIT("pfs-t");
  do_all_tests();
  my_end(0);
  return (exit_status());
}
