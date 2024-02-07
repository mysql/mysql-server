/*
 * Copyright (c) 2017, 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifndef UNITTEST_GUNIT_XPLUGIN_XCL_MOCK_PROTOCOL_H_
#define UNITTEST_GUNIT_XPLUGIN_XCL_MOCK_PROTOCOL_H_

#include <gmock/gmock.h>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "plugin/x/client/mysqlxclient/xprotocol.h"

namespace xcl {
namespace test {
namespace mock {

class XProtocol : public xcl::XProtocol {
 public:
  XProtocol();
  virtual ~XProtocol() override;

  MOCK_METHOD(Handler_id, add_notice_handler,
              (Notice_handler handler, const Handler_position,
               const Handler_priority),
              (override));
  MOCK_METHOD(Handler_id, add_received_message_handler,
              (Server_message_handler handler, const Handler_position,
               const Handler_priority),
              (override));
  MOCK_METHOD(Handler_id, add_send_message_handler,
              (Client_message_handler handler, const Handler_position,
               const Handler_priority),
              (override));
  MOCK_METHOD(void, remove_notice_handler, (Handler_id handler), (override));
  MOCK_METHOD(void, remove_received_message_handler, (Handler_id handler),
              (override));
  MOCK_METHOD(void, remove_send_message_handler, (Handler_id handler),
              (override));
  MOCK_METHOD(xcl::XConnection &, get_connection, (), (override));
  MOCK_METHOD(XError, recv,
              (Header_message_type_id * out_mid, uint8_t **buffer,
               std::size_t *buffer_size),
              (override));
  MOCK_METHOD(Message *, deserialize_received_message_raw,
              (const Header_message_type_id mid, const uint8_t *payload,
               const std::size_t payload_size, XError *out_error));
  MOCK_METHOD(Message *, recv_single_message_raw,
              (Server_message_type_id * out_mid, XError *out_error));

  MOCK_METHOD(XError, send_compressed_frame,
              (const Client_message_type_id mid, const Message &msg),
              (override));
  using Message_list =
      std::vector<std::pair<Client_message_type_id, const Message *>>;
  MOCK_METHOD(XError, send_compressed_multiple_frames,
              (const Message_list &messages), (override));
  MOCK_METHOD(XError, send,
              (const Client_message_type_id mid, const Message &msg),
              (override));
  MOCK_METHOD(XError, send,
              (const Header_message_type_id mid, const uint8_t *buffer,
               const std::size_t length),
              (override));
  MOCK_METHOD(XError, send, (const Mysqlx::Session::AuthenticateStart &m),
              (override));
  MOCK_METHOD(XError, send, (const Mysqlx::Session::AuthenticateContinue &m),
              (override));
  MOCK_METHOD(XError, send, (const Mysqlx::Session::Reset &m), (override));
  MOCK_METHOD(XError, send, (const Mysqlx::Session::Close &m), (override));
  MOCK_METHOD(XError, send, (const Mysqlx::Sql::StmtExecute &m), (override));
  MOCK_METHOD(XError, send, (const Mysqlx::Crud::Find &m), (override));
  MOCK_METHOD(XError, send, (const Mysqlx::Crud::Insert &m), (override));
  MOCK_METHOD(XError, send, (const Mysqlx::Crud::Update &m), (override));
  MOCK_METHOD(XError, send, (const Mysqlx::Crud::Delete &m), (override));
  MOCK_METHOD(XError, send, (const Mysqlx::Crud::CreateView &m), (override));
  MOCK_METHOD(XError, send, (const Mysqlx::Crud::ModifyView &m), (override));
  MOCK_METHOD(XError, send, (const Mysqlx::Crud::DropView &m), (override));
  MOCK_METHOD(XError, send, (const Mysqlx::Expect::Open &m), (override));
  MOCK_METHOD(XError, send, (const Mysqlx::Expect::Close &m), (override));
  MOCK_METHOD(XError, send, (const Mysqlx::Connection::CapabilitiesGet &m),
              (override));
  MOCK_METHOD(XError, send, (const Mysqlx::Connection::CapabilitiesSet &m),
              (override));
  MOCK_METHOD(XError, send, (const Mysqlx::Connection::Close &m), (override));
  MOCK_METHOD(XError, send, (const Mysqlx::Cursor::Open &m), (override));
  MOCK_METHOD(XError, send, (const Mysqlx::Cursor::Close &m), (override));
  MOCK_METHOD(XError, send, (const Mysqlx::Cursor::Fetch &m), (override));
  MOCK_METHOD(XError, send, (const Mysqlx::Prepare::Prepare &m), (override));
  MOCK_METHOD(XError, send, (const Mysqlx::Prepare::Execute &m), (override));
  MOCK_METHOD(XError, send, (const Mysqlx::Prepare::Deallocate &m), (override));
  MOCK_METHOD(XError, recv_ok, (), (override));
  MOCK_METHOD(XError, execute_close, (), (override));
  MOCK_METHOD(Capabilities *, execute_fetch_capabilities_raw,
              (XError * out_error));
  MOCK_METHOD(XError, execute_set_capability,
              (const ::Mysqlx::Connection::CapabilitiesSet &m), (override));
  MOCK_METHOD(XError, execute_authenticate,
              (const std::string &, const std::string &, const std::string &,
               const std::string &),
              (override));
  MOCK_METHOD(xcl::XQuery_result *, recv_resultset_raw, ());
  MOCK_METHOD(xcl::XQuery_result *, recv_resultset_raw, (XError * out_error));
  MOCK_METHOD(xcl::XQuery_result *, execute_with_resultset_raw,
              (const Client_message_type_id mid, const Message &msg,
               XError *out_error));
  MOCK_METHOD(xcl::XQuery_result *, execute_stmt_raw,
              (const Mysqlx::Sql::StmtExecute &m, XError *out_error));
  MOCK_METHOD(xcl::XQuery_result *, execute_find_raw,
              (const Mysqlx::Crud::Find &m, XError *out_error));
  MOCK_METHOD(xcl::XQuery_result *, execute_update_raw,
              (const Mysqlx::Crud::Update &m, XError *out_error));
  MOCK_METHOD(xcl::XQuery_result *, execute_insert_raw,
              (const Mysqlx::Crud::Insert &m, XError *out_error));
  MOCK_METHOD(xcl::XQuery_result *, execute_delete_raw,
              (const Mysqlx::Crud::Delete &m, XError *out_error));
  MOCK_METHOD(xcl::XQuery_result *, execute_prep_stmt_raw,
              (const Mysqlx::Prepare::Execute &m, XError *out_error));
  MOCK_METHOD(xcl::XQuery_result *, execute_cursor_open_raw,
              (const Mysqlx::Cursor::Open &m, XError *out_error));
  MOCK_METHOD(xcl::XQuery_result *, execute_cursor_fetch_raw,
              (const Mysqlx::Cursor::Fetch &m,
               const std::unique_ptr<xcl::XQuery_result> &cursor_open_res,
               XError *out_error));

  MOCK_METHOD(void, use_compression, (const Compression_algorithm algo),
              (override));
  MOCK_METHOD(void, use_compression,
              (const Compression_algorithm algo, const int32_t level),
              (override));
  MOCK_METHOD(void, reset_buffering, (), (override));

 private:
  using XQuery_result_ptr = std::unique_ptr<xcl::XQuery_result>;
  using Message_ptr = std::unique_ptr<Message>;
  using Capabilities_ptr = std::unique_ptr<Capabilities>;

  Message_ptr deserialize_received_message(const Header_message_type_id mid,
                                           const uint8_t *payload,
                                           const std::size_t payload_size,
                                           XError *out_error) override {
    return Message_ptr(deserialize_received_message_raw(
        mid, payload, payload_size, out_error));
  }

  Capabilities_ptr execute_fetch_capabilities(XError *out_error) override {
    return Capabilities_ptr(execute_fetch_capabilities_raw(out_error));
  }

  Message_ptr recv_single_message(Server_message_type_id *out_mid,
                                  XError *out_error) override {
    return Message_ptr(recv_single_message_raw(out_mid, out_error));
  }

  XQuery_result_ptr recv_resultset() override {
    return XQuery_result_ptr(recv_resultset_raw());
  }

  XQuery_result_ptr recv_resultset(XError *out_error) override {
    return XQuery_result_ptr(recv_resultset_raw(out_error));
  }

  XQuery_result_ptr execute_with_resultset(const Client_message_type_id mid,
                                           const Message &msg,
                                           XError *out_error) override {
    return XQuery_result_ptr(execute_with_resultset_raw(mid, msg, out_error));
  }

  XQuery_result_ptr execute_stmt(const Mysqlx::Sql::StmtExecute &m,
                                 XError *out_error) override {
    return XQuery_result_ptr(execute_stmt_raw(m, out_error));
  }

  XQuery_result_ptr execute_find(const Mysqlx::Crud::Find &m,
                                 XError *out_error) override {
    return XQuery_result_ptr(execute_find_raw(m, out_error));
  }

  XQuery_result_ptr execute_cursor_open(const Mysqlx::Cursor::Open &m,
                                        XError *out_error) override {
    return XQuery_result_ptr(execute_cursor_open_raw(m, out_error));
  }

  XQuery_result_ptr execute_cursor_fetch(const Mysqlx::Cursor::Fetch &m,
                                         XQuery_result_ptr cursor_open_result,
                                         XError *out_error) override {
    return XQuery_result_ptr(
        execute_cursor_fetch_raw(m, cursor_open_result, out_error));
  }

  XQuery_result_ptr execute_update(const Mysqlx::Crud::Update &m,
                                   XError *out_error) override {
    return XQuery_result_ptr(execute_update_raw(m, out_error));
  }

  XQuery_result_ptr execute_insert(const Mysqlx::Crud::Insert &m,
                                   XError *out_error) override {
    return XQuery_result_ptr(execute_insert_raw(m, out_error));
  }

  XQuery_result_ptr execute_delete(const Mysqlx::Crud::Delete &m,
                                   XError *out_error) override {
    return XQuery_result_ptr(execute_delete_raw(m, out_error));
  }

  XQuery_result_ptr execute_prep_stmt(const Mysqlx::Prepare::Execute &m,
                                      XError *out_error) override {
    return XQuery_result_ptr(execute_prep_stmt_raw(m, out_error));
  }
};

}  // namespace mock
}  // namespace test
}  // namespace xcl

#endif  // UNITTEST_GUNIT_XPLUGIN_XCL_MOCK_PROTOCOL_H_
