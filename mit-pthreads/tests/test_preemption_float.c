/* Test to see if floating point state is being properly maintained
   for each thread.  Different threads doing floating point operations
   simultaneously should not interfere with one another.  This
   includes operations that might change some FPU flags, such as
   rounding modes, at least implicitly.  */

#include <pthread.h>
#include <math.h>
#include <stdio.h>

int limit = 2;
int float_passed = 0;
int float_failed = 1;

void *log_loop (void *x) {
  int i;
  double d, d1, d2;
  /* sleep (1); */
  for (i = 0; i < limit; i++) {
    d = 42.0;
    d = log (exp (d));
    d = (d + 39.0) / d;
    if (i == 0)
      d1 = d;
    else {
		d2 = d;
		d = sin(d);
		/* if (d2 != d1) { */
		if (memcmp (&d2, &d1, 8)) {
			pthread_exit(&float_failed);
		}
	}
  }
  pthread_exit(&float_passed);
}

void *trig_loop (void *x) {
  int i;
  double d, d1, d2;
  /* sleep (1);  */
  for (i = 0; i < limit; i++) {
    d = 35.0;
    d *= M_PI;
    d /= M_LN2;
    d = sin (d);
    d = cos (1 / d);
    if (i == 0)
      d1 = d;
    else {
		d2 = d;
		d = sin(d);
		/* if (d2 != d1) { */
		if (memcmp (&d2, &d1, 8)) {
  			pthread_exit(&float_failed);
		}
	}
  }
  pthread_exit(&float_passed);
}

#define N 10
int main () {
  int i;
  pthread_t thread[2];
  pthread_attr_t attr;
  int *x, *y;

  pthread_init ();
  pthread_attr_init(&attr);
  pthread_attr_setfloatstate(&attr, PTHREAD_NOFLOAT);

  while(limit < 100000) {
    pthread_create (&thread[0], &attr, trig_loop, 0);
    pthread_create (&thread[1], &attr, log_loop, 0);
  	pthread_join(thread[0], (void **) &x);	
  	pthread_join(thread[1], (void **) &y);	
  	if ((*x == float_failed) || (*y == float_failed)) {
		limit *= 4;
		break;
	}
	limit *= 4;
  }
  if ((*x == float_passed) && (*y == float_passed)) {
	printf("test_preemption_float INDETERMINATE\n");
    return(0);
  }
  pthread_create (&thread[0], NULL, trig_loop, 0);
  pthread_create (&thread[1], NULL, log_loop, 0);
  pthread_join(thread[0], (void **) &x);	
  pthread_join(thread[1], (void **) &y);	

  if ((*x == float_failed) || (*y == float_failed)) {
	printf("test_preemption_float FAILED\n");
	return(1);
  }
  printf("test_preemption_float PASSED\n");
  return(0);
}
