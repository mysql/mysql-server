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
#include <boost/scoped_ptr.hpp>
#include "mysqlx.h"
#include "mysqlx_connection.h"
#include "mysqlx_crud.h"
#include "mysqlx_row.h"
#include "xpl_error.h"

#include "my_config.h"

#include <boost/bind.hpp>

#ifdef MYSQLXTEST_STANDALONE
#include "mysqlx/auth_mysql41.h"
#else
#include "password_hasher.h"
namespace mysqlx {
  std::string build_mysql41_authentication_response(const std::string &salt_data,
    const std::string &user,
    const std::string &password,
    const std::string &schema)
  {
    std::string password_hash;
    if (password.length())
      password_hash = Password_hasher::get_password_from_salt(Password_hasher::scramble(salt_data.c_str(), password.c_str()));
    std::string data;
    data.append(schema).push_back('\0'); // authz
    data.append(user).push_back('\0'); // authc
    data.append(password_hash); // pass
    return data;
  }
}
#endif

#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
#pragma GCC diagnostic pop
#elif defined _MSC_VER
#pragma warning (pop)
#endif

#include <iostream>
#ifdef WIN32
#pragma warning(push, 0)
#endif
#include <boost/asio/error.hpp>
#ifdef WIN32
#pragma warning(pop)
#endif
#ifndef WIN32
#include <netdb.h>
#include <sys/socket.h>
#endif
#include <string>
#include <iostream>
#include <limits>
#include "compilerutils.h"
#include "ngs_common/xdecimal.h"

#ifdef WIN32
#  define snprintf _snprintf
#  pragma push_macro("ERROR")
#  undef ERROR
#endif

using namespace mysqlx;

bool mysqlx::parse_mysql_connstring(const std::string &connstring,
                                    std::string &protocol, std::string &user, std::string &password,
                                    std::string &host, int &port, std::string &sock,
                                    std::string &db, int &pwd_found)
{
  // format is [protocol://][user[:pass]]@host[:port][/db] or user[:pass]@::socket[/db], like what cmdline utilities use
  pwd_found = 0;
  std::string remaining = connstring;

  std::string::size_type p;
  p = remaining.find("://");
  if (p != std::string::npos)
  {
    protocol = connstring.substr(0, p);
    remaining = remaining.substr(p + 3);
  }

  std::string s = remaining;
  p = remaining.find('/');
  if (p != std::string::npos)
  {
    db = remaining.substr(p + 1);
    s = remaining.substr(0, p);
  }
  p = s.rfind('@');
  std::string user_part;
  std::string server_part = (p == std::string::npos) ? s : s.substr(p + 1);

  if (p == std::string::npos)
  {
    // by default, connect using the current OS username
#ifdef _WIN32
    //XXX find out current username here
#else
    const char *tmp = getenv("USER");
    user_part = tmp ? tmp : "";
#endif
  }
  else
    user_part = s.substr(0, p);

  if ((p = user_part.find(':')) != std::string::npos)
  {
    user = user_part.substr(0, p);
    password = user_part.substr(p + 1);
    pwd_found = 1;
  }
  else
    user = user_part;

  p = server_part.find(':');
  if (p != std::string::npos)
  {
    host = server_part.substr(0, p);
    server_part = server_part.substr(p + 1);
    p = server_part.find(':');
    if (p != std::string::npos)
      sock = server_part.substr(p + 1);
    else
      if (!sscanf(server_part.substr(0, p).c_str(), "%i", &port))
        return false;
  }
  else
    host = server_part;
  return true;
}

using namespace mysqlx;

Error::Error(int err, const std::string &message)
: std::runtime_error(message), _message(message), _error(err)
{
}

Error::~Error() BOOST_NOEXCEPT_OR_NOTHROW
{
}

static void throw_server_error(const Mysqlx::Error &error)
{
  throw Error(error.code(), error.msg());
}

Session::Session(const mysqlx::Ssl_config &ssl_config, const std::size_t timeout)
{
  m_connection.reset(new Connection(ssl_config, timeout));
}

Session::~Session()
{
  m_connection.reset();
}

boost::shared_ptr<Session> mysqlx::openSession(const std::string &uri, const std::string &pass, const mysqlx::Ssl_config &ssl_config,
                                               const bool cap_expired_password, const std::size_t timeout, const bool get_caps)
{
  boost::shared_ptr<Session> session(new Session(ssl_config, timeout));
  session->connection()->connect(uri, pass, cap_expired_password);
  if (get_caps)
    session->connection()->fetch_capabilities();
  return session;
}

boost::shared_ptr<Session> mysqlx::openSession(const std::string &host, int port, const std::string &schema,
                                               const std::string &user, const std::string &pass,
                                               const mysqlx::Ssl_config &ssl_config, const std::size_t timeout,
                                               const std::string &auth_method,
                                               const bool get_caps)
{
  boost::shared_ptr<Session> session(new Session(ssl_config, timeout));
  session->connection()->connect(host, port);
  if (get_caps)
    session->connection()->fetch_capabilities();
  if (auth_method.empty())
    session->connection()->authenticate(user, pass, schema);
  else
  {
    if (auth_method == "PLAIN")
      session->connection()->authenticate_plain(user, pass, schema);
    else if (auth_method == "MYSQL41")
      session->connection()->authenticate_mysql41(user, pass, schema);
    else
      throw Error(CR_INVALID_AUTH_METHOD, "Invalid authentication method " + auth_method);
  }
  return session;
}

Connection::Connection(const Ssl_config &ssl_config, const std::size_t timeout, const bool dont_wait_for_disconnect)
: m_sync_connection(ssl_config.key, ssl_config.ca, ssl_config.ca_path,
                    ssl_config.cert, ssl_config.cipher, ssl_config.tls_version, timeout),
  m_client_id(0),
  m_trace_packets(false), m_closed(true),
  m_dont_wait_for_disconnect(dont_wait_for_disconnect)
{
  if (getenv("MYSQLX_TRACE_CONNECTION"))
    m_trace_packets = true;
}

Connection::~Connection()
{
  try
  {
    close();
  }
  catch (Error &)
  {
    // ignore close errors
  }
}

