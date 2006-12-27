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

typedef struct {pthread_rwlock_t rw;} my_atomic_rwlock_t;

#ifdef MY_ATOMIC_MODE_DUMMY
/*
  the following can never be enabled by ./configure, one need to put #define in
  a source to trigger the following warning. The resulting code will be broken,
  it only makes sense to do it to see now test_atomic detects broken
  implementations (another way is to run a UP build on an SMP box).
*/
#warning MY_ATOMIC_MODE_DUMMY and MY_ATOMIC_MODE_RWLOCKS are incompatible
#define my_atomic_rwlock_destroy(name)
#define my_atomic_rwlock_init(name)
#define my_atomic_rwlock_rdlock(name)
#define my_atomic_rwlock_wrlock(name)
#define my_atomic_rwlock_rdunlock(name)
#define my_atomic_rwlock_wrunlock(name)
#define MY_ATOMIC_MODE "dummy (non-atomic)"
#else
#define my_atomic_rwlock_destroy(name)     pthread_rwlock_destroy(& (name)->rw)
#define my_atomic_rwlock_init(name)        pthread_rwlock_init(& (name)->rw, 0)
#define my_atomic_rwlock_rdlock(name)      pthread_rwlock_rdlock(& (name)->rw)
#define my_atomic_rwlock_wrlock(name)      pthread_rwlock_wrlock(& (name)->rw)
#define my_atomic_rwlock_rdunlock(name)    pthread_rwlock_unlock(& (name)->rw)
#define my_atomic_rwlock_wrunlock(name)    pthread_rwlock_unlock(& (name)->rw)
#define MY_ATOMIC_MODE "rwlocks"
#endif

#define make_atomic_add_body(S)     int ## S sav; sav= *a; *a+= v; v=sav;
#define make_atomic_swap_body(S)    int ## S sav; sav= *a; *a= v; v=sav;
#define make_atomic_cas_body(S)     if ((ret= (*a == *cmp))) *a= set; else *cmp=*a;
#define make_atomic_load_body(S)    ret= *a;
#define make_atomic_store_body(S)   *a= v;

