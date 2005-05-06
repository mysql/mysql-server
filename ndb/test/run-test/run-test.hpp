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

#ifndef atrt_config_hpp
#define atrt_config_hpp

#include <getarg.h>
#include <Vector.hpp>
#include <BaseString.hpp>
#include <Logger.hpp>
#include <mgmapi.h>
#include <CpcClient.hpp>

#undef MYSQL_CLIENT

enum ErrorCodes {
  ERR_OK = 0,
  ERR_NDB_FAILED = 101,
  ERR_SERVERS_FAILED = 102,
  ERR_MAX_TIME_ELAPSED = 103
};

struct atrt_host {
  size_t m_index;
  BaseString m_user;
  BaseString m_base_dir;
  BaseString m_hostname;
  SimpleCpcClient * m_cpcd;
};

struct atrt_process {
  size_t m_index;
  BaseString m_hostname;
  struct atrt_host * m_host;

  enum Type {
    ALL = 255,
    NDB_DB = 1,
    NDB_API = 2,
    NDB_MGM = 4,
    NDB_REP = 8,
    MYSQL_SERVER = 16,
    MYSQL_CLIENT = 32
  } m_type;

  SimpleCpcClient::Process m_proc;
  short m_ndb_mgm_port;
  NdbMgmHandle m_ndb_mgm_handle; // if type == ndb_mgm
};

struct atrt_config {
  BaseString m_key;
  Vector<atrt_host> m_hosts;
  Vector<atrt_process> m_processes;
};

struct atrt_testcase {
  bool m_report;
  bool m_run_all;
  time_t m_max_time;
  BaseString m_command;
  BaseString m_args;
};

extern Logger g_logger;

bool parse_args(int argc, const char** argv);
bool setup_config(atrt_config&);
bool connect_hosts(atrt_config&);
bool connect_ndb_mgm(atrt_config&);
bool wait_ndb(atrt_config&, int ndb_mgm_node_status);
bool start_processes(atrt_config&, int);
bool stop_processes(atrt_config&, int);
bool update_status(atrt_config&, int);
int is_running(atrt_config&, int);
bool gather_result(atrt_config&, int * result);

bool read_test_case(FILE *, atrt_testcase&, int& line);
bool setup_test_case(atrt_config&, const atrt_testcase&);

bool setup_hosts(atrt_config&);

#endif
