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


#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <string.h>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <ios>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>

#include "dummy_stream.h"
#include "m_string.h" // needed by writer.h, but has to be included after expr_parser.h
#include "my_global.h"
#include "mysqlx_error.h"
#include "mysqlx_protocol.h"
#include "mysqlx_resultset.h"
#include "mysqlx_session.h"
#include "mysqlx_version.h"
#include "ngs_common/bind.h"
#include "mysqlxtest_error_names.h"
#include "common/utils_string_parsing.h"
#include "ngs_common/chrono.h"
#include "ngs_common/protocol_const.h"
#include "ngs_common/protocol_protobuf.h"
#include "ngs_common/to_string.h"
#include "utils_mysql_parsing.h"
#include "message_formatter.h"
#include "violite.h"

#ifdef HAVE_SYS_UN_H
#include <sys/un.h>
#endif

const char * const CMD_ARG_BE_QUIET = "be-quiet";
const char * const MYSQLXTEST_VERSION = "1.0";
const char CMD_ARG_SEPARATOR = '\t';

#include <mysql/service_my_snprintf.h>
#include <mysql.h>

#ifdef _MSC_VER
#  pragma push_macro("ERROR")
#  undef ERROR
#endif

using namespace google::protobuf;

typedef std::map<std::string, std::string> Message_by_full_name;
static Message_by_full_name server_msgs_by_full_name;
static Message_by_full_name client_msgs_by_full_name;

typedef std::map<std::string, std::pair<mysqlx::Message* (*)(), int8_t> > Message_by_name;
typedef ngs::function<void (std::string)> Value_callback;
static Message_by_name server_msgs_by_name;
static Message_by_name client_msgs_by_name;

typedef std::map<int8_t, std::pair<mysqlx::Message* (*)(), std::string> > Message_by_id;
static Message_by_id server_msgs_by_id;
static Message_by_id client_msgs_by_id;

typedef ngs::unique_ptr<mysqlx::Message> Message_ptr;

bool OPT_quiet = false;
bool OPT_bindump = false;
bool OPT_show_warnings = false;
bool OPT_fatal_errors = true;
bool OPT_verbose = false;
bool OPT_query = true;
#ifndef _WIN32
bool OPT_color = false;
#endif
const char current_dir[] = {FN_CURLIB, FN_LIBCHAR, '\0'};
std::string OPT_import_path(current_dir);

class Expected_error;
static Expected_error *OPT_expect_error = 0;

struct Stack_frame {
  int line_number;
  std::string context;
};
static std::list<Stack_frame> script_stack;

static std::map<std::string, std::string> variables;
static std::list<std::string> variables_to_unreplace;

static void ignore_traces_from_libraries(enum loglevel ll, const char *format, va_list args)
{
}

static std::ostream &get_stream_for_results(const bool force_quiet = false)
{
  if (OPT_query && !force_quiet)
    return std::cout;

  static Dummy_stream dummy;

  return dummy;
}

static void replace_variables(std::string &s)
{
  for (std::map<std::string, std::string>::const_iterator sub = variables.begin();
      sub != variables.end(); ++sub)
  {
    std::string tmp(sub->second);

    aux::replace_all(tmp, "\"", "\\\"");
    aux::replace_all(tmp, "\n", "\\n");
    aux::replace_all(s, sub->first, tmp);
  }
}

static std::string unreplace_variables(const std::string &in, bool clear)
{
  std::string s = in;
  for (std::list<std::string>::const_iterator sub = variables_to_unreplace.begin();
      sub != variables_to_unreplace.end(); ++sub)
  {
    aux::replace_all(s, variables[*sub], *sub);
  }
  if (clear)
    variables_to_unreplace.clear();
  return s;
}

static std::string error()
{
  std::string context;

  for (std::list<Stack_frame>::const_reverse_iterator it = script_stack.rbegin(); it != script_stack.rend(); ++it)
  {
    char tmp[1024];
    my_snprintf(tmp, sizeof(tmp), "in %s, line %i:", it->context.c_str(), it->line_number);
    context.append(tmp);
  }

#ifndef _WIN32
  if (OPT_color)
    return std::string("\e[1;31m").append(context).append("ERROR: ");
  else
#endif
    return std::string(context).append("ERROR: ");
}

static std::string eoerr()
{
#ifndef _WIN32
  if (OPT_color)
    return "\e[0m\n";
  else
#endif
    return "\n";
}

static void dumpx(const std::exception &exc)
{
  std::cerr << error() << exc.what() << eoerr();
}

static void dumpx(const mysqlx::Error &exc)
{
  std::cerr << error() << exc.what() << " (code " << exc.error() << ")" << eoerr();
}

static void print_columndata(const std::vector<mysqlx::ColumnMetadata> &meta);
static void print_result_set(mysqlx::Result &result);
static void print_result_set(mysqlx::Result &result, const std::vector<std::string> &columns,
                             Value_callback value_callback = Value_callback(), bool quiet = false);

//---------------------------------------------------------------------------------------------------------

class Expected_error
{
public:
  Expected_error() {}

  void expect_errno(int err)
  {
    m_expect_errno.insert(err);
  }

  bool check_error(const mysqlx::Error &err)
  {
    if (m_expect_errno.empty())
    {
      dumpx(err);
      return !OPT_fatal_errors;
    }

    return check(err);
  }

  bool check_ok()
  {
    if (m_expect_errno.empty())
      return true;
    return check(mysqlx::Error());
  }

private:
  bool check(const mysqlx::Error &err)
  {
    if (m_expect_errno.find(err.error()) == m_expect_errno.end())
    {
      print_unexpected_error(err);
      m_expect_errno.clear();
      return !OPT_fatal_errors;
    }

    print_expected_error(err);
    m_expect_errno.clear();
    return true;
  }

  void print_unexpected_error(const mysqlx::Error &err)
  {
    std::cerr << error() << "Got unexpected error";
    print_error_msg(std::cerr, err);
    std::cerr << "; expected was ";
    if (m_expect_errno.size() > 1)
      std::cerr << "one of: ";
    print_expect_errors(std::cerr);
    std::cerr << "\n";
  }

  void print_expected_error(const mysqlx::Error &err)
  {
    std::cout << "Got expected error";
    if (m_expect_errno.size() == 1)
      print_error_msg(std::cout, err);
    else
    {
      std::cout << " (one of: ";
      print_expect_errors(std::cout);
      std::cout << ")";
    }
    std::cout << "\n";
  }

  void print_error_msg(std::ostream & os, const mysqlx::Error &err)
  {
    if (err.error())
      os << ": " << err.what();
    os << " (code " << err.error() << ")";
  }

  void print_expect_errors(std::ostream & os)
  {
    std::copy(m_expect_errno.begin(),
              m_expect_errno.end(),
              std::ostream_iterator<int>(os, " "));
  }

  std::set<int> m_expect_errno;
};

//---------------------------------------------------------------------------------------------------------

struct Connection_options
{
  Connection_options()
  : port(0)
  {
  }

  std::string socket;
  std::string host;
  int port;
  std::string user;
  std::string password;
  std::string schema;
};

class Connection_manager
{
public:
  Connection_manager(const std::string &uri,
                     const Connection_options &co,
                     const mysqlx::Ssl_config &ssl_config_,
                     const std::size_t timeout_,
                     const bool _dont_wait_for_disconnect,
                     const mysqlx::Internet_protocol ip_mode)
  : connection_options(co),
    ssl_config(ssl_config_),
    timeout(timeout_),
    dont_wait_for_disconnect(_dont_wait_for_disconnect),
    m_ip_mode(ip_mode)
  {
    int pwdfound;
    std::string proto;

    if (uri.length())
    {
      mysqlx::parse_mysql_connstring(uri, proto,
          connection_options.user,
          connection_options.password,
          connection_options.host,
          connection_options.port,
          connection_options.socket,
          connection_options.schema,
          pwdfound);
    }
    variables["%OPTION_CLIENT_USER%"]     = connection_options.user;
    variables["%OPTION_CLIENT_PASSWORD%"] = connection_options.password;
    variables["%OPTION_CLIENT_HOST%"]     = connection_options.host;
    variables["%OPTION_CLIENT_PORT%"]     = connection_options.port;
    variables["%OPTION_CLIENT_SOCKET%"]   = connection_options.socket;
    variables["%OPTION_CLIENT_SCHEMA%"]   = connection_options.schema;

    active_connection.reset(new mysqlx::XProtocol(ssl_config, timeout, dont_wait_for_disconnect, m_ip_mode));
    connections[""] = active_connection;

    if (OPT_verbose)
      std::cout << "Connecting...\n";

    make_connection(active_connection);
  }

  void get_credentials(std::string &ret_user, std::string &ret_pass)
  {
    ret_user = connection_options.user;
    ret_pass = connection_options.password;
  }

  void connect_default(const bool send_cap_password_expired = false, bool use_plain_auth = false)
  {
    if (send_cap_password_expired)
      active_connection->setup_capability("client.pwd_expire_ok", true);

    if (use_plain_auth)
      active_connection->authenticate_plain(connection_options.user, connection_options.password, connection_options.schema);
    else
      active_connection->authenticate(connection_options.user, connection_options.password, connection_options.schema);

    std::stringstream s;
    s << active_connection->client_id();
    variables["%ACTIVE_CLIENT_ID%"] = s.str();

    if (OPT_verbose)
      std::cout << "Connected client #" << active_connection->client_id() << "\n";
  }

  void create(const std::string &name,
              const std::string &user, const std::string &password, const std::string &db,
              bool no_ssl)
  {
    if (connections.find(name) != connections.end())
      throw std::runtime_error("a session named "+name+" already exists");

    std::cout << "connecting...\n";

    ngs::shared_ptr<mysqlx::XProtocol> connection;
    mysqlx::Ssl_config                    connection_ssl_config;

    if (!no_ssl)
      connection_ssl_config = ssl_config;

    connection.reset(new mysqlx::XProtocol(connection_ssl_config, timeout, dont_wait_for_disconnect, m_ip_mode));

    make_connection(connection);

    if (user != "-")
    {
      if (user.empty())
        connection->authenticate(connection_options.user, connection_options.password, db.empty() ? connection_options.schema : db);
      else
        connection->authenticate(user, password, db.empty() ? connection_options.schema : db);
    }

    active_connection = connection;
    active_connection_name = name;
    connections[name] = active_connection;
    std::stringstream s;
    s << active_connection->client_id();
    variables["%ACTIVE_CLIENT_ID%"] = s.str();
    std::cout << "active session is now '" << name << "'\n";

    if (OPT_verbose)
      std::cout << "Connected client #" << active_connection->client_id() << "\n";
  }

  void abort_active()
  {
    if (active_connection)
    {
      if (!active_connection_name.empty())
        std::cout << "aborting session " << active_connection_name << "\n";
      active_connection->set_closed();
      active_connection.reset();
      connections.erase(active_connection_name);
      if (active_connection_name != "")
        set_active("");
    }
    else
      throw std::runtime_error("no active session");
  }

  bool is_default_active()
  {
    return active_connection_name.empty();
  }

  void close_active(bool shutdown = false)
  {
    if (active_connection)
    {
      if (active_connection_name.empty() && !shutdown)
        throw std::runtime_error("cannot close default session");
      try
      {
        if (!active_connection_name.empty())
          std::cout << "closing session " << active_connection_name << "\n";

        if (!active_connection->is_closed())
        {
          // send a close message and wait for the corresponding Ok message
          active_connection->send(Mysqlx::Session::Close());
          active_connection->set_closed();
          int msgid;
          Message_ptr msg(active_connection->recv_raw(msgid));
          std::cout << formatter::message_to_text(*msg);
          if (Mysqlx::ServerMessages::OK != msgid)
            throw mysqlx::Error(CR_COMMANDS_OUT_OF_SYNC,
                                "Disconnect was expecting Mysqlx.Ok(bye!), but got the one above (one or more calls to -->recv are probably missing)");

          std::string text = static_cast<Mysqlx::Ok*>(msg.get())->msg();
          if (text != "bye!" && text != "tchau!")
            throw mysqlx::Error(CR_COMMANDS_OUT_OF_SYNC,
                                "Disconnect was expecting Mysqlx.Ok(bye!), but got the one above (one or more calls to -->recv are probably missing)");

          if (!dont_wait_for_disconnect)
          {
            try
            {
              Message_ptr msg(active_connection->recv_raw(msgid));

              std::cout << formatter::message_to_text(*msg);

              throw mysqlx::Error(CR_COMMANDS_OUT_OF_SYNC,
                  "Was expecting closure but got the one above message");
            }
            catch (...)
            {}
          }
        }
        connections.erase(active_connection_name);
        if (!shutdown)
          set_active("");
      }
      catch (...)
      {
        connections.erase(active_connection_name);
        if (!shutdown)
          set_active("");
        throw;
      }
    }
    else if (!shutdown)
      throw std::runtime_error("no active session");
  }

  void set_active(const std::string &name)
  {
    if (connections.find(name) == connections.end())
    {
      std::string slist;
      for (std::map<std::string, ngs::shared_ptr<mysqlx::XProtocol> >::const_iterator it = connections.begin(); it != connections.end(); ++it)
        slist.append(it->first).append(", ");
      if (!slist.empty())
        slist.resize(slist.length()-2);
      throw std::runtime_error("no session named '"+name+"': " + slist);
    }
    active_connection = connections[name];
    active_connection_name = name;
    std::stringstream s;
    s << active_connection->client_id();
    variables["%ACTIVE_CLIENT_ID%"] = s.str();
    std::cout << "switched to session " << (active_connection_name.empty() ? "default" : active_connection_name) << "\n";
  }

