/* Copyright (c) 2011, 2026, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation. The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA */

#include <iostream>
#include <limits>
#include <string_view>

#include <mysql/components/library_mysys/my_system.h>  // my_num_vcpus
#include <mysql/components/my_service.h>
#include <mysql/components/services/dynamic_privilege.h>
#include <mysql/components/services/log_builtins.h>
#include <mysql/components/services/mysql_system_variable.h>  // sysvar_reader
#include <mysql/components/services/system_variable_source.h>
#include <mysql/plugin.h>
#include <mysql/psi/mysql_thread.h>
#include <mysql/service_thread_scheduler.h>
#include <mysql/thread_pool_priv.h>
#include <mysqld_error.h>
#include <sys/types.h>
#include <cmath>
#include <cstddef>
#include <sstream>  // std::stringstream

#include "my_atomic.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sys.h"
/* Thread pool includes */
#include "plugin/thread_pool/src/option_usage.h"
#include "plugin/thread_pool/src/thread_pool.h"
#include "plugin/thread_pool/src/thread_pool_tables.h"

#include "scope_guard.h"
#include "sql/derror.h"
#include "sql/sql_error.h"
#include "sql/sql_plugin_var.h"
#include "sql/table.h"

struct SYS_VAR;
struct SHOW_VAR;

static std::uint32_t thread_pool_transaction_delay = DEF_TRANSACTION_DELAY;

bool is_valid_connection_report_interval(longlong);
void set_connection_report_interval(std::chrono::seconds);
namespace {
/** Dummy variable which is only needed for the sysvar definition. An atomic
 is used to store the actual value */
unsigned int sv_connection_report_interval = 0;
}  // namespace

/*
  Pool of threads scheduler.

  This is the scheduler we add if we have a pool of threads
  implementation.

  Linux: poll, epoll
  FreeBSD/Mac OS X: poll
  Solaris: poll
  Windows: WSAPoll
*/
static THD_event_functions thread_pool_event_functions = {
    thd_pool_wait_begin, thd_pool_wait_end, thd_pool_post_kill_notification};

static Connection_handler_functions thread_pool_handler_functions = {
    0,  // Temporary value
    thd_pool_add_connection, thd_pool_end};

SERVICE_TYPE(registry) *reg_srv = nullptr;
static SERVICE_TYPE(registry_registration) *reg_reg = nullptr;
SERVICE_TYPE(log_builtins) *log_bi = nullptr;
SERVICE_TYPE(log_builtins_string) *log_bs = nullptr;

/**
  Thread pool initialization function.

  Code to make this a daemon plugin. These plugins do not really do
  anything, but it will allow options to be loaded and provide a value
  to the thread scheduler.
*/

constexpr int TP_INIT_SUCCESS = 0;
constexpr int TP_INIT_FAILURE = 1;

const char *DYN_PRIV_REG_SVCNAM = "dynamic_privilege_register.mysql_server";
using Dyn_priv_reg = const s_mysql_dynamic_privilege_register;

unsigned int longrun_trx_limit_ = 0;

/**
  Read the value of the system variable and convert it into a bool
  @param[in]  reg_srv  Registry Service
  @param[in]  name  Name of the system variable
  @return Boolean value of the system variable if success, nullopt otherwise
*/
static inline std::optional<std::string> get_system_variable_value(
    SERVICE_TYPE(registry) * reg_srv, const std::string_view name) {
  assert(reg_srv);
  my_service<SERVICE_TYPE(mysql_system_variable_reader)> sysvar_reader(
      "mysql_system_variable_reader", reg_srv);
  if (!sysvar_reader.is_valid()) {
    return std::nullopt;
  }

  // Buffer stores value of the sysvar: either "ON" or "OFF"
  std::array<char, 4> buffer;
  char *system_variable = buffer.data();
  size_t length = buffer.size();

  if (sysvar_reader->get(nullptr, "GLOBAL", "mysql_server", name.data(),
                         reinterpret_cast<void **>(&system_variable),
                         &length) != 0) {
    return std::nullopt;
  }
  return std::string{system_variable, length};
}
static inline bool update_config_defaults(SERVICE_TYPE(registry) * reg_svc);

