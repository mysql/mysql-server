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


class event_timed
{
  event_timed(const event_timed &);	/* Prevent use of these */
  void operator=(event_timed &);
  my_bool running;
  pthread_mutex_t LOCK_running;

  bool status_changed;
  bool last_executed_changed;
  TIME last_executed;

public:
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

  longlong expression;
  interval_type interval;

  longlong created;
  longlong modified;
  enum enum_event_on_completion on_completion;
  enum enum_event_status status;
  sp_head *sphead;

  const uchar *body_begin;
  
  bool dropped;
  bool free_sphead_on_delete;
  uint flags;//all kind of purposes

  event_timed():running(0), status_changed(false), last_executed_changed(false),
                expression(0), created(0), modified(0),
                on_completion(MYSQL_EVENT_ON_COMPLETION_DROP),
                status(MYSQL_EVENT_ENABLED), sphead(0), dropped(false),
                free_sphead_on_delete(true), flags(0)
                
  {
    pthread_mutex_init(&this->LOCK_running, MY_MUTEX_INIT_FAST);
    init();
  }
 
  ~event_timed()
  {
    pthread_mutex_destroy(&this->LOCK_running);
    if (free_sphead_on_delete)
	    free_sp();
  }
  
  void
  init();

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
  event_timed::init_body(THD *thd);

  void
  init_comment(THD *thd, LEX_STRING *set_comment);

  int
  load_from_row(MEM_ROOT *mem_root, TABLE *table);
  
  bool
  compute_next_execution_time();  

  void
  mark_last_executed();
  
  int
  drop(THD *thd);
  
  bool
  update_fields(THD *thd);

  char *
  get_show_create_event(THD *thd, uint *length);
  
  int
  execute(THD *thd, MEM_ROOT *mem_root= NULL);

  int
  compile(THD *thd, MEM_ROOT *mem_root= NULL);
  
  void free_sp()
  {
    delete sphead;
    sphead= 0;
  }
};


int
evex_create_event(THD *thd, event_timed *et, uint create_options);

int
evex_update_event(THD *thd, event_timed *et, sp_name *new_name);

int
evex_drop_event(THD *thd, event_timed *et, bool drop_if_exists);


int
init_events();

void
shutdown_events();


// auxiliary
int 
event_timed_compare(event_timed **a, event_timed **b);


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
  PRIMARY KEY  (db,name)
) ENGINE=MyISAM DEFAULT CHARSET=utf8 COMMENT 'Events';
*/

#endif /* _EVENT_H_ */
