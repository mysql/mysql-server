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

#include "mysql_priv.h"
#ifdef HAVE_REPLICATION

#include "repl_failsafe.h"
#include "sql_repl.h"
#include "slave.h"
#include "sql_acl.h"
#include "log_event.h"
#include <mysql.h>

#define SLAVE_LIST_CHUNK 128
#define SLAVE_ERRMSG_SIZE (FN_REFLEN+64)


RPL_STATUS rpl_status=RPL_NULL;
pthread_mutex_t LOCK_rpl_status;
pthread_cond_t COND_rpl_status;
HASH slave_list;

const char *rpl_role_type[] = {"MASTER","SLAVE",NullS};
TYPELIB rpl_role_typelib = {array_elements(rpl_role_type)-1,"",
			    rpl_role_type, NULL};

const char* rpl_status_type[]=
{
  "AUTH_MASTER","ACTIVE_SLAVE","IDLE_SLAVE", "LOST_SOLDIER","TROOP_SOLDIER",
  "RECOVERY_CAPTAIN","NULL",NullS
};
TYPELIB rpl_status_typelib= {array_elements(rpl_status_type)-1,"",
			     rpl_status_type, NULL};


static Slave_log_event* find_slave_event(IO_CACHE* log,
					 const char* log_file_name,
					 char* errmsg);

/*
  All of the functions defined in this file which are not used (the ones to
  handle failsafe) are not used; their code has not been updated for more than
  one year now so should be considered as BADLY BROKEN. Do not enable it.
  The used functions (to handle LOAD DATA FROM MASTER, plus some small
  functions like register_slave()) are working.
*/

