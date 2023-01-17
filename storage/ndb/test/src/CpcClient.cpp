/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <ndb_global.h>

#include <NdbTCP.h>
#include <NdbOut.hpp>
#include "CpcClient.hpp"

#define CPC_CMD(name, value, desc)                                           \
  {                                                                          \
    (name), 0, ParserRow_t::Cmd, ParserRow_t::String, ParserRow_t::Optional, \
        ParserRow_t::IgnoreMinMax, 0, 0, 0, (desc), (value)                  \
  }

#define CPC_ARG(name, type, opt, desc)                                \
  {                                                                   \
    (name), 0, ParserRow_t::Arg, ParserRow_t::type, ParserRow_t::opt, \
        ParserRow_t::IgnoreMinMax, 0, 0, 0, (desc), 0                 \
  }

#define CPC_IGNORE_EXTRA_ARG()                                             \
  {                                                                        \
    "", 0, ParserRow_t::Arg, ParserRow_t::LongString, ParserRow_t::Ignore, \
        ParserRow_t::IgnoreMinMax, 0, 0, 0, 0, 0                           \
  }

#define CPC_END()                                                    \
  {                                                                  \
    0, 0, ParserRow_t::End, ParserRow_t::Int, ParserRow_t::Optional, \
        ParserRow_t::IgnoreMinMax, 0, 0, 0, 0, 0                     \
  }

#ifdef DEBUG_PRINT_PROPERTIES
static void printprop(const Properties &p) {
  Properties::Iterator iter(&p);
  const char *name;
  while ((name = iter.next()) != NULL) {
    PropertiesType t;
    Uint32 val_i;
    BaseString val_s;

    p.getTypeOf(name, &t);
    switch (t) {
      case PropertiesType_Uint32:
        p.get(name, &val_i);
        ndbout << name << " (Uint32): " << val_i << endl;
        break;
      case PropertiesType_char:
        p.get(name, val_s);
        ndbout << name << " (string): " << val_s << endl;
        break;
      default:
        ndbout << "Unknown type " << t << endl;
        break;
    }
  }
}
#endif

int SimpleCpcClient::stop_process(Uint32 id, Properties &reply) {
  const ParserRow_t stop_reply[] = {
      CPC_CMD("stop process", NULL, ""), CPC_ARG("status", Int, Mandatory, ""),
      CPC_ARG("id", Int, Optional, ""),
      CPC_ARG("errormessage", String, Optional, ""),

      CPC_END()};

  Properties args;
  args.put("id", id);

  const Properties *ret = cpc_call("stop process", args, stop_reply);
  if (ret == 0) {
    reply.put("status", (Uint32)0);
    reply.put("errormessage", "unknown error");
    return -1;
  }

  Uint32 status = 0;
  ret->get("status", &status);
  reply.put("status", status);
  if (status != 0) {
    BaseString msg;
    ret->get("errormessage", msg);
    reply.put("errormessage", msg.c_str());
  }
  delete ret;
  return status;
}

int SimpleCpcClient::start_process(Uint32 id, Properties &reply) {
  const ParserRow_t start_reply[] = {
      CPC_CMD("start process", NULL, ""), CPC_ARG("status", Int, Mandatory, ""),
      CPC_ARG("id", Int, Optional, ""),
      CPC_ARG("errormessage", String, Optional, ""),

      CPC_END()};

  Properties args;
  args.put("id", id);

  const Properties *ret = cpc_call("start process", args, start_reply);
  if (ret == 0) {
    reply.put("status", (Uint32)0);
    reply.put("errormessage", "unknown error");
    return -1;
  }

  Uint32 status = 0;
  ret->get("status", &status);
  reply.put("status", status);
  if (status != 0) {
    BaseString msg;
    ret->get("errormessage", msg);
    reply.put("errormessage", msg.c_str());
  }

  delete ret;

  return status;
}

int SimpleCpcClient::undefine_process(Uint32 id, Properties &reply) {
  const ParserRow_t stop_reply[] = {
      CPC_CMD("undefine process", NULL, ""),
      CPC_ARG("status", Int, Mandatory, ""), CPC_ARG("id", Int, Optional, ""),
      CPC_ARG("errormessage", String, Optional, ""),

      CPC_END()};

  Properties args;
  args.put("id", id);

  const Properties *ret = cpc_call("undefine process", args, stop_reply);
  if (ret == 0) {
    reply.put("status", (Uint32)0);
    reply.put("errormessage", "unknown error");
    return -1;
  }

  Uint32 status = 0;
  ret->get("status", &status);
  reply.put("status", status);
  if (status != 0) {
    BaseString msg;
    ret->get("errormessage", msg);
    reply.put("errormessage", msg.c_str());
  }

  delete ret;

  return status;
}

