/* Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

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

#include "my_global.h"
#include "rpl_channel_service_interface.h"
#include "rpl_slave.h"
#include "rpl_info_factory.h"
#include "rpl_mi.h"
#include "rpl_msr.h"         /* Multisource replication */
#include "rpl_rli.h"
#include "rpl_rli_pdb.h"

int initialize_channel_service_interface()
{
  DBUG_ENTER("initialize_channel_service_interface");

  //master info and relay log repositories must be TABLE
  if (opt_mi_repository_id != INFO_REPOSITORY_TABLE ||
      opt_rli_repository_id != INFO_REPOSITORY_TABLE)
  {
    sql_print_error("For the creation of replication channels the master info"
                    " and relay log info repositories must be set to TABLE");
    DBUG_RETURN(1);
  }

  //server id must be different from 0
  if (server_id == 0)
  {
    sql_print_error("For the creation of replication channels the server id"
                    " must be different from 0");
    DBUG_RETURN(1);
  }

  DBUG_RETURN(0);
}

#ifdef HAVE_REPLICATION

void set_mi_settings(Master_info *mi, Channel_creation_info* channel_info)
{
  mysql_mutex_lock(&mi->data_lock);

  mi->rli->set_thd_tx_priority(channel_info->thd_tx_priority);

  mi->rli->replicate_same_server_id=
    (channel_info->replicate_same_server_id == RPL_SERVICE_SERVER_DEFAULT) ?
     replicate_same_server_id : channel_info->replicate_same_server_id;

  mi->rli->opt_slave_parallel_workers=
    (channel_info->channel_mts_parallel_workers == RPL_SERVICE_SERVER_DEFAULT) ?
    opt_mts_slave_parallel_workers : channel_info->channel_mts_parallel_workers;

  if (channel_info->channel_mts_parallel_type == RPL_SERVICE_SERVER_DEFAULT)
  {
    if (mts_parallel_option == MTS_PARALLEL_TYPE_DB_NAME)
      mi->rli->channel_mts_submode = MTS_PARALLEL_TYPE_DB_NAME;
    else
      mi->rli->channel_mts_submode = MTS_PARALLEL_TYPE_LOGICAL_CLOCK;
  }
  else
  {
   if (channel_info->channel_mts_parallel_type == CHANNEL_MTS_PARALLEL_TYPE_DB_NAME)
      mi->rli->channel_mts_submode = MTS_PARALLEL_TYPE_DB_NAME;
    else
      mi->rli->channel_mts_submode = MTS_PARALLEL_TYPE_LOGICAL_CLOCK;
  }

  mi->rli->checkpoint_group=
    (channel_info->channel_mts_checkpoint_group == RPL_SERVICE_SERVER_DEFAULT) ?
    opt_mts_checkpoint_group : channel_info->channel_mts_checkpoint_group;

  mi->set_mi_description_event(new Format_description_log_event(BINLOG_VERSION));

  mysql_mutex_unlock(&mi->data_lock);
}

bool init_thread_context()
{
  if (!mysys_thread_var())
  {
    my_thread_init();
    return true;
  }
  return false;
}

void clean_thread_context()
{
  my_thread_end();
}

THD *create_surrogate_thread()
{
  THD *thd= NULL;
  thd= new THD;
  thd->thread_stack= (char*) &thd;
  thd->store_globals();
  thd->security_context()->skip_grants();

  return(thd);
}

void delete_surrogate_thread(THD *thd)
{
  thd->release_resources();
  delete thd;
  my_thread_set_THR_THD(NULL);
}

void
initialize_channel_creation_info(Channel_creation_info* channel_info)
{
  channel_info->type= SLAVE_REPLICATION_CHANNEL;
  channel_info->hostname= 0;
  channel_info->port= 0;
  channel_info->user= 0;
  channel_info->password= 0;
  channel_info->auto_position= RPL_SERVICE_SERVER_DEFAULT;
  channel_info->channel_mts_parallel_type= RPL_SERVICE_SERVER_DEFAULT;
  channel_info->channel_mts_parallel_workers= RPL_SERVICE_SERVER_DEFAULT;
  channel_info->channel_mts_checkpoint_group= RPL_SERVICE_SERVER_DEFAULT;
  channel_info->replicate_same_server_id= RPL_SERVICE_SERVER_DEFAULT;
  channel_info->thd_tx_priority= 0;
  channel_info->sql_delay= RPL_SERVICE_SERVER_DEFAULT;
  channel_info->preserve_relay_logs= false;
  channel_info->retry_count= 0;
  channel_info->connect_retry= 0;
}