void Connection::connect(const std::string &uri, const std::string &pass, const bool cap_expired_password)
{
  std::string protocol, host, schema, user, password;
  std::string sock;
  int pwd_found = 0;
  int port = 33060;

  if (!parse_mysql_connstring(uri, protocol, user, password, host, port, sock, schema, pwd_found))
    throw Error(CR_WRONG_HOST_INFO, "Unable to parse connection string");

  if (protocol != "mysqlx" && !protocol.empty())
    throw Error(CR_WRONG_HOST_INFO, "Unsupported protocol "+protocol);

  if (!pass.empty())
    password = pass;

  connect(host, port);

  if (cap_expired_password)
    setup_capability("client.pwd_expire_ok", true);

  authenticate(user, pass.empty() ? password : pass, schema);
}

void Connection::connect(const std::string &host, int port)
{
  struct addrinfo *res_lst, hints, *t_res;
  int gai_errno;
  boost::system::error_code error;
  char port_buf[NI_MAXSERV];

  snprintf(port_buf, NI_MAXSERV, "%d", port);

  memset(&hints, 0, sizeof(hints));
  hints.ai_socktype= SOCK_STREAM;
  hints.ai_protocol= IPPROTO_TCP;
  hints.ai_family= AF_INET;

  gai_errno= getaddrinfo(host.c_str(), port_buf, &hints, &res_lst);
  if (gai_errno != 0)
    throw Error(CR_UNKNOWN_HOST, "No such host is known '" + host + "'");

  for (t_res= res_lst; t_res; t_res= t_res->ai_next)
  {
    error = m_sync_connection.connect((sockaddr_in*)t_res->ai_addr, t_res->ai_addrlen);

    if (!error)
      break;
  }
  freeaddrinfo(res_lst);

  if (error)
  {
    throw Error(CR_CONNECTION_ERROR, error.message()+" connecting to "+host+":"+port_buf);
  }

  m_closed = false;
}

void Connection::authenticate(const std::string &user, const std::string &pass, const std::string &schema)
{
  if (m_sync_connection.supports_ssl())
  {
    setup_capability("tls", true);

    enable_tls();
    authenticate_plain(user, pass, schema);
  }
  else
    authenticate_mysql41(user, pass, schema);
}

void Connection::fetch_capabilities()
{
  send(Mysqlx::Connection::CapabilitiesGet());
  int mid;
  boost::scoped_ptr<Message> message(recv_raw(mid));
  if (mid != Mysqlx::ServerMessages::CONN_CAPABILITIES)
    throw Error(CR_COMMANDS_OUT_OF_SYNC, "Unexpected response received from server");
  m_capabilities = *static_cast<Mysqlx::Connection::Capabilities*>(message.get());
}

void Connection::enable_tls()
{
  boost::system::error_code ec = m_sync_connection.activate_tls();

  if (ec)
  {
    // If ssl activation failed then
    // server and client are in different states
    // lets force disconnect
    set_closed();

    if (boost::system::errc::not_supported == ec.value())
      throw Error(CR_SSL_CONNECTION_ERROR, "SSL not configured");

    throw Error(CR_SSL_CONNECTION_ERROR, ec.message());
  }
}

void Connection::set_closed()
{
  m_closed = true;
}

void Connection::close()
{
  if (!m_closed)
  {
    if (m_last_result)
      m_last_result->buffer();

    send(Mysqlx::Session::Close());
    m_closed = true;

    int mid;
    try
    {
      boost::scoped_ptr<Message> message(recv_raw(mid));
      if (mid != Mysqlx::ServerMessages::OK)
        throw Error(CR_COMMANDS_OUT_OF_SYNC, "Unexpected message received in response to Session.Close");

      perform_close();
    }
    catch (...)
    {
      m_sync_connection.close();
      throw;
    }
  }
}

void Connection::perform_close()
{
  if (m_dont_wait_for_disconnect)
  {
    m_sync_connection.close();
    return;
  }

  int mid;
  boost::scoped_ptr<Message> message(recv_raw(mid));
  std::stringstream s;

  s << "Unexpected message received with id:" << mid << " while waiting for disconnection";

  throw Error(CR_COMMANDS_OUT_OF_SYNC, s.str());
}

boost::shared_ptr<Result> Connection::recv_result()
{
  return new_result(true);
}

boost::shared_ptr<Result> Connection::execute_sql(const std::string &sql)
{
  {
    Mysqlx::Sql::StmtExecute exec;
    exec.set_namespace_("sql");
    exec.set_stmt(sql);
    send(exec);
  }

  return new_result(true);
}

boost::shared_ptr<Result> Connection::execute_stmt(const std::string &ns, const std::string &sql, const std::vector<ArgumentValue> &args)
{
  {
    Mysqlx::Sql::StmtExecute exec;
    exec.set_namespace_(ns);
    exec.set_stmt(sql);

    for (std::vector<ArgumentValue>::const_iterator iter = args.begin();
         iter != args.end(); ++iter)
    {
      Mysqlx::Datatypes::Any *any = exec.mutable_args()->Add();

      any->set_type(Mysqlx::Datatypes::Any::SCALAR);
      switch (iter->type())
      {
        case ArgumentValue::TInteger:
          any->mutable_scalar()->set_type(Mysqlx::Datatypes::Scalar::V_SINT);
          any->mutable_scalar()->set_v_signed_int(*iter);
          break;
        case ArgumentValue::TUInteger:
          any->mutable_scalar()->set_type(Mysqlx::Datatypes::Scalar::V_UINT);
          any->mutable_scalar()->set_v_unsigned_int(*iter);
          break;
        case ArgumentValue::TNull:
          any->mutable_scalar()->set_type(Mysqlx::Datatypes::Scalar::V_NULL);
          break;
        case ArgumentValue::TDouble:
          any->mutable_scalar()->set_type(Mysqlx::Datatypes::Scalar::V_DOUBLE);
          any->mutable_scalar()->set_v_double(*iter);
          break;
        case ArgumentValue::TFloat:
          any->mutable_scalar()->set_type(Mysqlx::Datatypes::Scalar::V_FLOAT);
          any->mutable_scalar()->set_v_float(*iter);
          break;
        case ArgumentValue::TBool:
          any->mutable_scalar()->set_type(Mysqlx::Datatypes::Scalar::V_BOOL);
          any->mutable_scalar()->set_v_bool(*iter);
          break;
        case ArgumentValue::TString:
          any->mutable_scalar()->set_type(Mysqlx::Datatypes::Scalar::V_STRING);
          any->mutable_scalar()->mutable_v_string()->set_value(*iter);
          break;
        case ArgumentValue::TOctets:
          any->mutable_scalar()->set_type(Mysqlx::Datatypes::Scalar::V_OCTETS);
          any->mutable_scalar()->mutable_v_octets()->set_value(*iter);
          break;
      }
    }
    send(exec);
  }

  return new_result(true);
}