  mysqlx::XProtocol* active()
  {
    if (!active_connection)
      std::runtime_error("no active session");
    return active_connection.get();
  }

private:
  void make_connection(ngs::shared_ptr<mysqlx::XProtocol> &connection)
  {
    if (connection_options.socket.empty())
      connection->connect(connection_options.host, connection_options.port);
    else
      connection->connect_to_localhost(connection_options.socket);
  }

  std::map<std::string, ngs::shared_ptr<mysqlx::XProtocol> > connections;
  ngs::shared_ptr<mysqlx::XProtocol> active_connection;
  std::string active_connection_name;
  Connection_options connection_options;

  mysqlx::Ssl_config ssl_config;
  const std::size_t timeout;
  const bool dont_wait_for_disconnect;
  const mysqlx::Internet_protocol m_ip_mode;
};

static std::string data_to_bindump(const std::string &bindump)
{
  std::string res;

  for (size_t i = 0; i < bindump.length(); i++)
  {
    unsigned char ch = bindump[i];

    if (i >= 5 && ch == '\\')
    {
      res.push_back('\\');
      res.push_back('\\');
    }
    else if (i >= 5 && isprint(ch) && !isblank(ch))
      res.push_back(ch);
    else
    {
      res.append("\\x");
      res.push_back(aux::ALLOWED_HEX_CHARACTERS[(ch >> 4) & 0xf]);
      res.push_back(aux::ALLOWED_HEX_CHARACTERS[ch & 0xf]);
    }
  }

  return res;
}

static std::string bindump_to_data(const std::string &bindump)
{
  std::string res;
  for (size_t i = 0; i < bindump.length(); i++)
  {
    if (bindump[i] == '\\')
    {
      if (bindump[i+1] == '\\')
      {
        res.push_back('\\');
        ++i;
      }
      else if (bindump[i+1] == 'x')
      {
        int value = 0;
        const char *hex = aux::ALLOWED_HEX_CHARACTERS.c_str();
        const char *p = strchr(hex, bindump[i+2]);
        if (p)
          value = (p - hex) << 4;
        else
        {
          std::cerr << error() << "Invalid bindump char at " << i+2 << eoerr();
          break;
        }
        p = strchr(hex, bindump[i+3]);
        if (p)
          value |= p - hex;
        else
        {
          std::cerr << error() << "Invalid bindump char at " << i+3 << eoerr();
          break;
        }
        i += 3;
        res.push_back(value);
      }
    }
    else
      res.push_back(bindump[i]);
  }
  return res;
}

static std::string message_to_bindump(const mysqlx::Message &message)
{
  std::string res;
  std::string out;

  message.SerializeToString(&out);

  res.resize(5);
  *(uint32_t*)res.data() = static_cast<uint32_t>(out.size() + 1);

#ifdef WORDS_BIGENDIAN
  std::swap(res[0], res[3]);
  std::swap(res[1], res[2]);
#endif

  res[4] = client_msgs_by_name[client_msgs_by_full_name[message.GetDescriptor()->full_name()]].second;
  res.append(out);

  return data_to_bindump(res);
}

class ErrorDumper : public ::google::protobuf::io::ErrorCollector
{
  std::stringstream m_out;

public:
  virtual void AddError(int line, int column, const string & message)
  {
    m_out << "ERROR in message: line " << line+1 << ": column " << column << ": " << message<<"\n";
  }

  virtual void AddWarning(int line, int column, const string & message)
  {
    m_out << "WARNING in message: line " << line+1 << ": column " << column << ": " << message<<"\n";
  }

  std::string str() { return m_out.str(); }
};

static mysqlx::Message *text_to_client_message(const std::string &name, const std::string &data, int8_t &msg_id)
{
  if (client_msgs_by_full_name.find(name) == client_msgs_by_full_name.end())
  {
    std::cerr << error() << "Invalid message type " << name << eoerr();
    return NULL;
  }

  Message_by_name::const_iterator msg = client_msgs_by_name.find(client_msgs_by_full_name[name]);
  if (msg == client_msgs_by_name.end())
  {
    std::cerr << error() << "Invalid message type " << name << eoerr();
    return NULL;
  }

  mysqlx::Message *message = msg->second.first();
  msg_id = msg->second.second;

  google::protobuf::TextFormat::Parser parser;
  ErrorDumper dumper;
  parser.RecordErrorsTo(&dumper);
  if (!parser.ParseFromString(data, message))
  {
    std::cerr << error() << "Invalid message in input: " << name << eoerr();
    int i = 1;
    for (std::string::size_type p = 0, n = data.find('\n', p+1);
        p != std::string::npos;
        p = (n == std::string::npos ? n : n+1), n = data.find('\n', p+1), ++i)
    {
      std::cerr << i << ": " << data.substr(p, n-p) << "\n";
    }
    std::cerr << "\n" << dumper.str();
    delete message;
    return NULL;
  }

  return message;
}

static bool dump_notices(int type, const std::string &data)
{
  if (type == 3)
  {
    Mysqlx::Notice::SessionStateChanged change;
    change.ParseFromString(data);
    if (!change.IsInitialized())
      std::cerr << "Invalid notice received from server " << change.InitializationErrorString() << "\n";
    else
    {
      if (change.param() == Mysqlx::Notice::SessionStateChanged::ACCOUNT_EXPIRED)
      {
        std::cout << "NOTICE: Account password expired\n";
        return true;
      }
    }
  }
  return false;
}

//-----------------------------------------------------------------------------------

class Execution_context
{
public:
  Execution_context(std::istream &stream, Connection_manager *cm)
  : m_stream(stream), m_cm(cm)
  { }

  std::string         m_command_name;
  std::istream       &m_stream;
  Connection_manager *m_cm;

  mysqlx::XProtocol *connection() { return m_cm->active(); }
};

//---------------------------------------------------------------------------------------------------------

class Macro
{
public:
  Macro(const std::string &name, const std::list<std::string> &argnames)
  : m_name(name), m_args(argnames)
  { }

  std::string name() const { return m_name; }

  void set_body(const std::string &body)
  {
    m_body = body;
  }

  std::string get(const std::list<std::string> &args) const
  {
    if (args.size() != m_args.size())
    {
      std::cerr << error() << "Invalid number of arguments for macro "+m_name << ", expected:" << m_args.size() << " actual:" << args.size() << eoerr();
      return "";
    }

    std::string text = m_body;
    std::list<std::string>::const_iterator n = m_args.begin(), v = args.begin();
    for (size_t i = 0; i < args.size(); i++)
    {
      aux::replace_all(text, *(n++), *(v++));
    }
    return text;
  }

public:
  static std::list<ngs::shared_ptr<Macro> > macros;

  static void add(ngs::shared_ptr<Macro> macro)
  {
    macros.push_back(macro);
  }

  static std::string get(const std::string &cmd, std::string &r_name)
  {
    std::list<std::string> args;
    std::string::size_type p = std::min(cmd.find(' '), cmd.find('\t'));
    if (p == std::string::npos)
      r_name = cmd;
    else
    {
      r_name = cmd.substr(0, p);
      std::string rest = cmd.substr(p+1);
      aux::split(args, rest, "\t", true);
    }
    if (r_name.empty())
    {
      std::cerr << error() << "Missing macro name for macro call" << eoerr();
      return "";
    }

    for (std::list<ngs::shared_ptr<Macro> >::const_iterator iter = macros.begin(); iter != macros.end(); ++iter)
    {
      if ((*iter)->m_name == r_name)
      {
        return (*iter)->get(args);
      }
    }
    std::cerr << error() << "Undefined macro " << r_name << eoerr();
    return "";
  }

  static bool call(Execution_context &context, const std::string &cmd);

private:
  std::string m_name;
  std::list<std::string> m_args;
  std::string m_body;
};

std::list<ngs::shared_ptr<Macro> > Macro::macros;


//---------------------------------------------------------------------------------------------------------

class Command
{
public:
  enum Result {Continue, Stop_with_success, Stop_with_failure};

  Command()
  : m_cmd_prefix("-->")
  {
    m_commands["title "]      = &Command::cmd_title;
    m_commands["echo "]       = &Command::cmd_echo;
    m_commands["recvtype "]   = &Command::cmd_recvtype;
    m_commands["recverror "]  = &Command::cmd_recverror;
    m_commands["recvresult"]  = &Command::cmd_recvresult;
    m_commands["recvtovar "]  = &Command::cmd_recvtovar;
    m_commands["recvuntil "]  = &Command::cmd_recvuntil;
    m_commands["recvuntildisc"] = &Command::cmd_recv_all_until_disc;
    m_commands["enablessl"]   = &Command::cmd_enablessl;
    m_commands["sleep "]      = &Command::cmd_sleep;
    m_commands["login "]      = &Command::cmd_login;
    m_commands["stmtadmin "]  = &Command::cmd_stmtadmin;
    m_commands["stmtsql "]    = &Command::cmd_stmtsql;
    m_commands["loginerror "] = &Command::cmd_loginerror;
    m_commands["repeat "]     = &Command::cmd_repeat;
    m_commands["endrepeat"]   = &Command::cmd_endrepeat;
    m_commands["system "]     = &Command::cmd_system;
    m_commands["peerdisc "]   = &Command::cmd_peerdisc;
    m_commands["recv"]        = &Command::cmd_recv;
    m_commands["exit"]        = &Command::cmd_exit;
    m_commands["abort"]        = &Command::cmd_abort;
    m_commands["nowarnings"]  = &Command::cmd_nowarnings;
    m_commands["yeswarnings"] = &Command::cmd_yeswarnings;
    m_commands["fatalerrors"] = &Command::cmd_fatalerrors;
    m_commands["nofatalerrors"] = &Command::cmd_nofatalerrors;
    m_commands["newsession "]  = &Command::cmd_newsession;
    m_commands["newsessionplain "]  = &Command::cmd_newsessionplain;
    m_commands["setsession "]  = &Command::cmd_setsession;
    m_commands["setsession"]  = &Command::cmd_setsession; // for setsession with no args
    m_commands["closesession"]= &Command::cmd_closesession;
    m_commands["expecterror "] = &Command::cmd_expecterror;
    m_commands["measure"]      = &Command::cmd_measure;
    m_commands["endmeasure "]  = &Command::cmd_endmeasure;
    m_commands["quiet"]        = &Command::cmd_quiet;
    m_commands["noquiet"]      = &Command::cmd_noquiet;
    m_commands["varfile "]     = &Command::cmd_varfile;
    m_commands["varlet "]      = &Command::cmd_varlet;
    m_commands["varinc "]      = &Command::cmd_varinc;
    m_commands["varsub "]      = &Command::cmd_varsub;
    m_commands["vargen "]      = &Command::cmd_vargen;
    m_commands["binsend "]     = &Command::cmd_binsend;
    m_commands["hexsend "]     = &Command::cmd_hexsend;
    m_commands["binsendoffset "] = &Command::cmd_binsendoffset;
    m_commands["callmacro "]   = &Command::cmd_callmacro;
    m_commands["import "]      = &Command::cmd_import;
    m_commands["assert_eq "]      = &Command::cmd_assert_eq;
    m_commands["assert_gt "]      = &Command::cmd_assert_gt;
    m_commands["assert_ge "]      = &Command::cmd_assert_ge;
    m_commands["query_result"]    = &Command::cmd_query;
    m_commands["noquery_result"]  = &Command::cmd_noquery;
    m_commands["wait_for "]       = &Command::cmd_wait_for;
    m_commands["received "]       = &Command::cmd_received;
  }

  bool is_command_syntax(const std::string &cmd) const
  {
    return 0 == strncmp(cmd.c_str(), m_cmd_prefix.c_str(), m_cmd_prefix.length());
  }

  Result process(Execution_context &context, const std::string &command)
  {
    if (!is_command_syntax(command))
      return Stop_with_failure;

    Command_map::iterator i = std::find_if(m_commands.begin(),
                                           m_commands.end(),
                                           ngs::bind(&Command::match_command_name, this, ngs::placeholders::_1, command));

    if (i == m_commands.end())
    {
      std::cerr << "Unknown command " << command << "\n";
      return Stop_with_failure;
    }

    if (OPT_verbose)
      std::cout << "Execute " << command <<"\n";

    context.m_command_name = (*i).first;

    return (*this.*(*i).second)(context, command.c_str() + m_cmd_prefix.length() + (*i).first.length());
  }

private:
  typedef std::map< std::string, Result (Command::*)(Execution_context &,const std::string &) > Command_map;
  typedef ::Mysqlx::Datatypes::Any Any;

  struct Loop_do
  {
    std::streampos block_begin;
    int            iterations;
    int            value;
    std::string    variable_name;
  };

  Command_map        m_commands;
  std::list<Loop_do> m_loop_stack;
  std::string        m_cmd_prefix;

