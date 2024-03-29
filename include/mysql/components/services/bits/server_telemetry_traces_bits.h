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

#ifndef SERVER_TELEMETRY_TRACES_BITS_H
#define SERVER_TELEMETRY_TRACES_BITS_H

#include <cstddef>  // size_t
#include <cstdint>  // uint64_t

/** Telemetry tracing scope (span types) flags */
#define TRACE_STATEMENTS 0x0001

/*
  Telemetry tracing scope flags are used as both in and out values
  for the "flags" parameter in m_tel_stmt_start, m_tel_stmt_notify_qa callbacks.
  Input can be either TRACE_STATEMENTS or 0, depending if the PS instruments
  required for telemetry were already enabled in PS configuration or not.
  Output can be either TRACE_STATEMENTS or 0, depending if the telemetry
  component wants to trace this statement or not. Input value can be used by the
  component to decide if it wants to force tracing of the statement, regardless
  of the PS configuration of the required instruments.
*/

/** Telemetry tracing scope (span types) masks */
#define TRACE_NOTHING 0x0000
#define TRACE_EVERYTHING 0xFFFF

/** Opaque. */
struct telemetry_session_t;

/** Opaque. */
struct telemetry_locker_t;

typedef telemetry_session_t *(*tel_session_create_v1_t)();

typedef void (*tel_session_destroy_v1_t)(telemetry_session_t *session);

typedef telemetry_locker_t *(*tel_stmt_start_v1_t)(telemetry_session_t *session,
                                                   uint64_t *flags);
typedef telemetry_locker_t *(*tel_stmt_notify_qa_v1_t)(
    telemetry_locker_t *locker, bool with_query_attributes, uint64_t *flags);

struct telemetry_stmt_data_v1_t {
  /** Performance schema event name. */
  const char *m_event_name;
  /** Locked time. */
  unsigned long long m_lock_time;
  /** SQL text. */
  const char *m_sql_text;
  size_t m_sql_text_length;
  /** DIGEST text. */
  const char *m_digest_text;
  /** Current schema. */
  const char *m_current_schema;
  size_t m_current_schema_length;
  /** Object type. */
  const char *m_object_type;
  size_t m_object_type_length;
  /** Object schema. */
  const char *m_object_schema;
  size_t m_object_schema_length;
  /** Object name. */
  const char *m_object_name;
  size_t m_object_name_length;
  /** MYSQL_ERRNO. */
  int m_sql_errno;
  /** SQLSTATE. */
  const char *m_sqlstate;
  /** Error message text. */
  const char *m_message_text;
  /** Number or errors. */
  unsigned long m_error_count;
  /** Number of warnings. */
  unsigned long m_warning_count;
  /** Rows affected. */
  unsigned long long m_rows_affected;
  /** Rows sent. */
  unsigned long long m_rows_sent;
  /** Rows examined. */
  unsigned long long m_rows_examined;
  /** Metric, temporary tables created on disk. */
  unsigned long m_created_tmp_disk_tables;
  /** Metric, temporary tables created. */
  unsigned long m_created_tmp_tables;
  /** Metric, number of select full join. */
  unsigned long m_select_full_join;
  /** Metric, number of select full range join. */
  unsigned long m_select_full_range_join;
  /** Metric, number of select range. */
  unsigned long m_select_range;
  /** Metric, number of select range check. */
  unsigned long m_select_range_check;
  /** Metric, number of select scan. */
  unsigned long m_select_scan;
  /** Metric, number of sort merge passes. */
  unsigned long m_sort_merge_passes;
  /** Metric, number of sort merge. */
  unsigned long m_sort_range;
  /** Metric, number of sort rows. */
  unsigned long m_sort_rows;
  /** Metric, number of sort scans. */
  unsigned long m_sort_scan;
  /** Metric, no index used flag. */
  unsigned char m_no_index_used;
  /** Metric, no good index used flag. */
  unsigned char m_no_good_index_used;
  size_t m_max_controlled_memory;
  size_t m_max_total_memory;
  size_t m_cpu_time;
};

typedef void (*tel_stmt_abort_v1_t)(telemetry_locker_t *locker);

typedef void (*tel_stmt_end_v1_t)(telemetry_locker_t *locker,
                                  telemetry_stmt_data_v1_t *stmt_data);

struct telemetry_v1_t {
  tel_session_create_v1_t m_tel_session_create;
  tel_session_destroy_v1_t m_tel_session_destroy;
  tel_stmt_start_v1_t m_tel_stmt_start;
  tel_stmt_notify_qa_v1_t m_tel_stmt_notify_qa;
  tel_stmt_abort_v1_t m_tel_stmt_abort;
  tel_stmt_end_v1_t m_tel_stmt_end;
};

class THD;

/**
  Register set of telemetry notification callbacks.

  @param telemetry pointer to struct of functions to be registered.
  @retval FALSE: success
  @retval TRUE: failure
*/
typedef bool (*register_telemetry_v1_t)(telemetry_v1_t *telemetry);

/**
  Abort the current statement and session.
  @param thd session pointer.
*/
typedef void (*abort_telemetry_v1_t)(THD *thd);

/**
  Unregister set of telemetry notification callbacks.

  @param telemetry pointer to struct of functions to be unregistered.
  @retval FALSE: success
  @retval TRUE: failure
*/
typedef bool (*unregister_telemetry_v1_t)(telemetry_v1_t *telemetry);

typedef telemetry_stmt_data_v1_t telemetry_stmt_data_t;
typedef telemetry_v1_t telemetry_t;

#endif /* SERVER_TELEMETRY_TRACES_BITS_H */
