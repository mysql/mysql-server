/*
  Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "classic_query_forwarder.h"

#include <charconv>
#include <chrono>
#include <exception>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <system_error>
#include <variant>

#define RAPIDJSON_HAS_STDSTRING 1

#include "my_rapidjson_size_t.h"

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>

#include "classic_connection_base.h"
#include "classic_frame.h"
#include "classic_lazy_connect.h"
#include "classic_query_param.h"
#include "classic_query_sender.h"
#include "classic_quit_sender.h"
#include "classic_session_tracker.h"
#include "command_router_set.h"
#include "harness_assert.h"
#include "hexify.h"
#include "implicit_commit_parser.h"
#include "my_sys.h"  // get_charset_by_name
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/tls_error.h"
#include "mysql/harness/utility/string.h"
#include "mysqld_error.h"  // mysql errors
#include "mysqlrouter/classic_protocol_binary.h"
#include "mysqlrouter/classic_protocol_codec_binary.h"
#include "mysqlrouter/classic_protocol_codec_error.h"
#include "mysqlrouter/classic_protocol_constants.h"
#include "mysqlrouter/classic_protocol_message.h"
#include "mysqlrouter/client_error_code.h"
#include "mysqlrouter/connection_pool_component.h"
#include "mysqlrouter/datatypes.h"
#include "mysqlrouter/routing.h"
#include "mysqlrouter/utils.h"  // to_string
#include "show_warnings_parser.h"
#include "sql/lex.h"
#include "sql_exec_context.h"
#include "sql_lexer.h"
#include "sql_lexer_thd.h"
#include "sql_parser.h"
#include "sql_parser_state.h"
#include "sql_splitting_allowed.h"
#include "start_transaction_parser.h"
#include "stmt_classifier.h"

#undef DEBUG_DUMP_TOKENS

namespace {
const auto forwarded_status_flags =
    classic_protocol::status::in_transaction |
    classic_protocol::status::in_transaction_readonly |
    classic_protocol::status::autocommit;

/**
 * format a timepoint as json-value (date-time format).
 */
std::string string_from_timepoint(
    std::chrono::time_point<std::chrono::system_clock> tp) {
  time_t cur = decltype(tp)::clock::to_time_t(tp);
  struct tm cur_gmtime;
#ifdef _WIN32
  gmtime_s(&cur_gmtime, &cur);
#else
  gmtime_r(&cur, &cur_gmtime);
#endif
  auto usec = std::chrono::duration_cast<std::chrono::microseconds>(
      tp - std::chrono::system_clock::from_time_t(cur));

  return mysql_harness::utility::string_format(
      "%04d-%02d-%02dT%02d:%02d:%02d.%06ldZ", cur_gmtime.tm_year + 1900,
      cur_gmtime.tm_mon + 1, cur_gmtime.tm_mday, cur_gmtime.tm_hour,
      cur_gmtime.tm_min, cur_gmtime.tm_sec,
      // cast to long int as it is "longlong" on 32bit, and "long" on
      // 64bit platforms, but we only have a range of 0-999
      static_cast<long int>(usec.count()));
}

bool ieq(const std::string_view &a, const std::string_view &b) {
  return std::equal(a.begin(), a.end(), b.begin(), b.end(),
                    [](char lhs, char rhs) {
                      auto ascii_tolower = [](char c) {
                        return c >= 'A' && c <= 'Z' ? c | 0x20 : c;
                      };
                      return ascii_tolower(lhs) == ascii_tolower(rhs);
                    });
}

#ifdef DEBUG_DUMP_TOKENS
void dump_token(SqlLexer::iterator::Token tkn) {
  std::map<int, std::string_view> syms;

  for (size_t ndx{}; ndx < sizeof(symbols) / sizeof(symbols[0]); ++ndx) {
    auto sym = symbols[ndx];
    syms.emplace(sym.tok, std::string_view{sym.name, sym.length});
  }
  std::cerr << "<" << tkn.id << ">\t| ";

  auto it = syms.find(tkn.id);
  if (it != syms.end()) {
    std::cerr << "sym[" << std::quoted(it->second) << "]";
  } else if (tkn.id >= 32 && tkn.id < 127) {
    std::cerr << (char)tkn.id;
  } else if (tkn.id == IDENT || tkn.id == IDENT_QUOTED) {
    std::cerr << std::quoted(tkn.text, '`');
  } else if (tkn.id == TEXT_STRING) {
    std::cerr << std::quoted(tkn.text);
  } else if (tkn.id == NUM) {
    std::cerr << tkn.text;
  } else if (tkn.id == ABORT_SYM) {
    std::cerr << "<ABORT>";
  } else if (tkn.id == END_OF_INPUT) {
    std::cerr << "<END>";
  }
  std::cerr << "\n";
}
#endif

std::string to_string(stdx::flags<StmtClassifier> flags) {
  std::string out;

  for (auto [flag, str] : {
           std::pair{StmtClassifier::ForbiddenFunctionWithConnSharing,
                     "forbidden_function_with_connection_sharing"},
           std::pair{StmtClassifier::ForbiddenSetWithConnSharing,
                     "forbidden_set_with_connection_sharing"},
           std::pair{StmtClassifier::NoStateChangeIgnoreTracker,
                     "ignore_session_tracker_some_state_changed"},
           std::pair{StmtClassifier::StateChangeOnError,
                     "session_not_sharable_on_error"},
           std::pair{StmtClassifier::StateChangeOnSuccess,
                     "session_not_sharable_on_success"},
           std::pair{StmtClassifier::StateChangeOnTracker,
                     "accept_session_state_from_session_tracker"},
           std::pair{StmtClassifier::ReadOnly, "read-only"},
       }) {
    if (flags & flag) {
      if (!out.empty()) {
        out += ",";
      }
      out += str;
    }
  }

  return out;
}

/*
 * check if the statement is a multi-statement.
 *
 * true for:  DO 1; DO 2
 * true for:  BEGIN; DO 1; COMMIT
 * false for: CREATE PROCEDURE ... BEGIN DO 1; DO 2; END
 * false for: CREATE PROCEDURE ... BEGIN IF 1 THEN DO 1; END IF; END
 */
bool contains_multiple_statements(SqlLexer &&lexer) {
  {
    bool is_first{true};
    int begin_end_depth{0};

    std::optional<SqlLexer::iterator::Token> first_tkn;
    std::optional<SqlLexer::iterator::Token> last_tkn;

    for (auto tkn : lexer) {
#ifdef DEBUG_DUMP_TOKENS
      dump_token(tkn);
#endif

      if (is_first) {
        first_tkn = tkn;
        is_first = false;
      }

      // semicolon may be inside a BEGIN ... END compound statement of a
      // CREATE PROCEDURE|EVENT|TRIGGER|FUNCTION
      if (first_tkn->id == CREATE) {
        // BEGIN
        if (tkn.id == BEGIN_SYM) {
          ++begin_end_depth;
        }

        // END, at the end of the input.
        //
        // but not END IF, END LOOP, ...
        if (last_tkn && last_tkn->id == END && (tkn.id == END_OF_INPUT)) {
          --begin_end_depth;
        }
      }

      if (begin_end_depth == 0) {
        if (last_tkn) {
          // semicolon outside a BEGIN...END block.

          if (last_tkn->id == ';' && tkn.id != END_OF_INPUT) return true;
        }
      }

      last_tkn = tkn;
    }
  }

  return false;
}

/*
 * classify statements about their behaviour with the session-tracker.
 *
 * Statements may
 *
 * - set user vars, but not set the session-tracker like:
 *
 * @code
 * SELECT 1 INTO @a
 * @endcode
 *
 * - create global locks, but not set the session-tracker like:
 *
 * @code
 * LOCK INSTANCE FOR BACKUP
 * FLUSH TABLES WITH READ LOCK
 * @endcode
 */