  bool match_command_name(const Command_map::value_type &command, const std::string &instruction)
  {
    if (m_cmd_prefix.length() + command.first.length() > instruction.length())
      return false;

    std::string::const_iterator i = std::find(instruction.begin(), instruction.end(), ' ');
    std::string                 command_name(instruction.begin() + m_cmd_prefix.length(), i);

    if (0 != command.first.compare(command_name))
    {
      if (instruction.end() != i)
      {
        ++i;
        return 0 == command.first.compare(std::string(instruction.begin() + m_cmd_prefix.length(), i));
      }

      return false;
    }

    return true;
  }

  Result cmd_echo(Execution_context &context, const std::string &args)
  {
    std::string s = args;
    replace_variables(s);
    std::cout << s << "\n";

    return Continue;
  }

  Result cmd_title(Execution_context &context, const std::string &args)
  {
    if (!args.empty())
    {
      std::cout << "\n" << args.substr(1) << "\n";
      std::string sep(args.length()-1, args[0]);
      std::cout << sep << "\n";
    }
    else
      std::cout << "\n\n";

    return Continue;
  }

  Result cmd_recvtype(Execution_context &context, const std::string &args)
  {
    std::vector<std::string> vargs;
    aux::split(vargs, args, " ", true);

    if (1 != vargs.size() &&
        2 != vargs.size())
    {
      std::stringstream error_message;
      error_message << "Received wrong number of arguments, got:"
                    << vargs.size();
      throw std::logic_error(error_message.str());
    }

    bool be_quiet = false;
    int msgid;
    Message_ptr msg(context.connection()->recv_raw(msgid));

    if (1 < vargs.size())
    {
      if (vargs[1] == CMD_ARG_BE_QUIET)
        be_quiet = true;
    }

    if (NULL == msg.get())
      return OPT_fatal_errors ? Stop_with_failure : Continue;

    try
    {
      const std::string message_in_text = unreplace_variables(formatter::message_to_text(*msg), true);

      if (msg->GetDescriptor()->full_name() != vargs[0])
      {
        std::cout << "Received unexpected message. Was expecting:\n    " << vargs[0] << "\nbut got:\n";
        std::cout << message_in_text << "\n";

        return OPT_fatal_errors ? Stop_with_failure : Continue;
      }

      std::ostream &out = get_stream_for_results(be_quiet);

      out << message_in_text << "\n";
    }
    catch (std::exception &e)
    {
      dumpx(e);
      if (OPT_fatal_errors)
        return Stop_with_success;
    }

    return Continue;
  }

  Result cmd_recverror(Execution_context &context, const std::string &args)
  {
    int msgid;
    Message_ptr msg(context.connection()->recv_raw(msgid));

    if (msg.get())
    {
      bool failed = false;
      try
      {
        const int expected_error_code = mysqlxtest::get_error_code_by_text(args);
        if (msg->GetDescriptor()->full_name() != "Mysqlx.Error" ||
            expected_error_code != (int)static_cast<Mysqlx::Error*>(msg.get())->code())
        {
          std::cout << error() << "Was expecting Error " << args <<", but got:" << eoerr();
          failed = true;
        }
        else
        {
          std::cout << "Got expected error:\n";
        }

        std::cout << formatter::message_to_text(*msg) << "\n";
        if (failed && OPT_fatal_errors)
          return Stop_with_success;
      }
      catch (std::exception &e)
      {
        dumpx(e);
        if (OPT_fatal_errors)
          return Stop_with_success;
      }
    }

    return Continue;
  }

  static void set_variable(std::string name, std::string value)
  {
    variables[name] = value;
  }

  Result cmd_recvtovar(Execution_context &context, const std::string &args)
  {
    std::string args_cmd = args;
    std::vector<std::string> args_array;
    aux::trim(args_cmd);

    aux::split(args_array, args_cmd, " ", false);

    args_cmd = CMD_ARG_BE_QUIET;

    if (args_array.size() > 1)
    {
      args_cmd += " ";
      args_cmd += args_array.at(1);
    }

    cmd_recvresult(context, args_cmd, ngs::bind(&Command::set_variable, args_array.at(0), ngs::placeholders::_1));

    return Continue;
  }

  Result cmd_recvresult(Execution_context &context, const std::string &args)
  {
    return cmd_recvresult(context, args, Value_callback());
  }

  Result cmd_recvresult(Execution_context &context, const std::string &args, Value_callback value_callback)
  {
    ngs::shared_ptr<mysqlx::Result> result;
    try
    {
      std::vector<std::string> columns;
      std::string cmd_args = args;

      aux::trim(cmd_args);

      if (cmd_args.size())
        aux::split(columns, cmd_args, " ", false);

      std::vector<std::string>::iterator i = std::find(columns.begin(), columns.end(), "print-columnsinfo");
      const bool print_colinfo = i != columns.end();
      if (print_colinfo) columns.erase(i);

      i = std::find(columns.begin(), columns.end(), CMD_ARG_BE_QUIET);
      const bool quiet = i != columns.end();
      if (quiet) columns.erase(i);

      std::ostream &out = get_stream_for_results(quiet);

      result = context.connection()->recv_result();
      print_result_set(*result, columns, value_callback, quiet);

      if (print_colinfo)
        print_columndata(*result->columnMetadata());

      variables_to_unreplace.clear();
      int64_t x = result->affectedRows();
      if (x >= 0)
        out << x << " rows affected\n";
      else
        out << "command ok\n";
      if (result->lastInsertId() > 0)
        out << "last insert id: " << result->lastInsertId() << "\n";
      if (!result->infoMessage().empty())
        out << result->infoMessage() << "\n";
      {
        std::vector<mysqlx::Result::Warning> warnings(result->getWarnings());
        if (!warnings.empty())
          out << "Warnings generated:\n";
        for (std::vector<mysqlx::Result::Warning>::const_iterator w = warnings.begin();
            w != warnings.end(); ++w)
        {
          out << (w->is_note ? "NOTE" : "WARNING") << " | " << w->code << " | " << w->text << "\n";
        }
      }

      if (!OPT_expect_error->check_ok())
        return Stop_with_failure;
    }
    catch (mysqlx::Error &err)
    {
      if (result.get())
        result->mark_error();
      if (!OPT_expect_error->check_error(err))
        return Stop_with_failure;
    }
    return Continue;
  }

  Result cmd_recvuntil(Execution_context &context, const std::string &args)
  {
    int msgid;

    std::vector<std::string> argl;

    aux::split(argl, args, " ", true);

    bool show = true, stop = false;

    if (argl.size() > 1)
    {
      const char *argument_do_not_print = argl[1].c_str();
      show = false;

      if (0 != strcmp(argument_do_not_print, "do_not_show_intermediate"))
      {
        std::cout << "Invalid argument received: " << argl[1] << "\n";
        return Stop_with_failure;
      }
    }

    Message_by_full_name::iterator iterator_msg_name = server_msgs_by_full_name.find(argl[0]);

    if (server_msgs_by_full_name.end() == iterator_msg_name)
    {
      std::cout << "Unknown message name: " << argl[0] << " " << server_msgs_by_full_name.size() << "\n";
      return Stop_with_failure;
    }

    Message_by_name::iterator iterator_msg_id = server_msgs_by_name.find(iterator_msg_name->second);

    if (server_msgs_by_name.end() == iterator_msg_id)
    {
      std::cout << "Invalid data in internal message list, entry not found:" << iterator_msg_name->second << "\n";
      return Stop_with_failure;
    }

    const int expected_msg_id = iterator_msg_id->second.second;

    do
    {
      Message_ptr msg(context.connection()->recv_raw(msgid));

      if (msg.get())
      {
        if (msg->GetDescriptor()->full_name() == argl[0] ||
            msgid == Mysqlx::ServerMessages::ERROR)
        {
          show = true;
          stop = true;
        }

        try
        {
          if (show)
            std::cout << formatter::message_to_text(*msg) << "\n";
        }
        catch (std::exception &e)
        {
          dumpx(e);
          if (OPT_fatal_errors)
            return Stop_with_success;
        }
      }
    }
    while (!stop);

    variables_to_unreplace.clear();

    if (Mysqlx::ServerMessages::ERROR == msgid &&
        Mysqlx::ServerMessages::ERROR != expected_msg_id)
      return Stop_with_failure;

    return Continue;
  }

  Result cmd_enablessl(Execution_context &context, const std::string &args)
  {
    try
    {
      context.connection()->enable_tls();
    }
    catch (const mysqlx::Error &err)
    {
      dumpx(err);
      return Stop_with_failure;
    }

    return Continue;
  }

  Result cmd_stmtsql(Execution_context &context, const std::string &args)
  {
    Mysqlx::Sql::StmtExecute stmt;

    std::string command = args;
    replace_variables(command);

    stmt.set_stmt(command);
    stmt.set_namespace_("sql");

    context.connection()->send(stmt);

    if (!OPT_quiet)
      std::cout << "RUN " << command << "\n";

    return Continue;
  }


  Result cmd_stmtadmin(Execution_context &context, const std::string &args)
  {
    std::string tmp = args;
    replace_variables(tmp);
    std::vector<std::string> params;
    aux::split(params, tmp, "\t", true);
    if (params.empty())
    {
      std::cerr << "Invalid empty admin command\n";
      return Stop_with_failure;
    }

    aux::trim(params[0]);

    Mysqlx::Sql::StmtExecute stmt;
    stmt.set_stmt(params[0]);
    stmt.set_namespace_("mysqlx");

    if (params.size() == 2)
    {
      Any obj;
      if (!json_string_to_any(params[1], obj))
      {
        std::cerr << "Invalid argument for '" << params[0] << "' command; json object expected\n";
        return Stop_with_failure;
      }
      stmt.add_args()->CopyFrom(obj);
    }

    context.connection()->send(stmt);

    return Continue;
  }


  bool json_string_to_any(const std::string &json_string, Any &any) const;


  Result cmd_sleep(Execution_context &context, const std::string &args)
  {
    std::string tmp = args;
    replace_variables(tmp);
    const double delay_in_seconds = ngs::stod(tmp);
#ifdef _WIN32
    const int delay_in_miliseconds = delay_in_seconds * 1000;
    Sleep(delay_in_miliseconds);
#else
    const int delay_in_ultraseconds = delay_in_seconds * 1000000;
    usleep(delay_in_ultraseconds);
#endif
    return Continue;
  }

  Result cmd_login(Execution_context &context, const std::string &args)
  {
    std::string user, pass, db, auth_meth;

    if (args.empty())
      context.m_cm->get_credentials(user, pass);
    else
    {
      std::string s = args;
      replace_variables(s);

      std::string::size_type p = s.find(CMD_ARG_SEPARATOR);
      if (p != std::string::npos)
      {
        user = s.substr(0, p);
        s = s.substr(p+1);
        p = s.find(CMD_ARG_SEPARATOR);
        if (p != std::string::npos)
        {
          pass = s.substr(0, p);
          s = s.substr(p+1);
          p = s.find(CMD_ARG_SEPARATOR);
          if (p != std::string::npos)
          {
            db = s.substr(0, p);
            auth_meth = s.substr(p+1);
          }
          else
            db = s;
        }
        else
          pass = s;
      }
      else
        user = s;
    }

    void (mysqlx::XProtocol::*method)(const std::string &, const std::string &, const std::string &);

    method = &mysqlx::XProtocol::authenticate_mysql41;

    try
    {
      context.connection()->push_local_notice_handler(ngs::bind(dump_notices, ngs::placeholders::_1, ngs::placeholders::_2));
      //XXX
      // Prepered for method map
      if (0 == strncmp(auth_meth.c_str(), "plain", 5))
      {
        method = &mysqlx::XProtocol::authenticate_plain;
      }
      else if ( !(0 == strncmp(auth_meth.c_str(), "mysql41", 5) || 0 == auth_meth.length()))
        throw mysqlx::Error(CR_UNKNOWN_ERROR, "Wrong authentication method");

      (context.connection()->*method)(user, pass, db);

      context.connection()->pop_local_notice_handler();

      std::cout << "Login OK\n";
    }
    catch (mysqlx::Error &err)
    {
      context.connection()->pop_local_notice_handler();
      if (!OPT_expect_error->check_error(err))
        return Stop_with_failure;
    }

    return Continue;
  }

  Result cmd_repeat(Execution_context &context, const std::string &args)
  {
    std::string variable_name = "";
    std::vector<std::string> argl;

    aux::split(argl, args, "\t", true);

    if (argl.size() > 1)
    {
      variable_name = argl[1];
    }

    // Allow use of variables as a source of number of iterations
    replace_variables(argl[0]);

    Loop_do loop = {context.m_stream.tellg(), ngs::stoi(argl[0]), 0, variable_name};

    m_loop_stack.push_back(loop);

    if (variable_name.length())
      variables[variable_name] = ngs::to_string(loop.value);

    return Continue;
  }

  Result cmd_endrepeat(Execution_context &context, const std::string &args)
  {
    while (m_loop_stack.size())
    {
      Loop_do &ld = m_loop_stack.back();

      --ld.iterations;
      ++ld.value;

      if (ld.variable_name.length())
        variables[ld.variable_name] = ngs::to_string(ld.value);

      if (1 > ld.iterations)
      {
        m_loop_stack.pop_back();
        break;
      }

      context.m_stream.seekg(ld.block_begin);
      break;
    }

    return Continue;
  }

