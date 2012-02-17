/* Copyright (c) 2008, 2011, Oracle and/or its affiliates. All rights reserved.

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

#ifndef _waiting_threads_h
#define _waiting_threads_h

#include <my_global.h>
#include <my_sys.h>

#include <lf.h>

C_MODE_START

typedef struct st_wt_resource_id WT_RESOURCE_ID;
typedef struct st_wt_resource WT_RESOURCE;

typedef struct st_wt_resource_type {
  my_bool (*compare)(const void *a, const void *b);
  const void *(*make_key)(const WT_RESOURCE_ID *id, uint *len); /* not used */
} WT_RESOURCE_TYPE;

struct st_wt_resource_id {
  ulonglong value;
  const WT_RESOURCE_TYPE *type;
};
/* the below differs from sizeof(WT_RESOURCE_ID) by the amount of padding */
#define sizeof_WT_RESOURCE_ID (sizeof(ulonglong)+sizeof(void*))

#define WT_WAIT_STATS  24
#define WT_CYCLE_STATS 32
extern ulonglong wt_wait_table[WT_WAIT_STATS];
extern uint32    wt_wait_stats[WT_WAIT_STATS+1];
extern uint32    wt_cycle_stats[2][WT_CYCLE_STATS+1];
extern uint32    wt_success_stats;

typedef struct st_wt_thd {
  /*
    XXX
    there's no protection (mutex) against concurrent access of the
    dynarray below. it is assumed that a caller will have it anyway
    (not to protect this array but to protect its own - caller's -
    data structures), and we'll get it for free. A caller needs to
    ensure that a blocker won't release a resource before a blocked
    thread starts waiting, which is usually done with a mutex.
    
    If the above assumption is wrong, we'll need to add a mutex here.
  */
  DYNAMIC_ARRAY   my_resources;
  /*
    'waiting_for' is modified under waiting_for->lock, and only by thd itself
    'waiting_for' is read lock-free (using pinning protocol), but a thd object
    can read its own 'waiting_for' without any locks or tricks.
  */
  WT_RESOURCE    *waiting_for;
  LF_PINS        *pins;

  /* pointers to values */
  const ulong *timeout_short;
  const ulong *deadlock_search_depth_short;
  const ulong *timeout_long;
  const ulong *deadlock_search_depth_long;

  /*
    weight relates to the desirability of a transaction being killed if it's
    part of a deadlock. In a deadlock situation transactions with lower weights
    are killed first.

    Examples of using the weight to implement different selection strategies:

    1. Latest
        Keep all weights equal.
    2. Random
        Assight weights at random.
        (variant: modify a weight randomly before every lock request)
    3. Youngest
        Set weight to -NOW()
    4. Minimum locks
        count locks granted in your lock manager, store the value as a weight
    5. Minimum work
        depends on the definition of "work". For example, store the number
        of rows modifies in this transaction (or a length of REDO log for a
        transaction) as a weight.

    It is only statistically relevant and is not protected by any locks.
  */
  ulong volatile weight;
  /*
    'killed' is indirectly protected by waiting_for->lock because
    a killed thread needs to clear its 'waiting_for' and thus needs a lock.
    That is a thread needs an exclusive lock to read 'killed' reliably.
    But other threads may change 'killed' from 0 to 1, a shared
    lock is enough for that.
   */
  my_bool killed;
#ifndef DBUG_OFF
  const char     *name;
#endif
} WT_THD;

#define WT_TIMEOUT              ETIMEDOUT
#define WT_OK                   0
#define WT_DEADLOCK             -1
#define WT_DEPTH_EXCEEDED       -2
#define WT_FREE_TO_GO           -3

void wt_init(void);
void wt_end(void);
void wt_thd_lazy_init(WT_THD *, const ulong *, const ulong *, const ulong *, const ulong *);
void wt_thd_destroy(WT_THD *);
int wt_thd_will_wait_for(WT_THD *, WT_THD *, const WT_RESOURCE_ID *);
int wt_thd_cond_timedwait(WT_THD *, mysql_mutex_t *);
void wt_thd_release(WT_THD *, const WT_RESOURCE_ID *);
#define wt_thd_release_all(THD) wt_thd_release((THD), 0)
my_bool wt_resource_id_memcmp(const void *, const void *);

C_MODE_END

#endif
