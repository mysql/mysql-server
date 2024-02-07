/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <socket_io.h>
#include <NdbOut.hpp>
#include <Parser.hpp>
#include <Properties.hpp>

#include <NdbMutex.h>
#include <OutputStream.hpp>
#include "APIService.hpp"
#include "CPCD.hpp"

/**
   const char * name;
   const char * realName;
   const Type type;
   const ArgType argType;
   const ArgRequired argRequired;
   const ArgMinMax argMinMax;
   const int minVal;
   const int maxVal;
   void (T::* function)(const class Properties & args);
   const char * description;
*/

#define CPCD_CMD(name, fun, desc)                                   \
  {                                                                 \
    name, 0, ParserRow<CPCDAPISession>::Cmd,                        \
        ParserRow<CPCDAPISession>::String,                          \
        ParserRow<CPCDAPISession>::Optional,                        \
        ParserRow<CPCDAPISession>::IgnoreMinMax, 0, 0, fun, desc, 0 \
  }

#define CPCD_ARG(name, type, opt, desc)                                       \
  {                                                                           \
    name, 0, ParserRow<CPCDAPISession>::Arg, ParserRow<CPCDAPISession>::type, \
        ParserRow<CPCDAPISession>::opt,                                       \
        ParserRow<CPCDAPISession>::IgnoreMinMax, 0, 0, 0, desc, 0             \
  }

#define CPCD_ARG2(name, type, opt, min, max, desc)                            \
  {                                                                           \
    name, 0, ParserRow<CPCDAPISession>::Arg, ParserRow<CPCDAPISession>::type, \
        ParserRow<CPCDAPISession>::opt,                                       \
        ParserRow<CPCDAPISession>::IgnoreMinMax, min, max, 0, desc, 0         \
  }

#define CPCD_END()                                                        \
  {                                                                       \
    0, 0, ParserRow<CPCDAPISession>::End, ParserRow<CPCDAPISession>::Int, \
        ParserRow<CPCDAPISession>::Optional,                              \
        ParserRow<CPCDAPISession>::IgnoreMinMax, 0, 0, 0, 0, 0            \
  }

#define CPCD_CMD_ALIAS(name, realName, fun)                                  \
  {                                                                          \
    name, realName, ParserRow<CPCDAPISession>::CmdAlias,                     \
        ParserRow<CPCDAPISession>::Int, ParserRow<CPCDAPISession>::Optional, \
        ParserRow<CPCDAPISession>::IgnoreMinMax, 0, 0, 0, 0, 0               \
  }

#define CPCD_ARG_ALIAS(name, realName, fun)                                  \
  {                                                                          \
    name, realName, ParserRow<CPCDAPISession>::ArgAlias,                     \
        ParserRow<CPCDAPISession>::Int, ParserRow<CPCDAPISession>::Optional, \
        ParserRow<CPCDAPISession>::IgnoreMinMax, 0, 0, 0, 0, 0               \
  }

const ParserRow<CPCDAPISession> commands[] = {
    CPCD_CMD("define process", &CPCDAPISession::defineProcess, ""),
    CPCD_ARG("id", Int, Optional, "Id of process."),
    CPCD_ARG("name", String, Mandatory, "Name of process"),
    CPCD_ARG("group", String, Mandatory, "Group of process"),
    CPCD_ARG("env", LongString, Optional, "Environment variables for process"),
    CPCD_ARG("path", String, Mandatory, "Path to binary"),
    CPCD_ARG("args", LongString, Optional, "Arguments to process"),
    CPCD_ARG("type", String, Mandatory, "Type of process"),
    CPCD_ARG("cwd", String, Mandatory, "Working directory of process"),
    CPCD_ARG("owner", String, Mandatory, "Owner of process"),
    CPCD_ARG("runas", String, Optional, "Run as user"),
    CPCD_ARG("cpuset", LongString, Optional, "CPU affinity set"),
    CPCD_ARG("stdout", String, Optional, "Redirection of stdout"),
    CPCD_ARG("stderr", String, Optional, "Redirection of stderr"),
    CPCD_ARG("stdin", String, Optional, "Redirection of stderr"),
    CPCD_ARG("ulimit", String, Optional, "ulimit"),
    CPCD_ARG("shutdown", String, Optional, "shutdown options"),

    CPCD_CMD("undefine process", &CPCDAPISession::undefineProcess, ""),
    CPCD_CMD_ALIAS("undef", "undefine process", 0),
    CPCD_ARG("id", Int, Mandatory, "Id of process"),
    CPCD_ARG_ALIAS("i", "id", 0),

    CPCD_CMD("start process", &CPCDAPISession::startProcess, ""),
    CPCD_ARG("id", Int, Mandatory, "Id of process"),

    CPCD_CMD("stop process", &CPCDAPISession::stopProcess, ""),
    CPCD_ARG("id", Int, Mandatory, "Id of process"),

    CPCD_CMD("list processes", &CPCDAPISession::listProcesses, ""),

    CPCD_CMD("show version", &CPCDAPISession::showVersion, ""),

    CPCD_CMD("select protocol", &CPCDAPISession::selectProtocol, ""),
    CPCD_ARG("version", Int, Mandatory, "Protocol version to use"),

    CPCD_END()};