static int thread_pool_plugin_init(void *plugin [[maybe_unused]]) {
  DBUG_TRACE;

  // Initialize error logging service.
  if (init_logging_service_for_plugin(&reg_srv, &log_bi, &log_bs))
    return TP_INIT_FAILURE;

  auto log_grd = create_scope_guard(
      []() { deinit_logging_service_for_plugin(&reg_srv, &log_bi, &log_bs); });

  // Initialize container_aware to fetch available system resources
  const auto use_cgroup = get_system_variable_value(reg_srv, "container_aware");
  assert(use_cgroup.has_value());
  init_container_aware(use_cgroup.value());

  if (update_config_defaults(reg_srv)) {
    return TP_INIT_FAILURE;
  }

  thread_pool_handler_functions.max_threads =
      static_cast<uint>(get_max_connections() + MAX_NORMAL_THREAD_GROUPS);

  if (my_connection_handler_set(&thread_pool_handler_functions,
                                &thread_pool_event_functions)) {
    LogErr(ERROR_LEVEL, ER_THREAD_POOL_CON_HANDLER_INIT_FAILED);
    return TP_INIT_FAILURE;
  }
  auto con_grd = create_scope_guard([]() { my_connection_handler_reset(); });

  if (!is_valid_connection_report_interval(sv_connection_report_interval)) {
    sv_connection_report_interval = DEF_CONNECTION_REPORT_INTERVAL;
  }

  set_connection_report_interval(
      std::chrono::seconds(sv_connection_report_interval));

  ATMC_longrun_trx_limit.store(Misec(longrun_trx_limit_));

  assert(reg_srv != nullptr);
  {
    my_service<Dyn_priv_reg> const service(DYN_PRIV_REG_SVCNAM, reg_srv);
    if (!service.is_valid() ||
        service->register_privilege(&TP_CONNECTION_ADMIN.front(),
                                    TP_CONNECTION_ADMIN.length())) {
      LogErr(ERROR_LEVEL, ER_THREAD_POOL_CANNOT_REGISTER_DYNAMIC_PRIVILEGE,
             &TP_CONNECTION_ADMIN.front());
      return TP_INIT_FAILURE;
    }
  }

  {
    my_service<SERVICE_TYPE(registry_registration)> svc("registry_registration",
                                                        reg_srv);
    if (!svc.is_valid()) {
      return TP_INIT_FAILURE;
    }
    reg_reg = svc.untie();
    if (thread_pool_plugin_option_usage_init(reg_srv, reg_reg)) {
      return TP_INIT_FAILURE;
    }
  }
  if (thd_pool_init()) {
    LogErr(ERROR_LEVEL, ER_THREAD_POOL_INIT_FAILED);
    return TP_INIT_FAILURE;
  }

  auto tsvc_grd =
      create_scope_guard([]() { deinit_plugin_table_service(reg_srv); });
  if (init_plugin_table_service(reg_srv)) {
    LogErr(ERROR_LEVEL, ER_THREAD_POOL_PFS_TABLES_INIT_FAILED);
    return TP_INIT_FAILURE;
  }

  if (tp_tables_init()) {
    LogErr(ERROR_LEVEL, ER_THREAD_POOL_PFS_TABLES_ADD_FAILED);
    return TP_INIT_FAILURE;
  }

  std::stringstream startmsg;
  startmsg << "thread_pool_size = " << thread_pool_size
           << ", thread_pool_algorithm = "
           << (thread_pool_algorithm ? "High Concurrency Algorithm"
                                     : "Low Concurrency Algorithm")
           << ", thread_pool_stall_limit = " << thread_pool_stall_limit
           << ", thread_pool_prio_kickup_timer = "
           << thread_pool_prio_kickup_timer
           << ", thread_pool_max_unused_threads = "
           << thread_pool_max_unused_threads
           << ", thread_pool_max_active_query_threads = "
           << thread_pool_max_active_query_threads
           << ", thread_pool_dedicated_listeners = "
           << thread_pool_dedicated_listeners
           << ", thread_pool_max_transactions_limit = "
           << thread_pool_max_transactions_limit
           << ", thread_pool_transaction_delay = "
           << thread_pool_transaction_delay
           << ", thread_pool_query_threads_per_group = "
           << thread_pool_query_threads_per_group
           << ", thread_pool_connection_report_interval = "
           << sv_connection_report_interval
           << ", thread_pool_longrun_trx_limit = " << longrun_trx_limit_
           << " ; " << ATMC_longrun_trx_limit.load().count();
  LogErr(SYSTEM_LEVEL, ER_THREAD_POOL_PLUGIN_STARTED, startmsg.str().c_str());

  thread_pool_plugin_initialized.store(true);
  log_grd.release();
  con_grd.release();
  tsvc_grd.release();

  return TP_INIT_SUCCESS;
}

/**
  Thread pool check function.
  This function is called during UNINSTALL PLUGIN.
 */
static int thread_pool_plugin_check(void *) {
  DBUG_TRACE;

  tp_tables_deinit();
  deinit_plugin_table_service(reg_srv);

  return 0;
}

/**
  Thread pool deinitialization function.
 */
static int thread_pool_plugin_deinit(void *) {
  DBUG_TRACE;
  if (reg_srv != nullptr) {
    my_service<Dyn_priv_reg> const service(DYN_PRIV_REG_SVCNAM, reg_srv);
    if (service.is_valid()) {
#ifndef NDEBUG
      auto r =
#endif /* NDEBUG */
          service->unregister_privilege(&TP_CONNECTION_ADMIN.front(),
                                        TP_CONNECTION_ADMIN.length());
      assert(!r);
    }
    if (reg_reg) {
#ifndef NDEBUG
      auto r1 =
#endif /* NDEBUG */
          thread_pool_plugin_option_usage_deinit(reg_srv, reg_reg);
      assert(!r1);
    }
  }

  tp_tables_deinit();
  deinit_plugin_table_service(reg_srv);

  if (is_thread_pool_plugin_initialized()) {
    (void)my_connection_handler_reset();
  }
  if (reg_reg) {
#ifndef NDEBUG
    auto r =
#endif /* NDEBUG */
        reg_srv->release(reinterpret_cast<my_h_service>(
            const_cast<SERVICE_TYPE_NO_CONST(registry_registration) *>(
                reg_reg)));
    assert(!r);
    reg_reg = nullptr;
  }
  deinit_container_aware();
  deinit_logging_service_for_plugin(&reg_srv, &log_bi, &log_bs);

  return 0;
}

/**
  Log a message in the error log (at system level) showing which variable was
  changed along with the old and new value. In debug mode the raw value
  obtained from the sysvar object is also displayed (to help identfy conversion
  errors).
 */
template <class VT, class RT>
static void log_sysvar_change(const char *sysvar_name, const VT &ov,
                              const VT &nv, const RT &rv [[maybe_unused]]) {
  std::stringstream ss;
  ss << "Old value: " << ov << ", new value: " << nv;
#ifndef NDBUG
  ss << ", (raw: " << rv << ")";
#endif /* NDEBUG */

  LogErr(SYSTEM_LEVEL, ER_THREAD_POOL_SYSVAR_CHANGE, sysvar_name,
         ss.str().c_str());
}

/**
  Check the new value to thread_pool_stall_limit

  @param save            Placeholder to save new value
  @param value           Object to discover new value

  @retval                1 if update isn't allowed, 0 means allowed update
*/
static int stall_limit_check(THD *, SYS_VAR *, void *save,
                             struct st_mysql_value *value) {
  longlong new_val;

  if (value->val_int(value, &new_val)) return 1;
  /* Variable is NOT NULL */
  *(longlong *)save = new_val;
  if (new_val >= TP_MIN_STALL_LIMIT && new_val <= TP_MAX_STALL_LIMIT) return 0;
  return 1;
}

/**
  Verify the new priority value of thread_pool_high_priority_connection

  @param save            Placeholder to save new value
  @param value           Object to discover new value

  @retval                1 if update isn't allowed, 0 means allowed update
*/
static int high_priority_connection_check(THD *, SYS_VAR *, void *save,
                                          struct st_mysql_value *value) {
  longlong new_val;

  if (value->val_int(value, &new_val)) return 1;
  /* Variable is NOT NULL */
  *(longlong *)save = new_val;
  if (new_val >= 0 && new_val <= 1) /* 2 prio levels supported */
    return 0;
  return 1;
}

/**
  Verify the new priority value of thread_pool_prio_kickup_timer

  @param save            Placeholder to save new value
  @param value           Object to discover new value

  @retval                1 if update isn't allowed, 0 means allowed update
*/
static int prio_kickup_timer_check(THD *, SYS_VAR *, void *save,
                                   struct st_mysql_value *value) {
  longlong new_val;

  if (value->val_int(value, &new_val)) return 1;
  /* Variable is NOT NULL */
  *(longlong *)save = new_val;
  if (new_val <= UINT_MAX32 && new_val >= 0) return 0;
  return 1;
}