#if 0
static void
printproc(SimpleCpcClient::Process & p) {
  ndbout.println("Name:                %s", p.m_name.c_str());
  ndbout.println("Id:                  %d", p.m_id);
  ndbout.println("Type:                %s", p.m_type.c_str());
  ndbout.println("Group:               %s", p.m_group.c_str());
  ndbout.println("Program path:        %s", p.m_path.c_str());
  ndbout.println("Arguments:           %s", p.m_args.c_str());
  ndbout.println("Environment:         %s", p.m_env.c_str());
  ndbout.println("Working directory:   %s", p.m_cwd.c_str());
  ndbout.println("Owner:               %s", p.m_owner.c_str());
  ndbout.println("Runas:               %s", p.m_runas.c_str());
  ndbout.println("Ulimit:              %s", p.m_ulimit.c_str());
  ndbout.println("%s", "");
}
#endif

static int convert(const Properties &src, SimpleCpcClient::Process &dst) {
  bool b = true;
  b &= src.get("id", (Uint32 *)&dst.m_id);
  b &= src.get("name", dst.m_name);
  b &= src.get("type", dst.m_type);
  b &= src.get("status", dst.m_status);
  b &= src.get("owner", dst.m_owner);
  b &= src.get("group", dst.m_group);
  b &= src.get("path", dst.m_path);
  b &= src.get("args", dst.m_args);
  b &= src.get("env", dst.m_env);
  b &= src.get("cwd", dst.m_cwd);
  b &= src.get("runas", dst.m_runas);
  b &= src.get("cpuset", dst.m_cpuset);

  b &= src.get("stdin", dst.m_stdin);
  b &= src.get("stdout", dst.m_stdout);
  b &= src.get("stderr", dst.m_stderr);
  b &= src.get("ulimit", dst.m_ulimit);
  b &= src.get("shutdown", dst.m_shutdown_options);

  return b;
}

static int convert(const SimpleCpcClient::Process &src, Properties &dst) {
  bool b = true;
  // b &= dst.put("id",     (Uint32)src.m_id);
  b &= dst.put("name", src.m_name.c_str());
  b &= dst.put("type", src.m_type.c_str());
  // b &= dst.put("status", src.m_status.c_str());
  b &= dst.put("owner", src.m_owner.c_str());
  b &= dst.put("group", src.m_group.c_str());
  b &= dst.put("path", src.m_path.c_str());
  b &= dst.put("args", src.m_args.c_str());
  b &= dst.put("env", src.m_env.c_str());
  b &= dst.put("cwd", src.m_cwd.c_str());
  b &= dst.put("runas", src.m_runas.c_str());

  if (!src.m_cpuset.empty()) {
    b &= dst.put("cpuset", src.m_cpuset.c_str());
  }

  b &= dst.put("stdin", src.m_stdin.c_str());
  b &= dst.put("stdout", src.m_stdout.c_str());
  b &= dst.put("stderr", src.m_stderr.c_str());
  b &= dst.put("ulimit", src.m_ulimit.c_str());
  b &= dst.put("shutdown", src.m_shutdown_options.c_str());

  return b;
}

int SimpleCpcClient::define_process(Process &p, Properties &reply) {
  const ParserRow_t define_reply[] = {
      CPC_CMD("define process", NULL, ""),
      CPC_ARG("status", Int, Mandatory, ""), CPC_ARG("id", Int, Optional, ""),
      CPC_ARG("errormessage", String, Optional, ""),

      CPC_END()};

  Properties args;
  convert(p, args);

  const Properties *ret = cpc_call("define process", args, define_reply);
  if (ret == 0) {
    reply.put("status", (Uint32)0);
    reply.put("errormessage", "unknown error");
    return -1;
  }

  Uint32 status = 0;
  ret->get("status", &status);
  reply.put("status", status);
  if (status != 0) {
    BaseString msg;
    ret->get("errormessage", msg);
    reply.put("errormessage", msg.c_str());
  }

  Uint32 id;
  if (!ret->get("id", &id)) {
    return -1;
  }

  p.m_id = id;
  delete ret;
  return status;
}

