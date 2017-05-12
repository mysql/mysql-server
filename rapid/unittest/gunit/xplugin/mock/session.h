/*
 * Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
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

#include "account_verification_handler.h"
#include "ngs_common/connection_vio.h"
#include "ngs/interface/account_verification_interface.h"
#include "ngs/interface/client_interface.h"
#include "ngs/interface/server_interface.h"
#include "ngs/interface/sql_session_interface.h"
#include "ngs/scheduler.h"
#include "sql_data_context.h"
#include "xpl_resultset.h"
#include "xpl_session.h"


namespace ngs
{
namespace test
{

class Mock_connection : public ngs::Connection_vio
{
public:
  Mock_connection()
  : ngs::Connection_vio(m_context, NULL)
  {
  }

  MOCK_METHOD0(connection_type, Connection_type ());
  MOCK_METHOD0(options, ngs::IOptions_session_ptr());
private:
  Ssl_context m_context;
};


class Mock_scheduler_dynamic : public Scheduler_dynamic
{
public:
  Mock_scheduler_dynamic()
  : Scheduler_dynamic("")
  {
  }

  MOCK_METHOD0(launch, void());
  MOCK_METHOD0(stop, void());
  MOCK_METHOD0(thread_init, bool ());
  MOCK_METHOD0(thread_end, void ());
  MOCK_METHOD1(set_num_workers, unsigned int(unsigned int n));
};


class Mock_server : public ngs::Server_interface
{
public:


  Authentication_interface_ptr get_auth_handler(const std::string &p1, Session_interface *p2)
  {
    return Authentication_interface_ptr(get_auth_handler2(p1, p2));
  }

  MOCK_METHOD2(get_auth_handler2, Authentication_interface_ptr::element_type *(const std::string &, Session_interface *));
  MOCK_CONST_METHOD0(get_config, ngs::shared_ptr<Protocol_config> ());
  MOCK_METHOD0(is_running, bool ());
  MOCK_CONST_METHOD0(get_worker_scheduler, ngs::shared_ptr<Scheduler_dynamic> ());
  MOCK_CONST_METHOD0(ssl_context, Ssl_context *());
  MOCK_METHOD1(on_client_closed, void (const Client_interface &));
  MOCK_METHOD3(create_session, ngs::shared_ptr<Session_interface> (Client_interface &, Protocol_encoder &, int));
  MOCK_METHOD0(get_client_exit_mutex, Mutex &());
  MOCK_METHOD0(restart_client_supervision_timer, void ());

  // Workaround for GMOCK undefined behaviour with ResultHolder
  MOCK_METHOD2(get_authentication_mechanisms_void, bool (std::vector<std::string> &auth_mech, Client_interface &client));
  void get_authentication_mechanisms(std::vector<std::string> &auth_mech, Client_interface &client)
  {
    get_authentication_mechanisms_void(auth_mech, client);
  }
};

class Mock_authentication_interface : public ngs::Authentication_interface
{
 public:
  MOCK_METHOD3(handle_start, Response(const std::string &, const std::string &,
                                      const std::string &));

  MOCK_METHOD1(handle_continue, Response(const std::string &));

  MOCK_CONST_METHOD3(authenticate_account,
                     ngs::Error_code(const std::string &, const std::string &,
                                     const std::string &));
};

class Mock_account_verification
    : public ngs::Account_verification_interface {
 public:
  MOCK_CONST_METHOD0(get_salt, const std::string&());
  MOCK_CONST_METHOD2(verify_authentication_string,
                     bool(const std::string &, const std::string &));
};

class Mock_sql_data_context : public ngs::Sql_session_interface {
 public:
  MOCK_METHOD1(set_connection_type, Error_code(const Connection_type));
  MOCK_METHOD1(execute_kill_sql_session, Error_code(uint64_t));
  MOCK_CONST_METHOD0(is_killed, bool());
  MOCK_CONST_METHOD0(password_expired, bool());
  MOCK_METHOD0(proto, Protocol_encoder &());
  MOCK_CONST_METHOD0(get_authenticated_user_name, std::string());
  MOCK_CONST_METHOD0(get_authenticated_user_host, std::string());
  MOCK_CONST_METHOD0(has_authenticated_user_a_super_priv, bool());
  MOCK_CONST_METHOD0(mysql_session_id, uint64_t());
  MOCK_METHOD7(authenticate,
               Error_code(const char *, const char *, const char *,
                          const char *, const std::string &,
                          const Authentication_interface &, bool));
  MOCK_METHOD3(execute,
               Error_code(const char *, std::size_t, Resultset_interface *));
};
}  // namespace test
}  // namespace ngs


namespace xpl
{
namespace test
{


class Mock_client : public ngs::Client_interface
{
public:
  MOCK_METHOD0(get_session_exit_mutex, ngs::Mutex &());

  MOCK_CONST_METHOD0(client_id, const char *());

  MOCK_CONST_METHOD0(client_address, const char *());
  MOCK_CONST_METHOD0(client_hostname, const char *());
  MOCK_METHOD0(connection, ngs::Connection_vio  &());
  MOCK_CONST_METHOD0(server, ngs::Server_interface &());

  MOCK_CONST_METHOD0(client_id_num, Client_id ());
  MOCK_CONST_METHOD0(client_port, int ());

  MOCK_CONST_METHOD0(get_accept_time, ngs::chrono::time_point ());
  MOCK_CONST_METHOD0(get_state, Client_state ());

  MOCK_METHOD0(session, ngs::shared_ptr<ngs::Session_interface> ());
  MOCK_CONST_METHOD0(supports_expired_passwords, bool ());

public:
  MOCK_METHOD1(on_session_reset_void, bool (ngs::Session_interface &));
  MOCK_METHOD1(on_session_close_void, bool (ngs::Session_interface &));
  MOCK_METHOD1(on_session_auth_success_void, bool (ngs::Session_interface &));

  MOCK_METHOD0(disconnect_and_trigger_close_void, bool ());
  MOCK_METHOD0(activate_tls_void, bool ());
  MOCK_METHOD0(on_auth_timeout_void, bool ());
  MOCK_METHOD0(on_server_shutdown_void, bool ());
  MOCK_METHOD1(run_void, bool (bool));
  MOCK_METHOD0(reset_accept_time_void, bool ());

  void on_session_reset(ngs::Session_interface &arg)
  {
    on_session_reset_void(arg);
  }

  void on_session_close(ngs::Session_interface &arg)
  {
    on_session_close_void(arg);
  }

  void on_session_auth_success(ngs::Session_interface &arg)
  {
    on_session_auth_success_void(arg);
  }

  void disconnect_and_trigger_close()
  {
    disconnect_and_trigger_close_void();
  }

  void activate_tls()
  {
    activate_tls_void();
  }

  void on_auth_timeout()
  {
    on_auth_timeout_void();
  }

  void on_server_shutdown()
  {
    on_server_shutdown_void();
  }

  void run(bool arg)
  {
    run_void(arg);
  }

  void reset_accept_time()
  {
    reset_accept_time_void();
  }
};

class Mock_session : public xpl::Session
{
public:
  Mock_session(ngs::Client_interface* client)
  : xpl::Session(*client, NULL, 0)
  {
  }

  MOCK_METHOD0(data_context, ngs::Sql_session_interface&());
};


class Mock_account_verification_handler
    : public xpl::Account_verification_handler {
 public:
  Mock_account_verification_handler(xpl::Session *session)
      : xpl::Account_verification_handler(session) {}

  MOCK_CONST_METHOD2(authenticate,
                     ngs::Error_code(const ngs::Authentication_interface &,
                                     const std::string &));
  MOCK_CONST_METHOD1(
      get_account_verificator,
      const ngs::Account_verification_interface *(
          const ngs::Account_verification_interface::Account_type));
};

} // namespace test
}  // namespace xpl
