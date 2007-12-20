/* Copyright (C) 2006 MySQL AB

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

#ifndef _tablockman_h
#define _tablockman_h

/*
  Lock levels:
  ^^^^^^^^^^^

  N    - "no lock", not a lock, used sometimes internally to simplify the code
  S    - Shared
  X    - eXclusive
  IS   - Intention Shared
  IX   - Intention eXclusive
  SIX  - Shared + Intention eXclusive
  LS   - Loose Shared
  LX   - Loose eXclusive
  SLX  - Shared + Loose eXclusive
  LSIX - Loose Shared + Intention eXclusive
*/
#ifndef _lockman_h
/* QQ: TODO remove N-locks */
enum lockman_lock_type { N, S, X, IS, IX, SIX, LS, LX, SLX, LSIX, LOCK_TYPE_LAST };
enum lockman_getlock_result {
  NO_MEMORY_FOR_LOCK=1, DEADLOCK, LOCK_TIMEOUT,
  GOT_THE_LOCK,
  GOT_THE_LOCK_NEED_TO_LOCK_A_SUBRESOURCE,
  GOT_THE_LOCK_NEED_TO_INSTANT_LOCK_A_SUBRESOURCE
};
#endif

#define LOCK_TYPES (LOCK_TYPE_LAST-1)

typedef struct st_table_lock TABLE_LOCK;

typedef struct st_table_lock_owner {
  TABLE_LOCK *active_locks;                          /* list of active locks */
  TABLE_LOCK *waiting_lock;                  /* waiting lock (one lock only) */
  struct st_table_lock_owner *waiting_for; /* transaction we're waiting for  */
  pthread_cond_t  *cond;      /* transactions waiting for us, wait on 'cond' */
  pthread_mutex_t *mutex;                 /* mutex is required to use 'cond' */
  uint16    loid, waiting_for_loid;                 /* Lock Owner IDentifier */
} TABLE_LOCK_OWNER;

typedef struct st_locked_table {
  pthread_mutex_t mutex;                        /* mutex for everything below */
  HASH latest_locks;                                /* latest locks in a hash */
  TABLE_LOCK *active_locks[LOCK_TYPES];          /* dl-list of locks per type */
  TABLE_LOCK *wait_queue_in, *wait_queue_out; /* wait deque (double-end queue)*/
} LOCKED_TABLE;

typedef TABLE_LOCK_OWNER *loid_to_tlo_func(uint16);

typedef struct {
  pthread_mutex_t pool_mutex;
  TABLE_LOCK *pool;                                /* lifo pool of free locks */
  uint lock_timeout;                          /* lock timeout in milliseconds */
  loid_to_tlo_func *loid_to_tlo;      /* for mapping loid to TABLE_LOCK_OWNER */
} TABLOCKMAN;

void tablockman_init(TABLOCKMAN *, loid_to_tlo_func *, uint);
void tablockman_destroy(TABLOCKMAN *);
enum lockman_getlock_result tablockman_getlock(TABLOCKMAN *, TABLE_LOCK_OWNER *,
                                               LOCKED_TABLE *, enum lockman_lock_type);
void tablockman_release_locks(TABLOCKMAN *, TABLE_LOCK_OWNER *);
void tablockman_init_locked_table(LOCKED_TABLE *, int);
void tablockman_destroy_locked_table(LOCKED_TABLE *);

#ifdef EXTRA_DEBUG
void tablockman_print_tlo(TABLE_LOCK_OWNER *);
#endif

#endif

