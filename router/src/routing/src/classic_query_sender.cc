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

#include "classic_query_sender.h"

#include <charconv>
#include <limits>
#include <memory>
#include <system_error>
#include <variant>

#include "classic_connection_base.h"
#include "classic_frame.h"
#include "classic_lazy_connect.h"
#include "harness_assert.h"
#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/tls_error.h"
#include "mysqld_error.h"  // mysql errors
#include "mysqlrouter/classic_protocol_message.h"
#include "mysqlrouter/utils.h"  // to_string
#include "sql/lex.h"
#include "sql_exec_context.h"
#include "sql_lexer.h"
#include "sql_lexer_thd.h"
#include "sql_parser_state.h"

#undef DEBUG_DUMP_TOKENS

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
static stdx::flags<StmtClassifier> classify(SqlLexer &&lexer,
                                            bool forbid_set_trackers) {
  stdx::flags<StmtClassifier> classified{};

  {
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
  auto &dst_conn = connection()->server_conn();
  auto &dst_protocol = dst_conn.protocol();

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("query::command"));
    tr.trace(Tracer::Event().stage(">> " + stmt_));
  }

  dst_protocol.seq_id(0xff);

  auto send_res = ClassicFrame::send_msg(
      dst_conn, classic_protocol::borrowed::message::client::Query{stmt_});
  if (!send_res) return send_server_failed(send_res.error());

  stage(Stage::Response);

  return Result::SendToServer;
}

stdx::expected<Processor::Result, std::error_code> QuerySender::response() {
  auto &src_conn = connection()->server_conn();
  auto &src_protocol = src_conn.protocol();

  auto read_res = ClassicFrame::ensure_has_msg_prefix(src_conn);
  if (!read_res) return recv_server_failed(read_res.error());

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

stdx::expected<Processor::Result, std::error_code> QuerySender::load_data() {
  auto &src_conn = connection()->server_conn();

  auto msg_res =
      ClassicFrame::recv_msg<classic_protocol::borrowed::wire::String>(
          src_conn);
  if (!msg_res) return recv_server_failed(msg_res.error());

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("query::load_data"));
  }

  // we could decode the filename here.

  discard_current_msg(src_conn);

  stage(Stage::Data);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code> QuerySender::data() {
  auto &dst_conn = connection()->server_conn();

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("query::data"));
  }

  // an empty packet.
  auto send_res =
      ClassicFrame::send_msg<classic_protocol::borrowed::wire::String>(dst_conn,
                                                                       {});
  if (!send_res) return send_server_failed(send_res.error());

  return Result::SendToServer;
}

stdx::expected<Processor::Result, std::error_code> QuerySender::column_count() {
  auto &src_conn = connection()->server_conn();
  auto &src_channel = src_conn.channel();
  auto &src_protocol = src_conn.protocol();

  auto msg_res = ClassicFrame::recv_msg<
      classic_protocol::borrowed::message::server::ColumnCount>(src_channel,
                                                                src_protocol);
  if (!msg_res) return recv_server_failed(msg_res.error());

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("query::column_count"));
  }

  if (handler_) handler_->on_column_count(msg_res->count());

  columns_left_ = msg_res->count();

  discard_current_msg(src_conn);

  stage(Stage::Column);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code> QuerySender::column() {
  auto &src_conn = connection()->server_conn();
  auto &src_protocol = src_conn.protocol();

  auto msg_res =
      ClassicFrame::recv_msg<classic_protocol::message::server::ColumnMeta>(
          src_conn);
  if (!msg_res) return recv_server_failed(msg_res.error());

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("query::column"));
  }

  discard_current_msg(src_conn);

  if (handler_) handler_->on_column(*msg_res);

  if (--columns_left_ == 0) {
    const auto skips_eof_pos =
        classic_protocol::capabilities::pos::text_result_with_session_tracking;

    const bool server_skips_end_of_columns{
        src_protocol.shared_capabilities().test(skips_eof_pos)};

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
  auto &src_conn = connection()->server_conn();

  auto msg_res =
      ClassicFrame::recv_msg<classic_protocol::borrowed::message::server::Eof>(
          src_conn);
  if (!msg_res) return recv_server_failed(msg_res.error());

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("query::column_end"));
  }

  discard_current_msg(src_conn);

  stage(Stage::RowOrEnd);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code> QuerySender::row_or_end() {
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
  auto &src_conn = connection()->server_conn();

  auto msg_res =
      ClassicFrame::recv_msg<classic_protocol::message::server::Row>(src_conn);
  if (!msg_res) return recv_server_failed(msg_res.error());

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("query::row"));
  }

  discard_current_msg(src_conn);

  if (handler_) handler_->on_row(*msg_res);

  stage(Stage::RowOrEnd);
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code> QuerySender::row_end() {
  auto &src_conn = connection()->server_conn();
  auto &src_protocol = src_conn.protocol();

  auto msg_res =
      ClassicFrame::recv_msg<classic_protocol::message::server::Eof>(src_conn);
  if (!msg_res) return recv_server_failed(msg_res.error());

  auto eof_msg = std::move(*msg_res);

  if (handler_) handler_->on_row_end(eof_msg);

  if (!eof_msg.session_changes().empty()) {
    auto track_res = connection()->track_session_changes(
        net::buffer(eof_msg.session_changes()),
        src_protocol.shared_capabilities());
    if (!track_res) {
      // ignore
    }
  }

  discard_current_msg(src_conn);

  if (eof_msg.status_flags().test(
          classic_protocol::status::pos::more_results_exist)) {
    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("query::more_resultsets"));
    }
    stage(Stage::Response);

    return Result::Again;
  } else {
    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("query::row_end"));
    }
    stage(Stage::Done);
    return Result::Again;
  }
}

stdx::expected<Processor::Result, std::error_code> QuerySender::ok() {
  auto &src_conn = connection()->server_conn();
  auto &src_protocol = src_conn.protocol();

  auto msg_res =
      ClassicFrame::recv_msg<classic_protocol::message::server::Ok>(src_conn);
  if (!msg_res) return recv_server_failed(msg_res.error());

  discard_current_msg(src_conn);

  auto msg = std::move(*msg_res);

  if (handler_) handler_->on_ok(msg);

  if (!msg.session_changes().empty()) {
    SqlParserState sql_parser_state;

    sql_parser_state.statement(stmt_);

    auto changes_state = classify(sql_parser_state.lexer(), false);

    auto track_res = connection()->track_session_changes(
        net::buffer(msg.session_changes()), src_protocol.shared_capabilities(),
        changes_state & StmtClassifier::NoStateChangeIgnoreTracker);
    if (!track_res) {
      // ignore
    }
  }

  if (msg.status_flags().test(
          classic_protocol::status::pos::more_results_exist)) {
    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("query::ok::more"));
    }
    stage(Stage::Response);
  } else {
    if (auto &tr = tracer()) {
      tr.trace(Tracer::Event().stage("query::ok::done"));
    }
    stage(Stage::Done);
  }
  return Result::Again;
}

stdx::expected<Processor::Result, std::error_code> QuerySender::error() {
  auto &src_conn = connection()->server_conn();

  auto msg_res =
      ClassicFrame::recv_msg<classic_protocol::message::server::Error>(
          src_conn);
  if (!msg_res) return recv_server_failed(msg_res.error());

  if (auto &tr = tracer()) {
    tr.trace(Tracer::Event().stage("query::error"));
  }

  discard_current_msg(src_conn);

  if (handler_) handler_->on_error(*msg_res);

  stage(Stage::Done);
  return Result::Again;
}