stdx::flags<StmtClassifier> classify(SqlLexer &&lexer, bool forbid_set_trackers,
                                     bool config_access_mode_auto) {
  stdx::flags<StmtClassifier> classified{};

  {
    bool is_lhs{true};

    auto lexer_it = lexer.begin();
    if (lexer_it != lexer.end()) {
      auto first = *lexer_it;
      auto last = first;
#ifdef DEBUG_DUMP_TOKENS
      dump_token(first);
#endif

      switch (first.id) {
        case SELECT_SYM:
        case DO_SYM:
        case VALUES:
        case TABLE_SYM:
        case WITH:
        case HELP_SYM:
        case USE_SYM:
        case DESC:
        case DESCRIBE:  // EXPLAIN, DESCRIBE
        case CHECKSUM_SYM:
        case '(':
          classified |= StmtClassifier::ReadOnly;
          break;
      }

      ++lexer_it;

      for (; lexer_it != lexer.end(); ++lexer_it) {
        auto tkn = *lexer_it;

#ifdef DEBUG_DUMP_TOKENS
        dump_token(tkn);
#endif

        if (first.id == SELECT_SYM) {
          if (tkn.id == SQL_CALC_FOUND_ROWS) {
            classified |= StmtClassifier::StateChangeOnSuccess;
            classified |= StmtClassifier::StateChangeOnError;
          }

          // SELECT ... INTO ...
          if (tkn.id == INTO) {
            classified |= StmtClassifier::StateChangeOnSuccess;
          }
        } else if (first.id == LOCK_SYM) {
          // match:   LOCK INSTANCE FOR BACKUP
          // but not: LOCK TABLES ...
          if (tkn.id == INSTANCE_SYM) {
            classified |= StmtClassifier::StateChangeOnSuccess;
          }
        } else if (first.id == FLUSH_SYM) {
          // match:   FLUSH TABLES WITH ...
          // but not: FLUSH TABLES t1 WITH ...
          if (last.id == TABLES && tkn.id == WITH) {
            classified |= StmtClassifier::StateChangeOnSuccess;
          }
        } else if (first.id == GET_SYM && tkn.id == DIAGNOSTICS_SYM) {
          // GET [CURRENT] DIAGNOSTICS ...
          classified |= StmtClassifier::ForbiddenFunctionWithConnSharing;
        } else if (first.id == DESCRIBE || first.id == WITH) {
          // EXPLAIN supports:
          //
          // - SELECT, TABLE, ANALYZE -> read-only
          // - DELETE, INSERT, UPDATE, REPLACE -> read-write.
          //
          // WITH supports:
          //
          // - UPDATE
          // - DELETE
          if (tkn.id == UPDATE_SYM || tkn.id == DELETE_SYM ||
              tkn.id == REPLACE_SYM || tkn.id == INSERT_SYM) {
            // always sent to the read-write servers.
            classified &=
                ~stdx::flags<StmtClassifier>(StmtClassifier::ReadOnly);
          }
        }

        // check forbidden functions in DML statements:
        //
        // can appear more or less everywhere:
        //
        // - INSERT INTO tlb VALUES (GET_LOCK("abc", 1))
        // - SELECT GET_LOCK("abc", 1)
        // - SELECT * FROM tbl WHERE GET_LOCK(...)
        // - CALL FOO(GET_LOCK(...))
        // - DO GET_LOCK()
        //
        // It is ok, if it appears in:
        //
        // - DDL like CREATE|DROP|ALTER

        switch (first.id) {
          case SELECT_SYM:
          case DO_SYM:
          case INSERT_SYM:
          case UPDATE_SYM:
          case DELETE_SYM:
          case CALL_SYM:
          case SET_SYM:
            if (tkn.id == '(' &&
                (last.id == IDENT || last.id == IDENT_QUOTED)) {
              std::string ident;
              ident.resize(last.text.size());

              // ascii-upper-case
              std::transform(
                  last.text.begin(), last.text.end(), ident.begin(),
                  [](auto c) { return (c >= 'a' && c <= 'z') ? c - 0x20 : c; });

              if (ident == "GET_LOCK" ||  //
                  ident == "SERVICE_GET_WRITE_LOCKS" ||
                  ident == "SERVICE_GET_READ_LOCKS" ||
                  ident == "VERSION_TOKENS_LOCK_SHARED" ||
                  ident == "VERSION_TOKENS_LOCK_EXCLUSIVE") {
                classified |= StmtClassifier::StateChangeOnSuccess;

                // always sent to the read-write servers.
                classified &=
                    ~stdx::flags<StmtClassifier>(StmtClassifier::ReadOnly);
              }

              if (ident == "LAST_INSERT_ID") {
                classified |= StmtClassifier::ForbiddenFunctionWithConnSharing;
              }
            }

            break;
        }

        // SELECT ... FOR UPDATE|SHARE
        if (first.id == SELECT_SYM) {
          if (last.id == FOR_SYM &&
              (tkn.id == UPDATE_SYM || tkn.id == SHARE_SYM)) {
            // always sent to the read-write servers.
            classified &=
                ~stdx::flags<StmtClassifier>(StmtClassifier::ReadOnly);
          }
        }

        if (first.id == SET_SYM) {
          if (tkn.id == SET_VAR || tkn.id == EQ) {
            is_lhs = false;
            if (last.id == LEX_HOSTNAME) {
              // LEX_HOSTNAME: @IDENT -> user-var
              // SET_VAR     : :=
              // EQ          : =

              classified |= StmtClassifier::StateChangeOnSuccess;
              classified |= StmtClassifier::StateChangeOnError;
            } else if ((last.id == IDENT || last.id == IDENT_QUOTED)) {
              // SET .* session_track_gtids := ...
              //                             ^^ or =
              //         ^^ or quoted with backticks
              //
              // forbids also
              //
              // - SET SESSION (ident|ident_quoted)
              // - SET @@SESSION.(ident|ident_quoted)
              // - SET LOCAL (ident|ident_quoted)
              // - SET @@LOCAL.(ident|ident_quoted)

              std::string ident;
              ident.resize(last.text.size());

              // ascii-upper-case
              std::transform(
                  last.text.begin(), last.text.end(), ident.begin(),
                  [](auto c) { return (c >= 'a' && c <= 'z') ? c - 0x20 : c; });

              if (ident == "SESSION_TRACK_GTIDS" ||  //
                  ident == "SESSION_TRACK_TRANSACTION_INFO" ||
                  ident == "SESSION_TRACK_STATE_CHANGE" ||
                  ident == "SESSION_TRACK_SYSTEM_VARIABLES") {
                if (forbid_set_trackers) {
                  classified |= StmtClassifier::ForbiddenSetWithConnSharing;
                }
              }
            }
          } else if (tkn.id == ',') {
            is_lhs = true;
          } else if (config_access_mode_auto && is_lhs &&
                     (tkn.id == PERSIST_SYM || tkn.id == PERSIST_ONLY_SYM ||
                      tkn.id == GLOBAL_SYM)) {
            classified |= StmtClassifier::ForbiddenSetWithConnSharing;
          }
        } else {
          if (last.id == LEX_HOSTNAME && tkn.id == SET_VAR) {  // :=
            classified |= StmtClassifier::StateChangeOnSuccess;
            classified |= StmtClassifier::StateChangeOnError;
          }
        }

        last = tkn;
      }

      if (classified & (StmtClassifier::StateChangeOnError |
                        StmtClassifier::StateChangeOnSuccess)) {
        // If the statement would mark the connection as not-sharable, make sure
        // that happens on the read-write server as we don't want to get
        // stuck on a read-only server and not be able to switch back on a
        // UPDATE
        classified &= ~stdx::flags<StmtClassifier>(StmtClassifier::ReadOnly);
      }

      if (first.id == SET_SYM || first.id == USE_SYM) {
        if (!classified || classified == stdx::flags<StmtClassifier>(
                                             StmtClassifier::ReadOnly)) {
          return classified | StmtClassifier::NoStateChangeIgnoreTracker;
        } else {
          return classified;
        }
      } else {
        if (!classified || classified == stdx::flags<StmtClassifier>(
                                             StmtClassifier::ReadOnly)) {
          return classified | StmtClassifier::StateChangeOnTracker;
        } else {
          return classified;
        }
      }
    }
  }

  // unknown or empty statement.
  return StmtClassifier::StateChangeOnTracker;
}

uint64_t get_error_count(MysqlRoutingClassicConnectionBase *connection) {
  uint64_t count{};
  for (auto const &w :
       connection->execution_context().diagnostics_area().warnings()) {
    if (w.level() == "Error") ++count;
  }

  return count;
}

uint64_t get_warning_count(MysqlRoutingClassicConnectionBase *connection) {
  return connection->execution_context().diagnostics_area().warnings().size() +
         (connection->events().events().empty() ? 0 : 1);
}

stdx::expected<void, std::error_code> send_resultset(
    MysqlRoutingClassicConnectionBase::ClientSideConnection &conn,
    const std::vector<classic_protocol::message::server::ColumnMeta> &columns,
    const std::vector<classic_protocol::message::server::Row> &rows) {
  {
    const auto send_res = ClassicFrame::send_msg<
        classic_protocol::borrowed::message::server::ColumnCount>(
        conn, {columns.size()});
    if (!send_res) return stdx::unexpected(send_res.error());
  }

  for (auto const &col : columns) {
    const auto send_res = ClassicFrame::send_msg(conn, col);
    if (!send_res) return stdx::unexpected(send_res.error());
  }

  const auto skips_eof_pos =
      classic_protocol::capabilities::pos::text_result_with_session_tracking;

  const bool router_skips_end_of_columns{
      conn.protocol().shared_capabilities().test(skips_eof_pos)};

  if (!router_skips_end_of_columns) {
    // add a EOF after columns if the client expects it.
    const auto send_res = ClassicFrame::send_msg<
        classic_protocol::borrowed::message::server::Eof>(conn, {});
    if (!send_res) return stdx::unexpected(send_res.error());
  }

  for (auto const &row : rows) {
    const auto send_res = ClassicFrame::send_msg(conn, row);
    if (!send_res) return stdx::unexpected(send_res.error());
  }

  {
    const auto send_res = ClassicFrame::send_msg<
        classic_protocol::borrowed::message::server::Eof>(
        conn, {conn.protocol().status_flags() & forwarded_status_flags, 0});
    if (!send_res) return stdx::unexpected(send_res.error());
  }

  return {};
}

stdx::expected<void, std::error_code> trace_as_json(
    const TraceSpan &event_time_series,
    rapidjson::PrettyWriter<rapidjson::StringBuffer> &writer,
    const TraceEvent &event) {
  // build a Span.
  writer.StartObject();

  if (event.start_time == event.end_time) {
    writer.Key("timestamp");
    writer.String(string_from_timepoint(event.start_time_system));
  } else {
    writer.Key("start_time");
    writer.String(string_from_timepoint(event.start_time_system));

    writer.Key("end_time");
    writer.String(string_from_timepoint(
        event.start_time_system +
        std::chrono::duration_cast<std::chrono::system_clock::duration>(
            event.end_time - event.start_time)));

    // for easier readability by a human.
    writer.Key("elapsed_in_span_us");
    writer.Uint(std::chrono::duration_cast<std::chrono::microseconds>(
                    event.end_time - event.start_time)
                    .count());
  }

  if (event.status_code != TraceEvent::StatusCode::kUnset) {
    writer.Key("status_code");
    switch (event.status_code) {
      case TraceEvent::StatusCode::kOk:
        writer.String("OK");
        break;
      case TraceEvent::StatusCode::kError:
        writer.String("ERROR");
        break;
      default:
        writer.String("UNSET");
        break;
    }
  }

  writer.Key("name");
  writer.String(event.name);

  if (!event.attrs.empty()) {
    writer.Key("attributes");
    writer.StartObject();
    for (const auto &attr : event.attrs) {
      writer.Key(attr.first);

      if (std::holds_alternative<std::monostate>(attr.second)) {
        writer.Null();
      } else if (std::holds_alternative<int64_t>(attr.second)) {
        writer.Int64(std::get<int64_t>(attr.second));
      } else if (std::holds_alternative<std::string>(attr.second)) {
        writer.String(std::get<std::string>(attr.second));
      } else if (std::holds_alternative<bool>(attr.second)) {
        writer.Bool(std::get<bool>(attr.second));
      } else {
        assert(false || "unexpected type");
      }
    }
    writer.EndObject();
  }

  if (!event.events.empty()) {
    writer.Key("events");
    writer.StartArray();
    for (const auto &evs : event.events) {
      trace_as_json(event_time_series, writer, evs);
    }
    writer.EndArray();
  }
  writer.EndObject();

  return {};
}

stdx::expected<std::string, std::error_code> trace_as_json(
    const TraceSpan &event_time_series) {
  rapidjson::StringBuffer buf;
  rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buf);
  writer.SetIndent(' ', 2);
#if 0
  // enabling it makes the trace more compact, but is also a bit less readable.
  writer.SetFormatOptions(
      rapidjson::PrettyFormatOptions::kFormatSingleLineArray);
#endif

  for (const auto &event : event_time_series.events()) {
    trace_as_json(event_time_series, writer, event);
  }

  return buf.GetString();
}

std::vector<classic_protocol::message::server::Row> rows_from_warnings(
    MysqlRoutingClassicConnectionBase *connection,
    ShowWarnings::Verbosity verbosity, uint64_t row_count, uint64_t offset) {
  std::vector<classic_protocol::message::server::Row> rows;

  uint64_t r{};

  for (auto const &w :
       connection->execution_context().diagnostics_area().warnings()) {
    if (verbosity != ShowWarnings::Verbosity::Error || w.level() == "Error") {
      if (r++ < offset) continue;

      if (row_count == rows.size()) break;

      rows.emplace_back(std::vector<std::optional<std::string>>{
          w.level(), std::to_string(w.code()), w.message()});
    }
  }

  const auto &event_time_series = connection->events();
  if (verbosity != ShowWarnings::Verbosity::Error &&
      !event_time_series.events().empty()) {
    auto trace_res = trace_as_json(event_time_series);
    if (trace_res) {
      rows.emplace_back(std::vector<std::optional<std::string>>{
          "Note", std::to_string(ER_ROUTER_TRACE), *trace_res});
    }
  }

  return rows;
}

stdx::expected<void, std::error_code> show_count(
    MysqlRoutingClassicConnectionBase *connection, const char *name,
    uint64_t count) {
  auto &src_conn = connection->client_conn();

  auto send_res =
      send_resultset(src_conn,
                     {
                         {
                             "def",                // catalog
                             "",                   // schema
                             "",                   // table
                             "",                   // orig_table
                             name,                 // name
                             "",                   // orig_name
                             63,                   // collation (binary)
                             21,                   // column_length
                             FIELD_TYPE_LONGLONG,  // type
                             UNSIGNED_FLAG | BINARY_FLAG | NUM_FLAG,  // flags
                             0,  // decimals
                         },
                     },
                     {std::vector<std::optional<std::string>>{
                         std::optional<std::string>(std::to_string(count))}});
  if (!send_res) return stdx::unexpected(send_res.error());

  return {};
}

const char *show_warning_count_name(ShowWarningCount::Verbosity verbosity,
                                    ShowWarningCount::Scope scope) {
  if (verbosity == ShowWarningCount::Verbosity::Error) {
    switch (scope) {
      case ShowWarningCount::Scope::Local:
        return "@@local.error_count";
      case ShowWarningCount::Scope::Session:
        return "@@session.error_count";
      case ShowWarningCount::Scope::None:
        return "@@error_count";
    }
  } else {
    switch (scope) {
      case ShowWarningCount::Scope::Local:
        return "@@local.warning_count";
      case ShowWarningCount::Scope::Session:
        return "@@session.warning_count";
      case ShowWarningCount::Scope::None:
        return "@@warning_count";
    }
  }

  harness_assert_this_should_not_execute();
}

