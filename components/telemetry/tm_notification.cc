/*
  Copyright (c) 2022, 2026, Oracle and/or its affiliates.

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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "tm_notification.h"
#include "tm_global.h"
#include "tm_log.h"
#include "tm_mysql_metric.h"
#include "tm_propagation.h"
#include "tm_psi.h"
#include "tm_required_services.h"
#include "tm_slot.h"
#include "tm_system_variables.h"

#include <chrono>
#include <map>

#include "opentelemetry/logs/log_record.h"
#include "opentelemetry/metrics/async_instruments.h"
#include "opentelemetry/metrics/observer_result.h"

namespace nostd = opentelemetry::nostd;

namespace telemetry {

static const nostd::string_view k_session("session");
static const nostd::string_view k_stmt("stmt");

static const nostd::string_view k_processlist_id("mysql.processlist_id");
static const nostd::string_view k_thread_id("mysql.thread_id");
static const nostd::string_view k_user("mysql.user");
static const nostd::string_view k_host("mysql.host");
static const nostd::string_view k_group("mysql.group");
static const nostd::string_view k_session_attr_prefix("mysql.session_attr.");

static const nostd::string_view k_event_name("mysql.event_name");
static const nostd::string_view k_lock_time("mysql.lock_time");
static const nostd::string_view k_sql_text("mysql.sql_text");
static const nostd::string_view k_digest_text("mysql.digest_text");
static const nostd::string_view k_current_schema("mysql.current_schema");
static const nostd::string_view k_object_type("mysql.object_type");
static const nostd::string_view k_object_schema("mysql.object_schema");
static const nostd::string_view k_object_name("mysql.object_name");
static const nostd::string_view k_sql_errno("mysql.sql_errno");
static const nostd::string_view k_sqlstate("mysql.sqlstate");

static const nostd::string_view k_message_text("mysql.message_text");
static const nostd::string_view k_error_count("mysql.error_count");
static const nostd::string_view k_warning_count("mysql.warning_count");

static const nostd::string_view k_rows_affected("mysql.rows_affected");
static const nostd::string_view k_rows_sent("mysql.rows_sent");
static const nostd::string_view k_rows_examined("mysql.rows_examined");
static const nostd::string_view k_created_tmp_disk_tables(
    "mysql.created_tmp_disk_tables");
static const nostd::string_view k_created_tmp_tables(
    "mysql.created_tmp_tables");
static const nostd::string_view k_select_full_join("mysql.select_full_join");
static const nostd::string_view k_select_full_range_join(
    "mysql.select_full_range_join");
static const nostd::string_view k_select_range("mysql.select_range");
static const nostd::string_view k_select_range_check(
    "mysql.select_range_check");
static const nostd::string_view k_select_scan("mysql.select_scan");
static const nostd::string_view k_sort_merge_passes("mysql.sort_merge_passes");
static const nostd::string_view k_sort_range("mysql.sort_range");
static const nostd::string_view k_sort_rows("mysql.sort_rows");
static const nostd::string_view k_sort_scan("mysql.sort_scan");
static const nostd::string_view k_no_index_used("mysql.no_index_used");
static const nostd::string_view k_no_good_index_used(
    "mysql.no_good_index_used");
static const nostd::string_view k_max_controlled_memory(
    "mysql.max_controlled_memory");
static const nostd::string_view k_max_total_memory("mysql.max_total_memory");
static const nostd::string_view k_cpu_time("mysql.cpu_time");

/*
  For debug only, do not use in production.
*/
// #define DEBUG_SESSION_COUNT

#ifdef DEBUG_SESSION_COUNT
void debug_inc_session_count(const char *reason, THD *thd,
                             const PSI_thread_attrs *thread_attrs) {
  if (thread_attrs != nullptr) {
    (void)fprintf(stderr,
                  "inc_session_count(%s) g_session_count = %ld, thd = %p, "
                  "processlist_id = %ld\n",
                  reason, g_session_count.load(), thd,
                  thread_attrs->m_processlist_id);
  } else {
    (void)fprintf(stderr,
                  "inc_session_count(%s) g_session_count = %ld, thd = %p\n",
                  reason, g_session_count.load(), thd);
  }
}
#endif /* DEBUG_SESSION_COUNT */

#ifdef DEBUG_SESSION_COUNT
void debug_dec_session_count(const char *reason, THD *thd,
                             const PSI_thread_attrs *thread_attrs) {
  if (thread_attrs != nullptr) {
    (void)fprintf(stderr,
                  "dec_session_count(%s) g_session_count = %ld, thd = %p, "
                  "processlist_id = %ld\n",
                  reason, g_session_count.load(), thd,
                  thread_attrs->m_processlist_id);
  } else {
    (void)fprintf(stderr,
                  "dec_session_count(%s) g_session_count = %ld, thd = %p\n",
                  reason, g_session_count.load(), thd);
  }
}
#endif /* DEBUG_SESSION_COUNT */

void inc_session_count() { g_session_count++; }

void dec_session_count() {
  g_session_count--;

  const bool down = g_shutting_down.load();

  if (down) {
    g_sessions_closed++;

    /*
      The thread performing UNINSTALL COMPONENT is waiting
      for sessions to disconnect.
      Notify to wake up UNINSTALL COMPONENT.
    */
    mutex_srv->lock(&g_session_notify_mutex, __FILE__, __LINE__);
    cond_srv->signal(&g_session_notify_cond, __FILE__, __LINE__);
    mutex_srv->unlock(&g_session_notify_mutex, __FILE__, __LINE__);
  }
}

