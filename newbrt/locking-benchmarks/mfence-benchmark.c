/* Time {m,l,s}fence vs.xchgl for a memory barrier. */

/* Timing numbers:
 * Intel T2500 2GHZ

do1       9.0ns/loop
mfence:  29.0ns/loop  (marginal cost=  20.0ns)
sfence:  17.3ns/loop  (marginal cost=   8.3ns)
lfence:  23.6ns/loop  (marginal cost=  14.6ns)
 xchgl:  35.8ns/loop  (marginal cost=  26.8ns)

* AMD Athlon 64 X2 Dual Core Processor 4200+
  Timings are more crazy

do1      20.6ns/loop
mfence:  12.9ns/loop  (marginal cost=  -7.6ns)
sfence:   8.4ns/loop  (marginal cost= -12.1ns)
lfence:  20.2ns/loop  (marginal cost=  -0.3ns)
 xchgl:  16.6ns/loop  (marginal cost=  -3.9ns)

do1      13.0ns/loop
mfence:  25.6ns/loop  (marginal cost=  12.6ns)
sfence:  21.0ns/loop  (marginal cost=   8.1ns)
lfence:  12.9ns/loop  (marginal cost=  -0.1ns)
 xchgl:  29.3ns/loop  (marginal cost=  16.3ns)

*/


#include <sys/time.h>
#include <stdio.h>

enum { COUNT = 100000000 };

static inline void xchgl (void) {
    {
	/*
	 * According to the Intel Architecture Software Developer's
	 * Manual, Volume 3: System Programming Guide
	 * (http://www.intel.com/design/pro/manuals/243192.htm), page
	 * 7-6, "For the P6 family processors, locked operations
	 * serialize all outstanding load and store operations (that
	 * is, wait for them to complete)."  
	 * Since xchg is locked by default, it is one way to do membar.
	 */
	int x=0, y;
	asm volatile ("xchgl %0,%1" :"=r" (x) :"m" (y), "0" (x) :"memory");
   }
}

static inline void mfence (void) {
    asm volatile ("mfence":::"memory");
}

static inline void lfence (void) {
    asm volatile ("lfence":::"memory");
}

static inline void sfence (void) {
    asm volatile ("sfence":::"memory");
}


double tdiff (struct timeval *start, struct timeval *end) {
    return ((end->tv_sec-start->tv_sec + 1e-6*(end->tv_usec + start->tv_usec))/COUNT)*1e9;
}

double nop_cost;

void do1 (volatile int *x) {
    int i;
    struct timeval start, end;
    gettimeofday(&start, 0);
    for (i=0; i<COUNT; i++) {
	x[0]++;
	x[1]++;
	x[2]++;
	x[3]++;
    }
    gettimeofday(&end, 0);
    printf("do1    %6.1fns/loop\n", nop_cost=tdiff(&start, &end));
}

#define doit(name) void do ##name (volatile int *x) { \
    int i;                      \
    struct timeval start, end;  \
    gettimeofday(&start, 0);    \
    for (i=0; i<COUNT; i++) {   \
	x[0]++;                 \
	x[1]++;                 \
	name();                 \
	x[2]++;                 \
	x[3]++;                 \
    }                           \
    gettimeofday(&end, 0);      \
    double this_cost = tdiff(&start, &end); \
    printf("%6s:%6.1fns/loop  (marginal cost=%6.1fns)\n",  #name, this_cost, this_cost-nop_cost); \
}


doit(mfence)
doit(lfence)
doit(sfence)
doit(xchgl)

int main (int argc __attribute__((__unused__)), 
	  char *argv[] __attribute__((__unused__))) {
    int x[4];
    int i;
    for (i=0; i<4; i++) {
	do1(x);
	domfence(x);
	dosfence(x);
	dolfence(x);
	doxchgl(x);
    }
    return 0;
}