  Result cmd_loginerror(Execution_context &context, const std::string &args)
  {
    std::string s = args;
    std::string expected, user, pass, db;
    int expected_error_code = 0;

    replace_variables(s);
    std::string::size_type p = s.find('\t');
    if (p != std::string::npos)
    {
      expected = s.substr(0, p);
      s = s.substr(p+1);
      p = s.find('\t');
      if (p != std::string::npos)
      {
        user = s.substr(0, p);
        s = s.substr(p+1);
        p = s.find('\t');
        if (p != std::string::npos)
        {
          pass = s.substr(0, p+1);
          db = s.substr(p+1);
        }
        else
          pass = s;
      }
      else
        user = s;
    }
    else
    {
      std::cout << error() << "Missing arguments to -->loginerror" << eoerr();
      return Stop_with_failure;
    }

    try
    {
      replace_variables(expected);
      aux::trim(expected);
      expected_error_code = mysqlxtest::get_error_code_by_text(expected);
      context.connection()->push_local_notice_handler(ngs::bind(dump_notices, ngs::placeholders::_1, ngs::placeholders::_2));

      context.connection()->authenticate_mysql41(user, pass, db);

      context.connection()->pop_local_notice_handler();

      std::cout << error() << "Login succeeded, but an error was expected" << eoerr();
      if (OPT_fatal_errors)
        return Stop_with_failure;
    }
    catch (const std::exception &e)
    {
      std::cerr << e.what() << "\n";

      return Stop_with_failure;
    }
    catch (mysqlx::Error &err)
    {
      context.connection()->pop_local_notice_handler();

      if (err.error() == expected_error_code)
        std::cerr << "error (as expected): " << err.what() << " (code " << err.error() << ")\n";
      else
      {
        std::cerr << error() << "was expecting: " << expected_error_code << " but got: " << err.what() << " (code " << err.error() << ")" << eoerr();
        if (OPT_fatal_errors)
          return Stop_with_failure;
      }
    }

    return Continue;
  }

  Result cmd_system(Execution_context &context, const std::string &args)
  {
    // command used only at dev level
    // example of usage
    // -->system (sleep 3; echo "Killing"; ps aux | grep mysqld | egrep -v "gdb .+mysqld" | grep -v  "kdeinit4"| awk '{print($2)}' | xargs kill -s SIGQUIT)&
    if (0 == system(args.c_str()))
      return Continue;

    return Stop_with_failure;
  }

  Result cmd_recv_all_until_disc(Execution_context &context, const std::string &args)
  {
    int msgid;
    try
    {
      while(true)
      {
        Message_ptr msg(context.connection()->recv_raw(msgid));

        //TODO:
        // For now this command will be used in places where random messages
        // can reach mysqlxtest in different mtr rans
        // the random behavior of server in such cases should be fixed
        //if (msg.get())
        //  std::cout << unreplace_variables(message_to_text(*msg), true) << "\n";
      }
    }
    catch (mysqlx::Error&)
    {
      std::cerr << "Server disconnected\n";
    }

    if (context.m_cm->is_default_active())
      return Stop_with_success;

    context.m_cm->active()->set_closed();
    context.m_cm->close_active(false);

    return Continue;
  }

  Result cmd_peerdisc(Execution_context &context, const std::string &args)
  {
    int expected_delta_time;
    int tolerance;
    int result = sscanf(args.c_str(),"%i %i", &expected_delta_time, &tolerance);

    if (result <1 || result > 2)
    {
      std::cerr << "ERROR: Invalid use of command\n";

      return Stop_with_failure;
    }

    if (1 == result)
    {
      tolerance = 10 * expected_delta_time / 100;
    }

    ngs::chrono::time_point start_time = ngs::chrono::now();
    try
    {
      int msgid;

      Message_ptr msg(context.connection()->recv_raw_with_deadline(msgid, 2 * expected_delta_time));

      if (msg.get())
      {
        std::cerr << "ERROR: Received unexpected message.\n";
        std::cerr << formatter::message_to_text(*msg) << "\n";
      }
      else
      {
        std::cerr << "ERROR: Timeout occur while waiting for disconnection.\n";
      }

      return Stop_with_failure;
    }
    catch (const mysqlx::Error &ec)
    {
      if (CR_SERVER_GONE_ERROR != ec.error())
      {
        dumpx(ec);
        return Stop_with_failure;
      }
    }

    int execution_delta_time = ngs::chrono::to_milliseconds(ngs::chrono::now() - start_time);

    if (abs(execution_delta_time - expected_delta_time) > tolerance)
    {
      std::cerr << "ERROR: Peer disconnected after: "<< execution_delta_time << "[ms], expected: " << expected_delta_time << "[ms]\n";
      return Stop_with_failure;
    }

    context.m_cm->active()->set_closed();

    if (context.m_cm->is_default_active())
      return Stop_with_success;

    context.m_cm->close_active(false);

    return Continue;
  }

  Result cmd_recv(Execution_context &context, const std::string &args)
  {
    int msgid;
    bool quiet = false;
    std::string args_copy(args);

    aux::trim(args_copy);
    if (args_copy == "quiet") {
      quiet = true;
      args_copy = "";
    }

    try
    {
      Message_ptr msg(context.connection()->recv_raw(msgid));

      std::ostream &out = get_stream_for_results(quiet);

      if (msg.get())
        out << unreplace_variables(formatter::message_to_text(*msg, args_copy), true) << "\n";
      if (!OPT_expect_error->check_ok())
        return Stop_with_failure;
    }
    catch (mysqlx::Error &e)
    {
      if (!quiet && !OPT_expect_error->check_error(e)) //TODO do we need this !quiet ?
        return Stop_with_failure;
    }
    catch (std::exception &e)
    {
      std::cerr << "ERROR: "<< e.what()<<"\n";
      if (OPT_fatal_errors)
        return Stop_with_failure;
    }
    return Continue;
  }

  Result cmd_exit(Execution_context &context, const std::string &args)
  {
    return Stop_with_success;
  }

  Result cmd_abort(Execution_context &context, const std::string &args)
  {
    exit(2);
    return Stop_with_success;
  }

  Result cmd_nowarnings(Execution_context &context, const std::string &args)
  {
    OPT_show_warnings = false;
    return Continue;
  }

  Result cmd_yeswarnings(Execution_context &context, const std::string &args)
  {
    OPT_show_warnings = true;
    return Continue;
  }

  Result cmd_fatalerrors(Execution_context &context, const std::string &args)
  {
    OPT_fatal_errors = true;
    return Continue;
  }

  Result cmd_nofatalerrors(Execution_context &context, const std::string &args)
  {
    OPT_fatal_errors = false;
    return Continue;
  }

  Result cmd_newsessionplain(Execution_context &context, const std::string &args)
  {
    return do_newsession(context, args, true);
  }

  Result cmd_newsession(Execution_context &context, const std::string &args)
  {
    return do_newsession(context, args, false);
  }

  Result do_newsession(Execution_context &context, const std::string &args, bool plain)
  {
    std::string s = args;
    std::string user, pass, db, name;

    replace_variables(s);

    std::string::size_type p = s.find(CMD_ARG_SEPARATOR);

    if (p != std::string::npos)
    {
      name = s.substr(0, p);
      s = s.substr(p+1);
      p = s.find(CMD_ARG_SEPARATOR);
      if (p != std::string::npos)
      {
        user = s.substr(0, p);
        s = s.substr(p+1);
        p = s.find(CMD_ARG_SEPARATOR);
        if (p != std::string::npos)
        {
          pass = s.substr(0, p);
          db = s.substr(p+1);
        }
        else
          pass = s;
      }
      else
        user = s;
    }
    else
      name = s;

    try
    {
      context.m_cm->create(name, user, pass, db, plain);
      if (!OPT_expect_error->check_ok())
        return Stop_with_failure;
    }
    catch (mysqlx::Error &err)
    {
      if (!OPT_expect_error->check_error(err))
        return Stop_with_failure;
    }

    return Continue;
  }

  Result cmd_setsession(Execution_context &context, const std::string &args)
  {
    std::string s = args;

    replace_variables(s);

    if (!s.empty() && (s[0] == ' ' || s[0] == '\t'))
      context.m_cm->set_active(s.substr(1));
    else
      context.m_cm->set_active(s);
    return Continue;
  }

  Result cmd_closesession(Execution_context &context, const std::string &args)
  {
    try
    {
      if (args == " abort")
        context.m_cm->abort_active();
      else
        context.m_cm->close_active();
      if (!OPT_expect_error->check_ok())
        return Stop_with_failure;
    }
    catch (mysqlx::Error &err)
    {
      if (!OPT_expect_error->check_error(err))
        return Stop_with_failure;
    }
    return Continue;
  }

  Result cmd_expecterror(Execution_context &context, const std::string &args)
  {
    try
    {
      if (args.empty())
        throw std::logic_error("expecterror requires an errno argument");

      std::vector<std::string> argl;
      aux::split(argl, args, ",", true);
      for (std::vector<std::string>::const_iterator arg = argl.begin(); arg != argl.end(); ++arg)
      {
        std::string value = *arg;

        replace_variables(value);
        aux::trim(value);

        const int error_code = mysqlxtest::get_error_code_by_text(value);

        OPT_expect_error->expect_errno(error_code);
      }
    }
    catch(const std::exception &e)
    {
      std::cerr << e.what() << "\n";

      return Stop_with_failure;
    }

    return Continue;
  }


  static ngs::chrono::time_point m_start_measure;

  Result cmd_measure(Execution_context &context, const std::string &args)
  {
    m_start_measure = ngs::chrono::now();
    return Continue;
  }

  Result cmd_endmeasure(Execution_context &context, const std::string &args)
  {
    if (!ngs::chrono::is_valid(m_start_measure))
    {
      std::cerr << "Time measurement, wasn't initialized\n";
      return Stop_with_failure;
    }

    std::vector<std::string> argl;
    aux::split(argl, args, " ", true);
    if (argl.size() != 2 && argl.size() != 1)
    {
      std::cerr << "Invalid number of arguments for command endmeasure\n";
      return Stop_with_failure;
    }

    const int64_t expected_msec = ngs::stoi(argl[0]);
    const int64_t msec = ngs::chrono::to_milliseconds(ngs::chrono::now() - m_start_measure);

    int64_t tolerance = expected_msec * 10 / 100;

    if (2 == argl.size())
      tolerance = ngs::stoi(argl[1]);

    if (abs(expected_msec - msec) > tolerance)
    {
      std::cerr << "Timeout should occur after " << expected_msec << "ms, but it was " << msec <<"ms.  \n";
      return Stop_with_failure;
    }

    m_start_measure = ngs::chrono::time_point();
    return Continue;
  }

  Result cmd_quiet(Execution_context &context, const std::string &args)
  {
    OPT_quiet = true;

    return Continue;
  }

  Result cmd_noquiet(Execution_context &context, const std::string &args)
  {
    OPT_quiet = false;

    return Continue;
  }

  Result cmd_varsub(Execution_context &context, const std::string &args)
  {
    variables_to_unreplace.push_back(args);
    return Continue;
  }

  Result cmd_varlet(Execution_context &context, const std::string &args)
  {
    std::string::size_type p = args.find(' ');
    if (p == std::string::npos)
    {
      variables[args] = "";
    }
    else
    {
      std::string value = args.substr(p+1);
      replace_variables(value);
      variables[args.substr(0, p)] = value;
    }
    return Continue;
  }

  Result cmd_varinc(Execution_context &context, const std::string &args)
  {
    std::vector<std::string> argl;
    aux::split(argl, args, " ", true);
    if (argl.size() != 2)
    {
      std::cerr << "Invalid number of arguments for command varinc\n";
      return Stop_with_failure;
    }

    if (variables.find(argl[0]) == variables.end())
    {
      std::cerr << "Invalid variable " << argl[0] << "\n";
      return Stop_with_failure;
    }

    std::string val = variables[argl[0]];
    char* c;
    std::string inc_by = argl[1].c_str();

    replace_variables(inc_by);

    long int_val = strtol(val.c_str(), &c, 10);
    long int_n = strtol(inc_by.c_str(), &c, 10);
    int_val += int_n;
    val = ngs::to_string(int_val);
    variables[argl[0]] = val;

    return Continue;
  }

  Result cmd_vargen(Execution_context &context, const std::string &args)
  {
    std::vector<std::string> argl;
    aux::split(argl, args, " ", true);
    if (argl.size() != 3)
    {
      std::cerr << "Invalid number of arguments for command vargen\n";
      return Stop_with_failure;
    }
    std::string data(ngs::stoi(argl[2]), *argl[1].c_str());
    variables[argl[0]] = data;
    return Continue;
  }

  Result cmd_varfile(Execution_context &context, const std::string &args)
  {
    std::vector<std::string> argl;
    aux::split(argl, args, " ", true);
    if (argl.size() != 2)
    {
      std::cerr << "Invalid number of arguments for command varfile " << args << "\n";
      return Stop_with_failure;
    }

    std::string path_to_file = argl[1];
    replace_variables(path_to_file);

    std::ifstream file(path_to_file.c_str());
    if (!file.is_open())
    {
      std::cerr << "Couldn't not open file " << path_to_file <<"\n";
      return Stop_with_failure;
    }

    file.seekg(0, file.end);
    size_t len = file.tellg();
    file.seekg(0);

    char *buffer = new char[len];
    file.read(buffer, len);
    variables[argl[0]] = std::string(buffer, len);
    delete []buffer;

    return Continue;
  }

