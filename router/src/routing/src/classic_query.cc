/*
  Copyright (c) 2022, Oracle and/or its affiliates.

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

#include "classic_query.h"

#include <charconv>
#include <limits>
#include <memory>
#include <system_error>
#include <variant>

#include "classic_connection.h"
#include "classic_forwarder.h"
#include "classic_frame.h"
#include "classic_lazy_connect.h"
#include "harness_assert.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/tls_error.h"
#include "mysqld_error.h"  // mysql errors
#include "mysqlrouter/classic_protocol_message.h"
#include "mysqlrouter/utils.h"  // to_string
#include "processor.h"
#include "sql/lex.h"
#include "sql_exec_context.h"
#include "sql_lexer.h"
#include "sql_lexer_thd.h"

#undef DEBUG_DUMP_TOKENS

stdx::expected<Processor::Result, std::error_code> QueryForwarder::process() {
  switch (stage()) {
    case Stage::Command:
      return command();
    case Stage::Connect:
      return connect();
    case Stage::Connected:
      return connected();
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
    os << "ignore_tracker";
  }
  if (flags & StmtClassifier::StateChangeOnError) {
    if (one) os << ",";
    one = true;
    os << "change-on-error";
  }
  if (flags & StmtClassifier::StateChangeOnSuccess) {
    if (one) os << ",";
    one = true;
    os << "change-on-success";
  }
  if (flags & StmtClassifier::StateChangeOnTracker) {
    if (one) os << ",";
    one = true;
    os << "change-on-tracker";
  }

  return os;
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

      if (first.id == SET_SYM) {
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

static uint64_t get_error_count(MysqlRoutingClassicConnection *connection) {
  uint64_t count{};
  for (auto const &w :
       connection->execution_context().diagnostics_area().warnings()) {
    if (w.level() == "Error") ++count;
  }

  return count;
}

static uint64_t get_warning_count(MysqlRoutingClassicConnection *connection) {
  return connection->execution_context().diagnostics_area().warnings().size();
}

static stdx::expected<void, std::error_code> send_resultset(
    Channel *src_channel, ClassicProtocolState *src_protocol,
    std::vector<classic_protocol::message::server::ColumnMeta> columns,
    std::vector<classic_protocol::message::server::Row> rows) {
  {
    auto send_res =
        ClassicFrame::send_msg<classic_protocol::message::server::ColumnCount>(
            src_channel, src_protocol, {columns.size()});
    if (!send_res) return stdx::make_unexpected(send_res.error());
  }

  for (auto const &col : columns) {
    auto send_res =
        ClassicFrame::send_msg<classic_protocol::message::server::ColumnMeta>(
            src_channel, src_protocol, col);
    if (!send_res) return stdx::make_unexpected(send_res.error());
  }

  for (auto const &row : rows) {
    auto send_res =
        ClassicFrame::send_msg<classic_protocol::message::server::Row>(
            src_channel, src_protocol, row);
    if (!send_res) return stdx::make_unexpected(send_res.error());
  }

  {
    auto send_res =
        ClassicFrame::send_msg<classic_protocol::message::server::Eof>(
            src_channel, src_protocol, {});
    if (!send_res) return stdx::make_unexpected(send_res.error());
  }

  return {};
}

std::vector<classic_protocol::message::server::Row> rows_from_warnings(
    MysqlRoutingClassicConnection *connection, bool only_errors,
    uint64_t row_count, uint64_t offset) {
  std::vector<classic_protocol::message::server::Row> rows;

  uint64_t r{};

  for (auto const &w :
       connection->execution_context().diagnostics_area().warnings()) {
    if (!only_errors || w.level() == "Error") {
      if (r++ < offset) continue;

      if (row_count == rows.size()) break;

      rows.emplace_back(std::vector<std::optional<std::string>>{
          w.level(), std::to_string(w.code()), w.message()});
    }
  }

  return rows;
}

class ShowWarnings {
 public:
  ShowWarnings(bool only_errors,
               uint64_t row_count = std::numeric_limits<uint64_t>().max(),
               uint64_t offset = 0)
      : only_errors_(only_errors), row_count_{row_count}, offset_{offset} {}

  bool only_errors() const { return only_errors_; }
  uint64_t row_count() const { return row_count_; }
  uint64_t offset() const { return offset_; }

 private:
  bool only_errors_;

  uint64_t row_count_;
  uint64_t offset_;
};

class ShowWarningCount {
 public:
  enum class Scope { Local, Session, None };

  ShowWarningCount(bool only_errors, Scope scope)
      : only_errors_(only_errors), scope_{scope} {}

  bool only_errors() const { return only_errors_; }
  Scope scope() const { return scope_; }

 private:
  bool only_errors_;
  Scope scope_;
};

static stdx::expected<void, std::error_code> show_count(
    MysqlRoutingClassicConnection *connection, const char *name,
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

static const char *show_warning_count_name(bool only_errors,
                                           ShowWarningCount::Scope scope) {
  if (only_errors) {
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
    MysqlRoutingClassicConnection *connection, bool only_errors,
    ShowWarningCount::Scope scope) {
  if (only_errors) {
    return show_count(connection, show_warning_count_name(only_errors, scope),
                      get_error_count(connection));
  } else {
    return show_count(connection, show_warning_count_name(only_errors, scope),
                      get_warning_count(connection));
  }
}

static stdx::expected<void, std::error_code> show_warnings(
    MysqlRoutingClassicConnection *connection, bool only_errors,
    uint64_t row_count, uint64_t offset) {
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
      rows_from_warnings(connection, only_errors, row_count, offset));
  if (!send_res) return stdx::make_unexpected(send_res.error());

  return {};
}

struct Limit {
  uint64_t row_count{std::numeric_limits<uint64_t>::max()};
  uint64_t offset{};
};

class Parser {
 public:
  Parser(SqlLexer::iterator first, SqlLexer::iterator last)
      : cur_{first}, end_{last} {}

  stdx::expected<std::variant<std::monostate, ShowWarningCount, ShowWarnings>,
                 std::string>
  parse() {
    if (accept(SHOW)) {
      if (accept(WARNINGS)) {
        stdx::expected<Limit, std::string> limit_res;

        if (accept(LIMIT)) {  // optional limit
          limit_res = limit();
        }

        if (expect(END_OF_INPUT)) {
          return {std::in_place,
                  ShowWarnings{false, limit_res->row_count, limit_res->offset}};
        }
      } else if (accept(ERRORS)) {
        stdx::expected<Limit, std::string> limit_res;

        if (accept(LIMIT)) {
          limit_res = limit();
        }

        if (expect(END_OF_INPUT)) {
          return {std::in_place,
                  ShowWarnings{true, limit_res->row_count, limit_res->offset}};
        }
      } else if (accept(COUNT_SYM)) {
        expect('(');
        expect('*');
        expect(')');

        if (accept(WARNINGS)) {
          if (expect(END_OF_INPUT)) {
            return {std::in_place,
                    ShowWarningCount{false, ShowWarningCount::Scope::Session}};
          }
        } else if (accept(ERRORS)) {
          if (expect(END_OF_INPUT)) {
            return {std::in_place,
                    ShowWarningCount{true, ShowWarningCount::Scope::Session}};
          }
        } else {
          error_ = "expected WARNINGS|ERRORS.";
        }
      } else {
        error_ = "expected WARNINGS|ERRORS|COUNT";
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
              if (ident_res && expect(END_OF_INPUT)) {
                return {std::in_place,
                        ShowWarningCount(*ident_res,
                                         ShowWarningCount::Scope::Session)};
              }
            }
          } else if (accept(LOCAL_SYM)) {
            if (accept('.')) {
              auto ident_res = warning_count_ident();
              if (ident_res && expect(END_OF_INPUT)) {
                return {std::in_place,
                        ShowWarningCount(*ident_res,
                                         ShowWarningCount::Scope::Local)};
              }
            }
          } else {
            auto ident_res = warning_count_ident();
            if (ident_res && expect(END_OF_INPUT)) {
              return {
                  std::in_place,
                  ShowWarningCount(*ident_res, ShowWarningCount::Scope::None)};
            }
          }
        }
      }
    }

    return stdx::make_unexpected(error_);
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
  static uint64_t sv_to_num(std::string_view s) {
    uint64_t v{};

    auto conv_res = std::from_chars(s.data(), s.data() + s.size(), v);
    if (conv_res.ec == std::errc{}) {
      return v;
    } else {
      // NUM is a number without, it should always convert.
      harness_assert_this_should_not_execute();
    }
  }

  // accept: NUM [, NUM]
  stdx::expected<Limit, std::string> limit() {
    if (auto num1_tkn = expect(NUM)) {
      auto num1 = sv_to_num(num1_tkn.text());  // offset_or_row_count
      if (accept(',')) {
        if (auto num2_tkn = expect(NUM)) {
          auto num2 = sv_to_num(num2_tkn.text());  // row_count

          return Limit{num2, num1};
        }
      } else {
        return Limit{num1, 0};
      }
    }

    return stdx::make_unexpected(error_);
  }

  stdx::expected<bool, std::string> warning_count_ident() {
    if (auto sess_var_tkn = ident()) {
      if (sess_var_tkn.text() == "warning_count") {
        return false;
      } else if (sess_var_tkn.text() == "error_count") {
        return true;
      }
    }

    return stdx::make_unexpected(error_);
  }

  class TokenText {
   public:
    TokenText() = default;
    TokenText(std::string_view txt) : txt_{txt} {}

    operator bool() const { return !txt_.empty(); }

    [[nodiscard]] std::string_view text() const { return txt_; }

   private:
    std::string_view txt_{};
  };

  TokenText ident() {
    if (auto ident_tkn = accept(IDENT)) {
      return ident_tkn;
    } else if (auto ident_tkn = accept(IDENT_QUOTED)) {
      return ident_tkn;
    } else {
      return {};
    }
  }

  TokenText accept(int sym) {
    if (has_error()) return {};

    if (cur_->id == sym) {
      auto txt = cur_->text;
      ++cur_;
      return txt;
    }

    return {};
  }

  TokenText expect(int sym) {
    if (has_error()) return {};

    if (auto txt = accept(sym)) {
      return txt;
    }

    error_ = "expected sym, got ...";

    return {};
  }

  bool has_error() const { return !error_.empty(); }

  SqlLexer::iterator cur_;
  SqlLexer::iterator end_;

  std::string error_{};
};

static stdx::expected<
    std::variant<std::monostate, ShowWarningCount, ShowWarnings>,
    std::error_code>
intercept_diagnostics_area_queries(const std::string &stmt) {
  MEM_ROOT mem_root;
  THD session;
  session.mem_root = &mem_root;

  {
    Parser_state parser_state;
    parser_state.init(&session, stmt.data(), stmt.size());
    session.m_parser_state = &parser_state;
    SqlLexer lexer{&session};

    auto parse_res = Parser(lexer.begin(), lexer.end()).parse();
    if (!parse_res) {
      return {std::in_place, std::monostate{}};
    } else {
      return parse_res.value();
    }
  }
}

stdx::expected<Processor::Result, std::error_code> QueryForwarder::command() {
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->client_channel();
  auto src_protocol = connection()->client_protocol();

  if (connection()->connection_sharing_possible()) {
    auto msg_res =
        ClassicFrame::recv_msg<classic_protocol::message::client::Query>(
            src_channel, src_protocol);
    if (!msg_res) return recv_client_failed(msg_res.error());

    trace(Tracer::Event().stage("query::command: " +
                                msg_res->statement().substr(0, 1024)));

    if (connection()->connection_sharing_allowed()) {
      // the diagnostics-area is only maintained, if connection-sharing is
      // allowed.
      //
      // Otherwise, all queries for the to the diagnostics area MUST go to the
      // server.
      auto intercept_res =
          intercept_diagnostics_area_queries(msg_res->statement());
      if (intercept_res) {
        if (std::holds_alternative<std::monostate>(*intercept_res)) {
          // no match
        } else if (std::holds_alternative<ShowWarnings>(*intercept_res)) {
          discard_current_msg(src_channel, src_protocol);

          auto cmd = std::get<ShowWarnings>(*intercept_res);

          auto send_res = show_warnings(connection(), cmd.only_errors(),
                                        cmd.row_count(), cmd.offset());
          if (!send_res) return send_client_failed(send_res.error());

          stage(Stage::Done);
          return Result::SendToClient;
        } else if (std::holds_alternative<ShowWarningCount>(*intercept_res)) {
          discard_current_msg(src_channel, src_protocol);

          auto cmd = std::get<ShowWarningCount>(*intercept_res);

          auto send_res =
              show_warning_count(connection(), cmd.only_errors(), cmd.scope());
          if (!send_res) return send_client_failed(send_res.error());

          stage(Stage::Done);
          return Result::SendToClient;
        }
      }
    }

    stmt_classified_ = classify(msg_res->statement(), true);

    trace(Tracer::Event().stage("query::classified: " +
                                mysqlrouter::to_string(stmt_classified_)));

    // SET session_track... is forbidden if router sets session-trackers on the
    // server-side.
    if ((stmt_classified_ & StmtClassifier::ForbiddenSetWithConnSharing) &&
        connection()->connection_sharing_possible()) {
      discard_current_msg(src_channel, src_protocol);

      trace(Tracer::Event().stage("query::forbidden"));

      auto send_res =
          ClassicFrame::send_msg<classic_protocol::message::server::Error>(
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

      trace(Tracer::Event().stage("query::forbidden"));

      auto send_res = ClassicFrame::send_msg<
          classic_protocol::message::server::Error>(
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

  auto &server_conn = connection()->socket_splicer()->server_conn();
  if (!server_conn.is_open()) {
    stage(Stage::Connect);
    return Result::Again;
  } else {
    stage(Stage::Response);
    return forward_client_to_server();
  }
}

stdx::expected<Processor::Result, std::error_code> QueryForwarder::connect() {
  trace(Tracer::Event().stage("query::connect"));

  stage(Stage::Connected);

  connection()->push_processor(std::make_unique<LazyConnector>(
      connection(), false /* not in handshake */));

  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code> QueryForwarder::connected() {
  auto &server_conn = connection()->socket_splicer()->server_conn();
  if (!server_conn.is_open()) {
    // Connector sent an server::Error already.
    auto *socket_splicer = connection()->socket_splicer();
    auto src_channel = socket_splicer->client_channel();
    auto src_protocol = connection()->client_protocol();

    // take the client::command from the connection.
    auto msg_res = ClassicFrame::recv_msg<classic_protocol::wire::String>(
        src_channel, src_protocol);
    if (!msg_res) return recv_client_failed(msg_res.error());

    discard_current_msg(src_channel, src_protocol);

    trace(Tracer::Event().stage("query::error"));

    stage(Stage::Done);
    return Result::Again;
  }

  trace(Tracer::Event().stage("query::connected"));
  stage(Stage::Response);
  return forward_client_to_server();
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
  trace(Tracer::Event().stage("query::load_data"));

  stage(Stage::Data);
  return forward_server_to_client();
}

