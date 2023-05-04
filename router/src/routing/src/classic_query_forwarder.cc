/*
  Copyright (c) 2023, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "classic_query_forwarder.h"

#include <charconv>
#include <chrono>
#include <limits>
#include <memory>
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
#include "classic_query_sender.h"
#include "classic_session_tracker.h"
#include "command_router_set.h"
#include "harness_assert.h"
#include "hexify.h"
#include "my_sys.h"  // get_charset_by_name
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/tls_error.h"
#include "mysql/harness/utility/string.h"
#include "mysqld_error.h"  // mysql errors
#include "mysqlrouter/classic_protocol_binary.h"
#include "mysqlrouter/classic_protocol_codec_binary.h"
#include "mysqlrouter/classic_protocol_codec_error.h"
#include "mysqlrouter/classic_protocol_message.h"
#include "mysqlrouter/client_error_code.h"
#include "mysqlrouter/utils.h"  // to_string
#include "show_warnings_parser.h"
#include "sql/lex.h"
#include "sql_exec_context.h"
#include "sql_lexer.h"
#include "sql_lexer_thd.h"
#include "sql_parser.h"

#undef DEBUG_DUMP_TOKENS

static const auto forwarded_status_flags =
    classic_protocol::status::in_transaction |
    classic_protocol::status::in_transaction_readonly |
    classic_protocol::status::autocommit;

/**
 * format a timepoint as json-value (date-time format).
 */
