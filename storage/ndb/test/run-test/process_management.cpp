/*
   Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

#include <NdbSleep.h>
#include "process_management.hpp"

bool ProcessManagement::startAllProcesses() {
  if (clusterProcessesStatus == ProcessesStatus::RUNNING) {
    g_logger.debug("All processes already RUNNING. No action required");
    return true;
  }

  if (clusterProcessesStatus == ProcessesStatus::ERROR) {
    g_logger.debug("Processes in ERROR status. Stopping all processes first");
    if (!stopAllProcesses()) {
      g_logger.warning("Failure to stop processes while in ERROR status");
      return false;
    }
  }
  assert(clusterProcessesStatus == ProcessesStatus::STOPPED);

  if (!setupHostsFilesystem()) {
    g_logger.warning("Failed to setup hosts filesystem");
    return false;
  }

  if (!startClusters()) {
    clusterProcessesStatus = ProcessesStatus::ERROR;
    g_logger.warning("Unable to start all processes: ERROR status");
    g_logger.debug("Trying to stop all processes to recover from failure");

    if (!stopAllProcesses()) {
      g_logger.warning("Failed to stop all processes during recovery");
    }
    return false;
  }

  clusterProcessesStatus = ProcessesStatus::RUNNING;
  g_logger.debug("All processes RUNNING");
  return true;
}

bool ProcessManagement::stopAllProcesses() {
  if (clusterProcessesStatus == ProcessesStatus::STOPPED) {
    g_logger.debug("All processes already STOPPED. No action required");
    return true;
  }

  if (!shutdownProcesses(atrt_process::AP_ALL)) {
    clusterProcessesStatus = ProcessesStatus::ERROR;
    g_logger.debug("Unable to stop all processes: ERROR status");
    return false;
  }

  clusterProcessesStatus = ProcessesStatus::STOPPED;
  g_logger.debug("All processes STOPPED");
  return true;
}

bool ProcessManagement::startClientProcesses() {
  if (!startProcesses(ProcessManagement::P_CLIENTS)) {
    clusterProcessesStatus = ProcessesStatus::ERROR;
    g_logger.debug("Unable to start client processes: ERROR status");
    return false;
  }
  return true;
}

bool ProcessManagement::stopClientProcesses() {
  if (!shutdownProcesses(P_CLIENTS)) {
    clusterProcessesStatus = ProcessesStatus::ERROR;
    g_logger.debug("Unable to stop client processes: ERROR status");
    return false;
  }
  return true;
}

int ProcessManagement::updateProcessesStatus() {
  if (!updateStatus(atrt_process::AP_ALL)) {
    g_logger.warning("Failed to update status for all processes");
    return ERR_CRITICAL;
  }
  return checkNdbOrServersFailures();
}

bool ProcessManagement::startClusters() {
  if (!start(P_NDB | P_SERVERS)) {
    g_logger.critical("Failed to start server processes");
    return false;
  }

  if (!setup_db(config)) {
    g_logger.critical("Failed to setup database");
    return false;
  }

  if (!checkClusterStatus(atrt_process::AP_ALL)) {
    g_logger.critical("Cluster start up failed");
    return false;
  }

  return true;
}

bool ProcessManagement::shutdownProcesses(int types) {
  const char *p_type = getProcessTypeName(types);

  g_logger.info("Stopping %s processes", p_type);

  if (!stopProcesses(types)) {
    g_logger.critical("Failed to stop %s processes", p_type);
    return false;
  }

  if (!waitForProcessesToStop(types)) {
    g_logger.critical("Failed to stop %s processes", p_type);
    return false;
  }
  return true;
}

bool ProcessManagement::start(unsigned proc_mask) {
  if (proc_mask & atrt_process::AP_NDB_MGMD)
    if (!startProcesses(atrt_process::AP_NDB_MGMD)) return false;

  if (proc_mask & atrt_process::AP_NDBD) {
    if (!connectNdbMgm()) {
      return false;
    }

    if (!startProcesses(atrt_process::AP_NDBD)) return false;

    if (!waitNdb(NDB_MGM_NODE_STATUS_NOT_STARTED)) return false;

    for (Uint32 i = 0; i < 3; i++)
      if (waitNdb(NDB_MGM_NODE_STATUS_STARTED)) goto started;
    return false;
  }

started:
  if (!startProcesses(P_SERVERS & proc_mask)) return false;

  return true;
}

bool ProcessManagement::startProcesses(int types) {
  for (unsigned i = 0; i < config.m_processes.size(); i++) {
    atrt_process &proc = *config.m_processes[i];
    if (IF_WIN(!(proc.m_type & atrt_process::AP_MYSQLD), 1) &&
        (types & proc.m_type) != 0 && proc.m_proc.m_path != "") {
      if (!startProcess(proc)) {
        return false;
      }
    }
  }
  return true;
}

bool ProcessManagement::startProcess(atrt_process &proc, bool run_setup) {
  if (proc.m_proc.m_id != -1) {
    g_logger.critical("starting already started process: %u",
                      (unsigned)proc.m_index);
    return false;
  }

  if (run_setup) {
    BaseString tmp = setup_progname;
    tmp.appfmt(" %s %s/ %s", proc.m_host->m_hostname.c_str(),
               proc.m_proc.m_cwd.c_str(), proc.m_proc.m_cwd.c_str());

    g_logger.debug("system(%s)", tmp.c_str());
    const int r1 = sh(tmp.c_str());
    if (r1 != 0) {
      g_logger.critical("Failed to setup process");
      return false;
    }
  }

  /**
   * For MySQL server program we need to pass the correct basedir.
   */
  const bool mysqld = proc.m_type & atrt_process::AP_MYSQLD;
  if (mysqld) {
    BaseString basedir;
    /**
     * If MYSQL_BASE_DIR is set use that for basedir.
     */
    ssize_t pos = proc.m_proc.m_env.indexOf("MYSQL_BASE_DIR=");
    if (pos > 0) {
      pos = proc.m_proc.m_env.indexOf(" MYSQL_BASE_DIR=");
      if (pos != -1) pos++;
    }
    if (pos >= 0) {
      pos += strlen("MYSQL_BASE_DIR=");
      ssize_t endpos = proc.m_proc.m_env.indexOf(' ', pos);
      if (endpos == -1) endpos = proc.m_proc.m_env.length();
      basedir = proc.m_proc.m_env.substr(pos, endpos);
    } else {
      /**
       * If no MYSQL_BASE_DIR set, derive basedir from program path.
       * Assuming that program path is on the form
       *   <basedir>/{bin,sql}/mysqld
       */
      const BaseString sep("/");
      Vector<BaseString> dir_parts;
      int num_of_parts = proc.m_proc.m_path.split(dir_parts, sep);
      dir_parts.erase(num_of_parts - 1);  // remove trailing /mysqld
      dir_parts.erase(num_of_parts - 2);  // remove trailing /bin
      num_of_parts -= 2;
      basedir.assign(dir_parts, sep);
    }
    if (proc.m_proc.m_args.indexOf("--basedir=") == -1) {
      proc.m_proc.m_args.appfmt(" --basedir=%s", basedir.c_str());
      g_logger.info("appended '--basedir=%s' to mysqld process",
                    basedir.c_str());
    }
  }
  BaseString save_args(proc.m_proc.m_args);
  {
    Properties reply;
    if (proc.m_host->m_cpcd->define_process(proc.m_proc, reply) != 0) {
      BaseString msg;
      reply.get("errormessage", msg);
      g_logger.error("Unable to define process: %s", msg.c_str());
      if (mysqld) {
        proc.m_proc.m_args = save_args; /* restore args */
      }
      return false;
    }
  }
  if (mysqld) {
    proc.m_proc.m_args = save_args; /* restore args */
  }
  {
    Properties reply;
    if (proc.m_host->m_cpcd->start_process(proc.m_proc.m_id, reply) != 0) {
      BaseString msg;
      reply.get("errormessage", msg);
      g_logger.error("Unable to start process: %s", msg.c_str());
      return false;
    }
  }
  return true;
}

