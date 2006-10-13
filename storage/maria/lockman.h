/* Copyright (C) 2000 MySQL AB

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

#ifndef _lockman_h
#define _lockman_h

/*
  N    - "no lock", not a lock, used sometimes to simplify the code
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
enum lock_type { N, S, X, IS, IX, SIX, LS, LX, SLX, LSIX };

struct lockman_lock;

typedef struct st_lock_owner LOCK_OWNER;
struct st_lock_owner {
  LF_PINS  *pins;
  struct lockman_lock *all_locks;
  LOCK_OWNER  *waiting_for;
  pthread_cond_t  *cond;    /* transactions waiting for this, wait on 'cond' */
  pthread_mutex_t *mutex;   /* mutex is required to use 'cond'               */
  uint16    loid;
};

typedef LOCK_OWNER *loid_to_lo_func(uint16);
typedef struct {
  LF_DYNARRAY array;                    /* hash itself */
  LF_ALLOCATOR alloc;                   /* allocator for elements */
  int32 volatile size;                  /* size of array */
  int32 volatile count;                 /* number of elements in the hash */
  uint lock_timeout;
  loid_to_lo_func *loid_to_lo;
} LOCKMAN;

enum lockman_getlock_result {
  DIDNT_GET_THE_LOCK=0, GOT_THE_LOCK,
  GOT_THE_LOCK_NEED_TO_LOCK_A_SUBRESOURCE,
  GOT_THE_LOCK_NEED_TO_INSTANT_LOCK_A_SUBRESOURCE
};

void lockman_init(LOCKMAN *, loid_to_lo_func *, uint);
void lockman_destroy(LOCKMAN *);
enum lockman_getlock_result lockman_getlock(LOCKMAN *lm, LOCK_OWNER *lo,
                                            uint64 resource,
                                            enum lock_type lock);
int lockman_release_locks(LOCKMAN *, LOCK_OWNER *);

#ifdef EXTRA_DEBUG
void print_lockhash(LOCKMAN *lm);
#endif

#endif