static int64_t big_attribute(unsigned long long value) {
  // TODO: opentelemetry-cpp data truncation
  // - opentelemetry-specification does support int64_t
  // - opentelemetry-specification does not support uint64_t
  // - opentelemetry-cpp supports uint64_t, but as an extension to the spec
  // If using uint64_t here, downstream systems may break.
  // Do not pass size_t to SetAttribute(), make EXPLICIT type conversion.

  if (value >
      static_cast<unsigned long long>(std::numeric_limits<int64_t>::max())) {
    return std::numeric_limits<int64_t>::max();
  }

  return static_cast<int64_t>(value);
}

void set_span_thread_attr(
    const opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span>
        &otel_span,
    const PSI_thread_attrs *thread_attrs) {
  int64_t big_value = 0;

  big_value = big_attribute(thread_attrs->m_processlist_id);
  otel_span->SetAttribute(k_processlist_id.data(), big_value);

  big_value = big_attribute(thread_attrs->m_thread_internal_id);
  otel_span->SetAttribute(k_thread_id.data(), big_value);

  if (thread_attrs->m_username_length > 0) {
    std::string user(thread_attrs->m_username, thread_attrs->m_username_length);
    otel_span->SetAttribute(k_user, user);
  }

  if (thread_attrs->m_hostname_length > 0) {
    std::string host(thread_attrs->m_hostname, thread_attrs->m_hostname_length);
    otel_span->SetAttribute(k_host, host);
  }

  if (thread_attrs->m_groupname_length > 0) {
    std::string group(thread_attrs->m_groupname,
                      thread_attrs->m_groupname_length);
    otel_span->SetAttribute(k_group, group);
  }
}

void set_span_session_attr(
    const opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span>
        &otel_span,
    MYSQL_THD thd) {
  my_h_connection_attributes_iterator con_attr_it = nullptr;

  const int rc = con_attr_srv->init(thd, &con_attr_it);
  if (rc == 0) {
    MYSQL_LEX_CSTRING name{};
    MYSQL_LEX_CSTRING value{};
    const char *charset_string = nullptr;
    std::string attr_name;

    while (con_attr_srv->get(thd, &con_attr_it, &name.str, &name.length,
                             &value.str, &value.length, &charset_string) == 0) {
      attr_name = k_session_attr_prefix.data();
      attr_name.append(name.str, name.length);
      std::string attr_value(value.str, value.length);

      otel_span->SetAttribute(attr_name, attr_value);
    }

    con_attr_srv->deinit(con_attr_it);
  }
}

void set_span_stmt_aborted(
    const opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span>
        &otel_span) {
  static const std::string event_name("sql/statement/unknown");
  otel_span->SetAttribute(k_event_name, event_name);

  static const std::string why("Instrumentation aborted, telemetry shutdown");
  otel_span->SetAttribute(k_message_text, why);

  otel_span->SetStatus(opentelemetry::trace::StatusCode::kUnset);
}

void set_span_stmt_data(
    const opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span>
        &otel_span,
    telemetry_stmt_data_t *stmt_data) {
  int64_t big_value = 0;

  otel_span->SetAttribute(k_event_name, stmt_data->m_event_name);

  big_value = big_attribute(stmt_data->m_lock_time);
  otel_span->SetAttribute(k_lock_time, big_value);

  if (sv_query_text_enabled) {
    std::string sql_text(stmt_data->m_sql_text, stmt_data->m_sql_text_length);
    otel_span->SetAttribute(k_sql_text, sql_text);
  }

  if (stmt_data->m_digest_text != nullptr) {
    otel_span->SetAttribute(k_digest_text, stmt_data->m_digest_text);
  }

  if (stmt_data->m_current_schema != nullptr) {
    std::string current_schema(stmt_data->m_current_schema,
                               stmt_data->m_current_schema_length);
    otel_span->SetAttribute(k_current_schema, current_schema);
  }

  if (stmt_data->m_object_type != nullptr) {
    std::string object_type(stmt_data->m_object_type,
                            stmt_data->m_object_type_length);
    otel_span->SetAttribute(k_object_type, object_type);
  }

  if (stmt_data->m_object_schema != nullptr) {
    std::string object_schema(stmt_data->m_object_schema,
                              stmt_data->m_object_schema_length);
    otel_span->SetAttribute(k_object_schema, object_schema);
  }

  if (stmt_data->m_object_name != nullptr) {
    std::string object_name(stmt_data->m_object_name,
                            stmt_data->m_object_name_length);
    otel_span->SetAttribute(k_object_name, object_name);
  }

  big_value = big_attribute(stmt_data->m_sql_errno);
  otel_span->SetAttribute(k_sql_errno, big_value);

  otel_span->SetAttribute(k_sqlstate, stmt_data->m_sqlstate);

  otel_span->SetAttribute(k_message_text, stmt_data->m_message_text);

  big_value = big_attribute(stmt_data->m_error_count);
  otel_span->SetAttribute(k_error_count, big_value);

  big_value = big_attribute(stmt_data->m_warning_count);
  otel_span->SetAttribute(k_warning_count, big_value);

  if (stmt_data->m_error_count == 0) {
    otel_span->SetStatus(opentelemetry::trace::StatusCode::kOk);
  } else {
    otel_span->SetStatus(opentelemetry::trace::StatusCode::kError);
  }

  big_value = big_attribute(stmt_data->m_rows_affected);
  otel_span->SetAttribute(k_rows_affected, big_value);

  big_value = big_attribute(stmt_data->m_rows_sent);
  otel_span->SetAttribute(k_rows_sent, big_value);

  big_value = big_attribute(stmt_data->m_rows_examined);
  otel_span->SetAttribute(k_rows_examined, big_value);

  big_value = big_attribute(stmt_data->m_created_tmp_disk_tables);
  otel_span->SetAttribute(k_created_tmp_disk_tables, big_value);

  big_value = big_attribute(stmt_data->m_created_tmp_tables);
  otel_span->SetAttribute(k_created_tmp_tables, big_value);

  big_value = big_attribute(stmt_data->m_select_full_join);
  otel_span->SetAttribute(k_select_full_join, big_value);

  big_value = big_attribute(stmt_data->m_select_full_range_join);
  otel_span->SetAttribute(k_select_full_range_join, big_value);

  big_value = big_attribute(stmt_data->m_select_range);
  otel_span->SetAttribute(k_select_range, big_value);

  big_value = big_attribute(stmt_data->m_select_range_check);
  otel_span->SetAttribute(k_select_range_check, big_value);

  big_value = big_attribute(stmt_data->m_select_scan);
  otel_span->SetAttribute(k_select_scan, big_value);

  big_value = big_attribute(stmt_data->m_sort_merge_passes);
  otel_span->SetAttribute(k_sort_merge_passes, big_value);

  big_value = big_attribute(stmt_data->m_sort_range);
  otel_span->SetAttribute(k_sort_range, big_value);

  big_value = big_attribute(stmt_data->m_sort_rows);
  otel_span->SetAttribute(k_sort_rows, big_value);

  big_value = big_attribute(stmt_data->m_sort_scan);
  otel_span->SetAttribute(k_sort_scan, big_value);

  big_value = big_attribute(stmt_data->m_no_index_used);
  otel_span->SetAttribute(k_no_index_used, big_value);

  big_value = big_attribute(stmt_data->m_no_good_index_used);
  otel_span->SetAttribute(k_no_good_index_used, big_value);

  big_value = big_attribute(stmt_data->m_max_controlled_memory);
  otel_span->SetAttribute(k_max_controlled_memory, big_value);

  big_value = big_attribute(stmt_data->m_max_total_memory);
  otel_span->SetAttribute(k_max_total_memory, big_value);

  big_value = big_attribute(stmt_data->m_cpu_time);
  otel_span->SetAttribute(k_cpu_time, big_value);
}