bool ProcessManagement::stopProcesses(int types) {
  int failures = 0;

  for (unsigned i = 0; i < config.m_processes.size(); i++) {
    atrt_process &proc = *config.m_processes[i];
    if ((types & proc.m_type) != 0) {
      if (!stopProcess(proc)) {
        failures++;
      }
    }
  }
  return failures == 0;
}

bool ProcessManagement::stopProcess(atrt_process &proc) {
  if (proc.m_proc.m_id == -1) {
    return true;
  }

  if (proc.m_type == atrt_process::AP_MYSQLD) {
    disconnect_mysqld(proc);
  }

  {
    Properties reply;
    if (proc.m_host->m_cpcd->stop_process(proc.m_proc.m_id, reply) != 0) {
      Uint32 status;
      reply.get("status", &status);
      if (status != 4) {
        BaseString msg;
        reply.get("errormessage", msg);
        g_logger.error(
            "Unable to stop process id: %d host: %s cmd: %s, "
            "msg: %s, status: %d",
            proc.m_proc.m_id, proc.m_host->m_hostname.c_str(),
            proc.m_proc.m_path.c_str(), msg.c_str(), status);
        return false;
      }
    }
  }
  {
    Properties reply;
    if (proc.m_host->m_cpcd->undefine_process(proc.m_proc.m_id, reply) != 0) {
      BaseString msg;
      reply.get("errormessage", msg);
      g_logger.error("Unable to stop process id: %d host: %s cmd: %s, msg: %s",
                     proc.m_proc.m_id, proc.m_host->m_hostname.c_str(),
                     proc.m_proc.m_path.c_str(), msg.c_str());
      return false;
    }
  }

  return true;
}

