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

typedef struct {pthread_rwlock_t rw;} my_atomic_rwlock_t;

#ifdef MY_ATOMIC_EXTRA_DEBUG
#define CHECK_RW if (rw) if (a->rw) assert(rw == a->rw); else a->rw=rw;
#else
#define CHECK_RW
#endif

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
#else
#define my_atomic_rwlock_destroy(name)     pthread_rwlock_destroy(& (name)->rw)
#define my_atomic_rwlock_init(name)        pthread_rwlock_init(& (name)->rw, 0)
#define my_atomic_rwlock_rdlock(name)      pthread_rwlock_rdlock(& (name)->rw)
#define my_atomic_rwlock_wrlock(name)      pthread_rwlock_wrlock(& (name)->rw)
#define my_atomic_rwlock_rdunlock(name)    pthread_rwlock_unlock(& (name)->rw)
#define my_atomic_rwlock_wrunlock(name)    pthread_rwlock_unlock(& (name)->rw)
#endif

#ifdef HAVE_INLINE

#define make_atomic_add(S)						\
static inline uint ## S my_atomic_add ## S(				\
        my_atomic_ ## S ## _t *a, uint ## S v, my_atomic_rwlock_t *rw)	\
{									\
  uint ## S ret;							\
  CHECK_RW;								\
  if (rw) my_atomic_rwlock_wrlock(rw);					\
  ret= a->val;								\
  a->val+= v;								\
  if (rw) my_atomic_rwlock_wrunlock(rw);				\
  return ret;								\
}

#define make_atomic_swap(S)						\
static inline uint ## S my_atomic_swap ## S(				\
        my_atomic_ ## S ## _t *a, uint ## S v, my_atomic_rwlock_t *rw)	\
{									\
  uint ## S ret;							\
  CHECK_RW;								\
  if (rw) my_atomic_rwlock_wrlock(rw);					\
  ret= a->val;								\
  a->val= v;								\
  if (rw) my_atomic_rwlock_wrunlock(rw);				\
  return ret;								\
}

#define make_atomic_cas(S)						\
static inline uint my_atomic_cas ## S(my_atomic_ ## S ## _t *a,		\
        uint ## S *cmp, uint ## S set, my_atomic_rwlock_t *rw)		\
{									\
  uint ret;								\
  CHECK_RW;								\
  if (rw) my_atomic_rwlock_wrlock(rw);					\
  if (ret= (a->val == *cmp)) a->val= set; else *cmp=a->val;		\
  if (rw) my_atomic_rwlock_wrunlock(rw);				\
  return ret;								\
}

#define make_atomic_load(S)						\
static inline uint ## S my_atomic_load ## S(				\
        my_atomic_ ## S ## _t *a, my_atomic_rwlock_t *rw)		\
{									\
  uint ## S ret;							\
  CHECK_RW;								\
  if (rw) my_atomic_rwlock_wrlock(rw);					\
  ret= a->val;								\
  if (rw) my_atomic_rwlock_wrunlock(rw);				\
  return ret;								\
}

#define make_atomic_store(S)						\
static inline void my_atomic_store ## S(				\
        my_atomic_ ## S ## _t *a, uint ## S v, my_atomic_rwlock_t *rw)	\
{									\
  CHECK_RW;								\
  if (rw) my_atomic_rwlock_rdlock(rw);					\
  (a)->val= (v);							\
  if (rw) my_atomic_rwlock_rdunlock(rw);				\
}

#else /* no inline functions */

#define make_atomic_add(S)						\
extern uint ## S my_atomic_add ## S(					\
        my_atomic_ ## S ## _t *a, uint ## S v, my_atomic_rwlock_t *rw);

#define make_atomic_swap(S)						\
extern uint ## S my_atomic_swap ## S(					\
        my_atomic_ ## S ## _t *a, uint ## S v, my_atomic_rwlock_t *rw);

#define make_atomic_cas(S)						\
extern uint my_atomic_cas ## S(my_atomic_ ## S ## _t *a,		\
        uint ## S *cmp, uint ## S set, my_atomic_rwlock_t *rw);

#define make_atomic_load(S)						\
extern uint ## S my_atomic_load ## S(					\
        my_atomic_ ## S ## _t *a, my_atomic_rwlock_t *rw);

#define make_atomic_store(S)						\
extern void my_atomic_store ## S(					\
        my_atomic_ ## S ## _t *a, uint ## S v, my_atomic_rwlock_t *rw);

#endif

make_atomic_add( 8)
make_atomic_add(16)
make_atomic_add(32)
make_atomic_add(64)
make_atomic_cas( 8)
make_atomic_cas(16)
make_atomic_cas(32)
make_atomic_cas(64)
make_atomic_load( 8)
make_atomic_load(16)
make_atomic_load(32)
make_atomic_load(64)
make_atomic_store( 8)
make_atomic_store(16)
make_atomic_store(32)
make_atomic_store(64)
make_atomic_swap( 8)
make_atomic_swap(16)
make_atomic_swap(32)
make_atomic_swap(64)
#undef make_atomic_add
#undef make_atomic_cas
#undef make_atomic_load
#undef make_atomic_store
#undef make_atomic_swap
#undef CHECK_RW


