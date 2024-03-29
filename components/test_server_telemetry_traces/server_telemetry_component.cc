/* Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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

#include "server_telemetry_component.h"
#include <cinttypes>  // PRId64
#include <thread>     // sleep_for()
#include "server_telemetry_data.h"
#include "template_utils.h"  // pointer_cast

using namespace test_telemetry;

/* test_server_telemetry_traces_component requires/uses the following services.
 */
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_server_telemetry_traces_v1,
                                telemetry_v1_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_query_attributes_iterator,
                                qa_iterator_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_query_attribute_string, qa_string_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_query_attribute_isnull, qa_isnull_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_string_converter, string_converter_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_string_charset_converter,
                                charset_converter_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_charset, charset_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_string_factory, string_factory_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(component_sys_variable_register,
                                sysvar_register_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(component_sys_variable_unregister,
                                sysvar_unregister_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_thd_attributes, thd_attributes_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_current_thread_reader, current_thd_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_thd_store, thd_store_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_thd_security_context, thd_scx_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(mysql_security_context_options,
                                scx_options_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(pfs_notification_v3, notification_srv);
REQUIRES_SERVICE_PLACEHOLDER_AS(status_variable_registration,
                                statvar_register_srv);

// Pointers to component's system variable values.
static char *trace_key_value;
static char *application_context_keys_value;
static char *callsite_context_keys_value;

/**
  @page TEST_SERVER_TELEMETRY_TRACES_COMPONENT A test component for PS server
  telemetry service

  Component Name  : test_server_telemetry_traces \n
  Source location : components/test_server_telemetry_traces

  This file contains a definition of the test_server_telemetry_traces component.
*/

static FileLogger g_log("test_server_telemetry_traces_component.log");

/*
  Custom per-customer definition of application/call-site context query
  attribute names, set through component sysvars, example:
  app_ctx = {"client_id", "root_id", "parent_id", "id"};
  call_ctx = {"source_file", "source_line"};
*/
static std::set<std::string> g_tags_app_ctx;
static std::set<std::string> g_tags_call_ctx;

// counts live "telemetry sessions" for this component
static std::atomic_int64_t g_RefCount = 0LL;

/*
  Update function for "application_context_keys" system variable.
  Parses new sysvar value to initialize set of tags variable (g_tags_app_ctx).
*/
static void tracing_app_ctx_update(MYSQL_THD, SYS_VAR * /*unused*/,
                                   void *var_ptr, const void *save) {
  *static_cast<const char **>(var_ptr) =
      *static_cast<const char **>(const_cast<void *>(save));
  g_log.write("> sysvar 'application_context_keys' updated to '%s'\n",
              application_context_keys_value);
  parse_tags(application_context_keys_value, g_tags_app_ctx);
}

/*
  Update function for "callsite_context_keys" system variable.
  Parses new sysvar value to initialize set of tags variable (g_tags_call_ctx).
*/
static void tracing_call_ctx_update(MYSQL_THD, SYS_VAR * /*unused*/,
                                    void *var_ptr, const void *save) {
  *static_cast<const char **>(var_ptr) =
      *static_cast<const char **>(const_cast<void *>(save));
  g_log.write("> sysvar 'callsite_context_keys' updated to '%s'\n",
              callsite_context_keys_value);
  parse_tags(callsite_context_keys_value, g_tags_call_ctx);
}

static int register_system_variables() {
  STR_CHECK_ARG(str) str_arg;
  str_arg.def_val = const_cast<char *>("trace");
  if (sysvar_register_srv->register_variable(
          "test_server_telemetry_traces", "trace_key",
          PLUGIN_VAR_STR | PLUGIN_VAR_MEMALLOC,
          "query attribute name used to switch statement tracing on/off",
          nullptr, nullptr, (void *)&str_arg, (void *)&trace_key_value)) {
    g_log.write("register_variable failed (trace_key).\n");
    return 1;
  }

  str_arg.def_val = nullptr;
  if (sysvar_register_srv->register_variable(
          "test_server_telemetry_traces", "application_context_keys",
          PLUGIN_VAR_STR | PLUGIN_VAR_MEMALLOC,
          "semi-colon delimited list of application context tags (query "
          "attribute names)",
          nullptr, tracing_app_ctx_update, (void *)&str_arg,
          (void *)&application_context_keys_value)) {
    g_log.write("register_variable failed (application_context_keys).\n");
    goto activate_key;
  }

  str_arg.def_val = nullptr;
  if (sysvar_register_srv->register_variable(
          "test_server_telemetry_traces", "callsite_context_keys",
          PLUGIN_VAR_STR | PLUGIN_VAR_MEMALLOC,
          "semi-colon delimited list of call-site context tags (query "
          "attribute names)",
          nullptr, tracing_call_ctx_update, (void *)&str_arg,
          (void *)&callsite_context_keys_value)) {
    g_log.write("register_variable failed (callsite_context_keys).\n");
    goto app_context;
  }

  return 0; /* All system variables registered successfully */

app_context:
  sysvar_unregister_srv->unregister_variable("test_server_telemetry_traces",
                                             "application_context_keys");

activate_key:
  sysvar_unregister_srv->unregister_variable("test_server_telemetry_traces",
                                             "trace_key");

  return 1; /* register_variable() api failed for one of the system variables */
}

static void unregister_system_variables() {
  if (sysvar_unregister_srv->unregister_variable("test_server_telemetry_traces",
                                                 "trace_key")) {
    g_log.write("unregister_variable failed (trace_key).\n");
  }

  if (sysvar_unregister_srv->unregister_variable("test_server_telemetry_traces",
                                                 "application_context_keys")) {
    g_log.write("unregister_variable failed (application_context_keys).\n");
  }

  if (sysvar_unregister_srv->unregister_variable("test_server_telemetry_traces",
                                                 "callsite_context_keys")) {
    g_log.write("unregister_variable failed (callsite_context_keys).\n");
  }
}

static int show_session_refcount(THD * /*unused*/, SHOW_VAR *var, char *buf) {
  var->type = SHOW_LONG;
  var->value = buf;
  *(pointer_cast<long *>(buf)) = g_RefCount.load();
  return 0;
}

static SHOW_VAR status_func_var[] = {
    {"test_server_telemetry_traces.live_sessions",
     (char *)show_session_refcount, SHOW_FUNC, SHOW_SCOPE_GLOBAL},
    {nullptr, nullptr, SHOW_UNDEF,
     SHOW_SCOPE_UNDEF}  // null terminator required
};

static int register_status_variables() {
  if (statvar_register_srv->register_variable((SHOW_VAR *)&status_func_var)) {
    g_log.write("Failed to register status variable.");
    return 1;
  }
  return 0;
}

static void unregister_status_variables() {
  if (statvar_register_srv->unregister_variable((SHOW_VAR *)&status_func_var)) {
    g_log.write("Failed to unregister status variable.");
  }
}

static void tm_thread_create(const PSI_thread_attrs * /*thread_attrs*/) {}

static void tm_thread_destroy(const PSI_thread_attrs * /*thread_attrs*/) {}

/**
  Server telemetry callback that handles "client session connect" event.
*/
static void tm_session_connect(const PSI_thread_attrs * /*thread_attrs*/) {
  MYSQL_THD thd = nullptr;
  if (current_thd_srv->get(&thd) || thd == nullptr) {
    g_log.write(" tm_session_connect: failed to get current THD\n");
    return;
  }
  MYSQL_LEX_CSTRING user{};
  if (get_user(thd, user)) {
    g_log.write(" tm_session_connect: failed to get user name\n");
  }
  g_log.write(" tm_session_connect: client session started (user=%s)\n",
              user.str);
}

/**
  Server telemetry callback that handles "client session disconnect" event.
*/
static void tm_session_disconnect(const PSI_thread_attrs * /*thread_attrs*/) {
  MYSQL_THD thd = nullptr;
  if (current_thd_srv->get(&thd) || thd == nullptr) {
    g_log.write(" tm_session_disconnect: failed to get current THD\n");
    return;
  }
  MYSQL_LEX_CSTRING user{};
  if (get_user(thd, user)) {
    g_log.write(" tm_session_disconnect: failed to get user name\n");
  }
  g_log.write(" tm_session_disconnect: client session ended (user=%s)\n",
              user.str);
}

static void tm_session_change_user(const PSI_thread_attrs * /*thread_attrs*/) {}

static PSI_notification_v3 tm_notif = {
    tm_thread_create, tm_thread_destroy, tm_session_connect,
    tm_session_disconnect, tm_session_change_user};

static int tm_notification_handle = 0;

static bool register_notification_callback() {
  tm_notification_handle =
      notification_srv->register_notification(&tm_notif, true);
  const bool failure = (tm_notification_handle == 0);
  return failure;
}

static void unregister_notification_callback() {
  notification_srv->unregister_notification(tm_notification_handle);
  tm_notification_handle = 0;
}

/**
  Server telemetry callback that handles "telemetry session created" event:
  - create new Session_data object and assign it to the current THD
  - increment telemetry session reference count
*/
static telemetry_session_t *tm_session_create() {
  MYSQL_THD thd = nullptr;
  if (current_thd_srv->get(&thd) || thd == nullptr) {
    g_log.write("> tm_session_create: failed to get current THD\n");
    return nullptr;
  }
  MYSQL_LEX_CSTRING user{};
  if (get_user(thd, user)) {
    g_log.write(" tm_session_create: failed to get user name\n");
  }

  Session_data *data = Session_data::create();
  Session_data::set(thd, data, g_log);

  const int64_t valueNew = ++g_RefCount;
  assert(valueNew > 0);

  g_log.write(
      " tm_session_create: telemetry session started, increase refcount by "
      "user=%s to %" PRId64 "\n",
      user.str, valueNew);

  auto *session = reinterpret_cast<telemetry_session_t *>(data);
  return session;
}

/**
  Server telemetry callback that handles "telemetry session destroyed" event:
  - decrement telemetry session reference count
  - destruct Session_data object assigned to the current THD
*/
static void tm_session_destroy(telemetry_session_t *session) {
  // log before the refcount decrement to prevent random log ordering in tests
  // (component deinit loop could sometimes write its log before this one)
  g_log.write(
      " tm_session_destroy: telemetry session ended, decrease refcount to "
      "%" PRId64 "\n",
      g_RefCount.load() - 1);

  const int64_t valueNew [[maybe_unused]] = --g_RefCount;
  assert(valueNew >= 0);

  auto *data = reinterpret_cast<Session_data *>(session);

  MYSQL_THD thd = nullptr;
  if (current_thd_srv->get(&thd) || thd == nullptr) {
    g_log.write(" tm_session_destroy: failed to get current THD\n");
    return;
  }

#ifndef NDEBUG
  const Session_data *data1 = Session_data::get(thd);
  assert(data == data1);
#endif

  Session_data::set(thd, nullptr, g_log);
  if (data != nullptr) {
    Session_data::destroy(data);
  }
}

/**
  Server telemetry callback that handles "statement start" event:
  - filter statement by user name: discard statements by 'internal'
  - demonstrate PS instrument forcing: discard statements by users other than
  'root'/'api' unless "events_statements_current" is configured
  - create and push new Statement_Data object to the statement stack for the
  current THD
*/
static telemetry_locker_t *tm_stmt_start(telemetry_session_t * /* session */,
                                         uint64_t *flags) {
  MYSQL_THD thd = nullptr;
  if (current_thd_srv->get(&thd) || thd == nullptr) {
    g_log.write("> tm_stmt_start: failed to get current THD\n");
    *flags = TRACE_NOTHING;
    return nullptr;
  }
  MYSQL_LEX_CSTRING user{};
  if (get_user(thd, user)) {
    g_log.write(" tm_stmt_start: failed to get user name\n");
  }

  // Custom tracing strategy example:
  // never trace statements from user "internal".
  if (user.length && 0 == strcmp(user.str, "internal")) {
    g_log.write(
        "> tm_stmt_start: skip tracing from user "
        "'internal'\n");
    *flags = TRACE_NOTHING;
    return nullptr;
  }

  char query[2048] = "";
  if (get_query(thd, query, sizeof(query))) {
    g_log.write(" tm_stmt_start: failed to get query text\n");
  }
  char host[1024] = "";
  if (get_host_or_ip(thd, host, sizeof(host))) {
    g_log.write(" tm_stmt_start: failed to get host info\n");
  }
  char schema[1024] = "";
  if (get_schema(thd, schema, sizeof(schema))) {
    g_log.write(" tm_stmt_start: failed to get schema info\n");
  }

  Session_data *data = Session_data::get(thd);
  if (data == nullptr) {
    data = Session_data::create();
    Session_data::set(thd, data, g_log);
  } else {
    if (!data->m_stmt_stack.empty()) {
      const Statement_Data &root = data->m_stmt_stack[0];
      if (!root.traced) {
        g_log.write(
            "> tm_stmt_start: discard substatement (user=%s, host=%s, db=%s, "
            "query='%s'), its root "
            "statement "
            "will be filtered out\n",
            user.str, host, schema, query);
        *flags = TRACE_NOTHING;
        return nullptr;
      }
    }
  }

  const bool ps_instrument_enabled = (*flags == TRACE_STATEMENTS);

  // Custom tracing strategy example:
  // - force tracing statements from user "api" or "root" (used in tests)
  // - other users requests will not be forced (trace only if PS enabled the
  // instrument)
  if (ps_instrument_enabled ||
      (user.length &&
       (0 == strcmp(user.str, "api") || 0 == strcmp(user.str, "root")))) {
    *flags = TRACE_STATEMENTS;

    Statement_Data info;
    data->m_stmt_stack.push_back(std::move(info));

  } else {
    *flags = TRACE_NOTHING;

    g_log.write(
        "> tm_stmt_start: discard statement (user=%s, host=%s, db=%s, "
        "query='%s'), "
        "statement "
        "will not be forced\n",
        user.str, host, schema, query);

    return nullptr;
  }

  g_log.write(
      "> tm_stmt_start: proceed further (depth=%lu, user=%s, host=%s, db=%s, "
      "query='%s')\n",
      data->stmt_stack_depth(), user.str, host, schema, query);
  auto *locker = reinterpret_cast<telemetry_locker_t *>(data);
  return locker;
}

/**
  Server telemetry callback that handles "statement got query attributes
  assigned" event:
  - filter statement by query attributes: discard statements not having
  "trace=on" attribute attached
  - read query attributes to fetch application and call-site context information
*/
static telemetry_locker_t *tm_stmt_notify_qa(telemetry_locker_t *locker,
                                             bool with_query_attributes,
                                             uint64_t *flags) {
  auto *data = reinterpret_cast<Session_data *>(locker);
  if (data == nullptr) {
    assert(false);
    *flags = TRACE_NOTHING;
    return nullptr;
  }

  MYSQL_THD thd = nullptr;
  if (current_thd_srv->get(&thd) || thd == nullptr) {
    g_log.write("tm_stmt_notify_qa: failed to get current THD\n");
    data->discard_stmt();
    *flags = TRACE_NOTHING;
    return nullptr;
  }
  MYSQL_LEX_CSTRING user{};
  if (get_user(thd, user)) {
    g_log.write(" tm_stmt_notify_qa: failed to get user name\n");
  }

  char query[2048] = "";
  if (get_query(thd, query, sizeof(query))) {
    g_log.write(" tm_stmt_notify_qa: failed to get query text\n");
  }

  // Only top level statement may have query attributes.
  assert(!with_query_attributes || data->m_stmt_stack.size() == 1);

  // Ignore top-level statement without any qa attached.
  if (!with_query_attributes && data->m_stmt_stack.size() == 1) {
    g_log.write(
        " > tm_stmt_notify_qa: skip tracing, no qa (depth=%lu, with_qa=%d, "
        "user=%s, "
        "query='%s')\n",
        data->stmt_stack_depth(), with_query_attributes, user.str, query);
    data->discard_stmt();
    *flags = TRACE_NOTHING;
    return nullptr;
  }

  // "trace: on" query attribute existence is a prerequisite for tracing.
  std::string value;
  if (query_attr_read(thd, trace_key_value, value, g_log)) {
    g_log.write(
        " > tm_stmt_notify_qa: skip tracing, no attribute '%s' (depth=%lu, "
        "with_qa=%d, "
        "user=%s, query='%s')\n",
        trace_key_value, data->stmt_stack_depth(), with_query_attributes,
        user.str, query);
    data->discard_stmt();
    *flags = TRACE_NOTHING;
    return nullptr;
  }
  if (value != "on") {
    g_log.write(
        " > tm_stmt_notify_qa: skip tracing, attribute '%s'="
        "'%s' (depth=%lu, with_qa=%d, user=%s, query='%s')\n",
        trace_key_value, value.c_str(), data->stmt_stack_depth(),
        with_query_attributes, user.str, query);
    data->discard_stmt();
    *flags = TRACE_NOTHING;
    return nullptr;
  }

  assert(!data->m_stmt_stack.empty());
  Statement_Data &info = data->m_stmt_stack.back();

  // Fetch custom app/callsite contexts from query attributes as JSON.
  if (!g_tags_app_ctx.empty() &&
      query_attrs_to_json(thd, g_tags_app_ctx, info.app_ctx, g_log)) {
    g_log.write(" > tm_stmt_notify_qa: error fetching application context\n");
    data->discard_stmt();
    *flags = TRACE_NOTHING;
    return nullptr;
  }
  if (!g_tags_call_ctx.empty() &&
      query_attrs_to_json(thd, g_tags_call_ctx, info.call_ctx, g_log)) {
    g_log.write(" > tm_stmt_notify_qa: error fetching callsite context\n");
    data->discard_stmt();
    *flags = TRACE_NOTHING;
    return nullptr;
  }

  // No need to modify 'flags' parameter here, we don't plan to reduce
  // the scope we already set within the tm_stmt_start.

  info.traced = true;
  g_log.write(
      "> tm_stmt_notify_qa: proceed further (depth=%lu, with_qa=%d, user=%s, "
      "query='%s', "
      "app[%s], "
      "call[%s])\n",
      data->stmt_stack_depth(), with_query_attributes, user.str, query,
      info.app_ctx.c_str(), info.call_ctx.c_str());
  return locker;
}

/**
  Server telemetry callback that handles "statement tracing got aborted" event:
  - discard matching statement info data
*/
static void tm_stmt_abort(telemetry_locker_t *locker) {
  auto *data = reinterpret_cast<Session_data *>(locker);
  if (data == nullptr) {
    assert(false);
    return;
  }

  MYSQL_THD thd = nullptr;
  if (current_thd_srv->get(&thd) || thd == nullptr) {
    g_log.write("> tm_stmt_abort: failed to get current THD\n");
    return;
  }
  MYSQL_LEX_CSTRING user{};
  if (get_user(thd, user)) {
    g_log.write(" tm_stmt_abort: failed to get user name\n");
  }

  char query[2048] = "";
  if (get_query(thd, query, sizeof(query))) {
    g_log.write(" tm_stmt_abort: failed to get query text\n");
  }

  assert(!data->m_stmt_stack.empty());
  const Statement_Data &info = data->m_stmt_stack.back();

  g_log.write(
      "> tm_stmt_abort: abort statement (depth=%lu, "
      "user=%s, query='%s', "
      "app[%s], "
      "call[%s])\n",
      data->stmt_stack_depth(), user.str, query, info.app_ctx.c_str(),
      info.call_ctx.c_str());
  data->discard_stmt();
}

/**
  Server telemetry callback that handles "statement ends" event:
  - discard matching statement info data
*/
static void tm_stmt_end(telemetry_locker_t *locker,
                        telemetry_stmt_data_t *stmt_data) {
  auto *data = reinterpret_cast<Session_data *>(locker);
  if (data == nullptr) {
    assert(false);
    return;
  }

  MYSQL_THD thd = nullptr;
  if (current_thd_srv->get(&thd) || thd == nullptr) {
    g_log.write("> tm_stmt_end: failed to get current THD\n");
    return;
  }
  MYSQL_LEX_CSTRING user{};
  if (get_user(thd, user)) {
    g_log.write(" tm_stmt_end: failed to get user name\n");
  }

  char query[2048] = "";
  if (get_query(thd, query, sizeof(query))) {
    g_log.write(" tm_stmt_end: failed to get query text\n");
  }

  assert(!data->m_stmt_stack.empty());
  const Statement_Data &info = data->m_stmt_stack.back();

  // if this was a root statement, traced flag would be set
  // in tm_stmt_notify_qa handler (unless discarded)
  if (data->m_stmt_stack.size() == 1 && !info.traced) {
    g_log.write(
        "> tm_stmt_end: discard substatement, root discarded (depth=%lu, "
        "user=%s, query='%s', query1='%.*s', digest='%s', "
        "app[%s], "
        "call[%s])\n",
        data->stmt_stack_depth(), user.str, query,
        (int)stmt_data->m_sql_text_length, stmt_data->m_sql_text,
        stmt_data->m_digest_text, info.app_ctx.c_str(), info.call_ctx.c_str());

  } else {
    g_log.write(
        "> tm_stmt_end: trace statement (depth=%lu, user=%s, query='%s', "
        "query1='%.*s', digest='%s', app[%s], "
        "call[%s])\n",
        data->stmt_stack_depth(), user.str, query,
        (int)stmt_data->m_sql_text_length, stmt_data->m_sql_text,
        stmt_data->m_digest_text, info.app_ctx.c_str(), info.call_ctx.c_str());
  }

  data->discard_stmt();
}

static telemetry_t tm_callback = {tm_session_create, tm_session_destroy,
                                  tm_stmt_start,     tm_stmt_notify_qa,
                                  tm_stmt_abort,     tm_stmt_end};

static bool register_telemetry_callback() {
  return telemetry_v1_srv->register_telemetry(&tm_callback);
}

static bool unregister_telemetry_callback() {
  const bool failure = telemetry_v1_srv->unregister_telemetry(&tm_callback);
  return failure;
}

/**
  Abort the current session.
  Terminate statement/session for "UNINSTALL COMPONENT ..."
  that triggered component de-init.
*/
static void abort_current_session() {
  MYSQL_THD thd = nullptr;
  bool failure;

  failure = current_thd_srv->get(&thd);
  if (failure) {
    g_log.write("abort_current_session: failed to get current session");
    return;
  }

  if (thd == nullptr) {
    // Nothing to abort
    return;
  }

  telemetry_v1_srv->abort_telemetry(thd);
}

/**
 *  Initialize the test_server_telemetry_traces component at server start or
 *  component installation:
 *
 *    - Register system variables
 *    - Register status variables
 *    - Register telemetry per-session data slot
 *    - Register telemetry session callbacks
 *    - Register telemetry statement callbacks
 *
 *  @retval 0  success
 *  @retval non-zero   failure
 */
mysql_service_status_t test_server_telemetry_traces_component_init() {
  mysql_service_status_t result = 0;

  g_log.write("test_server_telemetry_traces_component_init init:\n");

  if (register_system_variables()) {
    g_log.write("Error returned from register_system_variables()\n");
    result = true;
    goto error;
  }
  g_log.write(" - System variables registered.\n");

  if (register_status_variables()) {
    g_log.write("Error returned from register_status_variables()\n");
    unregister_system_variables();
    result = true;
    goto error;
  }
  g_log.write(" - Status variables registered.\n");

  if (register_server_telemetry_slot(g_log)) {
    g_log.write("Error returned from register_server_telemetry_slot()\n");
    unregister_system_variables();
    unregister_status_variables();
    result = true;
    goto error;
  }
  g_log.write(" - Telemetry per-session data slot registered.\n");

  if (register_notification_callback()) {
    g_log.write("Error returned from register_notification_callback()\n");
    unregister_system_variables();
    unregister_status_variables();
    unregister_server_telemetry_slot(g_log);
    result = true;
    goto error;
  }
  g_log.write(" - Telemetry session callbacks registered.\n");

  if (register_telemetry_callback()) {
    g_log.write("Error returned from register_telemetry_callback()\n");
    unregister_system_variables();
    unregister_status_variables();
    unregister_server_telemetry_slot(g_log);
    unregister_notification_callback();
    result = true;
    goto error;
  }
  g_log.write(" - Telemetry statement callbacks registered.\n");

error:
  g_log.write("End of init\n");
  return result;
}

/**
 *  Terminate the test_server_telemetry_traces_component at server shutdown or
 *  component deinstallation:
 *
 *   - Unregister telemetry statement callbacks
 *   - Wait for telemetry sessions handled by this component to end
 *   - Unregister telemetry session callbacks
 *   - Unregister telemetry per-session data slot
 *   - Unregister status variables
 *   - Unregister system variables
 *
 *  @retval 0  success
 *  @retval non-zero   failure
 */
mysql_service_status_t test_server_telemetry_traces_component_deinit() {
  g_log.write("test_server_telemetry_traces_component_deinit:\n");

  unregister_telemetry_callback();
  g_log.write(" - Telemetry statement callbacks unregistered.\n");

  abort_current_session();
  g_log.write(" - Current session aborted.\n");

  // Wait (without timeout) for existing session refcount to drop to 0
  // We need to make sure the unload code is
  // not waiting on itself, since the unload statement is recorded in telemetry.
  // That's why we call abort_telemetry within the previous unregister step.
  static constexpr int wait_time_usec = 10;
  while (g_RefCount.load() > 0) {
    std::this_thread::sleep_for(std::chrono::microseconds(wait_time_usec));
  }
  g_log.write(
      " - Done waiting for telemetry sessions to finish (refcount = %" PRId64
      ").\n",
      g_RefCount.load());

  unregister_notification_callback();
  g_log.write(" - Telemetry session callbacks unregistered.\n");

  unregister_server_telemetry_slot(g_log);
  g_log.write(" - Telemetry per-session data slot unregistered.\n");

  unregister_status_variables();
  g_log.write(" - Status variables unregistered.\n");

  unregister_system_variables();
  g_log.write(" - System variables unregistered.\n");

  g_log.write("End of deinit\n");
  return 0;
}

/* test_server_telemetry_traces component doesn't provide any service */
BEGIN_COMPONENT_PROVIDES(test_server_telemetry_traces)
END_COMPONENT_PROVIDES();

BEGIN_COMPONENT_REQUIRES(test_server_telemetry_traces)
REQUIRES_SERVICE_AS(mysql_server_telemetry_traces_v1, telemetry_v1_srv),
    REQUIRES_SERVICE_AS(mysql_query_attributes_iterator, qa_iterator_srv),
    REQUIRES_SERVICE_AS(mysql_query_attribute_string, qa_string_srv),
    REQUIRES_SERVICE_AS(mysql_query_attribute_isnull, qa_isnull_srv),
    REQUIRES_SERVICE_AS(mysql_string_converter, string_converter_srv),
    REQUIRES_SERVICE_AS(mysql_string_charset_converter, charset_converter_srv),
    REQUIRES_SERVICE_AS(mysql_charset, charset_srv),
    REQUIRES_SERVICE_AS(mysql_string_factory, string_factory_srv),
    REQUIRES_SERVICE_AS(component_sys_variable_register, sysvar_register_srv),
    REQUIRES_SERVICE_AS(component_sys_variable_unregister,
                        sysvar_unregister_srv),
    REQUIRES_SERVICE_AS(mysql_thd_attributes, thd_attributes_srv),
    REQUIRES_SERVICE_AS(mysql_current_thread_reader, current_thd_srv),
    REQUIRES_SERVICE_AS(mysql_thd_store, thd_store_srv),
    REQUIRES_SERVICE_AS(mysql_thd_security_context, thd_scx_srv),
    REQUIRES_SERVICE_AS(mysql_security_context_options, scx_options_srv),
    REQUIRES_SERVICE_AS(pfs_notification_v3, notification_srv),
    REQUIRES_SERVICE_AS(status_variable_registration, statvar_register_srv),
    END_COMPONENT_REQUIRES();

/* A list of metadata to describe the Component. */
BEGIN_COMPONENT_METADATA(test_server_telemetry_traces)
METADATA("mysql.author", "Oracle Corporation"),
    METADATA("mysql.license", "GPL"), METADATA("test_property", "1"),
    END_COMPONENT_METADATA();

/* Declaration of the Component. */
DECLARE_COMPONENT(test_server_telemetry_traces,
                  "mysql:test_server_telemetry_traces")
test_server_telemetry_traces_component_init,
    test_server_telemetry_traces_component_deinit END_DECLARE_COMPONENT();

/* Defines list of Components contained in this library. Note that for now
  we assume that library will have exactly one Component. */
DECLARE_LIBRARY_COMPONENTS &COMPONENT_REF(test_server_telemetry_traces)
    END_DECLARE_LIBRARY_COMPONENTS