bool ProcessManagement::connectNdbMgm() {
  for (unsigned i = 0; i < config.m_processes.size(); i++) {
    atrt_process &proc = *config.m_processes[i];
    if ((proc.m_type & atrt_process::AP_NDB_MGMD) != 0) {
      if (!connectNdbMgm(proc)) {
        return false;
      }
    }
  }

  return true;
}

bool ProcessManagement::connectNdbMgm(atrt_process &proc) {
  NdbMgmHandle handle = ndb_mgm_create_handle();
  if (handle == 0) {
    g_logger.critical("Unable to create mgm handle");
    return false;
  }
  BaseString tmp = proc.m_host->m_hostname;
  const char *val;
  proc.m_options.m_loaded.get("--PortNumber=", &val);
  tmp.appfmt(":%s", val);

  if (ndb_mgm_set_connectstring(handle, tmp.c_str())) {
    g_logger.critical("Unable to create parse connectstring");
    return false;
  }

  if (ndb_mgm_connect(handle, 30, 1, 0) != -1) {
    proc.m_ndb_mgm_handle = handle;
    return true;
  }

  g_logger.critical("Unable to connect to ndb mgm %s", tmp.c_str());
  return false;
}

bool ProcessManagement::waitNdb(int goal) {
  goal = remap(goal);

  size_t cnt = 0;
  for (unsigned i = 0; i < config.m_clusters.size(); i++) {
    atrt_cluster *cluster = config.m_clusters[i];

    if (strcmp(cluster->m_name.c_str(), ".atrt") == 0) {
      /**
       * skip atrt mysql
       */
      cnt++;
      continue;
    }

    /**
     * Get mgm handle for cluster
     */
    NdbMgmHandle handle = 0;
    for (unsigned j = 0; j < cluster->m_processes.size(); j++) {
      atrt_process &proc = *cluster->m_processes[j];
      if ((proc.m_type & atrt_process::AP_NDB_MGMD) != 0) {
        handle = proc.m_ndb_mgm_handle;
        break;
      }
    }

    if (handle == 0) {
      return true;
    }

    if (goal == NDB_MGM_NODE_STATUS_STARTED) {
      /**
       * 1) wait NOT_STARTED
       * 2) send start
       * 3) wait STARTED
       */
      if (!waitNdb(NDB_MGM_NODE_STATUS_NOT_STARTED)) return false;

      ndb_mgm_start(handle, 0, 0);
    }

    struct ndb_mgm_cluster_state *state;

    time_t now = time(0);
    time_t end = now + 360;
    int min = remap(NDB_MGM_NODE_STATUS_NO_CONTACT);
    int min2 = goal;

    while (now < end) {
      /**
       * 1) retrieve current state
       */
      state = 0;
      do {
        state = ndb_mgm_get_status(handle);
        if (state == 0) {
          const int err = ndb_mgm_get_latest_error(handle);
          g_logger.error("Unable to poll db state: %d %s %s",
                         ndb_mgm_get_latest_error(handle),
                         ndb_mgm_get_latest_error_msg(handle),
                         ndb_mgm_get_latest_error_desc(handle));
          if (err == NDB_MGM_SERVER_NOT_CONNECTED && connectNdbMgm()) {
            g_logger.error("Reconnected...");
            continue;
          }
          return false;
        }
      } while (state == 0);
      NdbAutoPtr<void> tmp(state);

      min2 = goal;
      for (int j = 0; j < state->no_of_nodes; j++) {
        if (state->node_states[j].node_type == NDB_MGM_NODE_TYPE_NDB) {
          const int s = remap(state->node_states[j].node_status);
          min2 = (min2 < s ? min2 : s);

          if (s < remap(NDB_MGM_NODE_STATUS_NO_CONTACT) ||
              s > NDB_MGM_NODE_STATUS_STARTED) {
            g_logger.critical("Strange DB status during start: %d %d", j, min2);
            return false;
          }

          if (min2 < min) {
            g_logger.critical("wait ndb failed node: %d %d %d %d",
                              state->node_states[j].node_id, min, min2, goal);
          }
        }
      }

      if (min2 < min) {
        g_logger.critical("wait ndb failed %d %d %d", min, min2, goal);
        return false;
      }

      if (min2 == goal) {
        cnt++;
        goto next;
      }

      min = min2;
      now = time(0);
    }

    g_logger.critical("wait ndb timed out %d %d %d", min, min2, goal);
    break;

  next:;
  }

  return cnt == config.m_clusters.size();
}

