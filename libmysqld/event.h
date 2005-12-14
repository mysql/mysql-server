/* -*- C++ -*- */
#ifndef _EVENT_H_
#define _EVENT_H_
#include "sp_head.h"


extern ulong opt_event_executor;

#define EVEX_OK                 0
#define EVEX_KEY_NOT_FOUND     -1
#define EVEX_OPEN_TABLE_FAILED -2
#define EVEX_WRITE_ROW_FAILED  -3
#define EVEX_DELETE_ROW_FAILED -4
#define EVEX_GET_FIELD_FAILED  -5
#define EVEX_PARSE_ERROR       -6
#define EVEX_INTERNAL_ERROR    -7
#define EVEX_NO_DB_ERROR       -8
#define EVEX_GENERAL_ERROR     -9
#define EVEX_BAD_PARAMS        -10
#define EVEX_NOT_RUNNING       -11

#define EVENT_EXEC_NO_MORE	(1L << 0)
#define EVENT_NOT_USED	(1L << 1)


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

public:
  LEX_STRING m_db;
  LEX_STRING m_name;
  LEX_STRING m_qname;		// db.name
  LEX_STRING m_body;

  LEX_STRING m_definer_user;
  LEX_STRING m_definer_host;
  LEX_STRING m_definer;// combination of user and host

  LEX_STRING m_comment;
  TIME m_starts;
  TIME m_ends;
  TIME m_execute_at;
  longlong m_expr;
  interval_type m_interval;
  longlong m_created;
  longlong m_modified;
  TIME m_last_executed;
  enum enum_event_on_completion m_on_completion;
  enum enum_event_status m_status;
  sp_head *m_sphead;



  uint m_old_cmq;  // Old CLIENT_MULTI_QUERIES value  
  const uchar *m_body_begin;
  
  bool m_dropped;
  bool m_free_sphead_on_delete;
  uint m_flags;//all kind of purposes
  bool m_last_executed_changed;
  bool m_status_changed;

  event_timed():m_expr(0), m_created(0), m_modified(0),
                m_on_completion(MYSQL_EVENT_ON_COMPLETION_DROP),
                m_status(MYSQL_EVENT_ENABLED), m_sphead(0), m_dropped(false),
                m_free_sphead_on_delete(true), m_flags(0),
                m_last_executed_changed(false), m_status_changed(false)
  { init(); }
 
  ~event_timed()
  {
    if (m_free_sphead_on_delete)
	    free_sp();
  }
  
  void
  init();

  int 
  init_definer(THD *thd);
  
  int
  init_execute_at(THD *thd, Item *expr);

  int
  init_interval(THD *thd, Item *expr, interval_type interval);

  void
  init_name(THD *thd, sp_name *name);

  int
  init_starts(THD *thd, Item *starts);

  int
  init_ends(THD *thd, Item *ends);
  
  void
  event_timed::init_body(THD *thd);

  void
  init_comment(THD *thd, LEX_STRING *comment);

  void
  set_on_completion_drop(bool drop);

  void
  set_event_status(bool enabled);

  int
  load_from_row(MEM_ROOT *mem_root, TABLE *table);
  
  bool
  compute_next_execution_time();  

  void
  mark_last_executed();
  
  bool
  drop(THD *thd);
  
  bool
  update_fields(THD *thd);

  char *
  get_show_create_event(THD *thd, uint *length);
  
  int
  execute(THD *thd, MEM_ROOT *mem_root);

  int
  compile(THD *thd, MEM_ROOT *mem_root);
  
  void free_sp()
  {
    if (m_sphead)
    {
      delete m_sphead;
      m_sphead= 0;
    }
  }
};


int
evex_create_event(THD *thd, event_timed *et, uint create_options);

int
evex_update_event(THD *thd, sp_name *name, event_timed *et);

int
evex_drop_event(THD *thd, event_timed *et, bool drop_if_exists);


int
init_events();

void
shutdown_events();

/*
typedef struct st_event_item {
  my_time_t execute_at;
  sp_head *proc;
  char *definer_user;
  char *definer_host;
} EVENT_ITEM;
*/


/*
CREATE TABLE `event` (
  `db` varchar(64) character set latin1 collate latin1_bin NOT NULL default '',
  `name` varchar(64) NOT NULL default '',
  `body` blob NOT NULL,
  `definer` varchar(77) character set latin1 collate latin1_bin NOT NULL default '',
  `execute_at` datetime default NULL,
  `transient_expression` int(11) default NULL,
  `interval_type` enum('YEAR','QUARTER','MONTH','DAY','HOUR','MINUTE','WEEK',
              'SECOND','MICROSECOND','YEAR_MONTH','DAY_HOUR','DAY_MINUTE',
	      'DAY_SECOND','HOUR_MINUTE','HOUR_SECOND','MINUTE_SECOND',
	      'DAY_MICROSECOND','HOUR_MICROSECOND','MINUTE_MICROSECOND',
	      'SECOND_MICROSECOND') DEFAULT NULL,
  `created` timestamp NOT NULL default '0000-00-00 00:00:00',
  `modified` timestamp NOT NULL default '0000-00-00 00:00:00',
  `last_executed` datetime default NULL,
  `starts` datetime default NULL,
  `ends` datetime default NULL,
  `status` enum('ENABLED','DISABLED') NOT NULL default 'ENABLED',
  `on_completion` enum('DROP','PRESERVE') NOT NULL default 'DROP',
  `comment` varchar(64) character set latin1 collate latin1_bin NOT NULL default '',
  PRIMARY KEY  (`db`,`name`)
) ENGINE=MyISAM DEFAULT CHARSET=latin1
*/

#endif /* _EVENT_H_ */
