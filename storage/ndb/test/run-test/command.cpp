/*
   Copyright (c) 2008, 2024, Oracle and/or its affiliates.

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

#include <AtrtClient.hpp>
#include "atrt.hpp"
#include "process_management.hpp"
#include "util/require.h"

MYSQL *find_atrtdb_client(atrt_config &config) {
  atrt_cluster *cluster = 0;
  for (unsigned i = 0; i < config.m_clusters.size(); i++) {
    if (strcmp(config.m_clusters[i]->m_name.c_str(), ".atrt") == 0) {
      cluster = config.m_clusters[i];

      for (unsigned i = 0; i < cluster->m_processes.size(); i++) {
        if (cluster->m_processes[i]->m_type == atrt_process::AP_CLIENT) {
          atrt_process *atrt_client = cluster->m_processes[i];
          if (!atrt_client) return NULL; /* No atrt db */

          atrt_process *f_mysqld = atrt_client->m_mysqld;
          require(f_mysqld);

          return &f_mysqld->m_mysql;
        }
      }
      break;
    }
  }
  return NULL;
}

static bool ack_command(AtrtClient &atrtdb, int command_id, const char *state) {
  BaseString sql;
  sql.assfmt("UPDATE command SET state = '%s' WHERE id = %d", state,
             command_id);
  return atrtdb.doQuery(sql);
}

static BaseString set_env_var(const BaseString &existing,
                              const BaseString &name, const BaseString &value) {
  /* Split existing on space
   * (may have issues with env vars with spaces)
   * Split assignments on =
   * Where name == name, output new value
   */
  BaseString newEnv;
  Vector<BaseString> assignments;
  int assignmentCount = existing.split(assignments, BaseString(" "));

  for (int i = 0; i < assignmentCount; i++) {
    Vector<BaseString> terms;
    int termCount = assignments[i].split(terms, BaseString("="));

    if (termCount) {
      if (strcmp(name.c_str(), terms[0].c_str()) == 0) {
        /* Found element */
        newEnv.append(name);
        newEnv.append('=');
        newEnv.append(value);
      } else {
        newEnv.append(assignments[i]);
      }
    }
    newEnv.append(' ');
  }

  return newEnv;
}

static bool do_change_prefix(atrt_config &config, SqlResultSet &command) {
  const char *new_prefix = g_prefix1 ? g_prefix1 : g_prefix0;
  const char *process_args = command.column("process_args");
  atrt_process &proc = *config.m_processes[command.columnAsInt("process_id")];

  if ((proc.m_type == atrt_process::AP_NDB_API) && proc.m_proc.m_changed) {
    g_logger.critical("Changing API processes back is not supported");
    return false;
  }

  if (!proc.m_proc.m_changed) {
    // Save current proc state
    proc.m_save.m_proc = proc.m_proc;
    proc.m_save.m_saved = true;
    proc.m_proc.m_changed = true;
  } else {
    proc.m_proc = proc.m_save.m_proc;
    proc.m_save.m_saved = false;
    proc.m_proc.m_changed = false;
  }

  if (process_args && strlen(process_args)) {
    /* Beware too long args */
    proc.m_proc.m_args.append(" ");
    proc.m_proc.m_args.append(process_args);
  }

  int new_prefix_idx = proc.m_proc.m_changed ? 1 : 0;
  BaseString newEnv = set_env_var(
      proc.m_proc.m_env, BaseString("MYSQL_BASE_DIR"), BaseString(new_prefix));
  proc.m_proc.m_env.assign(newEnv);

  ssize_t pos = proc.m_proc.m_path.lastIndexOf('/') + 1;
  BaseString exename(proc.m_proc.m_path.substr(pos));

  proc.m_proc.m_path =
      g_resources.getExecutableFullPath(exename.c_str(), new_prefix_idx)
          .c_str();
  if (proc.m_proc.m_path == "") {
    // Attempt to dynamically find executable that was not previously registered
    proc.m_proc.m_path =
        g_resources.findExecutableFullPath(exename.c_str(), new_prefix_idx)
            .c_str();
  }
  if (proc.m_proc.m_path == "") {
    g_logger.critical("Could not find full path for exe %s", exename.c_str());
    return false;
  }

  {
    /**
     * In 5.5...binaries aren't compiled with rpath
     * So we need an explicit LD_LIBRARY_PATH
     * So when upgrading..we need to change LD_LIBRARY_PATH
     * So I hate 5.5...
     */
#if defined(__MACH__)
    ssize_t p0 = proc.m_proc.m_env.indexOf(" DYLD_LIBRARY_PATH=");
    const char *libname = g_resources.LIBMYSQLCLIENT_DYLIB;
#else
    ssize_t p0 = proc.m_proc.m_env.indexOf(" LD_LIBRARY_PATH=");
    const char *libname = g_resources.LIBMYSQLCLIENT_SO;
#endif
    ssize_t p1 = proc.m_proc.m_env.indexOf(' ', p0 + 1);

    BaseString part0 = proc.m_proc.m_env.substr(0, p0);
    BaseString part1 = proc.m_proc.m_env.substr(p1);

    proc.m_proc.m_env.assfmt("%s%s", part0.c_str(), part1.c_str());

    BaseString libdir =
        g_resources.getLibraryDirectory(libname, new_prefix_idx).c_str();
#if defined(__MACH__)
    proc.m_proc.m_env.appfmt(" DYLD_LIBRARY_PATH=%s", libdir.c_str());
#else
    proc.m_proc.m_env.appfmt(" LD_LIBRARY_PATH=%s", libdir.c_str());
#endif
  }

  return true;
}

