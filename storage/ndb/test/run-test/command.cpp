/*
   Copyright (C) 2008 MySQL AB, 2008-2010 Sun Microsystems, Inc.
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


BaseString
set_env_var(const BaseString& existing,
            const BaseString& name,
            const BaseString& value)
{
  /* Split existing on space 
   * (may have issues with env vars with spaces) 
   * Split assignments on =
   * Where name == name, output new value
   */
  BaseString newEnv;
  Vector<BaseString> assignments;
  int assignmentCount = existing.split(assignments, BaseString(" "));
  
  for (int i=0; i < assignmentCount; i++)
  {
    Vector<BaseString> terms;
    int termCount = assignments[i].split(terms, BaseString("="));
    
    if (termCount)
    {
      if (strcmp(name.c_str(), terms[0].c_str()) == 0)
      {
        /* Found element */
        newEnv.append(name);
        newEnv.append('=');
        newEnv.append(value);
      }
      else
      {
        newEnv.append(assignments[i]);
      }
    }
    newEnv.append(' ');
  }

  return newEnv;
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

  const char* new_prefix= g_prefix1 ? g_prefix1 : g_prefix;
  const char* old_prefix= g_prefix;
  const char *start= strstr(proc.m_proc.m_path.c_str(), old_prefix);
  if (!start){
    /* Process path does not contain old prefix.  
     * Perhaps it contains the new prefix - e.g. is already
     * upgraded?
     */
    if (strstr(proc.m_proc.m_path.c_str(), new_prefix))
    {
      /* Process is already upgraded, *assume* that this
       * is ok
       * Alternatives could be - error, or downgrade.
       */
      g_logger.info("Process already upgraded");
      return true;
    }
      
    g_logger.critical("Could not find '%s' in '%s'",
                      old_prefix, proc.m_proc.m_path.c_str());
    return false;
  }

  // Save current proc state
  if (proc.m_save.m_saved == false)
  {
    proc.m_save.m_proc= proc.m_proc;
    proc.m_save.m_saved= true;
  }
  
  g_logger.info("stopping process...");
  if (!stop_process(proc))
    return false;
  BaseString newEnv = set_env_var(proc.m_proc.m_env, 
                                  BaseString("MYSQL_BASE_DIR"),
                                  BaseString(new_prefix));
  proc.m_proc.m_env.assign(newEnv);
  BaseString suffix(proc.m_proc.m_path.substr(strlen(old_prefix)));
  proc.m_proc.m_path.assign(new_prefix).append(suffix);
  if (process_args && strlen(process_args))
  {
    /* Beware too long args */
    proc.m_proc.m_args.append(" ");
    proc.m_proc.m_args.append(process_args);
  }

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

#ifdef _WIN32
  return true;
#endif

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