boost::shared_ptr<Result> Connection::execute_find(const Mysqlx::Crud::Find &m)
{
  send(m);

  return new_result(true);
}

boost::shared_ptr<Result> Connection::execute_update(const Mysqlx::Crud::Update &m)
{
  send(m);

  return new_result(false);
}

boost::shared_ptr<Result> Connection::execute_insert(const Mysqlx::Crud::Insert &m)
{
  send(m);

  return new_result(false);
}

boost::shared_ptr<Result> Connection::execute_delete(const Mysqlx::Crud::Delete &m)
{
  send(m);

  return new_result(false);
}

void Connection::setup_capability(const std::string &name, const bool value)
{
  Mysqlx::Connection::CapabilitiesSet capSet;
  Mysqlx::Connection::Capability     *cap = capSet.mutable_capabilities()->add_capabilities();
  ::Mysqlx::Datatypes::Scalar        *scalar = cap->mutable_value()->mutable_scalar();

  cap->set_name(name);
  cap->mutable_value()->set_type(Mysqlx::Datatypes::Any_Type_SCALAR);
  scalar->set_type(Mysqlx::Datatypes::Scalar_Type_V_BOOL);
  scalar->set_v_bool(value);
  send(capSet);

  if (m_last_result)
    m_last_result->buffer();

  int mid;
  boost::scoped_ptr<Message> msg(recv_raw(mid));

  if (Mysqlx::ServerMessages::ERROR == mid)
    throw_server_error(*(Mysqlx::Error*)msg.get());
  if (Mysqlx::ServerMessages::OK != mid)
  {
    if (getenv("MYSQLX_DEBUG"))
    {
      std::string out;
      google::protobuf::TextFormat::PrintToString(*msg, &out);
      std::cout << out << "\n";
    }
    throw Error(CR_MALFORMED_PACKET, "Unexpected message received from server during handshake");
  }
}

void Connection::authenticate_mysql41(const std::string &user, const std::string &pass, const std::string &db)
{
  {
    Mysqlx::Session::AuthenticateStart auth;

    auth.set_mech_name("MYSQL41");

    send(Mysqlx::ClientMessages::SESS_AUTHENTICATE_START, auth);
  }

  {
    int mid;
    boost::scoped_ptr<Message> message(recv_raw(mid));
    switch (mid)
    {
      case Mysqlx::ServerMessages::SESS_AUTHENTICATE_CONTINUE:
      {
        Mysqlx::Session::AuthenticateContinue &auth_continue = *static_cast<Mysqlx::Session::AuthenticateContinue*>(message.get());

        std::string data;

        if (!auth_continue.has_auth_data())
          throw Error(CR_MALFORMED_PACKET, "Missing authentication data");

        std::string password_hash;

        Mysqlx::Session::AuthenticateContinue auth_continue_response;

#ifdef MYSQLXTEST_STANDALONE
        auth_continue_response.set_auth_data(build_mysql41_authentication_response(auth_continue.auth_data(), user, pass, db));
#else
        if (pass.length())
        {
          password_hash = Password_hasher::scramble(auth_continue.auth_data().c_str(), pass.c_str());
          password_hash = Password_hasher::get_password_from_salt(password_hash);
        }

        data.append(db).push_back('\0'); // authz
        data.append(user).push_back('\0'); // authc
        data.append(password_hash); // pass
        auth_continue_response.set_auth_data(data);
#endif

        send(Mysqlx::ClientMessages::SESS_AUTHENTICATE_CONTINUE, auth_continue_response);
      }
      break;

      case Mysqlx::ServerMessages::NOTICE:
        dispatch_notice(static_cast<Mysqlx::Notice::Frame*>(message.get()));
        break;

      case Mysqlx::ServerMessages::ERROR:
        throw_server_error(*static_cast<Mysqlx::Error*>(message.get()));

      default:
        throw Error(CR_MALFORMED_PACKET, "Unexpected message received from server during authentication");
        break;
    }
  }

  bool done = false;
  while (!done)
  {
    int mid;
    boost::scoped_ptr<Message> message(recv_raw(mid));
    switch (mid)
    {
      case Mysqlx::ServerMessages::SESS_AUTHENTICATE_OK:
        done = true;
        break;

      case Mysqlx::ServerMessages::ERROR:
        throw_server_error(*static_cast<Mysqlx::Error*>(message.get()));

      case Mysqlx::ServerMessages::NOTICE:
        dispatch_notice(static_cast<Mysqlx::Notice::Frame*>(message.get()));
        break;

      default:
        throw Error(CR_MALFORMED_PACKET, "Unexpected message received from server during authentication");
        break;
    }
  }
}

void Connection::authenticate_plain(const std::string &user, const std::string &pass, const std::string &db)
{
  {
    Mysqlx::Session::AuthenticateStart auth;

    auth.set_mech_name("PLAIN");
    std::string data;

    data.append(db).push_back('\0'); // authz
    data.append(user).push_back('\0'); // authc
    data.append(pass); // pass

    auth.set_auth_data(data);
    send(Mysqlx::ClientMessages::SESS_AUTHENTICATE_START, auth);
  }

  bool done = false;
  while (!done)
  {
    int mid;
    boost::scoped_ptr<Message> message(recv_raw(mid));
    switch (mid)
    {
      case Mysqlx::ServerMessages::SESS_AUTHENTICATE_OK:
        done = true;
        break;

      case Mysqlx::ServerMessages::ERROR:
        throw_server_error(*static_cast<Mysqlx::Error*>(message.get()));

      case Mysqlx::ServerMessages::NOTICE:
        dispatch_notice(static_cast<Mysqlx::Notice::Frame*>(message.get()));
        break;

      default:
        throw Error(CR_MALFORMED_PACKET, "Unexpected message received from server during authentication");
        break;
    }
  }
}

