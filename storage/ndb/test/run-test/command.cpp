/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


#include "atrt.hpp"
#include <AtrtClient.hpp>



MYSQL* find_atrtdb_client(atrt_config& config)
{
  atrt_cluster* cluster = 0;
  for (size_t i = 0; i<config.m_clusters.size(); i++)
  {
    if (strcmp(config.m_clusters[i]->m_name.c_str(), ".atrt") == 0)
    {
      cluster = config.m_clusters[i];

      for (size_t i = 0; i<cluster->m_processes.size(); i++)
      {
        if (cluster->m_processes[i]->m_type == atrt_process::AP_CLIENT)
        {
          atrt_process* atrt_client= cluster->m_processes[i];
          if (!atrt_client)
            return NULL; /* No atrt db */

          atrt_process* f_mysqld = atrt_client->m_mysqld;
          assert(f_mysqld);

          return &f_mysqld->m_mysql;
        }
      }
      break;
    }
  }
  return NULL;
}



static bool
ack_command(AtrtClient& atrtdb, int command_id, const char* state)
{
  BaseString sql;
  sql.assfmt("UPDATE command SET state = '%s' WHERE id = %d",
             state, command_id);
  return atrtdb.doQuery(sql);
}


Vector<atrt_process> g_saved_procs;

static
bool
do_change_version(atrt_config& config, SqlResultSet& command,
                  AtrtClient& atrtdb){
  /**
   * TODO make option to restart "not" initial
   */
  uint process_id= command.columnAsInt("process_id");
  const char* process_args= command.column("process_args");

  g_logger.info("Change version for process: %d, args: %s",
                process_id, process_args);

  // Get the process
  if (process_id > config.m_processes.size()){
    g_logger.critical("Invalid process id %d", process_id);
    return false;
  }
  atrt_process& proc= *config.m_processes[process_id];

  // Save current proc state
  assert(proc.m_save.m_saved == false);
  proc.m_save.m_proc= proc.m_proc;
  proc.m_save.m_saved= true;

  g_logger.info("stopping process...");
  if (!stop_process(proc))
    return false;

  const char* new_prefix= g_prefix1 ? g_prefix1 : g_prefix;
  const char* old_prefix= g_prefix;
  proc.m_proc.m_env.assfmt("MYSQL_BASE_DIR=%s", new_prefix);
  const char *start= strstr(proc.m_proc.m_path.c_str(), old_prefix);
  if (!start){
    g_logger.critical("Could not find '%s' in '%s'",
                      old_prefix, proc.m_proc.m_path.c_str());
    return false;
  }
  BaseString suffix(proc.m_proc.m_path.substr(strlen(old_prefix)));
  proc.m_proc.m_path.assign(new_prefix).append(suffix);

  ndbout << proc << endl;

  g_logger.info("starting process...");
  if (!start_process(proc))
    return false;
  return true;
}


static
bool
do_reset_proc(atrt_config& config, SqlResultSet& command,
               AtrtClient& atrtdb){
  uint process_id= command.columnAsInt("process_id");
  g_logger.info("Reset process: %d", process_id);

  // Get the process
  if (process_id > config.m_processes.size()){
    g_logger.critical("Invalid process id %d", process_id);
    return false;
  }
  atrt_process& proc= *config.m_processes[process_id];

  g_logger.info("stopping process...");
  if (!stop_process(proc))
    return false;

  if (proc.m_save.m_saved)
  {
    ndbout << "before: " << proc << endl;

    proc.m_proc= proc.m_save.m_proc;
    proc.m_save.m_saved= false;
    proc.m_proc.m_id= -1;

    ndbout << "after: " << proc << endl;

  }
  else
  {
    ndbout << "process has not changed" << endl;
  }

  g_logger.info("starting process...");
  if (!start_process(proc))
    return false;
  return true;
}


bool
do_command(atrt_config& config){

  MYSQL* mysql= find_atrtdb_client(config);
  if (!mysql)
    return true;

  AtrtClient atrtdb(mysql);
  SqlResultSet command;
  if (!atrtdb.doQuery("SELECT * FROM command " \
                     "WHERE state = 'new' ORDER BY id LIMIT 1", command)){
    g_logger.critical("query failed");
    return false;
  }

  if (command.numRows() == 0)
    return true;

  uint id= command.columnAsInt("id");
  uint cmd= command.columnAsInt("cmd");
  g_logger.info("Got command, id: %d, cmd: %d", id, cmd);
  // command.print();

  // Set state of command to running
  if (!ack_command(atrtdb, id, "running"))
    return false;

  switch (cmd){
  case AtrtClient::ATCT_CHANGE_VERSION:
    if (!do_change_version(config, command, atrtdb))
      return false;
    break;

  case AtrtClient::ATCT_RESET_PROC:
    if (!do_reset_proc(config, command, atrtdb))
      return false;
    break;

  default:
    command.print();
    g_logger.error("got unknown command: %d", cmd);
    return false;
  }

  // Set state of command to done
  if (!ack_command(atrtdb, id, "done"))
    return false;

  g_logger.info("done!");

  return true;
}


template class Vector<atrt_process>;
