/* Copyright (C) 2005 MySQL AB

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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA */

#include "mysql_priv.h"
#include "sp_rcontext.h"

#define RUSAGE_USEC(tv)  ((tv).tv_sec*1000000 + (tv).tv_usec)
#define RUSAGE_DIFF_USEC(tv1, tv2) (RUSAGE_USEC((tv1))-RUSAGE_USEC((tv2)))

PROFILE_ENTRY::PROFILE_ENTRY()
  :status(NULL), time(0), function(NULL), file(NULL), line(0)
{
  collect();
}

PROFILE_ENTRY::PROFILE_ENTRY(PROFILE *profile_arg, const char *status_arg)
  :profile(profile_arg), function(NULL), file(NULL), line(0)
{
  collect();
  if (status_arg)
    set_status(status_arg);
}

PROFILE_ENTRY::PROFILE_ENTRY(PROFILE *profile_arg, const char *status_arg,
                             const char *function_arg,
                             const char *file_arg, unsigned int line_arg)
  :profile(profile_arg)
{
  collect();
  if (status_arg)
    set_status(status_arg);
  if (function_arg)
    function= strdup_root(&profile->profiling->root, function_arg);
  if (file_arg)
    file= strdup_root(&profile->profiling->root, file_arg);
  line= line_arg;
}

PROFILE_ENTRY::~PROFILE_ENTRY()
{
  if (status)
    free(status);
  if (function)
    free(function);
  if (file)
    free(file);
}
  
void PROFILE_ENTRY::set_status(const char *status_arg)
{
  status= strdup_root(&profile->profiling->root, status_arg);
}

void PROFILE_ENTRY::collect()
{
  time= my_getsystime();
  getrusage(RUSAGE_SELF, &rusage);
}

PROFILE::PROFILE(PROFILING *profiling_arg)
  :profiling(profiling_arg)
{
  profile_end= &profile_start;
}

PROFILE::~PROFILE()
{
  entries.empty();
}

void PROFILE::status(const char *status_arg,
                     const char *function_arg=NULL,
                     const char *file_arg=NULL, unsigned int line_arg=0)
{
  PROFILE_ENTRY *prof= NULL;
  MEM_ROOT *old_root= NULL;
  THD *thd= profiling->thd;

  DBUG_ENTER("PROFILE::status");

  /* Blank status.  Just return, and thd->proc_info will be set blank later. */
  if (unlikely(!status_arg))
    DBUG_VOID_RETURN;

  /* If thd->proc_info is currently set to status_arg, don't profile twice. */
  if (unlikely(thd->proc_info && !(strcmp(thd->proc_info, status_arg))))
    DBUG_VOID_RETURN;

  /* Is this the same query as our profile currently contains? */
  if (unlikely(thd->query_id != query_id && !thd->spcont))
    reset();
    
  /*
    In order to keep the profile information between queries (i.e. from
    SELECT to the following SHOW PROFILE command) the following code is
    necessary to keep the profile from getting freed automatically when
    mysqld cleans up after the query.  This code is shamelessly stolen
    from SHOW WARNINGS.
    
    The thd->mem_root structure is freed after each query is completed,
    so temporarily override it.
  */
  old_root= thd->mem_root;
  thd->mem_root= &profiling->root;
  if (function_arg && file_arg) {
    if ((profile_end= prof= new PROFILE_ENTRY(this, status_arg, function_arg, file_arg, line_arg)))
      entries.push_back(prof);
  } else {
    if ((profile_end= prof= new PROFILE_ENTRY(this, status_arg)))
      entries.push_back(prof);
  }
  thd->mem_root= old_root;

  DBUG_VOID_RETURN;
}

void PROFILE::reset()
{
  DBUG_ENTER("PROFILE::reset");
  if (profiling->thd->query_id != query_id)
  {
    query_id= profiling->thd->query_id;
    profile_start.collect();
    entries.empty();
  }
  DBUG_VOID_RETURN;
}