void tm_thread_create(const PSI_thread_attrs * /* thread_attrs */) {}

void tm_thread_destroy(const PSI_thread_attrs * /* thread_attrs */) {}

void tm_session_connect(const PSI_thread_attrs *thread_attrs) {
  if (g_shutting_down.load()) {
    return;
  }

  MYSQL_THD thd = nullptr;

  const int rc = current_thd_srv->get(&thd);

  if (rc == 0) {
    Session_data *data = Session_data::create(thd, sv_trace_enabled);

    if (data != nullptr) {
      if (data->m_trace && (data->m_session_tracer != nullptr)) {
        opentelemetry::trace::StartSpanOptions options;
        options.kind = opentelemetry::trace::SpanKind::kInternal;

        opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> otel_span =
            data->m_session_tracer->StartSpan(k_session, options);

        set_span_thread_attr(otel_span, thread_attrs);

        set_span_session_attr(otel_span, thd);

        data->m_session_span = std::move(otel_span);
      }

      Session_data::set(thd, data);
      inc_session_count();
#ifdef DEBUG_SESSION_COUNT
      debug_inc_session_count("tm_session_connect", thd, thread_attrs);
#endif /* DEBUG_SESSION_COUNT */
    }
  }
}

void tm_session_disconnect(const PSI_thread_attrs *thread_attrs
                           [[maybe_unused]]) {
  MYSQL_THD thd = nullptr;

  const int rc = current_thd_srv->get(&thd);

  if (rc == 0) {
    Session_data *data = Session_data::get(thd);

    if (data != nullptr) {
      Session_data::set(thd, nullptr);
      data->close();
      if (!data->m_used_in_telemetry) {
        Session_data::destroy(data);
        dec_session_count();
#ifdef DEBUG_SESSION_COUNT
        debug_dec_session_count("tm_session_disconnect", thd, thread_attrs);
#endif /* DEBUG_SESSION_COUNT */
      }
    }
  }
}

void tm_session_change_user(const PSI_thread_attrs * /* thread_attrs */) {}

static bool notification_callback_is_registered = false;

static PSI_notification_v3 tm_notif = {
    tm_thread_create, tm_thread_destroy, tm_session_connect,
    tm_session_disconnect, tm_session_change_user};

static int tm_notification_handle = 0;

int register_notification_callback() {
  assert(!notification_callback_is_registered);
  int rc = 0;
  tm_notification_handle =
      notification_srv->register_notification(&tm_notif, true);
  if (tm_notification_handle == 0) {
    log_error("%s: Failed to register notification callback.", component_name);
    rc = 1;
  } else {
    notification_callback_is_registered = true;
  }
  return rc;
}

void unregister_notification_callback() {
  if (notification_callback_is_registered) {
    const int rc =
        notification_srv->unregister_notification(tm_notification_handle);
    if (rc != 0) {
      log_error("%s: Failed to unregister notification callback, rc %d.",
                component_name, rc);
    }
    notification_callback_is_registered = false;
    tm_notification_handle = 0;
  }
}

telemetry_session_t *tm_session_create() {
  MYSQL_THD thd = nullptr;

  const int rc = current_thd_srv->get(&thd);

  if (rc != 0) {
    return nullptr;
  }

  assert(thd != nullptr);

  Session_data *data = Session_data::get(thd);
  if (data == nullptr) {
    data = Session_data::create(thd, sv_trace_enabled);

    if (data != nullptr) {
      Session_data::set(thd, data);
      inc_session_count();
#ifdef DEBUG_SESSION_COUNT
      debug_inc_session_count("tm_session_create", thd, nullptr);
#endif /* DEBUG_SESSION_COUNT */
    } else {
      return nullptr;
    }
  }

  assert(data->m_depth == 0);
  assert(!data->m_used_in_telemetry);
  data->m_used_in_telemetry = true;
  data->m_trace = sv_trace_enabled;
  auto *session = reinterpret_cast<telemetry_session_t *>(data);
  return session;
}

