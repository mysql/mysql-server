/* Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include "applier_sql_thread.h"
#include <rpl_info_factory.h>
#include <rpl_slave.h>
#include <gcs_replication.h>

Applier_sql_thread::Applier_sql_thread(): mi(NULL), rli(NULL)
{}

int Applier_sql_thread::initialize_sql_thread()
{
  DBUG_ENTER("Applier_sql_thread::initialize_sql_thread");

  int error= 0;

  if (mi)
  {
    lock_slave_threads(mi);

    //Initialize only the SQL thread
    int thread_mask= SLAVE_SQL;

    error= start_slave_threads(false/*need_lock_slave=false*/,
                               true/*wait_for_start=true*/,
                               mi,
                               thread_mask);
    if (error)
    {
      log_message(MY_ERROR_LEVEL, "Error on the SQL thread initialization");
    }

    unlock_slave_threads(mi);
  }
  else
  {
    log_message(MY_ERROR_LEVEL,
                "No information available when starting the SQL thread "
                "due to an error on the relay log initialization");
  }

  DBUG_RETURN(error);
}

int Applier_sql_thread::terminate_sql_thread()
{
  DBUG_ENTER("Applier_sql_thread::terminate_sql_thread");

  int error= 0;
  if (mi)
  {
    /*
      The thread mask that we pass into the terminate method refers only to
      the  SQL thread. In fact the information in the Master_info object will
      only declare the *plugin's* SQL thread as running, but the information
      about the relay log is there also, so we also ported part of the IO
      thread shutdown logic to flush our relay log.
    */

    int thread_mask= 0;
    // Get a mask of _running_ threads
    init_thread_mask(&thread_mask,mi,0 /* not inverse*/);
    lock_slave_threads(mi);

    if ((error= terminate_slave_threads(mi, thread_mask, false))){
      log_message(MY_ERROR_LEVEL, "Error when stopping the applier SQL thread");
    }

    //don't return the possible error and go to the relay log flushing.
    mysql_mutex_t *log_lock= mi->rli->relay_log.get_log_lock();
    mysql_mutex_lock(log_lock);

    /*
      Flushes the relay log regardless of the sync_relay_log option.
      We do not flush the master info as it has no real info and could cause
      the server to identify itself as a master or destroy a real master info
      file/table.
    */
    if (mi->rli->relay_log.is_open() &&
        mi->rli->relay_log.flush_and_sync(true))
    {
      log_message(MY_ERROR_LEVEL, "Error when flushing the applier's relay log");
      error= 1;
    }

    mysql_mutex_unlock(log_lock);
    unlock_slave_threads(mi);
  }
  DBUG_RETURN(error);
}


int Applier_sql_thread::initialize()
{
  DBUG_ENTER("Applier_sql_thread::initialize");

  int error= 0;

  /*
    TODO: Modify the slave code allowing the MI/RLI initialization methods
    to receive these variables as input.

    TODO: These variables should belong to the handler and defined in the
    plugin.

    We change the relay log variable names here isolating this relay log from
    the server. Not doing this would imply that the server would take the
    present relay logs as being a remain from a stopped server invoking
    unnecessary methods that could fail due to the wrong context.
  */
  char *relay_log_name= (char* )"sql_applier";
  char *orig_relay_log_name= set_relay_log_name(relay_log_name);
  char *orig_relay_log_index_name= set_relay_log_index_name(relay_log_name);
  char *relay_log_info_name= (char*) "sql_applier_relay_log.info";
  char *original_relay_info_file= set_relay_log_info_name(relay_log_info_name);

  /*
    Master info repositories are not important here.
    They only function as holders for the SQL thread/Relay log variables.
  */
  if ((error= Rpl_info_factory::create_coordinators(INFO_REPOSITORY_DUMMY,
                                                    &this->mi,
                                                    INFO_REPOSITORY_FILE,
                                                    &this->rli)))
  {
    log_message(MY_ERROR_LEVEL, "Failed to setup the node metadata containers.");
    DBUG_RETURN(1);
  }

  mysql_mutex_lock(&mi->data_lock);
  mysql_mutex_lock(&mi->rli->data_lock);

  if ((error= mi->mi_init_info()))
  {
    log_message(MY_ERROR_LEVEL, "Failed to setup the node (mi) metadata container.");
    DBUG_RETURN(1);
  }

  mi->set_mi_description_event(
    new Format_description_log_event(BINLOG_VERSION));

  mi->host[0]= 'h';
  mi->host[1]= '\0';

  if ((error= rli->rli_init_info()))
  {
    log_message(MY_ERROR_LEVEL, "Failed to setup the node (rli) metadata container.");
    DBUG_RETURN(1);
  }

  //return the server variable to their original state
  set_relay_log_name(orig_relay_log_name);
  set_relay_log_index_name(orig_relay_log_index_name);
  set_relay_log_info_name(original_relay_info_file);

  //MTS is disable for now
  mi->rli->opt_slave_parallel_workers= 0;

  mi->rli= rli;

  mysql_mutex_unlock(&mi->rli->data_lock);
  mysql_mutex_unlock(&mi->data_lock);

  error= initialize_sql_thread();

  DBUG_RETURN(error);
}

int Applier_sql_thread::terminate()
{
  DBUG_ENTER("Applier_sql_thread::terminate");

  int error= 0;
  if ((error= terminate_sql_thread()))
  {
    log_message(MY_ERROR_LEVEL, "Failed to stop the node SQL thread.");
    DBUG_RETURN(error);
  }

  if (mi)
  {
    delete mi;
  }
  if (rli)
  {
    delete rli;
  }

  DBUG_RETURN(error);
}

int Applier_sql_thread::handle(PipelineEvent *event,Continuation* cont)
{
  DBUG_ENTER("Applier_sql_thread::handle");

  Packet* p;
  event->get_Packet(&p);

  int error= queue_event(this->mi, (const char*)p->payload, p->len);

  if (error)
    cont->signal(error);
  else
    next(event,cont);

  DBUG_RETURN(error);
}

bool Applier_sql_thread::is_unique(){
  return true;
}

Handler_role Applier_sql_thread::get_role()
{
  return QUEUER;
}
