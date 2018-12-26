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
#include "mysqlx_protocol.h"
#include "mysqlx_resultset.h"
#include "mysqlx_row.h"
#include "mysqlx_error.h"
#include "mysqlx_version.h"

#include "my_config.h"
#include "ngs_common/bind.h"

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
#ifndef WIN32
#include <netdb.h>
#include <sys/socket.h>
#endif // WIN32
#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif // HAVE_SYS_UN_H
#include <string>
#include <iostream>
#include <limits>

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
    char tmp_buffer[1024];
    char *tmp = tmp_buffer;
    DWORD tmp_size = sizeof(tmp_buffer);

    if (!GetUserNameA(tmp_buffer, &tmp_size))
    {
      tmp = NULL;
    }
#else
    const char *tmp = getenv("USER");
#endif
    user_part = tmp ? tmp : "";
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

static void throw_server_error(const Mysqlx::Error &error)
{
  throw Error(error.code(), error.msg());
}


XProtocol::XProtocol(const Ssl_config &ssl_config,
                     const std::size_t timeout,
                     const bool dont_wait_for_disconnect,
                     const Internet_protocol ip_mode)
: m_sync_connection(ssl_config.key, ssl_config.ca, ssl_config.ca_path,
                    ssl_config.cert, ssl_config.cipher, ssl_config.tls_version, timeout),
  m_client_id(0),
  m_trace_packets(false), m_closed(true),
  m_dont_wait_for_disconnect(dont_wait_for_disconnect),
  m_ip_mode(ip_mode)
{
  if (getenv("MYSQLX_TRACE_CONNECTION"))
    m_trace_packets = true;
}

XProtocol::~XProtocol()
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