void
initialize_channel_connection_info(Channel_connection_info* channel_info)
{
  channel_info->until_condition= CHANNEL_NO_UNTIL_CONDITION;
  channel_info->gtid= 0;
  channel_info->view_id= 0;
}

int channel_create(const char* channel,
                   Channel_creation_info* channel_info)
{
  DBUG_ENTER("channel_create");

  Master_info *mi= 0;
  int error= 0;
  LEX_MASTER_INFO* lex_mi= NULL;

  bool thd_created= false;
  THD *thd= current_thd;

  mysql_mutex_lock(&LOCK_msr_map);

  //Don't create default channels
  if (!strcmp(msr_map.get_default_channel(), channel))
  {
    error= RPL_CHANNEL_SERVICE_DEFAULT_CHANNEL_CREATION_ERROR;
    goto err;
  }

  if (sql_slave_skip_counter > 0)
  {
    error= RPL_CHANNEL_SERVICE_SLAVE_SKIP_COUNTER_ACTIVE;
    goto err;
  }

  /* Get the Master_info of the channel */
  mi= msr_map.get_mi(channel);

    /* create a new channel if doesn't exist */
  if (!mi)
  {
    if ((error= add_new_channel(&mi, channel,
                                channel_info->type)))
        goto err;
  }

  lex_mi= new st_lex_master_info();
  lex_mi->channel= channel;
  lex_mi->host= channel_info->hostname;
  lex_mi->port= channel_info->port;
  lex_mi->user= channel_info->user;
  lex_mi->password= channel_info->password;
  lex_mi->sql_delay= channel_info->sql_delay;
  lex_mi->connect_retry= channel_info->connect_retry;
  if (channel_info->retry_count)
  {
    lex_mi->retry_count_opt= LEX_MASTER_INFO::LEX_MI_ENABLE;
    lex_mi->retry_count= channel_info->retry_count;
  }

  if (channel_info->auto_position)
  {
    lex_mi->auto_position= LEX_MASTER_INFO::LEX_MI_ENABLE;
    if (mi && mi->is_auto_position())
    {
      //So change master allows new configurations with a running SQL thread
      lex_mi->auto_position= LEX_MASTER_INFO::LEX_MI_UNCHANGED;
    }
  }

  if (mi)
  {
    if (!thd)
    {
      thd_created= true;
      thd= create_surrogate_thread();
    }

    if ((error= change_master(thd, mi, lex_mi,
                              channel_info->preserve_relay_logs)))
    {
      goto err;
    }
  }

  set_mi_settings(mi, channel_info);

err:
  mysql_mutex_unlock(&LOCK_msr_map);

  if (thd_created)
  {
    delete_surrogate_thread(thd);
  }

  delete lex_mi;

  DBUG_RETURN(error);
}

