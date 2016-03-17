/*
 * Copyright (c) 2015, 2016 Oracle and/or its affiliates. All rights reserved.
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


#include "ngs/server.h"
#include "ngs_common/connection_vio.h"
#include "ngs/scheduler.h"
#include "xpl_client.h"
#include "xpl_session.h"
#include "sql_data_context.h"


namespace ngs
{
namespace test
{

class Mock_connection : public ngs::Connection_vio
{
public:
  Mock_connection()
  : ngs::Connection_vio(m_context, 0)
  {
  }

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


class Mock_server : public ngs::IServer
{
public:
  MOCK_METHOD2(get_authentication_mechanisms, void (std::vector<std::string> &auth_mech, Client &client));

  Authentication_handler_ptr get_auth_handler(const std::string &p1, Session *p2)
  {
    return Authentication_handler_ptr(get_auth_handler2(p1, p2));
  }

  MOCK_METHOD2(get_auth_handler2, Authentication_handler_ptr::element_type *(const std::string &, Session *));
  MOCK_CONST_METHOD0(config, boost::shared_ptr<Protocol_config> ());
  MOCK_METHOD0(is_running, bool ());
  MOCK_CONST_METHOD0(worker_scheduler, boost::shared_ptr<Scheduler_dynamic> ());
  MOCK_CONST_METHOD0(ssl_context, Ssl_context *());
  MOCK_METHOD1(on_client_closed, void (boost::shared_ptr<Client>));
  MOCK_METHOD0(restart_client_supervision_timer, void());
  MOCK_METHOD3(create_session, boost::shared_ptr<Session> (boost::shared_ptr<Client> , Protocol_encoder *, int));
  MOCK_METHOD0(get_client_exit_mutex, Mutex &());
};

}  // namespace test
}  // namespace ngs


namespace xpl
{
namespace test
{


class Mock_client : public xpl::Client
{
public:
  Mock_client()
  : xpl::Client(ngs::Connection_ptr(), NULL, 0, NULL)
  {
  }

  MOCK_CONST_METHOD0(client_address, const char *());
  MOCK_CONST_METHOD0(client_hostname, const std::string&());
  MOCK_METHOD0(connection, ngs::Connection_vio  &());
  MOCK_METHOD0(activate_tls, void ());
  MOCK_CONST_METHOD0(server, ngs::IServer *());
};

class Mock_session : public xpl::Session
{
public:
  Mock_session(ngs::Client* client)
  : xpl::Session(*client, NULL, 0)
  {
  }

  MOCK_METHOD0(data_context, xpl::Sql_data_context&());
};


class Mock_sql_data_context : public xpl::Sql_data_context
{
public:
  Mock_sql_data_context(ngs::Protocol_encoder *p=0)
  : xpl::Sql_data_context(p)
  {
  }

  MOCK_METHOD7(authenticate, ngs::Error_code (const char *user, const char *host, const char *ip, const char *db, On_user_password_hash cb_password_hash, bool, ngs::IOptions_session_ptr &));
};


} // namespace test
}  // namespace xpl
