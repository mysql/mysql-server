/*
 * Copyright (c) 2015, 2023, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_SRC_INTERFACE_SESSION_H_
#define PLUGIN_X_SRC_INTERFACE_SESSION_H_

#include <string>

#include "plugin/x/src/interface/authentication.h"
#include "plugin/x/src/interface/document_id_aggregator.h"
#include "plugin/x/src/interface/notice_configuration.h"
#include "plugin/x/src/interface/notice_output_queue.h"
#include "plugin/x/src/interface/protocol_encoder.h"
#include "plugin/x/src/interface/sql_session.h"
#include "plugin/x/src/ngs/notice_descriptor.h"
#include "plugin/x/src/ngs/session_status_variables.h"

namespace xpl {
namespace iface {

class Client;

class Session {
 public:
  using Session_id = int32_t;

  enum class State {
    // start as authenticating
    k_authenticating,
    // once authenticated, we can handle work
    k_ready,
    // connection is closing, but wait for data to flush out first
    k_closing
  };

  enum class Close_flags {
    k_none = 0,
    k_update_old_state = 1,
    k_force_close_client = 2
  };

 public:
  virtual ~Session() = default;

  virtual Session_id session_id() const = 0;
  virtual ngs::Error_code init() = 0;

 public:
  virtual void on_close(
      const Close_flags flags = Close_flags::k_force_close_client) = 0;
  virtual void on_kill() = 0;
  virtual void on_auth_success(const Authentication::Response &response) = 0;
  virtual void on_auth_failure(const Authentication::Response &response) = 0;
  virtual void on_reset() = 0;

  // handle a single message, returns true if message was handled false if not
  virtual bool handle_message(const ngs::Message_request &command) = 0;

  virtual State state() const = 0;
  virtual State state_before_close() const = 0;

  virtual Client &client() = 0;
  virtual const Client &client() const = 0;
  virtual bool can_see_user(const std::string &user) const = 0;

  virtual Notice_output_queue &get_notice_output_queue() = 0;
  virtual Notice_configuration &get_notice_configuration() = 0;
  virtual ngs::Session_status_variables &get_status_variables() = 0;
  virtual void mark_as_tls_session() = 0;
  virtual THD *get_thd() const = 0;
  virtual iface::Sql_session &data_context() = 0;
  virtual Protocol_encoder &proto() = 0;
  virtual void set_proto(Protocol_encoder *encode) = 0;
  virtual bool get_prepared_statement_id(const uint32_t client_stmt_id,
                                         uint32_t *stmt_id) const = 0;
  using Common_status_variable =
      ngs::Common_status_variables::Variable ngs::Common_status_variables::*;

  virtual void update_status(Common_status_variable variable) = 0;

  virtual Document_id_aggregator &get_document_id_aggregator() = 0;
};

inline Session::Close_flags operator|(const Session::Close_flags a,
                                      const Session::Close_flags b) {
  return static_cast<Session::Close_flags>(static_cast<int>(a) |
                                           static_cast<int>(b));
}

inline bool operator&(const Session::Close_flags a,
                      const Session::Close_flags b) {
  return (static_cast<int>(a) & static_cast<int>(b));
}

}  // namespace iface
}  // namespace xpl

#endif  // PLUGIN_X_SRC_INTERFACE_SESSION_H_