/**
  Verify the new value of thread_pool_max_unused_threads

  @param save            Placeholder to save new value
  @param value           Object to discover new value

  @retval                1 if update isn't allowed, 0 means allowed update
*/
static int max_unused_threads_check(THD *, SYS_VAR *, void *save,
                                    struct st_mysql_value *value) {
  longlong new_val;

  if (value->val_int(value, &new_val)) return 1;
  /* Variable is NOT NULL */
  *(longlong *)save = new_val;
  if (new_val <= TP_MAX_MAX_UNUSED_THREADS && new_val >= 0) return 0;
  return 1;
}

static int check_max_trxn_limits(MYSQL_THD thd, SYS_VAR *sys_var, void *save,
                                 struct st_mysql_value *value) {
  if (thd_check_connection_admin_privilege(thd)) {
    my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0), "CONNECTION_ADMIN");
    return true;
  }

  longlong thread_pool_max_transactions_limit_new;
  value->val_int(value, &thread_pool_max_transactions_limit_new);
  if (thread_pool_max_transactions_limit_new ==
      thread_pool_max_transactions_limit) {
    *(uint *)save = thread_pool_max_transactions_limit;
    return false;
  }

  if (thread_pool_max_transactions_limit_new < 0 ||
      static_cast<uint32_t>(thread_pool_max_transactions_limit_new) >
          (pointer_cast<const sysvar_uint_t *>(sys_var))->max_val) {
    my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0),
             "thread_pool_max_transactions_limit",
             std::to_string(thread_pool_max_transactions_limit_new).c_str());
    return true;
  }

  *(uint *)save = thread_pool_max_transactions_limit_new;
  return false;
}

static void update_max_trxn_limit(THD *, SYS_VAR *, void *, const void *save) {
  using VarType = decltype(thread_pool_max_transactions_limit);
  const auto &thread_pool_max_transactions_limit_new =
      *(reinterpret_cast<const VarType *>(save));
  auto thread_pool_max_transactions_limit_old =
      thread_pool_max_transactions_limit;

  if (thread_pool_max_transactions_limit_new !=
      thread_pool_max_transactions_limit) {
    std::uint32_t trx_limit_per_tg = 0;
    if (thread_pool_max_transactions_limit_new > 0) {
      trx_limit_per_tg = std::max(
          1,
          static_cast<int>(ceil((double)thread_pool_max_transactions_limit_new /
                                thread_pool_size)));
    }

    /*
      thread_pool_max_transactions_limit introduces even transaction threads
      limit per group. So query threads per group should not exceed transaction
      thread limit set by the thread_pool_max_transactions_limit.
    */
    if (trx_limit_per_tg != 0 &&
        thread_pool_query_threads_per_group > MIN_QUERY_THREADS_PER_GROUP &&
        thread_pool_query_threads_per_group > trx_limit_per_tg) {
      my_error(ER_TP_QUERY_THRS_PER_GRP_EXCEEDS_TXN_THR_LIMIT, MYF(0),
               thread_pool_query_threads_per_group, trx_limit_per_tg);
      return;
    }
    // Need to perform this check here as the default value for MTL is 0 and
    // the check function appears not to be called for SET <var> = DEFAULT.
    if (thread_pool_max_transactions_limit_new == 0 &&
        thread_pool_dedicated_listeners != 0) {
      my_error(ER_TP_CANNOT_DISABLE_MTL_WITH_DL, MYF(0));
      return;
    }

    thread_pool_max_transactions_limit = thread_pool_max_transactions_limit_new;
    thread_pool_max_transactions_limit_per_tg = trx_limit_per_tg;
  }

  log_sysvar_change("max_transactions_limit",
                    thread_pool_max_transactions_limit_old,
                    thread_pool_max_transactions_limit,
                    thread_pool_max_transactions_limit_new);
}

static std::atomic<Misec> transaction_delay_a{Misec{DEF_TRANSACTION_DELAY}};
static_assert(transaction_delay_a.is_always_lock_free);

Misec get_transaction_delay() {
  return transaction_delay_a.load(std::memory_order_relaxed);
}

/**
  Method to validate new value being assigned to the system variable
  "thread_pool_transaction_delay".

  @param       thd      Thread Handle.
  @param       sys_var  System variable being altered.
  @param[out]  save     Placeholder to store validated new value.
  @param       value    user provided value.

  @retval   false if value is OK. true otherwise.
*/
static int check_transaction_delay(MYSQL_THD thd, SYS_VAR *sys_var, void *save,
                                   struct st_mysql_value *value) {
  if (thd_check_connection_admin_privilege(thd)) {
    my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0), "CONNECTION_ADMIN");
    return true;
  }

  longlong new_value = 0;
  value->val_int(value, &new_value);
  if (new_value < 0 ||
      new_value > (pointer_cast<const sysvar_uint_t *>(sys_var))->max_val) {
    my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0), "thread_pool_transaction_delay",
             std::to_string(new_value).c_str());
    return true;
  }

  using VarType = decltype(thread_pool_transaction_delay);
  *(reinterpret_cast<VarType *>(save)) = new_value;

  return false;
}

/**
  Method to update new value to the system variable
  "thread_pool_transaction_delay".

  @param       save     Validated new value.
*/
static void update_transaction_delay(MYSQL_THD, SYS_VAR *, void *,
                                     const void *save) {
  using VarType = decltype(thread_pool_transaction_delay);
  const VarType old_tp_transaction_delay = thread_pool_transaction_delay;
  const auto &new_value = *(reinterpret_cast<const VarType *>(save));

  if (new_value != thread_pool_transaction_delay) {
    thread_pool_transaction_delay = new_value;
    transaction_delay_a.store(Misec{new_value}, std::memory_order_relaxed);
  }

  log_sysvar_change("transaction_delay", old_tp_transaction_delay, new_value,
                    new_value);
}

static int check_max_active_query_threads(MYSQL_THD thd, SYS_VAR *sys_var,
                                          void *save,
                                          struct st_mysql_value *value) {
  if (thd_check_connection_admin_privilege(thd)) {
    my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0), "CONNECTION_ADMIN");
    return true;
  }
  push_deprecated_warn_no_replacement(
      thd, "Plugin variable thread_pool_max_actuve_query_threads");
  longlong new_value = 0;
  value->val_int(value, &new_value);
  if (new_value < 0 ||
      new_value > (pointer_cast<const sysvar_uint_t *>(sys_var))->max_val) {
    my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0),
             "thread_pool_max_active_query_threads",
             std::to_string(new_value).c_str());
    return true;
  }

  using VarType = decltype(thread_pool_max_active_query_threads);
  *(reinterpret_cast<VarType *>(save)) = new_value;

  return false;
}