void tm_session_destroy(telemetry_session_t *session) {
  auto *data = reinterpret_cast<Session_data *>(session);
  assert(data != nullptr);
  assert(data->m_depth == 0);
  THD *thd = data->m_thd;

  assert(thd != nullptr);
  assert(data->m_used_in_telemetry);
  data->m_used_in_telemetry = false;
  Session_data::set(thd, nullptr);
  data->close();
  Session_data::destroy(data);
  dec_session_count();
#ifdef DEBUG_SESSION_COUNT
  debug_dec_session_count("tm_session_destroy", thd, nullptr);
#endif /* DEBUG_SESSION_COUNT */
}

telemetry_locker_t *tm_stmt_start(telemetry_session_t *session,
                                  uint64_t *flags) {
  auto *data = reinterpret_cast<Session_data *>(session);

  assert(data != nullptr);

  if (data->m_depth == 0) {
    /* Top level statement. */
    data->m_depth++;
    /* Delay span creation on query attributes. */
    data->m_query_attributes_seen = false;
    /* Re capture the trace flag at every top level statement */
    data->m_trace = sv_trace_enabled;
  } else {
    if (!data->m_query_attributes_seen) {
      /*
        Under normal conditions (no degraded instrumentation),
        the SQL layer should notify attributes properly.
        Please fix the root cause if this fails.
      */
      assert(false);
      /*
        Under stress or with incomplete instrumentation,
        ignore this (malformed) statement entirely.
      */
      return nullptr;
    }

    /* Nested statement. */
    data->m_depth++;

    if (data->m_trace && (data->m_session_tracer != nullptr)) {
      if (!data->m_stmt_stack.empty()) {
        opentelemetry::trace::StartSpanOptions options;
        options.kind = opentelemetry::trace::SpanKind::kInternal;
        options.parent = data->m_stmt_stack.back()->GetContext();
        opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> otel_span;

        otel_span = data->m_session_tracer->StartSpan(k_stmt, options);
        data->m_stmt_stack.push_back(std::move(otel_span));
      }
    }
  }

  auto *locker = reinterpret_cast<telemetry_locker_t *>(data);
  *flags = TRACE_STATEMENTS;
  return locker;
}

void tm_set_parent_from_qa(opentelemetry::trace::StartSpanOptions &options,
                           Session_data *data) {
  if (data->m_thd != nullptr) {
    const QueryAttributeTextMapCarrier carrier(data->m_thd);
    QueryAttributeTextMapPropagator propagator;
    opentelemetry::context::Context context;

    auto new_context = propagator.Extract(carrier, context);
    options.parent = opentelemetry::trace::GetSpan(new_context)->GetContext();
  }
}

telemetry_locker_t *tm_stmt_notify_qa(telemetry_locker_t *locker,
                                      bool with_query_attributes,
                                      uint64_t * /* flags */) {
  auto *data = reinterpret_cast<Session_data *>(locker);
  assert(data != nullptr);

  if (data->m_depth > 1) {
    /* Ignore spurious calls for nested statements */
    return locker;
  }

  /* Only top level statement may have query attributes. */
  assert(data->m_depth == 1);

  /* Only one notification expected. */
  assert(!data->m_query_attributes_seen);
  data->m_query_attributes_seen = true;

  if (data->m_trace && (data->m_session_tracer != nullptr)) {
    opentelemetry::trace::StartSpanOptions options;
    opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span> otel_span;

    if (data->m_stmt_stack.empty()) {
      options.kind = opentelemetry::trace::SpanKind::kServer;

      if (with_query_attributes) {
        tm_set_parent_from_qa(options, data);
      }

      if (data->m_session_span != nullptr) {
        const opentelemetry::trace::SpanContext &session_context =
            data->m_session_span->GetContext();

        static const std::map<std::string, std::string> link_map;

        const std::pair<opentelemetry::trace::SpanContext,
                        std::map<std::string, std::string>>
            link{session_context, link_map};

        const std::vector<std::pair<opentelemetry::trace::SpanContext,
                                    std::map<std::string, std::string>>>
            links = {link};

        static const std::map<std::string, std::string> no_attrs = {};

        otel_span =
            data->m_session_tracer->StartSpan(k_stmt, no_attrs, links, options);
      } else {
        otel_span = data->m_session_tracer->StartSpan(k_stmt, options);
      }
    } else {
      options.kind = opentelemetry::trace::SpanKind::kInternal;
      options.parent = data->m_stmt_stack.back()->GetContext();

      otel_span = data->m_session_tracer->StartSpan(k_stmt, options);
    }

    data->m_stmt_stack.push_back(std::move(otel_span));
  }

  return locker;
}

void tm_stmt_abort(telemetry_locker_t *locker) {
  auto *data = reinterpret_cast<Session_data *>(locker);
  assert(data != nullptr);

  /* Abort on top level statement only */
  assert(data->m_depth == 1);

  if (data->m_stmt_stack.size() >= data->m_depth) {
    const opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span>
        otel_span = data->m_stmt_stack.back();

    data->m_stmt_stack.pop_back();

    set_span_stmt_aborted(otel_span);

    otel_span->End();
  }

  data->m_depth--;
}

