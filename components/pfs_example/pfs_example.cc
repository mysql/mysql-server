/* Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include <mysql/components/component_implementation.h>
#include <mysql/components/services/mysql_cond.h>
#include <mysql/components/services/mysql_mutex.h>
#include <mysql/components/services/mysql_rwlock.h>
#include <mysql/components/services/psi_cond.h>
#include <mysql/components/services/psi_error.h>
#include <mysql/components/services/psi_file.h>
#include <mysql/components/services/psi_idle.h>
#include <mysql/components/services/psi_mdl.h>
#include <mysql/components/services/psi_memory.h>
#include <mysql/components/services/psi_mutex.h>
#include <mysql/components/services/psi_rwlock.h>
#include <mysql/components/services/psi_socket.h>
#include <mysql/components/services/psi_stage.h>
#include <mysql/components/services/psi_statement.h>
#include <mysql/components/services/psi_system.h>
#include <mysql/components/services/psi_table.h>
#include <mysql/components/services/psi_thread.h>
#include <mysql/components/services/psi_transaction.h>

// FIXME: need to be visible in include
#include <mysql/psi/psi_base.h>

/* Mutex */

static PSI_mutex_key key_mutex_x = 0;
static PSI_mutex_key key_mutex_y = 0;

static PSI_mutex_info all_example_mutex[] = {
    {&key_mutex_x, "X", PSI_FLAG_SINGLETON, PSI_VOLATILITY_PERMANENT,
     "Example doc, permanent mutex, singleton."},
    {&key_mutex_y, "Y", 0, PSI_VOLATILITY_QUERY,
     "Example doc, very volatile mutexes."}};

static mysql_mutex_t my_mutex_x;
static mysql_mutex_t my_mutex_y;

/* PSI mutex */

static PSI_mutex_key key_mutex_z = 0;
static PSI_mutex_key key_mutex_t = 0;

static PSI_mutex_info all_example_psi_mutex[] = {
    {&key_mutex_z, "Z", PSI_FLAG_SINGLETON, PSI_VOLATILITY_PROVISIONING,
     "Another example."},
    {&key_mutex_t, "T", 0, PSI_VOLATILITY_DDL, "And more."}};

static PSI_mutex *psi_mutex_z;
static PSI_mutex *psi_mutex_t;

/* RW-lock, PR-lock */

static PSI_rwlock_key key_rwlock_x = 0;
static PSI_rwlock_key key_rwlock_y = 0;
static PSI_rwlock_key key_prlock_z = 0;
static PSI_rwlock_key key_prlock_t = 0;

static PSI_rwlock_info all_example_rwlock[] = {{&key_rwlock_x, "X", 0, 0, ""},
                                               {&key_rwlock_y, "Y", 0, 0, ""},
                                               {&key_prlock_z, "Z", 0, 0, ""},
                                               {&key_prlock_t, "T", 0, 0, ""}};

static mysql_rwlock_t my_rwlock_x;
static mysql_rwlock_t my_rwlock_y;
static mysql_prlock_t my_prlock_z;
static mysql_prlock_t my_prlock_t;

/* PSI rwlock */

static PSI_rwlock_key key_rwlock_s1 = 0;
static PSI_rwlock_key key_rwlock_s2 = 0;

static PSI_rwlock_info all_example_psi_rwlock[] = {
    {&key_rwlock_s1, "S1", PSI_FLAG_RWLOCK_SX, 0, ""},
    {&key_rwlock_s2, "S2", PSI_FLAG_RWLOCK_SX, 0, ""}};

static PSI_rwlock *psi_rwlock_s1;
static PSI_rwlock *psi_rwlock_s2;

/* Cond */

static PSI_cond_key key_cond_x = 0;
static PSI_cond_key key_cond_y = 0;

static PSI_cond_info all_example_cond[] = {{&key_cond_x, "X", 0, 0, ""},
                                           {&key_cond_y, "Y", 0, 0, ""}};

static mysql_cond_t my_cond_x;
static mysql_cond_t my_cond_y;

static void do_something_part_1();
static void do_something_part_2();

static mysql_service_status_t pfs_example_init() {
  mysql_mutex_register("pfs_example", all_example_mutex, 2);
  mysql_rwlock_register("pfs_example", all_example_rwlock, 4);
  mysql_cond_register("pfs_example", all_example_cond, 2);

  PSI_MUTEX_CALL(register_mutex)("pfs_example", all_example_psi_mutex, 2);
  PSI_RWLOCK_CALL(register_rwlock)("pfs_example", all_example_psi_rwlock, 2);

  do_something_part_1();

  return 0;
}

static mysql_service_status_t pfs_example_deinit() {
  do_something_part_2();

  return 0;
}

static void test_mysql_mutex_part_1() {
  mysql_mutex_init(key_mutex_x, &my_mutex_x, NULL);
  mysql_mutex_init(key_mutex_y, &my_mutex_y, NULL);

  mysql_mutex_lock(&my_mutex_x);
  mysql_mutex_trylock(&my_mutex_y);

  mysql_mutex_unlock(&my_mutex_y);
  mysql_mutex_unlock(&my_mutex_x);
}

static void test_mysql_mutex_part_2() {
  mysql_mutex_destroy(&my_mutex_x);
  mysql_mutex_destroy(&my_mutex_y);
}

static void test_psi_mutex_part_1() {
  psi_mutex_z = PSI_MUTEX_CALL(init_mutex)(key_mutex_z, NULL);
  psi_mutex_t = PSI_MUTEX_CALL(init_mutex)(key_mutex_t, NULL);
}

