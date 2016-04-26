/*
 * Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

#ifndef _NGS_CLIENT_SESSION_H_
#define _NGS_CLIENT_SESSION_H_

#include "ngs/protocol_encoder.h"
#include "ngs/thread.h"
#include "ngs/protocol_authentication.h"
#include <boost/enable_shared_from_this.hpp>

namespace ngs
{
  class Client;
  class Protocol_encoder;


  class Session : public boost::enable_shared_from_this<Session>
  {
  public:
    typedef int32_t Session_id;

    class Delegate
    {
    public:
      virtual ~Delegate() {}

      virtual void on_session_auth_success(Session *s) = 0;
      virtual void on_session_close(Session *s) = 0;
    };

    Session(Client &client, Protocol_encoder *proto, Session_id session_id);
    virtual ~Session();

    Session_id session_id() const { return m_id; }
    bool is_ready() const;

  public:
    virtual void on_close(const bool update_old_state = false);
    virtual void on_kill();
    virtual void on_auth_success(const Authentication_handler::Response &response);
    virtual void on_auth_failure(const Authentication_handler::Response &response);

    // handle a single message, returns true if message was handled false if not
    virtual bool handle_message(ngs::Request &command);

    Client &client() const { return m_client; }

    Protocol_encoder &proto() { return *m_encoder; }
    virtual Error_code init() = 0;

  protected:
    virtual bool handle_auth_message(ngs::Request &command);
    virtual bool handle_ready_message(ngs::Request &command);

    void stop_auth();

  public:
    enum State
    {
      // start as Authenticating
      Authenticating,
      // once authenticated, we can handle work
      Ready,
      // connection is closing, but wait for data to flush out first
      Closing
    };

    State state() const { return m_state; }
    State state_before_close() const { return m_state_before_close; }

  protected:
    Client &m_client;
    Protocol_encoder *m_encoder;
    Authentication_handler_ptr m_auth_handler;
    State m_state;
    State m_state_before_close;

    Session_id m_id;
    // true if a session session was already scheduled for execution in a thread
    int32 m_thread_pending;
    // true if the session is currently assigned to a thread and executing
    int32 m_thread_active;

    void check_thread()
    {
#ifndef WIN32
      assert(mdbg_my_thread == pthread_self());
#endif
    }
#ifndef WIN32
    pthread_t mdbg_my_thread;
#endif
  };


} // namespace ngs

#endif