bool PROFILE::show(uint options)
{  
  PROFILE_ENTRY *prof;
  THD *thd= profiling->thd;
  PROFILE_ENTRY *ps= &profile_start;
  List<Item> field_list;
  DBUG_ENTER("PROFILE::show");

  field_list.push_back(new Item_empty_string("Status", MYSQL_ERRMSG_SIZE));
  field_list.push_back(new Item_return_int("Time_elapsed", 20,
                                           MYSQL_TYPE_LONGLONG));

  if (options & PROFILE_CPU)
  {
    field_list.push_back(new Item_return_int("CPU_user", 20,
                                             MYSQL_TYPE_LONGLONG));
    field_list.push_back(new Item_return_int("CPU_system", 20, 
                                             MYSQL_TYPE_LONGLONG));
  }
  
  if (options & PROFILE_MEMORY)
  {
  }
  
  if (options & PROFILE_CONTEXT)
  {
    field_list.push_back(new Item_return_int("Context_voluntary", 10,
                                             MYSQL_TYPE_LONG));
    field_list.push_back(new Item_return_int("Context_involuntary", 10,
                                             MYSQL_TYPE_LONG));
  }

  if (options & PROFILE_BLOCK_IO)
  {
    field_list.push_back(new Item_return_int("Block_ops_in", 10,
                                             MYSQL_TYPE_LONG));
    field_list.push_back(new Item_return_int("Block_ops_out", 10,
                                             MYSQL_TYPE_LONG));
  }
  
  if (options & PROFILE_IPC)
  {
    field_list.push_back(new Item_return_int("Messages_sent", 10,
                                             MYSQL_TYPE_LONG));
    field_list.push_back(new Item_return_int("Messages_received", 10,
                                             MYSQL_TYPE_LONG));
  }
  
  if (options & PROFILE_PAGE_FAULTS)
  {
    field_list.push_back(new Item_return_int("Page_faults_major", 10,
                                             MYSQL_TYPE_LONG));
    field_list.push_back(new Item_return_int("Page_faults_minor", 10,
                                             MYSQL_TYPE_LONG));
  }
  
  if (options & PROFILE_SWAPS)
  {
    field_list.push_back(new Item_return_int("Swaps", 10, MYSQL_TYPE_LONG));
  }
  
  if(options & PROFILE_SOURCE)
  {
    field_list.push_back(new Item_empty_string("Source_function",
                                               MYSQL_ERRMSG_SIZE));  
    field_list.push_back(new Item_empty_string("Source_file",
                                               MYSQL_ERRMSG_SIZE));
    field_list.push_back(new Item_return_int("Source_line", 10,
                                             MYSQL_TYPE_LONG));
  }
  
  if (thd->protocol->send_fields(&field_list,
                                 Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    DBUG_RETURN(TRUE);

  SELECT_LEX *sel= &thd->lex->select_lex;
  SELECT_LEX_UNIT *unit= &thd->lex->unit;
  ha_rows idx= 0;
  Protocol *protocol=thd->protocol;

  unit->set_limit(sel);

  List_iterator<PROFILE_ENTRY> it(entries);
  ulonglong last_time= ps->time;
  while ((prof= it++))
  {
    if (++idx <= unit->offset_limit_cnt)
      continue;
    if (idx > unit->select_limit_cnt)
      break;

    protocol->prepare_for_resend();
    protocol->store(prof->status, strlen(prof->status), system_charset_info);
    protocol->store((ulonglong)(prof->time - ps->time)/10);

    if (options & PROFILE_CPU)
    {
      protocol->store((ulonglong)RUSAGE_DIFF_USEC(prof->rusage.ru_utime,
                                                  ps->rusage.ru_utime));
      protocol->store((ulonglong)RUSAGE_DIFF_USEC(prof->rusage.ru_stime,
                                                  ps->rusage.ru_stime));
    }
    
    if (options & PROFILE_CONTEXT)
    {
      protocol->store((uint32)(prof->rusage.ru_nvcsw - ps->rusage.ru_nvcsw));
      protocol->store((uint32)(prof->rusage.ru_nivcsw - ps->rusage.ru_nivcsw));
    }

    if (options & PROFILE_BLOCK_IO)
    {
      protocol->store((uint32)(prof->rusage.ru_inblock-ps->rusage.ru_inblock));
      protocol->store((uint32)(prof->rusage.ru_oublock-ps->rusage.ru_oublock));
    }
    
    if (options & PROFILE_IPC)
    {
      protocol->store((uint32)(prof->rusage.ru_msgsnd - ps->rusage.ru_msgsnd));
      protocol->store((uint32)(prof->rusage.ru_msgrcv - ps->rusage.ru_msgrcv));
    }
    
    if (options & PROFILE_PAGE_FAULTS)
    {
      protocol->store((uint32)(prof->rusage.ru_majflt - ps->rusage.ru_majflt));
      protocol->store((uint32)(prof->rusage.ru_minflt - ps->rusage.ru_minflt));
    }

    if (options & PROFILE_SWAPS)
    {
      protocol->store((uint32)(prof->rusage.ru_nswap - ps->rusage.ru_nswap));
    }
    
    if (options & PROFILE_SOURCE)
    {
      if(prof->function && prof->file)
      {
        protocol->store(prof->function, strlen(prof->function), system_charset_info);        
        protocol->store(prof->file, strlen(prof->file), system_charset_info);
        protocol->store(prof->line);
      } else {
        protocol->store("(unknown)", 10, system_charset_info);
        protocol->store("(unknown)", 10, system_charset_info);
        protocol->store((uint32) 0);
      }
    }

    if (protocol->write())
      DBUG_RETURN(TRUE);
    last_time= prof->time;
  }
  send_eof(thd);
  DBUG_RETURN(FALSE);
}

/* XXX: enabled should be set to the current global profiling setting */
PROFILING::PROFILING()
  :enabled(1), keeping(1), current(NULL), last(NULL)
{
  init_sql_alloc(&root,
                 PROFILE_ALLOC_BLOCK_SIZE,
                 PROFILE_ALLOC_PREALLOC_SIZE);
}

PROFILING::~PROFILING()
{
  free_root(&root, MYF(0));
}

void PROFILING::status(const char *status_arg,
                       const char *function_arg,
                       const char *file_arg, unsigned int line_arg)
{
  DBUG_ENTER("PROFILING::status");
  
  if(!current)
    reset();

  if(unlikely(enabled))
    current->status(status_arg, function_arg, file_arg, line_arg);
  
  thd->proc_info= status_arg;
  
  DBUG_VOID_RETURN;
}

void PROFILING::store()
{
  MEM_ROOT *old_root;
  DBUG_ENTER("PROFILING::store");

  if (last && current && (last->query_id == current->query_id))
    DBUG_VOID_RETURN;

  if (history.elements > 10)  /* XXX: global/session var */
  {
    PROFILE *tmp= history.pop();
    delete tmp;
  }

  old_root= thd->mem_root;
  thd->mem_root= &root;
  
  if (current)
  {
    if (keeping && (!current->entries.is_empty())) {
      last= current;
      history.push_back(current);
      current= NULL;
    } else {
      delete current;
    }
  }
  
  current= new PROFILE(this);
  thd->mem_root= old_root;

  DBUG_VOID_RETURN;
}

void PROFILING::reset()
{
  DBUG_ENTER("PROFILING::reset");

  store();

  current->reset();
  /*free_root(&root, MYF(0));*/
  keep();

  DBUG_VOID_RETURN;
}

bool PROFILING::show_profiles()
{
  PROFILE *prof;
  List<Item> field_list;
  DBUG_ENTER("PROFILING::list_all");

  field_list.push_back(new Item_return_int("Query_ID", 10,
                                           MYSQL_TYPE_LONG));
  field_list.push_back(new Item_return_int("Time", 20,
                                           MYSQL_TYPE_LONGLONG));
  /* TODO: Add another field that lists the query. */
                                           
  if (thd->protocol->send_fields(&field_list,
                                 Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    DBUG_RETURN(TRUE);
    
  SELECT_LEX *sel= &thd->lex->select_lex;
  SELECT_LEX_UNIT *unit= &thd->lex->unit;
  ha_rows idx= 0;
  Protocol *protocol=thd->protocol;

  unit->set_limit(sel);
  
  List_iterator<PROFILE> it(history);
  while ((prof= it++))
  {
    PROFILE_ENTRY *ps= &prof->profile_start;
    PROFILE_ENTRY *pe=  prof->profile_end;

    if (++idx <= unit->offset_limit_cnt)
      continue;
    if (idx > unit->select_limit_cnt)
      break;

    protocol->prepare_for_resend();
    protocol->store((uint32)(prof->query_id));
    protocol->store((ulonglong)((pe->time - ps->time)/10));
    
    if (protocol->write())
      DBUG_RETURN(TRUE);
  }
  send_eof(thd);
  DBUG_RETURN(FALSE);
}

bool PROFILING::show(uint options, uint query_id)
{
  DBUG_ENTER("PROFILING::show");
  PROFILE *prof;

  List_iterator<PROFILE> it(history);
  while ((prof= it++))
  {
    if(prof->query_id == query_id)
      prof->show(options);
  }

  DBUG_RETURN(TRUE);
}

bool PROFILING::show_last(uint options)
{
  DBUG_ENTER("PROFILING::show_last");
  if (!history.is_empty()) {
    DBUG_RETURN(last->show(options));
  }
  DBUG_RETURN(TRUE);
}