stdx::expected<void, std::error_code> show_warning_count(
    MysqlRoutingClassicConnectionBase *connection,
    ShowWarningCount::Verbosity verbosity, ShowWarningCount::Scope scope) {
  if (verbosity == ShowWarningCount::Verbosity::Error) {
    return show_count(connection, show_warning_count_name(verbosity, scope),
                      get_error_count(connection));
  } else {
    return show_count(connection, show_warning_count_name(verbosity, scope),
                      get_warning_count(connection));
  }
}

stdx::expected<void, std::error_code> show_warnings(
    MysqlRoutingClassicConnectionBase *connection,
    ShowWarnings::Verbosity verbosity, uint64_t row_count, uint64_t offset) {
  auto &src_conn = connection->client_conn();

  // character_set_results
  uint8_t collation = 0xff;  // utf8

  auto send_res = send_resultset(
      src_conn,
      {
          {
              "def",                  // catalog
              "",                     // schema
              "",                     // table
              "",                     // orig_table
              "Level",                // name
              "",                     // orig_name
              collation,              // collation
              28,                     // column_length
              FIELD_TYPE_VAR_STRING,  // type
              NOT_NULL_FLAG,          // flags
              31,                     // decimals
          },
          {
              "def",            // catalog
              "",               // schema
              "",               // table
              "",               // orig_table
              "Code",           // name
              "",               // orig_name
              63,               // collation (binary)
              4,                // column_length
              FIELD_TYPE_LONG,  // type
              NOT_NULL_FLAG | UNSIGNED_FLAG | NUM_FLAG | BINARY_FLAG,  // flags
              0,  // decimals
          },
          {
              "def",                  // catalog
              "",                     // schema
              "",                     // table
              "",                     // orig_table
              "Message",              // name
              "",                     // orig_name
              collation,              // collation
              2048,                   // column_length
              FIELD_TYPE_VAR_STRING,  // type
              NOT_NULL_FLAG,          // flags
              31,                     // decimals
          },
      },
      rows_from_warnings(connection, verbosity, row_count, offset));
  if (!send_res) return stdx::unexpected(send_res.error());

  return {};
}

class Name_string {
 public:
  explicit Name_string(const char *name) : name_(name) {}

  bool eq(const char *rhs) {
    /*
     * charset of system-variables
     */
    static const CHARSET_INFO *system_charset_info =
        &my_charset_utf8mb3_general_ci;

    return 0 == my_strcasecmp(system_charset_info, name_, rhs);
  }

 private:
  const char *name_;
};

stdx::expected<void, std::error_code> execute_command_router_set_trace(
    MysqlRoutingClassicConnectionBase *connection,
    const CommandRouterSet &cmd) {
  auto &src_conn = connection->client_conn();
  auto &src_protocol = src_conn.protocol();

  if (std::holds_alternative<int64_t>(cmd.value())) {
    auto val = std::get<int64_t>(cmd.value());

    switch (val) {
      case 0:
      case 1: {
        src_protocol.trace_commands(val != 0);

        auto send_res =
            ClassicFrame::send_msg<classic_protocol::message::server::Ok>(
                src_conn,
                {0, 0, src_protocol.status_flags() & forwarded_status_flags,
                 0});
        if (!send_res) return stdx::unexpected(send_res.error());

        return {};
      }

      default: {
        auto send_res =
            ClassicFrame::send_msg<classic_protocol::message::server::Error>(
                src_conn, {ER_WRONG_VALUE_FOR_VAR,
                           "Variable '" + cmd.name() +
                               "' can't be set to the value of '" +
                               std::to_string(val) + "'",
                           "42000"});
        if (!send_res) return stdx::unexpected(send_res.error());

        return {};
      }
    };
  }

  auto send_res =
      ClassicFrame::send_msg<classic_protocol::message::server::Error>(
          src_conn,
          {ER_WRONG_VALUE_FOR_VAR,
           "Variable '" + cmd.name() + "' can't be set. Expected an integer.",
           "42000"});
  if (!send_res) return stdx::unexpected(send_res.error());

  return {};
}

stdx::expected<void, std::error_code> execute_command_router_set_access_mode(
    MysqlRoutingClassicConnectionBase *connection,
    const CommandRouterSet &cmd) {
  auto &src_conn = connection->client_conn();
  auto &src_protocol = src_conn.protocol();

  if (std::holds_alternative<std::string>(cmd.value())) {
    auto v = std::get<std::string>(cmd.value());

    auto from_string = [](const std::string_view &v)
        -> stdx::expected<
            std::optional<ClientSideClassicProtocolState::AccessMode>,
            std::string> {
      if (ieq(v, "read_write")) {
        return ClientSideClassicProtocolState::AccessMode::ReadWrite;
      } else if (ieq(v, "read_only")) {
        return ClientSideClassicProtocolState::AccessMode::ReadOnly;
      } else if (ieq(v, "auto")) {
        return std::nullopt;
      } else {
        return stdx::unexpected("Expected 'read_write', 'read_only' or 'auto'");
      }
    };

    const auto access_mode_res = from_string(v);
    if (!access_mode_res) {
      auto send_res =
          ClassicFrame::send_msg<classic_protocol::message::server::Error>(
              src_conn, {1064,
                         "parse error in 'ROUTER SET access_mode = <...>'. " +
                             access_mode_res.error(),
                         "42000"});
      if (!send_res) return stdx::unexpected(send_res.error());

      return {};
    }

    // transaction.
    if (connection->trx_characteristics() &&
        !connection->trx_characteristics()->characteristics().empty()) {
      auto send_res =
          ClassicFrame::send_msg<classic_protocol::message::server::Error>(
              src_conn, {1064,
                         "'ROUTER SET access_mode = <...>' not allowed while "
                         "transaction is active.",
                         "42000"});
      if (!send_res) return stdx::unexpected(send_res.error());

      return {};
    }

    // prepared statements, locked tables, ...
    if (!connection->connection_sharing_allowed()) {
      auto send_res =
          ClassicFrame::send_msg<classic_protocol::message::server::Error>(
              src_conn, {1064,
                         "ROUTER SET access_mode = <...> not allowed while "
                         "connection-sharing is not possible.",
                         "42000"});
      if (!send_res) return stdx::unexpected(send_res.error());

      return {};
    }

    // config's access_mode MUST be 'auto'
    if (connection->context().access_mode() != routing::AccessMode::kAuto) {
      auto send_res =
          ClassicFrame::send_msg<classic_protocol::message::server::Error>(
              src_conn, {1064,
                         "ROUTER SET access_mode = <...> not allowed if the "
                         "configuration variable 'access_mode' is not 'auto'",
                         "42000"});
      if (!send_res) return stdx::unexpected(send_res.error());

      return {};
    }

    src_protocol.access_mode(*access_mode_res);

    auto send_res =
        ClassicFrame::send_msg<classic_protocol::message::server::Ok>(src_conn,
                                                                      {});
    if (!send_res) return stdx::unexpected(send_res.error());

    return {};
  }

  auto send_res =
      ClassicFrame::send_msg<classic_protocol::message::server::Error>(
          src_conn,
          {1064,
           "parse error in 'ROUTER SET access_mode = <...>'. Expected a string",
           "42000"});
  if (!send_res) return stdx::unexpected(send_res.error());

  return {};
}

stdx::expected<void, std::error_code>
execute_command_router_set_wait_for_my_writes(
    MysqlRoutingClassicConnectionBase *connection,
    const CommandRouterSet &cmd) {
  auto &src_conn = connection->client_conn();
  auto &src_protocol = src_conn.protocol();

  if (std::holds_alternative<int64_t>(cmd.value())) {
    switch (auto val = std::get<int64_t>(cmd.value())) {
      case 0:
      case 1: {
        src_protocol.wait_for_my_writes(val != 0);

        auto send_res =
            ClassicFrame::send_msg<classic_protocol::message::server::Ok>(
                src_conn, {});
        if (!send_res) return stdx::unexpected(send_res.error());

        return {};
      }
      default: {
        auto send_res =
            ClassicFrame::send_msg<classic_protocol::message::server::Error>(
                src_conn,
                {1064,
                 "parse error in 'ROUTER SET wait_for_my_writes = <...>'. "
                 "Expected a number in the range 0..1 inclusive",
                 "42000"});
        if (!send_res) return stdx::unexpected(send_res.error());

        return {};
      }
    }
  }

  auto send_res =
      ClassicFrame::send_msg<classic_protocol::message::server::Error>(
          src_conn, {1064,
                     "parse error in 'ROUTER SET wait_for_my_writes = <...>'. "
                     "Expected a number",
                     "42000"});
  if (!send_res) return stdx::unexpected(send_res.error());

  return {};
}

stdx::expected<void, std::error_code>
execute_command_router_set_wait_for_my_writes_timeout(
    MysqlRoutingClassicConnectionBase *connection,
    const CommandRouterSet &cmd) {
  auto &src_conn = connection->client_conn();
  auto &src_protocol = src_conn.protocol();

  if (std::holds_alternative<int64_t>(cmd.value())) {
    auto val = std::get<int64_t>(cmd.value());

    if (val < 0 || val > 3600) {
      auto send_res = ClassicFrame::send_msg<
          classic_protocol::message::server::Error>(
          src_conn,
          {1064,
           "parse error in 'ROUTER SET wait_for_my_writes_timeout = <...>'. "
           "Expected a number between 0 and 3600 inclusive",
           "42000"});
      if (!send_res) return stdx::unexpected(send_res.error());

      return {};
    }

    src_protocol.wait_for_my_writes_timeout(std::chrono::seconds(val));

    auto send_res =
        ClassicFrame::send_msg<classic_protocol::message::server::Ok>(src_conn,
                                                                      {});
    if (!send_res) return stdx::unexpected(send_res.error());

    return {};
  }

  auto send_res =
      ClassicFrame::send_msg<classic_protocol::message::server::Error>(
          src_conn,
          {1064,
           "parse error in 'ROUTER SET wait_for_my_writes_timeout = <...>'. "
           "Expected a number",
           "42000"});
  if (!send_res) return stdx::unexpected(send_res.error());

  return {};
}

/*
 * ROUTER SET <key> = <value>
 *
 * @retval expected        done
 * @retval unexpected      fatal-error
 */
stdx::expected<void, std::error_code> execute_command_router_set(
    MysqlRoutingClassicConnectionBase *connection,
    const CommandRouterSet &cmd) {
  auto &src_conn = connection->client_conn();

  if (Name_string(cmd.name().c_str()).eq("trace")) {
    return execute_command_router_set_trace(connection, cmd);
  }

  if (Name_string(cmd.name().c_str()).eq("access_mode")) {
    return execute_command_router_set_access_mode(connection, cmd);
  }

  if (Name_string(cmd.name().c_str()).eq("wait_for_my_writes")) {
    return execute_command_router_set_wait_for_my_writes(connection, cmd);
  }

  if (Name_string(cmd.name().c_str()).eq("wait_for_my_writes_timeout")) {
    return execute_command_router_set_wait_for_my_writes_timeout(connection,
                                                                 cmd);
  }

  auto send_res =
      ClassicFrame::send_msg<classic_protocol::message::server::Error>(
          src_conn,
          {ER_UNKNOWN_SYSTEM_VARIABLE,
           "Unknown Router system variable '" + cmd.name() + "'", "HY000"});
  if (!send_res) return stdx::unexpected(send_res.error());

  return {};
}

