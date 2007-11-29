#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#if defined(__x86_64) || defined(__i386)

static inline void mfence (void) {
    __asm__ volatile ("mfence":::"memory");
}
static inline void rfence (void) {
    __asm__ volatile ("rfence":::"memory");
}
static inline void sfence (void) {
    __asm__ volatile ("sfence":::"memory");
}

/* According to the Intel Architecture Software Developer's
 * Manual, Volume 3: System Programming Guide
 * (http://www.intel.com/design/pro/manuals/243192.htm), page 7-6,
 * "For the P6 family processors, locked operations serialize all
 * outstanding load and store operations (that is, wait for them to
 * complete)."
 *
 * Bradley found that fence instructions is faster on an opteron
	 *   mfence takes 8ns on a 1.5GHZ AMD64 (maybe this is an 801)
	 *   sfence takes 5ns
	 *   lfence takes 3ns
	 *   xchgl  takes 14ns
 */

static inline lock_xchgl(volatile int *ptr, int x)
{
    __asm__("xchgl %0,%1" :"=r" (x) :"m" (*(ptr)), "0" (x) :"memory");
    return x;
}

#endif

typedef volatile int SPINLOCK[1];

static inline void spin_init (SPINLOCK v) {
    v[0] = 0;
    mfence();
}

static inline void spin_lock (SPINLOCK v) {
    while (lock_xchgl((int*)v, 1)!=0) {
	while (v[0]); /* Spin using only reads.  It would be better to use MCS locks, but this reduces bus traffic. */
    }
}
static inline void spin_unlock (SPINLOCK v) {
    sfence(); // Want all previous stores to take place before we unlock.
    v[0]=0;
}

#else
#error Need to define architectur-specific stuff for other machines.
#endif
