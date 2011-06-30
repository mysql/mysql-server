/* Copyright (c) 2000, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/* For use with thr_lock:s */

#ifndef _thr_lock_h
#define _thr_lock_h
#ifdef	__cplusplus
extern "C" {
#endif

#include <my_pthread.h>
#include <my_list.h>

struct st_thr_lock;
extern ulong locks_immediate,locks_waited ;

/*
  Important: if a new lock type is added, a matching lock description
             must be added to sql_test.cc's lock_descriptions array.
*/
enum thr_lock_type { TL_IGNORE=-1,
		     TL_UNLOCK,			/* UNLOCK ANY LOCK */
                     /*
                       Parser only! At open_tables() becomes TL_READ or
                       TL_READ_NO_INSERT depending on the binary log format
                       (SBR/RBR) and on the table category (log table).
                       Used for tables that are read by statements which
                       modify tables.
                     */
                     TL_READ_DEFAULT,
		     TL_READ,			/* Read lock */
		     TL_READ_WITH_SHARED_LOCKS,
		     /* High prior. than TL_WRITE. Allow concurrent insert */
		     TL_READ_HIGH_PRIORITY,
		     /* READ, Don't allow concurrent insert */
		     TL_READ_NO_INSERT,
		     /* 
			Write lock, but allow other threads to read / write.
			Used by BDB tables in MySQL to mark that someone is
			reading/writing to the table.
		      */
		     TL_WRITE_ALLOW_WRITE,
		     /*
		       WRITE lock used by concurrent insert. Will allow
		       READ, if one could use concurrent insert on table.
		     */
		     TL_WRITE_CONCURRENT_INSERT,
		     /* Write used by INSERT DELAYED.  Allows READ locks */
		     TL_WRITE_DELAYED,
                     /* 
                       parser only! Late bound low_priority flag. 
                       At open_tables() becomes thd->update_lock_default.
                     */
                     TL_WRITE_DEFAULT,
		     /* WRITE lock that has lower priority than TL_READ */
		     TL_WRITE_LOW_PRIORITY,
		     /* Normal WRITE lock */
		     TL_WRITE,
		     /* Abort new lock request with an error */
		     TL_WRITE_ONLY};

enum enum_thr_lock_result { THR_LOCK_SUCCESS= 0, THR_LOCK_ABORTED= 1,
                            THR_LOCK_WAIT_TIMEOUT= 2, THR_LOCK_DEADLOCK= 3 };


extern ulong max_write_lock_count;
extern my_bool thr_lock_inited;
extern enum thr_lock_type thr_upgraded_concurrent_insert_lock;

/*
  A description of the thread which owns the lock. The address
  of an instance of this structure is used to uniquely identify the thread.
*/

typedef struct st_thr_lock_info
{
  pthread_t thread;
  my_thread_id thread_id;
} THR_LOCK_INFO;


typedef struct st_thr_lock_data {
  THR_LOCK_INFO *owner;
  struct st_thr_lock_data *next,**prev;
  struct st_thr_lock *lock;
  mysql_cond_t *cond;
  enum thr_lock_type type;
  void *status_param;			/* Param to status functions */
  void *debug_print_param;
  struct PSI_table *m_psi;
} THR_LOCK_DATA;

struct st_lock_list {
  THR_LOCK_DATA *data,**last;
};

typedef struct st_thr_lock {
  LIST list;
  mysql_mutex_t mutex;
  struct st_lock_list read_wait;
  struct st_lock_list read;
  struct st_lock_list write_wait;
  struct st_lock_list write;
  /* write_lock_count is incremented for write locks and reset on read locks */
  ulong write_lock_count;
  uint read_no_write_count;
  void (*get_status)(void*, int);	/* When one gets a lock */
  void (*copy_status)(void*,void*);
  void (*update_status)(void*);		/* Before release of write */
  void (*restore_status)(void*);         /* Before release of read */
  my_bool (*check_status)(void *);
} THR_LOCK;


extern LIST *thr_lock_thread_list;
extern mysql_mutex_t THR_LOCK_lock;

my_bool init_thr_lock(void);		/* Must be called once/thread */
void thr_lock_info_init(THR_LOCK_INFO *info);
void thr_lock_init(THR_LOCK *lock);
void thr_lock_delete(THR_LOCK *lock);
void thr_lock_data_init(THR_LOCK *lock,THR_LOCK_DATA *data,
			void *status_param);
enum enum_thr_lock_result thr_lock(THR_LOCK_DATA *data,
                                   THR_LOCK_INFO *owner,
                                   enum thr_lock_type lock_type,
                                   ulong lock_wait_timeout);
void thr_unlock(THR_LOCK_DATA *data);
enum enum_thr_lock_result thr_multi_lock(THR_LOCK_DATA **data,
                                         uint count, THR_LOCK_INFO *owner,
                                         ulong lock_wait_timeout);
void thr_multi_unlock(THR_LOCK_DATA **data,uint count);
void
thr_lock_merge_status(THR_LOCK_DATA **data, uint count);
void thr_abort_locks(THR_LOCK *lock, my_bool upgrade_lock);
my_bool thr_abort_locks_for_thread(THR_LOCK *lock, my_thread_id thread);
void thr_print_locks(void);		/* For debugging */
my_bool thr_upgrade_write_delay_lock(THR_LOCK_DATA *data,
                                     enum thr_lock_type new_lock_type,
                                     ulong lock_wait_timeout);
void    thr_downgrade_write_lock(THR_LOCK_DATA *data,
                                 enum thr_lock_type new_lock_type);
my_bool thr_reschedule_write_lock(THR_LOCK_DATA *data,
                                  ulong lock_wait_timeout);
void thr_set_lock_wait_callback(void (*before_wait)(void),
                                void (*after_wait)(void));
#ifdef	__cplusplus
}
#endif
#endif /* _thr_lock_h */
