/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB & Sasha

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

// Sasha Pachev <sasha@mysql.com> is currently in charge of this file

#include "mysql_priv.h"
#include "repl_failsafe.h"
#include "sql_repl.h"
#include "slave.h"
#include "mini_client.h"
#include <mysql.h>

RPL_STATUS rpl_status=RPL_NULL;
pthread_mutex_t LOCK_rpl_status;
pthread_cond_t COND_rpl_status;

const char *rpl_role_type[] = {"MASTER","SLAVE",NullS};
TYPELIB rpl_role_typelib = {array_elements(rpl_role_type)-1,"",
			    rpl_role_type};

const char* rpl_status_type[] = {"AUTH_MASTER","ACTIVE_SLAVE","IDLE_SLAVE",
				 "LOST_SOLDIER","TROOP_SOLDIER",
				 "RECOVERY_CAPTAIN","NULL",NullS};
TYPELIB rpl_status_typelib= {array_elements(rpl_status_type)-1,"",
			     rpl_status_type};

static int init_failsafe_rpl_thread(THD* thd)
{
  DBUG_ENTER("init_failsafe_rpl_thread");
  thd->system_thread = thd->bootstrap = 1;
  thd->client_capabilities = 0;
  my_net_init(&thd->net, 0);
  thd->net.timeout = slave_net_timeout;
  thd->max_packet_length=thd->net.max_packet;
  thd->master_access= ~0;
  thd->priv_user = 0;
  thd->system_thread = 1;
  pthread_mutex_lock(&LOCK_thread_count);
  thd->thread_id = thread_id++;
  pthread_mutex_unlock(&LOCK_thread_count);

  if (init_thr_lock() ||
      my_pthread_setspecific_ptr(THR_THD,  thd) ||
      my_pthread_setspecific_ptr(THR_MALLOC, &thd->mem_root) ||
      my_pthread_setspecific_ptr(THR_NET,  &thd->net))
  {
    close_connection(&thd->net,ER_OUT_OF_RESOURCES); // is this needed?
    end_thread(thd,0);
    DBUG_RETURN(-1);
  }

  thd->mysys_var=my_thread_var;
  thd->dbug_thread_id=my_thread_id();
#if !defined(__WIN__) && !defined(OS2)
  sigset_t set;
  VOID(sigemptyset(&set));			// Get mask in use
  VOID(pthread_sigmask(SIG_UNBLOCK,&set,&thd->block_signals));
#endif

  thd->mem_root.free=thd->mem_root.used=0;	
  if (thd->max_join_size == (ulong) ~0L)
    thd->options |= OPTION_BIG_SELECTS;

  thd->proc_info="Thread initialized";
  thd->version=refresh_version;
  thd->set_time();
  DBUG_RETURN(0);
}

void change_rpl_status(RPL_STATUS from_status, RPL_STATUS to_status)
{
  pthread_mutex_lock(&LOCK_rpl_status);
  if (rpl_status == from_status || rpl_status == RPL_ANY)
    rpl_status = to_status;
  pthread_cond_signal(&COND_rpl_status);
  pthread_mutex_unlock(&LOCK_rpl_status);
}

int update_slave_list(MYSQL* mysql)
{
  MYSQL_RES* res=0;
  MYSQL_ROW row;
  const char* error=0;
  bool have_auth_info;
  int port_ind;
  
  if (mc_mysql_query(mysql,"SHOW SLAVE HOSTS",0) ||
      !(res = mc_mysql_store_result(mysql)))
  {
    error = "Query error";
    goto err;
  }

  switch (mc_mysql_num_fields(res))
  {
  case 5:
    have_auth_info = 0;
    port_ind=2;
    break;
  case 7:
    have_auth_info = 1;
    port_ind=4;
    break;
  default:
    error = "Invalid number of fields in SHOW SLAVE HOSTS";
    goto err;
  }

  pthread_mutex_lock(&LOCK_slave_list);

  while ((row = mc_mysql_fetch_row(res)))
  {
    uint32 server_id;
    SLAVE_INFO* si, *old_si;
    server_id = atoi(row[0]);
    if ((old_si = (SLAVE_INFO*)hash_search(&slave_list,
					   (byte*)&server_id,4)))
      si = old_si;
    else
    {
      if (!(si = (SLAVE_INFO*)my_malloc(sizeof(SLAVE_INFO), MYF(MY_WME))))
      {
	error = "Out of memory";
	pthread_mutex_unlock(&LOCK_slave_list);
	goto err;
      }
      si->server_id = server_id;
    }
    strnmov(si->host, row[1], sizeof(si->host));
    si->port = atoi(row[port_ind]);
    si->rpl_recovery_rank = atoi(row[port_ind+1]);
    si->master_id = atoi(row[port_ind+2]);
    if (have_auth_info)
    {
      strnmov(si->user, row[2], sizeof(si->user));
      strnmov(si->password, row[3], sizeof(si->password));
    }
  }
  pthread_mutex_unlock(&LOCK_slave_list);
err:
  if (res)
    mc_mysql_free_result(res);
  if (error)
  {
    sql_print_error("Error updating slave list:",error);
    return 1;
  }
  return 0;
}

int find_recovery_captain(THD* thd, MYSQL* mysql)
{
  
  return 0;
}

pthread_handler_decl(handle_failsafe_rpl,arg)
{
  DBUG_ENTER("handle_failsafe_rpl");
  THD *thd = new THD;
  thd->thread_stack = (char*)&thd;
  MYSQL* recovery_captain = 0;
  pthread_detach_this_thread();
  if (init_failsafe_rpl_thread(thd) || !(recovery_captain=mc_mysql_init(0)))
  {
    sql_print_error("Could not initialize failsafe replication thread");
    goto err;
  }
  pthread_mutex_lock(&LOCK_rpl_status);
  while (!thd->killed && !abort_loop)
  {
    bool break_req_chain = 0;
    const char* msg = thd->enter_cond(&COND_rpl_status,
				      &LOCK_rpl_status, "Waiting for request");
    pthread_cond_wait(&COND_rpl_status, &LOCK_rpl_status);
    thd->proc_info="Processling request";
    while (!break_req_chain)
    {
      switch (rpl_status)
      {
      case RPL_LOST_SOLDIER:
	if (find_recovery_captain(thd, recovery_captain))
	  rpl_status=RPL_TROOP_SOLDIER;
	else
	  rpl_status=RPL_RECOVERY_CAPTAIN;
	break_req_chain=1; /* for now until other states are implemented */
	break;
      default:
	break_req_chain=1;
	break;
      }
    }
    thd->exit_cond(msg);
  }
  pthread_mutex_unlock(&LOCK_rpl_status);
err:
  if (recovery_captain)
    mc_mysql_close(recovery_captain);
  delete thd;
  my_thread_end();
  pthread_exit(0);
  DBUG_RETURN(0);
}




