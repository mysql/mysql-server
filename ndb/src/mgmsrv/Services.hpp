/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef MGMAPI_SERVICE_HPP
#define MGMAPI_SERVICE_HPP

#include <SocketServer.hpp>
#include <NdbSleep.h>
#include <Parser.hpp>
#include <OutputStream.hpp>
#include <InputStream.hpp>

#include "MgmtSrvr.hpp"

/** Undefine this to remove backwards compatibility for "GET CONFIG". */
#define MGM_GET_CONFIG_BACKWARDS_COMPAT

class MgmApiSession : public SocketServer::Session
{
  static void stop_session_if_timed_out(SocketServer::Session *_s, void *data);
  static void stop_session_if_not_connected(SocketServer::Session *_s, void *data);
private:
  typedef Parser<MgmApiSession> Parser_t;

  class MgmtSrvr & m_mgmsrv;
  InputStream *m_input;
  OutputStream *m_output;
  Parser_t *m_parser;
  MgmtSrvr::Allocated_resources *m_allocated_resources;
  char m_err_str[1024];
  int m_stopSelf; // -1 is restart, 0 do nothing, 1 stop

  void getConfig_common(Parser_t::Context &ctx,
			const class Properties &args,
			bool compat = false);
  const char *get_error_text(int err_no)
  { return m_mgmsrv.getErrorText(err_no, m_err_str, sizeof(m_err_str)); }

public:
  MgmApiSession(class MgmtSrvr & mgm, NDB_SOCKET_TYPE sock);  
  virtual ~MgmApiSession();
  void runSession();

  void getStatPort(Parser_t::Context &ctx, const class Properties &args);
  void getConfig(Parser_t::Context &ctx, const class Properties &args);
#ifdef MGM_GET_CONFIG_BACKWARDS_COMPAT
  void getConfig_old(Parser_t::Context &ctx);
#endif /* MGM_GET_CONFIG_BACKWARDS_COMPAT */

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
};

class MgmApiService : public SocketServer::Service {
  class MgmtSrvr * m_mgmsrv;
public:
  MgmApiService(){
    m_mgmsrv = 0;
  }
  
  void setMgm(class MgmtSrvr * mgmsrv){
    m_mgmsrv = mgmsrv;
  }
  
  SocketServer::Session * newSession(NDB_SOCKET_TYPE socket){
    return new MgmApiSession(* m_mgmsrv, socket);
  }
};

#endif