bool ProcessManagement::checkClusterStatus(int types) {
  if (!updateStatus(types)) {
    g_logger.critical("Failed to get updated status for all processes");
    return false;
  }
  if (checkNdbOrServersFailures() != 0) {
    return false;
  }
  return true;
}

int ProcessManagement::checkNdbOrServersFailures() {
  int failed_processes = 0;
  const int types = P_NDB | P_SERVERS;
  for (unsigned i = 0; i < config.m_processes.size(); i++) {
    atrt_process &proc = *config.m_processes[i];
    bool skip =
      proc.m_atrt_stopped || IF_WIN(proc.m_type & atrt_process::AP_MYSQLD, 0);
    bool isRunning = proc.m_proc.m_status == "running";
    if ((types & proc.m_type) != 0 && !isRunning && !skip) {
      g_logger.critical("%s #%d not running on %s", proc.m_name.c_str(),
                        proc.m_index, proc.m_host->m_hostname.c_str());
      failed_processes |= proc.m_type;
    }
  }
  if ((failed_processes & P_NDB) && (failed_processes & P_SERVERS)) {
    return ERR_NDB_AND_SERVERS_FAILED;
  }
  if ((failed_processes & P_NDB) != 0) {
    return ERR_NDB_FAILED;
  }
  if ((failed_processes & P_SERVERS) != 0) {
    return ERR_SERVERS_FAILED;
  }
  return 0;
}