stdx::expected<Processor::Result, std::error_code> QueryForwarder::data() {
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->client_channel();
  auto src_protocol = connection()->client_protocol();

  auto read_res = ClassicFrame::ensure_frame_header(src_channel, src_protocol);
  if (!read_res) return recv_client_failed(read_res.error());

  trace(Tracer::Event().stage("query::data"));

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

  auto msg_res = ClassicFrame::recv_msg<classic_protocol::wire::VarInt>(
      src_channel, src_protocol);
  if (!msg_res) return recv_server_failed(msg_res.error());

  trace(Tracer::Event().stage("query::column_count"));

  columns_left_ = msg_res->value();

  stage(Stage::Column);

  return forward_server_to_client(true);
}

stdx::expected<Processor::Result, std::error_code> QueryForwarder::column() {
  const auto trace_event = Tracer::Event().stage("query::column");

  trace(trace_event);

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
    trace(Tracer::Event().stage("query::column_end::eof"));
    stage(Stage::RowOrEnd);
    return forward_server_to_client(true);
  } else if (!server_skips_end_of_columns && router_skips_end_of_columns) {
    // client is new, server is old: drop the server's EOF.
    trace(Tracer::Event().stage("query::column_end::skip_eof"));

    auto msg_res =
        ClassicFrame::recv_msg<classic_protocol::message::server::Eof>(
            src_channel, src_protocol);
    if (!msg_res) return recv_server_failed(msg_res.error());

    discard_current_msg(src_channel, src_protocol);

    stage(Stage::RowOrEnd);
    return Result::Again;
  } else {
    // client is old, server is new: inject an EOF between column-meta and rows.
    trace(Tracer::Event().stage("query::column_end::add_eof"));

    auto msg_res =
        ClassicFrame::send_msg<classic_protocol::message::server::Eof>(
            dst_channel, dst_protocol, {});
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
  trace(Tracer::Event().stage("query::row"));

  stage(Stage::RowOrEnd);
  return forward_server_to_client(true /* noflush */);
}