static bool do_start_process(ProcessManagement &processManagement,
                             atrt_config &config, SqlResultSet &command,
                             AtrtClient &atrtdb) {
  uint process_id = command.columnAsInt("process_id");
  if (process_id > config.m_processes.size()) {
    g_logger.critical("Invalid process id %d", process_id);
    return false;
  }

  atrt_process &proc = *config.m_processes[process_id];

  if (proc.m_atrt_stopped != true) {
    g_logger.info("start process %s failed", proc.m_name.c_str());
    return false;
  }
  proc.m_atrt_stopped = false;

  g_logger.info("starting process - %s", proc.m_name.c_str());
  bool status = processManagement.startProcess(proc, false);
  return status;
}

static bool do_stop_process(ProcessManagement &processManagement,
                            atrt_config &config, SqlResultSet &command,
                            AtrtClient &atrtdb) {
  uint process_id = command.columnAsInt("process_id");

  // Get the process
  if (process_id > config.m_processes.size()) {
    g_logger.critical("Invalid process id %d", process_id);
    return false;
  }

  atrt_process &proc = *config.m_processes[process_id];
  proc.m_atrt_stopped = true;

  g_logger.info("stopping process - %s", proc.m_name.c_str());
  if (!processManagement.stopProcess(proc)) {
    return false;
  }

  g_logger.info("waiting for process to stop...");
  if (!processManagement.waitForProcessToStop(proc)) {
    g_logger.critical("Failed to stop process");
    return false;
  }

  return true;
}

static bool do_change_version(ProcessManagement &processManagement,
                              atrt_config &config, SqlResultSet &command,
                              AtrtClient &atrtdb) {
  if (!do_stop_process(processManagement, config, command, atrtdb)) {
    return false;
  }

  if (!do_change_prefix(config, command)) {
    return false;
  }

  if (!do_start_process(processManagement, config, command, atrtdb)) {
    return false;
  }

  return true;
}

static bool do_reset_proc(ProcessManagement &processManagement,
                          atrt_config &config, SqlResultSet &command,
                          AtrtClient &atrtdb) {
  uint process_id = command.columnAsInt("process_id");
  g_logger.info("Reset process: %d", process_id);

  // Get the process
  if (process_id > config.m_processes.size()) {
    g_logger.critical("Invalid process id %d", process_id);
    return false;
  }
  atrt_process &proc = *config.m_processes[process_id];

  g_logger.info("stopping process...");
  if (!processManagement.stopProcess(proc)) return false;

  if (!processManagement.waitForProcessToStop(proc)) return false;

  if (proc.m_save.m_saved) {
    ndbout << "before: " << proc << endl;

    proc.m_proc = proc.m_save.m_proc;
    proc.m_save.m_saved = false;

    ndbout << "after: " << proc << endl;

  } else {
    ndbout << "process has not changed" << endl;
  }

  return true;
}

bool do_command(ProcessManagement &processManagement, atrt_config &config) {
#ifdef _WIN32
  return true;
#endif

  MYSQL *mysql = find_atrtdb_client(config);
  if (!mysql) return true;

  AtrtClient atrtdb(mysql);
  SqlResultSet command;
  if (!atrtdb.doQuery("SELECT * FROM command "
                      "WHERE state = 'new' ORDER BY id LIMIT 1",
                      command)) {
    g_logger.critical("query failed");
    return false;
  }

  if (command.numRows() == 0) return true;

  uint id = command.columnAsInt("id");
  uint cmd = command.columnAsInt("cmd");
  g_logger.info("Got command, id: %d, cmd: %d", id, cmd);
  // command.print();

  // Set state of command to running
  if (!ack_command(atrtdb, id, "running")) return false;

  switch (cmd) {
    case AtrtClient::ATCT_CHANGE_VERSION:
      if (!do_change_version(processManagement, config, command, atrtdb)) {
        return false;
      }
      break;

    case AtrtClient::ATCT_RESET_PROC:
      if (!do_reset_proc(processManagement, config, command, atrtdb)) {
        return false;
      }
      break;

    case AtrtClient::ATCT_START_PROCESS:
      if (!do_start_process(processManagement, config, command, atrtdb)) {
        return false;
      }
      break;

    case AtrtClient::ATCT_STOP_PROCESS:
      if (!do_stop_process(processManagement, config, command, atrtdb)) {
        return false;
      }
      break;

    case AtrtClient::ATCT_SWITCH_CONFIG:
      if (!do_change_prefix(config, command)) return false;
      break;

    default:
      command.print();
      g_logger.error("got unknown command: %d", cmd);
      return false;
  }

  // Set state of command to done
  if (!ack_command(atrtdb, id, "done")) return false;

  g_logger.info("done!");

  return true;
}