void Connection::send_bytes(const std::string &data)
{
  boost::system::error_code error = m_sync_connection.write(data.data(), data.size());
  throw_mysqlx_error(error);
}

void Connection::send(int mid, const Message &msg)
{
  boost::system::error_code error;

  union
  {
    uint8_t buf[5];                        // Must be properly aligned
    longlong dummy;
  };
  /*
    Use dummy, otherwise g++ 4.4 reports: unused variable 'dummy'
    MY_ATTRIBUTE((unused)) did not work, so we must use it.
  */
  dummy= 0;

  uint32_t *buf_ptr = (uint32_t *)buf;
  *buf_ptr = msg.ByteSize() + 1;
#ifdef WORDS_BIGENDIAN
  std::swap(buf[0], buf[3]);
  std::swap(buf[1], buf[2]);
#endif
  buf[4] = mid;

  if (m_trace_packets)
  {
    std::string out;
    google::protobuf::TextFormat::Printer p;
    p.SetInitialIndentLevel(1);
    p.PrintToString(msg, &out);
    std::cout << ">>>> SEND " << msg.ByteSize()+1 << " " << msg.GetDescriptor()->full_name() << " {\n" << out << "}\n";
  }

  error = m_sync_connection.write(buf, 5);
  if (!error)
  {
    std::string mbuf;
    msg.SerializeToString(&mbuf);

    if (0 != mbuf.length())
      error = m_sync_connection.write(mbuf.data(), mbuf.length());
  }

  throw_mysqlx_error(error);
}

void Connection::push_local_notice_handler(Local_notice_handler handler)
{
  m_local_notice_handlers.push_back(handler);
}

void Connection::pop_local_notice_handler()
{
  m_local_notice_handlers.pop_back();
}

void Connection::dispatch_notice(Mysqlx::Notice::Frame *frame)
{
  if (frame->scope() == Mysqlx::Notice::Frame::LOCAL)
  {
    for (std::list<Local_notice_handler>::iterator iter = m_local_notice_handlers.begin();
         iter != m_local_notice_handlers.end(); ++iter)
      if ((*iter)(frame->type(), frame->payload())) // handler returns true if the notice was handled
        return;

    {
      if (frame->type() == 3)
      {
        Mysqlx::Notice::SessionStateChanged change;
        change.ParseFromString(frame->payload());
        if (!change.IsInitialized())
          std::cerr << "Invalid notice received from server " << change.InitializationErrorString() << "\n";
        else
        {
          if (change.param() == Mysqlx::Notice::SessionStateChanged::ACCOUNT_EXPIRED)
          {
            std::cout << "NOTICE: Account password expired\n";
            return;
          }
          else if (change.param() == Mysqlx::Notice::SessionStateChanged::CLIENT_ID_ASSIGNED)
          {
            if (!change.has_value() || change.value().type() != Mysqlx::Datatypes::Scalar::V_UINT)
              std::cerr << "Invalid notice received from server. Client_id is of the wrong type\n";
            else
              m_client_id = change.value().v_unsigned_int();
            return;
          }
        }
      }
      std::cout << "Unhandled local notice\n";
    }
  }
  else
  {
    std::cout << "Unhandled global notice\n";
  }
}

Message *Connection::recv_next(int &mid)
{
  for (;;)
  {
    Message *msg = recv_raw(mid);
    if (mid != Mysqlx::ServerMessages::NOTICE)
      return msg;

    dispatch_notice(static_cast<Mysqlx::Notice::Frame*>(msg));
    delete msg;
  }
}

Message *Connection::recv_raw_with_deadline(int &mid, const int deadline_milliseconds)
{
  char header_buffer[5];
  std::size_t data = sizeof(header_buffer);
  boost::system::error_code error = m_sync_connection.read_with_timeout(header_buffer, data, deadline_milliseconds);

  if (0 == data)
  {
    m_closed = true;
    return NULL;
  }

  throw_mysqlx_error(error);

  return recv_message_with_header(mid, header_buffer, sizeof(header_buffer));
}

Message *Connection::recv_payload(const int mid, const std::size_t msglen)
{
  boost::system::error_code error;
  Message* ret_val = NULL;
  char *mbuf = new char[msglen];

  if (0 < msglen)
    error = m_sync_connection.read(mbuf, msglen);

  if (!error)
  {
    switch (mid)
    {
      case Mysqlx::ServerMessages::OK:
        ret_val = new Mysqlx::Ok();
        break;
      case Mysqlx::ServerMessages::ERROR:
        ret_val = new Mysqlx::Error();
        break;
      case Mysqlx::ServerMessages::NOTICE:
        ret_val = new Mysqlx::Notice::Frame();
        break;
      case Mysqlx::ServerMessages::CONN_CAPABILITIES:
        ret_val = new Mysqlx::Connection::Capabilities();
        break;
      case Mysqlx::ServerMessages::SESS_AUTHENTICATE_CONTINUE:
        ret_val = new Mysqlx::Session::AuthenticateContinue();
        break;
      case Mysqlx::ServerMessages::SESS_AUTHENTICATE_OK:
        ret_val = new Mysqlx::Session::AuthenticateOk();
        break;
      case Mysqlx::ServerMessages::RESULTSET_COLUMN_META_DATA:
        ret_val = new Mysqlx::Resultset::ColumnMetaData();
        break;
      case Mysqlx::ServerMessages::RESULTSET_ROW:
        ret_val = new Mysqlx::Resultset::Row();
        break;
      case Mysqlx::ServerMessages::RESULTSET_FETCH_DONE:
        ret_val = new Mysqlx::Resultset::FetchDone();
        break;
      case Mysqlx::ServerMessages::RESULTSET_FETCH_DONE_MORE_RESULTSETS:
        ret_val = new Mysqlx::Resultset::FetchDoneMoreResultsets();
        break;
      case Mysqlx::ServerMessages::SQL_STMT_EXECUTE_OK:
        ret_val = new Mysqlx::Sql::StmtExecuteOk();
        break;
    }

    if (!ret_val)
    {
      delete[] mbuf;
      std::stringstream ss;
      ss << "Unknown message received from server ";
      ss << mid;
      throw Error(CR_MALFORMED_PACKET, ss.str());
    }

    // Parses the received message
    ret_val->ParseFromString(std::string(mbuf, msglen));

    if (m_trace_packets)
    {
      std::string out;
      google::protobuf::TextFormat::Printer p;
      p.SetInitialIndentLevel(1);
      p.PrintToString(*ret_val, &out);
      std::cout << "<<<< RECEIVE " << msglen << " " << ret_val->GetDescriptor()->full_name() << " {\n" << out << "}\n";
    }

    if (!ret_val->IsInitialized())
    {
      delete[] mbuf;
      delete ret_val;
      std::string err("Message is not properly initialized: ");
      err += ret_val->InitializationErrorString();
      throw Error(CR_MALFORMED_PACKET, err);
    }
  }
  else
  {
    delete[] mbuf;
    throw_mysqlx_error(error);
  }

  delete[] mbuf;

  return ret_val;
}