stdx::expected<Processor::Result, std::error_code> QueryForwarder::row_end() {
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = connection()->server_protocol();

  auto msg_res = ClassicFrame::recv_msg<classic_protocol::message::server::Eof>(
      src_channel, src_protocol);
  if (!msg_res) return recv_server_failed(msg_res.error());

  trace(Tracer::Event().stage("query::row_end"));

  auto msg = std::move(*msg_res);

  if (!msg.session_changes().empty()) {
    auto track_res = connection()->track_session_changes(
        net::buffer(msg.session_changes()),
        src_protocol->shared_capabilities());
  }

  if (msg.status_flags().test(
          classic_protocol::status::pos::more_results_exist)) {
    stage(Stage::Response);  // another resultset is coming

    trace(Tracer::Event().stage("query::more_resultsets"));
    return forward_server_to_client(true);
  } else {
    if (stmt_classified_ & StmtClassifier::StateChangeOnSuccess) {
      connection()->some_state_changed(true);
    }

    if (msg.warning_count() > 0) {
      connection()->diagnostic_area_changed(true);
    }

    stage(Stage::Done);  // once the message is forwarded, we are done.
    return forward_server_to_client();
  }
}

stdx::expected<Processor::Result, std::error_code> QueryForwarder::ok() {
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = connection()->server_protocol();

  auto msg_res = ClassicFrame::recv_msg<classic_protocol::message::server::Ok>(
      src_channel, src_protocol);
  if (!msg_res) return recv_server_failed(msg_res.error());

  trace(Tracer::Event().stage("query::ok"));

  auto msg = std::move(*msg_res);

  if (!msg.session_changes().empty()) {
    auto track_res = connection()->track_session_changes(
        net::buffer(msg.session_changes()), src_protocol->shared_capabilities(),
        stmt_classified_ & StmtClassifier::NoStateChangeIgnoreTracker);
  }

  if (stmt_classified_ & StmtClassifier::StateChangeOnSuccess) {
    connection()->some_state_changed(true);
  }

  if (msg.status_flags().test(
          classic_protocol::status::pos::more_results_exist)) {
    stage(Stage::Response);  // another resultset is coming

    trace(Tracer::Event().stage("query::more_resultsets"));
    return forward_server_to_client(true);
  } else {
    if (msg.warning_count() > 0) {
      connection()->diagnostic_area_changed(true);
    } else {
      // there are no warnings on the server side.
      connection()->diagnostic_area_changed(false);
    }

    stage(Stage::Done);  // once the message is forwarded, we are done.
    return forward_server_to_client();
  }
}

