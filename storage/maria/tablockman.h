/* Copyright (C) 2006 MySQL AB

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
enum lock_type { N, S, X, IS, IX, SIX, LS, LX, SLX, LSIX };
enum lockman_getlock_result {
  DIDNT_GET_THE_LOCK=0, GOT_THE_LOCK,
  GOT_THE_LOCK_NEED_TO_LOCK_A_SUBRESOURCE,
  GOT_THE_LOCK_NEED_TO_INSTANT_LOCK_A_SUBRESOURCE
};

#endif

#define LOCK_TYPES LSIX

typedef struct st_table_lock_owner TABLE_LOCK_OWNER;
typedef struct st_table_lock TABLE_LOCK;
typedef struct st_locked_table LOCKED_TABLE;
typedef TABLE_LOCK_OWNER *loid_to_tlo_func(uint16);

typedef struct {
  pthread_mutex_t pool_mutex;
  TABLE_LOCK *pool;
  uint lock_timeout;
  loid_to_tlo_func *loid_to_lo;
} TABLOCKMAN;

struct st_table_lock_owner {
  TABLE_LOCK *active_locks, *waiting_lock;
  TABLE_LOCK_OWNER  *waiting_for;
  pthread_cond_t  *cond;    /* transactions waiting for this, wait on 'cond' */
  pthread_mutex_t *mutex;   /* mutex is required to use 'cond'               */
  uint16    loid;
};

struct st_locked_table {
  pthread_mutex_t mutex;
  HASH active;                              // fast to remove
  TABLE_LOCK *active_locks[LOCK_TYPES];     // fast to see a conflict
  TABLE_LOCK *wait_queue_in, *wait_queue_out;
};

void tablockman_init(TABLOCKMAN *, loid_to_tlo_func *, uint);
void tablockman_destroy(TABLOCKMAN *);
enum lockman_getlock_result tablockman_getlock(TABLOCKMAN *, TABLE_LOCK_OWNER *,
                                             LOCKED_TABLE *,
                                             enum lock_type lock);
void tablockman_release_locks(TABLOCKMAN *, TABLE_LOCK_OWNER *);
void tablockman_init_locked_table(LOCKED_TABLE *);
void print_tlo(TABLE_LOCK_OWNER *);

#endif