void tm_stmt_end(telemetry_locker_t *locker, telemetry_stmt_data_t *stmt_data) {
  auto *data = reinterpret_cast<Session_data *>(locker);
  assert(data != nullptr);

  assert(data->m_depth > 0);

  /*
    For the top level statement,
    a call to tm_stmt_notify_qa() is mandatory.
  */
  assert(data->m_depth != 1 || data->m_query_attributes_seen);

  if (data->m_stmt_stack.size() >= data->m_depth) {
    const opentelemetry::nostd::shared_ptr<opentelemetry::trace::Span>
        otel_span = data->m_stmt_stack.back();

    data->m_stmt_stack.pop_back();

    set_span_stmt_data(otel_span, stmt_data);

    otel_span->End();
  }

  data->m_depth--;
}

static bool telemetry_callback_is_registered = false;

static telemetry_t tm_callback = {tm_session_create, tm_session_destroy,
                                  tm_stmt_start,     tm_stmt_notify_qa,
                                  tm_stmt_abort,     tm_stmt_end};

int register_telemetry_callback() {
  assert(!telemetry_callback_is_registered);
  int rc = 0;
  const bool failure = telemetry_traces_srv->register_telemetry(&tm_callback);
  if (failure) {
    log_error("%s: Failed to register telemetry callback.", component_name);
    rc = 1;
  } else {
    telemetry_callback_is_registered = true;
  }
  return rc;
}

void unregister_telemetry_callback() {
  // Be robust to out of order unregister calls.
  if (telemetry_callback_is_registered) {
    const bool failure =
        telemetry_traces_srv->unregister_telemetry(&tm_callback);
    if (failure) {
      log_error("%s: Failed to unregister telemetry callback.", component_name);
    } else {
      telemetry_callback_is_registered = false;
    }
  }
}

void abort_current_session() {
  MYSQL_THD thd = nullptr;
  const int rc = current_thd_srv->get(&thd);
  if (rc != 0) {
    log_error("%s: failed to get current session", component_name);
  }

  if (thd != nullptr) {
    telemetry_traces_srv->abort_telemetry(thd);
  }
}

void wait_for_sessions() {
  std::chrono::time_point<std::chrono::system_clock> wait_until;
  const std::chrono::milliseconds delay{500};
  struct timespec deadline {};

  PSI_stage_progress *progress = nullptr;

  stage_srv->start_stage(g_session_stage.m_key, __FILE__, __LINE__);
  progress = stage_srv->get_current_stage_progress();

  /*
    From now on, count disconnected sessions.
    Note that arming g_shutting_down is racy,
    so we may miss some signal on g_session_notify_cond.
    As a result, always use a timedwait.
    The source of truth is g_session_count,
    not the number of signal received (or not).
    Beside, the timedwait is used to check for THD::killed as well.
  */
  assert(g_shutting_down.load());

  if (progress != nullptr) {
    progress->m_work_completed = 0;
    progress->m_work_estimated = g_session_count.load();
  }

  int attempts = 0;

  while (g_session_count.load() > 0) {
    /*
      TODO: Check for THD::killed flag and abort UNINSTALL.

      Currently, UNINSTALL waits for every instrumented sessions,
      and is not responsive to KILL while waiting.

      To make UNINSTALL progress, user sessions this code waits on
      should be forcefully killed if necessary.

      Consider making:
      - UNINSTALL killable
      by checking for the THD::killed flag during the wait loop.

      This in turn implies:
      - INSTALL should be restartable,
        to recover after a killed UNINSTALL.
      - UNINSTALL should be restartable,
        to recover after a killed UNINSTALL.
    */

    wait_until = std::chrono::system_clock::now();
    wait_until += delay;
    to_timespec(wait_until, deadline);

    attempts++;

    log_info("%s: waiting for %d sessions, attempt %d", component_name,
             g_session_count.load(), attempts);

#ifdef DEBUG_SESSION_COUNT
    assert(attempts <= 10);
#endif /* DEBUG_SESSION_COUNT */

    mutex_srv->lock(&g_session_notify_mutex, __FILE__, __LINE__);
    cond_srv->timedwait(&g_session_notify_cond, &g_session_notify_mutex,
                        &deadline, __FILE__, __LINE__);
    mutex_srv->unlock(&g_session_notify_mutex, __FILE__, __LINE__);

    if (progress != nullptr) {
      progress->m_work_completed = g_sessions_closed.load();
    }
  }

  if (progress != nullptr) {
    progress->m_work_completed = g_sessions_closed.load();
  }
}

/* Protected by mutex g_all_meters_mutex. */
std::map<std::string, std::unique_ptr<MySQLMeter>> g_all_meters;

void setup_otel_metric(const std::unique_ptr<MySQLMeter> &meter,
                       const char *metric_name, const char *metric_desc,
                       const char *metric_unit, MetricNumType metric_numeric,
                       MetricOTELType metric_type,
                       measurement_callback_t metric_cb,
                       void *metric_cb_context) {
  switch (metric_numeric) {
    case METRIC_INTEGER:
      switch (metric_type) {
        case ASYNC_COUNTER:
          meter->createInt64ObservableCounter(metric_name, metric_desc,
                                              metric_unit, metric_cb,
                                              metric_cb_context);
          break;
        case ASYNC_UPDOWN_COUNTER:
          meter->createInt64ObservableUpDownCounter(metric_name, metric_desc,
                                                    metric_unit, metric_cb,
                                                    metric_cb_context);
          break;
        case ASYNC_GAUGE_COUNTER:
          meter->createInt64ObservableGauge(metric_name, metric_desc,
                                            metric_unit, metric_cb,
                                            metric_cb_context);
          break;
        default:
          assert(false);
          return;
      }
      break;
    case METRIC_DOUBLE:
      switch (metric_type) {
        case ASYNC_COUNTER:
          meter->createDoubleObservableCounter(metric_name, metric_desc,
                                               metric_unit, metric_cb,
                                               metric_cb_context);
          break;
        case ASYNC_UPDOWN_COUNTER:
          meter->createDoubleObservableUpDownCounter(metric_name, metric_desc,
                                                     metric_unit, metric_cb,
                                                     metric_cb_context);
          break;
        case ASYNC_GAUGE_COUNTER:
          meter->createDoubleObservableGauge(metric_name, metric_desc,
                                             metric_unit, metric_cb,
                                             metric_cb_context);
          break;
        default:
          assert(false);
          return;
      }
      break;
    default:
      assert(false);
      return;
  };
}