Message *Connection::recv_raw(int &mid)
{
  union
  {
    char buf[5];                                // Must be properly aligned
    longlong dummy;
  };

  /*
    Use dummy, otherwise g++ 4.4 reports: unused variable 'dummy'
    MY_ATTRIBUTE((unused)) did not work, so we must use it.
  */
  dummy= 0;
  mid = 0;

  return recv_message_with_header(mid, buf, 0);
}

Message *Connection::recv_message_with_header(int &mid, char (&header_buffer)[5], const std::size_t header_offset)
{
  Message* ret_val = NULL;
  boost::system::error_code error;

  error = m_sync_connection.read(header_buffer + header_offset, 5 - header_offset);

#ifdef WORDS_BIGENDIAN
  std::swap(header_buffer[0], header_buffer[3]);
  std::swap(header_buffer[1], header_buffer[2]);
#endif

  if (!error)
  {
  uint32_t msglen = *(uint32_t*)header_buffer - 1;
  mid = header_buffer[4];

  ret_val = recv_payload(mid, msglen);
  }
  else
  {
    throw_mysqlx_error(error);
  }

  return ret_val;
}

void Connection::throw_mysqlx_error(const boost::system::error_code &error)
{
  if (!error)
    return;

  switch (error.value())
  {
    // OSX return this undocumented error in case of kernel race-conndition
    // lets ignore it and next call to any io function should return correct
    // error
    case boost::system::errc::wrong_protocol_type:
	return;
    case boost::asio::error::eof:
    case boost::asio::error::connection_reset:
    case boost::asio::error::connection_aborted:
      throw Error(CR_SERVER_GONE_ERROR, "MySQL server has gone away");

    case boost::asio::error::broken_pipe:
      throw Error(CR_BROKEN_PIPE, "MySQL server has gone away");

    default:
      throw Error(CR_UNKNOWN_ERROR, error.message());
  }
}

boost::shared_ptr<Result> Connection::new_result(bool expect_data)
{
  if (m_last_result)
    m_last_result->buffer();

  m_last_result.reset(new Result(shared_from_this(), expect_data));

  return m_last_result;
}

boost::shared_ptr<Result> Connection::new_empty_result()
{
  boost::shared_ptr<Result> empty_result(new Result(shared_from_this(), false, false));

  return empty_result;
}

boost::shared_ptr<Schema> Session::getSchema(const std::string &name)
{
  std::map<std::string, boost::shared_ptr<Schema> >::const_iterator iter = m_schemas.find(name);
  if (iter != m_schemas.end())
    return iter->second;

  return m_schemas[name] = boost::shared_ptr<Schema>(new Schema(shared_from_this(), name));
}

boost::shared_ptr<Result> Session::executeSql(const std::string &sql)
{
  return m_connection->execute_sql(sql);
}

boost::shared_ptr<Result> Session::executeStmt(const std::string &ns, const std::string &stmt,
                             const std::vector<ArgumentValue> &args)
{
  return m_connection->execute_stmt(ns, stmt, args);
}

void Session::close()
{
  if (m_connection)
  {
    m_connection->close();

    m_connection.reset();
  }
}

Document::Document()
{
  m_expression = false;
}

Document::Document(const std::string &doc, bool expression, const std::string& id)
  : m_data(new std::string(doc)), m_expression(expression), m_id(id)
{
}

Document::Document(const Document &doc)
  : m_data(doc.m_data), m_expression(doc.m_expression), m_id(doc.m_id)
{
}

void Document::reset(const std::string &doc, bool expression, const std::string &id)
{
  m_data.reset(new std::string(doc));
  m_expression = expression;
  m_id = id;
}

Result::Result(boost::shared_ptr<Connection>owner, bool expect_data, bool expect_ok)
: current_message(NULL), m_owner(owner), m_last_insert_id(-1), m_affected_rows(-1),
  m_result_index(0), m_state(expect_data ? ReadMetadataI : expect_ok ? ReadStmtOkI : ReadDone), m_buffered(false), m_buffering(false), m_has_doc_ids(false)
{
}

Result::Result()
  : current_message(NULL), m_state(ReadDone), m_buffered(false), m_buffering(false)
{
}

Result::~Result()
{
  // flush the resultset from the pipe
  while (m_state != ReadError && m_state != ReadDone)
    nextDataSet();

  delete current_message;
}

boost::shared_ptr<std::vector<ColumnMetadata> > Result::columnMetadata()
{
  // If cached, works with the cache data
  if (m_buffered)
    return m_current_result->columnMetadata();
  else
  {
  if (m_state == ReadMetadataI)
    read_metadata();
  }
  return m_columns;
}

bool Result::ready()
{
  // if we've read something (ie not on initial state), then we're ready
  return m_state != ReadMetadataI && m_state != ReadStmtOkI;
}

void Result::wait()
{
  if (m_state == ReadMetadataI)
    read_metadata();
  if (m_state == ReadStmtOkI)
    read_stmt_ok();
}