  Result cmd_binsend(Execution_context &context, const std::string &args)
  {
    std::string args_copy = args;
    replace_variables(args_copy);
    std::string data = bindump_to_data(args_copy);

    std::cout << "Sending " << data.length() << " bytes raw data...\n";
    context.m_cm->active()->send_bytes(data);
    return Continue;
  }

  Result cmd_hexsend(Execution_context &context, const std::string &args)
  {
    std::string args_copy = args;
    replace_variables(args_copy);

    if (0 == args_copy.length())
    {
      std::cerr << "Data should not be present\n";
      return Stop_with_failure;
    }

    if (0 != args_copy.length() % 2)
    {
      std::cerr << "Size of data should be a multiplication of two, current length:" << args_copy.length()<<"\n";
      return Stop_with_failure;
    }

    std::string data;
    try
    {
      aux::unhex(args_copy, data);
    }
    catch(const std::exception&)
    {
      std::cerr << "Hex string is invalid\n";
      return Stop_with_failure;
    }

    std::cout << "Sending " << data.length() << " bytes raw data...\n";
    context.m_cm->active()->send_bytes(data);
    return Continue;
  }

  size_t value_to_offset(const std::string &data, const size_t maximum_value)
  {
    if ('%' == *data.rbegin())
    {
      size_t percent = ngs::stoi(data);

      return maximum_value * percent / 100;
    }

    return ngs::stoi(data);
  }

  Result cmd_binsendoffset(Execution_context &context, const std::string &args)
  {
    std::string args_copy = args;
    replace_variables(args_copy);

    std::vector<std::string> argl;
    aux::split(argl, args_copy, " ", true);

    size_t begin_bin = 0;
    size_t end_bin = 0;
    std::string data;

    try
    {
      data = bindump_to_data(argl[0]);
      end_bin = data.length();

      if (argl.size() > 1)
      {
        begin_bin = value_to_offset(argl[1], data.length());
        if (argl.size() > 2)
        {
          end_bin = value_to_offset(argl[2], data.length());

          if (argl.size() > 3)
            throw std::out_of_range("Too many arguments");
        }
      }
    }
    catch (const std::out_of_range&)
    {
      std::cerr << "Invalid number of arguments for command binsendoffset:" << argl.size() << "\n";
      return Stop_with_failure;
    }

    std::cout << "Sending " << end_bin << " bytes raw data...\n";
    context.m_cm->active()->send_bytes(data.substr(begin_bin, end_bin - begin_bin));
    return Continue;
  }

  Result cmd_callmacro(Execution_context &context, const std::string &args)
  {
    if (Macro::call(context, args))
      return Continue;
    return Stop_with_failure;
  }

  Result cmd_assert_eq(Execution_context &context, const std::string &args)
  {
    std::vector<std::string> vargs;

    aux::split(vargs, args, "\t", true);

    if (2 != vargs.size())
    {
      std::cerr << "Specified invalid number of arguments for command assert_eq:" << vargs.size() << " expecting 2\n";
      return Stop_with_failure;
    }

    replace_variables(vargs[0]);
    replace_variables(vargs[1]);

    if (vargs[0] != vargs[1])
    {
      std::cerr << "Expecting '" << vargs[0] << "', but received '" << vargs[1] << "'\n";
      return Stop_with_failure;
    }

    return Continue;
  }

  Result cmd_assert_gt(Execution_context &context, const std::string &args)
  {
    std::vector<std::string> vargs;

    aux::split(vargs, args, "\t", true);

    if (2 != vargs.size())
    {
      std::cerr << "Specified invalid number of arguments for command assert_gt:" << vargs.size() << " expecting 2\n";
      return Stop_with_failure;
    }

    replace_variables(vargs[0]);
    replace_variables(vargs[1]);

    if (ngs::stoi(vargs[0]) <= ngs::stoi(vargs[1]))
    {
      std::cerr << "Expecting '" << vargs[0] << "' to be greater than '" << vargs[1] << "'\n";
      return Stop_with_failure;
    }

    return Continue;
  }

  Result cmd_assert_ge(Execution_context &context, const std::string &args)
  {
    std::vector<std::string> vargs;
    char *end_string = NULL;

    aux::split(vargs, args, "\t", true);

    if (2 != vargs.size())
    {
      std::cerr << "Specified invalid number of arguments for command assert_gt:" << vargs.size() << " expecting 2\n";
      return Stop_with_failure;
    }

    replace_variables(vargs[0]);
    replace_variables(vargs[1]);

    if (strtoll(vargs[0].c_str(), &end_string, 10) < strtoll(vargs[1].c_str(), &end_string, 10))
    {
      std::cerr << "assert_gt(" << args << ") failed!\n";
      std::cerr << "Expecting '" << vargs[0] << "' to be greater or equal to '" << vargs[1] << "'\n";
      return Stop_with_failure;
    }

    return Continue;
  }

  Result cmd_query(Execution_context &context, const std::string &args)
  {
    OPT_query = true;
    return Continue;
  }

  Result cmd_noquery(Execution_context &context, const std::string &args)
  {
    OPT_query = false;
    return Continue;
  }

  static void put_variable_to(std::string &result, const std::string &value)
  {
    result = value;
  }

  static void try_result(Result result)
  {
    if (result != Continue)
      throw result;
  }

  template <typename T>
  class Backup_and_restore
  {
  public:
    Backup_and_restore(T &variable, const T &temporaru_value)
    : m_variable(variable), m_value(variable)
    {
      m_variable = temporaru_value;
    }

    ~Backup_and_restore()
    {
      m_variable = m_value;
    }

  private:
    T &m_variable;
    T m_value;
  };

  Result cmd_wait_for(Execution_context &context, const std::string &args)
  {
    bool match = false;
    const int countdown_start_value = 30;
    int  countdown_retries = countdown_start_value;

    std::string args_variables_replaced = args;
    std::vector<std::string> vargs;

    replace_variables(args_variables_replaced);
    aux::split(vargs, args_variables_replaced, "\t", true);

    if (2 != vargs.size())
    {
      std::cerr << "Specified invalid number of arguments for command wait_for:" << vargs.size() << " expecting 2\n";
      return Stop_with_failure;
    }

    const std::string &expected_value = vargs[0];
    std::string value;

    try
    {
      do
      {
        Backup_and_restore<bool>        backup_and_restore_fatal_errors(OPT_fatal_errors, true);
        Backup_and_restore<bool>        backup_and_restore_query(OPT_query, false);
        Backup_and_restore<std::string> backup_and_restore_command_name(context.m_command_name, "sql");

        try_result(cmd_stmtsql(context, vargs[1]));
        try_result(cmd_recvresult(context, "", ngs::bind(&Command::put_variable_to, ngs::ref(value), ngs::placeholders::_1)));
        try_result(cmd_sleep(context,"1"));

        match = (value == expected_value);
      }
      while(!match && --countdown_retries);
    }
    catch(const Result result)
    {
      std::cerr << "'Wait_for' failed because one of subsequent commands failed\n";
      return  result;
    }

    if (!match)
    {
      std::cerr << "Query didn't return expected value, tried " << countdown_start_value << " times\n";
      std::cerr << "Expected '" << expected_value << "', received '" << value << "'\n";
      return Stop_with_failure;
    }

    return Continue;
  }

  Result cmd_import(Execution_context &context, const std::string &args);

  Result cmd_received(Execution_context &context, const std::string &args)
  {
    std::string cargs(args);
    std::vector<std::string> vargs;
    aux::split(vargs, cargs, " \t", true);
    replace_variables(vargs[0]);

    if (2 != vargs.size())
    {
      std::cerr << "Specified invalid number of arguments for command received:"
                << vargs.size() << " expecting 2\n";
      return Stop_with_failure;
    }

    set_variable(vargs[1],
                 ngs::to_string(
                     context.connection()->get_received_msg_counter(vargs[0])));
    return Continue;
  }
};

ngs::chrono::time_point Command::m_start_measure;

static int process_client_message(mysqlx::XProtocol *connection, int8_t msg_id, const mysqlx::Message &msg)
{
  if (!OPT_quiet)
    std::cout << "send " << formatter::message_to_text(msg) << "\n";

  if (OPT_bindump)
    std::cout << message_to_bindump(msg) << "\n";

  try
  {
    // send request
    connection->send(msg_id, msg);

    if (!OPT_expect_error->check_ok())
      return 1;
  }
  catch (mysqlx::Error &err)
  {
    if (!OPT_expect_error->check_error(err))
      return 1;
  }
  return 0;
}

static void print_result_set(mysqlx::Result &result)
{
  std::vector<std::string> empty_column_array_print_all;

  print_result_set(result, empty_column_array_print_all);
}

template<typename T>
std::string get_object_value(const T &value)
{
  std::stringstream result;
  result << value;

  return result.str();
}

std::string get_field_value(ngs::shared_ptr<mysqlx::Row> &row, const int field, ngs::shared_ptr<std::vector<mysqlx::ColumnMetadata> > &meta)
{
  if (row->isNullField(field))
  {
    return "null";
  }

  try
  {
    const mysqlx::ColumnMetadata &col(meta->at(field));

    switch (col.type)
    {
    case mysqlx::SINT:
      return ngs::to_string(row->sInt64Field(field));

    case mysqlx::UINT:
      return ngs::to_string(row->uInt64Field(field));

    case mysqlx::DOUBLE:
      if (col.fractional_digits < 31)
      {
        char buffer[100];
        my_fcvt(row->doubleField(field), col.fractional_digits, buffer, NULL);
        return buffer;
      }
      return ngs::to_string(row->doubleField(field));

    case mysqlx::FLOAT:
      if (col.fractional_digits < 31)
      {
        char buffer[100];
        my_fcvt(row->floatField(field), col.fractional_digits, buffer, NULL);
        return buffer;
      }
      return ngs::to_string(row->floatField(field));

    case mysqlx::BYTES:
    {
      std::string tmp(row->stringField(field));
      return unreplace_variables(tmp, false);
    }

    case mysqlx::TIME:
      return get_object_value(row->timeField(field));

    case mysqlx::DATETIME:
      return get_object_value(row->dateTimeField(field));

    case mysqlx::DECIMAL:
      return row->decimalField(field);

    case mysqlx::SET:
      return row->setFieldStr(field);

    case mysqlx::ENUM:
      return row->enumField(field);

    case mysqlx::BIT:
      return get_object_value(row->bitField(field));
    }
  }
  catch (std::exception &e)
  {
    std::cout << "ERROR: " << e.what() << "\n";
  }

  return "";
}


namespace
{

inline std::string get_typename(const mysqlx::FieldType& field)
{
  switch (field)
  {
  case mysqlx::SINT:
    return "SINT";
  case mysqlx::UINT:
    return "UINT";
  case mysqlx::DOUBLE:
    return "DOUBLE";
  case mysqlx::FLOAT:
    return "FLOAT";
  case mysqlx::BYTES:
    return "BYTES";
  case mysqlx::TIME:
    return "TIME";
  case mysqlx::DATETIME:
    return "DATETIME";
  case mysqlx::SET:
    return "SET";
  case mysqlx::ENUM:
    return "ENUM";
  case mysqlx::BIT:
    return "BIT";
  case mysqlx::DECIMAL:
    return "DECIMAL";
  }
  return "UNKNOWN";
}


inline std::string get_flags(const mysqlx::FieldType& field, uint32_t flags)
{
  std::string r;

  if (flags & MYSQLX_COLUMN_FLAGS_UINT_ZEROFILL) // and other equal 1
  {
    switch (field)
    {
    case mysqlx::SINT:
    case mysqlx::UINT:
      r += " ZEROFILL";
      break;

    case mysqlx::DOUBLE:
    case mysqlx::FLOAT:
    case mysqlx::DECIMAL:
      r += " UNSIGNED";
      break;

    case mysqlx::BYTES:
      r += " RIGHTPAD";
      break;

    case mysqlx::DATETIME:
      r += " TIMESTAMP";
      break;

    default:
      ;
    }
  }
  if (flags & MYSQLX_COLUMN_FLAGS_NOT_NULL)
    r += " NOT_NULL";

  if (flags & MYSQLX_COLUMN_FLAGS_PRIMARY_KEY)
    r += " PRIMARY_KEY";

  if (flags & MYSQLX_COLUMN_FLAGS_UNIQUE_KEY)
    r += " UNIQUE_KEY";

  if (flags & MYSQLX_COLUMN_FLAGS_MULTIPLE_KEY)
    r += " MULTIPLE_KEY";

  if (flags & MYSQLX_COLUMN_FLAGS_AUTO_INCREMENT)
    r += " AUTO_INCREMENT";

  return r;
}


} // namespace


static void print_columndata(const std::vector<mysqlx::ColumnMetadata> &meta)
{
  for (std::vector<mysqlx::ColumnMetadata>::const_iterator col = meta.begin(); col != meta.end(); ++col)
  {
    std::cout << col->name << ":" << get_typename(col->type) << ':'
              << get_flags(col->type, col->flags) << '\n';
  }
}