std::unique_ptr<MySQLMeter> do_setup_otel_meter(const char *name,
                                                ulong frequency) {
  std::unique_ptr<MySQLMeter> mysql_meter;
  opentelemetry::metrics::MeterProvider *provider;

#ifdef SINGLE_METER_PROVIDER
  provider = g_meter_provider->get();
#else
  provider = g_all_meter_providers.get(frequency);
#endif

  if (provider != nullptr) {
    bool rc;
    telemetry_metrics_iterator it_metric = nullptr;

    char raw_metric_name[64];
    char raw_metric_desc[1024];
    char raw_metric_unit[64];

    MetricNumType metric_numeric;
    MetricOTELType metric_type;
    measurement_callback_t metric_cb;
    void *metric_cb_context;
    opentelemetry::nostd::shared_ptr<opentelemetry::metrics::Meter> otel_meter;

    rc = telemetry_metrics_srv->metric_iterator_create(name, &it_metric);
    if (rc) {
      OTEL_INTERNAL_LOG_INFO("MySQL meter skipped (no metrics).");
      return nullptr;
    }

    otel_meter = provider->GetMeter(name, "1.0", "url");
    mysql_meter = std::make_unique<MySQLMeter>(otel_meter);

    do {
      my_h_string metric_name = nullptr;
      my_h_string metric_desc = nullptr;
      my_h_string metric_unit = nullptr;
      bool ignore = false;
      bool srv_error;

      // read metric name
      srv_error =
          telemetry_metrics_srv->metric_get_name(it_metric, &metric_name);

      if (!srv_error) {
        if (string_converter_srv->convert_to_buffer(
                metric_name, raw_metric_name, sizeof(raw_metric_name),
                "utf8mb4")) {
          log_error("%s: Failed convert metric name for meter <%s>",
                    component_name, name);
          ignore = true;
        }

        if (metric_name != nullptr) {
          string_factory_srv->destroy(metric_name);
        }
      } else {
        log_error("%s: Failed read metric name for meter <%s>", component_name,
                  name);
        ignore = true;
      }

      // read metric description
      srv_error = telemetry_metrics_srv->metric_get_description(it_metric,
                                                                &metric_desc);

      if (!srv_error) {
        if (string_converter_srv->convert_to_buffer(
                metric_desc, raw_metric_desc, sizeof(raw_metric_desc),
                "utf8mb4")) {
          log_error(
              "%s: Failed convert metric description for meter <%s>, metric "
              "<%s>",
              component_name, name, raw_metric_name);
          ignore = true;
        }

        if (metric_desc != nullptr) {
          string_factory_srv->destroy(metric_desc);
        }
      } else {
        log_error(
            "%s: Failed read metric description for meter <%s>, metric <%s>",
            component_name, name, raw_metric_name);
        ignore = true;
      }

      // read metric unit
      srv_error =
          telemetry_metrics_srv->metric_get_unit(it_metric, &metric_unit);

      if (!srv_error) {
        if (string_converter_srv->convert_to_buffer(
                metric_unit, raw_metric_unit, sizeof(raw_metric_unit),
                "utf8mb4")) {
          log_error(
              "%s: Failed convert metric unit for meter <%s>, metric <%s>",
              component_name, name, raw_metric_name);
          ignore = true;
        }

        if (metric_unit != nullptr) {
          string_factory_srv->destroy(metric_unit);
        }
      } else {
        log_error("%s: Failed read metric unit for meter <%s>, metric <%s>",
                  component_name, name, raw_metric_name);
        ignore = true;
      }

      // read metric numeric type
      if (telemetry_metrics_srv->metric_get_numeric_type(it_metric,
                                                         metric_numeric)) {
        log_error(
            "%s: Failed read metric numeric type for meter <%s>, metric <%s>",
            component_name, name, raw_metric_name);
        ignore = true;
      }

      // read metric otel type
      if (telemetry_metrics_srv->metric_get_metric_type(it_metric,
                                                        metric_type)) {
        log_error(
            "%s: Failed read metric otel type for meter <%s>, metric <%s>",
            component_name, name, raw_metric_name);
        ignore = true;
      }

      // read metric callback
      if (telemetry_metrics_srv->metric_get_callback(it_metric, metric_cb,
                                                     metric_cb_context)) {
        log_error("%s: Failed read metric callback for meter <%s>, metric <%s>",
                  component_name, name, raw_metric_name);
        ignore = true;
      }

      if (!ignore) {
        setup_otel_metric(mysql_meter, raw_metric_name, raw_metric_desc,
                          raw_metric_unit, metric_numeric, metric_type,
                          metric_cb, metric_cb_context);
      }

      rc = telemetry_metrics_srv->metric_iterator_advance(it_metric);
    } while (!rc);

    telemetry_metrics_srv->metric_iterator_destroy(it_metric);
    OTEL_INTERNAL_LOG_INFO("MySQL meter created.");
  } else {
    OTEL_INTERNAL_LOG_INFO("MySQL meter skipped (no provider).");
  }

  return mysql_meter;
}

