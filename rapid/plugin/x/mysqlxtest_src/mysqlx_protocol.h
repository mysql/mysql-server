/*
 * Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
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

#ifndef _MYSQLX_PROTOCOL_H_
#define _MYSQLX_PROTOCOL_H_

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

#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
#pragma GCC diagnostic pop
#elif defined _MSC_VER
#pragma warning (pop)
#endif

#include "ngs_common/smart_ptr.h"
#include "ngs_common/bind.h"
#include <list>
#include <assert.h>

#include "ngs_common/protocol_protobuf.h"
#include "mysqlx_connection.h"


namespace mysqlx
{
  typedef google::protobuf::Message Message;
  typedef ngs::function<bool (int,std::string)> Local_notice_handler;

  class Result;

  class ArgumentValue
  {
  public:
    enum Type
    {
      TInteger,
      TUInteger,
      TNull,
      TDouble,
      TFloat,
      TBool,
      TString,
      TOctets,
    };

    ArgumentValue(const ArgumentValue &other)
    {
      m_type = other.m_type;
      m_value = other.m_value;
      if (m_type == TString || m_type == TOctets)
        m_value.s = new std::string(*other.m_value.s);
    }

    ArgumentValue &operator = (const ArgumentValue &other)
    {
      if (&other == this)
        return *this;

      m_type = other.m_type;
      m_value = other.m_value;
      if (m_type == TString || m_type == TOctets)
        m_value.s = new std::string(*other.m_value.s);

      return *this;
    }

    explicit ArgumentValue(const std::string &s, Type type = TString)
    {
      assert(type == TOctets || type == TString);
      m_type = type;
      m_value.s = new std::string(s);
    }

    explicit ArgumentValue(int64_t n)
    {
      m_type = TInteger;
      m_value.i = n;
    }

    explicit ArgumentValue(uint64_t n)
    {
      m_type = TUInteger;
      m_value.ui = n;
    }

    explicit ArgumentValue(double n)
    {
      m_type = TDouble;
      m_value.d = n;
    }

    explicit ArgumentValue(float n)
    {
      m_type = TFloat;
      m_value.f = n;
    }

    explicit ArgumentValue(bool n)
    {
      m_type = TBool;
      m_value.b = n;
    }

    explicit ArgumentValue()
    {
      m_type = TNull;
    }

    ~ArgumentValue()
    {
      if (m_type == TString || m_type == TOctets)
        delete m_value.s;
    }

    inline Type type() const { return m_type; }

    inline operator uint64_t () const
    {
      if (m_type != TUInteger)
        throw std::logic_error("type error");
      return m_value.ui;
    }

    inline operator int64_t () const
    {
      if (m_type != TInteger)
        throw std::logic_error("type error");
      return m_value.i;
    }

    inline operator double() const
    {
      if (m_type != TDouble)
        throw std::logic_error("type error");
      return m_value.d;
    }

    inline operator float() const
    {
      if (m_type != TFloat)
        throw std::logic_error("type error");
      return m_value.f;
    }

    inline operator bool() const
    {
      if (m_type != TBool)
        throw std::logic_error("type error");
      return m_value.b;
    }

    inline operator const std::string & () const
    {
      if (m_type != TString && m_type != TOctets)
        throw std::logic_error("type error");
      return *m_value.s;
    }

  private:
    Type m_type;
    union
    {
      std::string *s;
      int64_t i;
      uint64_t ui;
      double d;
      float f;
      bool b;
    } m_value;
  };

  struct Ssl_config
  {
    Ssl_config()
    : key(NULL),
      ca(NULL),
      ca_path(NULL),
      cert(NULL),
      cipher(NULL),
      tls_version(NULL)
    {
    }

    const char *key;
    const char *ca;
    const char *ca_path;
    const char *cert;
    const char *cipher;
    const char *tls_version;
  };

  enum Internet_protocol
  {
    IP_any = 0,
    IPv4,
    IPv6,
  };

  class XProtocol : public ngs::enable_shared_from_this<XProtocol>
  {
  public:
    XProtocol(const Ssl_config &ssl_config, const std::size_t timeout, const bool dont_wait_for_disconnect = true, const Internet_protocol ip_mode = IPv4);
    ~XProtocol();

    uint64_t client_id() const { return m_client_id; }
    const Mysqlx::Connection::Capabilities &capabilities() const { return m_capabilities; }

    void push_local_notice_handler(Local_notice_handler handler);
    void pop_local_notice_handler();

    void connect(const std::string &uri, const std::string &pass, const bool cap_expired_password = false); //XXX capabilities flags
    void connect(const std::string &host, int port);
    void connect_to_localhost(const std::string &unix_socket_or_named_pipe);

    void close();
    void set_closed();
    bool is_closed() const { return m_closed; }

    void enable_tls();

    void send(int mid, const Message &msg);
    Message *recv_next(int &mid);

    Message *recv_raw(int &mid);
    Message *recv_payload(const int mid, const std::size_t msglen);
    Message *recv_raw_with_deadline(int &mid, const int deadline_milliseconds);

    ngs::shared_ptr<Result> recv_result();
    ngs::shared_ptr<Result> new_empty_result();

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
    ngs::shared_ptr<Result> execute_sql(const std::string &sql);
    ngs::shared_ptr<Result> execute_stmt(const std::string &ns, const std::string &sql, const std::vector<ArgumentValue> &args);

    ngs::shared_ptr<Result> execute_find(const Mysqlx::Crud::Find &m);
    ngs::shared_ptr<Result> execute_update(const Mysqlx::Crud::Update &m);
    ngs::shared_ptr<Result> execute_insert(const Mysqlx::Crud::Insert &m);
    ngs::shared_ptr<Result> execute_delete(const Mysqlx::Crud::Delete &m);

    void fetch_capabilities();
    void setup_capability(const std::string &name, const bool value);

    void authenticate(const std::string &user, const std::string &pass, const std::string &schema);
    void authenticate_plain(const std::string &user, const std::string &pass, const std::string &db);
    void authenticate_mysql41(const std::string &user, const std::string &pass, const std::string &db);

    void send_bytes(const std::string &data);

    void set_trace_protocol(bool flag) { m_trace_packets = flag; }
    unsigned long get_received_msg_counter(const std::string &id) const;

  private:
    void perform_close();
    void dispatch_notice(Mysqlx::Notice::Frame *frame);
    Message *recv_message_with_header(int &mid, char (&header_buffer)[5], const std::size_t header_offset);
    void throw_mysqlx_error(const Error &ec);
    ngs::shared_ptr<Result> new_result(bool expect_data);
    void update_received_msg_counter(const Message* msg);
  private:
    std::list<Local_notice_handler> m_local_notice_handlers;
    Mysqlx::Connection::Capabilities m_capabilities;

    Connection m_sync_connection;
    uint64_t m_client_id;
    bool m_trace_packets;
    bool m_closed;
    const bool m_dont_wait_for_disconnect;
    const Internet_protocol m_ip_mode;
    ngs::shared_ptr<Result> m_last_result;
    std::map<std::string, unsigned long> m_received_msg_counters;
  };

  bool parse_mysql_connstring(const std::string &connstring,
                              std::string &protocol, std::string &user, std::string &password,
                              std::string &host, int &port, std::string &sock,
                              std::string &db, int &pwd_found);
} // namespace mysqlx

#endif // _MYSQLX_PROTOCOL_H_