int channel_start(const char* channel,
                  Channel_connection_info* connection_info,
                  int threads_to_start,
                  int wait_for_connection)
{
  DBUG_ENTER("channel_start(channel, threads_to_start, wait_for_connection");

  Master_info *mi= msr_map.get_mi(channel);
  int error= 0;

  if (mi == NULL)
  {
    DBUG_RETURN(RPL_CHANNEL_SERVICE_CHANNEL_DOES_NOT_EXISTS_ERROR);
  }

  if (sql_slave_skip_counter > 0)
  {
    error= RPL_CHANNEL_SERVICE_SLAVE_SKIP_COUNTER_ACTIVE;
    DBUG_RETURN(error);
  }

  int thread_mask= 0;
  if (threads_to_start & CHANNEL_APPLIER_THREAD)
  {
    thread_mask |= SLAVE_SQL;
  }
  if (threads_to_start & CHANNEL_RECEIVER_THREAD)
  {
    thread_mask |= SLAVE_IO;
  }

  //Nothing to be done here
  if (!thread_mask)
    DBUG_RETURN(0);

  LEX_SLAVE_CONNECTION lex_connection;
  lex_connection.reset();

  LEX_MASTER_INFO lex_mi;
  if (connection_info->until_condition != CHANNEL_NO_UNTIL_CONDITION)
  {
    switch (connection_info->until_condition)
    {
      case CHANNEL_UNTIL_APPLIER_AFTER_GTIDS:
        lex_mi.gtid_until_condition= LEX_MASTER_INFO::UNTIL_SQL_AFTER_GTIDS;
        lex_mi.gtid= connection_info->gtid;
        break;
      case CHANNEL_UNTIL_APPLIER_BEFORE_GTIDS:
        lex_mi.gtid_until_condition= LEX_MASTER_INFO::UNTIL_SQL_BEFORE_GTIDS;
        lex_mi.gtid= connection_info->gtid;
        break;
      case CHANNEL_UNTIL_APPLIER_AFTER_GAPS:
        lex_mi.until_after_gaps= true;
        break;
      case CHANNEL_UNTIL_VIEW_ID:
        DBUG_ASSERT((thread_mask & SLAVE_SQL) && connection_info->view_id);
        lex_mi.view_id= connection_info->view_id;
        break;
      default:
        DBUG_ASSERT(0);
    }
  }

  ulong thread_start_id= 0;
  if (wait_for_connection && (thread_mask & SLAVE_IO))
    thread_start_id= mi->slave_run_id;

  bool thd_created= false;

  THD* thd= current_thd;
  if (!thd)
  {
    thd_created= true;
    thd= create_surrogate_thread();
  }

  error= start_slave(thd, &lex_connection, &lex_mi,
                         thread_mask, mi, false, true);

  if (wait_for_connection && (thread_mask & SLAVE_IO) && !error)
  {
    mysql_mutex_lock(&mi->run_lock);
    /*
      If the ids are still equal this means the start thread method did not
      wait for the thread to start
    */
    while (thread_start_id == mi->slave_run_id)
    {
      mysql_cond_wait(&mi->start_cond, &mi->run_lock);
    }
    mysql_mutex_unlock(&mi->run_lock);

    while (mi->slave_running != MYSQL_SLAVE_RUN_CONNECT)
    {
      //If there is such a state change then there was an error on connection
      if (mi->slave_running == MYSQL_SLAVE_NOT_RUN)
      {
        error= RPL_CHANNEL_SERVICE_RECEIVER_CONNECTION_ERROR;
        break;
      }
      my_sleep(100);
    }
  }

  if (thd_created)
  {
    delete_surrogate_thread(thd);
  }

  DBUG_RETURN(error);
}

int channel_stop(const char* channel,
                 int threads_to_stop,
                 long timeout)
{
  DBUG_ENTER("channel_stop(channel, stop_receiver, stop_applier, timeout");

  Master_info *mi= msr_map.get_mi(channel);

  if (mi == NULL)
  {
    DBUG_RETURN(RPL_CHANNEL_SERVICE_CHANNEL_DOES_NOT_EXISTS_ERROR);
  }

  int thread_mask= 0;
  int server_thd_mask= 0;
  lock_slave_threads(mi);

  init_thread_mask(&server_thd_mask, mi, 0 /* not inverse*/);

  if ((threads_to_stop & CHANNEL_APPLIER_THREAD)
          && (server_thd_mask & SLAVE_SQL))
  {
    thread_mask |= SLAVE_SQL;
  }
  if ((threads_to_stop & CHANNEL_RECEIVER_THREAD)
          && (server_thd_mask & SLAVE_IO))
  {
    thread_mask |= SLAVE_IO;
  }

  if (thread_mask == 0)
  {
    DBUG_RETURN(0);
  }

  bool thd_init= init_thread_context();

  int error= terminate_slave_threads(mi, thread_mask, timeout, false);
  unlock_slave_threads(mi);

  if (thd_init)
  {
    clean_thread_context();
  }

  DBUG_RETURN(error);
}

int channel_purge_queue(const char* channel, bool reset_all)
{
  DBUG_ENTER("channel_purge_queue(channel, only_purge");

  Master_info *mi= msr_map.get_mi(channel);

  if (mi == NULL)
  {
    DBUG_RETURN(RPL_CHANNEL_SERVICE_CHANNEL_DOES_NOT_EXISTS_ERROR);
  }

  bool thd_init= init_thread_context();

  int error= reset_slave(current_thd, mi, reset_all);

  if (thd_init)
  {
    clean_thread_context();
  }

  DBUG_RETURN(error);
}

bool channel_is_active(const char* channel, enum_channel_thread_types thd_type)
{
  DBUG_ENTER("channel_is_active(channel, thd_type");

  Master_info *mi= msr_map.get_mi(channel);

  if (mi == NULL)
  {
    DBUG_RETURN(false);
  }

  int thread_mask= 0;
  init_thread_mask(&thread_mask, mi, 0 /* not inverse*/);

  switch(thd_type)
  {
    case CHANNEL_NO_THD:
      DBUG_RETURN(true); //return true as the channel exists
    case CHANNEL_RECEIVER_THREAD:
      DBUG_RETURN(thread_mask & SLAVE_IO);
    case CHANNEL_APPLIER_THREAD:
      DBUG_RETURN(thread_mask & SLAVE_SQL);
    default:
      DBUG_ASSERT(0);
  }
  DBUG_RETURN(false);
}

