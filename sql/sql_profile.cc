/* Copyright (C) 2007 MySQL AB

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


#include "mysql_priv.h"
#include "my_sys.h"

#define TIME_FLOAT_DIGITS 9
#define MAX_QUERY_LENGTH 300

bool schema_table_store_record(THD *thd, TABLE *table);

/* Reserved for systems that can't record the function name in source. */
const char * const _unknown_func_ = "<unknown>";

/**
  Connects Information_Schema and Profiling.
*/
int fill_query_profile_statistics_info(THD *thd, struct st_table_list *tables, 
                                       Item *cond)
{
#if defined(ENABLED_PROFILING) && defined(COMMUNITY_SERVER)
  return(thd->profiling.fill_statistics_info(thd, tables, cond));
#else
  return(1);
#endif
}

ST_FIELD_INFO query_profile_statistics_info[]=
{
  /* name, length, type, value, maybe_null, old_name */
  {"QUERY_ID", 20, MYSQL_TYPE_LONG, 0, false, NULL},
  {"SEQ", 20, MYSQL_TYPE_LONG, 0, false, NULL},
  {"STATE", 30, MYSQL_TYPE_STRING, 0, false, NULL},
  {"DURATION", TIME_FLOAT_DIGITS, MYSQL_TYPE_DOUBLE, 0, false, NULL},
  {"CPU_USER", TIME_FLOAT_DIGITS, MYSQL_TYPE_DOUBLE, 0, true, NULL},
  {"CPU_SYSTEM", TIME_FLOAT_DIGITS, MYSQL_TYPE_DOUBLE, 0, true, NULL},
  {"CONTEXT_VOLUNTARY", 20, MYSQL_TYPE_LONG, 0, true, NULL},
  {"CONTEXT_INVOLUNTARY", 20, MYSQL_TYPE_LONG, 0, true, NULL},
  {"BLOCK_OPS_IN", 20, MYSQL_TYPE_LONG, 0, true, NULL},
  {"BLOCK_OPS_OUT", 20, MYSQL_TYPE_LONG, 0, true, NULL},
  {"MESSAGES_SENT", 20, MYSQL_TYPE_LONG, 0, true, NULL},
  {"MESSAGES_RECEIVED", 20, MYSQL_TYPE_LONG, 0, true, NULL},
  {"PAGE_FAULTS_MAJOR", 20, MYSQL_TYPE_LONG, 0, true, NULL},
  {"PAGE_FAULTS_MINOR", 20, MYSQL_TYPE_LONG, 0, true, NULL},
  {"SWAPS", 20, MYSQL_TYPE_LONG, 0, true, NULL},
  {"SOURCE_FUNCTION", 30, MYSQL_TYPE_STRING, 0, true, NULL},
  {"SOURCE_FILE", 20, MYSQL_TYPE_STRING, 0, true, NULL},
  {"SOURCE_LINE", 20, MYSQL_TYPE_LONG, 0, true, NULL},
  {NULL, 0, MYSQL_TYPE_STRING, 0, true, NULL}
};

#if defined(ENABLED_PROFILING) && defined(COMMUNITY_SERVER)

#define RUSAGE_USEC(tv)  ((tv).tv_sec*1000*1000 + (tv).tv_usec)
#define RUSAGE_DIFF_USEC(tv1, tv2) (RUSAGE_USEC((tv1))-RUSAGE_USEC((tv2)))

PROFILE_ENTRY::PROFILE_ENTRY()
  :profile(NULL), status(NULL), function(NULL), file(NULL), line(0), 
  time_usecs(0.0), allocated_status_memory(NULL)
{
  collect();

  /* The beginning of the query, before any state is set. */
  set_status("(initialization)", NULL, NULL, 0);  
}

PROFILE_ENTRY::PROFILE_ENTRY(QUERY_PROFILE *profile_arg, const char *status_arg)
  :profile(profile_arg)
{
  collect();
  set_status(status_arg, NULL, NULL, 0);
}

PROFILE_ENTRY::PROFILE_ENTRY(QUERY_PROFILE *profile_arg, const char *status_arg,
                             const char *function_arg,
                             const char *file_arg, unsigned int line_arg)
  :profile(profile_arg)
{
  collect();
  set_status(status_arg, function_arg, file_arg, line_arg);
}

