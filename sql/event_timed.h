#ifndef _EVENT_TIMED_H_
#define _EVENT_TIMED_H_
/* Copyright (C) 2004-2006 MySQL AB

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


#define EVEX_OK                  0
#define EVEX_KEY_NOT_FOUND      -1
#define EVEX_OPEN_TABLE_FAILED  -2
#define EVEX_WRITE_ROW_FAILED   -3
#define EVEX_DELETE_ROW_FAILED  -4
#define EVEX_GET_FIELD_FAILED   -5
#define EVEX_PARSE_ERROR        -6
#define EVEX_INTERNAL_ERROR     -7
#define EVEX_NO_DB_ERROR        -8
#define EVEX_COMPILE_ERROR     -19
#define EVEX_GENERAL_ERROR     -20
#define EVEX_BAD_IDENTIFIER    -21 
#define EVEX_BODY_TOO_LONG     -22
#define EVEX_BAD_PARAMS        -23
#define EVEX_NOT_RUNNING       -24
#define EVEX_MICROSECOND_UNSUP -25
#define EVEX_CANT_KILL         -26

#define EVENT_EXEC_NO_MORE      (1L << 0)
#define EVENT_NOT_USED          (1L << 1)
#define EVENT_FREE_WHEN_FINISHED (1L << 2)


class sp_head;

class Event_timed
{
  Event_timed(const Event_timed &);	/* Prevent use of these */
  void operator=(Event_timed &);
  my_bool in_spawned_thread;
  ulong locked_by_thread_id;
  my_bool running;
  ulong thread_id;
  pthread_mutex_t LOCK_running;
  pthread_cond_t COND_finished;

  bool status_changed;
  bool last_executed_changed;

public:
  enum enum_status
  {
    ENABLED = 1,
    DISABLED
  };

  enum enum_on_completion
  {
    ON_COMPLETION_DROP = 1,
    ON_COMPLETION_PRESERVE
  };

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
  enum enum_on_completion on_completion;
  enum enum_status status;
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

  Event_timed();

  ~Event_timed();

  void
  init();

  void
  deinit_mutexes();

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

  int
  drop(THD *thd);

  void
  mark_last_executed(THD *thd);

  bool
  update_fields(THD *thd);

  int
  get_create_event(THD *thd, String *buf);

  int
  execute(THD *thd, MEM_ROOT *mem_root);

  int
  compile(THD *thd, MEM_ROOT *mem_root);

  bool
  is_running();

  int
  spawn_now(void * (*thread_func)(void*), void *arg);
  
  bool
  spawn_thread_finish(THD *thd);
  
  void
  free_sp();

  bool
  has_equal_db(Event_timed *etn);

  int
  kill_thread(THD *thd);

  void
  set_thread_id(ulong tid) { thread_id= tid; }
};
 
#endif /* _EVENT_H_ */
