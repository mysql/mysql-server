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

#ifndef _EVENT_PRIV_H_
#define _EVENT_PRIV_H_
#include "mysql_priv.h"


#define EVEX_USE_QUEUE

#define UNLOCK_MUTEX_AND_BAIL_OUT(__mutex, __label) \
    { VOID(pthread_mutex_unlock(&__mutex)); goto __label; }

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



int
my_time_compare(TIME *a, TIME *b);

int
evex_db_find_event_aux(THD *thd, const LEX_STRING dbname,
                       const LEX_STRING rname, TABLE *table);
                       
TABLE *
evex_open_event_table(THD *thd, enum thr_lock_type lock_type);

int 
event_timed_compare_q(void *vptr, byte* a, byte *b);

int
evex_time_diff(TIME *a, TIME *b);



#define EXEC_QUEUE_QUEUE_NAME executing_queue
#define EXEC_QUEUE_DARR_NAME evex_executing_queue


#define EVEX_QUEUE_TYPE QUEUE
#define EVEX_PTOQEL byte *

#define EVEX_EQ_NAME executing_queue
#define evex_queue_first_element(queue, __cast) ((__cast)queue_top(queue))
#define evex_queue_element(queue, idx, __cast) ((__cast)queue_element(queue, idx))
#define evex_queue_delete_element(queue, idx)  queue_remove(queue, idx)
#define evex_queue_destroy(queue)              delete_queue(queue)
#define evex_queue_first_updated(queue)        queue_replaced(queue)
#define evex_queue_insert(queue, element)      queue_insert_safe(queue, element);



void
evex_queue_init(EVEX_QUEUE_TYPE *queue);

#define evex_queue_num_elements(queue) queue.elements


extern bool evex_is_running;
extern MEM_ROOT evex_mem_root;
extern pthread_mutex_t LOCK_event_arrays,
                       LOCK_workers_count,
                       LOCK_evex_running;
extern ulonglong evex_main_thread_id;
extern QUEUE EVEX_EQ_NAME;

#endif /* _EVENT_PRIV_H_ */