int channel_get_appliers_thread_id(const char* channel,
                                   unsigned long** appliers_id)
{
  DBUG_ENTER("channel_get_appliers_thread_id(channel, *appliers_id");

  int number_appliers= -1;

  Master_info *mi= msr_map.get_mi(channel);

  if (mi == NULL)
  {
    DBUG_RETURN(RPL_CHANNEL_SERVICE_CHANNEL_DOES_NOT_EXISTS_ERROR);
  }

  if (mi->rli != NULL)
  {
    mysql_mutex_lock(&mi->rli->run_lock);

    int num_workers= mi->rli->slave_parallel_workers;
    if (num_workers > 1)
    {
      *appliers_id=
          (unsigned long*) my_malloc(PSI_NOT_INSTRUMENTED,
                                     num_workers * sizeof(unsigned long),
                                     MYF(MY_WME));
      unsigned long *appliers_id_pointer= *appliers_id;

      for (int i= 0; i < num_workers; i++, appliers_id_pointer++)
      {
        mysql_mutex_lock(&mi->rli->workers.at(i)->info_thd_lock);
        *appliers_id_pointer= mi->rli->workers.at(i)->info_thd->thread_id();
        mysql_mutex_unlock(&mi->rli->workers.at(i)->info_thd_lock);
      }

      number_appliers= num_workers;
    }
    else
    {
      if (mi->rli->info_thd != NULL)
      {
        *appliers_id= (unsigned long*) my_malloc(PSI_NOT_INSTRUMENTED,
                                                 sizeof(unsigned long),
                                                 MYF(MY_WME));
        mysql_mutex_lock(&mi->rli->info_thd_lock);
        **appliers_id= mi->rli->info_thd->thread_id();
        mysql_mutex_unlock(&mi->rli->info_thd_lock);
        number_appliers= 1;
      }
    }
    mysql_mutex_unlock(&mi->rli->run_lock);
  }

  DBUG_RETURN(number_appliers);
}

long long channel_get_last_delivered_gno(const char* channel, int sidno)
{
  DBUG_ENTER("channel_get_last_delivered_gno(channel, sidno)");

  Master_info *mi= msr_map.get_mi(channel);

  if (mi == NULL)
  {
    DBUG_RETURN(RPL_CHANNEL_SERVICE_CHANNEL_DOES_NOT_EXISTS_ERROR);
  }

  rpl_gno last_gno= 0;

  global_sid_lock->rdlock();
  last_gno= mi->rli->get_gtid_set()->get_last_gno(sidno);
  global_sid_lock->unlock();

#if !defined(DBUG_OFF)
  const Gtid_set *retrieved_gtid_set= mi->rli->get_gtid_set();
  char *retrieved_gtid_set_string= NULL;
  global_sid_lock->wrlock();
  retrieved_gtid_set->to_string(&retrieved_gtid_set_string);
  global_sid_lock->unlock();
  DBUG_PRINT("info", ("get_last_delivered_gno retrieved_set_string: %s",
                      retrieved_gtid_set_string));
  my_free(retrieved_gtid_set_string);
#endif

  DBUG_RETURN(last_gno);
}

int channel_queue_packet(const char* channel,
                         const char* buf,
                         unsigned long event_len)
{
  DBUG_ENTER("channel_queue_packet(channel, event_buffer, event_len)");

  Master_info *mi= msr_map.get_mi(channel);

  if (mi == NULL)
  {
    DBUG_RETURN(RPL_CHANNEL_SERVICE_CHANNEL_DOES_NOT_EXISTS_ERROR);
  }

  DBUG_RETURN(queue_event(mi, buf, event_len));
}

int channel_wait_until_apply_queue_empty(char* channel, long long timeout)
{
  DBUG_ENTER("channel_wait_until_apply_queue_empty(channel, timeout)");

  Master_info *mi= msr_map.get_mi(channel);

  if (mi == NULL)
  {
    DBUG_RETURN(RPL_CHANNEL_SERVICE_CHANNEL_DOES_NOT_EXISTS_ERROR);
  }

  int error = mi->rli->wait_for_gtid_set(current_thd, mi->rli->get_gtid_set(),
                                         timeout);

  if(error == -1)
    DBUG_RETURN(REPLICATION_THREAD_WAIT_TIMEOUT_ERROR);
  if(error == -2)
    DBUG_RETURN(REPLICATION_THREAD_WAIT_NO_INFO_ERROR);

  DBUG_RETURN(error);
}

#endif /* HAVE_REPLICATION */
