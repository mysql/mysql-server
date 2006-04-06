/* Copyright (C) 2004-2005 MySQL AB

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

#ifndef _EVENT_H_
#define _EVENT_H_

#include "sp.h"
#include "sp_head.h"

#define EVEX_OK                 SP_OK
#define EVEX_KEY_NOT_FOUND      SP_KEY_NOT_FOUND
#define EVEX_OPEN_TABLE_FAILED  SP_OPEN_TABLE_FAILED
#define EVEX_WRITE_ROW_FAILED   SP_WRITE_ROW_FAILED
#define EVEX_DELETE_ROW_FAILED  SP_DELETE_ROW_FAILED
#define EVEX_GET_FIELD_FAILED   SP_GET_FIELD_FAILED
#define EVEX_PARSE_ERROR        SP_PARSE_ERROR
#define EVEX_INTERNAL_ERROR     SP_INTERNAL_ERROR
#define EVEX_NO_DB_ERROR        SP_NO_DB_ERROR
#define EVEX_COMPILE_ERROR     -19
#define EVEX_GENERAL_ERROR     -20
#define EVEX_BAD_IDENTIFIER     SP_BAD_IDENTIFIER
#define EVEX_BODY_TOO_LONG      SP_BODY_TOO_LONG
#define EVEX_BAD_PARAMS        -21
#define EVEX_NOT_RUNNING       -22
#define EVEX_MICROSECOND_UNSUP -23

#define EVENT_EXEC_NO_MORE      (1L << 0)
#define EVENT_NOT_USED          (1L << 1)

extern ulong opt_event_executor;

enum enum_event_on_completion
{
  MYSQL_EVENT_ON_COMPLETION_DROP = 1,
  MYSQL_EVENT_ON_COMPLETION_PRESERVE
};

enum enum_event_status
{
  MYSQL_EVENT_ENABLED = 1,
  MYSQL_EVENT_DISABLED
};

enum evex_table_field
{
  EVEX_FIELD_DB = 0,
  EVEX_FIELD_NAME,
  EVEX_FIELD_BODY,
  EVEX_FIELD_DEFINER,
  EVEX_FIELD_EXECUTE_AT,
  EVEX_FIELD_INTERVAL_EXPR,
  EVEX_FIELD_TRANSIENT_INTERVAL,
  EVEX_FIELD_CREATED,
  EVEX_FIELD_MODIFIED,
  EVEX_FIELD_LAST_EXECUTED,
  EVEX_FIELD_STARTS,
  EVEX_FIELD_ENDS,
  EVEX_FIELD_STATUS,
  EVEX_FIELD_ON_COMPLETION,
  EVEX_FIELD_SQL_MODE,
  EVEX_FIELD_COMMENT,
  EVEX_FIELD_COUNT /* a cool trick to count the number of fields :) */
} ;

class Event_timed
{
  Event_timed(const Event_timed &);	/* Prevent use of these */
  void operator=(Event_timed &);
  my_bool in_spawned_thread;
  ulong locked_by_thread_id;
  my_bool running;
  pthread_mutex_t LOCK_running;

  bool status_changed;
  bool last_executed_changed;

public:
  TIME last_executed;

  LEX_STRING dbname;
  LEX_STRING name;
  LEX_STRING body;

  LEX_STRING definer_user;
  LEX_STRING definer_host;
  LEX_STRING definer;// combination of user and host

  LEX_STRING comment;
  TIME starts;
  TIME ends;
  TIME execute_at;
  my_bool starts_null;
  my_bool ends_null;
  my_bool execute_at_null;

  longlong expression;
  interval_type interval;

  ulonglong created;
  ulonglong modified;
  enum enum_event_on_completion on_completion;
  enum enum_event_status status;
  sp_head *sphead;
  ulong sql_mode;
  const uchar *body_begin;

  bool dropped;
  bool free_sphead_on_delete;
  uint flags;//all kind of purposes

  static void *operator new(size_t size)
  {
    void *p;
    DBUG_ENTER("Event_timed::new(size)");
    p= my_malloc(size, MYF(0));
    DBUG_PRINT("info", ("alloc_ptr=0x%lx", p));
    DBUG_RETURN(p);
  }

  static void *operator new(size_t size, MEM_ROOT *mem_root)
  { return (void*) alloc_root(mem_root, (uint) size); }

  static void operator delete(void *ptr, size_t size)
  {
    DBUG_ENTER("Event_timed::delete(ptr,size)");
    DBUG_PRINT("enter", ("free_ptr=0x%lx", ptr));
    TRASH(ptr, size);
    my_free((gptr) ptr, MYF(0));
    DBUG_VOID_RETURN;
  }

  static void operator delete(void *ptr, MEM_ROOT *mem_root)
  {
    /*
      Don't free the memory it will be done by the mem_root but
      we need to call the destructor because we free other resources
      which are not allocated on the root but on the heap, or we
      deinit mutexes.
    */
    DBUG_ASSERT(0);
  }


  Event_timed():in_spawned_thread(0),locked_by_thread_id(0),
                running(0), status_changed(false),
                last_executed_changed(false), expression(0), created(0),
                modified(0), on_completion(MYSQL_EVENT_ON_COMPLETION_DROP),
                status(MYSQL_EVENT_ENABLED), sphead(0), sql_mode(0), 
                body_begin(0), dropped(false),
                free_sphead_on_delete(true), flags(0)
                