CPCDAPISession::CPCDAPISession(NdbSocket &&sock, CPCD &cpcd)
    : SocketServer::Session(m_secure_socket),
      m_cpcd(cpcd),
      m_secure_socket(std::move(sock)),
      m_protocol_version(1) {
  m_input = new SocketInputStream(m_secure_socket, 7 * 24 * 60 * 60000);
  m_output = new SocketOutputStream(m_secure_socket);
  m_parser = new Parser<CPCDAPISession>(commands, *m_input);
}

CPCDAPISession::CPCDAPISession(FILE *f, CPCD &cpcd)
    : SocketServer::Session(m_secure_socket),
      m_cpcd(cpcd),
      m_protocol_version(1) {
  m_input = new FileInputStream(f);
  m_parser = new Parser<CPCDAPISession>(commands, *m_input);
  m_output = 0;
}

CPCDAPISession::~CPCDAPISession() {
  delete m_input;
  delete m_parser;
  if (m_output) delete m_output;
}

void CPCDAPISession::runSession() {
  Parser_t::Context ctx;
  while (!m_stop) {
    m_parser->run(ctx, *this);
    if (ctx.m_currentToken == 0) break;

    m_input->reset_timeout();
    m_output->reset_timeout();

    switch (ctx.m_status) {
      case Parser_t::Ok:
        for (unsigned i = 0; i < ctx.m_aliasUsed.size(); i++)
          ndbout_c("Used alias: %s -> %s", ctx.m_aliasUsed[i]->name,
                   ctx.m_aliasUsed[i]->realName);
        break;
      case Parser_t::NoLine:
      case Parser_t::EmptyLine:
        break;
      default:
        break;
    }
  }
  m_secure_socket.close();
}

void CPCDAPISession::stopSession() {
  CPCD::RequestStatus rs;
  for (unsigned i = 0; i < m_temporaryProcesses.size(); i++) {
    Uint32 id = m_temporaryProcesses[i];
    m_cpcd.stopProcess(id, getSessionid(), &rs);
    m_cpcd.undefineProcess(id, getSessionid(), &rs);
  }
}

void CPCDAPISession::loadFile() {
  Parser_t::Context ctx;
  while (!m_stop) {
    m_parser->run(ctx, *this);
    if (ctx.m_currentToken == 0) break;

    switch (ctx.m_status) {
      case Parser_t::Ok:
        for (unsigned i = 0; i < ctx.m_aliasUsed.size(); i++)
          ndbout_c("Used alias: %s -> %s", ctx.m_aliasUsed[i]->name,
                   ctx.m_aliasUsed[i]->realName);
        break;
      case Parser_t::NoLine:
      case Parser_t::EmptyLine:
        break;
      default:
        break;
    }
  }
}

void CPCDAPISession::defineProcess(Parser_t::Context & /* unused */,
                                   const class Properties &args) {
  int id;
  CPCD::RequestStatus rs;
  bool ret = m_cpcd.defineProcess(args, getSessionid(), &rs, &id);
  if (!m_cpcd.loadingProcessList) {
    m_output->println("define process");
    m_output->println("status: %d", rs.getStatus());
    if (ret == true) {
      m_output->println("id: %d", id);

      BaseString procType;
      args.get("type", procType);
      CPCD::ProcessType processType(procType.c_str());
      if (processType == CPCD::ProcessType::TEMPORARY) {
        m_temporaryProcesses.push_back(id);
      }
    } else {
      m_output->println("errormessage: %s", rs.getErrMsg());
    }
    m_output->println("%s", "");
  }
}

void CPCDAPISession::undefineProcess(Parser_t::Context & /* unused */,
                                     const class Properties &args) {
  Uint32 id;
  CPCD::RequestStatus rs;

  args.get("id", &id);
  bool ret = m_cpcd.undefineProcess(id, getSessionid(), &rs);

  for (unsigned i = 0; i < m_temporaryProcesses.size(); i++) {
    if (static_cast<int>(id) == m_temporaryProcesses[i]) {
      m_temporaryProcesses.erase(i);
      break;
    }
  }

  m_output->println("undefine process");
  m_output->println("id: %d", id);
  m_output->println("status: %d", rs.getStatus());
  if (!ret) m_output->println("errormessage: %s", rs.getErrMsg());

  m_output->println("%s", "");
}

void CPCDAPISession::startProcess(Parser_t::Context & /* unused */,
                                  const class Properties &args) {
  Uint32 id;
  CPCD::RequestStatus rs;

  args.get("id", &id);
  const int ret = m_cpcd.startProcess(id, getSessionid(), &rs);

  if (!m_cpcd.loadingProcessList) {
    m_output->println("start process");
    m_output->println("id: %d", id);
    m_output->println("status: %d", rs.getStatus());
    if (!ret) m_output->println("errormessage: %s", rs.getErrMsg());
    m_output->println("%s", "");
  }
}