stdx::expected<Processor::Result, std::error_code> QueryForwarder::error() {
  trace(Tracer::Event().stage("query::error"));

  if (stmt_classified_ & StmtClassifier::StateChangeOnError) {
    connection()->some_state_changed(true);
  }

  // at least one.
  connection()->diagnostic_area_changed(true);

  stage(Stage::Done);
  return forward_server_to_client();
}

// Sender

stdx::expected<Processor::Result, std::error_code> QuerySender::process() {
  switch (stage()) {
    case Stage::Command:
      return command();
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
    case Stage::Done:
      return Result::Done;
  }

  harness_assert_this_should_not_execute();
}

stdx::expected<Processor::Result, std::error_code> QuerySender::command() {
  auto *socket_splicer = connection()->socket_splicer();
  auto dst_channel = socket_splicer->server_channel();
  auto dst_protocol = connection()->server_protocol();

  trace(Tracer::Event().stage("query::command"));
  trace(Tracer::Event().stage(">> " + stmt_));

  dst_protocol->seq_id(0xff);

  auto send_res =
      ClassicFrame::send_msg<classic_protocol::message::client::Query>(
          dst_channel, dst_protocol, stmt_);
  if (!send_res) return send_server_failed(send_res.error());

  stage(Stage::Response);

  return Result::SendToServer;
}

