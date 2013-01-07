/*
   Copyright (C) 2003-2008 MySQL AB, 2008-2010 Sun Microsystems, Inc.
    All rights reserved. Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef MGMAPI_SERVICE_HPP
#define MGMAPI_SERVICE_HPP

#include <SocketServer.hpp>
#include <NdbSleep.h>
#include <Parser.hpp>
#include <OutputStream.hpp>
#include <InputStream.hpp>

#include "MgmtSrvr.hpp"

class MgmApiSession : public SocketServer::Session
{
  static void list_session(SocketServer::Session *_s, void *data);
  static void get_session(SocketServer::Session *_s, void *data);
private:
  typedef Parser<MgmApiSession> Parser_t;

  class MgmtSrvr & m_mgmsrv;
  InputStream *m_input;
  OutputStream *m_output;
  Parser_t *m_parser;
  char m_err_str[1024];
  int m_stopSelf; // -1 is restart, 0 do nothing, 1 stop
  NdbMutex *m_mutex;

  // for listing sessions and other fun:
  Parser_t::Context *m_ctx;
  Uint64 m_session_id;

  int m_errorInsert;

  BaseString m_name;
  const char* name() { return m_name.c_str(); }

  const char *get_error_text(int err_no)
  { return m_mgmsrv.getErrorText(err_no, m_err_str, sizeof(m_err_str)); }

public:
  MgmApiSession(class MgmtSrvr & mgm, NDB_SOCKET_TYPE sock, Uint64 session_id);
  virtual ~MgmApiSession();
  void runSession();

  static const unsigned SOCKET_TIMEOUT = 30000;

  void getConfig(Parser_t::Context &ctx, const class Properties &args);
  void setConfig(Parser_t::Context &ctx, const class Properties &args);
  void showConfig(Parser_t::Context &ctx, const class Properties &args);
  void reloadConfig(Parser_t::Context &ctx, const class Properties &args);

  void get_nodeid(Parser_t::Context &ctx, const class Properties &args);
  void getVersion(Parser_t::Context &ctx, const class Properties &args);
  void getStatus(Parser_t::Context &ctx, const class Properties &args);
  void getInfoClusterLog(Parser_t::Context &ctx, const class Properties &args);
  void restart(const class Properties &args, int version);
  void restart_v1(Parser_t::Context &ctx, const class Properties &args);
  void restart_v2(Parser_t::Context &ctx, const class Properties &args);
  void restartAll(Parser_t::Context &ctx, const class Properties &args);
  void insertError(Parser_t::Context &ctx, const class Properties &args);
  void setTrace(Parser_t::Context &ctx, const class Properties &args);
  void logSignals(Parser_t::Context &ctx, const class Properties &args);
  void startSignalLog(Parser_t::Context &ctx, const class Properties &args);
  void stopSignalLog(Parser_t::Context &ctx, const class Properties &args);
  void dumpState(Parser_t::Context &ctx, const class Properties &args);
  void startBackup(Parser_t::Context &ctx, const class Properties &args);
  void abortBackup(Parser_t::Context &ctx, const class Properties &args);
  void enterSingleUser(Parser_t::Context &ctx, const class Properties &args);
  void exitSingleUser(Parser_t::Context &ctx, const class Properties &args);
  void stop_v1(Parser_t::Context &ctx, const class Properties &args);
  void stop_v2(Parser_t::Context &ctx, const class Properties &args);
  void stop(const class Properties &args, int version);
  void stopAll(Parser_t::Context &ctx, const class Properties &args);
  void start(Parser_t::Context &ctx, const class Properties &args);
  void startAll(Parser_t::Context &ctx, const class Properties &args);
  void bye(Parser_t::Context &ctx, const class Properties &args);
  void endSession(Parser_t::Context &ctx, const class Properties &args);
  void setLogLevel(Parser_t::Context &ctx, const class Properties &args);
  void getClusterLogLevel(Parser_t::Context &ctx, 
			  const class Properties &args);
  void setClusterLogLevel(Parser_t::Context &ctx, 
			  const class Properties &args);
  void setLogFilter(Parser_t::Context &ctx, const class Properties &args);

  void setParameter(Parser_t::Context &ctx, const class Properties &args);
  void setConnectionParameter(Parser_t::Context &ctx,
			      const class Properties &args);
  void getConnectionParameter(Parser_t::Context &ctx,
			      Properties const &args);

  void listen_event(Parser_t::Context &ctx, const class Properties &args);

  void purge_stale_sessions(Parser_t::Context &ctx, const class Properties &args);
  void check_connection(Parser_t::Context &ctx, const class Properties &args);

  void transporter_connect(Parser_t::Context &ctx, Properties const &args);

  void get_mgmd_nodeid(Parser_t::Context &ctx, Properties const &args);

  void report_event(Parser_t::Context &ctx, Properties const &args);

  void listSessions(Parser_t::Context &ctx, Properties const &args);

  void getSessionId(Parser_t::Context &ctx, Properties const &args);
  void getSession(Parser_t::Context &ctx, Properties const &args);

  void create_nodegroup(Parser_t::Context &ctx, Properties const &args);
  void drop_nodegroup(Parser_t::Context &ctx, Properties const &args);

  void show_variables(Parser_t::Context &ctx, Properties const &args);

  void dump_events(Parser_t::Context &ctx, Properties const &args);
};

class MgmApiService : public SocketServer::Service {
  MgmtSrvr& m_mgmsrv;
  Uint64 m_next_session_id; // Protected by m_sessions mutex it SocketServer
public:
  MgmApiService(MgmtSrvr& mgm):
    m_mgmsrv(mgm),
    m_next_session_id(1) {}

  SocketServer::Session * newSession(NDB_SOCKET_TYPE socket){
    return new MgmApiSession(m_mgmsrv, socket, m_next_session_id++);
  }
};

static const char* str_null(const char* str)
{
  return (str ? str : "(null)");
}

static const char* yes_no(bool value)
{
  return (value ? "yes" : "no");
}


#endif