static int init_failsafe_rpl_thread(THD* thd)
{
  DBUG_ENTER("init_failsafe_rpl_thread");
  /*
    thd->bootstrap is to report errors barely to stderr; if this code is
    enable again one day, one should check if bootstrap is still needed (maybe
    this thread has no other error reporting method).
  */
  thd->system_thread = thd->bootstrap = 1;
  thd->host_or_ip= "";
  thd->client_capabilities = 0;
  my_net_init(&thd->net, 0);
  thd->net.read_timeout = slave_net_timeout;
  thd->max_client_packet_length=thd->net.max_packet;
  thd->master_access= ~0;
  thd->priv_user = 0;
  pthread_mutex_lock(&LOCK_thread_count);
  thd->thread_id = thread_id++;
  pthread_mutex_unlock(&LOCK_thread_count);

  if (init_thr_lock() || thd->store_globals())
  {
    close_connection(thd, ER_OUT_OF_RESOURCES, 1); // is this needed?
    statistic_increment(aborted_connects,&LOCK_status);
    end_thread(thd,0);
    DBUG_RETURN(-1);
  }

#if !defined(__WIN__) && !defined(OS2) && !defined(__NETWARE__)
  sigset_t set;
  VOID(sigemptyset(&set));			// Get mask in use
  VOID(pthread_sigmask(SIG_UNBLOCK,&set,&thd->block_signals));
#endif

  thd->mem_root->free= thd->mem_root->used= 0;
  if (thd->variables.max_join_size == HA_POS_ERROR)
    thd->options|= OPTION_BIG_SELECTS;

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


#define get_object(p, obj) \
{\
  uint len = (uint)*p++;  \
  if (p + len > p_end || len >= sizeof(obj)) \
    goto err; \
  strmake(obj,(char*) p,len); \
  p+= len; \
}\


static inline int cmp_master_pos(Slave_log_event* sev, LEX_MASTER_INFO* mi)
{
  return cmp_master_pos(sev->master_log, sev->master_pos, mi->log_file_name,
			mi->pos);
}


void unregister_slave(THD* thd, bool only_mine, bool need_mutex)
{
  if (thd->server_id)
  {
    if (need_mutex)
      pthread_mutex_lock(&LOCK_slave_list);

    SLAVE_INFO* old_si;
    if ((old_si = (SLAVE_INFO*)hash_search(&slave_list,
					   (byte*)&thd->server_id, 4)) &&
	(!only_mine || old_si->thd == thd))
    hash_delete(&slave_list, (byte*)old_si);

    if (need_mutex)
      pthread_mutex_unlock(&LOCK_slave_list);
  }
}


/*
  Register slave in 'slave_list' hash table

  RETURN VALUES
  0	ok
  1	Error.   Error message sent to client
*/

int register_slave(THD* thd, uchar* packet, uint packet_length)
{
  int res;
  SLAVE_INFO *si;
  uchar *p= packet, *p_end= packet + packet_length;

  if (check_access(thd, REPL_SLAVE_ACL, any_db,0,0,0))
    return 1;
  if (!(si = (SLAVE_INFO*)my_malloc(sizeof(SLAVE_INFO), MYF(MY_WME))))
    goto err2;

  thd->server_id= si->server_id= uint4korr(p);
  p+= 4;
  get_object(p,si->host);
  get_object(p,si->user);
  get_object(p,si->password);
  if (p+10 > p_end)
    goto err;
  si->port= uint2korr(p);
  p += 2;
  si->rpl_recovery_rank= uint4korr(p);
  p += 4;
  if (!(si->master_id= uint4korr(p)))
    si->master_id= server_id;
  si->thd= thd;

  pthread_mutex_lock(&LOCK_slave_list);
  unregister_slave(thd,0,0);
  res= my_hash_insert(&slave_list, (byte*) si);
  pthread_mutex_unlock(&LOCK_slave_list);
  return res;

err:
  my_free((gptr) si, MYF(MY_WME));
  my_message(ER_UNKNOWN_ERROR, "Wrong parameters to function register_slave",
	     MYF(0));
err2:
  send_error(thd);
  return 1;
}

extern "C" uint32
*slave_list_key(SLAVE_INFO* si, uint* len,
		my_bool not_used __attribute__((unused)))
{
  *len = 4;
  return &si->server_id;
}

extern "C" void slave_info_free(void *s)
{
  my_free((gptr) s, MYF(MY_WME));
}

void init_slave_list()
{
  hash_init(&slave_list, system_charset_info, SLAVE_LIST_CHUNK, 0, 0,
	    (hash_get_key) slave_list_key, (hash_free_key) slave_info_free, 0);
  pthread_mutex_init(&LOCK_slave_list, MY_MUTEX_INIT_FAST);
}

void end_slave_list()
{
  /* No protection by a mutex needed as we are only called at shutdown */
  if (hash_inited(&slave_list))
  {
    hash_free(&slave_list);
    pthread_mutex_destroy(&LOCK_slave_list);
  }
}

static int find_target_pos(LEX_MASTER_INFO *mi, IO_CACHE *log, char *errmsg)
{
  my_off_t log_pos =	    (my_off_t) mi->pos;
  uint32 target_server_id = mi->server_id;

  for (;;)
  {
    Log_event* ev;
    if (!(ev = Log_event::read_log_event(log, (pthread_mutex_t*) 0, 0)))
    {
      if (log->error > 0)
	strmov(errmsg, "Binary log truncated in the middle of event");
      else if (log->error < 0)
	strmov(errmsg, "I/O error reading binary log");
      else
	strmov(errmsg, "Could not find target event in the binary log");
      return 1;
    }

    if (ev->log_pos >= log_pos && ev->server_id == target_server_id)
    {
      delete ev;
      mi->pos = my_b_tell(log);
      return 0;
    }
    delete ev;
  }
  /* Impossible */
}

/* 
  Before 4.0.15 we had a member of THD called log_pos, it was meant for
  failsafe replication code in repl_failsafe.cc which is disabled until
  it is reworked. Event's log_pos used to be preserved through 
  log-slave-updates to make code in repl_failsafe.cc work (this 
  function, SHOW NEW MASTER); but on the other side it caused unexpected
  values in Exec_Master_Log_Pos in A->B->C replication setup, 
  synchronization problems in master_pos_wait(), ... So we 
  (Dmitri & Guilhem) removed it.
  
  So for now this function is broken. 
*/

int translate_master(THD* thd, LEX_MASTER_INFO* mi, char* errmsg)
{
  LOG_INFO linfo;
  char last_log_name[FN_REFLEN];
  IO_CACHE log;
  File file = -1, last_file = -1;
  pthread_mutex_t *log_lock;
  const char* errmsg_p;
  Slave_log_event* sev = 0;
  my_off_t last_pos = 0;
  int error = 1;
  int cmp_res;
  LINT_INIT(cmp_res);
  DBUG_ENTER("translate_master");

  if (!mysql_bin_log.is_open())
  {
    strmov(errmsg,"Binary log is not open");
    DBUG_RETURN(1);
  }

  if (!server_id_supplied)
  {
    strmov(errmsg, "Misconfigured master - server id was not set");
    DBUG_RETURN(1);
  }

  if (mysql_bin_log.find_log_pos(&linfo, NullS, 1))
  {
    strmov(errmsg,"Could not find first log");
    DBUG_RETURN(1);
  }
  thd->current_linfo = &linfo;

  bzero((char*) &log,sizeof(log));
  log_lock = mysql_bin_log.get_log_lock();
  pthread_mutex_lock(log_lock);

  for (;;)
  {
    if ((file=open_binlog(&log, linfo.log_file_name, &errmsg_p)) < 0)
    {
      strmov(errmsg, errmsg_p);
      goto err;
    }

    if (!(sev = find_slave_event(&log, linfo.log_file_name, errmsg)))
      goto err;

    cmp_res = cmp_master_pos(sev, mi);
    delete sev;

    if (!cmp_res)
    {
      /* Copy basename */
      fn_format(mi->log_file_name, linfo.log_file_name, "","",1);
      mi->pos = my_b_tell(&log);
      goto mi_inited;
    }
    else if (cmp_res > 0)
    {
      if (!last_pos)
      {
	strmov(errmsg,
	       "Slave event in first log points past the target position");
	goto err;
      }
      end_io_cache(&log);
      (void) my_close(file, MYF(MY_WME));
      if (init_io_cache(&log, (file = last_file), IO_SIZE, READ_CACHE, 0, 0,
			MYF(MY_WME)))
      {
	errmsg[0] = 0;
	goto err;
      }
      break;
    }

    strmov(last_log_name, linfo.log_file_name);
    last_pos = my_b_tell(&log);

    switch (mysql_bin_log.find_next_log(&linfo, 1)) {
    case LOG_INFO_EOF:
      if (last_file >= 0)
       (void)my_close(last_file, MYF(MY_WME));
      last_file = -1;
      goto found_log;
    case 0:
      break;
    default:
      strmov(errmsg, "Error reading log index");
      goto err;
    }

    end_io_cache(&log);
    if (last_file >= 0)
     (void) my_close(last_file, MYF(MY_WME));
    last_file = file;
  }

found_log:
  my_b_seek(&log, last_pos);
  if (find_target_pos(mi,&log,errmsg))
    goto err;
  fn_format(mi->log_file_name, last_log_name, "","",1);  /* Copy basename */

mi_inited:
  error = 0;
err:
  pthread_mutex_unlock(log_lock);
  end_io_cache(&log);
  pthread_mutex_lock(&LOCK_thread_count);
  thd->current_linfo = 0;
  pthread_mutex_unlock(&LOCK_thread_count);
  if (file >= 0)
    (void) my_close(file, MYF(MY_WME));
  if (last_file >= 0 && last_file != file)
    (void) my_close(last_file, MYF(MY_WME));

  DBUG_RETURN(error);
}


/*
  Caller must delete result when done
*/

static Slave_log_event* find_slave_event(IO_CACHE* log,
					 const char* log_file_name,
					 char* errmsg)
{
  Log_event* ev;
  int i;
  bool slave_event_found = 0;
  LINT_INIT(ev);

  for (i = 0; i < 2; i++)
  {
    if (!(ev = Log_event::read_log_event(log, (pthread_mutex_t*)0, 0)))
    {
      my_snprintf(errmsg, SLAVE_ERRMSG_SIZE,
		  "Error reading event in log '%s'",
		  (char*)log_file_name);
      return 0;
    }
    if (ev->get_type_code() == SLAVE_EVENT)
    {
      slave_event_found = 1;
      break;
    }
    delete ev;
  }
  if (!slave_event_found)
  {
    my_snprintf(errmsg, SLAVE_ERRMSG_SIZE,
		"Could not find slave event in log '%s'",
		(char*)log_file_name);
    return 0;
  }

  return (Slave_log_event*)ev;
}

/*
   This function is broken now. See comment for translate_master().
 */

int show_new_master(THD* thd)
{
  Protocol *protocol= thd->protocol;
  DBUG_ENTER("show_new_master");
  List<Item> field_list;
  char errmsg[SLAVE_ERRMSG_SIZE];
  LEX_MASTER_INFO* lex_mi= &thd->lex->mi;

  errmsg[0]=0;					// Safety
  if (translate_master(thd, lex_mi, errmsg))
  {
    if (errmsg[0])
      my_error(ER_ERROR_WHEN_EXECUTING_COMMAND, MYF(0),
	       "SHOW NEW MASTER", errmsg);
    DBUG_RETURN(-1);
  }
  else
  {
    field_list.push_back(new Item_empty_string("Log_name", 20));
    field_list.push_back(new Item_return_int("Log_pos", 10,
					     MYSQL_TYPE_LONGLONG));
    if (protocol->send_fields(&field_list, 1))
      DBUG_RETURN(-1);
    protocol->prepare_for_resend();
    protocol->store(lex_mi->log_file_name, &my_charset_bin);
    protocol->store((ulonglong) lex_mi->pos);
    if (protocol->write())
      DBUG_RETURN(-1);
    send_eof(thd);
    DBUG_RETURN(0);
  }
}

/*
  Asks the master for the list of its other connected slaves.
  This is for failsafe replication: 
  in order for failsafe replication to work, the servers involved in
  replication must know of each other. We accomplish this by having each
  slave report to the master how to reach it, and on connection, each
  slave receives information about where the other slaves are.

  SYNOPSIS
    update_slave_list()
    mysql           pre-existing connection to the master
    mi              master info

  NOTES
    mi is used only to give detailed error messages which include the
    hostname/port of the master, the username used by the slave to connect to
    the master.
    If the user used by the slave to connect to the master does not have the
    REPLICATION SLAVE privilege, it will pop in this function because
    SHOW SLAVE HOSTS will fail on the master.

  RETURN VALUES
    1           error
    0           success
 */

int update_slave_list(MYSQL* mysql, MASTER_INFO* mi)
{
  MYSQL_RES* res=0;
  MYSQL_ROW row;
  const char* error=0;
  bool have_auth_info;
  int port_ind;
  DBUG_ENTER("update_slave_list");

  if (mysql_real_query(mysql,"SHOW SLAVE HOSTS",16) ||
      !(res = mysql_store_result(mysql)))
  {
    error= mysql_error(mysql);
    goto err;
  }

  switch (mysql_num_fields(res)) {
  case 5:
    have_auth_info = 0;
    port_ind=2;
    break;
  case 7:
    have_auth_info = 1;
    port_ind=4;
    break;
  default:
    error= "the master returned an invalid number of fields for SHOW SLAVE \
HOSTS";
    goto err;
  }

  pthread_mutex_lock(&LOCK_slave_list);

  while ((row= mysql_fetch_row(res)))
  {
    uint32 server_id;
    SLAVE_INFO* si, *old_si;
    server_id = atoi(row[0]);
    if ((old_si= (SLAVE_INFO*)hash_search(&slave_list,
					  (byte*)&server_id,4)))
      si = old_si;
    else
    {
      if (!(si = (SLAVE_INFO*)my_malloc(sizeof(SLAVE_INFO), MYF(MY_WME))))
      {
	error= "the slave is out of memory";
	pthread_mutex_unlock(&LOCK_slave_list);
	goto err;
      }
      si->server_id = server_id;
      my_hash_insert(&slave_list, (byte*)si);
    }
    strmake(si->host, row[1], sizeof(si->host)-1);
    si->port = atoi(row[port_ind]);
    si->rpl_recovery_rank = atoi(row[port_ind+1]);
    si->master_id = atoi(row[port_ind+2]);
    if (have_auth_info)
    {
      strmake(si->user, row[2], sizeof(si->user)-1);
      strmake(si->password, row[3], sizeof(si->password)-1);
    }
  }
  pthread_mutex_unlock(&LOCK_slave_list);

err:
  if (res)
    mysql_free_result(res);
  if (error)
  {
    sql_print_error("While trying to obtain the list of slaves from the master \
'%s:%d', user '%s' got the following error: '%s'", 
                    mi->host, mi->port, mi->user, error);
    DBUG_RETURN(1);
  }
  DBUG_RETURN(0);
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
  const char* msg;

  pthread_detach_this_thread();
  if (init_failsafe_rpl_thread(thd) || !(recovery_captain=mysql_init(0)))
  {
    sql_print_error("Could not initialize failsafe replication thread");
    goto err;
  }
  pthread_mutex_lock(&LOCK_rpl_status);
  msg= thd->enter_cond(&COND_rpl_status,
                       &LOCK_rpl_status, "Waiting for request");
  while (!thd->killed && !abort_loop)
  {
    bool break_req_chain = 0;
    pthread_cond_wait(&COND_rpl_status, &LOCK_rpl_status);
    thd->proc_info="Processing request";
    while (!break_req_chain)
    {
      switch (rpl_status) {
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
  }
  thd->exit_cond(msg);
err:
  if (recovery_captain)
    mysql_close(recovery_captain);
  delete thd;
  my_thread_end();
  pthread_exit(0);
  DBUG_RETURN(0);
}


int show_slave_hosts(THD* thd)
{
  List<Item> field_list;
  Protocol *protocol= thd->protocol;
  DBUG_ENTER("show_slave_hosts");

  field_list.push_back(new Item_return_int("Server_id", 10,
					   MYSQL_TYPE_LONG));
  field_list.push_back(new Item_empty_string("Host", 20));
  if (opt_show_slave_auth_info)
  {
    field_list.push_back(new Item_empty_string("User",20));
    field_list.push_back(new Item_empty_string("Password",20));
  }
  field_list.push_back(new Item_return_int("Port", 7, MYSQL_TYPE_LONG));
  field_list.push_back(new Item_return_int("Rpl_recovery_rank", 7,
					   MYSQL_TYPE_LONG));
  field_list.push_back(new Item_return_int("Master_id", 10,
					   MYSQL_TYPE_LONG));

  if (protocol->send_fields(&field_list, 1))
    DBUG_RETURN(-1);

  pthread_mutex_lock(&LOCK_slave_list);

  for (uint i = 0; i < slave_list.records; ++i)
  {
    SLAVE_INFO* si = (SLAVE_INFO*) hash_element(&slave_list, i);
    protocol->prepare_for_resend();
    protocol->store((uint32) si->server_id);
    protocol->store(si->host, &my_charset_bin);
    if (opt_show_slave_auth_info)
    {
      protocol->store(si->user, &my_charset_bin);
      protocol->store(si->password, &my_charset_bin);
    }
    protocol->store((uint32) si->port);
    protocol->store((uint32) si->rpl_recovery_rank);
    protocol->store((uint32) si->master_id);
    if (protocol->write())
    {
      pthread_mutex_unlock(&LOCK_slave_list);
      DBUG_RETURN(-1);
    }
  }
  pthread_mutex_unlock(&LOCK_slave_list);
  send_eof(thd);
  DBUG_RETURN(0);
}


int connect_to_master(THD *thd, MYSQL* mysql, MASTER_INFO* mi)
{
  DBUG_ENTER("connect_to_master");

  if (!mi->host || !*mi->host)			/* empty host */
  {
    strmov(mysql->net.last_error, "Master is not configured");
    DBUG_RETURN(1);
  }
  mysql_options(mysql, MYSQL_OPT_CONNECT_TIMEOUT, (char *) &slave_net_timeout);
  mysql_options(mysql, MYSQL_OPT_READ_TIMEOUT, (char *) &slave_net_timeout);

#ifdef HAVE_OPENSSL
  if (mi->ssl)
    mysql_ssl_set(mysql, 
        mi->ssl_key[0]?mi->ssl_key:0,
        mi->ssl_cert[0]?mi->ssl_cert:0,
        mi->ssl_ca[0]?mi->ssl_ca:0, 
        mi->ssl_capath[0]?mi->ssl_capath:0,
        mi->ssl_cipher[0]?mi->ssl_cipher:0);
#endif
    
  mysql_options(mysql, MYSQL_SET_CHARSET_NAME, default_charset_info->csname);
  mysql_options(mysql, MYSQL_SET_CHARSET_DIR, (char *) charsets_dir);
  if (!mysql_real_connect(mysql, mi->host, mi->user, mi->password, 0,
			mi->port, 0, 0))
    DBUG_RETURN(1);
  DBUG_RETURN(0);
}


static inline void cleanup_mysql_results(MYSQL_RES* db_res,
					 MYSQL_RES** cur, MYSQL_RES** start)
{
  for (; cur >= start; --cur)
  {
    if (*cur)
      mysql_free_result(*cur);
  }
  mysql_free_result(db_res);
}


static int fetch_db_tables(THD *thd, MYSQL *mysql, const char *db,
			   MYSQL_RES *table_res, MASTER_INFO *mi)
{
  MYSQL_ROW row;
  for (row = mysql_fetch_row(table_res); row;
       row = mysql_fetch_row(table_res))
  {
    TABLE_LIST table;
    const char* table_name= row[0];
    int error;
    if (table_rules_on)
    {
      bzero((char*) &table, sizeof(table)); //just for safe
      table.db= (char*) db;
      table.real_name= (char*) table_name;
      table.updating= 1;

      if (!tables_ok(thd, &table))
	continue;
    }
    /* download master's table and overwrite slave's table */
    if ((error= fetch_master_table(thd, db, table_name, mi, mysql, 1)))
      return error;
  }
  return 0;
}

/*
  Load all MyISAM tables from master to this slave.

  REQUIREMENTS
   - No active transaction (flush_relay_log_info would not work in this case)
*/

int load_master_data(THD* thd)
{
  MYSQL mysql;
  MYSQL_RES* master_status_res = 0;
  int error = 0;
  const char* errmsg=0;
  int restart_thread_mask;
  HA_CREATE_INFO create_info;

  mysql_init(&mysql);

  /*
    We do not want anyone messing with the slave at all for the entire
    duration of the data load.
  */
  pthread_mutex_lock(&LOCK_active_mi);
  lock_slave_threads(active_mi);
  init_thread_mask(&restart_thread_mask,active_mi,0 /*not inverse*/);
  if (restart_thread_mask &&
      (error=terminate_slave_threads(active_mi,restart_thread_mask,
				     1 /*skip lock*/)))
  {
    send_error(thd,error);
    unlock_slave_threads(active_mi);
    pthread_mutex_unlock(&LOCK_active_mi);
    return 1;
  }
  
  if (connect_to_master(thd, &mysql, active_mi))
  {
    net_printf(thd, error= ER_CONNECT_TO_MASTER,
	       mysql_error(&mysql));
    goto err;
  }

  // now that we are connected, get all database and tables in each
  {
    MYSQL_RES *db_res, **table_res, **table_res_end, **cur_table_res;
    uint num_dbs;

    if (mysql_real_query(&mysql, "SHOW DATABASES", 14) ||
	!(db_res = mysql_store_result(&mysql)))
    {
      net_printf(thd, error = ER_QUERY_ON_MASTER,
		 mysql_error(&mysql));
      goto err;
    }

    if (!(num_dbs = (uint) mysql_num_rows(db_res)))
      goto err;
    /*
      In theory, the master could have no databases at all
      and run with skip-grant
    */

    if (!(table_res = (MYSQL_RES**)thd->alloc(num_dbs * sizeof(MYSQL_RES*))))
    {
      net_printf(thd, error = ER_OUTOFMEMORY);
      goto err;
    }

    /*
      This is a temporary solution until we have online backup
      capabilities - to be replaced once online backup is working
      we wait to issue FLUSH TABLES WITH READ LOCK for as long as we
      can to minimize the lock time.
    */
    if (mysql_real_query(&mysql, "FLUSH TABLES WITH READ LOCK", 27) ||
	mysql_real_query(&mysql, "SHOW MASTER STATUS",18) ||
	!(master_status_res = mysql_store_result(&mysql)))
    {
      net_printf(thd, error = ER_QUERY_ON_MASTER,
		 mysql_error(&mysql));
      goto err;
    }

    /*
      Go through every table in every database, and if the replication
      rules allow replicating it, get it
    */

    table_res_end = table_res + num_dbs;

    for (cur_table_res = table_res; cur_table_res < table_res_end;
	 cur_table_res++)
    {
      // since we know how many rows we have, this can never be NULL
      MYSQL_ROW row = mysql_fetch_row(db_res);
      char* db = row[0];

      /*
	Do not replicate databases excluded by rules. We also test
	replicate_wild_*_table rules (replicate_wild_ignore_table='db1.%' will
	be considered as "ignore the 'db1' database as a whole, as it already
	works for CREATE DATABASE and DROP DATABASE).
	Also skip 'mysql' database - in most cases the user will
	mess up and not exclude mysql database with the rules when
	he actually means to - in this case, he is up for a surprise if
	his priv tables get dropped and downloaded from master
	TODO - add special option, not enabled
	by default, to allow inclusion of mysql database into load
	data from master
      */

      if (!db_ok(db, replicate_do_db, replicate_ignore_db) ||
          !db_ok_with_wild_table(db) ||
	  !strcmp(db,"mysql"))
      {
	*cur_table_res = 0;
	continue;
      }

      bzero((char*) &create_info, sizeof(create_info));
      create_info.options= HA_LEX_CREATE_IF_NOT_EXISTS;

      if (mysql_create_db(thd, db, &create_info, 1))
      {
	send_error(thd, 0, 0);
	cleanup_mysql_results(db_res, cur_table_res - 1, table_res);
	goto err;
      }

      if (mysql_select_db(&mysql, db) ||
	  mysql_real_query(&mysql, "SHOW TABLES", 11) ||
	  !(*cur_table_res = mysql_store_result(&mysql)))
      {
	net_printf(thd, error = ER_QUERY_ON_MASTER,
		   mysql_error(&mysql));
	cleanup_mysql_results(db_res, cur_table_res - 1, table_res);
	goto err;
      }

      if ((error = fetch_db_tables(thd,&mysql,db,*cur_table_res,active_mi)))
      {
	// we do not report the error - fetch_db_tables handles it
	cleanup_mysql_results(db_res, cur_table_res, table_res);
	goto err;
      }
    }

    cleanup_mysql_results(db_res, cur_table_res - 1, table_res);

    // adjust replication coordinates from the master
    if (master_status_res)
    {
      MYSQL_ROW row = mysql_fetch_row(master_status_res);

      /*
	We need this check because the master may not be running with
	log-bin, but it will still allow us to do all the steps
	of LOAD DATA FROM MASTER - no reason to forbid it, really,
	although it does not make much sense for the user to do it
      */
      if (row && row[0] && row[1])
      {
        /*
          If the slave's master info is not inited, we init it, then we write
          the new coordinates to it. Must call init_master_info() *before*
          setting active_mi, because init_master_info() sets active_mi with
          defaults.
        */
        int error;

        if (init_master_info(active_mi, master_info_file, relay_log_info_file,
			     0))
          send_error(thd, ER_MASTER_INFO);
	strmake(active_mi->master_log_name, row[0],
		sizeof(active_mi->master_log_name));
	active_mi->master_log_pos= my_strtoll10(row[1], (char**) 0, &error);
        /* at least in recent versions, the condition below should be false */
	if (active_mi->master_log_pos < BIN_LOG_HEADER_SIZE)
	  active_mi->master_log_pos = BIN_LOG_HEADER_SIZE;
        /*
          Relay log's IO_CACHE may not be inited (even if we are sure that some
          host was specified; there could have been a problem when replication
          started, which led to relay log's IO_CACHE to not be inited.
        */
	flush_master_info(active_mi, 0);
      }
      mysql_free_result(master_status_res);
    }

    if (mysql_real_query(&mysql, "UNLOCK TABLES", 13))
    {
      net_printf(thd, error = ER_QUERY_ON_MASTER,
		 mysql_error(&mysql));
      goto err;
    }
  }
  thd->proc_info="purging old relay logs";
  if (purge_relay_logs(&active_mi->rli,thd,
		       0 /* not only reset, but also reinit */,
		       &errmsg))
  {
    send_error(thd, 0, "Failed purging old relay logs");
    unlock_slave_threads(active_mi);
    pthread_mutex_unlock(&LOCK_active_mi);
    return 1;
  }
  pthread_mutex_lock(&active_mi->rli.data_lock);
  active_mi->rli.group_master_log_pos = active_mi->master_log_pos;
  strmake(active_mi->rli.group_master_log_name,active_mi->master_log_name,
	  sizeof(active_mi->rli.group_master_log_name)-1);
  /*
     Cancel the previous START SLAVE UNTIL, as the fact to download
     a new copy logically makes UNTIL irrelevant.
  */
  clear_until_condition(&active_mi->rli);

  /*
    No need to update rli.event* coordinates, they will be when the slave
    threads start ; only rli.group* coordinates are necessary here.
  */
  flush_relay_log_info(&active_mi->rli);
  pthread_cond_broadcast(&active_mi->rli.data_cond);
  pthread_mutex_unlock(&active_mi->rli.data_lock);
  thd->proc_info = "starting slave";
  if (restart_thread_mask)
  {
    error=start_slave_threads(0 /* mutex not needed */,
			      1 /* wait for start */,
			      active_mi,master_info_file,relay_log_info_file,
			      restart_thread_mask);
  }

err:
  unlock_slave_threads(active_mi);
  pthread_mutex_unlock(&LOCK_active_mi);
  thd->proc_info = 0;

  mysql_close(&mysql); // safe to call since we always do mysql_init()
  if (!error)
    send_ok(thd);

  return error;
}

#endif /* HAVE_REPLICATION */