void CPCDAPISession::stopProcess(Parser_t::Context & /* unused */,
                                 const class Properties &args) {
  Uint32 id;
  CPCD::RequestStatus rs;

  args.get("id", &id);
  int ret = m_cpcd.stopProcess(id, getSessionid(), &rs);

  m_output->println("stop process");
  m_output->println("id: %d", id);
  m_output->println("status: %d", rs.getStatus());
  if (!ret) m_output->println("errormessage: %s", rs.getErrMsg());

  m_output->println("%s", "");
}

static const char *propToString(Properties *prop, const char *key) {
  static char buf[32];
  const char *retval = NULL;
  PropertiesType pt;

  prop->getTypeOf(key, &pt);
  switch (pt) {
    case PropertiesType_Uint32:
      Uint32 val;
      prop->get(key, &val);
      BaseString::snprintf(buf, sizeof buf, "%d", val);
      retval = buf;
      break;
    case PropertiesType_char:
      const char *str;
      prop->get(key, &str);
      retval = str;
      break;
    default:
      BaseString::snprintf(buf, sizeof buf, "(unknown)");
      retval = buf;
  }
  return retval;
}

void CPCDAPISession::printProperty(Properties *prop, const char *key) {
  m_output->println("%s: %s", key, propToString(prop, key));
}

void CPCDAPISession::printLongString(const char *key, const char *value) {
  size_t remaining = strlen(value);
  bool append = false;

  do {
    const char *fmt = (append ? "+%s:\"%.*s\"\n" : "%s:\"%.*s\"\n");
    const int reserved_bytes_for_format = 5;  // 2 x '"', ':', '\n', '\0'
    const int reserved_byte_for_plus_sign = 1;
    const size_t keylen = strlen(key);

    size_t size =
        Parser_t::Context::MaxParseBytes - keylen - reserved_bytes_for_format;
    size -= append ? reserved_byte_for_plus_sign : 0;
    if (size > remaining) {
      size = remaining;
    }
    m_output->print(fmt, key, (int)size, value);

    value += size;
    remaining -= size;
    append = true;
  } while (remaining > 0);
}

void CPCDAPISession::listProcesses(Parser_t::Context & /* unused */,
                                   const class Properties & /* unused */) {
  m_cpcd.m_processes.lock();
  MutexVector<CPCD::Process *> *proclist = m_cpcd.getProcessList();

  m_output->println("start processes");
  m_output->println("%s", "");

  for (unsigned i = 0; i < proclist->size(); i++) {
    CPCD::Process *p = (*proclist)[i];

    m_output->println("process");

    m_output->println("id: %d", p->m_id);
    m_output->println("name: %s", p->m_name.c_str());
    m_output->println("path: %s", p->m_path.c_str());
    printLongString("args", p->m_args.c_str());
    m_output->println("type: %s", p->m_type.c_str());
    m_output->println("cwd: %s", p->m_cwd.c_str());
    printLongString("env", p->m_env.c_str());
    m_output->println("owner: %s", p->m_owner.c_str());
    m_output->println("group: %s", p->m_group.c_str());
    m_output->println("runas: %s", p->m_runas.c_str());

    if (may_print_process_cpuset()) {
      printLongString("cpuset", p->m_cpuset.c_str());
    }

    m_output->println("stdin: %s", p->m_stdin.c_str());
    m_output->println("stdout: %s", p->m_stdout.c_str());
    m_output->println("stderr: %s", p->m_stderr.c_str());
    m_output->println("ulimit: %s", p->m_ulimit.c_str());
    m_output->println("shutdown: %s", p->m_shutdown_options.c_str());
    switch (p->m_status) {
      case STOPPED:
        m_output->println("status: stopped");
        break;
      case STARTING:
        m_output->println("status: starting");
        break;
      case RUNNING:
        m_output->println("status: running");
        break;
      case STOPPING:
        m_output->println("status: stopping");
        break;
    }

    m_output->println("%s", "");
  }

  m_output->println("end processes");
  m_output->println("%s", "");

  m_cpcd.m_processes.unlock();
}

void CPCDAPISession::showVersion(Parser_t::Context & /* unused */,
                                 const class Properties & /*args*/) {
  CPCD::RequestStatus rs;

  m_output->println("show version");

  m_output->println("supported protocol: %u", CPCD::CPC_PROTOCOL_VERSION);
  m_output->println("effective protocol: %u", m_protocol_version);
  m_output->println("%s", "");
}

void CPCDAPISession::selectProtocol(Parser_t::Context & /* unused */,
                                    const class Properties &args) {
  Uint32 version;
  CPCD::RequestStatus rs;

  args.get("version", &version);
  if (version < 1) {
    rs.err(Error, "Invalid protocol version");
  } else if (version > CPCD::CPC_PROTOCOL_VERSION) {
    rs.err(Error, "Unsupported protocol version");
  } else {
    m_protocol_version = version;
  }

  m_output->println("select protocol");
  m_output->println("status: %d", rs.getStatus());
  if (rs.getStatus() != 0)
    m_output->println("errormessage: %s", rs.getErrMsg());
  m_output->println("%s", "");
}

template class Vector<ParserRow<CPCDAPISession> const *>;