/**
  Method to validate new value being assigned to the system variable
  "thread_pool_query_threads_per_group".

  @param       thd      Thread Handle.
  @param       sys_var  System variable being altered.
  @param[out]  save     Placeholder to store validated new value.
  @param       value    user provided value.

  @retval   false if value is OK. true otherwise.
*/
static int check_query_threads_per_group(MYSQL_THD thd,
                                         SYS_VAR *sys_var [[maybe_unused]],
                                         void *save,
                                         struct st_mysql_value *value) {
  if (thd_check_connection_admin_privilege(thd)) {
    my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0), "CONNECTION_ADMIN");
    return 1;
  }

  longlong new_value = 0;
  value->val_int(value, &new_value);
  if (new_value < MIN_QUERY_THREADS_PER_GROUP ||
      new_value > MAX_QUERY_THREADS_PER_GROUP) {
    my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0),
             "thread_pool_query_threads_per_group",
             std::to_string(new_value).c_str());
    return 1;
  }

  using VarType = decltype(thread_pool_query_threads_per_group);
  *(reinterpret_cast<VarType *>(save)) = new_value;

  return 0;
}

/**
  Method to update new value to the system variable
  "thread_pool_query_threads_per_group".

  @param       save     Validated new value.
*/
static void update_query_threads_per_group(MYSQL_THD, SYS_VAR *, void *,
                                           const void *save) {
  using VarType = decltype(thread_pool_query_threads_per_group);
  const auto &thread_pool_query_threads_per_group_new =
      *(reinterpret_cast<const VarType *>(save));
  const VarType thread_pool_query_threads_per_group_old =
      thread_pool_query_threads_per_group;

  if (thread_pool_query_threads_per_group_new !=
      thread_pool_query_threads_per_group) {
    /*
      thread_pool_max_transactions_limit introduces even transaction threads
      limit per group. So query threads per group should not exceed transaction
      thread limit set by the thread_pool_max_transactions_limit.
    */
    if (thread_pool_query_threads_per_group_new > MIN_QUERY_THREADS_PER_GROUP &&
        thread_pool_max_transactions_limit_per_tg != 0 &&
        thread_pool_query_threads_per_group_new >
            thread_pool_max_transactions_limit_per_tg) {
      my_error(ER_TP_QUERY_THRS_PER_GRP_EXCEEDS_TXN_THR_LIMIT, MYF(0),
               thread_pool_query_threads_per_group_new,
               thread_pool_max_transactions_limit_per_tg);
      return;
    }

    adjust_query_threads_in_each_thread_group(
        thread_pool_query_threads_per_group_new);
    thread_pool_query_threads_per_group =
        thread_pool_query_threads_per_group_new;
  }

  log_sysvar_change("query_threads_per_group",
                    thread_pool_query_threads_per_group_old,
                    thread_pool_query_threads_per_group_new,
                    thread_pool_query_threads_per_group_new);
}

static MYSQL_SYSVAR_ULONG(
    size, thread_pool_size, PLUGIN_VAR_READONLY | PLUGIN_VAR_RQCMDARG,
    "How many thread groups we should create to handle query requests"
    " (minimum 1, maximum 512, all other values disables threadpool)",
    nullptr, nullptr,  // check, update
    // DEF_POOL_SIZE is only a placeholder. Default value for this
    // variable is set based on underlying hardware during plugin
    // initialization.
    DEF_POOL_SIZE, TP_MIN_POOL_SIZE, TP_MAX_POOL_SIZE,
    1);  // def, min, max, blk

static MYSQL_SYSVAR_ULONG(
    algorithm, thread_pool_algorithm, PLUGIN_VAR_READONLY | PLUGIN_VAR_RQCMDARG,
    "0 = Low concurrency algorithm, 1 = High concurrency algorithm. "
    "Deprecated. Using the default value is recommended.",
    nullptr,
    nullptr,  // check, update
    DEF_ALGORITHM, LOW_CONCURRENCY_ALGORITHM, HIGH_CONCURRENCY_ALGORITHM,
    1);  // def, min, max, blk

/* This parameter is allowed to change in runtime */
static MYSQL_SYSVAR_ULONG(
    stall_limit, thread_pool_stall_limit, PLUGIN_VAR_RQCMDARG,
    "Number of 10ms interval passed before a query is declared as stalled",
    stall_limit_check, nullptr,  // check, update
    DEF_STALL_LIMIT, TP_MIN_STALL_LIMIT, TP_MAX_STALL_LIMIT,
    1);  // def, min, max, blk

/* This parameter is allowed to change in runtime */
static MYSQL_SYSVAR_ULONG(
    prio_kickup_timer, thread_pool_prio_kickup_timer, PLUGIN_VAR_RQCMDARG,
    "How many milliseconds before a queued transactions receives higher prio",
    prio_kickup_timer_check, nullptr,  // check, update
    DEF_PRIO_KICKUP_TIMER, TP_MIN_PRIO_KICKUP_TIMER, TP_MAX_PRIO_KICKUP_TIMER,
    1);  // def, min, max, blk

/* This parameter is allowed to change in runtime */
static MYSQL_SYSVAR_ULONG(
    max_unused_threads, thread_pool_max_unused_threads, PLUGIN_VAR_RQCMDARG,
    "Max number of unused threads per thread group, 0 means no limit",
    max_unused_threads_check, nullptr,  // check, update
    DEF_MAX_UNUSED_THREADS, TP_MIN_MAX_UNUSED_THREADS,
    TP_MAX_MAX_UNUSED_THREADS,
    1);  // def, min, max, blk

static MYSQL_THDVAR_ULONG(high_priority_connection, PLUGIN_VAR_RQCMDARG,
                          "Set priority of queries in this connection",
                          high_priority_connection_check, nullptr, 0UL, 0UL,
                          1UL,
                          1);  // def, min, max, blk