void XProtocol::connect(const std::string &uri, const std::string &pass, const bool cap_expired_password)
{
  std::string protocol, host, schema, user, password;
  std::string sock;
  int pwd_found = 0;
  int port = MYSQLX_TCP_PORT;

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

void XProtocol::connect(const std::string &host, int port)
{
  struct addrinfo *res_lst, hints, *t_res;
  int gai_errno;
  Error error;
  char port_buf[NI_MAXSERV];

  snprintf(port_buf, NI_MAXSERV, "%d", port);

  memset(&hints, 0, sizeof(hints));
  hints.ai_socktype= SOCK_STREAM;
  hints.ai_protocol= IPPROTO_TCP;
  hints.ai_family= AF_UNSPEC;

  if (IPv6 == m_ip_mode)
    hints.ai_family = AF_INET6;
  else if (IPv4 == m_ip_mode)
    hints.ai_family = AF_INET;

  gai_errno= getaddrinfo(host.c_str(), port_buf, &hints, &res_lst);
  if (gai_errno != 0)
    throw Error(CR_UNKNOWN_HOST, "No such host is known '" + host + "'");

  for (t_res= res_lst; t_res; t_res= t_res->ai_next)
  {
    error = m_sync_connection.connect((sockaddr*)t_res->ai_addr, t_res->ai_addrlen);

    if (!error)
      break;
  }
  freeaddrinfo(res_lst);

  if (error)
  {
    std::string error_description = error.what();
    throw Error(CR_CONNECTION_ERROR, error_description + " connecting to " + host + ":" + port_buf);
  }

  m_closed = false;
}

void XProtocol::connect_to_localhost(const std::string &unix_socket_or_named_pipe)
{
  Error error = m_sync_connection.connect_to_localhost(unix_socket_or_named_pipe);

  if (error)
  {
    std::string error_description = error.what();
    throw Error(CR_CONNECTION_ERROR, error_description + ", while connecting to "+unix_socket_or_named_pipe);
  }

  m_closed = false;
}



void XProtocol::authenticate(const std::string &user, const std::string &pass, const std::string &schema)
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

void XProtocol::fetch_capabilities()
{
  send(Mysqlx::Connection::CapabilitiesGet());
  int mid;
  ngs::unique_ptr<Message> message(recv_raw(mid));
  if (mid != Mysqlx::ServerMessages::CONN_CAPABILITIES)
    throw Error(CR_COMMANDS_OUT_OF_SYNC, "Unexpected response received from server");
  m_capabilities = *static_cast<Mysqlx::Connection::Capabilities*>(message.get());
}

void XProtocol::enable_tls()
{
  Error ec = m_sync_connection.activate_tls();

  if (ec)
  {
    // If ssl activation failed then
    // server and client are in different states
    // lets force disconnect
    set_closed();

    throw ec;
  }
}

void XProtocol::set_closed()
{
  m_closed = true;
}

void XProtocol::close()
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
      ngs::unique_ptr<Message> message(recv_raw(mid));
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

unsigned long XProtocol::get_received_msg_counter(const std::string &id) const
{
  std::map<std::string, unsigned long>::const_iterator i =
      m_received_msg_counters.find(id);
  return i == m_received_msg_counters.end() ? 0ul : i->second;
}

void XProtocol::perform_close()
{
  if (m_dont_wait_for_disconnect)
  {
    m_sync_connection.close();
    return;
  }

  int mid;
  ngs::unique_ptr<Message> message(recv_raw(mid));
  std::stringstream s;

  s << "Unexpected message received with id:" << mid << " while waiting for disconnection";

  throw Error(CR_COMMANDS_OUT_OF_SYNC, s.str());
}

ngs::shared_ptr<Result> XProtocol::recv_result()
{
  return new_result(true);
}

ngs::shared_ptr<Result> XProtocol::new_empty_result()
{
  ngs::shared_ptr<Result> empty_result(new Result(shared_from_this(), false, false));

  return empty_result;
}

ngs::shared_ptr<Result> XProtocol::execute_sql(const std::string &sql)
{
  {
    Mysqlx::Sql::StmtExecute exec;
    exec.set_namespace_("sql");
    exec.set_stmt(sql);
    send(exec);
  }

  return new_result(true);
}

ngs::shared_ptr<Result> XProtocol::execute_stmt(const std::string &ns, const std::string &sql, const std::vector<ArgumentValue> &args)
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

ngs::shared_ptr<Result> XProtocol::execute_find(const Mysqlx::Crud::Find &m)
{
  send(m);

  return new_result(true);
}

ngs::shared_ptr<Result> XProtocol::execute_update(const Mysqlx::Crud::Update &m)
{
  send(m);

  return new_result(false);
}

ngs::shared_ptr<Result> XProtocol::execute_insert(const Mysqlx::Crud::Insert &m)
{
  send(m);

  return new_result(false);
}

ngs::shared_ptr<Result> XProtocol::execute_delete(const Mysqlx::Crud::Delete &m)
{
  send(m);

  return new_result(false);
}

void XProtocol::setup_capability(const std::string &name, const bool value)
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
  ngs::unique_ptr<Message> msg(recv_raw(mid));

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

void XProtocol::authenticate_mysql41(const std::string &user, const std::string &pass, const std::string &db)
{
  {
    Mysqlx::Session::AuthenticateStart auth;

    auth.set_mech_name("MYSQL41");

    send(Mysqlx::ClientMessages::SESS_AUTHENTICATE_START, auth);
  }

  {
    int mid;
    ngs::unique_ptr<Message> message(recv_raw(mid));
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
        break;

      default:
        throw Error(CR_MALFORMED_PACKET, "Unexpected message received from server during authentication");
        break;
    }
  }

  bool done = false;
  while (!done)
  {
    int mid;
    ngs::unique_ptr<Message> message(recv_raw(mid));
    switch (mid)
    {
      case Mysqlx::ServerMessages::SESS_AUTHENTICATE_OK:
        done = true;
        break;

      case Mysqlx::ServerMessages::ERROR:
        throw_server_error(*static_cast<Mysqlx::Error*>(message.get()));
        break;

      case Mysqlx::ServerMessages::NOTICE:
        dispatch_notice(static_cast<Mysqlx::Notice::Frame*>(message.get()));
        break;

      default:
        throw Error(CR_MALFORMED_PACKET, "Unexpected message received from server during authentication");
        break;
    }
  }
}