class InterceptedStatementsParser : public ShowWarningsParser {
 public:
  using ShowWarningsParser::ShowWarningsParser;

  stdx::expected<std::variant<std::monostate, ShowWarningCount, ShowWarnings,
                              CommandRouterSet>,
                 std::string>
  parse() {
    using ret_type =
        stdx::expected<std::variant<std::monostate, ShowWarningCount,
                                    ShowWarnings, CommandRouterSet>,
                       std::string>;

    if (accept(SHOW)) {
      if (accept(WARNINGS)) {
        stdx::expected<Limit, std::string> limit_res;

        if (accept(LIMIT)) {  // optional limit
          limit_res = limit();
        }

        if (accept(END_OF_INPUT)) {
          if (limit_res) {
            return ret_type{
                std::in_place,
                ShowWarnings{ShowWarnings::Verbosity::Warning,
                             limit_res->row_count, limit_res->offset}};
          }

          return ret_type{std::in_place,
                          ShowWarnings{ShowWarnings::Verbosity::Warning}};
        }

        // unexpected input after SHOW WARNINGS [LIMIT ...]
        return {};
      } else if (accept(ERRORS)) {
        stdx::expected<Limit, std::string> limit_res;

        if (accept(LIMIT)) {
          limit_res = limit();
        }

        if (accept(END_OF_INPUT)) {
          if (limit_res) {
            return ret_type{
                std::in_place,
                ShowWarnings{ShowWarnings::Verbosity::Error,
                             limit_res->row_count, limit_res->offset}};
          }

          return ret_type{std::in_place,
                          ShowWarnings{ShowWarnings::Verbosity::Error}};
        }

        // unexpected input after SHOW ERRORS [LIMIT ...]
        return {};
      } else if (accept(COUNT_SYM) && accept('(') && accept('*') &&
                 accept(')')) {
        if (accept(WARNINGS)) {
          if (accept(END_OF_INPUT)) {
            return ret_type{std::in_place,
                            ShowWarningCount{ShowWarnings::Verbosity::Warning,
                                             ShowWarningCount::Scope::Session}};
          }

          // unexpected input after SHOW COUNT(*) WARNINGS
          return {};
        } else if (accept(ERRORS)) {
          if (accept(END_OF_INPUT)) {
            return ret_type{std::in_place,
                            ShowWarningCount{ShowWarnings::Verbosity::Error,
                                             ShowWarningCount::Scope::Session}};
          }

          // unexpected input after SHOW COUNT(*) ERRORS
          return {};
        }

        // unexpected input after SHOW COUNT(*), expected WARNINGS|ERRORS.
        return {};
      } else {
        // unexpected input after SHOW, expected WARNINGS|ERRORS|COUNT
        return {};
      }
    } else if (accept(SELECT_SYM)) {
      // match
      //
      // SELECT @@((LOCAL|SESSION).)?warning_count|error_count;
      //
      if (accept('@')) {
        if (accept('@')) {
          if (accept(SESSION_SYM)) {
            if (accept('.')) {
              auto ident_res = warning_count_ident();
              if (ident_res && accept(END_OF_INPUT)) {
                return ret_type{
                    std::in_place,
                    ShowWarningCount(*ident_res,
                                     ShowWarningCount::Scope::Session)};
              }
            }
          } else if (accept(LOCAL_SYM)) {
            if (accept('.')) {
              auto ident_res = warning_count_ident();
              if (ident_res && accept(END_OF_INPUT)) {
                return ret_type{
                    std::in_place,
                    ShowWarningCount(*ident_res,
                                     ShowWarningCount::Scope::Local)};
              }
            }
          } else {
            auto ident_res = warning_count_ident();
            if (ident_res && accept(END_OF_INPUT)) {
              return ret_type{
                  std::in_place,
                  ShowWarningCount(*ident_res, ShowWarningCount::Scope::None)};
            }
          }
        }
      }
    } else if (auto tkn = accept(IDENT)) {
      if (ieq(tkn.text(), "router")) {       // ROUTER
        if (accept(SET_SYM)) {               // SET
          if (auto name_tkn = ident()) {     // <name>
            if (accept(EQ)) {                // =
              if (auto val = value()) {      // <value>
                if (accept(END_OF_INPUT)) {  // $
                  return ret_type{std::in_place,
                                  CommandRouterSet(name_tkn.text(), *val)};
                } else {
                  return stdx::unexpected(
                      "ROUTER SET <name> = <value>. Extra data.");
                }
              } else {
                return stdx::unexpected(
                    "ROUTER SET <name> = expected <value>. " + error_);
              }
            } else {
              return stdx::unexpected("ROUTER SET <name> expects =");
            }
          } else {
            return stdx::unexpected("ROUTER SET expects <name>.");
          }
        } else {
          return stdx::unexpected("ROUTER expects SET.");
        }
      }
    }

    // not matched.
    return {};
  }

 private:
  // convert a NUM to a number
  //
  // NUM is a bare number.
  //
  // no leading minus or plus [both independent symbols '-' and '+']
  // no 0x... [HEX_NUM],
  // no 0b... [BIN_NUM],
  // no (1.0) [DECIMAL_NUM]
  template <class R>
  static R sv_to_num(std::string_view s) {
    R v{};

    const auto conv_res = std::from_chars(s.data(), s.data() + s.size(), v);
    if (conv_res.ec == std::errc{}) {
      return v;
    } else {
      // NUM is a number, it should always convert.
      harness_assert_this_should_not_execute();
    }
  }

  stdx::expected<CommandRouterSet::Value, std::string> value() {
    using ret_type = stdx::expected<CommandRouterSet::Value, std::string>;

    if (accept(TRUE_SYM)) return ret_type{std::in_place, true};
    if (accept(FALSE_SYM)) return ret_type{std::in_place, false};

    if (accept('-')) {
      if (auto num_tkn = expect(NUM)) {
        auto num = sv_to_num<int64_t>(num_tkn.text());
        return ret_type{std::in_place, -num};
      }
    } else if (auto tkn = accept(NUM)) {
      auto num = sv_to_num<uint64_t>(tkn.text());
      return ret_type{std::in_place, static_cast<int64_t>(num)};
    } else if (auto tkn = accept(TEXT_STRING)) {
      return ret_type{std::in_place, std::string(tkn.text())};
    } else {
      return stdx::unexpected("Expected <BOOL>, <NUM> or <STRING>");
    }

    return stdx::unexpected(error_);
  }
};

stdx::expected<std::variant<std::monostate, ShowWarningCount, ShowWarnings,
                            CommandRouterSet>,
               std::string>
intercept_diagnostics_area_queries(SqlLexer &&lexer) {
  return InterceptedStatementsParser(lexer.begin(), lexer.end()).parse();
}

stdx::expected<std::variant<std::monostate, StartTransaction>, std::string>
start_transaction(SqlLexer &&lexer) {
  return StartTransactionParser(lexer.begin(), lexer.end()).parse();
}

stdx::expected<SplittingAllowedParser::Allowed, std::string> splitting_allowed(
    SqlLexer &&lexer) {
  return SplittingAllowedParser(lexer.begin(), lexer.end()).parse();
}

stdx::expected<bool, std::string> is_implicitly_committed(
    SqlLexer &&lexer,
    std::optional<classic_protocol::session_track::TransactionState>
        trx_state) {
  return ImplicitCommitParser(lexer.begin(), lexer.end()).parse(trx_state);
}

/*
 * fetch the warnings from the server and inject the trace.
 */
class ForwardedShowWarningsHandler : public QuerySender::Handler {
 public:
  explicit ForwardedShowWarningsHandler(
      MysqlRoutingClassicConnectionBase *connection,
      ShowWarnings::Verbosity verbosity)
      : connection_(connection), verbosity_(verbosity) {}

  void on_column_count(uint64_t count) override {
    auto &dst_conn = connection_->client_conn();

    // forward the message.
    auto send_res =
        ClassicFrame::send_msg<classic_protocol::message::server::ColumnCount>(
            dst_conn, {count});
    if (!send_res) something_failed_ = true;

    col_count_ = count;

    if (col_count_ != 3) something_failed_ = true;
  }

  void on_column(
      const classic_protocol::message::server::ColumnMeta &col) override {
    auto &dst_conn = connection_->client_conn();
    auto &dst_protocol = dst_conn.protocol();

    auto send_res = ClassicFrame::send_msg(dst_conn, col);
    if (!send_res) {
      something_failed_ = true;
    }

    switch (col_cur_) {
      case 0:
        if (col.name() != "Level") {
          something_failed_ = true;
        }
        break;
      case 1:
        if (col.name() != "Code") {
          something_failed_ = true;
        }
        break;
      case 2:
        if (col.name() != "Message") {
          something_failed_ = true;
        }
        break;
      default:
        something_failed_ = true;
        break;
    }

    ++col_cur_;

    if (col_cur_ == 3 && !dst_protocol.shared_capabilities().test(
                             classic_protocol::capabilities::pos::
                                 text_result_with_session_tracking)) {
      // client needs a Eof packet after the columns.
      auto send_res = ClassicFrame::send_msg<
          classic_protocol::borrowed::message::server::Eof>(dst_conn, {});
      if (!send_res) {
        something_failed_ = true;
      }
    }
  }

  void on_row(const classic_protocol::message::server::Row &msg) override {
    auto &dst_conn = connection_->client_conn();

    auto send_res = ClassicFrame::send_msg(dst_conn, msg);
    if (!send_res) something_failed_ = true;
  }

  // end of rows.
  void on_row_end(const classic_protocol::message::server::Eof &msg) override {
    auto &dst_conn = connection_->client_conn();

    // inject the trace, if there are events and the user asked for WARNINGS.
    if (!something_failed_ && !connection_->events().empty() &&
        verbosity_ == ShowWarnings::Verbosity::Warning) {
      const auto trace_res = trace_as_json(connection_->events());
      if (trace_res) {
        using msg_type = classic_protocol::message::server::Row;
        const auto send_res = ClassicFrame::send_msg<msg_type>(
            dst_conn,
            std::vector<msg_type::value_type>{
                {"Note"}, {std::to_string(ER_ROUTER_TRACE)}, {*trace_res}});
        if (!send_res) something_failed_ = true;
      }
    }

    const auto send_res = ClassicFrame::send_msg(dst_conn, msg);
    if (!send_res) something_failed_ = true;
  }

  void on_ok(const classic_protocol::message::server::Ok &msg) override {
    auto &dst_conn = connection_->client_conn();

    const auto send_res = ClassicFrame::send_msg(dst_conn, msg);
    if (!send_res) something_failed_ = true;
  }

  void on_error(const classic_protocol::message::server::Error &msg) override {
    auto &dst_conn = connection_->client_conn();

    const auto send_res = ClassicFrame::send_msg(dst_conn, msg);
    if (!send_res) something_failed_ = true;
  }

 private:
  uint64_t col_count_{};
  uint64_t col_cur_{};
  MysqlRoutingClassicConnectionBase *connection_;

  bool something_failed_{false};

  ShowWarnings::Verbosity verbosity_;
};

/*
 * fetch the warning count from the server and increment the warning-count.
 */
class ForwardedShowWarningCountHandler : public QuerySender::Handler {
 public:
  explicit ForwardedShowWarningCountHandler(
      MysqlRoutingClassicConnectionBase *connection,
      ShowWarnings::Verbosity verbosity)
      : connection_(connection), verbosity_(verbosity) {}

  void on_column_count(uint64_t count) override {
    auto &dst_conn = connection_->client_conn();

    // forward the message.
    auto send_res =
        ClassicFrame::send_msg<classic_protocol::message::server::ColumnCount>(
            dst_conn, {count});
    if (!send_res) something_failed_ = true;

    col_count_ = count;

    if (col_count_ != 1) something_failed_ = true;
  }