static MYSQL_SYSVAR_UINT(
    max_active_query_threads, thread_pool_max_active_query_threads,
    PLUGIN_VAR_RQCMDARG,
    "Maximum active query threads in a thread group. Deprecated. Use is "
    "dangerous and could lead to deadlocks.",
    check_max_active_query_threads, nullptr, DEF_MAX_ACTIVE_QUERY_THREADS,
    TP_MIN_MAX_ACTIVE_QUERY_THREADS, TP_MAX_MAX_ACTIVE_QUERY_THREADS,
    1);  // def , min, max, blk

static MYSQL_SYSVAR_UINT(
    max_transactions_limit, thread_pool_max_transactions_limit,
    PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_PERSIST_AS_READ_ONLY,
    "Maximum Transactions Allowed By Thread Pool for Processing",
    check_max_trxn_limits, update_max_trxn_limit,
    // DEF_MAX_TRANSACTIONS_LIMIT is only a placeholder as the plugin
    // updates the default value during initialization.
    DEF_MAX_TRANSACTIONS_LIMIT, TP_MIN_MAX_TRANSACTIONS_LIMIT,
    TP_MAX_MAX_TRANSACTIONS_LIMIT,
    1);  // def , min, max, blk

static MYSQL_SYSVAR_BOOL(dedicated_listeners, thread_pool_dedicated_listeners,
                         PLUGIN_VAR_READONLY | PLUGIN_VAR_RQCMDARG,
                         "Do not allow listener threads to execute queries. "
                         "Deprecated. Lowers performance and is not needed.",
                         nullptr, nullptr, DEF_DEDICATED_LISTENERS);

static MYSQL_SYSVAR_UINT(transaction_delay, thread_pool_transaction_delay,
                         PLUGIN_VAR_OPCMDARG,
                         "Time to sleep before starting to execute a new "
                         "transaction (in milliseconds).",
                         check_transaction_delay, update_transaction_delay,
                         DEF_TRANSACTION_DELAY, MIN_TRANSACTION_DELAY,
                         MAX_TRANSACTION_DELAY /*5 mins*/,
                         1);  // def , min, max, blk

static MYSQL_SYSVAR_UINT(query_threads_per_group,
                         thread_pool_query_threads_per_group,
                         PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_PERSIST_AS_READ_ONLY,
                         "Number of query worker threads per thread group.",
                         check_query_threads_per_group,
                         update_query_threads_per_group,
                         DEF_QUERY_THREADS_PER_GROUP,
                         MIN_QUERY_THREADS_PER_GROUP,
                         MAX_QUERY_THREADS_PER_GROUP, 1);
static MYSQL_SYSVAR_UINT(
    connection_report_interval, sv_connection_report_interval,
    PLUGIN_VAR_OPCMDARG,
    "Time in seconds between each time the stall checker "
    "thread will emit a status message about connections managed"
    " by the thread pool. 0 means reporting is disabled.",
    [](MYSQL_THD thd, SYS_VAR *, void *save,
       st_mysql_value *value) -> int {  // check-variable function
      if (thd_check_connection_admin_privilege(thd)) {
        my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0), "CONNECTION_ADMIN");
        return 1;
      }

      longlong new_value = 0;
      value->val_int(value, &new_value);
      if (!is_valid_connection_report_interval(new_value)) {
        my_error(ER_WRONG_VALUE_FOR_VAR, MYF(0),
                 "thread_pool_connection_report_interval",
                 std::to_string(new_value).c_str());
        return 1;
      }
      // std::bitcast
      std::memcpy(save, &new_value, sizeof(new_value));
      return 0;
    },
    [](MYSQL_THD, SYS_VAR *, void *,
       const void *s) {  // update-variable function
      longlong new_value = 0;
      // std::bitcast
      std::memcpy(&new_value, s, sizeof(new_value));
      set_connection_report_interval(std::chrono::seconds(new_value));

      // For a MYSQL_SYSVAR_UINT the backing variable
      // (sv_connection_report_interval) must have type unsigned int, but the
      // creation of a seconds object can use the raw longlong value directly.
      // But the log_sysvar_change function template requires first and second
      // argument to have the same type, so a cast is required.
      log_sysvar_change(
          "connection_report_interval", sv_connection_report_interval,
          static_cast<decltype(sv_connection_report_interval)>(new_value),
          new_value);
      sv_connection_report_interval = new_value;
    },
    DEF_CONNECTION_REPORT_INTERVAL, CONNECTION_REPORT_INTERVAL_OFF,
    MAX_CONNECTION_REPORT_INTERVAL, 1);  // def , min, max, blk
static MYSQL_SYSVAR_UINT(
    longrun_trx_limit, longrun_trx_limit_, PLUGIN_VAR_OPCMDARG,
    "Time a transaction can last until it is considered to be long-running "
    " (in milliseconds). The value is only used when "
    "thread_pool_max_transactions_limit > 0.",
    [](auto thd, auto, auto save, auto value) {
      if (thd_check_connection_admin_privilege(thd)) {
        my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0), "CONNECTION_ADMIN");
        return 1;
      }

      longlong new_value = 0;
      value->val_int(value, &new_value);
      std::memcpy(save, &new_value, sizeof(new_value));
      return (new_value < 10 || new_value > 86400000) ? 1 : 0;
    },
    [](auto, auto, auto, auto s) {
      longlong new_value = 0;
      std::memcpy(&new_value, s, sizeof(new_value));
      ATMC_longrun_trx_limit.store(Misec(new_value), std::memory_order_relaxed);
      longrun_trx_limit_ = new_value;
    },
    DEF_LONGRUN_TRX_LIMIT, MIN_LONGRUN_TRX_LIMIT, MAX_LONGRUN_TRX_LIMIT,
    1);  // def , min, max, blk