void XProtocol::authenticate_plain(const std::string &user, const std::string &pass, const std::string &db)
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
    ngs::unique_ptr<Message> message(recv_raw(mid));
    switch (mid)
    {
      case Mysqlx::ServerMessages::SESS_AUTHENTICATE_OK:
        done = true;
        break;

      case Mysqlx::ServerMessages::ERROR:
        throw_server_error(*static_cast<Mysqlx::Error*>(message.get()));
        break;

      case Mysqlx::ServerMessages::NOTICE:
        dispatch_notice(static_cast<Mysqlx::Notice::Frame*>(message.get()));
        break;

      default:
        throw Error(CR_MALFORMED_PACKET, "Unexpected message received from server during authentication");
        break;
    }
  }
}

void XProtocol::send_bytes(const std::string &data)
{
  Error error = m_sync_connection.write(data.data(), data.size());
  throw_mysqlx_error(error);
}

void XProtocol::send(int mid, const Message &msg)
{
  Error error;
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

void XProtocol::push_local_notice_handler(Local_notice_handler handler)
{
  m_local_notice_handlers.push_back(handler);
}

void XProtocol::pop_local_notice_handler()
{
  m_local_notice_handlers.pop_back();
}

void XProtocol::dispatch_notice(Mysqlx::Notice::Frame *frame)
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

Message *XProtocol::recv_next(int &mid)
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

Message *XProtocol::recv_raw_with_deadline(int &mid, const int deadline_milliseconds)
{
  char header_buffer[5];
  std::size_t data = sizeof(header_buffer);
  Error error = m_sync_connection.read_with_timeout(header_buffer, data, deadline_milliseconds);

  if (0 == data)
  {
    m_closed = true;
    return NULL;
  }

  throw_mysqlx_error(error);

  return recv_message_with_header(mid, header_buffer, sizeof(header_buffer));
}

Message *XProtocol::recv_payload(const int mid, const std::size_t msglen)
{
  Error error;
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
      std::string err("Message is not properly initialized: ");
      err += ret_val->InitializationErrorString();

      delete[] mbuf;
      delete ret_val;

      throw Error(CR_MALFORMED_PACKET, err);
    }
  }
  else
  {
    delete[] mbuf;
    throw_mysqlx_error(error);
  }

  delete[] mbuf;
  update_received_msg_counter(ret_val);
  return ret_val;
}

Message *XProtocol::recv_raw(int &mid)
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

Message *XProtocol::recv_message_with_header(int &mid, char (&header_buffer)[5], const std::size_t header_offset)
{
  Message* ret_val = NULL;
  Error error;

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

void XProtocol::throw_mysqlx_error(const Error &error)
{
  if (!error)
    return;

  throw error;
}

ngs::shared_ptr<Result> XProtocol::new_result(bool expect_data)
{
  if (m_last_result)
    m_last_result->buffer();

  m_last_result.reset(new Result(shared_from_this(), expect_data));

  return m_last_result;
}

void XProtocol::update_received_msg_counter(const Message* msg)
{
  const std::string &id = msg->GetDescriptor()->full_name();
  ++m_received_msg_counters[id];

  if (id != Mysqlx::Notice::Frame::descriptor()->full_name()) return;

  static const std::string *notice_type_id[] = {
      &Mysqlx::Notice::Warning::descriptor()->full_name(),
      &Mysqlx::Notice::SessionVariableChanged::descriptor()->full_name(),
      &Mysqlx::Notice::SessionStateChanged::descriptor()->full_name()};
  static const unsigned notice_type_id_size =
      sizeof(notice_type_id) / sizeof(notice_type_id[0]);
  const ::google::protobuf::uint32 notice_type =
      static_cast<const Mysqlx::Notice::Frame *>(msg)->type() - 1u;
  if (notice_type < notice_type_id_size)
    ++m_received_msg_counters[*notice_type_id[notice_type]];
}

#ifdef WIN32
#  pragma pop_macro("ERROR")
#endif