int SimpleCpcClient::list_processes(Vector<Process> &procs, Properties &reply) {
  int start = 0, end = 0, entry;
  const ParserRow_t list_reply[] = {
      CPC_CMD("start processes", &start, ""),
      CPC_CMD("end processes", &end, ""),

      CPC_CMD("process", &entry, ""),
      CPC_ARG("id", Int, Mandatory, "Id of process."),
      CPC_ARG("name", String, Mandatory, "Name of process"),
      CPC_ARG("group", String, Mandatory, "Group of process"),
      CPC_ARG("env", LongString, Mandatory,
              "Environment variables for process"),
      CPC_ARG("path", String, Mandatory, "Path to binary"),
      CPC_ARG("args", LongString, Mandatory, "Arguments to process"),
      CPC_ARG("type", String, Mandatory, "Type of process"),
      CPC_ARG("cwd", String, Mandatory, "Working directory of process"),
      CPC_ARG("owner", String, Mandatory, "Owner of process"),
      CPC_ARG("status", String, Mandatory, "Status of process"),
      CPC_ARG("runas", String, Mandatory, "Run as user"),
      CPC_ARG("cpuset", LongString, Optional, "CPU affinity set"),
      CPC_ARG("stdin", String, Mandatory, "Redirect stdin"),
      CPC_ARG("stdout", String, Mandatory, "Redirect stdout"),
      CPC_ARG("stderr", String, Mandatory, "Redirect stderr"),
      CPC_ARG("ulimit", String, Mandatory, "ulimit"),
      CPC_ARG("shutdown", String, Mandatory, "shutdown"),

      CPC_END()};

  reply.clear();

  const Properties args;

  int status = cpc_send("list processes", args);
  if (status == -1) {
    ndbout_c("Failed to send command to CPCD: %d", __LINE__);
    return -1;
  }

  bool done = false;
  while (!done) {
    const Properties *proc;
    void *p;
    status = cpc_recv(list_reply, &proc, &p);
    if (status == -1) {
      ndbout_c("Failed to receive data from CPCD: %d", __LINE__);
      return -1;
    }

    end++;
    if (p == &start) {
      start = 1;
      /* do nothing */
    } else if (p == &end) {
      done = true;
    } else if (p == &entry) {
      if (proc != NULL) {
        Process p;
        convert(*proc, p);
        procs.push_back(p);
      } else {
        ndbout_c(
            "JONAS: start: %d loop: %d cnt: %u got p == &entry with proc == "
            "NULL",
            start, end, procs.size());
      }
    } else {
      ndbout_c("internal error: %d", __LINE__);
      return -1;
    }
    if (proc) {
      delete proc;
    }
  }
  return 0;
}

int SimpleCpcClient::show_version(Properties &reply) {
  const ParserRow_t start_reply[] = {
      CPC_CMD("show version", NULL, ""),
      CPC_ARG("supported protocol", Int, Optional, ""), CPC_IGNORE_EXTRA_ARG(),

      CPC_END()};

  Properties args;

  const Properties *ret = cpc_call("show version", args, start_reply);
  if (ret == 0) {
    reply.put("status", (Uint32)0);
    reply.put("errormessage", "unknown error");
    return -1;
  }

  Uint32 version;
  if (!ret->get("supported protocol", &version)) {
    reply.put("status", 1);
    return -1;
  }

  reply.put("version", version);
  delete ret;

  return 0;
}

int SimpleCpcClient::select_protocol(Properties &reply) {
  const ParserRow_t start_reply[] = {
      CPC_CMD("select protocol", NULL, ""),
      CPC_ARG("status", Int, Mandatory, ""),
      CPC_ARG("errormessage", String, Optional, ""),

      CPC_END()};

  Properties args;
  args.put("version", CPC_PROTOCOL_VERSION);

  const Properties *ret = cpc_call("select protocol", args, start_reply);
  if (ret == 0) {
    reply.put("status", (Uint32)0);
    reply.put("errormessage", "unknown error");
    return -1;
  }

  Uint32 status = 0;
  ret->get("status", &status);
  reply.put("status", status);
  if (status != 0) {
    BaseString msg;
    ret->get("errormessage", msg);
    reply.put("errormessage", msg.c_str());
  }

  delete ret;

  return 0;
}

SimpleCpcClient::SimpleCpcClient(const char *_host, int _port) {
  host = strdup(_host);
  port = _port;
  ndb_socket_initialize(&cpc_sock);
  m_cpcd_protocol_version = 0;
}

SimpleCpcClient::~SimpleCpcClient() {
  if (host != NULL) {
    free(host);
    host = NULL;
  }

  port = 0;

  if (ndb_socket_valid(cpc_sock)) {
    close_connection();
  }
}

int SimpleCpcClient::connect() {
  int res = open_connection();
  if (res != 0) return -1;

  res = negotiate_client_protocol();
  if (res != 0) {
    close_connection();
    return -1;
  }

  return 0;
}