/**
  Update the default values of thread pool plugin variables based on the
  current system configuration.

  This function is called during plugin initialization to ensure that
  the default values of the thread pool plugin variables are set
  according to virtual CPUs available to mysqld.

  @param  reg_svc The registry service used to access system variables.
  @return true if an error occurred, false otherwise.
*/
static inline bool update_config_defaults(SERVICE_TYPE(registry) * reg_svc) {
  assert(reg_svc != nullptr);
  my_h_service source_service;
  /* Acquire this service to determine source of the variables */
  if (reg_svc->acquire("system_variable_source", &source_service)) {
    return true;
  }
  assert(source_service != nullptr);
  SERVICE_TYPE(system_variable_source) *sysvar_source_svc =
      reinterpret_cast<SERVICE_TYPE(system_variable_source) *>(source_service);
  assert(sysvar_source_svc != nullptr);

  /* thread_pool_size: Update the default value */
  ulong thread_pool_size_default =
      std::clamp(my_num_vcpus() / 2, uint32_t{1}, uint32_t{16});
  size.def_val = thread_pool_size_default;

  /* thread_pool_size: Set to default if not configured */
  enum_variable_source src = COMPILED;
  auto result [[maybe_unused]] =
      sysvar_source_svc->get(STRING_WITH_LEN("thread_pool_size"), &src);
  assert(!result);
  if (src == COMPILED) {
    thread_pool_size = size.def_val;
  }

  /* thread_pool_max_transactions_limit: Update the default value */
  uint thread_pool_max_transactions_limit_default =
      std::clamp(static_cast<uint>(32 * thread_pool_size), uint{1}, uint{512});
  max_transactions_limit.def_val = thread_pool_max_transactions_limit_default;

  /* thread_pool_max_transactions_limit: Set to default if not configured */
  result = sysvar_source_svc->get(
      STRING_WITH_LEN("thread_pool_max_transactions_limit"), &src);
  assert(!result);
  if (src == COMPILED) {
    thread_pool_max_transactions_limit = max_transactions_limit.def_val;
  }

  /* Update thread_pool_max_transactions_limit_per_tg */
  if (thread_pool_max_transactions_limit != 0) {
    thread_pool_max_transactions_limit_per_tg = std::max(
        std::uint32_t(1),
        static_cast<uint32_t>(
            ceil(static_cast<double>(thread_pool_max_transactions_limit) /
                 thread_pool_size)));
    DBUG_PRINT("tp",
               ("Starting with thread_pool_max_transactions_limit_per_tg:%u",
                thread_pool_max_transactions_limit_per_tg));
  }

  // Log a warning if thread_pool_query_threads_per_group is 1 with
  // HIGH_CONCURRENCY_ALGORITHM
  if (thread_pool_query_threads_per_group == 1 &&
      thread_pool_algorithm == HIGH_CONCURRENCY_ALGORITHM) {
    LogErr(WARNING_LEVEL,
           ER_WARN_THREAD_POOL_QUERY_THREADS_MISMATCH_INCOMPATIBLE_ALGO);
  }

  // thread_pool_algorithm has been deprecated
  if (thread_pool_algorithm != DEF_ALGORITHM) {
    LogErr(ERROR_LEVEL, ER_DEPRECATE_MSG_NO_REPLACEMENT,
           "The plugin variable thread_pool_algorithm");
  }

  /*
    thread_pool_max_transactions_limit enforces limit on the max number
    of concurrent transactions at the group level. Because of this, the
    number of query threads per group
    i.e, thread_pool_query_threads_per_group should not exceed the
    limit set by thread_pool_max_transactions_limit_per_tg.

    Enforcing this ensures that the plugin does not create more query
    threads per group than the number of threads that can be actively
    used for transactions, which could otherwise lead to resource
    contention.

    If thread_pool_query_threads_per_group is set to a value higher than
    the per-group limit, it will be reduced to match the limit to
    maintain correct operation and avoid misconfiguration.
  */
  if ((thread_pool_max_transactions_limit_per_tg != 0) &&
      (thread_pool_query_threads_per_group > MIN_QUERY_THREADS_PER_GROUP) &&
      (thread_pool_query_threads_per_group >
       thread_pool_max_transactions_limit_per_tg)) {
    bool needs_extra_warning =
        (thread_pool_algorithm == HIGH_CONCURRENCY_ALGORITHM &&
         thread_pool_max_transactions_limit_per_tg == 1);
    LogErr(WARNING_LEVEL, ER_WARN_THREAD_POOL_QUERY_THREADS_INCOMPATIBLE_MTL,
           thread_pool_query_threads_per_group,
           thread_pool_max_transactions_limit_per_tg,
           thread_pool_max_transactions_limit_per_tg,
           needs_extra_warning
               ? " Please note that the auto-corrected value "
                 "thread_pool_query_threads_per_group(1) is not recommended "
                 "when used with HIGH_CONCURRENCY_ALGORITHM."
               : "");
    thread_pool_query_threads_per_group =
        thread_pool_max_transactions_limit_per_tg;
  }

  // thread_pool_dedicated_listeners is deprecated
  // Using it lowers performance and is not needed.
  if (thread_pool_dedicated_listeners != DEF_DEDICATED_LISTENERS) {
    LogErr(WARNING_LEVEL, ER_DEPRECATE_MSG_NO_REPLACEMENT,
           "The plugin variable thread_pool_dedicated_listeners");

    // Log a warning if thread_pool_dedicated_listeners is set without
    // setting thread_pool_max_transactions_limit.
    if (thread_pool_max_transactions_limit == 0) {
      LogErr(WARNING_LEVEL,
             ER_WARN_THREAD_POOL_DEDICATED_LISTENERS_INCOMPATIBLE_MTL);
      thread_pool_dedicated_listeners = false;
    }
  }

  // thread_pool_max_active_threads is deprecated. It is considered dangerous
  // and could lead to deadlocks.
  if (thread_pool_max_active_query_threads != DEF_MAX_ACTIVE_QUERY_THREADS) {
    LogErr(WARNING_LEVEL, ER_DEPRECATE_MSG_NO_REPLACEMENT,
           "The plugin variable thread_pool_max_active_threads");
  }

  return reg_svc->release(source_service);
}

static SYS_VAR *thread_pool_system_vars[] = {
    MYSQL_SYSVAR(size),
    MYSQL_SYSVAR(algorithm),
    MYSQL_SYSVAR(stall_limit),
    MYSQL_SYSVAR(prio_kickup_timer),
    MYSQL_SYSVAR(high_priority_connection),
    MYSQL_SYSVAR(max_unused_threads),
    MYSQL_SYSVAR(max_active_query_threads),
    MYSQL_SYSVAR(max_transactions_limit),
    MYSQL_SYSVAR(dedicated_listeners),
    MYSQL_SYSVAR(transaction_delay),
    MYSQL_SYSVAR(query_threads_per_group),
    MYSQL_SYSVAR(connection_report_interval),
    MYSQL_SYSVAR(longrun_trx_limit),
    nullptr};

static int thread_pool_show_query_worker_threads(MYSQL_THD, SHOW_VAR *var,
                                                 char *buff) {
  var->type = SHOW_LONGLONG;
  var->value = buff;
  auto *value = reinterpret_cast<long long *>(buff);
  *value = static_cast<long long>(tp_get_query_worker_threads());
  return 0;
}

