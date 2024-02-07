/*
   Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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

#ifndef PROCESS_MANAGEMENT_HPP_
#define PROCESS_MANAGEMENT_HPP_

#include "atrt.hpp"
#include "util/TlsKeyManager.hpp"

class ProcessManagement {
 public:
  static const int P_NDB = atrt_process::AP_NDB_MGMD | atrt_process::AP_NDBD;
  static const int P_SERVERS =
      atrt_process::AP_MYSQLD | atrt_process::AP_CUSTOM;
  static const int P_CLIENTS =
      atrt_process::AP_CLIENT | atrt_process::AP_NDB_API;

  ProcessManagement(atrt_config &g_config, const char *g_setup_progname)
      : config(g_config) {
    setup_progname = g_setup_progname;
    clusterProcessesStatus = ProcessesStatus::STOPPED;
  }

  bool startAllProcesses();
  bool stopAllProcesses();
  bool startClientProcesses();
  bool stopClientProcesses();
  bool startProcess(atrt_process &proc, bool run_setup = true);
  bool stopProcess(atrt_process &proc);
  bool waitForProcessToStop(atrt_process &proc, int retries = 60,
                            int wait_between_retries_s = 5);
  int updateProcessesStatus();

 private:
  const char *setup_progname;
  TlsKeyManager tlsKeyManager;

  bool startClusters();
  bool shutdownProcesses(int types);
  bool start(unsigned proc_mask);
  bool startProcesses(int types);
  bool stopProcesses(int types);
  bool stopSingleProcess(atrt_process &proc);
  bool connectNdbMgm();
  bool connectNdbMgm(atrt_process &proc);
  bool checkClusterStatus(int types);
  bool waitNdb(int goal);
  int checkNdbOrServersFailures();
  bool updateStatus(int types, bool fail_on_missing = true);
  bool waitForProcessesToStop(int types = atrt_process::AP_ALL,
                              int retries = 60, int wait_between_retries_s = 5);
  bool setupHostsFilesystem();

  static int remap(int i);
  const char *getProcessTypeName(int types);

  atrt_config &config;
  enum class ProcessesStatus { RUNNING, STOPPED, ERROR } clusterProcessesStatus;
};
#endif  // PROCESS_MANAGEMENT_HPP_