int SimpleCpcClient::open_connection() {
  struct sockaddr_in6 sa;

  /* Create socket */
  cpc_sock = ndb_socket_create_dual_stack(SOCK_STREAM, IPPROTO_TCP);
  if (!ndb_socket_valid(cpc_sock)) return -1;

  memset(&sa, 0, sizeof(sa));
  sa.sin6_family = AF_INET6;
  sa.sin6_port = htons(port);

  // Resolve server address
  if (Ndb_getInAddr6(&sa.sin6_addr, host))
  {
    ndb_socket_close(cpc_sock);
    ndb_socket_invalidate(&cpc_sock);
    errno = ENOENT;
    return -1;
  }

  return ndb_connect_inet6(cpc_sock, &sa);
}

int SimpleCpcClient::negotiate_client_protocol() {
  Properties p;
  int res = show_version(p);
  if (res != 0) return -1;

  Uint32 version = 1;
  p.get("version", &version);

  if (version < CPC_PROTOCOL_VERSION) return -1;

  res = select_protocol(p);
  if (res != 0) return -1;

  m_cpcd_protocol_version = version;
  return 0;
}

void SimpleCpcClient::close_connection() {
  ndb_socket_close(cpc_sock);
  ndb_socket_invalidate(&cpc_sock);
}

int SimpleCpcClient::cpc_send(const char *cmd, const Properties &args) {
  SocketOutputStream cpc_out(cpc_sock);

  int status = cpc_out.println("%s", cmd);
  if (status == -1) {
    return -1;
  }

  Properties::Iterator iter(&args);
  const char *name;
  while ((name = iter.next()) != NULL) {
    PropertiesType t;
    Uint32 val_i;
    BaseString val_s;
    const size_t namelen = strlen(name);

    args.getTypeOf(name, &t);
    switch (t) {
      case PropertiesType_Uint32:
        args.get(name, &val_i);
        cpc_out.println("%s: %d", name, val_i);
        break;
      case PropertiesType_char: {
        /**
         * Long string, chop up into multiple lines:
         * argname:"val-prefix..."\n
         * +argname:"val-part..."\n
         * ...
         * +argname:"val-part..."\n
         */
        args.get(name, val_s);
        /**
         * For first line there are five characters in addition to name and
         * value part, for the following lines the maximum length of a value
         * part is one character less since the extra plus sign is needed.
         */
        const char *value = val_s.c_str();
        int valuelen = val_s.length();
        int partlen = Parser_t::Context::MaxParseBytes - namelen - 5;
        cpc_out.print("%s:\"%.*s\"\n", name, partlen, value);
        int valueoff = partlen;
        partlen--;  // Maximum part length decreased to make room for plus sign
        while (valueoff < valuelen) {
          cpc_out.print("+%s:\"%.*s\"\n", name, partlen, value + valueoff);
          valueoff += partlen;
        }
        break;
      }
      default:
        /* Silently ignore */
        break;
    }
  }
  status = cpc_out.println("%s", "");

  return status == -1 ? status : 0;
}

/**
 * Receive a response from the CPCD. The argument reply will point
 * to a Properties object describing the reply. Note that the caller
 * is responsible for deleting the Properties object returned.
 */
SimpleCpcClient::Parser_t::ParserStatus SimpleCpcClient::cpc_recv(
    const ParserRow_t *syntax, const Properties **reply, void **user_value) {
  SocketInputStream cpc_in(cpc_sock, 2 * 60 * 1000);

  Parser_t::Context ctx;
  ParserDummy session(cpc_sock);
  Parser_t parser(syntax, cpc_in);
  *reply = parser.parse(ctx, session);

  if (user_value != NULL)
  {
    if (ctx.m_status == Parser_t::Ok ||
      ctx.m_status == Parser_t::CommandWithoutFunction)
    {
      *user_value = ctx.m_currentCmd->user_value;
    }
    else
    {
      *user_value = NULL;
    }
  }

  return ctx.m_status;
}

const Properties *SimpleCpcClient::cpc_call(const char *cmd,
                                            const Properties &args,
                                            const ParserRow_t *reply_syntax) {
  cpc_send(cmd, args);

  const Properties *ret;
  cpc_recv(reply_syntax, &ret);
  return ret;
}

SimpleCpcClient::ParserDummy::ParserDummy(ndb_socket_t sock)
    : SocketServer::Session(sock) {}

template class Vector<SimpleCpcClient::Process>;
template class Vector<ParserRow<SimpleCpcClient::ParserDummy> const *>;