  {
    pthread_mutex_init(&this->LOCK_running, MY_MUTEX_INIT_FAST);
    init();
  }

  ~Event_timed()
  {    
    deinit_mutexes();

    if (free_sphead_on_delete)
      free_sp();
  }

  void
  init();
  
  void
  deinit_mutexes()
  {
    pthread_mutex_destroy(&this->LOCK_running);
  }

  int
  init_definer(THD *thd);

  int
  init_execute_at(THD *thd, Item *expr);

  int
  init_interval(THD *thd, Item *expr, interval_type new_interval);

  void
  init_name(THD *thd, sp_name *spn);

  int
  init_starts(THD *thd, Item *starts);

  int
  init_ends(THD *thd, Item *ends);

  void
  init_body(THD *thd);

  void
  init_comment(THD *thd, LEX_STRING *set_comment);

  int
  load_from_row(MEM_ROOT *mem_root, TABLE *table);

  bool
  compute_next_execution_time();

  void
  mark_last_executed(THD *thd);

  int
  drop(THD *thd);

  bool
  update_fields(THD *thd);

  int
  get_create_event(THD *thd, String *buf);

  int
  execute(THD *thd, MEM_ROOT *mem_root= NULL);

  int
  compile(THD *thd, MEM_ROOT *mem_root= NULL);

  my_bool
  is_running()
  {
    my_bool ret;

    VOID(pthread_mutex_lock(&this->LOCK_running));
    ret= running;
    VOID(pthread_mutex_unlock(&this->LOCK_running));

    return ret;
  }

  /*
    Checks whether the object is being used in a spawned thread.
    This method is for very basic checking. Use ::can_spawn_now_n_lock()
    for most of the cases.
  */

  my_bool
  can_spawn_now()
  {
    my_bool ret;
    VOID(pthread_mutex_lock(&this->LOCK_running));
    ret= !in_spawned_thread;
    VOID(pthread_mutex_unlock(&this->LOCK_running));
    return ret;  
  }

  /*
    Checks whether this thread can lock the object for modification ->
    preventing being spawned for execution, and locks if possible.
    use ::can_spawn_now() only for basic checking because a race
    condition may occur between the check and eventual modification (deletion)
    of the object.
  */

  my_bool
  can_spawn_now_n_lock(THD *thd);

  int
  spawn_unlock(THD *thd);

  int
  spawn_now(void * (*thread_func)(void*));
  
  void
  spawn_thread_finish(THD *thd);
  
  void
  free_sp()
  {
    delete sphead;
    sphead= 0;
  }
protected:
  bool
  change_security_context(THD *thd, Security_context *s_ctx,
                                       Security_context **backup);

  void
  restore_security_context(THD *thd, Security_context *backup);
};


int
evex_create_event(THD *thd, Event_timed *et, uint create_options,
                  uint *rows_affected);

int
evex_update_event(THD *thd, Event_timed *et, sp_name *new_name,
                  uint *rows_affected);

int
evex_drop_event(THD *thd, Event_timed *et, bool drop_if_exists,
                uint *rows_affected);

int
evex_open_event_table(THD *thd, enum thr_lock_type lock_type, TABLE **table);

int
evex_show_create_event(THD *thd, sp_name *spn, LEX_STRING definer);

int sortcmp_lex_string(LEX_STRING s, LEX_STRING t, CHARSET_INFO *cs);

int
event_reconstruct_interval_expression(String *buf,
                                      interval_type interval,
                                      longlong expression);

int
evex_drop_db_events(THD *thd, char *db);


int
init_events();

void
shutdown_events();


// auxiliary
int
event_timed_compare(Event_timed **a, Event_timed **b);



/*
CREATE TABLE event (
  db char(64) CHARACTER SET utf8 COLLATE utf8_bin NOT NULL default '',
  name char(64) CHARACTER SET utf8 COLLATE utf8_bin NOT NULL default '',
  body longblob NOT NULL,
  definer char(77) CHARACTER SET utf8 COLLATE utf8_bin NOT NULL default '',
  execute_at DATETIME default NULL,
  interval_value int(11) default NULL,
  interval_field ENUM('YEAR','QUARTER','MONTH','DAY','HOUR','MINUTE','WEEK',
                       'SECOND','MICROSECOND', 'YEAR_MONTH','DAY_HOUR',
                       'DAY_MINUTE','DAY_SECOND',
                       'HOUR_MINUTE','HOUR_SECOND',
                       'MINUTE_SECOND','DAY_MICROSECOND',
                       'HOUR_MICROSECOND','MINUTE_MICROSECOND',
                       'SECOND_MICROSECOND') default NULL,
  created TIMESTAMP NOT NULL,
  modified TIMESTAMP NOT NULL,
  last_executed DATETIME default NULL,
  starts DATETIME default NULL,
  ends DATETIME default NULL,
  status ENUM('ENABLED','DISABLED') NOT NULL default 'ENABLED',
  on_completion ENUM('DROP','PRESERVE') NOT NULL default 'DROP',
  comment varchar(64) CHARACTER SET utf8 COLLATE utf8_bin NOT NULL default '',
  PRIMARY KEY  (definer,db,name)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COMMENT 'Events';
*/

#endif /* _EVENT_H_ */