static std::string string_from_timepoint(
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

static bool ieq(const std::string_view &a, const std::string_view &b) {
  return std::equal(a.begin(), a.end(), b.begin(), b.end(),
                    [](char lhs, char rhs) {
                      auto ascii_tolower = [](char c) {
                        return c >= 'A' && c <= 'Z' ? c | 0x20 : c;
                      };
                      return ascii_tolower(lhs) == ascii_tolower(rhs);
                    });
}

stdx::expected<Processor::Result, std::error_code> QueryForwarder::process() {
  switch (stage()) {
    case Stage::Command:
      return command();
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

#ifdef DEBUG_DUMP_TOKENS
static void dump_token(SqlLexer::iterator::Token tkn) {
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
  } else if (tkn.id == END_OF_INPUT) {
    std::cerr << "<END>";
  }
  std::cerr << "\n";
}
#endif

static std::ostream &operator<<(std::ostream &os,
                                stdx::flags<StmtClassifier> flags) {
  bool one{false};
  if (flags & StmtClassifier::ForbiddenFunctionWithConnSharing) {
    one = true;
    os << "forbidden_function_with_connection_sharing";
  }
  if (flags & StmtClassifier::ForbiddenSetWithConnSharing) {
    if (one) os << ",";
    one = true;
    os << "forbidden_set_with_connection_sharing";
  }
  if (flags & StmtClassifier::NoStateChangeIgnoreTracker) {
    if (one) os << ",";
    one = true;
    os << "ignore_session_tracker_some_state_changed";
  }
  if (flags & StmtClassifier::StateChangeOnError) {
    if (one) os << ",";
    one = true;
    os << "session_not_sharable_on_error";
  }
  if (flags & StmtClassifier::StateChangeOnSuccess) {
    if (one) os << ",";
    one = true;
    os << "session_not_sharable_on_success";
  }
  if (flags & StmtClassifier::StateChangeOnTracker) {
    if (one) os << ",";
    one = true;
    os << "accept_session_state_from_session_tracker";
  }

  return os;
}

static bool contains_multiple_statements(const std::string &stmt) {
  MEM_ROOT mem_root;
  THD session;
  session.mem_root = &mem_root;

  {
    Parser_state parser_state;
    parser_state.init(&session, stmt.data(), stmt.size());
    session.m_parser_state = &parser_state;
    SqlLexer lexer(&session);

    for (auto tkn : lexer) {
      if (tkn.id == ';') return true;
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
static stdx::flags<StmtClassifier> classify(const std::string &stmt,
                                            bool forbid_set_trackers) {
  stdx::flags<StmtClassifier> classified{};

  MEM_ROOT mem_root;
  THD session;
  session.mem_root = &mem_root;

  {
    Parser_state parser_state;
    parser_state.init(&session, stmt.data(), stmt.size());
    session.m_parser_state = &parser_state;
    SqlLexer lexer(&session);

    auto lexer_it = lexer.begin();
    if (lexer_it != lexer.end()) {
      auto first = *lexer_it;
      auto last = first;
#ifdef DEBUG_DUMP_TOKENS
      dump_token(first);
#endif

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
          case INSERT_SYM:
          case UPDATE_SYM:
          case DELETE_SYM:
          case DO_SYM:
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
              }

              if (ident == "LAST_INSERT_ID") {
                classified |= StmtClassifier::ForbiddenFunctionWithConnSharing;
              }
            }

            break;
        }

        if (first.id == SET_SYM) {
          if (tkn.id == SET_VAR || tkn.id == EQ) {
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
          }
        } else {
          if (last.id == LEX_HOSTNAME && tkn.id == SET_VAR) {  // :=
            classified |= StmtClassifier::StateChangeOnSuccess;
            classified |= StmtClassifier::StateChangeOnError;
          }
        }
        last = tkn;
      }

      if (first.id == SET_SYM || first.id == USE_SYM) {
        if (!classified) {
          return StmtClassifier::NoStateChangeIgnoreTracker;
        } else {
          return classified;
        }
      } else {
        if (!classified) {
          return StmtClassifier::StateChangeOnTracker;
        } else {
          return classified;
        }
      }
    }
  }

  // unknown or empty statement.
  return StmtClassifier::StateChangeOnTracker;
}

static uint64_t get_error_count(MysqlRoutingClassicConnectionBase *connection) {
  uint64_t count{};
  for (auto const &w :
       connection->execution_context().diagnostics_area().warnings()) {
    if (w.level() == "Error") ++count;
  }

  return count;
}

static uint64_t get_warning_count(
    MysqlRoutingClassicConnectionBase *connection) {
  return connection->execution_context().diagnostics_area().warnings().size() +
         (connection->events().events().empty() ? 0 : 1);
}

static stdx::expected<void, std::error_code> send_resultset(
    Channel *src_channel, ClassicProtocolState *src_protocol,
    std::vector<classic_protocol::message::server::ColumnMeta> columns,
    std::vector<classic_protocol::message::server::Row> rows) {
  {
    const auto send_res = ClassicFrame::send_msg<
        classic_protocol::borrowed::message::server::ColumnCount>(
        src_channel, src_protocol, {columns.size()});
    if (!send_res) return stdx::make_unexpected(send_res.error());
  }

  for (auto const &col : columns) {
    const auto send_res =
        ClassicFrame::send_msg(src_channel, src_protocol, col);
    if (!send_res) return stdx::make_unexpected(send_res.error());
  }

  for (auto const &row : rows) {
    const auto send_res =
        ClassicFrame::send_msg(src_channel, src_protocol, row);
    if (!send_res) return stdx::make_unexpected(send_res.error());
  }

  {
    const auto send_res = ClassicFrame::send_msg<
        classic_protocol::borrowed::message::server::Eof>(
        src_channel, src_protocol,
        {src_protocol->status_flags() & forwarded_status_flags, 0});
    if (!send_res) return stdx::make_unexpected(send_res.error());
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

static stdx::expected<void, std::error_code> show_count(
    MysqlRoutingClassicConnectionBase *connection, const char *name,
    uint64_t count) {
  auto *socket_splicer = connection->socket_splicer();
  auto src_channel = socket_splicer->client_channel();
  auto src_protocol = connection->client_protocol();

  auto send_res =
      send_resultset(src_channel, src_protocol,
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
  if (!send_res) return stdx::make_unexpected(send_res.error());

  return {};
}

static const char *show_warning_count_name(
    ShowWarningCount::Verbosity verbosity, ShowWarningCount::Scope scope) {
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

static stdx::expected<void, std::error_code> show_warning_count(
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

static stdx::expected<void, std::error_code> show_warnings(
    MysqlRoutingClassicConnectionBase *connection,
    ShowWarnings::Verbosity verbosity, uint64_t row_count, uint64_t offset) {
  auto *socket_splicer = connection->socket_splicer();
  auto src_channel = socket_splicer->client_channel();
  auto src_protocol = connection->client_protocol();

  // character_set_results
  uint8_t collation = 0xff;  // utf8

  auto send_res = send_resultset(
      src_channel, src_protocol,
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
  if (!send_res) return stdx::make_unexpected(send_res.error());

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

static stdx::expected<void, std::error_code> execute_command_router_set_trace(
    MysqlRoutingClassicConnectionBase *connection,
    const CommandRouterSet &cmd) {
  auto *socket_splicer = connection->socket_splicer();
  auto *src_channel = socket_splicer->client_channel();
  auto *src_protocol = connection->client_protocol();

  if (std::holds_alternative<int64_t>(cmd.value())) {
    auto val = std::get<int64_t>(cmd.value());

    switch (val) {
      case 0:
      case 1: {
        connection->client_protocol()->trace_commands(val != 0);

        auto send_res =
            ClassicFrame::send_msg<classic_protocol::message::server::Ok>(
                src_channel, src_protocol,
                {0, 0, src_protocol->status_flags() & forwarded_status_flags,
                 0});
        if (!send_res) return stdx::make_unexpected(send_res.error());

        return {};
      }

      default: {
        auto send_res =
            ClassicFrame::send_msg<classic_protocol::message::server::Error>(
                src_channel, src_protocol,
                {ER_WRONG_VALUE_FOR_VAR,
                 "Variable '" + cmd.name() +
                     "' can't be set to the value of '" + std::to_string(val) +
                     "'",
                 "42000"});
        if (!send_res) return stdx::make_unexpected(send_res.error());

        return {};
      }
    };
  }

  auto send_res =
      ClassicFrame::send_msg<classic_protocol::message::server::Error>(
          src_channel, src_protocol,
          {ER_WRONG_VALUE_FOR_VAR,
           "Variable '" + cmd.name() + "' can't be set. Expected an integer.",
           "42000"});
  if (!send_res) return stdx::make_unexpected(send_res.error());

  return {};
}

/*
 * ROUTER SET <key> = <value>
 *
 * @retval expected        done
 * @retval unexpected      fatal-error
 */
static stdx::expected<void, std::error_code> execute_command_router_set(
    MysqlRoutingClassicConnectionBase *connection,
    const CommandRouterSet &cmd) {
  auto *socket_splicer = connection->socket_splicer();
  auto *src_channel = socket_splicer->client_channel();
  auto *src_protocol = connection->client_protocol();

  if (Name_string(cmd.name().c_str()).eq("trace")) {
    return execute_command_router_set_trace(connection, cmd);
  }

  auto send_res =
      ClassicFrame::send_msg<classic_protocol::message::server::Error>(
          src_channel, src_protocol,
          {ER_UNKNOWN_SYSTEM_VARIABLE,
           "Unknown Router system variable '" + cmd.name() + "'", "HY000"});
  if (!send_res) return stdx::make_unexpected(send_res.error());

  return {};
}

class InterceptedStatementsParser : public ShowWarningsParser {
 public:
  using ShowWarningsParser::ShowWarningsParser;

  stdx::expected<std::variant<std::monostate, ShowWarningCount, ShowWarnings,
                              CommandRouterSet>,
                 std::string>
  parse() {
    if (accept(SHOW)) {
      if (accept(WARNINGS)) {
        stdx::expected<Limit, std::string> limit_res;

        if (accept(LIMIT)) {  // optional limit
          limit_res = limit();
        }

        if (accept(END_OF_INPUT)) {
          if (limit_res) {
            return {std::in_place,
                    ShowWarnings{ShowWarnings::Verbosity::Warning,
                                 limit_res->row_count, limit_res->offset}};
          }

          return {std::in_place,
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
            return {std::in_place,
                    ShowWarnings{ShowWarnings::Verbosity::Error,
                                 limit_res->row_count, limit_res->offset}};
          }

          return {std::in_place, ShowWarnings{ShowWarnings::Verbosity::Error}};
        }

        // unexpected input after SHOW ERRORS [LIMIT ...]
        return {};
      } else if (accept(COUNT_SYM) && accept('(') && accept('*') &&
                 accept(')')) {
        if (accept(WARNINGS)) {
          if (accept(END_OF_INPUT)) {
            return {std::in_place,
                    ShowWarningCount{ShowWarnings::Verbosity::Warning,
                                     ShowWarningCount::Scope::Session}};
          }

          // unexpected input after SHOW COUNT(*) WARNINGS
          return {};
        } else if (accept(ERRORS)) {
          if (accept(END_OF_INPUT)) {
            return {std::in_place,
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
                return {std::in_place,
                        ShowWarningCount(*ident_res,
                                         ShowWarningCount::Scope::Session)};
              }
            }
          } else if (accept(LOCAL_SYM)) {
            if (accept('.')) {
              auto ident_res = warning_count_ident();
              if (ident_res && accept(END_OF_INPUT)) {
                return {std::in_place,
                        ShowWarningCount(*ident_res,
                                         ShowWarningCount::Scope::Local)};
              }
            }
          } else {
            auto ident_res = warning_count_ident();
            if (ident_res && accept(END_OF_INPUT)) {
              return {
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
                  return {std::in_place,
                          CommandRouterSet(name_tkn.text(), *val)};
                } else {
                  return stdx::make_unexpected(
                      "ROUTER SET <name> = <value>. Extra data.");
                }
              } else {
                return stdx::make_unexpected(
                    "ROUTER SET <name> = expected <value>. " + error_);
              }
            } else {
              return stdx::make_unexpected("ROUTER SET <name> expects =");
            }
          } else {
            return stdx::make_unexpected("ROUTER SET expects <name>.");
          }
        } else {
          return stdx::make_unexpected("ROUTER expects SET.");
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
    if (accept(TRUE_SYM)) return {std::in_place, true};
    if (accept(FALSE_SYM)) return {std::in_place, false};

    if (accept('-')) {
      if (auto num_tkn = expect(NUM)) {
        auto num = sv_to_num<int64_t>(num_tkn.text());
        return {std::in_place, -num};
      }
    } else if (auto tkn = accept(NUM)) {
      auto num = sv_to_num<uint64_t>(tkn.text());
      return {std::in_place, static_cast<int64_t>(num)};
    } else if (auto tkn = accept(TEXT_STRING)) {
      return {std::in_place, std::string(tkn.text())};
    } else {
      return stdx::make_unexpected("Expected <BOOL>, <NUM> or <STRING>");
    }

    return stdx::make_unexpected(error_);
  }
};

static stdx::expected<std::variant<std::monostate, ShowWarningCount,
                                   ShowWarnings, CommandRouterSet>,
                      std::string>
intercept_diagnostics_area_queries(std::string_view stmt) {
  MEM_ROOT mem_root;
  THD session;
  session.mem_root = &mem_root;

  {
    Parser_state parser_state;
    parser_state.init(&session, stmt.data(), stmt.size());
    session.m_parser_state = &parser_state;
    SqlLexer lexer{&session};

    return InterceptedStatementsParser(lexer.begin(), lexer.end()).parse();
  }
}

template <class T>
static constexpr uint16_t binary_type() {
  return classic_protocol::Codec<T>::type();
}

namespace classic_protocol::borrowable::binary {
template <bool Borrowed>
std::ostream &operator<<(
    std::ostream &os,
    const classic_protocol::borrowable::binary::String<Borrowed> &v) {
  os << std::quoted(v.value());
  return os;
}

template <bool Borrowed>
std::ostream &operator<<(
    std::ostream &os,
    const classic_protocol::borrowable::binary::Json<Borrowed> &v) {
  os << v.value();
  return os;
}

template <bool Borrowed>
std::ostream &operator<<(
    std::ostream &os,
    const classic_protocol::borrowable::binary::Varchar<Borrowed> &v) {
  os << std::quoted(v.value());
  return os;
}

template <bool Borrowed>
std::ostream &operator<<(
    std::ostream &os,
    const classic_protocol::borrowable::binary::VarString<Borrowed> &v) {
  os << std::quoted(v.value());
  return os;
}

template <bool Borrowed>
std::ostream &operator<<(
    std::ostream &os,
    const classic_protocol::borrowable::binary::Decimal<Borrowed> &v) {
  os << v.value();
  return os;
}

template <bool Borrowed>
std::ostream &operator<<(
    std::ostream &os,
    const classic_protocol::borrowable::binary::NewDecimal<Borrowed> &v) {
  os << v.value();
  return os;
}

std::ostream &operator<<(
    std::ostream &os, const classic_protocol::borrowable::binary::Double &v) {
  os << v.value();
  return os;
}

std::ostream &operator<<(std::ostream &os,
                         const classic_protocol::borrowable::binary::Float &v) {
  os << v.value();
  return os;
}

std::ostream &operator<<(std::ostream &os,
                         const classic_protocol::borrowable::binary::Tiny &v) {
  os << static_cast<unsigned int>(v.value());
  return os;
}

std::ostream &operator<<(std::ostream &os,
                         const classic_protocol::borrowable::binary::Int24 &v) {
  os << v.value();
  return os;
}

std::ostream &operator<<(std::ostream &os,
                         const classic_protocol::borrowable::binary::Short &v) {
  os << v.value();
  return os;
}

std::ostream &operator<<(std::ostream &os,
                         const classic_protocol::borrowable::binary::Long &v) {
  os << v.value();
  return os;
}

std::ostream &operator<<(
    std::ostream &os, const classic_protocol::borrowable::binary::LongLong &v) {
  os << v.value();
  return os;
}

template <bool Borrowed>
std::ostream &operator<<(
    std::ostream &os,
    const classic_protocol::borrowable::binary::TinyBlob<Borrowed> &v) {
  os << std::quoted(v.value());
  return os;
}

template <bool Borrowed>
std::ostream &operator<<(
    std::ostream &os,
    const classic_protocol::borrowable::binary::Blob<Borrowed> &v) {
  os << std::quoted(v.value());
  return os;
}

template <bool Borrowed>
std::ostream &operator<<(
    std::ostream &os,
    const classic_protocol::borrowable::binary::MediumBlob<Borrowed> &v) {
  os << std::quoted(v.value());
  return os;
}

template <bool Borrowed>
std::ostream &operator<<(
    std::ostream &os,
    const classic_protocol::borrowable::binary::LongBlob<Borrowed> &v) {
  os << std::quoted(v.value());
  return os;
}

std::ostream &operator<<(
    std::ostream &os,
    const classic_protocol::borrowable::binary::DatetimeBase &v) {
  std::ostringstream oss;

  oss << std::setfill('0')  //
      << std::setw(4) << v.year() << "-" << std::setw(2)
      << static_cast<unsigned int>(v.month()) << "-" << std::setw(2)
      << static_cast<unsigned int>(v.day());
  if (v.hour() || v.minute() || v.second() || v.microsecond()) {
    oss << " " << std::setw(2) << static_cast<unsigned int>(v.hour()) << ":"
        << std::setw(2) << static_cast<unsigned int>(v.minute()) << ":"
        << std::setw(2) << static_cast<unsigned int>(v.second());

    if (v.microsecond()) {
      oss << "." << std::setw(6) << v.microsecond();
    }
  }

  os << oss.str();
  return os;
}

std::ostream &operator<<(std::ostream &os,
                         const classic_protocol::binary::Time &v) {
  std::ostringstream oss;

  oss << std::setfill('0')  //
      << static_cast<unsigned int>(v.days()) << "d " << std::setw(2)
      << static_cast<unsigned int>(v.hour()) << ":" << std::setw(2)
      << static_cast<unsigned int>(v.minute()) << ":" << std::setw(2)
      << static_cast<unsigned int>(v.second());

  if (v.microsecond()) {
    oss << "." << std::setw(6) << v.microsecond();
  }

  os << oss.str();
  return os;
}
}  // namespace classic_protocol::borrowable::binary

static stdx::expected<std::string, std::error_code> param_to_string(
    const classic_protocol::message::client::Query::Param &p) {
  enum class BinaryType {
    Decimal = binary_type<classic_protocol::binary::Decimal>(),
    NewDecimal = binary_type<classic_protocol::binary::NewDecimal>(),
    Double = binary_type<classic_protocol::binary::Double>(),
    Float = binary_type<classic_protocol::binary::Float>(),
    LongLong = binary_type<classic_protocol::binary::LongLong>(),
    Long = binary_type<classic_protocol::binary::Long>(),
    Int24 = binary_type<classic_protocol::binary::Int24>(),
    Short = binary_type<classic_protocol::binary::Short>(),
    Tiny = binary_type<classic_protocol::binary::Tiny>(),
    String = binary_type<classic_protocol::binary::String>(),
    Varchar = binary_type<classic_protocol::binary::Varchar>(),
    VarString = binary_type<classic_protocol::binary::VarString>(),
    MediumBlob = binary_type<classic_protocol::binary::MediumBlob>(),
    TinyBlob = binary_type<classic_protocol::binary::TinyBlob>(),
    Blob = binary_type<classic_protocol::binary::Blob>(),
    LongBlob = binary_type<classic_protocol::binary::LongBlob>(),
    Json = binary_type<classic_protocol::binary::Json>(),
    Date = binary_type<classic_protocol::binary::Date>(),
    DateTime = binary_type<classic_protocol::binary::DateTime>(),
    Timestamp = binary_type<classic_protocol::binary::Timestamp>(),
    Time = binary_type<classic_protocol::binary::Time>(),
  };

  std::ostringstream oss;

  auto type = p.type_and_flags & 0xff;

  oss << "<" << type << "> ";

  switch (BinaryType{type}) {
    case BinaryType::Double: {
      auto decode_res =
          classic_protocol::decode<classic_protocol::binary::Double>(
              net::buffer(*p.value), {});
      if (!decode_res) return stdx::make_unexpected(decode_res.error());

      oss << decode_res->second;
      break;
    }
    case BinaryType::Float: {
      auto decode_res =
          classic_protocol::decode<classic_protocol::binary::Float>(
              net::buffer(*p.value), {});
      if (!decode_res) return stdx::make_unexpected(decode_res.error());

      oss << decode_res->second;
      break;
    }
    case BinaryType::Tiny: {
      auto decode_res =
          classic_protocol::decode<classic_protocol::binary::Tiny>(
              net::buffer(*p.value), {});
      if (!decode_res) return stdx::make_unexpected(decode_res.error());

      oss << decode_res->second;
      break;
    }
    case BinaryType::Short: {
      auto decode_res =
          classic_protocol::decode<classic_protocol::binary::Short>(
              net::buffer(*p.value), {});
      if (!decode_res) return stdx::make_unexpected(decode_res.error());

      oss << decode_res->second;
      break;
    }
    case BinaryType::Int24: {
      auto decode_res =
          classic_protocol::decode<classic_protocol::binary::Int24>(
              net::buffer(*p.value), {});
      if (!decode_res) return stdx::make_unexpected(decode_res.error());

      oss << decode_res->second;
      break;
    }
    case BinaryType::Long: {
      auto decode_res =
          classic_protocol::decode<classic_protocol::binary::Long>(
              net::buffer(*p.value), {});
      if (!decode_res) return stdx::make_unexpected(decode_res.error());

      oss << decode_res->second;
      break;
    }
    case BinaryType::LongLong: {
      auto decode_res =
          classic_protocol::decode<classic_protocol::binary::LongLong>(
              net::buffer(*p.value), {});
      if (!decode_res) return stdx::make_unexpected(decode_res.error());

      oss << decode_res->second;
      break;
    }
    case BinaryType::String: {
      auto decode_res =
          classic_protocol::decode<classic_protocol::binary::String>(
              net::buffer(*p.value), {});
      if (!decode_res) return stdx::make_unexpected(decode_res.error());

      oss << decode_res->second;
      break;
    }
    case BinaryType::VarString: {
      auto decode_res =
          classic_protocol::decode<classic_protocol::binary::VarString>(
              net::buffer(*p.value), {});
      if (!decode_res) return stdx::make_unexpected(decode_res.error());

      oss << decode_res->second;
      break;
    }
    case BinaryType::Varchar: {
      auto decode_res =
          classic_protocol::decode<classic_protocol::binary::Varchar>(
              net::buffer(*p.value), {});
      if (!decode_res) return stdx::make_unexpected(decode_res.error());

      oss << decode_res->second;
      break;
    }
    case BinaryType::Json: {
      auto decode_res =
          classic_protocol::decode<classic_protocol::binary::Json>(
              net::buffer(*p.value), {});
      if (!decode_res) return stdx::make_unexpected(decode_res.error());

      oss << decode_res->second;
      break;
    }
    case BinaryType::TinyBlob: {
      auto decode_res =
          classic_protocol::decode<classic_protocol::binary::TinyBlob>(
              net::buffer(*p.value), {});
      if (!decode_res) return stdx::make_unexpected(decode_res.error());

      oss << decode_res->second;
      break;
    }
    case BinaryType::MediumBlob: {
      auto decode_res =
          classic_protocol::decode<classic_protocol::binary::MediumBlob>(
              net::buffer(*p.value), {});
      if (!decode_res) return stdx::make_unexpected(decode_res.error());

      oss << decode_res->second;
      break;
    }
    case BinaryType::Blob: {
      auto decode_res =
          classic_protocol::decode<classic_protocol::binary::Blob>(
              net::buffer(*p.value), {});
      if (!decode_res) return stdx::make_unexpected(decode_res.error());

      oss << decode_res->second;
      break;
    }
    case BinaryType::LongBlob: {
      auto decode_res =
          classic_protocol::decode<classic_protocol::binary::LongBlob>(
              net::buffer(*p.value), {});
      if (!decode_res) return stdx::make_unexpected(decode_res.error());

      oss << decode_res->second;
      break;
    }
    case BinaryType::Date: {
      auto decode_res =
          classic_protocol::decode<classic_protocol::binary::Date>(
              net::buffer(*p.value), {});
      if (!decode_res) return stdx::make_unexpected(decode_res.error());

      oss << decode_res->second;
      break;
    }
    case BinaryType::DateTime: {
      auto decode_res =
          classic_protocol::decode<classic_protocol::binary::DateTime>(
              net::buffer(*p.value), {});
      if (!decode_res) return stdx::make_unexpected(decode_res.error());

      oss << decode_res->second;
      break;
    }
    case BinaryType::Timestamp: {
      auto decode_res =
          classic_protocol::decode<classic_protocol::binary::Timestamp>(
              net::buffer(*p.value), {});
      if (!decode_res) return stdx::make_unexpected(decode_res.error());

      oss << decode_res->second;
      break;
    }
    case BinaryType::Time: {
      auto decode_res =
          classic_protocol::decode<classic_protocol::binary::Time>(
              net::buffer(*p.value), {});
      if (!decode_res) return stdx::make_unexpected(decode_res.error());

      oss << decode_res->second;
      break;
    }
    case BinaryType::Decimal: {
      auto decode_res =
          classic_protocol::decode<classic_protocol::binary::Decimal>(
              net::buffer(*p.value), {});
      if (!decode_res) return stdx::make_unexpected(decode_res.error());

      oss << decode_res->second;
      break;
    }
    case BinaryType::NewDecimal: {
      auto decode_res =
          classic_protocol::decode<classic_protocol::binary::NewDecimal>(
              net::buffer(*p.value), {});
      if (!decode_res) return stdx::make_unexpected(decode_res.error());

      oss << decode_res->second;
      break;
    }
  }

  return oss.str();
}

stdx::expected<uint64_t, std::error_code> param_to_number(
    classic_protocol::message::client::Query::Param param) {
  switch (param.type_and_flags & 0xff) {
    case binary_type<classic_protocol::binary::Blob>():
    case binary_type<classic_protocol::binary::TinyBlob>():
    case binary_type<classic_protocol::binary::MediumBlob>():
    case binary_type<classic_protocol::binary::LongBlob>():
    case binary_type<classic_protocol::binary::Varchar>():
    case binary_type<classic_protocol::binary::VarString>():
    case binary_type<classic_protocol::binary::String>(): {
      uint64_t val{};

      auto str = *param.value;

      auto conv_res = std::from_chars(str.data(), str.data() + str.size(), val);
      if (conv_res.ec == std::errc{}) return val;

      return stdx::make_unexpected(make_error_code(conv_res.ec));
    }
    case binary_type<classic_protocol::binary::Tiny>(): {
      auto decode_res =
          classic_protocol::Codec<classic_protocol::binary::Tiny>::decode(
              net::buffer(*param.value), {});

      if (!decode_res) return stdx::make_unexpected(decode_res.error());

      return decode_res->second.value();
    }
    case binary_type<classic_protocol::binary::Short>(): {
      auto decode_res =
          classic_protocol::Codec<classic_protocol::binary::Short>::decode(
              net::buffer(*param.value), {});

      if (!decode_res) return stdx::make_unexpected(decode_res.error());

      return decode_res->second.value();
    }
    case binary_type<classic_protocol::binary::Int24>(): {
      auto decode_res =
          classic_protocol::Codec<classic_protocol::binary::Int24>::decode(
              net::buffer(*param.value), {});

      if (!decode_res) return stdx::make_unexpected(decode_res.error());

      return decode_res->second.value();
    }
    case binary_type<classic_protocol::binary::Long>(): {
      auto decode_res =
          classic_protocol::Codec<classic_protocol::binary::Long>::decode(
              net::buffer(*param.value), {});

      if (!decode_res) return stdx::make_unexpected(decode_res.error());

      return decode_res->second.value();
    }
    case binary_type<classic_protocol::binary::LongLong>(): {
      auto decode_res =
          classic_protocol::Codec<classic_protocol::binary::LongLong>::decode(
              net::buffer(*param.value), {});

      if (!decode_res) return stdx::make_unexpected(decode_res.error());

      return decode_res->second.value();
    }
  }

  // all other types: fail.
  return stdx::make_unexpected(make_error_code(std::errc::bad_message));
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
    auto *socket_splicer = connection_->socket_splicer();
    auto *dst_channel = socket_splicer->client_channel();
    auto *dst_protocol = connection_->client_protocol();

    // forward the message.
    auto send_res =
        ClassicFrame::send_msg<classic_protocol::message::server::ColumnCount>(
            dst_channel, dst_protocol, {count});
    if (!send_res) something_failed_ = true;

    col_count_ = count;

    if (col_count_ != 3) something_failed_ = true;
  }

  void on_column(
      const classic_protocol::message::server::ColumnMeta &col) override {
    auto *socket_splicer = connection_->socket_splicer();
    auto *dst_channel = socket_splicer->client_channel();
    auto *dst_protocol = connection_->client_protocol();

    auto send_res = ClassicFrame::send_msg(dst_channel, dst_protocol, col);
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
  }

  void on_row(const classic_protocol::message::server::Row &msg) override {
    auto *socket_splicer = connection_->socket_splicer();
    auto *dst_channel = socket_splicer->client_channel();
    auto *dst_protocol = connection_->client_protocol();

    auto send_res = ClassicFrame::send_msg(dst_channel, dst_protocol, msg);
    if (!send_res) something_failed_ = true;
  }

  // end of rows.
  void on_row_end(const classic_protocol::message::server::Eof &msg) override {
    auto *socket_splicer = connection_->socket_splicer();
    auto *dst_channel = socket_splicer->client_channel();
    auto *dst_protocol = connection_->client_protocol();

    // inject the trace, if there are events and the user asked for WARNINGS.
    if (!something_failed_ && !connection_->events().empty() &&
        verbosity_ == ShowWarnings::Verbosity::Warning) {
      const auto trace_res = trace_as_json(connection_->events());
      if (trace_res) {
        using msg_type = classic_protocol::message::server::Row;
        const auto send_res = ClassicFrame::send_msg<msg_type>(
            dst_channel, dst_protocol,
            std::vector<msg_type::value_type>{
                {"Note"}, {std::to_string(ER_ROUTER_TRACE)}, {*trace_res}});
        if (!send_res) something_failed_ = true;
      }
    }

    const auto send_res =
        ClassicFrame::send_msg(dst_channel, dst_protocol, msg);
    if (!send_res) something_failed_ = true;
  }

  void on_ok(const classic_protocol::message::server::Ok &msg) override {
    auto *socket_splicer = connection_->socket_splicer();
    auto *dst_channel = socket_splicer->client_channel();
    auto *dst_protocol = connection_->client_protocol();

    const auto send_res =
        ClassicFrame::send_msg(dst_channel, dst_protocol, msg);
    if (!send_res) something_failed_ = true;
  }

  void on_error(const classic_protocol::message::server::Error &msg) override {
    auto *socket_splicer = connection_->socket_splicer();
    auto *dst_channel = socket_splicer->client_channel();
    auto *dst_protocol = connection_->client_protocol();

    const auto send_res =
        ClassicFrame::send_msg(dst_channel, dst_protocol, msg);
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
    auto *socket_splicer = connection_->socket_splicer();
    auto *dst_channel = socket_splicer->client_channel();
    auto *dst_protocol = connection_->client_protocol();

    // forward the message.
    auto send_res =
        ClassicFrame::send_msg<classic_protocol::message::server::ColumnCount>(
            dst_channel, dst_protocol, {count});
    if (!send_res) something_failed_ = true;

    col_count_ = count;

    if (col_count_ != 1) something_failed_ = true;
  }

  void on_column(
      const classic_protocol::message::server::ColumnMeta &col) override {
    auto *socket_splicer = connection_->socket_splicer();
    auto *dst_channel = socket_splicer->client_channel();
    auto *dst_protocol = connection_->client_protocol();

    auto send_res = ClassicFrame::send_msg(dst_channel, dst_protocol, col);
    if (!send_res) {
      something_failed_ = true;
    }
  }

  void on_row(const classic_protocol::message::server::Row &msg) override {
    auto *socket_splicer = connection_->socket_splicer();
    auto *dst_channel = socket_splicer->client_channel();
    auto *dst_protocol = connection_->client_protocol();

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
                  dst_channel, dst_protocol,
                  {{std::to_string(warning_count + 1)}});
          if (!send_res) something_failed_ = true;

          return;
        }
      }
    }

    auto send_res = ClassicFrame::send_msg(dst_channel, dst_protocol, msg);
    if (!send_res) something_failed_ = true;
  }

  // end of rows.
  void on_row_end(const classic_protocol::message::server::Eof &msg) override {
    auto *socket_splicer = connection_->socket_splicer();
    auto *dst_channel = socket_splicer->client_channel();
    auto *dst_protocol = connection_->client_protocol();

    const auto send_res =
        ClassicFrame::send_msg(dst_channel, dst_protocol, msg);
    if (!send_res) something_failed_ = true;
  }

  void on_ok(const classic_protocol::message::server::Ok &msg) override {
    auto *socket_splicer = connection_->socket_splicer();
    auto *dst_channel = socket_splicer->client_channel();
    auto *dst_protocol = connection_->client_protocol();

    const auto send_res =
        ClassicFrame::send_msg(dst_channel, dst_protocol, msg);
    if (!send_res) something_failed_ = true;
  }

  void on_error(const classic_protocol::message::server::Error &msg) override {
    auto *socket_splicer = connection_->socket_splicer();
    auto *dst_channel = socket_splicer->client_channel();
    auto *dst_protocol = connection_->client_protocol();

    const auto send_res =
        ClassicFrame::send_msg(dst_channel, dst_protocol, msg);
    if (!send_res) something_failed_ = true;
  }

 private:
  uint64_t col_count_{};
  uint64_t col_cur_{};
  MysqlRoutingClassicConnectionBase *connection_;

  bool something_failed_{false};

  ShowWarnings::Verbosity verbosity_;
};

stdx::expected<Processor::Result, std::error_code> QueryForwarder::command() {
  auto *socket_splicer = connection()->socket_splicer();
  auto *src_channel = socket_splicer->client_channel();
  auto *src_protocol = connection()->client_protocol();

  if (!connection()->connection_sharing_possible()) {
    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("query::command"));
    }
  } else {
    auto msg_res =
        ClassicFrame::recv_msg<classic_protocol::message::client::Query>(
            src_channel, src_protocol);
    if (!msg_res) {
      // all codec-errors should result in a Malformed Packet error..
      if (msg_res.error().category() !=
          make_error_code(classic_protocol::codec_errc::not_enough_input)
              .category()) {
        return recv_client_failed(msg_res.error());
      }

      discard_current_msg(src_channel, src_protocol);

      auto send_msg =
          ClassicFrame::send_msg<classic_protocol::message::server::Error>(
              src_channel, src_protocol,
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
          "query::command: " + msg_res->statement().substr(0, 1024) +
          oss.str()));
    }

    if (src_protocol->shared_capabilities().test(
            classic_protocol::capabilities::pos::multi_statements) &&
        contains_multiple_statements(msg_res->statement())) {
      auto send_res = ClassicFrame::send_msg<
          classic_protocol::message::server::Error>(
          src_channel, src_protocol,
          {ER_ROUTER_NOT_ALLOWED_WITH_CONNECTION_SHARING,
           "Multi-Statements are forbidden if connection-sharing is enabled.",
           "HY000"});
      if (!send_res) return send_client_failed(send_res.error());

      discard_current_msg(src_channel, src_protocol);

      stage(Stage::Done);
      return Result::SendToClient;
    }

    // the diagnostics-area is only maintained, if connection-sharing is
    // allowed.
    //
    // Otherwise, all queries for the diagnostics area MUST go to the
    // server.
    const auto intercept_res =
        intercept_diagnostics_area_queries(msg_res->statement());
    if (intercept_res) {
      if (std::holds_alternative<std::monostate>(*intercept_res)) {
        // no match
      } else if (std::holds_alternative<ShowWarnings>(*intercept_res)) {
        auto cmd = std::get<ShowWarnings>(*intercept_res);

        discard_current_msg(src_channel, src_protocol);

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
              connection(), msg_res->statement(),
              std::make_unique<ForwardedShowWarningsHandler>(connection(),
                                                             cmd.verbosity())));

          return Result::Again;
        }
      } else if (std::holds_alternative<ShowWarningCount>(*intercept_res)) {
        auto cmd = std::get<ShowWarningCount>(*intercept_res);

        discard_current_msg(src_channel, src_protocol);

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
              connection(), msg_res->statement(),
              std::make_unique<ForwardedShowWarningCountHandler>(
                  connection(), cmd.verbosity())));

          return Result::Again;
        }
      } else if (std::holds_alternative<CommandRouterSet>(*intercept_res)) {
        discard_current_msg(src_channel, src_protocol);

        connection()->execution_context().diagnostics_area().warnings().clear();
        connection()->events().clear();

        auto cmd = std::get<CommandRouterSet>(*intercept_res);

        auto set_res = execute_command_router_set(connection(), cmd);
        if (!set_res) return send_client_failed(set_res.error());

        stage(Stage::Done);
        return Result::SendToClient;
      }
    } else {
      discard_current_msg(src_channel, src_protocol);

      auto send_res =
          ClassicFrame::send_msg<classic_protocol::message::server::Error>(
              src_channel, src_protocol,
              {1064, intercept_res.error(), "42000"});
      if (!send_res) return send_client_failed(send_res.error());

      stage(Stage::Done);
      return Result::SendToClient;
    }

    // not a SHOW WARNINGS or so, reset the warnings.
    connection()->execution_context().diagnostics_area().warnings().clear();
    connection()->events().clear();

    auto collation_connection = connection()
                                    ->execution_context()
                                    .system_variables()
                                    .get("collation_connection")
                                    .value()
                                    .value_or("utf8mb4");

    const CHARSET_INFO *cs_collation_connection =
        get_charset_by_name(collation_connection.c_str(), 0);

    for (const auto &param : msg_res->values()) {
      if (0 == my_strcasecmp(cs_collation_connection, param.name.c_str(),
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
                discard_current_msg(src_channel, src_protocol);

                auto send_res = ClassicFrame::send_msg<
                    classic_protocol::message::server::Error>(
                    src_channel, src_protocol,
                    {1064, "Query attribute 'router.trace' requires 0 or 1",
                     "42000"});
                if (!send_res) return send_client_failed(send_res.error());

                stage(Stage::Done);
                return Result::SendToClient;
            }
          } else {
            discard_current_msg(src_channel, src_protocol);

            auto send_res = ClassicFrame::send_msg<
                classic_protocol::message::server::Error>(
                src_channel, src_protocol,
                {1064, "Query attribute 'router.trace' requires a number",
                 "42000"});
            if (!send_res) return send_client_failed(send_res.error());

            stage(Stage::Done);
            return Result::SendToClient;
          }
        } else {
          discard_current_msg(src_channel, src_protocol);

          auto send_res =
              ClassicFrame::send_msg<classic_protocol::message::server::Error>(
                  src_channel, src_protocol,
                  {1064, "router.trace requires a value", "42000"});
          if (!send_res) return send_client_failed(send_res.error());

          stage(Stage::Done);
          return Result::SendToClient;
        }
      } else {
        std::string param_prefix = param.name.substr(0, 7);
        if (0 == my_strcasecmp(cs_collation_connection, param_prefix.c_str(),
                               "router.")) {
          // unknown router. query-attribute.
          discard_current_msg(src_channel, src_protocol);

          auto send_res =
              ClassicFrame::send_msg<classic_protocol::message::server::Error>(
                  src_channel, src_protocol,
                  {1064, "Query attribute " + param.name + " is unknown",
                   "42000"});
          if (!send_res) return send_client_failed(send_res.error());

          stage(Stage::Done);
          return Result::SendToClient;
        }
      }
    }

    trace_event_command_ = trace_command(prefix());

    stmt_classified_ = classify(msg_res->statement(), true);

    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("query::classified: " +
                                     mysqlrouter::to_string(stmt_classified_)));
    }

    if (auto *ev = trace_span(trace_event_command_, "mysql/query_classify")) {
      ev->attrs.emplace_back("mysql.query.classification",
                             mysqlrouter::to_string(stmt_classified_));
    }

    // SET session_track... is forbidden if router sets session-trackers on the
    // server-side.
    if ((stmt_classified_ & StmtClassifier::ForbiddenSetWithConnSharing) &&
        connection()->connection_sharing_possible()) {
      discard_current_msg(src_channel, src_protocol);

      if (auto &tr = tracer()) {
        tr.trace(Tracer::Event().stage("query::forbidden"));
      }

      auto send_res = ClassicFrame::send_msg<
          classic_protocol::borrowed::message::server::Error>(
          src_channel, src_protocol,
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
      discard_current_msg(src_channel, src_protocol);

      if (auto &tr = tracer()) {
        tr.trace(Tracer::Event().stage("query::forbidden"));
      }
      auto send_res = ClassicFrame::send_msg<
          classic_protocol::borrowed::message::server::Error>(
          src_channel, src_protocol,
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

  auto &server_conn = connection()->socket_splicer()->server_conn();
  if (!server_conn.is_open()) {
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
    tr.trace(Tracer::Event().stage("query::connect"));
  }

  stage(Stage::Connected);
  return mysql_reconnect_start(trace_event_connect_and_forward_command_);
}

stdx::expected<Processor::Result, std::error_code> QueryForwarder::connected() {
  auto &server_conn = connection()->socket_splicer()->server_conn();
  if (!server_conn.is_open()) {
    auto *socket_splicer = connection()->socket_splicer();
    auto *src_channel = socket_splicer->client_channel();
    auto *src_protocol = connection()->client_protocol();

    // take the client::command from the connection.
    auto msg_res =
        ClassicFrame::recv_msg<classic_protocol::borrowed::wire::String>(
            src_channel, src_protocol);
    if (!msg_res) return recv_client_failed(msg_res.error());

    discard_current_msg(src_channel, src_protocol);

    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("query::connect::error"));
    }

    trace_span_end(trace_event_connect_and_forward_command_);
    trace_command_end(trace_event_command_);

    stage(Stage::Done);
    return reconnect_send_error_msg(src_channel, src_protocol);
  }

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("query::connected"));
  }

  trace_event_forward_command_ =
      trace_forward_command(trace_event_connect_and_forward_command_);

  stage(Stage::Forward);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code> QueryForwarder::forward() {
  auto *socket_splicer = connection()->socket_splicer();
  auto *src_protocol = connection()->client_protocol();
  auto *dst_protocol = connection()->server_protocol();

  auto client_caps = src_protocol->shared_capabilities();
  auto server_caps = dst_protocol->shared_capabilities();

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

  auto *src_channel = socket_splicer->client_channel();
  auto *dst_channel = socket_splicer->server_channel();

  auto msg_res =
      ClassicFrame::recv_msg<classic_protocol::message::client::Query>(
          src_channel, src_protocol);
  if (!msg_res) {
    // all codec-errors should result in Bad Message.
    if (msg_res.error().category() !=
        make_error_code(classic_protocol::codec_errc::not_enough_input)
            .category()) {
      return recv_client_failed(msg_res.error());
    }

    discard_current_msg(src_channel, src_protocol);

    auto send_msg =
        ClassicFrame::send_msg<classic_protocol::message::server::Error>(
            src_channel, src_protocol,
            {ER_MALFORMED_PACKET, "Malformed communication packet", "HY000"});
    if (!send_msg) send_client_failed(send_msg.error());

    trace_span_end(trace_event_connect_and_forward_command_);

    stage(Stage::Done);

    return Result::SendToClient;
  }

  // if the message contains attributes, error.
  if (!msg_res->values().empty()) {
    discard_current_msg(src_channel, src_protocol);

    auto send_msg = ClassicFrame::send_msg<
        classic_protocol::message::server::Error>(
        src_channel, src_protocol,
        {ER_MALFORMED_PACKET,
         "Message contains attributes, but server does not support attributes.",
         "HY000"});
    if (!send_msg) send_client_failed(send_msg.error());

    stage(Stage::Done);

    return Result::SendToClient;
  }

  auto send_res = ClassicFrame::send_msg(dst_channel, dst_protocol, *msg_res);
  if (!send_res) return send_server_failed(send_res.error());

  discard_current_msg(src_channel, src_protocol);

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
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = connection()->server_protocol();

  auto read_res =
      ClassicFrame::ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) return recv_server_failed(read_res.error());

  uint8_t msg_type = src_protocol->current_msg_type().value();

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
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->client_channel();
  auto src_protocol = connection()->client_protocol();

  auto read_res = ClassicFrame::ensure_frame_header(src_channel, src_protocol);
  if (!read_res) return recv_client_failed(read_res.error());

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("query::data"));
  }

  // local-data is finished with an empty packet.
  if (src_protocol->current_frame()->frame_size_ == 4) {
    stage(Stage::Response);
  }

  return forward_client_to_server();
}