static void print_result_set(mysqlx::Result &result, const std::vector<std::string> &columns,
                             Value_callback value_callback, bool quiet)
{
  ngs::shared_ptr<std::vector<mysqlx::ColumnMetadata> > meta(result.columnMetadata());
  std::vector<int> column_indexes;
  int column_index = -1;
  bool first = true;

  std::ostream &out = get_stream_for_results(quiet);

  for (std::vector<mysqlx::ColumnMetadata>::const_iterator col = meta->begin();
      col != meta->end(); ++col)
  {
    ++column_index;

    if (!first)
      out << "\t";
    else
      first = false;

    if (!columns.empty() && columns.end() == std::find(columns.begin(), columns.end(), col->name))
      continue;

    column_indexes.push_back(column_index);
    out << col->name;
  }
  out << "\n";

  for (;;)
  {
    ngs::shared_ptr<mysqlx::Row> row(result.next());
    if (!row.get())
      break;

    std::vector<int>::iterator i = column_indexes.begin();
    for (; i != column_indexes.end() && (*i) < row->numFields(); ++i)
    {
      int field = (*i);
      if (field != 0)
        out << "\t";

      std::string result = get_field_value(row, field, meta);

      if (value_callback)
      {
        value_callback(result);
        Value_callback().swap(value_callback);
      }
      out << result;
    }
    out << "\n";
  }
}

static int run_sql_batch(mysqlx::XProtocol *conn, const std::string &sql_)
{
  std::string delimiter = ";";
  std::vector<std::pair<size_t, size_t> > ranges;
  std::stack<std::string> input_context_stack;
  std::string sql = sql_;

  replace_variables(sql);

  shcore::mysql::splitter::determineStatementRanges(sql.data(), sql.length(), delimiter,
                                                    ranges, "\n", input_context_stack);

  for (std::vector<std::pair<size_t, size_t> >::const_iterator st = ranges.begin(); st != ranges.end(); ++st)
  {
    try
    {
      if (!OPT_quiet)
        std::cout << "RUN " << sql.substr(st->first, st->second) << "\n";
      ngs::shared_ptr<mysqlx::Result> result(conn->execute_sql(sql.substr(st->first, st->second)));
      if (result.get())
      {
        do
        {
          print_result_set(*result.get());
        } while (result->nextDataSet());

        int64_t x = result->affectedRows();
        if (x >= 0)
          std::cout << x << " rows affected\n";
        if (result->lastInsertId() > 0)
          std::cout << "last insert id: " << result->lastInsertId() << "\n";
        if (!result->infoMessage().empty())
          std::cout << result->infoMessage() << "\n";

        if (OPT_show_warnings)
        {
          std::vector<mysqlx::Result::Warning> warnings(result->getWarnings());
          if (!warnings.empty())
            std::cout << "Warnings generated:\n";
          for (std::vector<mysqlx::Result::Warning>::const_iterator w = warnings.begin();
              w != warnings.end(); ++w)
          {
            std::cout << (w->is_note ? "NOTE" : "WARNING") << " | " << w->code << " | " << w->text << "\n";
          }
        }
      }
    }
    catch (mysqlx::Error &err)
    {
      variables_to_unreplace.clear();

      std::cerr << "While executing " << sql.substr(st->first, st->second) << ":\n";
      if (!OPT_expect_error->check_error(err))
        return 1;
    }
  }
  variables_to_unreplace.clear();
  return 0;
}

enum Block_result
{
  Block_result_feed_more,
  Block_result_eated_but_not_hungry,
  Block_result_not_hungry,
  Block_result_indigestion,
  Block_result_everyone_not_hungry
};

class Block_processor
{
public:
  virtual ~Block_processor() {}

  virtual Block_result feed(std::istream &input, const char *linebuf) = 0;
  virtual bool feed_ended_is_state_ok() { return true; }
};

typedef ngs::shared_ptr<Block_processor> Block_processor_ptr;

class Sql_block_processor : public Block_processor
{
public:
  Sql_block_processor(Connection_manager *cm)
  : m_cm(cm), m_sql(false)
  { }

  virtual Block_result feed(std::istream &input, const char *linebuf)
  {
    if (m_sql)
    {
      if (strcmp(linebuf, "-->endsql") == 0)
      {
        {
          int r = run_sql_batch(m_cm->active(), m_rawbuffer);
          if (r != 0)
          {
            return Block_result_indigestion;
          }
        }
        m_sql = false;

        return Block_result_eated_but_not_hungry;
      }
      else
        m_rawbuffer.append(linebuf).append("\n");

      return Block_result_feed_more;
    }

    // -->command
    if (strcmp(linebuf, "-->sql") == 0)
    {
      m_rawbuffer.clear();
      m_sql = true;
      // feed everything until -->endraw to the mysql client

      return Block_result_feed_more;
    }

    return Block_result_not_hungry;
  }

  virtual bool feed_ended_is_state_ok()
  {
    if (m_sql)
    {
      std::cerr << error() << "Unclosed -->sql directive" << eoerr();
      return false;
    }

    return true;
  }

private:
  Connection_manager *m_cm;
  std::string m_rawbuffer;
  bool m_sql;
};

class Macro_block_processor : public Block_processor
{
public:
  Macro_block_processor(Connection_manager *cm)
  : m_cm(cm)
  { }

  ~Macro_block_processor()
  {
  }

  virtual Block_result feed(std::istream &input, const char *linebuf)
  {
    if (m_macro)
    {
      if (strcmp(linebuf, "-->endmacro") == 0)
      {
        m_macro->set_body(m_rawbuffer);

        Macro::add(m_macro);
        if (OPT_verbose)
          std::cout << "Macro " << m_macro->name() << " defined\n";

        m_macro.reset();

        return Block_result_eated_but_not_hungry;
      }
      else
        m_rawbuffer.append(linebuf).append("\n");

      return Block_result_feed_more;
    }

    // -->command
    const char *cmd = "-->macro ";
    if (strncmp(linebuf, cmd, strlen(cmd)) == 0)
    {
      std::list<std::string> args;
      std::string t(linebuf+strlen(cmd));
      aux::split(args, t, " \t", true);

      if (args.empty())
      {
        std::cerr << error() << "Missing macro name argument for -->macro" << eoerr();
        return Block_result_indigestion;
      }

      m_rawbuffer.clear();
      std::string name = args.front();
      args.pop_front();
      m_macro.reset(new Macro(name, args));

      return Block_result_feed_more;
    }

    return Block_result_not_hungry;
  }

  virtual bool feed_ended_is_state_ok()
  {
    if (m_macro)
    {
      std::cerr << error() << "Unclosed -->macro directive" << eoerr();
      return false;
    }

    return true;
  }

private:
  Connection_manager *m_cm;
  ngs::shared_ptr<Macro> m_macro;
  std::string m_rawbuffer;
};

class Single_command_processor: public Block_processor
{
public:
  Single_command_processor(Connection_manager *cm)
  : m_cm(cm)
  { }

  virtual Block_result feed(std::istream &input, const char *linebuf)
  {
    Execution_context context(input, m_cm);

    if (m_command.is_command_syntax(linebuf))
    {
      {
        Command::Result r = m_command.process(context, linebuf);
        if (Command::Stop_with_failure == r)
          return Block_result_indigestion;
        else if (Command::Stop_with_success == r)
          return Block_result_everyone_not_hungry;
      }

      return Block_result_eated_but_not_hungry;
    }
    // # comment
    else if (linebuf[0] == '#' || linebuf[0] == 0)
    {
      return Block_result_eated_but_not_hungry;
    }

    return Block_result_not_hungry;
  }

private:
  Command m_command;
  Connection_manager *m_cm;
};

class Snd_message_block_processor: public Block_processor
{
public:
  Snd_message_block_processor(Connection_manager *cm)
  : m_cm(cm)
  { }

  virtual Block_result feed(std::istream &input, const char *linebuf)
  {
    if (m_full_name.empty())
    {
      if (!(m_full_name = get_message_name(linebuf)).empty())
      {
        m_buffer.clear();
        return Block_result_feed_more;
      }
    }
    else
    {
      if (linebuf[0] == '}')
      {
        int8_t msg_id;
        std::string processed_buffer = m_buffer;
        replace_variables(processed_buffer);

        Message_ptr msg(text_to_client_message(m_full_name, processed_buffer, msg_id));

        m_full_name.clear();
        if (!msg.get())
          return Block_result_indigestion;

        {
          int r = process(msg_id, *msg.get());

          if (r != 0)
            return Block_result_indigestion;
        }

        return Block_result_eated_but_not_hungry;
      }
      else
      {
        m_buffer.append(linebuf).append("\n");
        return Block_result_feed_more;
      }
    }

    return Block_result_not_hungry;
  }

  virtual bool feed_ended_is_state_ok()
  {
    if (!m_full_name.empty())
    {
      std::cerr << error() << "Incomplete message " << m_full_name << eoerr();
      return false;
    }

    return true;
  }

private:
  virtual std::string get_message_name(const char *linebuf)
  {
    const char *p;
    if ((p = strstr(linebuf, " {")))
    {
      return std::string(linebuf, p-linebuf);
    }

    return "";
  }

  virtual int process(const int8_t msg_id, mysqlx::Message &message)
  {
    return process_client_message(m_cm->active(), msg_id, message);
  }

  Connection_manager *m_cm;
  std::string m_buffer;
  std::string m_full_name;
};

class Dump_message_block_processor: public Snd_message_block_processor
{
public:
  Dump_message_block_processor(Connection_manager *cm)
  : Snd_message_block_processor(cm)
  { }

private:
  virtual std::string get_message_name(const char *linebuf)
  {
    const char *command_dump = "-->binparse";
    std::vector<std::string> args;

    aux::split(args, linebuf, " ", true);

    if (4 != args.size())
      return "";

    if (args[0] == command_dump && args[3] == "{")
    {
      m_variable_name = args[1];
      return args[2];
    }

    return "";
  }

  virtual int process(const int8_t msg_id, mysqlx::Message &message)
  {
    std::string bin_message = message_to_bindump(message);

    variables[m_variable_name] = bin_message;

    return 0;
  }

  std::string m_variable_name;
};

static int process_client_input(std::istream &input, std::vector<Block_processor_ptr> &eaters)
{
  const std::size_t buffer_length = 64*1024 + 1024;
  char              linebuf[buffer_length + 1];

  linebuf[buffer_length] = 0;

  if (!input.good())
  {
    std::cerr << "Input stream isn't valid\n";

    return 1;
  }

  Block_processor_ptr hungry_block_reader;

  while (!input.eof())
  {
    Block_result result = Block_result_not_hungry;

    input.getline(linebuf, buffer_length);
    script_stack.front().line_number++;

    if (!hungry_block_reader)
    {
      std::vector<Block_processor_ptr>::iterator i = eaters.begin();

      while (i != eaters.end() &&
          Block_result_not_hungry == result)
      {
        result = (*i)->feed(input, linebuf);

        if (Block_result_indigestion == result)
          return 1;

        if (Block_result_feed_more == result)
          hungry_block_reader = (*i);

        ++i;
      }

      if (Block_result_everyone_not_hungry == result)
        break;

      continue;
    }

    result = hungry_block_reader->feed(input, linebuf);

    if (Block_result_indigestion == result)
      return 1;

    if (Block_result_feed_more != result)
      hungry_block_reader.reset();

    if (Block_result_everyone_not_hungry == result)
      break;
  }

  std::vector<Block_processor_ptr>::iterator i = eaters.begin();

  while (i != eaters.end())
  {
    if (!(*i)->feed_ended_is_state_ok())
      return 1;

    ++i;
  }

  return 0;
}

#include "cmdline_options.h"

class My_command_line_options : public Command_line_options
{
public:
  enum Run_mode{
    RunTest,
    RunTestWithoutAuth
  } run_mode;

  std::string run_file;
  bool        has_file;
  bool        cap_expired_password;
  bool        dont_wait_for_server_disconnect;
  bool        use_plain_auth;

  mysqlx::Internet_protocol ip_mode;
  int timeout;
  Connection_options connection;

  std::string uri;
  mysqlx::Ssl_config ssl;
  bool        daemon;
  std::string sql;

  void print_version()
  {
    printf("%s  Ver %s Distrib %s, for %s (%s)\n", my_progname, MYSQLXTEST_VERSION,
        MYSQL_SERVER_VERSION, SYSTEM_TYPE, MACHINE_TYPE);
  }