bool ProcessManagement::updateStatus(int types, bool fail_on_missing) {
  Vector<Vector<SimpleCpcClient::Process> > m_procs;

  Vector<SimpleCpcClient::Process> dummy;
  m_procs.fill(config.m_hosts.size(), dummy);
  for (unsigned i = 0; i < config.m_hosts.size(); i++) {
    if (config.m_hosts[i]->m_hostname.length() == 0) continue;

    Properties p;
    config.m_hosts[i]->m_cpcd->list_processes(m_procs[i], p);
  }

  for (unsigned i = 0; i < config.m_processes.size(); i++) {
    atrt_process &proc = *config.m_processes[i];

    if (proc.m_proc.m_id == -1 || (proc.m_type & types) == 0) {
      continue;
    }

    Vector<SimpleCpcClient::Process> &h_procs = m_procs[proc.m_host->m_index];
    bool found = false;
    for (unsigned j = 0; j < h_procs.size() && !found; j++) {
      if (proc.m_proc.m_id == h_procs[j].m_id) {
        found = true;
        proc.m_proc.m_status = h_procs[j].m_status;
      }
    }

    if (found) continue;

    if (!fail_on_missing) {
      proc.m_proc.m_id = -1;
      proc.m_proc.m_status.clear();
    } else {
      g_logger.error("update_status: not found");
      g_logger.error("id: %d host: %s cmd: %s", proc.m_proc.m_id,
                     proc.m_host->m_hostname.c_str(),
                     proc.m_proc.m_path.c_str());
      for (unsigned j = 0; j < h_procs.size(); j++) {
        g_logger.error("found: %d %s", h_procs[j].m_id,
                       h_procs[j].m_path.c_str());
      }
      return false;
    }
  }
  return true;
}

bool ProcessManagement::waitForProcessesToStop(int types, int retries,
                                     int wait_between_retries_s) {
  for (int attempts = 0; attempts < retries; attempts++) {
    bool last_attempt = attempts == (retries - 1);

    updateStatus(types, false);

    int found = 0;
    for (unsigned i = 0; i < config.m_processes.size(); i++) {
      atrt_process &proc = *config.m_processes[i];
      if ((types & proc.m_type) == 0 || proc.m_proc.m_id == -1) continue;

      found++;

      if (!last_attempt) continue;  // skip logging
      g_logger.error(
          "Failed to stop process id: %d host: %s status: %s cmd: %s",
          proc.m_proc.m_id, proc.m_host->m_hostname.c_str(),
          proc.m_proc.m_status.c_str(), proc.m_proc.m_path.c_str());
    }

    if (found == 0) return true;

    if (!last_attempt) NdbSleep_SecSleep(wait_between_retries_s);
  }

  return false;
}

bool ProcessManagement::waitForProcessToStop(atrt_process &proc, int retries,
                                   int wait_between_retries_s) {
  for (int attempts = 0; attempts < retries; attempts++) {
    updateStatus(proc.m_type, false);

    if (proc.m_proc.m_id == -1) return true;

    bool last_attempt = attempts == (retries - 1);
    if (!last_attempt) {
      NdbSleep_SecSleep(wait_between_retries_s);
      continue;
    }

    g_logger.error("Failed to stop process id: %d host: %s status: %s cmd: %s",
                   proc.m_proc.m_id, proc.m_host->m_hostname.c_str(),
                   proc.m_proc.m_status.c_str(), proc.m_proc.m_path.c_str());
  }

  return false;
}

bool ProcessManagement::setupHostsFilesystem() {
  if (!setup_directories(config, 2)) {
    g_logger.critical("Failed to setup directories");
    return false;
  }

  if (!setup_files(config, 2, 1)) {
    g_logger.critical("Failed to setup files");
    return false;
  }

  if (!setup_hosts(config)) {
    g_logger.critical("Failed to setup hosts");
    return false;
  }

  return true;
}

int ProcessManagement::remap(int i) {
  if (i == NDB_MGM_NODE_STATUS_NO_CONTACT) return NDB_MGM_NODE_STATUS_UNKNOWN;
  if (i == NDB_MGM_NODE_STATUS_UNKNOWN) return NDB_MGM_NODE_STATUS_NO_CONTACT;
  return i;
}

const char* ProcessManagement::getProcessTypeName(int types) {
  switch (types) {
    case ProcessManagement::P_CLIENTS:
      return "client";
    case ProcessManagement::P_NDB:
      return "ndb";
    case ProcessManagement::P_SERVERS:
      return "server";
    default:
      return "all";
  }
}