void setup_otel_meter(const char *name, ulong frequency) {
  const std::string key(name);

  mutex_srv->lock(&g_all_meters_mutex, __FILE__, __LINE__);

  auto it = g_all_meters.find(key);

  if (it == g_all_meters.end()) {
    std::unique_ptr<MySQLMeter> meter = do_setup_otel_meter(name, frequency);

    if (meter) {
      g_all_meters.insert(std::make_pair(key, std::move(meter)));
    }
  }

  mutex_srv->unlock(&g_all_meters_mutex, __FILE__, __LINE__);
}

void cleanup_otel_meter(const char *name) {
  log_info("%s: removing meter <%s>", component_name, name);

  std::string const key(name);

  mutex_srv->lock(&g_all_meters_mutex, __FILE__, __LINE__);

  auto it = g_all_meters.find(key);

  if (it != g_all_meters.end()) {
    g_all_meters.erase(it);

#ifdef SINGLE_METER_PROVIDER
    if (g_meter_provider != nullptr) {
      g_meter_provider->RemoveMeter(name, "1.0", "url");
    }
#else
    g_all_meter_providers.remove_meter(name, "1.0", "url");
#endif
  }

  mutex_srv->unlock(&g_all_meters_mutex, __FILE__, __LINE__);
}

void tm_meter_change_callback(const char *meter, MeterNotifyType change) {
  log_info("%s: Meter change notification for %s", component_name, meter);

  if (change == METER_REMOVED) {
    cleanup_otel_meter(meter);
    return;
  }

  bool rc;
  bool found = false;
  telemetry_meters_iterator it_meter = nullptr;
  bool meter_enabled;
  char raw_meter_name[64];
  unsigned int meter_frequency;

  rc = telemetry_metrics_srv->meter_iterator_create(&it_meter);
  if (rc) {
    log_error("%s: Failed to iterate on meters", component_name);
    return;
  }

  do {
    my_h_string meter_name = nullptr;
    bool ignore = false;
    bool srv_error;

    // read meter name
    srv_error = telemetry_metrics_srv->meter_get_name(it_meter, &meter_name);

    if (!srv_error) {
      if (string_converter_srv->convert_to_buffer(
              meter_name, raw_meter_name, sizeof(raw_meter_name), "utf8mb4")) {
        log_error("%s: Failed convert meter name", component_name);
        ignore = true;
      }

      if (meter_name != nullptr) {
        string_factory_srv->destroy(meter_name);
      }
    } else {
      log_error("%s: Failed read meter name", component_name);
      ignore = true;
    }

    if (!ignore) {
      if (strcmp(raw_meter_name, meter) == 0) {
        found = true;

        // read meter enabled
        if (telemetry_metrics_srv->meter_get_enabled(it_meter, meter_enabled)) {
          log_error("%s: Failed read enabled flag for meter <%s>",
                    component_name, raw_meter_name);
          /* Remove it then */
          meter_enabled = false;
        }

        // read meter frequency
        if (telemetry_metrics_srv->meter_get_frequency(it_meter,
                                                       meter_frequency)) {
          log_error("%s: Failed read frequency for meter <%s>", component_name,
                    raw_meter_name);
          /* Remove it then */
          meter_enabled = false;
        }

        if (change == METER_ADDED) {
          if (meter_enabled) {
            setup_otel_meter(meter, meter_frequency);
          }
        }

        if (change == METER_UPDATE) {
          if (meter_enabled) {
            /*
              We don't know what was updated, it could be:
               - the ENABLED flag,
               - the FREQUENCY
            */
            cleanup_otel_meter(meter);
            setup_otel_meter(meter, meter_frequency);
          } else {
            // METER_UPDATE
            cleanup_otel_meter(meter);
          }
        }
      }
    }

    rc = telemetry_metrics_srv->meter_iterator_advance(it_meter);
  } while (!rc);

  telemetry_metrics_srv->meter_iterator_destroy(it_meter);

  if (!found) {
    cleanup_otel_meter(meter);
  }
}

void setup_otel_meters_notification() {
  metric_srv->register_change_notification(tm_meter_change_callback);
}

void cleanup_otel_meters_notification() {
  metric_srv->unregister_change_notification(tm_meter_change_callback);
}

void setup_otel_meters() {
  bool rc;
  telemetry_meters_iterator it_meter = nullptr;

  rc = telemetry_metrics_srv->meter_iterator_create(&it_meter);
  if (rc) {
    log_error("%s: Failed to iterate on meters", component_name);
    return;
  }

  do {
    my_h_string meter_name = nullptr;
    char raw_meter_name[64];
    bool meter_enabled = false;
    unsigned int meter_frequency = 0;
    bool srv_error;

    // read meter enabled
    if (telemetry_metrics_srv->meter_get_enabled(it_meter, meter_enabled)) {
      log_error("%s: Failed read enabled flag for a meter", component_name);
      /* Ignore it then */
      meter_enabled = false;
    }

    if (meter_enabled) {
      // read meter name
      srv_error = telemetry_metrics_srv->meter_get_name(it_meter, &meter_name);
      if (!srv_error) {
        if (string_converter_srv->convert_to_buffer(meter_name, raw_meter_name,
                                                    sizeof(raw_meter_name),
                                                    "utf8mb4")) {
          log_error("%s: Failed convert name for a meter", component_name);
          /* Ignore it then */
          meter_enabled = false;
        }

        if (meter_name != nullptr) {
          string_factory_srv->destroy(meter_name);
        }
      } else {
        log_error("%s: Failed read name for a meter", component_name);
        /* Ignore it then */
        meter_enabled = false;
      }
    }

    if (meter_enabled) {
      // read meter frequency
      if (telemetry_metrics_srv->meter_get_frequency(it_meter,
                                                     meter_frequency)) {
        log_error("%s: Failed read frequency for meter <%s>", component_name,
                  raw_meter_name);
        /* Ignore it then */
        meter_enabled = false;
      }
    }

    if (meter_enabled) {
      setup_otel_meter(raw_meter_name, meter_frequency);
    }

    rc = telemetry_metrics_srv->meter_iterator_advance(it_meter);
  } while (!rc);

  telemetry_metrics_srv->meter_iterator_destroy(it_meter);
}