  void print_help()
  {
    std::cout << "mysqlxtest <options> [SCHEMA]\n";
    std::cout << "Options:\n";
    std::cout << "-f, --file=<file>     Reads input from file\n";
    std::cout << "-I, --import=<dir>    Reads macro files from dir; required by -->import\n";
    std::cout << "--sql=<SQL>           Use SQL as input and execute it like in -->sql block\n";
    std::cout << "-e=<SQL>, --execute=<SQL> Aliases for \"--sql\" option\n";
    std::cout << "-n, --no-auth         Skip authentication which is required by -->sql block (run mode)\n";
    std::cout << "--plain-auth          Use PLAIN text authentication mechanism\n";
    std::cout << "-u, --user=<user>     Connection user\n";
    std::cout << "-p, --password=<pass> Connection password\n";
    std::cout << "-h, --host=<host>     Connection host\n";
    std::cout << "-P, --port=<port>     Connection port (default:" << MYSQLX_TCP_PORT << ")\n";
    std::cout << "--ipv=<mode>          Force internet protocol (default:4):\n";
    std::cout << "                      0 - allow system to resolve IPv6 and IPv4, for example\n";
    std::cout << "                          resolving of 'localhost' can return both '::1' and '127.0.0.1'\n";
    std::cout << "                      4 - allow system to resolve only IPv4, for example\n";
    std::cout << "                          resolving of 'localhost' is going to return '127.0.0.1'\n";
    std::cout << "                      6 - allow system to resolve only IPv6, for example\n";
    std::cout << "                          resolving of 'localhost' is going to return '::1'\n";
    std::cout << "-t, --timeout=<ms>    I/O timeouts in milliseconds\n";
    std::cout << "--close-no-sync       Do not wait for connection to be closed by server(disconnect first)\n";
    std::cout << "--schema=<schema>     Default schema to connect to\n";
    std::cout << "--uri=<uri>           Connection URI\n";
    std::cout << "                      URI takes precedence before options like: user, host, password, port\n";
    std::cout << "--socket=<file>       Connection through UNIX socket\n";
    std::cout << "--use-socket          Connection through UNIX socket, using default file name '" << MYSQLX_UNIX_ADDR << "'\n";
    std::cout << "                      --use-socket* options take precedence before options like: uri, user,\n";
    std::cout << "                      host, password, port\n";
    std::cout << "--ssl-key             X509 key in PEM format\n";
    std::cout << "--ssl-ca              CA file in PEM format\n";
    std::cout << "--ssl-ca_path         CA directory\n";
    std::cout << "--ssl-cert            X509 cert in PEM format\n";
    std::cout << "--ssl-cipher          SSL cipher to use\n";
    std::cout << "--tls-version         TLS version to use\n";
    std::cout << "--connect-expired-password Allow expired password\n";
    std::cout << "--quiet               Don't print out messages sent\n";
    std::cout << "-vVARIABLE_NAME=VALUE Set variable VARIABLE_NAME from command line\n";
    std::cout << "--fatal-errors=<0|1>  Mysqlxtest is started with ignoring or stopping on fatal error (default: 1)\n";
    std::cout << "-B, --bindump         Dump binary representation of messages sent, in format suitable for\n";
    std::cout << "                      the \"-->binsend\" command\n";
    std::cout << "--verbose             Enable extra verbose messages\n";
    std::cout << "--daemon              Work as a daemon (unix only)\n";
    std::cout << "--help                Show command line help\n";
    std::cout << "--help-commands       Show help for input commands\n";
    std::cout << "-V, --version         Show version of mysqlxtest\n";
    std::cout << "\nOnly one option that changes run mode is allowed.\n";
  }

  void print_help_commands()
  {
    std::cout << "Input may be a file (or if no --file is specified, it stdin will be used)\n";
    std::cout << "The following commands may appear in the input script:\n";
    std::cout << "-->echo <text>\n";
    std::cout << "  Prints the text (allows variables)\n";
    std::cout << "-->title <c><text>\n";
    std::cout << "  Prints the text with an underline, using the character <c>\n";
    std::cout << "-->sql\n";
    std::cout << "  Begins SQL block. SQL statements that appear will be executed and results printed (allows variables).\n";
    std::cout << "-->endsql\n";
    std::cout << "  End SQL block. End a block of SQL started by -->sql\n";
    std::cout << "-->macro <macroname> <argname1> ...\n";
    std::cout << "  Start a block of text to be defined as a macro. Must be terminated with -->endmacro\n";
    std::cout << "-->endmacro\n";
    std::cout << "  Ends a macro block\n";
    std::cout << "-->callmacro <macro>\t<argvalue1>\t...\n";
    std::cout << "  Executes the macro text, substituting argument values with the provided ones (args separated by tabs).\n";
    std::cout << "-->import <macrofile>\n";
    std::cout << "  Loads macros from the specified file. The file must be in the directory specified by --import option in command line.\n";
    std::cout << "-->enablessl\n";
    std::cout << "  Enables ssl on current connection\n";
    std::cout << "<protomsg>\n";
    std::cout << "  Encodes the text format protobuf message and sends it to the server (allows variables).\n";
    std::cout << "-->recv [quiet|<FIELD PATH>]\n";
    std::cout << "  quiet        - received message isn't printed\n";
    std::cout << "  <FIELD PATH> - print only selected part of the message using \"field-path\" filter:\n";
    std::cout << "                 field_name1\n";
    std::cout << "                 field_name1.field_name2\n";
    std::cout << "                 repeated_field_name1[1].field_name1.field_name2\n";
    std::cout << "-->recvresult [print-columnsinfo] [" << CMD_ARG_BE_QUIET << "]\n";
    std::cout << "  Read and print one resultset from the server; if print-columnsinfo is present also print short columns status\n";
    std::cout << "-->recvtovar <varname> [COLUMN_NAME]\n";
    std::cout << "  Read first row and first column (or column with name COLUMN_NAME) of resultset\n";
    std::cout << "  and set the variable <varname>\n";
    std::cout << "-->recverror <errno>\n";
    std::cout << "  Read a message and ensure that it's an error of the expected type\n";
    std::cout << "-->recvtype <msgtype> [" << CMD_ARG_BE_QUIET << "]\n";
    std::cout << "  Read one message and print it, checking that its type is the specified one\n";
    std::cout << "-->recvuntil <msgtype> [do_not_show_intermediate]\n";
    std::cout << "  Read messages and print them, until a msg of the specified type (or Error) is received\n";
    std::cout << "  do_not_show_intermediate - if this argument is present then printing of intermediate message should be omitted\n";
    std::cout << "-->repeat <N> [<VARIABLE_NAME>]\n";
    std::cout << "  Begin block of instructions that should be repeated N times\n";
    std::cout << "-->endrepeat\n";
    std::cout << "  End block of instructions that should be repeated - next iteration\n";
    std::cout << "-->stmtsql <CMD>\n";
    std::cout << "  Send StmtExecute with sql command\n";
    std::cout << "-->stmtadmin <CMD> [json_string]\n";
    std::cout << "  Send StmtExecute with admin command with given aguments (formated as json object)\n";
    std::cout << "-->system <CMD>\n";
    std::cout << "  Execute application or script (dev only)\n";
    std::cout << "-->exit\n";
    std::cout << "  Stops reading commands, disconnects and exits (same as <eof>/^D)\n";
    std::cout << "-->abort\n";
    std::cout << "  Exit immediately, without performing cleanup\n";
    std::cout << "-->nowarnings/-->yeswarnings\n";
    std::cout << "  Whether to print warnings generated by the statement (default no)\n";
    std::cout << "-->peerdisc <MILLISECONDS> [TOLERANCE]\n";
    std::cout << "  Expect that xplugin disconnects after given number of milliseconds and tolerance\n";
    std::cout << "-->sleep <SECONDS>\n";
    std::cout << "  Stops execution of mysqlxtest for given number of seconds (may be fractional)\n";
    std::cout << "-->login <user>\t<pass>\t<db>\t<mysql41|plain>]\n";
    std::cout << "  Performs authentication steps (use with --no-auth)\n";
    std::cout << "-->loginerror <errno>\t<user>\t<pass>\t<db>\n";
    std::cout << "  Performs authentication steps expecting an error (use with --no-auth)\n";
    std::cout << "-->fatalerrors/nofatalerrors\n";
    std::cout << "  Whether to immediately exit on MySQL errors\n";
    std::cout << "-->expecterror <errno>\n";
    std::cout << "  Expect a specific error for the next command and fail if something else occurs\n";
    std::cout << "  Works for: newsession, closesession, recvresult\n";
    std::cout << "-->newsession <name>\t<user>\t<pass>\t<db>\n";
    std::cout << "  Create a new connection with given name and account (use - as user for no-auth)\n";
    std::cout << "-->newsessionplain <name>\t<user>\t<pass>\t<db>\n";
    std::cout << "  Create a new connection with given name and account and force it to NOT use ssl, even if its generally enabled\n";
    std::cout << "-->setsession <name>\n";
    std::cout << "  Activate the named session\n";
    std::cout << "-->closesession [abort]\n";
    std::cout << "  Close the active session (unless its the default session)\n";
    std::cout << "-->wait_for <VALUE_EXPECTED>\t<SQL QUERY>\n";
    std::cout << "  Wait until SQL query returns value matches expected value (time limit 30 second)\n";
    std::cout << "-->assert_eq <VALUE_EXPECTED>\t<VALUE_TESTED>\n";
    std::cout << "  Ensure that 'TESTED' value equals 'EXPECTED' by comparing strings lexicographically\n";
    std::cout << "-->assert_gt <VALUE_EXPECTED>\t<VALUE_TESTED>\n";
    std::cout << "  Ensure that 'TESTED' value is greater than 'EXPECTED' (only when the both are numeric values)\n";
    std::cout << "-->assert_ge <VALUE_EXPECTED>\t<VALUE_TESTED>\n";
    std::cout << "  Ensure that 'TESTED' value is greater  or equal to 'EXPECTED' (only when the both are numeric values)\n";
    std::cout << "-->varfile <varname> <datafile>\n";
    std::cout << "  Assigns the contents of the file to the named variable\n";
    std::cout << "-->varlet <varname> <value>\n";
    std::cout << "  Assign the value (can be another variable) to the variable\n";
    std::cout << "-->varinc <varname> <n>\n";
    std::cout << "  Increment the value of varname by n (assuming both convert to integral)\n";
    std::cout << "-->varsub <varname>\n";
    std::cout << "  Add a variable to the list of variables to replace for the next recv or sql command (value is replaced by the name)\n";
    std::cout << "-->binsend <bindump>[<bindump>...]\n";
    std::cout << "  Sends one or more binary message dumps to the server (generate those with --bindump)\n";
    std::cout << "-->binsendoffset <srcvar> [offset-begin[percent]> [offset-end[percent]]]\n";
    std::cout << "  Same as binsend with begin and end offset of data to be send\n";
    std::cout << "-->binparse MESSAGE.NAME {\n";
    std::cout << "    MESSAGE.DATA\n";
    std::cout << "}\n";
    std::cout << "  Dump given message to variable %MESSAGE_DUMP%\n";
    std::cout << "-->quiet/noquiet\n";
    std::cout << "  Toggle verbose messages\n";
    std::cout << "-->query_result/noquery_result\n";
    std::cout << "  Toggle visibility for query results\n";
    std::cout << "-->received <msgtype>\t<varname>\n";
    std::cout << "  Assigns number of received messages of indicated type (in active session) to a variable\n";
    std::cout << "# comment\n";
  }

  bool set_mode(Run_mode mode)
  {
    if (RunTest != run_mode)
      return false;

    run_mode = mode;

    return true;
  }

  std::string get_socket_name()
  {
    return MYSQLX_UNIX_ADDR;
  }

  My_command_line_options(int argc, char **argv)
  : Command_line_options(argc, argv), run_mode(RunTest), has_file(false),
    cap_expired_password(false), dont_wait_for_server_disconnect(false),
    use_plain_auth(false), ip_mode(mysqlx::IPv4), timeout(0l), daemon(false)
  {
    std::string user;

    run_mode = RunTest; // run tests by default

    for (int i = 1; i < argc && exit_code == 0; i++)
    {
      char *value;
      if (check_arg_with_value(argv, i, "--file", "-f", value))
      {
        run_file = value;
        has_file = true;
      }
      else if (check_arg(argv, i, "--no-auth", "-n"))
      {
        if (!set_mode(RunTestWithoutAuth))
        {
          std::cerr << "Only one option that changes run mode is allowed.\n";
          exit_code = 1;
        }
      }
      else if (check_arg(argv, i, "--plain-auth", NULL))
      {
        use_plain_auth = true;
      }
      else if (check_arg_with_value(argv, i, "--sql", NULL, value))
      {
        sql = value;
      }
      else if (check_arg_with_value(argv, i, "--execute", "-e", value))
      {
        sql = value;
      }
      else if (check_arg_with_value(argv, i, "--password", "-p", value))
        connection.password = value;
      else if (check_arg_with_value(argv, i, "--ssl-key", NULL, value))
        ssl.key = value;
      else if (check_arg_with_value(argv, i, "--ssl-ca", NULL, value))
        ssl.ca = value;
      else if (check_arg_with_value(argv, i, "--ssl-ca_path", NULL, value))
        ssl.ca_path = value;
      else if (check_arg_with_value(argv, i, "--ssl-cert", NULL, value))
        ssl.cert = value;
      else if (check_arg_with_value(argv, i, "--ssl-cipher", NULL, value))
        ssl.cipher = value;
      else if (check_arg_with_value(argv, i, "--tls-version", NULL, value))
        ssl.tls_version = value;
      else if (check_arg_with_value(argv, i, "--host", "-h", value))
        connection.host = value;
      else if (check_arg_with_value(argv, i, "--user", "-u", value))
        connection.user = value;
      else if (check_arg_with_value(argv, i, "--uri", NULL, value))
        uri = value;
      else if (check_arg_with_value(argv, i, "--schema", NULL, value))
        connection.schema = value;
      else if (check_arg_with_value(argv, i, "--port", "-P", value))
        connection.port = ngs::stoi(value);
      else if (check_arg_with_value(argv, i, "--ipv", NULL, value))
      {
        ip_mode = set_protocol(ngs::stoi(value));
      }
      else if (check_arg_with_value(argv, i, "--timeout", "-t", value))
        timeout = ngs::stoi(value);
      else if (check_arg_with_value(argv, i, "--fatal-errors", NULL, value))
        OPT_fatal_errors = ngs::stoi(value);
      else if (check_arg_with_value(argv, i, "--password", "-p", value))
        connection.password = value;
      else if (check_arg_with_value(argv, i, "--socket", "-S", value))
        connection.socket = value;
      else if (check_arg_with_value(argv, i, NULL, "-v", value))
        set_variable_option(value);
      else if (check_arg(argv, i, "--use-socket", NULL))
        connection.socket = get_socket_name();
      else if (check_arg(argv, i, "--close-no-sync", NULL))
        dont_wait_for_server_disconnect = true;
      else if (check_arg(argv, i, "--bindump", "-B"))
        OPT_bindump = true;
      else if (check_arg(argv, i, "--connect-expired-password", NULL))
        cap_expired_password = true;
      else if (check_arg(argv, i, "--quiet", "-q"))
        OPT_quiet = true;
      else if (check_arg(argv, i, "--verbose", NULL))
        OPT_verbose = true;
      else if (check_arg(argv, i, "--daemon", NULL))
        daemon = true;
#ifndef _WIN32
      else if (check_arg(argv, i, "--color", NULL))
        OPT_color = true;
#endif
      else if (check_arg_with_value(argv, i, "--import", "-I", value))
      {
        OPT_import_path = value;
        if (*OPT_import_path.rbegin() != FN_LIBCHAR)
          OPT_import_path += FN_LIBCHAR;
      }
      else if (check_arg(argv, i, "--help", "--help"))
      {
        print_help();
        exit_code = 1;
      }
      else if (check_arg(argv, i, "--help-commands", "--help-commands"))
      {
        print_help_commands();
        exit_code = 1;
      }
      else if (check_arg(argv, i, "--version", "-V"))
      {
        print_version();
        exit_code = 1;
      }
      else if (exit_code == 0)
      {
        if (argc -1 == i && std::isalnum(argv[i][0]))
        {
          connection.schema = argv[i];
          break;
        }

        std::cerr << argv[0] << ": unknown option " << argv[i] << "\n";
        exit_code = 1;
        break;
      }
    }

    if (connection.port == 0)
      connection.port = MYSQLX_TCP_PORT;
    if (connection.host.empty())
      connection.host = "localhost";
  }