  void on_column(
      const classic_protocol::message::server::ColumnMeta &col) override {
    auto &dst_conn = connection_->client_conn();

    auto send_res = ClassicFrame::send_msg(dst_conn, col);
    if (!send_res) {
      something_failed_ = true;
    }
  }

  void on_row(const classic_protocol::message::server::Row &msg) override {
    auto &dst_conn = connection_->client_conn();

    // increment the warning count, if there are events and the user asked for
    // WARNINGS.
    if (!something_failed_ && !connection_->events().empty() &&
        verbosity_ == ShowWarnings::Verbosity::Warning &&
        msg.begin() != msg.end()) {
      auto fld = *msg.begin();

      if (fld.has_value()) {
        // fld is a numeric string
        //
        // convert it to a number, increment it and convert it back to a string.
        uint64_t warning_count;
        const auto conv_res = std::from_chars(
            fld->data(), fld->data() + fld->size(), warning_count);
        if (conv_res.ec == std::errc{}) {
          auto send_res =
              ClassicFrame::send_msg<classic_protocol::message::server::Row>(
                  dst_conn, {{std::to_string(warning_count + 1)}});
          if (!send_res) something_failed_ = true;

          return;
        }
      }
    }

    auto send_res = ClassicFrame::send_msg(dst_conn, msg);
    if (!send_res) something_failed_ = true;
  }

  // end of rows.
  void on_row_end(const classic_protocol::message::server::Eof &msg) override {
    auto &dst_conn = connection_->client_conn();

    const auto send_res = ClassicFrame::send_msg(dst_conn, msg);
    if (!send_res) something_failed_ = true;
  }

  void on_ok(const classic_protocol::message::server::Ok &msg) override {
    auto &dst_conn = connection_->client_conn();

    const auto send_res = ClassicFrame::send_msg(dst_conn, msg);
    if (!send_res) something_failed_ = true;
  }

  void on_error(const classic_protocol::message::server::Error &msg) override {
    auto &dst_conn = connection_->client_conn();

    const auto send_res = ClassicFrame::send_msg(dst_conn, msg);
    if (!send_res) something_failed_ = true;
  }

 private:
  uint64_t col_count_{};
  uint64_t col_cur_{};
  MysqlRoutingClassicConnectionBase *connection_;

  bool something_failed_{false};

  ShowWarnings::Verbosity verbosity_;
};

/*
 * fetch the warnings from the server and inject the trace.
 */
class FailedQueryHandler : public QuerySender::Handler {
 public:
  explicit FailedQueryHandler(QueryForwarder &processor)
      : processor_(processor) {}

  void on_ok(const classic_protocol::message::server::Ok &) override {
    //
  }

  void on_error(const classic_protocol::message::server::Error &err) override {
    processor_.failed(err);
  }

 private:
  QueryForwarder &processor_;
};

bool ends_with(std::string_view haystack, std::string_view needle) {
  if (haystack.size() < needle.size()) return false;

  return haystack.substr(haystack.size() - needle.size()) == needle;
}

bool set_transaction_contains_read_only(
    std::optional<classic_protocol::session_track::TransactionCharacteristics>
        trx_char) {
  // match SET TRANSACTION READ ONLY; at the end of the string as
  // the server sends:
  //
  // SET TRANSACTION ISOLATION LEVEL READ COMMITTED; SET TRANSACTION
  // READ ONLY;
  return (trx_char &&
          ends_with(trx_char->characteristics(), "SET TRANSACTION READ ONLY;"));
}

}  // namespace

stdx::expected<Processor::Result, std::error_code> QueryForwarder::process() {
  switch (stage()) {
    case Stage::Command:
      return command();
    case Stage::ExplicitCommitConnect:
      return explicit_commit_connect();
    case Stage::ExplicitCommitConnectDone:
      return explicit_commit_connect_done();
    case Stage::ExplicitCommit:
      return explicit_commit();
    case Stage::ExplicitCommitDone:
      return explicit_commit_done();
    case Stage::ClassifyQuery:
      return classify_query();
    case Stage::SwitchBackend:
      return switch_backend();
    case Stage::PrepareBackend:
      return prepare_backend();
    case Stage::Connect:
      return connect();
    case Stage::Connected:
      return connected();
    case Stage::Forward:
      return forward();
    case Stage::ForwardDone:
      return forward_done();
    case Stage::Response:
      return response();
    case Stage::ColumnCount:
      return column_count();
    case Stage::LoadData:
      return load_data();
    case Stage::Data:
      return data();
    case Stage::Column:
      return column();
    case Stage::ColumnEnd:
      return column_end();
    case Stage::RowOrEnd:
      return row_or_end();
    case Stage::Row:
      return row();
    case Stage::RowEnd:
      return row_end();
    case Stage::Ok:
      return ok();
    case Stage::Error:
      return error();
    case Stage::ResponseDone:
      return response_done();
    case Stage::SendQueued:
      return send_queued();
    case Stage::Done:
      return Result::Done;
  }

  harness_assert_this_should_not_execute();
}

stdx::expected<Processor::Result, std::error_code> QueryForwarder::command() {
  auto &src_conn = connection()->client_conn();
  auto &src_protocol = src_conn.protocol();

  if (!connection()->connection_sharing_possible()) {
    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("query::command"));
    }
    stage(Stage::PrepareBackend);
    return Result::Again;
  } else {
    auto msg_res = ClassicFrame::recv_msg<
        classic_protocol::borrowed::message::client::Query>(src_conn);
    if (!msg_res) {
      // all codec-errors should result in a Malformed Packet error..
      if (msg_res.error().category() !=
          make_error_code(classic_protocol::codec_errc::not_enough_input)
              .category()) {
        return recv_client_failed(msg_res.error());
      }

      discard_current_msg(src_conn);

      auto send_msg =
          ClassicFrame::send_msg<classic_protocol::message::server::Error>(
              src_conn,
              {ER_MALFORMED_PACKET, "Malformed communication packet", "HY000"});
      if (!send_msg) send_client_failed(send_msg.error());

      stage(Stage::Done);

      return Result::SendToClient;
    }

    if (auto &tr = tracer()) {
      std::ostringstream oss;

      for (const auto &param : msg_res->values()) {
        oss << "\n";
        oss << "- " << param.name << ": ";

        if (!param.value) {
          oss << "NULL";
        } else if (auto param_str = param_to_string(param)) {
          oss << param_str.value();
        }
      }

      tr.trace(Tracer::Event().stage(
          "query::command: " +
          std::string(msg_res->statement().substr(0, 1024)) + oss.str()));
    }

    // init the parser-statement once.
    sql_parser_state_.statement(msg_res->statement());

    if (src_protocol.shared_capabilities().test(
            classic_protocol::capabilities::pos::multi_statements) &&
        contains_multiple_statements(sql_parser_state_.lexer())) {
      auto send_res = ClassicFrame::send_msg<
          classic_protocol::message::server::Error>(
          src_conn,
          {ER_ROUTER_NOT_ALLOWED_WITH_CONNECTION_SHARING,
           "Multi-Statements are forbidden if connection-sharing is enabled.",
           "HY000"});
      if (!send_res) return send_client_failed(send_res.error());

      discard_current_msg(src_conn);

      stage(Stage::Done);
      return Result::SendToClient;
    }

    // the diagnostics-area is only maintained, if connection-sharing is
    // allowed.
    //
    // Otherwise, all queries for the diagnostics area MUST go to the
    // server.
    const auto intercept_res =
        intercept_diagnostics_area_queries(sql_parser_state_.lexer());
    if (intercept_res) {
      if (std::holds_alternative<std::monostate>(*intercept_res)) {
        // no match
      } else if (std::holds_alternative<ShowWarnings>(*intercept_res)) {
        auto cmd = std::get<ShowWarnings>(*intercept_res);

        discard_current_msg(src_conn);

        if (connection()->connection_sharing_allowed()) {
          auto send_res = show_warnings(connection(), cmd.verbosity(),
                                        cmd.row_count(), cmd.offset());
          if (!send_res) return send_client_failed(send_res.error());

          stage(Stage::Done);
          return Result::SendToClient;
        } else {
          // send the message to the backend, and inject the trace if there is
          // one.
          stage(Stage::SendQueued);

          connection()->push_processor(std::make_unique<QuerySender>(
              connection(), std::string(msg_res->statement()),
              std::make_unique<ForwardedShowWarningsHandler>(connection(),
                                                             cmd.verbosity())));

          return Result::Again;
        }
      } else if (std::holds_alternative<ShowWarningCount>(*intercept_res)) {
        auto cmd = std::get<ShowWarningCount>(*intercept_res);

        discard_current_msg(src_conn);

        if (connection()->connection_sharing_allowed()) {
          auto send_res =
              show_warning_count(connection(), cmd.verbosity(), cmd.scope());
          if (!send_res) return send_client_failed(send_res.error());

          stage(Stage::Done);
          return Result::SendToClient;
        } else {
          // send the message to the backend, and increment the warning count
          // if there is a trace.
          stage(Stage::SendQueued);

          connection()->push_processor(std::make_unique<QuerySender>(
              connection(), std::string(msg_res->statement()),
              std::make_unique<ForwardedShowWarningCountHandler>(
                  connection(), cmd.verbosity())));

          return Result::Again;
        }
      } else if (std::holds_alternative<CommandRouterSet>(*intercept_res)) {
        discard_current_msg(src_conn);

        connection()->execution_context().diagnostics_area().warnings().clear();
        connection()->events().clear();

        auto cmd = std::get<CommandRouterSet>(*intercept_res);

        auto set_res = execute_command_router_set(connection(), cmd);
        if (!set_res) return send_client_failed(set_res.error());

        stage(Stage::Done);
        return Result::SendToClient;
      }
    } else {
      discard_current_msg(src_conn);

      auto send_res =
          ClassicFrame::send_msg<classic_protocol::message::server::Error>(
              src_conn, {1064, intercept_res.error(), "42000"});
      if (!send_res) return send_client_failed(send_res.error());

      stage(Stage::Done);
      return Result::SendToClient;
    }

    if (connection()->context().access_mode() == routing::AccessMode::kAuto) {
      const auto allowed_res = splitting_allowed(sql_parser_state_.lexer());
      if (!allowed_res) {
        auto send_res = ClassicFrame::send_msg<
            classic_protocol::borrowed::message::server::Error>(
            src_conn, {ER_ROUTER_NOT_ALLOWED_WITH_CONNECTION_SHARING,
                       allowed_res.error(), "HY000"});
        if (!send_res) return send_client_failed(send_res.error());

        discard_current_msg(src_conn);

        stage(Stage::Done);
        return Result::SendToClient;
      }

      switch (*allowed_res) {
        case SplittingAllowedParser::Allowed::Always:
          break;
        case SplittingAllowedParser::Allowed::Never: {
          auto send_res = ClassicFrame::send_msg<
              classic_protocol::borrowed::message::server::Error>(
              src_conn,
              {ER_ROUTER_NOT_ALLOWED_WITH_CONNECTION_SHARING,
               "Statement not allowed if access_mode is 'auto'", "HY000"});
          if (!send_res) return send_client_failed(send_res.error());

          discard_current_msg(src_conn);

          stage(Stage::Done);
          return Result::SendToClient;
        }
        case SplittingAllowedParser::Allowed::OnlyReadOnly:
        case SplittingAllowedParser::Allowed::OnlyReadWrite:
        case SplittingAllowedParser::Allowed::InTransaction:
          if (!connection()->trx_state() ||
              connection()->trx_state()->trx_type() == '_') {
            auto send_res = ClassicFrame::send_msg<
                classic_protocol::borrowed::message::server::Error>(
                src_conn,
                {ER_ROUTER_NOT_ALLOWED_WITH_CONNECTION_SHARING,
                 "Statement not allowed outside a transaction if access_mode "
                 "is 'auto'",
                 "HY000"});
            if (!send_res) return send_client_failed(send_res.error());

            discard_current_msg(src_conn);

            stage(Stage::Done);
            return Result::SendToClient;
          }
          break;
      }
    }

    if (!connection()->trx_state()) {
      // no trx state, no trx.
      stage(Stage::ClassifyQuery);
    } else {
      auto is_implictly_committed_res = is_implicitly_committed(
          sql_parser_state_.lexer(), connection()->trx_state());
      if (!is_implictly_committed_res) {
        // it fails if trx-state() is not set, but it has been set.
        harness_assert_this_should_not_execute();
      } else if (*is_implictly_committed_res) {
        auto &server_conn = connection()->server_conn();
        if (!server_conn.is_open()) {
          trace_event_connect_and_explicit_commit_ =
              trace_connect_and_explicit_commit(trace_event_command_);
          stage(Stage::ExplicitCommitConnect);
        } else {
          stage(Stage::ExplicitCommit);
        }
      } else {
        // not implicitly committed.
        stage(Stage::ClassifyQuery);
      }
    }

    return Result::Again;
  }
}

