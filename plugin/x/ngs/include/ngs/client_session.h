/*
 * Copyright (c) 2015, 2020, Oracle and/or its affiliates. All rights reserved.
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

#ifndef PLUGIN_X_NGS_INCLUDE_NGS_CLIENT_SESSION_H_
#define PLUGIN_X_NGS_INCLUDE_NGS_CLIENT_SESSION_H_

#include <cassert>
#include <cstdint>
#include <memory>

#include "plugin/x/ngs/include/ngs/thread.h"
#include "plugin/x/src/interface/authentication.h"
#include "plugin/x/src/interface/protocol_encoder.h"
#include "plugin/x/src/interface/session.h"

namespace ngs {
class Client;

class Session : public xpl::iface::Session {
 public:
  typedef int32_t Session_id;

  Session(xpl::iface::Client *client, xpl::iface::Protocol_encoder *proto,
          const Session_id session_id);
  ~Session() override;

  Session_id session_id() const override { return m_id; }

 public:
  void on_close(
      const Close_flags flags = Close_flags::k_force_close_client) override;
  void on_auth_success(
      const xpl::iface::Authentication::Response &response) override;
  void on_auth_failure(
      const xpl::iface::Authentication::Response &response) override;

  // handle a single message, returns true if message was handled false if not
  bool handle_message(const ngs::Message_request &command) override;

  xpl::iface::Client &client() override { return *m_client; }
  const xpl::iface::Client &client() const override { return *m_client; }

  xpl::iface::Protocol_encoder &proto() override { return *m_encoder; }
  void set_proto(xpl::iface::Protocol_encoder *encode) override;

 protected:
  virtual bool handle_auth_message(const ngs::Message_request &command);
  virtual bool handle_ready_message(const ngs::Message_request &command);

  void stop_auth();

  static bool can_forward_error_code_to_client(const int error_code);
  Error_code get_authentication_access_denied_error() const;

 public:
  State state() const override { return m_state; }
  State state_before_close() const override { return m_state_before_close; }

  bool can_authenticate_again() const;

 protected:
  xpl::iface::Client *m_client;
  xpl::iface::Protocol_encoder *m_encoder;
  std::unique_ptr<xpl::iface::Authentication> m_auth_handler;
  State m_state;
  State m_state_before_close;
  uint8_t m_failed_auth_count = 0;
  const uint8_t k_max_auth_attempts = 3;

  const Session_id m_id;
  // true if a session session was already scheduled for execution in a thread
  int32_t m_thread_pending;
  // true if the session is currently assigned to a thread and executing
  int32_t m_thread_active;

  void check_thread() {
#ifndef WIN32
    assert(mdbg_my_thread == pthread_self());
#endif
  }
#ifndef WIN32
  pthread_t mdbg_my_thread;
#endif
};

}  // namespace ngs

#endif  // PLUGIN_X_NGS_INCLUDE_NGS_CLIENT_SESSION_H_
