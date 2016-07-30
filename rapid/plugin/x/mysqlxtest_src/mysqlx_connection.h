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

#ifndef _MYSQLX_CONNECTION_H_
#define _MYSQLX_CONNECTION_H_

#undef ERROR //Needed to avoid conflict with ERROR in mysqlx.pb.h

// Avoid warnings from includes of other project and protobuf
#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#elif defined _MSC_VER
#pragma warning (push)
#pragma warning (disable : 4018 4996)
#endif

#include "ngs_common/protocol_protobuf.h"
#include <boost/enable_shared_from_this.hpp>

#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
#pragma GCC diagnostic pop
#elif defined _MSC_VER
#pragma warning (pop)
#endif

#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>
#include <list>

#include "mysqlx_sync_connection.h"
#include "mysqlx_common.h"

#define CR_UNKNOWN_ERROR        2000
#define CR_CONNECTION_ERROR     2002
#define CR_UNKNOWN_HOST         2005
#define CR_SERVER_GONE_ERROR    2006
#define CR_BROKEN_PIPE          2007
#define CR_WRONG_HOST_INFO      2009
#define CR_COMMANDS_OUT_OF_SYNC 2014
#define CR_SSL_CONNECTION_ERROR 2026
#define CR_MALFORMED_PACKET     2027
#define CR_INVALID_AUTH_METHOD  2028

namespace mysqlx
{
  typedef boost::function<bool (int,std::string)> Local_notice_handler;

  struct Ssl_config
  {
    Ssl_config()
    : key(NULL), ca(NULL), ca_path(NULL),
      cert(NULL), cipher(NULL), tls_version(NULL)
    {}

    const char *key;
    const char *ca;
    const char *ca_path;
    const char *cert;
    const char *cipher;
    const char *tls_version;
  };

  class MYSQLXTEST_PUBLIC Connection : public boost::enable_shared_from_this<Connection>
  {
  public:
    Connection(const Ssl_config &ssl_config, const std::size_t timeout, const bool dont_wait_for_disconnect = true);
    ~Connection();

    uint64_t client_id() const { return m_client_id; }
    const Mysqlx::Connection::Capabilities &capabilities() const { return m_capabilities; }

    void push_local_notice_handler(Local_notice_handler handler);
    void pop_local_notice_handler();

    void connect(const std::string &uri, const std::string &pass, const bool cap_expired_password = false); //XXX capabilities flags
    void connect(const std::string &host, int port);

    void close();
    void set_closed();
    bool is_closed() const { return m_closed; }

    void enable_tls();

    void send(int mid, const Message &msg);
    Message *recv_next(int &mid);

    Message *recv_raw(int &mid);
    Message *recv_payload(const int mid, const std::size_t msglen);
    Message *recv_raw_with_deadline(int &mid, const int deadline_milliseconds);

    boost::shared_ptr<Result> recv_result();

    // Overrides for Client Session Messages
    void send(const Mysqlx::Session::AuthenticateStart &m) { send(Mysqlx::ClientMessages::SESS_AUTHENTICATE_START, m); };
    void send(const Mysqlx::Session::AuthenticateContinue &m) { send(Mysqlx::ClientMessages::SESS_AUTHENTICATE_CONTINUE, m); };
    void send(const Mysqlx::Session::Reset &m) { send(Mysqlx::ClientMessages::SESS_RESET, m); };
    void send(const Mysqlx::Session::Close &m) { send(Mysqlx::ClientMessages::SESS_CLOSE, m); };

    // Overrides for SQL Messages
    void send(const Mysqlx::Sql::StmtExecute &m) { send(Mysqlx::ClientMessages::SQL_STMT_EXECUTE, m); };

    // Overrides for CRUD operations
    void send(const Mysqlx::Crud::Find &m) { send(Mysqlx::ClientMessages::CRUD_FIND, m); };
    void send(const Mysqlx::Crud::Insert &m) { send(Mysqlx::ClientMessages::CRUD_INSERT, m); };
    void send(const Mysqlx::Crud::Update &m) { send(Mysqlx::ClientMessages::CRUD_UPDATE, m); };
    void send(const Mysqlx::Crud::Delete &m) { send(Mysqlx::ClientMessages::CRUD_DELETE, m); };

    // Overrides for Connection
    void send(const Mysqlx::Connection::CapabilitiesGet &m) { send(Mysqlx::ClientMessages::CON_CAPABILITIES_GET, m); };
    void send(const Mysqlx::Connection::CapabilitiesSet &m) { send(Mysqlx::ClientMessages::CON_CAPABILITIES_SET, m); };
    void send(const Mysqlx::Connection::Close &m) { send(Mysqlx::ClientMessages::CON_CLOSE, m); };

  public:
    boost::shared_ptr<Result> execute_sql(const std::string &sql);
    boost::shared_ptr<Result> execute_stmt(const std::string &ns, const std::string &sql, const std::vector<ArgumentValue> &args);

    boost::shared_ptr<Result> execute_find(const Mysqlx::Crud::Find &m);
    boost::shared_ptr<Result> execute_update(const Mysqlx::Crud::Update &m);
    boost::shared_ptr<Result> execute_insert(const Mysqlx::Crud::Insert &m);
    boost::shared_ptr<Result> execute_delete(const Mysqlx::Crud::Delete &m);

    void fetch_capabilities();
    void setup_capability(const std::string &name, const bool value);

    void authenticate(const std::string &user, const std::string &pass, const std::string &schema);
    void authenticate_plain(const std::string &user, const std::string &pass, const std::string &db);
    void authenticate_mysql41(const std::string &user, const std::string &pass, const std::string &db);

    void send_bytes(const std::string &data);

    void set_trace_protocol(bool flag) { m_trace_packets = flag; }

    boost::shared_ptr<Result> new_empty_result();

  private:
    void perform_close();
    void dispatch_notice(Mysqlx::Notice::Frame *frame);
    Message *recv_message_with_header(int &mid, char (&header_buffer)[5], const std::size_t header_offset);
    void throw_mysqlx_error(const boost::system::error_code &ec);
    boost::shared_ptr<Result> new_result(bool expect_data);

  private:
    std::list<Local_notice_handler> m_local_notice_handlers;
    Mysqlx::Connection::Capabilities m_capabilities;

    Mysqlx_sync_connection m_sync_connection;
    uint64_t m_client_id;
    bool m_trace_packets;
    bool m_closed;
    const bool m_dont_wait_for_disconnect;
    boost::shared_ptr<Result> m_last_result;
  };

  typedef boost::shared_ptr<Connection> ConnectionRef;
} // namespace mysqlx

#endif // _MYSQLX_CONNECTION_H_