PROFILE_ENTRY::~PROFILE_ENTRY()
{
  if (allocated_status_memory != NULL)
    my_free(allocated_status_memory, MYF(0));
  status= function= file= NULL;
}
  
void PROFILE_ENTRY::set_status(const char *status_arg, const char *function_arg, const char *file_arg, unsigned int line_arg)
{
  size_t sizes[3];                              /* 3 == status+function+file */
  char *cursor;

  /*
    Compute all the space we'll need to allocate one block for everything
    we'll need, instead of N mallocs.
  */
  sizes[0]= (status_arg == NULL) ? 0 : strlen(status_arg) + 1;
  sizes[1]= (function_arg == NULL) ? 0 : strlen(function_arg) + 1;
  sizes[2]= (file_arg == NULL) ? 0 : strlen(file_arg) + 1;
    
  allocated_status_memory= (char *) my_malloc(sizes[0] + sizes[1] + sizes[2], MYF(0));
  DBUG_ASSERT(allocated_status_memory != NULL);

  cursor= allocated_status_memory;

  if (status_arg != NULL)
  {
    strcpy(cursor, status_arg);
    status= cursor;
    cursor+= sizes[0];
  }
  else
    status= NULL;

  if (function_arg != NULL)
  {
    strcpy(cursor, function_arg);
    function= cursor;
    cursor+= sizes[1];
  }
  else
    function= NULL;

  if (file_arg != NULL)
  {
    strcpy(cursor, file_arg);
    file= cursor;
    cursor+= sizes[2];
  }
  else
    file= NULL;

  line= line_arg;
}

void PROFILE_ENTRY::collect()
{
  time_usecs= (double) my_getsystime() / 10.0;  /* 1 sec was 1e7, now is 1e6 */
#ifdef HAVE_GETRUSAGE
  getrusage(RUSAGE_SELF, &rusage);
#endif
}

QUERY_PROFILE::QUERY_PROFILE(PROFILING *profiling_arg, char *query_source_arg,
                             uint query_length_arg)
  :profiling(profiling_arg), server_query_id(0), profiling_query_id(0),
  query_source(NULL)
{
  profile_end= &profile_start;
  set_query_source(query_source_arg, query_length_arg);
}

void QUERY_PROFILE::set_query_source(char *query_source_arg, 
                                     uint query_length_arg)
{
  if (! profiling->enabled)
    return;

  /* Truncate to avoid DoS attacks. */
  uint length= min(MAX_QUERY_LENGTH, query_length_arg); 
  /* TODO?: Provide a way to include the full text, as in  SHOW PROCESSLIST. */

  DBUG_ASSERT(query_source == NULL);
  if (query_source_arg != NULL)
    query_source= my_strdup_with_length(query_source_arg, length, MYF(0));
}

QUERY_PROFILE::~QUERY_PROFILE()
{
  while (! entries.is_empty())
    delete entries.pop();

  if (query_source != NULL)
    my_free(query_source, MYF(0));
}

void QUERY_PROFILE::status(const char *status_arg,
                     const char *function_arg= NULL,
                     const char *file_arg= NULL, unsigned int line_arg= 0)
{
  THD *thd= profiling->thd;
  PROFILE_ENTRY *prof;
  DBUG_ENTER("QUERY_PROFILE::status");

  /* Blank status.  Just return, and thd->proc_info will be set blank later. */
  if (unlikely(status_arg == NULL))
    DBUG_VOID_RETURN;

  /* If thd->proc_info is currently set to status_arg, don't profile twice. */
  if (likely((thd->proc_info != NULL) && 
      ((thd->proc_info == status_arg) || 
       (strcmp(thd->proc_info, status_arg) == 0))))
  {
    DBUG_VOID_RETURN;
  }

  /* Is this the same query as our profile currently contains? */
  if (unlikely((thd->query_id != server_query_id) && !thd->spcont))
    reset();
    
  if (function_arg && file_arg) 
  {
    if ((profile_end= prof= new PROFILE_ENTRY(this, status_arg, function_arg, 
                                              file_arg, line_arg)))
      entries.push_back(prof);
  } 
  else 
  {
    if ((profile_end= prof= new PROFILE_ENTRY(this, status_arg)))
      entries.push_back(prof);
  }

  DBUG_VOID_RETURN;
}