TraceEvent *QueryForwarder::trace_connect_and_explicit_commit(
    TraceEvent *parent_span) {
  auto *ev = trace_span(parent_span, "mysql/connect_and_explicit_commit");
  if (ev == nullptr) return nullptr;

  trace_set_connection_attributes(ev);

  return ev;
}

// connect to the old backend if needed before sending the COMMIT.
stdx::expected<Processor::Result, std::error_code>
QueryForwarder::explicit_commit_connect() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("query::explicit_commit::connect"));
  }

  stage(Stage::ExplicitCommitConnectDone);
  return mysql_reconnect_start(trace_event_connect_and_explicit_commit_);
}

stdx::expected<Processor::Result, std::error_code>
QueryForwarder::explicit_commit_connect_done() {
  auto &server_conn = connection()->server_conn();
  if (!server_conn.is_open()) {
    auto &src_conn = connection()->client_conn();

    discard_current_msg(src_conn);

    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("query::explicit_commit::connect::error"));
    }

    trace_span_end(trace_event_connect_and_explicit_commit_);
    trace_command_end(trace_event_command_);

    stage(Stage::Done);
    return reconnect_send_error_msg(src_conn);
  }

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("query::explicit_commit::connect::done"));
  }

  stage(Stage::ExplicitCommit);

  return Result::Again;
}

// explicitly COMMIT the transaction as the current statement would do an
// implicit COMMIT.
stdx::expected<Processor::Result, std::error_code>
QueryForwarder::explicit_commit() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("query::explicit_commit::commit"));
  }

  auto &dst_protocol = connection()->server_conn().protocol();

  // reset the seq-id before the command that's pushed.
  dst_protocol.seq_id(0xff);

  stage(Stage::ExplicitCommitDone);

  connection()->push_processor(std::make_unique<QuerySender>(
      connection(), "COMMIT", std::make_unique<FailedQueryHandler>(*this)));

  return Result::Again;
}

// check if the COMMIT succeeded.
stdx::expected<Processor::Result, std::error_code>
QueryForwarder::explicit_commit_done() {
  auto &dst_protocol = connection()->server_conn().protocol();

  if (auto err = failed()) {
    auto &src_conn = connection()->client_conn();

    discard_current_msg(src_conn);

    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("query::explicit_commit::error"));
    }

    auto send_msg = ClassicFrame::send_msg(src_conn, *err);
    if (!send_msg) send_client_failed(send_msg.error());

    trace_span_end(trace_event_connect_and_explicit_commit_);
    trace_command_end(trace_event_command_);

    stage(Stage::Done);

    return Result::Again;
  }

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("query::explicit_commit::done"));
  }

  // back to the current query.
  stage(Stage::ClassifyQuery);

  // next command with start at 0 again.
  dst_protocol.seq_id(0xff);

  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
