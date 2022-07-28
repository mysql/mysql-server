/*
 * Copyright (c) 2015, 2022, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
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

#ifndef PLUGIN_X_SRC_SESSION_H_
#define PLUGIN_X_SRC_SESSION_H_

#include <cassert>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "plugin/x/src/document_id_aggregator.h"
#include "plugin/x/src/interface/authentication.h"
#include "plugin/x/src/interface/notice_output_queue.h"
#include "plugin/x/src/interface/protocol_encoder.h"
#include "plugin/x/src/interface/session.h"
#include "plugin/x/src/mq/notice_configuration.h"
#include "plugin/x/src/mq/notice_output_queue.h"
#include "plugin/x/src/ngs/session_status_variables.h"
#include "plugin/x/src/ngs/thread.h"
#include "plugin/x/src/session.h"
#include "plugin/x/src/sql_data_context.h"
#include "plugin/x/src/variables/xpl_global_status_variables.h"
#include "plugin/x/src/xpl_dispatcher.h"

namespace xpl {

class Session : public iface::Session {
 public:
  Session(iface::Client *client, iface::Protocol_encoder *proto,
          const Session_id session_id);
  ~Session() override;

 public:  // impl iface::Session
  Session_id session_id() const override { return m_id; }
  ngs::Error_code init() override;

  void on_close(
      const Close_flags flags = Close_flags::k_force_close_client) override;
  void on_kill() override;
  void on_auth_success(
      const iface::Authentication::Response &response) override;
  void on_auth_failure(
      const iface::Authentication::Response &response) override;
  void on_reset() override;

  // handle a single message, returns true if message was handled false if not
  bool handle_message(const ngs::Message_request &command) override;

  State state() const override { return m_state; }
  State state_before_close() const override { return m_state_before_close; }

  iface::Client &client() override { return *m_client; }
  const iface::Client &client() const override { return *m_client; }
  bool can_see_user(const std::string &user) const override;

  iface::Notice_output_queue &get_notice_output_queue() override {
    return m_notice_output_queue;
  }
  iface::Notice_configuration &get_notice_configuration() override {
    return m_notice_configuration;
  }
  ngs::Session_status_variables &get_status_variables() override {
    return m_status_variables;
  }

  void mark_as_tls_session() override;
  THD *get_thd() const override { return m_sql.get_thd(); }
  iface::Sql_session &data_context() override { return m_sql; }
  iface::Protocol_encoder &proto() override { return *m_encoder; }
  void set_proto(iface::Protocol_encoder *encoder) override;
  bool get_prepared_statement_id(const uint32_t client_stmt_id,
                                 uint32_t *out_stmt_id) const override;

  void update_status(Common_status_variable variable) override;

  iface::Document_id_aggregator &get_document_id_aggregator() override {
    return m_document_id_aggregator;
  }

 protected:
  bool handle_auth_message(const ngs::Message_request &command);
  bool handle_ready_message(const ngs::Message_request &command);

  void stop_auth();

  static bool can_forward_error_code_to_client(const int error_code);
  ngs::Error_code get_authentication_access_denied_error() const;

  bool can_authenticate_again() const;

  void on_auth_failure_impl(const iface::Authentication::Response &response);

 protected:
  iface::Client *m_client;
  iface::Protocol_encoder *m_encoder;
  std::unique_ptr<iface::Authentication> m_auth_handler;
  State m_state;
  State m_state_before_close;
  uint8_t m_failed_auth_count = 0;
  const uint8_t k_max_auth_attempts = 3;

  const Session_id m_id;
  // true if a session session was already scheduled for execution in a thread
  int32_t m_thread_pending;
  // true if the session is currently assigned to a thread and executing
  int32_t m_thread_active;
#ifndef WIN32
  pthread_t mdbg_my_thread;
#endif
  Sql_data_context m_sql;
  Notice_configuration m_notice_configuration;
  Dispatcher m_dispatcher{this};
  Notice_output_queue m_notice_output_queue;
  ngs::Session_status_variables m_status_variables;
  bool m_was_authenticated;
  Document_id_aggregator m_document_id_aggregator;
};
}  // namespace xpl

#endif  // PLUGIN_X_SRC_SESSION_H_
