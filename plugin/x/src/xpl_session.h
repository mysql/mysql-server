/*
 * Copyright (c) 2015, 2019, Oracle and/or its affiliates. All rights reserved.
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

#ifndef PLUGIN_X_SRC_XPL_SESSION_H_
#define PLUGIN_X_SRC_XPL_SESSION_H_

#include <map>
#include <string>
#include <vector>

#include "plugin/x/ngs/include/ngs/client_session.h"
#include "plugin/x/ngs/include/ngs/session_status_variables.h"
#include "plugin/x/src/document_id_aggregator.h"
#include "plugin/x/src/interface/notice_output_queue.h"
#include "plugin/x/src/interface/protocol_encoder.h"
#include "plugin/x/src/mq/notice_configuration.h"
#include "plugin/x/src/mq/notice_output_queue.h"
#include "plugin/x/src/sql_data_context.h"
#include "plugin/x/src/xpl_dispatcher.h"
#include "plugin/x/src/xpl_global_status_variables.h"

namespace xpl {

class Sql_data_context;
class Cursor_manager;
class Client;

class Session : public ngs::Session {
 public:
  Session(iface::Client *client, iface::Protocol_encoder *proto,
          const Session_id session_id);
  ~Session() override;

 public:  // impl iface::Session
  ngs::Error_code init() override;
  void on_auth_success(
      const iface::Authentication::Response &response) override;
  void on_auth_failure(
      const iface::Authentication::Response &response) override;
  void on_reset() override;

  void mark_as_tls_session() override;
  THD *get_thd() const override;
  bool can_see_user(const std::string &user) const override;
  iface::Sql_session &data_context() override { return m_sql; }
  iface::Notice_output_queue &get_notice_output_queue() override {
    return m_notice_output_queue;
  }
  iface::Notice_configuration &get_notice_configuration() override {
    return m_notice_configuration;
  }

  void set_proto(iface::Protocol_encoder *encoder) override;

 public:
  using Variable =
      ngs::Common_status_variables::Variable ngs::Common_status_variables::*;

  ngs::Session_status_variables &get_status_variables() override {
    return m_status_variables;
  }

  void update_status(Variable variable) override;

  bool get_prepared_statement_id(const uint32_t client_stmt_id,
                                 uint32_t *out_stmt_id) const override;
  iface::Document_id_aggregator &get_document_id_aggregator() override {
    return m_document_id_aggregator;
  }

 private:  // reimpl ngs::Session
  void on_kill() override;

  bool handle_ready_message(const ngs::Message_request &command) override;

 private:
  Sql_data_context m_sql;
  Notice_configuration m_notice_configuration;
  Dispatcher m_dispatcher{this};
  Notice_output_queue m_notice_output_queue;
  ngs::Session_status_variables m_status_variables;
  bool m_was_authenticated;
  Document_id_aggregator m_document_id_aggregator;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_XPL_SESSION_H_