  void set_variable_option(const std::string &set_expression)
  {
    std::vector<std::string> args;

    aux::split(args, set_expression, "=", false);

    if (2 != args.size())
    {
      std::cerr << "Wrong format expected NAME=VALUE\n";
      exit_code = 1;
      return;
    }

    variables[args[0]] = args[1];
  }

  mysqlx::Internet_protocol set_protocol(const int ip_mode)
  {
    switch(ip_mode)
    {
    case 0:
      return mysqlx::IP_any;

    case 4:
      return mysqlx::IPv4;

    case 6:
      return mysqlx::IPv6;

    default:
      std::cerr << "Wrong Internet protocol version\n";
      exit_code = 1;
      return mysqlx::IP_any;
    }
  }
};

static std::vector<Block_processor_ptr> create_macro_block_processors(Connection_manager *cm)
{
  std::vector<Block_processor_ptr> result;

  result.push_back(ngs::make_shared<Sql_block_processor>(cm));
  result.push_back(ngs::make_shared<Dump_message_block_processor>(cm));
  result.push_back(ngs::make_shared<Single_command_processor>(cm));
  result.push_back(ngs::make_shared<Snd_message_block_processor>(cm));

  return result;
}

static std::vector<Block_processor_ptr> create_block_processors(Connection_manager *cm)
{
  std::vector<Block_processor_ptr> result;

  result.push_back(ngs::make_shared<Sql_block_processor>(cm));
  result.push_back(ngs::make_shared<Macro_block_processor>(cm));
  result.push_back(ngs::make_shared<Dump_message_block_processor>(cm));
  result.push_back(ngs::make_shared<Single_command_processor>(cm));
  result.push_back(ngs::make_shared<Snd_message_block_processor>(cm));

  return result;
}

static int process_client_input_on_session(const My_command_line_options &options, std::istream &input)
{
  Connection_manager cm(options.uri, options.connection, options.ssl, options.timeout, options.dont_wait_for_server_disconnect, options.ip_mode);
  int r = 1;

  try
  {
    std::vector<Block_processor_ptr> eaters;

    cm.connect_default(options.cap_expired_password, options.use_plain_auth);
    eaters = create_block_processors(&cm);
    r = process_client_input(input, eaters);
    cm.close_active(true);
  }
  catch (mysqlx::Error &error)
  {
    dumpx(error);
    std::cerr << "not ok\n";
    return 1;
  }

  if (r == 0)
    std::cerr << "ok\n";
  else
    std::cerr << "not ok\n";

  return r;
}

static int process_client_input_no_auth(const My_command_line_options &options, std::istream &input)
{
  Connection_manager cm(options.uri, options.connection, options.ssl, options.timeout, options.dont_wait_for_server_disconnect, options.ip_mode);
  int r = 1;

  try
  {
    std::vector<Block_processor_ptr> eaters;

    cm.active()->set_closed();
    eaters = create_block_processors(&cm);
    r = process_client_input(input, eaters);
  }
  catch (mysqlx::Error &error)
  {
    dumpx(error);
    std::cerr << "not ok\n";
    return 1;
  }

  if (r == 0)
    std::cerr << "ok\n";
  else
    std::cerr << "not ok\n";

  return r;
}

bool Macro::call(Execution_context &context, const std::string &cmd)
{
  std::string name;
  std::string macro = get(cmd, name);
  if (macro.empty())
    return false;

  Stack_frame frame = {0, "macro "+name};
  script_stack.push_front(frame);

  std::stringstream stream(macro);
  std::vector<Block_processor_ptr> processors(create_macro_block_processors(context.m_cm));

  bool r = process_client_input(stream, processors) == 0;

  script_stack.pop_front();

  return r;
}


namespace
{

class Json_to_any_handler : public rapidjson::BaseReaderHandler<rapidjson::UTF8<>, Json_to_any_handler>
{
public:
  typedef ::Mysqlx::Datatypes::Any Any;

  Json_to_any_handler(Any &any)
  {
    m_stack.push(&any);
  }

  bool Key(const char *str, rapidjson::SizeType length, bool copy)
  {
    typedef ::Mysqlx::Datatypes::Object_ObjectField Field;
    Field *f = m_stack.top()->mutable_obj()->add_fld();
    f->set_key(str, length);
    m_stack.push(f->mutable_value());
    return true;
  }

  bool Null()
  {
    get_scalar(::Mysqlx::Datatypes::Scalar_Type_V_NULL);
    return true;
  }

  bool Bool(bool b)
  {
    get_scalar(::Mysqlx::Datatypes::Scalar_Type_V_BOOL)->set_v_bool(b);
    return true;
  }

  bool Int(int i)
  {
    get_scalar(::Mysqlx::Datatypes::Scalar_Type_V_SINT)->set_v_signed_int(i);
    return true;
  }

  bool Uint(unsigned u)
  {
    get_scalar(::Mysqlx::Datatypes::Scalar_Type_V_UINT)->set_v_unsigned_int(u);
    return true;
  }

  bool Int64(int64_t i)
  {
    get_scalar(::Mysqlx::Datatypes::Scalar_Type_V_SINT)->set_v_signed_int(i);
    return true;
  }

  bool Uint64(uint64_t u)
  {
    get_scalar(::Mysqlx::Datatypes::Scalar_Type_V_UINT)->set_v_unsigned_int(u);
    return true;
  }

  bool Double(double d, bool = false)
  {
    get_scalar(::Mysqlx::Datatypes::Scalar_Type_V_DOUBLE)->set_v_double(d);
    return true;
  }

  bool String(const char* str, rapidjson::SizeType length, bool)
  {
    get_scalar(::Mysqlx::Datatypes::Scalar_Type_V_STRING)->mutable_v_string()->set_value(str, length);
    return true;
  }

  bool StartObject()
  {
    Any *any = m_stack.top();
    if (any->has_type() && any->type() == ::Mysqlx::Datatypes::Any_Type_ARRAY)
      m_stack.push(any->mutable_array()->add_value());
    m_stack.top()->set_type(::Mysqlx::Datatypes::Any_Type_OBJECT);
    m_stack.top()->mutable_obj();
    return true;
  }

  bool EndObject(rapidjson::SizeType memberCount)
  {
    m_stack.pop();
    return true;
  }

  bool StartArray()
  {
    m_stack.top()->set_type(::Mysqlx::Datatypes::Any_Type_ARRAY);
    m_stack.top()->mutable_array();
    return true;
  }

  bool EndArray(rapidjson::SizeType elementCount)
  {
    m_stack.pop();
    return true;
  }

private:
  typedef ::Mysqlx::Datatypes::Scalar Scalar;

  Scalar *get_scalar(Scalar::Type scalar_t)
  {
    Any *any = m_stack.top();
    if (any->has_type() && any->type() == ::Mysqlx::Datatypes::Any_Type_ARRAY)
      any = any->mutable_array()->add_value();
    else
      m_stack.pop();
    any->set_type(::Mysqlx::Datatypes::Any_Type_SCALAR);
    Scalar *s = any->mutable_scalar();
    s->set_type(scalar_t);
    return s;
  }

  std::stack<Any*> m_stack;
};

} // namespace


bool Command::json_string_to_any(const std::string &json_string, Any &any) const
{
  Json_to_any_handler handler(any);
  rapidjson::Reader reader;
  rapidjson::StringStream ss(json_string.c_str());
  return !reader.Parse(ss, handler).IsError();
}


Command::Result Command::cmd_import(Execution_context &context,
                                    const std::string &args)
{
  std::string varg(args);
  replace_variables(varg);
  const std::string filename = OPT_import_path + varg;

  std::ifstream fs(filename.c_str());
  if (!fs.good())
  {
    std::cerr << error() << "Could not open macro file " << args << " (aka "
              << filename << ")" << eoerr();
    return Stop_with_failure;
  }

  Stack_frame frame = {0, args};
  script_stack.push_front(frame);

  std::vector<Block_processor_ptr> processors;
  processors.push_back(ngs::make_shared<Macro_block_processor>(context.m_cm));
  bool r = process_client_input(fs, processors) == 0;
  script_stack.pop_front();

  return r ? Continue : Stop_with_failure;
}

typedef int (*Program_mode)(const My_command_line_options &, std::istream &input);

static std::istream &get_input(My_command_line_options &opt, std::ifstream &file, std::stringstream &string)
{
  if (opt.has_file)
  {
    if (!opt.sql.empty())
    {
      std::cerr << "ERROR: specified file and sql to execute, please enter only one of those\n";
      opt.exit_code = 1;
    }

    file.open(opt.run_file.c_str());
    file.rdbuf()->pubsetbuf(NULL, 0);

    if (!file.is_open())
    {
      std::cerr << "ERROR: Could not open file " << opt.run_file << "\n";
      opt.exit_code = 1;
    }

    return file;
  }

  if (!opt.sql.empty())
  {
    std::streampos position = string.tellp();

    string << "-->sql\n";
    string << opt.sql << "\n";
    string << "-->endsql\n";
    string.seekp(position, std::ios::beg);

    return string;
  }

  return std::cin;
}


static void unable_daemonize()
{
  std::cerr << "ERROR: Unable to put process in background\n";
  exit(2);
}


static void daemonize()
{
#ifdef WIN32
  unable_daemonize();
#else
  if (getppid() == 1) // already a daemon
    exit(0);
  pid_t pid = fork();
  if (pid < 0)
    unable_daemonize();
  if (pid > 0)
    exit(0);
  if (setsid() < 0)
    unable_daemonize();
#endif
}


static Program_mode get_mode_function(const My_command_line_options &opt)
{
  switch(opt.run_mode)
  {
  case My_command_line_options::RunTestWithoutAuth:
    return process_client_input_no_auth;

  case My_command_line_options::RunTest:
  default:
    return process_client_input_on_session;
  }
}


int main(int argc, char **argv)
{
  MY_INIT(argv[0]);
  local_message_hook = ignore_traces_from_libraries;

  OPT_expect_error = new Expected_error();
  My_command_line_options options(argc, argv);

  if (options.exit_code != 0)
    return options.exit_code;

  if (options.daemon)
    daemonize();

  std::cout << std::unitbuf;
  std::ifstream fs;
  std::stringstream ss;
  std::istream &input = get_input(options, fs, ss);
  Program_mode  mode  = get_mode_function(options);

#ifdef WIN32
  if (!have_tcpip)
  {
    std::cerr << "OS doesn't have tcpip\n";
    return 1;
  }
#endif

  ssl_start();

  bool result = 0;
  try
  {
    Stack_frame frame = {0, "main"};
    script_stack.push_front(frame);

    result = mode(options, input);
  }
  catch (mysqlx::Error &e)
  {
    std::cerr << "ERROR: " << e.what() << "\n";
    result = 1;
  }
  catch (std::exception &e)
  {
    std::cerr << "ERROR: " << e.what() << "\n";
    result = 1;
  }

  vio_end();
  my_end(0);
  return result;
}


#include "mysqlx_all_msgs.h"

#ifdef _MSC_VER
#  pragma pop_macro("ERROR")
#endif