stdx::expected<Processor::Result, std::error_code>
QueryForwarder::column_count() {
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = connection()->server_protocol();

  auto msg_res = ClassicFrame::recv_msg<
      classic_protocol::borrowed::message::server::ColumnCount>(src_channel,
                                                                src_protocol);
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
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = connection()->server_protocol();
  auto dst_channel = socket_splicer->client_channel();
  auto dst_protocol = connection()->client_protocol();

  auto skips_eof_pos =
      classic_protocol::capabilities::pos::text_result_with_session_tracking;

  bool server_skips_end_of_columns{
      src_protocol->shared_capabilities().test(skips_eof_pos)};

  bool router_skips_end_of_columns{
      dst_protocol->shared_capabilities().test(skips_eof_pos)};

  if (server_skips_end_of_columns && router_skips_end_of_columns) {
    // this is a Row, not a EOF packet.
    stage(Stage::RowOrEnd);
    return Result::Again;
  } else if (!server_skips_end_of_columns && !router_skips_end_of_columns) {
    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("query::column_end::eof"));
    }
    stage(Stage::RowOrEnd);
    return forward_server_to_client(true);
  } else if (!server_skips_end_of_columns && router_skips_end_of_columns) {
    // client is new, server is old: drop the server's EOF.
    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("query::column_end::skip_eof"));
    }

    auto msg_res = ClassicFrame::recv_msg<
        classic_protocol::borrowed::message::server::Eof>(src_channel,
                                                          src_protocol);
    if (!msg_res) return recv_server_failed(msg_res.error());

    discard_current_msg(src_channel, src_protocol);

    stage(Stage::RowOrEnd);
    return Result::Again;
  } else {
    // client is old, server is new: inject an EOF between column-meta and
    // rows.
    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("query::column_end::add_eof"));
    }

    auto msg_res = ClassicFrame::send_msg<
        classic_protocol::borrowed::message::server::Eof>(dst_channel,
                                                          dst_protocol, {});
    if (!msg_res) return recv_server_failed(msg_res.error());

    stage(Stage::RowOrEnd);
    return Result::SendToServer;
  }
}