static void test_psi_mutex_part_2() {
  PSI_MUTEX_CALL(destroy_mutex)(psi_mutex_z);
  PSI_MUTEX_CALL(destroy_mutex)(psi_mutex_t);
}

static void test_mysql_rwlock_part_1() {
  mysql_rwlock_init(key_rwlock_x, &my_rwlock_x);
  mysql_rwlock_init(key_rwlock_y, &my_rwlock_y);
  mysql_prlock_init(key_prlock_z, &my_prlock_z);
  mysql_prlock_init(key_prlock_t, &my_prlock_t);

  mysql_rwlock_rdlock(&my_rwlock_x);
  mysql_rwlock_wrlock(&my_rwlock_y);
  mysql_prlock_rdlock(&my_prlock_z);
  mysql_prlock_wrlock(&my_prlock_t);

  mysql_rwlock_unlock(&my_rwlock_x);
  mysql_rwlock_unlock(&my_rwlock_y);
  mysql_prlock_unlock(&my_prlock_z);
  mysql_prlock_unlock(&my_prlock_t);
}

static void test_mysql_rwlock_part_2() {
  mysql_rwlock_destroy(&my_rwlock_x);
  mysql_rwlock_destroy(&my_rwlock_y);
  mysql_prlock_destroy(&my_prlock_z);
  mysql_prlock_destroy(&my_prlock_t);
}

static void test_psi_rwlock_part_1() {
  PSI_rwlock_locker_state state;
  PSI_rwlock_locker *locker;

  psi_rwlock_s1 = PSI_RWLOCK_CALL(init_rwlock)(key_rwlock_s1, NULL);
  psi_rwlock_s2 = PSI_RWLOCK_CALL(init_rwlock)(key_rwlock_s2, NULL);

  if (psi_rwlock_s1 != NULL) {
    locker = PSI_RWLOCK_CALL(start_rwlock_rdwait)(
        &state, psi_rwlock_s1, PSI_RWLOCK_SHAREDLOCK, "HERE", 12);
    if (locker != NULL) {
      PSI_RWLOCK_CALL(end_rwlock_rdwait)(locker, 0);
    }
  }

  if (psi_rwlock_s2 != NULL) {
    locker = PSI_RWLOCK_CALL(start_rwlock_wrwait)(
        &state, psi_rwlock_s2, PSI_RWLOCK_EXCLUSIVELOCK, "THERE", 13);
    if (locker != NULL) {
      PSI_RWLOCK_CALL(end_rwlock_wrwait)(locker, 0);
    }
  }

  if (psi_rwlock_s1 != NULL) {
    PSI_RWLOCK_CALL(unlock_rwlock)(psi_rwlock_s1);
  }

  if (psi_rwlock_s2 != NULL) {
    PSI_RWLOCK_CALL(unlock_rwlock)(psi_rwlock_s2);
  }
}

static void test_psi_rwlock_part_2() {
  if (psi_rwlock_s1 != NULL) {
    PSI_RWLOCK_CALL(destroy_rwlock)(psi_rwlock_s1);
  }

  if (psi_rwlock_s2 != NULL) {
    PSI_RWLOCK_CALL(destroy_rwlock)(psi_rwlock_s2);
  }
}

static void test_mysql_cond_part_1() {
  mysql_cond_init(key_cond_x, &my_cond_x);
  mysql_cond_init(key_cond_y, &my_cond_y);
}

static void test_mysql_cond_part_2() {
  mysql_cond_destroy(&my_cond_x);
  mysql_cond_destroy(&my_cond_y);
}

static void do_something_part_1() {
  test_mysql_mutex_part_1();
  test_psi_mutex_part_1();
  test_mysql_rwlock_part_1();
  test_psi_rwlock_part_1();
  test_mysql_cond_part_1();
}

static void do_something_part_2() {
  test_mysql_mutex_part_2();
  test_psi_mutex_part_2();
  test_mysql_rwlock_part_2();
  test_psi_rwlock_part_2();
  test_mysql_cond_part_2();
}

BEGIN_COMPONENT_PROVIDES(pfs_example)
END_COMPONENT_PROVIDES();

BEGIN_COMPONENT_REQUIRES(pfs_example)
REQUIRES_MYSQL_MUTEX_SERVICE, REQUIRES_MYSQL_RWLOCK_SERVICE,
    REQUIRES_MYSQL_COND_SERVICE, REQUIRES_PSI_COND_SERVICE,
    REQUIRES_PSI_ERROR_SERVICE, REQUIRES_PSI_FILE_SERVICE,
    REQUIRES_PSI_IDLE_SERVICE, REQUIRES_PSI_MDL_SERVICE,
    REQUIRES_PSI_MEMORY_SERVICE, REQUIRES_PSI_MUTEX_SERVICE,
    REQUIRES_PSI_RWLOCK_SERVICE, REQUIRES_PSI_SOCKET_SERVICE,
    REQUIRES_PSI_STAGE_SERVICE, REQUIRES_PSI_STATEMENT_SERVICE,
    REQUIRES_PSI_TABLE_SERVICE, REQUIRES_PSI_THREAD_SERVICE,
    REQUIRES_PSI_TRANSACTION_SERVICE, END_COMPONENT_REQUIRES();

BEGIN_COMPONENT_METADATA(pfs_example)
METADATA("mysql.author", "Marc Alff, Oracle Corporation"),
    METADATA("mysql.license", "GPL"), END_COMPONENT_METADATA();

DECLARE_COMPONENT(pfs_example, "mysql:pfs_example")
pfs_example_init, pfs_example_deinit END_DECLARE_COMPONENT();

DECLARE_LIBRARY_COMPONENTS
&COMPONENT_REF(pfs_example) END_DECLARE_LIBRARY_COMPONENTS