static int deprecated_i_s_usage_count(THD *, SHOW_VAR *var, char *buff) {
  var->type = SHOW_LONGLONG;
  var->value = buff;
  std::int64_t usage = get_i_s_usage();
  std::memcpy(buff, &usage, sizeof(usage));
  return 0;
}

static SHOW_VAR thread_pool_plugin_status_vars[] = {
    {"Thread_pool_query_threads",
     reinterpret_cast<char *>(&thread_pool_show_query_worker_threads),
     SHOW_FUNC, SHOW_SCOPE_GLOBAL},
    {"Deprecated_thread_pool_i_s_usage_count",
     reinterpret_cast<char *>(&deprecated_i_s_usage_count), SHOW_FUNC,
     SHOW_SCOPE_GLOBAL},
    {"option_tracker_usage:Enterprise Thread Pool",
     reinterpret_cast<char *>(&opt_option_tracker_usage_thread_pool_plugin),
     SHOW_LONGLONG, SHOW_SCOPE_GLOBAL},
    {nullptr, nullptr, SHOW_LONG, SHOW_SCOPE_GLOBAL}};

struct st_mysql_daemon thread_pool_plugin = {MYSQL_DAEMON_INTERFACE_VERSION};

struct st_mysql_information_schema thread_pool_plugin_thread_state_table = {
    MYSQL_INFORMATION_SCHEMA_INTERFACE_VERSION};

struct st_mysql_information_schema thread_pool_plugin_thread_group_state_table =
    {MYSQL_INFORMATION_SCHEMA_INTERFACE_VERSION};

struct st_mysql_information_schema thread_pool_plugin_thread_group_stats_table =
    {MYSQL_INFORMATION_SCHEMA_INTERFACE_VERSION};

static ST_FIELD_INFO thread_state_table_fields[] = {
    {"TP_GROUP_ID", 6, MYSQL_TYPE_LONG, 0, MY_I_S_UNSIGNED, nullptr, 0},
    {"TP_THREAD_NUMBER", 6, MYSQL_TYPE_LONG, 0, MY_I_S_UNSIGNED, nullptr, 0},
    {"PROCESS_COUNT", 20, MYSQL_TYPE_LONGLONG, 0, MY_I_S_UNSIGNED, nullptr, 0},
    {"WAIT_TYPE", 30, MYSQL_TYPE_STRING, 0, MY_I_S_MAYBE_NULL, nullptr, 0},
    {"TP_THREAD_TYPE", 32, MYSQL_TYPE_STRING, 0, 0, nullptr, 0},
    {"THREAD_ID", 20, MYSQL_TYPE_LONGLONG, 0,
     MY_I_S_UNSIGNED | MY_I_S_MAYBE_NULL, nullptr, 0},
    {nullptr, 0, MYSQL_TYPE_NULL, 0, 0, nullptr, 0}};

static ST_FIELD_INFO thread_group_state_table_fields[] = {
    {"TP_GROUP_ID", 6, MYSQL_TYPE_LONG, 0, MY_I_S_UNSIGNED, nullptr, 0},
    {"CONSUMER_THREADS", 6, MYSQL_TYPE_LONG, 0, MY_I_S_UNSIGNED, nullptr, 0},
    {"RESERVE_THREADS", 6, MYSQL_TYPE_LONG, 0, MY_I_S_UNSIGNED, nullptr, 0},
    {"CONNECT_THREAD_COUNT", 6, MYSQL_TYPE_LONG, 0, MY_I_S_UNSIGNED, nullptr,
     0},
    {"CONNECTION_COUNT", 6, MYSQL_TYPE_LONG, 0, MY_I_S_UNSIGNED, nullptr, 0},
    {"QUEUED_QUERIES", 6, MYSQL_TYPE_LONG, 0, MY_I_S_UNSIGNED, nullptr, 0},
    {"QUEUED_TRANSACTIONS", 6, MYSQL_TYPE_LONG, 0, MY_I_S_UNSIGNED, nullptr, 0},
    {"STALL_LIMIT", 10, MYSQL_TYPE_LONG, 0, MY_I_S_UNSIGNED, nullptr, 0},
    {"PRIO_KICKUP_TIMER", 10, MYSQL_TYPE_LONG, 0, MY_I_S_UNSIGNED, nullptr, 0},
    {"ALGORITHM", 20, MYSQL_TYPE_STRING, 0, 0, nullptr, 0},
    {"THREAD_COUNT", 6, MYSQL_TYPE_LONG, 0, MY_I_S_UNSIGNED, nullptr, 0},
    {"ACTIVE_THREAD_COUNT", 6, MYSQL_TYPE_LONG, 0, MY_I_S_UNSIGNED, nullptr, 0},
    {"STALLED_THREAD_COUNT", 6, MYSQL_TYPE_LONG, 0, MY_I_S_UNSIGNED, nullptr,
     0},
    {"WAITING_THREAD_NUMBER", 6, MYSQL_TYPE_LONG, 0,
     MY_I_S_UNSIGNED | MY_I_S_MAYBE_NULL, nullptr, 0},
    {"OLDEST_QUEUED", 20, MYSQL_TYPE_LONGLONG, 0,
     MY_I_S_UNSIGNED | MY_I_S_MAYBE_NULL, nullptr, 0},
    {"MAX_THREAD_IDS_IN_GROUP", 6, MYSQL_TYPE_LONG, 0, MY_I_S_UNSIGNED, nullptr,
     0},
    {nullptr, 0, MYSQL_TYPE_NULL, 0, 0, nullptr, 0}};