void Result::mark_error()
{
  m_state = ReadError;
}

bool Result::handle_notice(int32_t type, const std::string &data)
{
  switch (type)
  {
    case 1: // warning
    {
      Mysqlx::Notice::Warning warning;
      warning.ParseFromString(data);
      if (!warning.IsInitialized())
        std::cerr << "Invalid notice received from server " << warning.InitializationErrorString() << "\n";
      else
      {
        Warning w;
        w.code = warning.code();
        w.text = warning.msg();
        w.is_note = warning.level() == Mysqlx::Notice::Warning::NOTE;
        m_warnings.push_back(w);
      }
      return true;
    }

    case 2: // session variable changed
      break;

    case 3: //session state changed
    {
      Mysqlx::Notice::SessionStateChanged change;
      change.ParseFromString(data);
      if (!change.IsInitialized())
        std::cerr << "Invalid notice received from server " << change.InitializationErrorString() << "\n";
      else
      {
        switch (change.param())
        {
          case Mysqlx::Notice::SessionStateChanged::GENERATED_INSERT_ID:
            if (change.value().type() == Mysqlx::Datatypes::Scalar::V_UINT)
              m_last_insert_id = change.value().v_unsigned_int();
            else
              std::cerr << "Invalid notice value received from server: " << data << "\n";
            break;

          case Mysqlx::Notice::SessionStateChanged::ROWS_AFFECTED:
            if (change.value().type() == Mysqlx::Datatypes::Scalar::V_UINT)
              m_affected_rows = change.value().v_unsigned_int();
            else
              std::cerr << "Invalid notice value received from server: " << data << "\n";
            break;

          case Mysqlx::Notice::SessionStateChanged::PRODUCED_MESSAGE:
            if (change.value().type() == Mysqlx::Datatypes::Scalar::V_STRING)
              m_info_message = change.value().v_string().value();
            else
              std::cerr << "Invalid notice value received from server: " << data << "\n";
            break;

          default:
            return false;
        }
      }
      return true;
    }
    default:
      std::cerr << "Unexpected notice type received " << type << "\n";
      return false;
  }
  return false;
}

int Result::get_message_id()
{
  if (NULL != current_message)
  {
    return current_message_id;
  }

  boost::shared_ptr<Connection>owner = m_owner.lock();

  if (owner)
  {
    owner->push_local_notice_handler(boost::bind(&Result::handle_notice, this, _1, _2));

    try
    {
        current_message = owner->recv_next(current_message_id);
    }
    catch (...)
    {
      m_state = ReadError;
        owner->pop_local_notice_handler();
      throw;
    }

    owner->pop_local_notice_handler();
  }

  // error messages that can be received in any state
  if (current_message_id == Mysqlx::ServerMessages::ERROR)
  {
    m_state = ReadError;
    throw_server_error(static_cast<const Mysqlx::Error&>(*current_message));
  }

  switch (m_state)
  {
    case ReadMetadataI:
    {
      switch (current_message_id)
      {
        case Mysqlx::ServerMessages::SQL_STMT_EXECUTE_OK:
          m_state = ReadDone;
          return current_message_id;

        case Mysqlx::ServerMessages::RESULTSET_COLUMN_META_DATA:
          m_state = ReadMetadata;
          return current_message_id;
      }
      break;
    }
    case ReadMetadata:
    {
      // while reading metadata, we can either get more metadata
      // start getting rows (which also signals end of metadata)
      // or EORows, which signals end of metadata AND empty resultset
      switch (current_message_id)
      {
      case Mysqlx::ServerMessages::RESULTSET_COLUMN_META_DATA:
          m_state = ReadMetadata;
          return current_message_id;

        case Mysqlx::ServerMessages::RESULTSET_ROW:
          m_state = ReadRows;
          return current_message_id;

        case Mysqlx::ServerMessages::RESULTSET_FETCH_DONE:
          // empty resultset
          m_state = ReadStmtOk;
          return current_message_id;
      }
      break;
    }
    case ReadRows:
    {
      switch (current_message_id)
      {
        case Mysqlx::ServerMessages::RESULTSET_ROW:
          return current_message_id;

        case Mysqlx::ServerMessages::RESULTSET_FETCH_DONE:
          m_state = ReadStmtOk;
          return current_message_id;

        case Mysqlx::ServerMessages::RESULTSET_FETCH_DONE_MORE_RESULTSETS:
          m_state = ReadMetadata;
          return current_message_id;
      }
      break;
    }
    case ReadStmtOkI:
    case ReadStmtOk:
    {
      switch (current_message_id)
      {
        case Mysqlx::ServerMessages::SQL_STMT_EXECUTE_OK:
          m_state = ReadDone;
          return current_message_id;
      }
      break;
    }
    case ReadError:
    case ReadDone:
      // not supposed to reach here
      throw std::logic_error("attempt to read data at wrong time");
  }

  if (getenv("MYSQLX_DEBUG"))
  {
    std::string out;
    google::protobuf::TextFormat::PrintToString(*current_message, &out);
    std::cout << out << "\n";
  }
  m_state = ReadError;
  throw Error(CR_COMMANDS_OUT_OF_SYNC, "Unexpected message received from server reading results");
}

mysqlx::Message* Result::pop_message()
{
  mysqlx::Message *result = current_message;

  current_message = NULL;

  return result;
}

std::string Result::lastDocumentId()
{
  // Last document id is only available on collection add operations
  // and only if a single document is added (MY-139 Spec, Req 4, 6)
  if (!m_has_doc_ids || m_last_document_ids.size() != 1)
    throw std::logic_error("document id is not available.");

  return m_last_document_ids.at(0);
}

const std::vector<std::string>& Result::lastDocumentIds()
{
  // Last document ids are available on any collection add operation (MY-139 Spec, Req 1-5)
  if (!m_has_doc_ids)
    throw std::logic_error("document ids are not available.");

  return m_last_document_ids;
}

void Result::setLastDocumentIDs(const std::vector<std::string>& document_ids)
{
  m_has_doc_ids = true;
  m_last_document_ids.reserve(document_ids.size());
  std::copy(document_ids.begin(), document_ids.end(), std::back_inserter(m_last_document_ids));
}

