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

#include <NdbEnv.h>
#include <NdbSleep.h>
#include <AtrtClient.hpp>
#include <NDBT_Output.hpp>
#include "util/require.h"

AtrtClient::AtrtClient(const char *_group_suffix)
    : SqlClient("atrt", _group_suffix) {}

AtrtClient::AtrtClient(MYSQL *mysql) : SqlClient(mysql) {}

AtrtClient::~AtrtClient() {}

int AtrtClient::writeCommand(AtrtCommandType _type, const Properties &args) {
  if (!isConnected()) return false;

  BaseString sql;
  sql.assfmt("INSERT  command ( ");

  const char *name;
  {
    Properties::Iterator iter(&args);
    while ((name = iter.next())) {
      sql.appfmt("%s, ", name);
    }
  }

  sql.appfmt(" state, cmd) VALUES (");

  {
    Properties::Iterator iter(&args);
    while ((name = iter.next())) {
      PropertiesType t;
      Uint32 val_i;
      BaseString val_s;
      args.getTypeOf(name, &t);
      switch (t) {
        case PropertiesType_Uint32:
          args.get(name, &val_i);
          sql.appfmt("%d, ", val_i);
          break;
        case PropertiesType_char:
          args.get(name, val_s);
          sql.appfmt("'%s', ", val_s.c_str());
          break;
        default:
          require(false);
          break;
      }
    }
  }

  sql.appfmt("'new', %d)", _type);
  SqlResultSet res;
  if (!doQuery(sql, res)) {
    return -1;
  }

  // Return the generated AUTO_INCREMENT id as command id
  const uint64_t last_insert_id = res.insertId();
  return static_cast<int>(last_insert_id);
}

bool AtrtClient::readCommand(uint command_id, SqlResultSet &result) {
  Properties args;
  args.put("0", command_id);
  return runQuery("SELECT * FROM command WHERE id = ?", args, result);
}

bool AtrtClient::doCommand(AtrtCommandType type, const Properties &args) {
  int running_timeout = 10;
  int total_timeout = 120;
  const int commandId = writeCommand(type, args);
  if (commandId == -1) {
    g_err << "Failed to write command" << endl;
    return false;
  }

  while (true) {
    SqlResultSet result;
    if (!readCommand(commandId, result)) {
      result.print();
      g_err << "Failed to read command " << commandId << endl;
      return false;
    }

    // Get first row
    result.next();

    // Check if command has completed
    BaseString state(result.column("state"));
    if (state == "done") {
      return true;
    }

    if (state == "new") {
      if (!running_timeout--) {
        g_err << "Timeout while waiting for command " << commandId
              << " to start run" << endl;
        return false;
      }
    } else if (!total_timeout--) {
      g_err << "Timeout while waiting for result of command " << commandId
            << endl;
      return false;
    }

    NdbSleep_SecSleep(1);
  }

  return false;
}

bool AtrtClient::changeVersion(int process_id, const char *process_args) {
  Properties args;
  args.put("process_id", process_id);
  args.put("process_args", process_args);
  return doCommand(ATCT_CHANGE_VERSION, args);
}

bool AtrtClient::switchConfig(int process_id, const char *process_args) {
  Properties args;
  args.put("process_id", process_id);
  args.put("process_args", process_args);
  return doCommand(ATCT_SWITCH_CONFIG, args);
}

bool AtrtClient::stopProcess(int process_id) {
  Properties args;
  args.put("process_id", process_id);
  return doCommand(ATCT_STOP_PROCESS, args);
}

bool AtrtClient::startProcess(int process_id) {
  Properties args;
  args.put("process_id", process_id);
  return doCommand(ATCT_START_PROCESS, args);
}

bool AtrtClient::resetProc(int process_id) {
  Properties args;
  args.put("process_id", process_id);
  return doCommand(ATCT_RESET_PROC, args);
}

bool AtrtClient::getConnectString(int cluster_id, SqlResultSet &result) {
  Properties args;
  args.put("0", cluster_id);
  return doQuery(
      "SELECT value as connectstring "
      "FROM cluster c, process p, host h, options o "
      "WHERE c.id=p.cluster_id AND p.host_id=h.id AND "
      "p.id=o.process_id AND c.id=? AND "
      "o.name='--ndb-connectstring=' AND type='ndb_mgmd'",
      args, result);
}

bool AtrtClient::getClusters(SqlResultSet &result) {
  Properties args;
  return runQuery("SELECT id, name FROM cluster WHERE name != '.atrt'", args,
                  result);
}

bool AtrtClient::getMgmds(int cluster_id, SqlResultSet &result) {
  Properties args;
  args.put("0", cluster_id);
  return runQuery(
      "SELECT * FROM process WHERE cluster_id=? and type='ndb_mgmd'", args,
      result);
}

bool AtrtClient::getNdbds(int cluster_id, SqlResultSet &result) {
  Properties args;
  args.put("0", cluster_id);
  return runQuery("SELECT * FROM process WHERE cluster_id=? and type='ndbd'",
                  args, result);
}

int AtrtClient::getOwnProcessId() {
  /**
   * Put in env for simplicity
   */
  char buf[100];
  if (NdbEnv_GetEnv("ATRT_PID", buf, sizeof(buf))) {
    return atoi(buf);
  }
  return -1;
}
