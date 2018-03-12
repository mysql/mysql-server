/*
 * Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
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

#include <assert.h>

#include "my_inttypes.h"
#include "plugin/x/ngs/include/ngs/interface/authentication_interface.h"
#include "plugin/x/ngs/include/ngs/interface/protocol_encoder_interface.h"
#include "plugin/x/ngs/include/ngs/interface/session_interface.h"
#include "plugin/x/ngs/include/ngs/thread.h"

namespace ngs {
class Client;

class Session : public Session_interface {
 public:
  typedef int32_t Session_id;

  Session(Client_interface &client, Protocol_encoder_interface *proto,
          const Session_id session_id);
  ~Session() override;

  Session_id session_id() const override { return m_id; }

 public:
  void on_close(const bool update_old_state = false) override;
  void on_auth_success(
      const Authentication_interface::Response &response) override;
  void on_auth_failure(
      const Authentication_interface::Response &response) override;

  // handle a single message, returns true if message was handled false if not
  bool handle_message(ngs::Message_request &command) override;

  Client_interface &client() override { return m_client; }

  Protocol_encoder_interface &proto() override { return *m_encoder; }

 protected:
  virtual bool handle_auth_message(ngs::Message_request &command);
  virtual bool handle_ready_message(ngs::Message_request &command);

  void stop_auth();

  static bool can_forward_error_code_to_client(const int error_code);

 public:
  State state() const override { return m_state; }
  State state_before_close() const override { return m_state_before_close; }

  bool can_authenticate_again() const;

 protected:
  Client_interface &m_client;
  Protocol_encoder_interface *m_encoder;
  Authentication_interface_ptr m_auth_handler;
  State m_state;
  State m_state_before_close;
  uint8_t m_failed_auth_count = 0;
  const uint8_t k_max_auth_attempts = 3;

  const Session_id m_id;
  // true if a session session was already scheduled for execution in a thread
  int32 m_thread_pending;
  // true if the session is currently assigned to a thread and executing
  int32 m_thread_active;

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