static ST_FIELD_INFO thread_group_stats_table_fields[] = {
    {"TP_GROUP_ID", 6, MYSQL_TYPE_LONG, 0, MY_I_S_UNSIGNED, nullptr, 0},
    {"CONNECTIONS_STARTED", 20, MYSQL_TYPE_LONGLONG, 0, MY_I_S_UNSIGNED,
     nullptr, 0},
    {"CONNECTIONS_CLOSED", 20, MYSQL_TYPE_LONGLONG, 0, MY_I_S_UNSIGNED, nullptr,
     0},
    {"QUERIES_EXECUTED", 20, MYSQL_TYPE_LONGLONG, 0, MY_I_S_UNSIGNED, nullptr,
     0},
    {"QUERIES_QUEUED", 20, MYSQL_TYPE_LONGLONG, 0, MY_I_S_UNSIGNED, nullptr, 0},
    {"THREADS_STARTED", 20, MYSQL_TYPE_LONGLONG, 0, MY_I_S_UNSIGNED, nullptr,
     0},
    {"PRIO_KICKUPS", 20, MYSQL_TYPE_LONGLONG, 0, MY_I_S_UNSIGNED, nullptr, 0},
    {"STALLED_QUERIES_EXECUTED", 20, MYSQL_TYPE_LONGLONG, 0, MY_I_S_UNSIGNED,
     nullptr, 0},
    {"BECOME_CONSUMER_THREAD", 20, MYSQL_TYPE_LONGLONG, 0, MY_I_S_UNSIGNED,
     nullptr, 0},
    {"BECOME_RESERVE_THREAD", 20, MYSQL_TYPE_LONGLONG, 0, MY_I_S_UNSIGNED,
     nullptr, 0},
    {"BECOME_WAITING_THREAD", 20, MYSQL_TYPE_LONGLONG, 0, MY_I_S_UNSIGNED,
     nullptr, 0},
    {"WAKE_THREAD_STALL_CHECKER", 20, MYSQL_TYPE_LONGLONG, 0, MY_I_S_UNSIGNED,
     nullptr, 0},
    {"SLEEP_WAITS", 20, MYSQL_TYPE_LONGLONG, 0, MY_I_S_UNSIGNED, nullptr, 0},
    {"DISK_IO_WAITS", 20, MYSQL_TYPE_LONGLONG, 0, MY_I_S_UNSIGNED, nullptr, 0},
    {"ROW_LOCK_WAITS", 20, MYSQL_TYPE_LONGLONG, 0, MY_I_S_UNSIGNED, nullptr, 0},
    {"GLOBAL_LOCK_WAITS", 20, MYSQL_TYPE_LONGLONG, 0, MY_I_S_UNSIGNED, nullptr,
     0},
    {"META_DATA_LOCK_WAITS", 20, MYSQL_TYPE_LONGLONG, 0, MY_I_S_UNSIGNED,
     nullptr, 0},
    {"TABLE_LOCK_WAITS", 20, MYSQL_TYPE_LONGLONG, 0, MY_I_S_UNSIGNED, nullptr,
     0},
    {"USER_LOCK_WAITS", 20, MYSQL_TYPE_LONGLONG, 0, MY_I_S_UNSIGNED, nullptr,
     0},
    {"BINLOG_WAITS", 20, MYSQL_TYPE_LONGLONG, 0, MY_I_S_UNSIGNED, nullptr, 0},
    {"GROUP_COMMIT_WAITS", 20, MYSQL_TYPE_LONGLONG, 0, MY_I_S_UNSIGNED, nullptr,
     0},
    {"FSYNC_WAITS", 20, MYSQL_TYPE_LONGLONG, 0, MY_I_S_UNSIGNED, nullptr, 0},
    {nullptr, 0, MYSQL_TYPE_NULL, 0, 0, nullptr, 0}};

static int thread_pool_plugin_thread_state_table_init(void *ptr) {
  auto *schema_table = (ST_SCHEMA_TABLE *)ptr;

  schema_table->fields_info = thread_state_table_fields;
  schema_table->fill_table = fill_thread_state_table;
  return 0;
}

static int thread_pool_plugin_thread_group_state_table_init(void *ptr) {
  auto *schema_table = (ST_SCHEMA_TABLE *)ptr;

  schema_table->fields_info = thread_group_state_table_fields;
  schema_table->fill_table = fill_thread_group_state_table;
  return 0;
}

static int thread_pool_plugin_thread_group_stats_table_init(void *ptr) {
  auto *schema_table = (ST_SCHEMA_TABLE *)ptr;

  schema_table->fields_info = thread_group_stats_table_fields;
  schema_table->fill_table = fill_thread_group_stats_table;
  return 0;
}

/**
  Support method to check if the connection is set to high priority

  @client_cntx               Low level client context
*/
bool is_high_priority_connection(THD *thd) {
  return (THDVAR(thd, high_priority_connection) != 0);
}

/*
  Plugin library descriptor
*/

mysql_declare_plugin(thread_pool){
    MYSQL_DAEMON_PLUGIN,
    &thread_pool_plugin,
    "thread_pool",
    PLUGIN_AUTHOR_ORACLE,
    "Threads pool implementation to handle multiple connections for each "
    "thread",
    PLUGIN_LICENSE_PROPRIETARY,
    thread_pool_plugin_init,        /* plugin init */
    thread_pool_plugin_check,       /* plugin check uninstall */
    thread_pool_plugin_deinit,      /* plugin deinit */
    0x0100,                         /* 1.0: GA */
    thread_pool_plugin_status_vars, /* status variables */
    thread_pool_system_vars,        /* system variables */
    nullptr,                        /* config options */
    PLUGIN_OPT_NO_INSTALL | PLUGIN_OPT_NO_UNINSTALL,
},
    {
        MYSQL_INFORMATION_SCHEMA_PLUGIN,
        &thread_pool_plugin_thread_state_table,
        "TP_THREAD_STATE",
        PLUGIN_AUTHOR_ORACLE,
        "I_S table describing state of threads in thread_pool",
        PLUGIN_LICENSE_PROPRIETARY,
        thread_pool_plugin_thread_state_table_init,
        nullptr,
        nullptr,
        0x0100, /* 1.0: GA */
        nullptr,
        nullptr,
        nullptr,
        0,
    },
    {
        MYSQL_INFORMATION_SCHEMA_PLUGIN,
        &thread_pool_plugin_thread_group_state_table,
        "TP_THREAD_GROUP_STATE",
        PLUGIN_AUTHOR_ORACLE,
        "I_S table describing state of thread groups in thread_pool",
        PLUGIN_LICENSE_PROPRIETARY,
        thread_pool_plugin_thread_group_state_table_init,
        nullptr,
        nullptr,
        0x0100, /* 1.0: GA */
        nullptr,
        nullptr,
        nullptr,
        0,
    },
    {
        MYSQL_INFORMATION_SCHEMA_PLUGIN,
        &thread_pool_plugin_thread_group_stats_table,
        "TP_THREAD_GROUP_STATS",
        PLUGIN_AUTHOR_ORACLE,
        "I_S table with thread group statistics in thread_pool",
        PLUGIN_LICENSE_PROPRIETARY,
        thread_pool_plugin_thread_group_stats_table_init,
        nullptr,
        nullptr,
        0x0100, /* 1.0: GA */
        nullptr,
        nullptr,
        nullptr,
        0,
    } mysql_declare_plugin_end;