void QUERY_PROFILE::reset()
{
  DBUG_ENTER("QUERY_PROFILE::reset");
  if (likely(profiling->thd->query_id != server_query_id))
  {
    server_query_id= profiling->thd->query_id; /* despite name, is global */
    profile_start.collect();

    while (! entries.is_empty())
      delete entries.pop();
  }
  DBUG_VOID_RETURN;
}

bool QUERY_PROFILE::show(uint options)
{  
  THD *thd= profiling->thd;
  List<Item> field_list;
  DBUG_ENTER("QUERY_PROFILE::show");

  field_list.push_back(new Item_empty_string("Status", MYSQL_ERRMSG_SIZE));
  field_list.push_back(new Item_return_int("Duration", TIME_FLOAT_DIGITS,
                                           MYSQL_TYPE_DOUBLE));

  if (options & PROFILE_CPU)
  {
    field_list.push_back(new Item_return_int("CPU_user", TIME_FLOAT_DIGITS,
                                             MYSQL_TYPE_DOUBLE));
    field_list.push_back(new Item_return_int("CPU_system", TIME_FLOAT_DIGITS, 
                                             MYSQL_TYPE_DOUBLE));
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
  
  if (options & PROFILE_SOURCE)
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

  Protocol *protocol= thd->protocol;
  SELECT_LEX *sel= &thd->lex->select_lex;
  SELECT_LEX_UNIT *unit= &thd->lex->unit;
  ha_rows idx= 0;
  unit->set_limit(sel);
  PROFILE_ENTRY *previous= &profile_start;

  PROFILE_ENTRY *entry;
  void *iterator;
  for (iterator= entries.new_iterator(); 
       iterator != NULL; 
       iterator= entries.iterator_next(iterator))
  {
    entry= entries.iterator_value(iterator);

#ifdef HAVE_GETRUSAGE
    struct rusage *rusage= &(entry->rusage);
#endif
    String elapsed;

    if (++idx <= unit->offset_limit_cnt)
      continue;
    if (idx > unit->select_limit_cnt)
      break;

    protocol->prepare_for_resend();

    /*
      This entry, n, has a point in time, T(n), and a status phrase, S(n).  The
      status phrase S(n) describes the period of time that begins at T(n).  The
      previous status phrase S(n-1) describes the period of time that starts at
      T(n-1) and ends at T(n).  Since we want to describe the time that a status
      phrase took T(n)-T(n-1), this line must describe the previous status.
    */
    protocol->store(previous->status, strlen(previous->status), 
                    system_charset_info);
    protocol->store((double)(entry->time_usecs - 
                    previous->time_usecs)/(1000.0*1000),
                    (uint32) TIME_FLOAT_DIGITS-1, &elapsed);

    if (options & PROFILE_CPU)
    {
#ifdef HAVE_GETRUSAGE
      String cpu_utime, cpu_stime;
      protocol->store((double)(RUSAGE_DIFF_USEC(rusage->ru_utime,
                      previous->rusage.ru_utime))/(1000.0*1000),
                      (uint32) TIME_FLOAT_DIGITS-1, &cpu_utime);
      protocol->store((double)(RUSAGE_DIFF_USEC(rusage->ru_stime,
                      previous->rusage.ru_stime))/(1000.0*1000),
                      (uint32) TIME_FLOAT_DIGITS-1, &cpu_stime);
#else
      protocol->store_null();
      protocol->store_null();
#endif
    }
    
    if (options & PROFILE_CONTEXT)
    {
#ifdef HAVE_GETRUSAGE
      protocol->store((uint32)(rusage->ru_nvcsw - previous->rusage.ru_nvcsw));
      protocol->store((uint32)(rusage->ru_nivcsw - previous->rusage.ru_nivcsw));
#else
      protocol->store_null();
      protocol->store_null();
#endif
    }

    if (options & PROFILE_BLOCK_IO)
    {
#ifdef HAVE_GETRUSAGE
      protocol->store((uint32)(rusage->ru_inblock - previous->rusage.ru_inblock));
      protocol->store((uint32)(rusage->ru_oublock - previous->rusage.ru_oublock));
#else
      protocol->store_null();
      protocol->store_null();
#endif
    }
    
    if (options & PROFILE_IPC)
    {
#ifdef HAVE_GETRUSAGE
      protocol->store((uint32)(rusage->ru_msgsnd - previous->rusage.ru_msgsnd));
      protocol->store((uint32)(rusage->ru_msgrcv - previous->rusage.ru_msgrcv));
#else
      protocol->store_null();
      protocol->store_null();
#endif
    }
    
    if (options & PROFILE_PAGE_FAULTS)
    {
#ifdef HAVE_GETRUSAGE
      protocol->store((uint32)(rusage->ru_majflt - previous->rusage.ru_majflt));
      protocol->store((uint32)(rusage->ru_minflt - previous->rusage.ru_minflt));
#else
      protocol->store_null();
      protocol->store_null();
#endif
    }

    if (options & PROFILE_SWAPS)
    {
#ifdef HAVE_GETRUSAGE
      protocol->store((uint32)(rusage->ru_nswap - previous->rusage.ru_nswap));
#else
      protocol->store_null();
#endif
    }
    
    if (options & PROFILE_SOURCE)
    {
      if ((entry->function != NULL) && (entry->file != NULL))
      {
        protocol->store(entry->function, strlen(entry->function),
                        system_charset_info);        
        protocol->store(entry->file, strlen(entry->file), system_charset_info);
        protocol->store((uint32) entry->line);
      } else {
        protocol->store_null();
        protocol->store_null();
        protocol->store_null();
      }
    }

    if (protocol->write())
      DBUG_RETURN(TRUE);

    previous= entry;
  }
  send_eof(thd);
  DBUG_RETURN(FALSE);
}

PROFILING::PROFILING()
  :profile_id_counter(1), keeping(TRUE), enabled(FALSE), current(NULL), last(NULL)
{
}

PROFILING::~PROFILING()
{
  while (! history.is_empty())
    delete history.pop();

  if (current != NULL)
    delete current;
}

void PROFILING::status_change(const char *status_arg,
                              const char *function_arg,
                              const char *file_arg, unsigned int line_arg)
{
  DBUG_ENTER("PROFILING::status_change");
  
  if (unlikely(enabled))
  {
    if (unlikely(current == NULL))
      reset();

    DBUG_ASSERT(current != NULL);

    current->status(status_arg, function_arg, file_arg, line_arg);
  }

  thd->proc_info= status_arg;
  DBUG_VOID_RETURN;
}

void PROFILING::store()
{
  DBUG_ENTER("PROFILING::store");

  /* Already stored */
  if (unlikely((last != NULL) && 
               (current != NULL) && 
               (last->server_query_id == current->server_query_id)))
  {
    DBUG_VOID_RETURN;
  }

  while (history.elements > thd->variables.profiling_history_size)
    delete history.pop();

  if (likely(((thd)->options & OPTION_PROFILING) == 0))
    DBUG_VOID_RETURN;

  if (current != NULL)
  {
    if (keeping && 
        (enabled) &&                                    /* ON at start? */
        (((thd)->options & OPTION_PROFILING) != 0) &&   /* and ON at end? */
        (current->query_source != NULL) && 
        (current->query_source[0] != '\0') && 
        (!current->entries.is_empty())) 
    {
      current->profiling_query_id= next_profile_id();   /* assign an id */

      last= current; /* never contains something that is not in the history. */
      history.push_back(current);
      current= NULL;
    } 
    else
    {
      delete current;
      current= NULL;
    }
  }
  
  DBUG_ASSERT(current == NULL);
  if (enabled)
    current= new QUERY_PROFILE(this, thd->query, thd->query_length);

  DBUG_VOID_RETURN;
}

/**
  Store and clean up the old information and get ready to hold info about this
  new query.  This is called very often so it must be very lightweight if
  profiling is not active.
*/
void PROFILING::reset()
{
  DBUG_ENTER("PROFILING::reset");

  store();

  if (likely(((thd)->options & OPTION_PROFILING) == 0))
  {
    enabled= FALSE;
    DBUG_VOID_RETURN;
  }
  else
    enabled= TRUE;

  if (current != NULL)
    current->reset();
  keep();

  DBUG_VOID_RETURN;
}

bool PROFILING::show_profiles()
{
  DBUG_ENTER("PROFILING::show_profiles");
  QUERY_PROFILE *prof;
  List<Item> field_list;

  field_list.push_back(new Item_return_int("Query_ID", 10,
                                           MYSQL_TYPE_LONG));
  field_list.push_back(new Item_return_int("Duration", TIME_FLOAT_DIGITS-1,
                                           MYSQL_TYPE_DOUBLE));
  field_list.push_back(new Item_empty_string("Query", 40));
                                           
  if (thd->protocol->send_fields(&field_list,
                                 Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF))
    DBUG_RETURN(TRUE);
    
  SELECT_LEX *sel= &thd->lex->select_lex;
  SELECT_LEX_UNIT *unit= &thd->lex->unit;
  ha_rows idx= 0;
  Protocol *protocol= thd->protocol;

  unit->set_limit(sel);
  
  void *iterator;
  for (iterator= history.new_iterator(); 
       iterator != NULL; 
       iterator= history.iterator_next(iterator))
  {
    prof= history.iterator_value(iterator);

    String elapsed;

    PROFILE_ENTRY *ps= &prof->profile_start;
    PROFILE_ENTRY *pe=  prof->profile_end;

    if (++idx <= unit->offset_limit_cnt)
      continue;
    if (idx > unit->select_limit_cnt)
      break;

    protocol->prepare_for_resend();
    protocol->store((uint32)(prof->profiling_query_id));
    protocol->store((double)(pe->time_usecs - ps->time_usecs)/(1000.0*1000), 
                    (uint32) TIME_FLOAT_DIGITS-1, &elapsed);
    if (prof->query_source != NULL)
      protocol->store(prof->query_source, strlen(prof->query_source), 
                      system_charset_info);
    else
      protocol->store_null();
    
    if (protocol->write())
      DBUG_RETURN(TRUE);
  }
  send_eof(thd);
  DBUG_RETURN(FALSE);
}

/*
  This is an awful hack to let prepared statements tell us the query
  that they're executing.
*/
void PROFILING::set_query_source(char *query_source_arg, uint query_length_arg)
{
  DBUG_ENTER("PROFILING::set_query_source");

  /* We can't get this query source through normal means. */
  DBUG_ASSERT((thd->query == NULL) || (thd->query_length == 0));

  if (current != NULL)
    current->set_query_source(query_source_arg, query_length_arg);
  else
    DBUG_PRINT("info", ("no current profile to send query source to"));
  DBUG_VOID_RETURN;
}

bool PROFILING::show(uint options, uint profiling_query_id)
{
  DBUG_ENTER("PROFILING::show");
  QUERY_PROFILE *prof;

  void *iterator;
  for (iterator= history.new_iterator(); 
       iterator != NULL; 
       iterator= history.iterator_next(iterator))
  {
    prof= history.iterator_value(iterator);

    if(prof->profiling_query_id == profiling_query_id)
      DBUG_RETURN(prof->show(options));
  }

  my_error(ER_WRONG_ARGUMENTS, MYF(0), "SHOW PROFILE");
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


/**
  Fill the information schema table, "query_profile", as defined in show.cc .
*/
int PROFILING::fill_statistics_info(THD *thd, struct st_table_list *tables, Item *cond)
{
  DBUG_ENTER("PROFILING::fill_statistics_info");
  TABLE *table= tables->table;
  ulonglong row_number= 0;

  QUERY_PROFILE *query;
  /* Go through each query in this thread's stored history... */
  void *history_iterator;
  for (history_iterator= history.new_iterator(); 
       history_iterator != NULL; 
       history_iterator= history.iterator_next(history_iterator))
  {
    query= history.iterator_value(history_iterator);
    PROFILE_ENTRY *previous= &(query->profile_start);

    /*
      Because we put all profiling info into a table that may be reordered, let
      us also include a numbering of each state per query.  The query_id and
      the "seq" together are unique.
    */
    ulonglong seq;

    void *entry_iterator;
    PROFILE_ENTRY *entry;
    /* ...and for each query, go through all its state-change steps. */
    for (seq= 0, entry_iterator= query->entries.new_iterator(); 
         entry_iterator != NULL; 
         entry_iterator= query->entries.iterator_next(entry_iterator),
         seq++, previous=entry, row_number++)
    {
      entry= query->entries.iterator_value(entry_iterator);

      /* Set default values for this row. */
      restore_record(table, s->default_values);

      /*
        The order of these fields is set by the  query_profile_statistics_info
        array.
      */
      table->field[0]->store((ulonglong) query->profiling_query_id);
      table->field[1]->store((ulonglong) seq); /* the step in the sequence */
      /*
        This entry, n, has a point in time, T(n), and a status phrase, S(n).
        The status phrase S(n) describes the period of time that begins at
        T(n).  The previous status phrase S(n-1) describes the period of time
        that starts at T(n-1) and ends at T(n).  Since we want to describe the
        time that a status phrase took T(n)-T(n-1), this line must describe the
        previous status.
      */
      table->field[2]->store(previous->status, strlen(previous->status), 
                             system_charset_info);
      table->field[3]->store((double)(entry->time_usecs - 
                             previous->time_usecs)/(1000*1000));

#ifdef HAVE_GETRUSAGE
      table->field[4]->store((double)RUSAGE_DIFF_USEC(entry->rusage.ru_utime,
                             previous->rusage.ru_utime)/(1000.0*1000));
      table->field[4]->set_notnull();
      table->field[5]->store((double)RUSAGE_DIFF_USEC(entry->rusage.ru_stime,
                             previous->rusage.ru_stime)/(1000.0*1000));

      table->field[5]->set_notnull();
#else
      /* TODO: Add CPU-usage info for non-BSD systems */
#endif
      
#ifdef HAVE_GETRUSAGE
      table->field[6]->store((uint32)(entry->rusage.ru_nvcsw - 
                             previous->rusage.ru_nvcsw));
      table->field[6]->set_notnull();
      table->field[7]->store((uint32)(entry->rusage.ru_nivcsw - 
                             previous->rusage.ru_nivcsw));
      table->field[7]->set_notnull();
#else
      /* TODO: Add context switch info for non-BSD systems */
#endif

#ifdef HAVE_GETRUSAGE
      table->field[8]->store((uint32)(entry->rusage.ru_inblock - 
                             previous->rusage.ru_inblock));
      table->field[8]->set_notnull();
      table->field[9]->store((uint32)(entry->rusage.ru_oublock - 
                             previous->rusage.ru_oublock));
      table->field[9]->set_notnull();
#else
      /* TODO: Add block IO info for non-BSD systems */
#endif
    
#ifdef HAVE_GETRUSAGE
      table->field[10]->store((uint32)(entry->rusage.ru_msgsnd - 
                             previous->rusage.ru_msgsnd), true);
      table->field[10]->set_notnull();
      table->field[11]->store((uint32)(entry->rusage.ru_msgrcv - 
                             previous->rusage.ru_msgrcv), true);
      table->field[11]->set_notnull();
#else
      /* TODO: Add message info for non-BSD systems */
#endif
    
#ifdef HAVE_GETRUSAGE
      table->field[12]->store((uint32)(entry->rusage.ru_majflt - 
                             previous->rusage.ru_majflt), true);
      table->field[12]->set_notnull();
      table->field[13]->store((uint32)(entry->rusage.ru_minflt - 
                             previous->rusage.ru_minflt), true);
      table->field[13]->set_notnull();
#else
      /* TODO: Add page fault info for non-BSD systems */
#endif

#ifdef HAVE_GETRUSAGE
      table->field[14]->store((uint32)(entry->rusage.ru_nswap - 
                             previous->rusage.ru_nswap), true);
      table->field[14]->set_notnull();
#else
      /* TODO: Add swap info for non-BSD systems */
#endif
    
      if ((entry->function != NULL) && (entry->file != NULL))
      {
        table->field[15]->store(entry->function, strlen(entry->function),
                        system_charset_info);        
        table->field[15]->set_notnull();
        table->field[16]->store(entry->file, strlen(entry->file), system_charset_info);
        table->field[16]->set_notnull();
        table->field[17]->store(entry->line, true);
        table->field[17]->set_notnull();
      }

      if (schema_table_store_record(thd, table))
        DBUG_RETURN(1);

    }
  }

  DBUG_RETURN(0);
}
#endif /* ENABLED_PROFILING */