QueryForwarder::classify_query() {
  auto &src_conn = connection()->client_conn();
  auto &src_protocol = src_conn.protocol();

  bool want_read_only_connection{false};

  if (true) {
    auto msg_res = ClassicFrame::recv_msg<
        classic_protocol::borrowed::message::client::Query>(src_conn);
    if (!msg_res) {
      // all codec-errors should result in a Malformed Packet error..
      if (msg_res.error().category() !=
          make_error_code(classic_protocol::codec_errc::not_enough_input)
              .category()) {
        return recv_client_failed(msg_res.error());
      }

      discard_current_msg(src_conn);

      auto send_msg =
          ClassicFrame::send_msg<classic_protocol::message::server::Error>(
              src_conn,
              {ER_MALFORMED_PACKET, "Malformed communication packet", "HY000"});
      if (!send_msg) send_client_failed(send_msg.error());

      stage(Stage::Done);

      return Result::SendToClient;
    }

    // not a SHOW WARNINGS or so, reset the warnings.
    connection()->execution_context().diagnostics_area().warnings().clear();
    connection()->events().clear();
    connection()->wait_for_my_writes(src_protocol.wait_for_my_writes());
    connection()->gtid_at_least_executed(src_protocol.gtid_executed());
    connection()->wait_for_my_writes_timeout(
        src_protocol.wait_for_my_writes_timeout());

    auto collation_connection = connection()
                                    ->execution_context()
                                    .system_variables()
                                    .get("collation_connection")
                                    .value()
                                    .value_or("utf8mb4");

    const CHARSET_INFO *cs_collation_connection =
        get_charset_by_name(collation_connection.c_str(), 0);

    std::optional<ClientSideClassicProtocolState::AccessMode> access_mode;
    for (const auto &param : msg_res->values()) {
      std::string param_name(param.name);

      if (0 == my_strcasecmp(cs_collation_connection, param_name.c_str(),
                             "router.trace")) {
        if (param.value) {
          auto val_res = param_to_number(param);
          if (val_res) {
            switch (*val_res) {
              case 0:
              case 1:
                connection()->events().active(*val_res != 0);
                break;
              default:
                discard_current_msg(src_conn);

                auto send_res = ClassicFrame::send_msg<
                    classic_protocol::message::server::Error>(
                    src_conn,
                    {1064, "Query attribute 'router.trace' requires 0 or 1",
                     "42000"});
                if (!send_res) return send_client_failed(send_res.error());

                stage(Stage::Done);
                return Result::SendToClient;
            }
          } else {
            discard_current_msg(src_conn);

            auto send_res = ClassicFrame::send_msg<
                classic_protocol::message::server::Error>(
                src_conn,
                {1064, "Query attribute 'router.trace' requires a number",
                 "42000"});
            if (!send_res) return send_client_failed(send_res.error());

            stage(Stage::Done);
            return Result::SendToClient;
          }
        } else {
          discard_current_msg(src_conn);

          auto send_res =
              ClassicFrame::send_msg<classic_protocol::message::server::Error>(
                  src_conn, {1064, "router.trace requires a value", "42000"});
          if (!send_res) return send_client_failed(send_res.error());

          stage(Stage::Done);
          return Result::SendToClient;
        }
      } else if (0 == my_strcasecmp(cs_collation_connection, param_name.c_str(),
                                    "router.access_mode")) {
        if (param.value) {
          auto param_res = param_as_string(param);
          if (param_res) {
            auto val = *param_res;

            if (val == "read_only") {
              access_mode =
                  ClientSideClassicProtocolState::AccessMode::ReadOnly;
            } else if (val == "read_write") {
              access_mode =
                  ClientSideClassicProtocolState::AccessMode::ReadWrite;
            } else if (val == "auto") {
              access_mode = std::nullopt;
            } else {
              // unknown router.access_mode value.
              discard_current_msg(src_conn);

              auto send_res = ClassicFrame::send_msg<
                  classic_protocol::message::server::Error>(
                  src_conn,
                  {1064,
                   "Value of Query attribute " + param_name + " is unknown",
                   "42000"});
              if (!send_res) return send_client_failed(send_res.error());

              stage(Stage::Done);
              return Result::SendToClient;
            }
          } else {
            // router.access_mode has invalid value.
            discard_current_msg(src_conn);

            auto send_res = ClassicFrame::send_msg<
                classic_protocol::message::server::Error>(
                src_conn,
                {1064, "Value of Query attribute " + param_name + " is unknown",
                 "42000"});
            if (!send_res) return send_client_failed(send_res.error());

            stage(Stage::Done);
            return Result::SendToClient;
          }
        } else {
          // NULL, ignore
        }
      } else if (0 == my_strcasecmp(cs_collation_connection, param_name.c_str(),
                                    "router.wait_for_my_writes")) {
        if (param.value) {
          auto val_res = param_to_number(param);
          if (val_res) {
            if (*val_res == 0 || *val_res == 1) {
              connection()->wait_for_my_writes(*val_res == 1);
            } else {
              // router.wait_for_my_writes has invalid value.
              discard_current_msg(src_conn);

              auto send_res = ClassicFrame::send_msg<
                  classic_protocol::borrowed::message::server::Error>(
                  src_conn,
                  {1064,
                   "Value of Query attribute " + param_name + " is unknown",
                   "42000"});
              if (!send_res) return send_client_failed(send_res.error());

              stage(Stage::Done);
              return Result::SendToClient;
            }
          } else {
            // router.wait_for_my_writes has invalid type.
            discard_current_msg(src_conn);

            auto send_res = ClassicFrame::send_msg<
                classic_protocol::borrowed::message::server::Error>(
                src_conn,
                {1064, "Value of Query attribute " + param_name + " is unknown",
                 "42000"});
            if (!send_res) return send_client_failed(send_res.error());

            stage(Stage::Done);
            return Result::SendToClient;
          }
        } else {
          // NULL, ignore
        }
      } else if (0 == my_strcasecmp(cs_collation_connection, param_name.c_str(),
                                    "router.wait_for_my_writes_timeout")) {
        if (param.value) {
          auto val_res = param_to_number(param);
          if (val_res) {
            if (*val_res <= 3600) {
              connection()->wait_for_my_writes_timeout(
                  std::chrono::seconds(*val_res));
            } else {
              // router.wait_for_my_writes_timeout has invalid type.
              discard_current_msg(src_conn);

              auto send_res = ClassicFrame::send_msg<
                  classic_protocol::borrowed::message::server::Error>(
                  src_conn,
                  {1064,
                   "Value of Query attribute " + param_name + " is unknown",
                   "42000"});
              if (!send_res) return send_client_failed(send_res.error());

              stage(Stage::Done);
              return Result::SendToClient;
            }
          } else {
            // router.wait_for_my_writes_timeout has invalid type.
            discard_current_msg(src_conn);

            auto send_res = ClassicFrame::send_msg<
                classic_protocol::borrowed::message::server::Error>(
                src_conn,
                {1064, "Value of Query attribute " + param_name + " is unknown",
                 "42000"});
            if (!send_res) return send_client_failed(send_res.error());

            stage(Stage::Done);
            return Result::SendToClient;
          }
        } else {
          // NULL, ignore
        }
      } else {
        const char router_prefix[] = "router.";

        std::string param_prefix =
            param_name.substr(0, sizeof(router_prefix) - 1);

        if (0 == my_strcasecmp(cs_collation_connection, param_prefix.c_str(),
                               router_prefix)) {
          // unknown router. query-attribute.
          discard_current_msg(src_conn);

          auto send_res =
              ClassicFrame::send_msg<classic_protocol::message::server::Error>(
                  src_conn,
                  {1064, "Query attribute " + param_name + " is unknown",
                   "42000"});
          if (!send_res) return send_client_failed(send_res.error());

          stage(Stage::Done);
          return Result::SendToClient;
        }
      }
    }

    stmt_classified_ = classify(
        sql_parser_state_.lexer(), true,
        connection()->context().access_mode() == routing::AccessMode::kAuto);

    enum class ReadOnlyDecider {
      Session,
      TrxState,
      QueryAttribute,
      Statement,
    } read_only_decider{ReadOnlyDecider::TrxState};

    auto read_only_decider_to_string = [](ReadOnlyDecider v) -> std::string {
      switch (v) {
        case ReadOnlyDecider::Session:
          return "session";
        case ReadOnlyDecider::TrxState:
          return "trx-state";
        case ReadOnlyDecider::QueryAttribute:
          return "query-attribute";
        case ReadOnlyDecider::Statement:
          return "statement";
      }

      harness_assert_this_should_not_execute();
    };

    if (src_protocol.access_mode()) {
      // access-mode set explicitly via ROUTER SET ...
      want_read_only_connection =
          (src_protocol.access_mode() ==
           ClientSideClassicProtocolState::AccessMode::ReadOnly);
      read_only_decider = ReadOnlyDecider::Session;
    } else {
      bool some_trx_state{false};
      bool in_read_only_trx{false};

      const auto &sysvars =
          connection()->execution_context().system_variables();

      // check the server's trx-characteristics if:
      //
      // - a transaction has been explicitly started
      // - some transaction characteristics have been specified

      const auto trx_char = connection()->trx_characteristics();
      if (trx_char && !trx_char->characteristics().empty()) {
        // some transaction state is set. Either is started or SET TRANSACTION
        // ...
        some_trx_state = true;

        if (ends_with(trx_char->characteristics(),
                      "START TRANSACTION READ ONLY;")) {
          // explicit read-only trx started.
          //
          // can be moved to read-only server even if it was started as it
          // didn't ask for a consistent snapshot.
          in_read_only_trx = true;
        } else if (ends_with(trx_char->characteristics(),
                             "SET TRANSACTION READ ONLY;")) {
          // check if the received statement is an explicit transaction start.
          const auto start_transaction_res =
              start_transaction(sql_parser_state_.lexer());
          if (!start_transaction_res) {
            discard_current_msg(src_conn);

            auto send_res = ClassicFrame::send_msg<
                classic_protocol::message::server::Error>(
                src_conn, {1064, start_transaction_res.error(), "42000"});
            if (!send_res) return send_client_failed(send_res.error());

            stage(Stage::Done);
            return Result::SendToClient;
          }

          if (std::holds_alternative<StartTransaction>(
                  *start_transaction_res)) {
            const auto start_trx =
                std::get<StartTransaction>(*start_transaction_res);

            if (auto access_mode = start_trx.access_mode()) {
              // READ ONLY or READ WRITE explicitely specified.
              in_read_only_trx =
                  (*access_mode == StartTransaction::AccessMode::ReadOnly);
            } else {
              in_read_only_trx = true;
            }
          }  // otherwise no START TRANSACTION or BEGIN
        }
      } else {
        // no trx-state yet.

        // check if the received statement is an explicit transaction start.
        const auto start_transaction_res =
            start_transaction(sql_parser_state_.lexer());
        if (!start_transaction_res) {
          discard_current_msg(src_conn);

          const auto send_res =
              ClassicFrame::send_msg<classic_protocol::message::server::Error>(
                  src_conn, {1064, start_transaction_res.error(), "42000"});
          if (!send_res) return send_client_failed(send_res.error());

          stage(Stage::Done);
          return Result::SendToClient;
        }

        if (std::holds_alternative<StartTransaction>(*start_transaction_res)) {
          some_trx_state = true;

          const auto start_trx =
              std::get<StartTransaction>(*start_transaction_res);

          if (auto access_mode = start_trx.access_mode()) {
            // READ ONLY or READ WRITE explicitely specified.
            in_read_only_trx =
                (*access_mode == StartTransaction::AccessMode::ReadOnly);
          } else {
            // if there is a SET TRANSACTION READ ONLY ...
            if (set_transaction_contains_read_only(
                    connection()->trx_characteristics())) {
              in_read_only_trx = true;
            }
          }

          // ignore
          //
          //   SET SESSION transaction_read_only = 1;
          //
          // as it should be handled by the server.
        } else {
          // ... or an implicit transaction start.
          auto autocommit_res = sysvars.get("autocommit").value();

          // if autocommit is off, there is always some transaction which should
          // be sent to the read-write server.
          if (autocommit_res && autocommit_res == "OFF") {
            some_trx_state = true;
          }
        }
      }

      // if autocommit is disabled, treat it as read-write transaction.
      auto autocommit_res = connection()
                                ->execution_context()
                                .system_variables()
                                .get("autocommit")
                                .value();
      if (autocommit_res && autocommit_res == "OFF") {
        some_trx_state = true;
      }

      if (some_trx_state) {
        want_read_only_connection = in_read_only_trx;
        read_only_decider = ReadOnlyDecider::TrxState;

        if (access_mode) {
          discard_current_msg(src_conn);

          auto send_res =
              ClassicFrame::send_msg<classic_protocol::message::server::Error>(
                  src_conn,
                  {ER_VARIABLE_NOT_SETTABLE_IN_TRANSACTION,
                   "Query attribute router.access_mode not allowed inside a "
                   "transaction.",
                   "42000"});
          if (!send_res) return send_client_failed(send_res.error());

          stage(Stage::Done);
          return Result::SendToClient;
        }
      } else if (access_mode) {
        // access-mode set via query-attributes.
        want_read_only_connection =
            (*access_mode ==
             ClientSideClassicProtocolState::AccessMode::ReadOnly);
        read_only_decider = ReadOnlyDecider::QueryAttribute;
      } else {
        // automatically detected.
        want_read_only_connection = stmt_classified_ & StmtClassifier::ReadOnly;
        read_only_decider = ReadOnlyDecider::Statement;
      }
    }

    trace_event_command_ = trace_command(prefix());
    if (auto *ev = trace_event_command_) {
      ev->attrs.emplace_back("mysql.session_is_read_only",
                             want_read_only_connection);
      ev->attrs.emplace_back("mysql.session_is_read_only_decider",
                             read_only_decider_to_string(read_only_decider));
    }

    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage(
          "query::classified: " + to_string(stmt_classified_) +
          ", use-read-only-decided-by=" +
          read_only_decider_to_string(read_only_decider)));
    }

    if (auto *ev = trace_span(trace_event_command_, "mysql/query_classify")) {
      ev->attrs.emplace_back("mysql.query.classification",
                             to_string(stmt_classified_));
    }

    // SET session_track... is forbidden if router sets session-trackers on the
    // server-side.
    if ((stmt_classified_ & StmtClassifier::ForbiddenSetWithConnSharing) &&
        connection()->connection_sharing_possible()) {
      discard_current_msg(src_conn);

      if (auto &tr = tracer()) {
        tr.trace(Tracer::Event().stage("query::forbidden"));
      }

      auto send_res = ClassicFrame::send_msg<
          classic_protocol::borrowed::message::server::Error>(
          src_conn,
          {ER_VARIABLE_NOT_SETTABLE_IN_TRANSACTION,
           "The system variable cannot be set when connection sharing is "
           "enabled",
           "HY000"});
      if (!send_res) return send_client_failed(send_res.error());

      stage(Stage::Done);
      return Result::SendToClient;
    }

    // functions are forbidden if the connection can be shared
    // (e.g. config allows sharing and outside a transaction)
    if ((stmt_classified_ & StmtClassifier::ForbiddenFunctionWithConnSharing) &&
        connection()->connection_sharing_allowed()) {
      discard_current_msg(src_conn);

      if (auto &tr = tracer()) {
        tr.trace(Tracer::Event().stage("query::forbidden"));
      }
      auto send_res = ClassicFrame::send_msg<
          classic_protocol::borrowed::message::server::Error>(
          src_conn,
          {ER_NO_ACCESS_TO_NATIVE_FCT,
           "Access to native function is rejected when connection sharing is "
           "enabled",
           "HY000"});
      if (!send_res) return send_client_failed(send_res.error());

      stage(Stage::Done);
      return Result::SendToClient;
    }
  }

  trace_event_connect_and_forward_command_ =
      trace_connect_and_forward_command(trace_event_command_);

  stage(Stage::PrepareBackend);

  if (connection()->connection_sharing_allowed() &&
      // only switch backends if access-mode is 'auto'
      connection()->context().access_mode() == routing::AccessMode::kAuto) {
    if ((want_read_only_connection && connection()->expected_server_mode() ==
                                          mysqlrouter::ServerMode::ReadWrite) ||
        (!want_read_only_connection && connection()->expected_server_mode() ==
                                           mysqlrouter::ServerMode::ReadOnly)) {
      connection()->expected_server_mode(
          want_read_only_connection ? mysqlrouter::ServerMode::ReadOnly
                                    : mysqlrouter::ServerMode::ReadWrite);

      // as the connection will be switched, get rid of this connection.
      connection()->stash_server_conn();

      stage(Stage::SwitchBackend);
    }
  }

  return Result::Again;
}

// switch to the new backend.
stdx::expected<Processor::Result, std::error_code>
QueryForwarder::switch_backend() {
  stage(Stage::PrepareBackend);

  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
QueryForwarder::prepare_backend() {
  if (!connection()->server_conn().is_open()) {
    stage(Stage::Connect);
  } else {
    trace_event_forward_command_ =
        trace_forward_command(trace_event_connect_and_forward_command_);
    stage(Stage::Forward);
  }

  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code> QueryForwarder::connect() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage(
        "query::connect: " +
        std::string(connection()->expected_server_mode() ==
                            mysqlrouter::ServerMode::ReadOnly
                        ? "ro"
                        : "rw-or-nothing")));
  }

  stage(Stage::Connected);
  return mysql_reconnect_start(trace_event_connect_and_forward_command_);
}

stdx::expected<Processor::Result, std::error_code> QueryForwarder::connected() {
  auto &server_conn = connection()->server_conn();
  if (!server_conn.is_open()) {
    auto &src_conn = connection()->client_conn();

    // take the client::command from the connection.
    auto msg_res =
        ClassicFrame::recv_msg<classic_protocol::borrowed::wire::String>(
            src_conn);
    if (!msg_res) return recv_client_failed(msg_res.error());

    discard_current_msg(src_conn);

    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("query::connect::error"));
    }

    trace_span_end(trace_event_connect_and_forward_command_);
    trace_command_end(trace_event_command_);

    stage(Stage::Done);
    return reconnect_send_error_msg(src_conn);
  }

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("query::connected"));
  }

  trace_event_forward_command_ =
      trace_forward_command(trace_event_connect_and_forward_command_);

  stage(Stage::Forward);
  return Result::Again;
}

bool has_non_router_attributes(
    const std::vector<classic_protocol::message::client::Query::Param>
        &params) {
  return std::any_of(params.begin(), params.end(), [](const auto &param) {
    std::string_view prefix("router.");

    return std::string_view{param.name}.substr(0, prefix.size()) != prefix;
  });
}

