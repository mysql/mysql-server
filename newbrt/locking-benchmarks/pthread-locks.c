/* How expensive is
 *   - Obtaining a read-only lock for the first obtainer.
 *   - Obtaining it for the second one?
 *   - The third one? */

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/time.h>

float tdiff (struct timeval *start, struct timeval *end) {
    return 1e6*(end->tv_sec-start->tv_sec) +(end->tv_usec - start->tv_usec);
}

/* My own rwlock implementation. */
struct brwl {
    int mutex;
    int state; // 0 for unlocked, -1 for a writer, otherwise many readers
};

static inline int xchg(volatile int *ptr, int x)
{
    __asm__("xchgl %0,%1" :"=r" (x) :"m" (*(ptr)), "0" (x) :"memory");
    return x;
}

static inline void sfence (void) {
    asm volatile ("sfence":::"memory");
}

static inline void brwl_rlock_fence (struct brwl *l) {
    while (xchg(&l->mutex, 1)) ;
    l->state++;
    sfence();
    l->mutex=0;
}

static inline void brwl_rlock_xchg (struct brwl *l) {
    while (xchg(&l->mutex, 1)) ;
    l->state++;
    xchg(&l->mutex, 0);
}

// Something wrong with the compiler  for longs
static inline long
fetch_and_add (volatile long *p, long incr)
{
  long result = incr;

  __asm__ __volatile__ ("lock; xaddl %0, %1" :
			"+r" (result), "+m" (*p) : : "memory");
  return result;
}

static inline int
fetch_and_add_i (volatile int *p, int incr)
{
  int result = incr;

  __asm__ __volatile__ ("lock; xadd %0, %1" :
			"+r" (result), "+m" (*p) : : "memory");
  return result;
}

// Something wrong with the compiler  for longs
/* Returns nonzero if the comparison succeeded. */
static inline long
compare_and_swap_full(volatile long *addr,
		  	     long old, long new_val) 
{
  char result;
  __asm__ __volatile__("lock; cmpxchgl %2, %0; setz %1"
	    	       : "+m"(*(addr)), "=q"(result)
		       : "r" (new_val), "a"(old) : "memory");
  return (int) result;
}

/* Returns nonzero if the comparison succeeded. */
//	Atomically compare *addr to old_val, and replace *addr by new_val
//	if the first comparison succeeds.  Returns nonzero if the comparison
//	succeeded and *addr was updated.
static inline int
compare_and_swap_full_i(volatile int *addr,
			int old, int new_val) 
{
  char result;
  __asm__ __volatile__("lock; cmpxchgl %2, %0; setz %1"
	    	       : "+m"(*(addr)), "=q"(result)
		       : "r" (new_val), "a"(old) : "memory");
  return (int) result;
}

enum {K=100000};
pthread_rwlock_t rwlocks[K];
struct brwl blocks[K];
pthread_mutex_t mlocks[K];
long lvals[K];
int ivals[K];

#define TIME(s, i, init, body) ({    \
   int j_tmp;          		     \
   printf("%-24s", s);               \
   for (j_tmp=0; j_tmp<3; j_tmp++) { \
       struct timeval start,end;     \
       int i;                        \
       for (i=0; i<K; i++) {         \
           init;                     \
       }                             \
       gettimeofday(&start, 0);      \
       for (i=0; i<K; i++) {         \
	   body;	             \
       }                             \
       gettimeofday(&end, 0);        \
       printf(" %9.3fus", tdiff(&start,&end)/K); \
   }                                 \
   printf("\n");                     \
	})

int main (int argc __attribute__((__unused__)), char *argv[] __attribute__((__unused__))) {
    TIME("pthread_mutex_lock", i,
	 ({ int r = pthread_mutex_init(&mlocks[i], NULL); assert(r==0); }),
	 ({ int r = pthread_mutex_lock(&mlocks[i]);       assert(r==0); }));
    TIME("pthread_mutex_unlock", i,
	 ({ int r = pthread_mutex_init(&mlocks[i], NULL); assert(r==0); r = pthread_mutex_lock(&mlocks[i]); assert(r==0); }),
	 ({ int r = pthread_mutex_unlock(&mlocks[i]);     assert(r==0); }));	 
    TIME("pthread_rwlock_tryrdlock", i,
	 ({ int r = pthread_rwlock_init(&rwlocks[i], NULL);  assert(r==0); }),
	 ({ int r = pthread_rwlock_tryrdlock(&rwlocks[i]); assert(r==0); }));
    TIME("pthread_rwlock_rdlock", i,
	 ({ int r = pthread_rwlock_init(&rwlocks[i], NULL); assert(r==0); }),
	 ({ int r = pthread_rwlock_rdlock(&rwlocks[i]);	    assert(r==0); }));
    TIME("brwl_rlock_xchg", i,
	 (blocks[i].state=0, blocks[i].mutex=0),
	 brwl_rlock_xchg(&blocks[i]));
    TIME("brwl_rlock_fence", i,
	 (blocks[i].state=0, blocks[i].mutex=0),
	 brwl_rlock_fence(&blocks[i]));
    int fa=0;
    TIME("fetchadd", i,
	 (void)0,
	 fetch_and_add_i(&fa, i));
    TIME("compare_and_swap", i,
	 ivals[i]=0,
	 ({ int r=compare_and_swap_full_i(&ivals[i], 0, 1); assert(r==1); }));
    return 0;
}