stdx::expected<Processor::Result, std::error_code>
QueryForwarder::row_or_end() {
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = connection()->server_protocol();

  auto read_res =
      ClassicFrame::ensure_has_msg_prefix(src_channel, src_protocol);
  if (!read_res) return recv_server_failed(read_res.error());

  uint8_t msg_type = src_protocol->current_msg_type().value();

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
      if (src_protocol->current_frame()->frame_size_ < 1024) {
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
  auto *socket_splicer = connection()->socket_splicer();
  auto *src_channel = socket_splicer->server_channel();
  auto *src_protocol = connection()->server_protocol();
  auto *dst_channel = socket_splicer->client_channel();
  auto *dst_protocol = connection()->client_protocol();

  auto msg_res =
      ClassicFrame::recv_msg<classic_protocol::borrowed::message::server::Eof>(
          src_channel, src_protocol);
  if (!msg_res) return recv_server_failed(msg_res.error());

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("query::row_end"));
  }

  auto msg = *msg_res;

  if (!msg.session_changes().empty()) {
    auto track_res = connection()->track_session_changes(
        net::buffer(msg.session_changes()),
        src_protocol->shared_capabilities());
  }

  dst_protocol->status_flags(msg.status_flags());

  if (msg.status_flags().test(
          classic_protocol::status::pos::more_results_exist)) {
    stage(Stage::Response);  // another resultset is coming

    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("query::more_resultsets"));
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

    auto send_res = ClassicFrame::send_msg<
        classic_protocol::borrowed::message::server::Eof>(dst_channel,
                                                          dst_protocol, msg);
    if (!send_res) return stdx::make_unexpected(send_res.error());

    // msg refers to src-channel's recv-buf. discard after send.
    discard_current_msg(src_channel, src_protocol);

    return Result::SendToClient;
  }

  return forward_server_to_client();
}