stdx::expected<Processor::Result, std::error_code> QuerySender::response() {
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

stdx::expected<Processor::Result, std::error_code> QuerySender::load_data() {
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = connection()->server_protocol();

  auto msg_res = ClassicFrame::recv_msg<classic_protocol::wire::String>(
      src_channel, src_protocol);
  if (!msg_res) return recv_server_failed(msg_res.error());

  trace(Tracer::Event().stage("query::load_data"));

  // we could decode the filename here.

  discard_current_msg(src_channel, src_protocol);

  stage(Stage::Data);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code> QuerySender::data() {
  auto *socket_splicer = connection()->socket_splicer();
  auto dst_channel = socket_splicer->server_channel();
  auto dst_protocol = connection()->server_protocol();

  trace(Tracer::Event().stage("query::data"));

  // an empty packet.
  auto send_res = ClassicFrame::send_msg<classic_protocol::wire::String>(
      dst_channel, dst_protocol, {});
  if (!send_res) return send_server_failed(send_res.error());

  return Result::SendToServer;
}

stdx::expected<Processor::Result, std::error_code> QuerySender::column_count() {
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = connection()->server_protocol();

  auto msg_res =
      ClassicFrame::recv_msg<classic_protocol::message::server::ColumnCount>(
          src_channel, src_protocol);
  if (!msg_res) return recv_server_failed(msg_res.error());

  trace(Tracer::Event().stage("query::column_count"));

  if (handler_) handler_->on_column_count(msg_res->count());

  columns_left_ = msg_res->count();

  discard_current_msg(src_channel, src_protocol);

  stage(Stage::Column);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code> QuerySender::column() {
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = connection()->server_protocol();

  auto msg_res =
      ClassicFrame::recv_msg<classic_protocol::message::server::ColumnMeta>(
          src_channel, src_protocol);
  if (!msg_res) return recv_server_failed(msg_res.error());

  trace(Tracer::Event().stage("query::column"));

  discard_current_msg(src_channel, src_protocol);

  if (handler_) handler_->on_column(*msg_res);

  if (--columns_left_ == 0) {
    const auto skips_eof_pos =
        classic_protocol::capabilities::pos::text_result_with_session_tracking;

    const bool server_skips_end_of_columns{
        src_protocol->shared_capabilities().test(skips_eof_pos)};

    if (server_skips_end_of_columns) {
      // next is a Row, not a EOF packet.
      stage(Stage::RowOrEnd);
    } else {
      stage(Stage::ColumnEnd);
    }
  }

  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code> QuerySender::column_end() {
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = connection()->server_protocol();

  auto msg_res = ClassicFrame::recv_msg<classic_protocol::message::server::Eof>(
      src_channel, src_protocol);
  if (!msg_res) return recv_server_failed(msg_res.error());

  trace(Tracer::Event().stage("query::column_end"));

  discard_current_msg(src_channel, src_protocol);

  stage(Stage::RowOrEnd);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code> QuerySender::row_or_end() {
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
    case Msg::EndOfResult:
      stage(Stage::RowEnd);
      return Result::Again;
    case Msg::Error:
      stage(Stage::Error);
      return Result::Again;
  }

  stage(Stage::Row);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code> QuerySender::row() {
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = connection()->server_protocol();

  auto msg_res = ClassicFrame::recv_msg<classic_protocol::message::server::Row>(
      src_channel, src_protocol);
  if (!msg_res) return recv_server_failed(msg_res.error());

  trace(Tracer::Event().stage("query::row"));

  discard_current_msg(src_channel, src_protocol);

  if (handler_) handler_->on_row(*msg_res);

  stage(Stage::RowOrEnd);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code> QuerySender::row_end() {
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = connection()->server_protocol();

  trace(Tracer::Event().stage("query::row_end"));

  auto msg_res = ClassicFrame::recv_msg<classic_protocol::message::server::Eof>(
      src_channel, src_protocol);
  if (!msg_res) return recv_server_failed(msg_res.error());

  auto eof_msg = std::move(*msg_res);

  if (handler_) handler_->on_row_end(eof_msg);

  if (!eof_msg.session_changes().empty()) {
    auto track_res = connection()->track_session_changes(
        net::buffer(eof_msg.session_changes()),
        src_protocol->shared_capabilities());
  }

  discard_current_msg(src_channel, src_protocol);

  if (eof_msg.status_flags().test(
          classic_protocol::status::pos::more_results_exist)) {
    trace(Tracer::Event().stage("query::more_resultsets"));
    stage(Stage::Response);

    return Result::Again;
  } else {
    trace(Tracer::Event().stage("query::row_end"));
    stage(Stage::Done);
    return Result::Again;
  }
}

stdx::expected<Processor::Result, std::error_code> QuerySender::ok() {
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = connection()->server_protocol();

  auto msg_res = ClassicFrame::recv_msg<classic_protocol::message::server::Ok>(
      src_channel, src_protocol);
  if (!msg_res) return recv_server_failed(msg_res.error());

  discard_current_msg(src_channel, src_protocol);

  auto msg = std::move(*msg_res);

  if (handler_) handler_->on_ok(msg);

  if (!msg.session_changes().empty()) {
    auto changes_state = classify(stmt_, false);

    auto track_res = connection()->track_session_changes(
        net::buffer(msg.session_changes()), src_protocol->shared_capabilities(),
        changes_state & StmtClassifier::NoStateChangeIgnoreTracker);
  }

  if (msg.status_flags().test(
          classic_protocol::status::pos::more_results_exist)) {
    trace(Tracer::Event().stage("query::ok::more"));
    stage(Stage::Response);
  } else {
    trace(Tracer::Event().stage("query::ok::done"));
    stage(Stage::Done);
  }
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code> QuerySender::error() {
  auto *socket_splicer = connection()->socket_splicer();
  auto src_channel = socket_splicer->server_channel();
  auto src_protocol = connection()->server_protocol();

  auto msg_res =
      ClassicFrame::recv_msg<classic_protocol::message::server::Error>(
          src_channel, src_protocol);
  if (!msg_res) return recv_server_failed(msg_res.error());

  trace(Tracer::Event().stage("query::error"));

  discard_current_msg(src_channel, src_protocol);

  if (handler_) handler_->on_error(*msg_res);

  stage(Stage::Done);
  return Result::Again;
}