stdx::expected<Processor::Result, std::error_code> QueryForwarder::forward() {
  auto &src_conn = connection()->client_conn();
  auto &src_protocol = src_conn.protocol();

  auto &dst_conn = connection()->server_conn();
  auto &dst_protocol = dst_conn.protocol();

  auto client_caps = src_protocol.shared_capabilities();
  auto server_caps = dst_protocol.shared_capabilities();

  if (client_caps.test(classic_protocol::capabilities::pos::query_attributes) ==
      server_caps.test(classic_protocol::capabilities::pos::query_attributes)) {
    // if caps are the same, forward the message as is
    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("query::forward"));
    }

    stage(Stage::ForwardDone);

    return forward_client_to_server();
  }

  // ... otherwise: recode the message.

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("query::forward::recode"));
  }

  auto msg_res =
      ClassicFrame::recv_msg<classic_protocol::message::client::Query>(
          src_conn);
  if (!msg_res) {
    // all codec-errors should result in Bad Message.
    if (msg_res.error().category() !=
        make_error_code(classic_protocol::codec_errc::not_enough_input)
            .category()) {
      return recv_client_failed(msg_res.error());
    }

    discard_current_msg(src_conn);

    auto send_msg =
        ClassicFrame::send_msg<classic_protocol::message::server::Error>(
            src_conn,
            {ER_MALFORMED_PACKET, "Malformed communication packet", "HY000"});
    if (!send_msg) send_client_failed(send_msg.error());

    trace_span_end(trace_event_connect_and_forward_command_);

    stage(Stage::Done);

    return Result::SendToClient;
  }

  // if the message contains non-"router." attributes, error.
  if (has_non_router_attributes(msg_res->values())) {
    discard_current_msg(src_conn);

    auto send_msg = ClassicFrame::send_msg<
        classic_protocol::message::server::Error>(
        src_conn,
        {ER_MALFORMED_PACKET,
         "Message contains attributes, but server does not support attributes.",
         "HY000"});
    if (!send_msg) send_client_failed(send_msg.error());

    stage(Stage::Done);

    return Result::SendToClient;
  }

  auto send_res = ClassicFrame::send_msg(dst_conn, *msg_res);
  if (!send_res) return send_server_failed(send_res.error());

  discard_current_msg(src_conn);

  stage(Stage::ForwardDone);
  return Result::SendToServer;
}

stdx::expected<Processor::Result, std::error_code>
QueryForwarder::forward_done() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("query::forward::done"));
  }

  trace_span_end(trace_event_forward_command_);
  trace_span_end(trace_event_connect_and_forward_command_);

  stage(Stage::Response);

  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code> QueryForwarder::response() {
  auto &src_conn = connection()->server_conn();
  auto &src_protocol = src_conn.protocol();

  auto read_res = ClassicFrame::ensure_has_msg_prefix(src_conn);
  if (!read_res) {
    return recv_server_failed_and_check_client_socket(read_res.error());
  }

  uint8_t msg_type = src_protocol.current_msg_type().value();

  enum class Msg {
    Error = ClassicFrame::cmd_byte<classic_protocol::message::server::Error>(),
    Ok = ClassicFrame::cmd_byte<classic_protocol::message::server::Ok>(),
    LoadData = 0xfb,
  };

  switch (Msg{msg_type}) {
    case Msg::Error:
      stage(Stage::Error);
      return Result::Again;
    case Msg::Ok:
      stage(Stage::Ok);
      return Result::Again;
    case Msg::LoadData:
      stage(Stage::LoadData);
      return Result::Again;
  }

  stage(Stage::ColumnCount);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code> QueryForwarder::load_data() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("query::load_data"));
  }

  stage(Stage::Data);
  return forward_server_to_client();
}

stdx::expected<Processor::Result, std::error_code> QueryForwarder::data() {
  auto &src_conn = connection()->client_conn();
  auto &src_protocol = src_conn.protocol();

  auto read_res = ClassicFrame::ensure_frame_header(src_conn);
  if (!read_res) return recv_client_failed(read_res.error());

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("query::data"));
  }

  // local-data is finished with an empty packet.
  if (src_protocol.current_frame()->frame_size_ == 4) {
    stage(Stage::Response);
  }

  return forward_client_to_server();
}

stdx::expected<Processor::Result, std::error_code>
QueryForwarder::column_count() {
  auto &src_conn = connection()->server_conn();

  auto msg_res = ClassicFrame::recv_msg<
      classic_protocol::borrowed::message::server::ColumnCount>(src_conn);
  if (!msg_res) return recv_server_failed(msg_res.error());

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("query::column_count"));
  }

  trace_event_query_result_ =
      trace_span(trace_event_command_, "mysql/query_result");

  columns_left_ = msg_res->count();

  stage(Stage::Column);

  return forward_server_to_client(true);
}

stdx::expected<Processor::Result, std::error_code> QueryForwarder::column() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("query::column"));
  }

  if (--columns_left_ == 0) {
    stage(Stage::ColumnEnd);
  }

  return forward_server_to_client(true);
}

stdx::expected<Processor::Result, std::error_code>
QueryForwarder::column_end() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("query::end_of_columns"));
  }

  stage(Stage::RowOrEnd);

  return skip_or_inject_end_of_columns(true);
}

stdx::expected<Processor::Result, std::error_code>
QueryForwarder::row_or_end() {
  auto &src_conn = connection()->server_conn();
  auto &src_protocol = src_conn.protocol();

  auto read_res = ClassicFrame::ensure_has_msg_prefix(src_conn);
  if (!read_res) return recv_server_failed(read_res.error());

  uint8_t msg_type = src_protocol.current_msg_type().value();

  enum class Msg {
    Error = ClassicFrame::cmd_byte<classic_protocol::message::server::Error>(),
    EndOfResult =
        ClassicFrame::cmd_byte<classic_protocol::message::server::Eof>(),
  };

  switch (Msg{msg_type}) {
    case Msg::Error:
      stage(Stage::Error);
      return Result::Again;
    case Msg::EndOfResult:
      // 0xfe is used for:
      //
      // - end-of-rows packet
      // - fields in a row > 16MByte.
      if (src_protocol.current_frame()->frame_size_ < 1024) {
        stage(Stage::RowEnd);
        return Result::Again;
      }
      [[fallthrough]];
    default:
      stage(Stage::Row);
      return Result::Again;
  }
}

stdx::expected<Processor::Result, std::error_code> QueryForwarder::row() {
  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("query::row"));
  }

  stage(Stage::RowOrEnd);
  return forward_server_to_client(true /* noflush */);
}

stdx::expected<Processor::Result, std::error_code> QueryForwarder::row_end() {
  auto &src_conn = connection()->server_conn();
  auto &src_protocol = src_conn.protocol();

  auto &dst_conn = connection()->client_conn();
  auto &dst_protocol = dst_conn.protocol();

  auto msg_res =
      ClassicFrame::recv_msg<classic_protocol::borrowed::message::server::Eof>(
          src_conn);
  if (!msg_res) return recv_server_failed(msg_res.error());

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("query::row_end"));
  }

  auto msg = *msg_res;

  if (!msg.session_changes().empty()) {
    auto track_res = connection()->track_session_changes(
        net::buffer(msg.session_changes()), src_protocol.shared_capabilities());
    if (!track_res) {
      // ignore
    }
  }

  dst_protocol.status_flags(msg.status_flags());

  if (msg.status_flags().test(
          classic_protocol::status::pos::more_results_exist)) {
    stage(Stage::Response);  // another resultset is coming

    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("query::more_resultsets"));
    }

    if (!message_can_be_forwarded_as_is(src_protocol, dst_protocol, msg)) {
      auto send_res = ClassicFrame::send_msg(dst_conn, msg);
      if (!send_res) return stdx::unexpected(send_res.error());

      // msg refers to src-channel's recv-buf. discard after send.
      discard_current_msg(src_conn);

      return Result::Again;  // no need to send this now as there will be more
                             // packets.
    }

    return forward_server_to_client(true);
  }

  if (auto *ev = trace_event_query_result_) {
    ClassicFrame::trace_set_attributes(ev, src_protocol, msg);

    trace_span_end(ev);
  }

  if (stmt_classified_ & StmtClassifier::StateChangeOnSuccess) {
    connection()->some_state_changed(true);
  }

  if (msg.warning_count() > 0) {
    connection()->diagnostic_area_changed(true);
  }

  stage(Stage::ResponseDone);  // once the message is forwarded, we are done.
  if (!connection()->events().empty()) {
    msg.warning_count(msg.warning_count() + 1);
  }

  if (!connection()->events().empty() ||
      !message_can_be_forwarded_as_is(src_protocol, dst_protocol, msg)) {
    auto send_res = ClassicFrame::send_msg(dst_conn, msg);
    if (!send_res) return stdx::unexpected(send_res.error());

    // msg refers to src-channel's recv-buf. discard after send.
    discard_current_msg(src_conn);

    return Result::SendToClient;
  }

  return forward_server_to_client();
}

stdx::expected<Processor::Result, std::error_code> QueryForwarder::ok() {
  auto &src_conn = connection()->server_conn();
  auto &src_protocol = src_conn.protocol();

  auto &dst_conn = connection()->client_conn();
  auto &dst_protocol = dst_conn.protocol();

  auto msg_res =
      ClassicFrame::recv_msg<classic_protocol::borrowed::message::server::Ok>(
          src_conn);
  if (!msg_res) return recv_server_failed(msg_res.error());

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("query::ok"));
  }

  auto msg = *msg_res;

  if (!msg.session_changes().empty()) {
    auto track_res = connection()->track_session_changes(
        net::buffer(msg.session_changes()), src_protocol.shared_capabilities(),
        stmt_classified_ & StmtClassifier::NoStateChangeIgnoreTracker);
    if (!track_res) {
      // ignore
    }
  }

  dst_protocol.status_flags(msg.status_flags());

  if (stmt_classified_ & StmtClassifier::StateChangeOnSuccess) {
    connection()->some_state_changed(true);
  }

  if (msg.status_flags().test(
          classic_protocol::status::pos::more_results_exist)) {
    stage(Stage::Response);  // another resultset is coming
    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("query::more_resultsets"));
    }

    return forward_server_to_client(true);
  }

  if (auto *ev = trace_span(trace_event_command_, "mysql/response")) {
    ClassicFrame::trace_set_attributes(ev, src_protocol, msg);

    trace_span_end(ev);
  }

  if (msg.warning_count() > 0) {
    connection()->diagnostic_area_changed(true);
  } else {
    // there are no warnings on the server side.
    connection()->diagnostic_area_changed(false);
  }

  stage(Stage::ResponseDone);  // once the message is forwarded, we are done.

  if (!connection()->events().empty()) {
    msg.warning_count(msg.warning_count() + 1);
  }

  if (!connection()->events().empty() ||
      !message_can_be_forwarded_as_is(src_protocol, dst_protocol, msg)) {
    auto send_res = ClassicFrame::send_msg(dst_conn, msg);
    if (!send_res) return stdx::unexpected(send_res.error());

    // msg refers to src-channel's recv-buf. discard after send.
    discard_current_msg(src_conn);

    return Result::SendToClient;
  }

  // forward the message AS IS.
  return forward_server_to_client();
}

stdx::expected<Processor::Result, std::error_code> QueryForwarder::error() {
  auto &src_conn = connection()->server_conn();
  auto &src_protocol = src_conn.protocol();

  auto msg_res = ClassicFrame::recv_msg<
      classic_protocol::borrowed::message::server::Error>(src_conn);
  if (!msg_res) return recv_server_failed(msg_res.error());

  auto msg = *msg_res;

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("query::error"));
  }

  if (auto *ev = trace_span(trace_event_command_, "mysql/response")) {
    ClassicFrame::trace_set_attributes(ev, src_protocol, msg);

    trace_span_end(ev);
  }

  trace_command_end(trace_event_command_, TraceEvent::StatusCode::kError);

  if (stmt_classified_ & StmtClassifier::StateChangeOnError) {
    connection()->some_state_changed(true);
  }

  // at least one.
  connection()->diagnostic_area_changed(true);

  stage(Stage::Done);
  return forward_server_to_client();
}

stdx::expected<Processor::Result, std::error_code>
QueryForwarder::response_done() {
  trace_command_end(trace_event_command_);

  stage(Stage::Done);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code>
QueryForwarder::send_queued() {
  stage(Stage::Done);
  return Result::SendToClient;
}
