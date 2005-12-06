/* Copyright (C) 2000-2003 MySQL AB

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

#ifndef _EVENT_PRIV_H_
#define _EVENT_PRIV_H_


enum
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
  EVEX_FIELD_COMMENT,
  EVEX_FIELD_COUNT /* a cool trick to count the number of fields :) */
};

extern bool evex_is_running;
extern bool mysql_event_table_exists;
extern DYNAMIC_ARRAY events_array;
extern DYNAMIC_ARRAY evex_executing_queue;
extern MEM_ROOT evex_mem_root;
extern pthread_mutex_t LOCK_event_arrays,
                       LOCK_workers_count,
                       LOCK_evex_running;


int
my_time_compare(TIME *a, TIME *b);

int
evex_db_find_routine_aux(THD *thd, const LEX_STRING dbname,
                       const LEX_STRING rname, TABLE *table);
                       
TABLE *
evex_open_event_table(THD *thd, enum thr_lock_type lock_type);             
#endif /* _EVENT_PRIV_H_ */