void cleanup_otel_meters() {
  log_info("%s: removing all meters", component_name);

  mutex_srv->lock(&g_all_meters_mutex, __FILE__, __LINE__);

  g_all_meters.clear();

  mutex_srv->unlock(&g_all_meters_mutex, __FILE__, __LINE__);
}

opentelemetry::logs::Severity convert_severity(uint8_t mysql_severity) {
  switch (mysql_severity) {
    case TLOG_ERROR:
      return opentelemetry::logs::Severity::kError;
    case TLOG_WARN:
      return opentelemetry::logs::Severity::kWarn;
    case TLOG_INFO:
      return opentelemetry::logs::Severity::kInfo;
    case TLOG_DEBUG:
      return opentelemetry::logs::Severity::kDebug;
    default:
      return opentelemetry::logs::Severity::kDebug;
  }
}

void set_log_record_trace(opentelemetry::logs::LogRecord *record) {
  MYSQL_THD thd = nullptr;
  const int rc = current_thd_srv->get(&thd);
  if (rc == 0) {
    Session_data *data = Session_data::get(thd);
    if (data != nullptr) {
      if (!data->m_stmt_stack.empty()) {
        opentelemetry::trace::SpanContext const parent =
            data->m_stmt_stack.back()->GetContext();
        record->SetTraceId(parent.trace_id());
        record->SetSpanId(parent.span_id());
        record->SetTraceFlags(parent.trace_flags());
      }
    }
  }
}

void tm_log_callback(const char *logger_name, OTELLogLevel severity,
                     const char *message, time_t timestamp,
                     const log_attribute_t *attr_array, size_t attr_count) {
  if (sv_log_enabled) {
    nostd::shared_ptr<opentelemetry::logs::Logger> const logger = g_logger;
    if (logger != nullptr) {
      auto otel_severity = convert_severity(severity);

      nostd::unique_ptr<opentelemetry::logs::LogRecord> record =
          logger->CreateLogRecord();

      const auto ts = std::chrono::system_clock::from_time_t(timestamp);
      record->SetTimestamp(ts);

      record->SetSeverity(otel_severity);

      opentelemetry::common::AttributeValue const otel_body(message);
      record->SetBody(otel_body);

      record->SetAttribute(k_event_name, logger_name);

      if (attr_array != nullptr) {
        const char *attr_name;
        bool bool_value;
        int32_t int32_value;
        uint32_t uint32_value;
        int64_t int64_value;
        uint64_t uint64_value;
        double double_value;
        const char *string_value;

        for (size_t i = 0; i < attr_count; i++) {
          attr_name = attr_array[i].name;

          switch (attr_array[i].type) {
            case LOG_ATTRIBUTE_BOOLEAN:
              bool_value = attr_array[i].value.bool_value;
              record->SetAttribute(attr_name, bool_value);
              break;

            case LOG_ATTRIBUTE_INT32:
              int32_value = attr_array[i].value.int32_value;
              record->SetAttribute(attr_name, int32_value);
              break;

            case LOG_ATTRIBUTE_UINT32:
              uint32_value = attr_array[i].value.uint32_value;
              record->SetAttribute(attr_name, uint32_value);
              break;

            case LOG_ATTRIBUTE_INT64:
              int64_value = attr_array[i].value.int64_value;
              record->SetAttribute(attr_name, int64_value);
              break;

            case LOG_ATTRIBUTE_UINT64:
              uint64_value = attr_array[i].value.uint64_value;
              record->SetAttribute(attr_name, uint64_value);
              break;

            case LOG_ATTRIBUTE_DOUBLE:
              double_value = attr_array[i].value.double_value;
              record->SetAttribute(attr_name, double_value);
              break;

            case LOG_ATTRIBUTE_STRING:
              string_value = attr_array[i].value.string_value;
              record->SetAttribute(attr_name, string_value);
              break;

            case LOG_ATTRIBUTE_STRING_VIEW: {
              nostd::string_view value(attr_array[i].value.string_value,
                                       attr_array[i].value.string_length);
              record->SetAttribute(attr_name, value);
              break;
            }
            default:
              assert(false);
              break;
          }
        }
      }

      set_log_record_trace(record.get());

      logger->EmitLogRecord(std::move(record));
    }
  }
}

static bool telemetry_logger_is_registered = false;

int register_telemetry_logger() {
  assert(!telemetry_logger_is_registered);
  int rc = 0;
  const bool failure = telemetry_logs_srv->register_logger(&tm_log_callback);
  if (failure) {
    log_error("%s: Failed to register telemetry logger.", component_name);
    rc = 1;
  } else {
    telemetry_logger_is_registered = true;
  }
  return rc;
}

void unregister_telemetry_logger() {
  // Be robust to out of order unregister calls.
  if (telemetry_logger_is_registered) {
    const bool failure =
        telemetry_logs_srv->unregister_logger(&tm_log_callback);
    if (failure) {
      log_error("%s: Failed to unregister telemetry logger.", component_name);
    } else {
      telemetry_logger_is_registered = false;
    }
  }
}

}  // namespace telemetry