stdx::expected<Processor::Result, std::error_code> QueryForwarder::ok() {
  auto *socket_splicer = connection()->socket_splicer();
  auto *src_channel = socket_splicer->server_channel();
  auto *src_protocol = connection()->server_protocol();
  auto *dst_channel = socket_splicer->client_channel();
  auto *dst_protocol = connection()->client_protocol();

  auto msg_res =
      ClassicFrame::recv_msg<classic_protocol::borrowed::message::server::Ok>(
          src_channel, src_protocol);
  if (!msg_res) return recv_server_failed(msg_res.error());

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("query::ok"));
  }

  auto msg = *msg_res;

  if (!msg.session_changes().empty()) {
    auto track_res = connection()->track_session_changes(
        net::buffer(msg.session_changes()), src_protocol->shared_capabilities(),
        stmt_classified_ & StmtClassifier::NoStateChangeIgnoreTracker);
  }

  dst_protocol->status_flags(msg.status_flags());

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

    auto send_res = ClassicFrame::send_msg(dst_channel, dst_protocol, msg);
    if (!send_res) return stdx::make_unexpected(send_res.error());

    discard_current_msg(src_channel, src_protocol);

    return Result::SendToClient;
  }

  // forward the message AS IS.
  return forward_server_to_client();
}

stdx::expected<Processor::Result, std::error_code> QueryForwarder::error() {
  auto *socket_splicer = connection()->socket_splicer();
  auto *src_channel = socket_splicer->server_channel();
  auto *src_protocol = connection()->server_protocol();

  auto msg_res = ClassicFrame::recv_msg<
      classic_protocol::borrowed::message::server::Error>(src_channel,
                                                          src_protocol);
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