static ColumnMetadata unwrap_column_metadata(const Mysqlx::Resultset::ColumnMetaData &column_data)
{
  ColumnMetadata column;

  switch (column_data.type())
  {
    case Mysqlx::Resultset::ColumnMetaData::SINT:
      column.type = mysqlx::SINT;
      break;
    case Mysqlx::Resultset::ColumnMetaData::UINT:
      column.type = mysqlx::UINT;
      break;
    case Mysqlx::Resultset::ColumnMetaData::DOUBLE:
      column.type = mysqlx::DOUBLE;
      break;
    case Mysqlx::Resultset::ColumnMetaData::FLOAT:
      column.type = mysqlx::FLOAT;
      break;
    case Mysqlx::Resultset::ColumnMetaData::BYTES:
      column.type = mysqlx::BYTES;
      break;
    case Mysqlx::Resultset::ColumnMetaData::TIME:
      column.type = mysqlx::TIME;
      break;
    case Mysqlx::Resultset::ColumnMetaData::DATETIME:
      column.type = mysqlx::DATETIME;
      break;
    case Mysqlx::Resultset::ColumnMetaData::SET:
      column.type = mysqlx::SET;
      break;
    case Mysqlx::Resultset::ColumnMetaData::ENUM:
      column.type = mysqlx::ENUM;
      break;
    case Mysqlx::Resultset::ColumnMetaData::BIT:
      column.type = mysqlx::BIT;
      break;
    case Mysqlx::Resultset::ColumnMetaData::DECIMAL:
      column.type = mysqlx::DECIMAL;
      break;
  }
  column.name = column_data.name();
  column.original_name = column_data.original_name();

  column.table = column_data.table();
  column.original_table = column_data.original_table();

  column.schema = column_data.schema();
  column.catalog = column_data.catalog();

  column.collation = column_data.has_collation() ? column_data.collation() : 0;

  column.fractional_digits = column_data.fractional_digits();

  column.length = column_data.length();

  column.flags = column_data.flags();
  column.content_type = column_data.content_type();
  return column;
}

void Result::read_metadata()
{
  if (m_state != ReadMetadata && m_state != ReadMetadataI)
    throw std::logic_error("read_metadata() called at wrong time");

  // msgs we can get in this state:
  // CURSOR_OK
  // META_DATA

  int msgid = -1;
  m_columns.reset(new std::vector<ColumnMetadata>());
  while (m_state == ReadMetadata || m_state == ReadMetadataI)
  {
    if (-1 != msgid)
    {
      delete pop_message();
    }

    msgid = get_message_id();

    if (msgid == Mysqlx::ServerMessages::RESULTSET_COLUMN_META_DATA)
    {
      msgid = -1;
      boost::scoped_ptr<Mysqlx::Resultset::ColumnMetaData> column_data(static_cast<Mysqlx::Resultset::ColumnMetaData*>(pop_message()));

      m_columns->push_back(unwrap_column_metadata(*column_data));
    }
  }
}

boost::shared_ptr<Row> Result::read_row()
{
  boost::shared_ptr<Row> ret_val;

  if (m_state != ReadRows)
    throw std::logic_error("read_row() called at wrong time");

  // msgs we can get in this state:
  // RESULTSET_ROW
  // RESULTSET_FETCH_DONE
  // RESULTSET_FETCH_DONE_MORE_RESULTSETS
  int mid = get_message_id();

  if (mid == Mysqlx::ServerMessages::RESULTSET_ROW)
  {
    ret_val.reset(new Row(m_columns, static_cast<Mysqlx::Resultset::Row*>(pop_message())));

    // If caching adds it to the cache instead
    if (m_buffering)
      m_current_result->add_row(ret_val);
  }

  return ret_val;
}

void Result::read_stmt_ok()
{
  if (m_state != ReadStmtOk && m_state != ReadStmtOkI)
    throw std::logic_error("read_stmt_ok() called at wrong time");

  // msgs we can get in this state:
  // STMT_EXEC_OK

  if (Mysqlx::ServerMessages::RESULTSET_FETCH_DONE == get_message_id())
    delete pop_message();

  if (Mysqlx::ServerMessages::SQL_STMT_EXECUTE_OK != get_message_id())
    throw std::runtime_error("Unexpected message id");

  boost::scoped_ptr<mysqlx::Message> msg(pop_message());
}

bool Result::rewind()
{
  bool ret_val = false;
  if (m_buffered)
  {
    for (m_result_index = 0; m_result_index < m_result_cache.size(); m_result_index++)
      m_result_cache[m_result_index]->rewind();

    m_result_index = 0;
    nextDataSet();

    ret_val = true;
  }

  return ret_val;
}

bool Result::tell(size_t &dataset, size_t&record)
{
  bool ret_val = false;

  if (m_buffered && m_current_result)
  {
    dataset = m_result_index;
    m_current_result->tell(record);
    ret_val = true;
  }

  return ret_val;
}

bool Result::seek(size_t dataset, size_t record)
{
  bool ret_val = false;

  if (m_buffered)
  {
    rewind();

    while (dataset < m_result_index)
      nextDataSet();

    m_current_result->seek(record);

    ret_val = true;
  }

  return ret_val;
}

bool Result::has_data()
{
  bool ret_val = false;

  if (m_buffered)
    ret_val = m_current_result->columnMetadata() && m_current_result->columnMetadata()->size() > 0;
  else
    ret_val = m_columns && m_columns->size() > 0;

  return ret_val;
}

bool Result::nextDataSet()
{
  if (m_buffered)
  {
    if (m_result_index < m_result_cache.size())
      m_current_result = m_result_cache[m_result_index++];
    else
      m_current_result.reset();

    return m_current_result ? true : false;
  }
  else
  {
  // flush left over rows
  while (m_state == ReadRows)
    read_row();

  if (m_state == ReadMetadata)
  {
    read_metadata();
    if (m_state == ReadRows)
      {
        // If caching adds this new resultset to the cache
        if (m_buffering)
        {
          m_current_result.reset(new ResultData(m_columns));
          m_result_cache.push_back(m_current_result);
        }
      return true;
  }
    }
  if (m_state == ReadStmtOk)
    read_stmt_ok();
  }
  return false;
}

boost::shared_ptr<Row> Result::next()
{
  boost::shared_ptr<Row> ret_val;

  if (m_buffered)
    ret_val = m_current_result->next();
  else
{
  if (!ready())
    wait();

  if (m_state == ReadStmtOk)
    read_stmt_ok();

    if (m_state != ReadDone)
    {
      ret_val = read_row();

  if (m_state == ReadStmtOk)
    read_stmt_ok();
    }
  }

  return ret_val;
}

// Flush will read all the messages from the IO
// If caching is enabled the data will be cached, if not
// it will be just discarded
void Result::flush()
{
  // Flushes the leftover data only if it was not previously cached
  wait();
  while (nextDataSet());
}

Result& Result::buffer()
{
  if (!ready())
  wait();

  // The buffer makes sense ONLY if there's something else
  // to be buffered
  if (m_state != ReadDone)
  {
    m_buffering = true;

    // This will enable data caching
    m_current_result.reset(new ResultData(m_columns));
    m_result_cache.push_back(m_current_result);

    // This will actually cache the data
    while (nextDataSet())
      ;

    m_buffering = false;
    m_buffered = true;

    m_result_index = 1;
  }

  return *this;
}

ResultData::ResultData(boost::shared_ptr<std::vector<ColumnMetadata> > columns) :
m_columns(columns), m_row_index(0)
{
}

void ResultData::add_row(boost::shared_ptr<Row> row)
{
  m_rows.push_back(row);
}

boost::shared_ptr<Row> ResultData::next()
{
  boost::shared_ptr<Row> ret_val;

  if (m_row_index < m_rows.size())
    ret_val = m_rows[m_row_index++];

  return ret_val;
}

void ResultData::rewind()
{
  m_row_index = 0;
}

void ResultData::tell(size_t &record)
{
  record = m_row_index;
}

void ResultData::seek(size_t record)
{
  m_row_index = m_rows.size();

  if (record < m_row_index)
    m_row_index = record;
}

Row::Row(boost::shared_ptr<std::vector<ColumnMetadata> > columns, Mysqlx::Resultset::Row *data)
: m_columns(columns), m_data(data)
{
}

Row::~Row()
{
  delete m_data;
}

void Row::check_field(int field, FieldType type) const
{
  if (field < 0 || field >= (int)m_columns->size())
    throw std::range_error("invalid field index");

  if (m_columns->at(field).type != type)
    throw std::range_error("invalid field type");
}

bool Row::isNullField(int field) const
{
  if (field < 0 || field >= (int)m_columns->size())
    throw std::range_error("invalid field index");

  if (m_data->field(field).empty())
    return true;
  return false;
}

int32_t Row::sIntField(int field) const
{
  int64_t t = sInt64Field(field);
  if (t > std::numeric_limits<int32_t>::max() || t < std::numeric_limits<int32_t>::min())
    throw std::invalid_argument("field of wrong type");

  return (int32_t)t;
}

uint32_t Row::uIntField(int field) const
{
  uint64_t t = uInt64Field(field);
  if (t > std::numeric_limits<uint32_t>::max())
    throw std::invalid_argument("field of wrong type");

  return (uint32_t)t;
}

int64_t Row::sInt64Field(int field) const
{
  check_field(field, SINT);
  const std::string& field_val = m_data->field(field);

  return Row_decoder::s64_from_buffer(field_val);
}

uint64_t Row::uInt64Field(int field) const
{
  check_field(field, UINT);
  const std::string& field_val = m_data->field(field);

  return Row_decoder::u64_from_buffer(field_val);
}

uint64_t Row::bitField(int field) const
{
  check_field(field, BIT);
  const std::string& field_val = m_data->field(field);

  return Row_decoder::u64_from_buffer(field_val);
}

std::string Row::stringField(int field) const
{
  size_t length;
  check_field(field, BYTES);

  const std::string& field_val = m_data->field(field);

  const char* res = Row_decoder::string_from_buffer(field_val, length);
  return std::string(res, length);
}

std::string Row::decimalField(int field) const
{
  check_field(field, DECIMAL);

  const std::string& field_val = m_data->field(field);

  mysqlx::Decimal decimal = Row_decoder::decimal_from_buffer(field_val);

  return std::string(decimal.str());
}

std::string Row::setFieldStr(int field) const
{
  check_field(field, SET);

  const std::string& field_val = m_data->field(field);

  return Row_decoder::set_from_buffer_as_str(field_val);
}

std::set<std::string> Row::setField(int field) const
{
  std::set<std::string> result;
  check_field(field, SET);

  const std::string& field_val = m_data->field(field);
  Row_decoder::set_from_buffer(field_val, result);

  return result;
}

std::string Row::enumField(int field) const
{
  size_t length;
  check_field(field, ENUM);

  const std::string& field_val = m_data->field(field);

  const char* res = Row_decoder::string_from_buffer(field_val, length);
  return std::string(res, length);
}

const char *Row::stringField(int field, size_t &rlength) const
{
  check_field(field, BYTES);

  const std::string& field_val = m_data->field(field);

  return Row_decoder::string_from_buffer(field_val, rlength);
}

float Row::floatField(int field) const
{
  check_field(field, FLOAT);

  const std::string& field_val = m_data->field(field);

  return Row_decoder::float_from_buffer(field_val);
}

double Row::doubleField(int field) const
{
  check_field(field, DOUBLE);

  const std::string& field_val = m_data->field(field);

  return Row_decoder::double_from_buffer(field_val);
}

DateTime Row::dateTimeField(int field) const
{
  check_field(field, DATETIME);

  const std::string& field_val = m_data->field(field);

  return Row_decoder::datetime_from_buffer(field_val);
}

Time Row::timeField(int field) const
{
  check_field(field, TIME);

  const std::string& field_val = m_data->field(field);

  return Row_decoder::time_from_buffer(field_val);
}

int Row::numFields() const
{
  return m_data->field_size();
}

#ifdef WIN32
#  pragma pop_macro("ERROR")
#endif
