/************************************************************************
The test module for the syncronization primitives

(c) 1995 Innobase Oy

Created 9/9/1995 Heikki Tuuri
*************************************************************************/


#include "../sync0sync.h"
#include "../sync0rw.h"
#include "../sync0arr.h"
#include "../sync0ipm.h"
#include "ut0ut.h"
#include "mem0mem.h"
#include "os0sync.h"
#include "os0thread.h"
#include "os0sync.h"

mutex_t	mutex;
mutex_t	mutex1;
mutex_t	mutex2;
mutex_t	mutex3;
mutex_t	mutex4;

ip_mutex_t	ip_mutex;

ip_mutex_t	ip_mutex1;
ip_mutex_t	ip_mutex2;
ip_mutex_t	ip_mutex3;
ip_mutex_t	ip_mutex4;

ip_mutex_hdl_t*	iph;

ip_mutex_hdl_t*	iph1;
ip_mutex_hdl_t*	iph2;
ip_mutex_hdl_t*	iph3;
ip_mutex_hdl_t*	iph4;


rw_lock_t	rw1;
rw_lock_t	rw2;
rw_lock_t	rw3;
rw_lock_t	rw4;

rw_lock_t	rw9;
rw_lock_t	rw10;
mutex_t		mutex9;

os_mutex_t	osm;

ulint	last_thr;
ulint	switch_count;
ulint	glob_count;
ulint	glob_inc;
ulint	rc;

bool	qprint		= FALSE;

/********************************************************************
Start function for thread 1 in test1. */
ulint
thread1(void* arg)
/*==============*/
{
	ulint	i, j;
	void*	arg2;

	arg2 = arg;

	printf("Thread1 started!\n");

	mutex_enter(&mutex);

	printf("Thread1 owns now the mutex!\n");

	j = 0;
	
	for (i = 1; i < 1000000; i++) {
		j += i;
	}

	printf("Thread1 releases now the mutex!\n");
	
	mutex_exit(&mutex);

	return(j);
}

/********************************************************************
Start function for thread 2 in test1. */
ulint
thread2(void* arg)
/*==============*/
{
	ulint	i, j;
	void*	arg2;

	arg2 = arg;

	printf("Thread2 started!\n");

	mutex_enter(&mutex);

	printf("Thread2 owns now the mutex!\n");

	j = 0;
	
	for (i = 1; i < 1000000; i++) {
		j += i;
	}

	printf("Thread2 releases now the mutex!\n");
	
	mutex_exit(&mutex);

	return(j);
}

/********************************************************************
Start function for the competing threads in test2. The function tests
the behavior lock-coupling through 4 mutexes. */

ulint
thread_n(volatile void* arg)
/*========================*/
{
	ulint	i, j, k, n;

	n = *((ulint*)arg);

	printf("Thread %ld started!\n", n);

	for (k = 0; k < 2000 * UNIV_DBC; k++) {
	
		mutex_enter(&mutex1);

		if (last_thr != n) {
			switch_count++;
			last_thr = n;
		}
		
		j = 0;
	
		for (i = 1; i < 400; i++) {
			j += i;
		}
	
		mutex_enter(&mutex2);

		mutex_exit(&mutex1);

		for (i = 1; i < 400; i++) {
			j += i;
		}
		mutex_enter(&mutex3);

		mutex_exit(&mutex2);

		for (i = 1; i < 400; i++) {
			j += i;
		}
		mutex_enter(&mutex4);

		mutex_exit(&mutex3);

		for (i = 1; i < 400; i++) {
			j += i;
		}

		mutex_exit(&mutex4);
	}

	printf("Thread %ld exits!\n", n);
	
	return(j);
}

/********************************************************************
Start function for mutex exclusion checking in test3. */

ulint
thread_x(void* arg)
/*===============*/
{
	ulint	k;
	void*	arg2;

	arg2 = arg;

	printf("Starting thread!\n");
	
	for (k = 0; k < 200000 * UNIV_DBC; k++) {
	
		mutex_enter(&mutex);

		glob_count += glob_inc;
		
		mutex_exit(&mutex);

	}

	printf("Exiting thread!\n");
	
	return(0);
}



void 
test1(void)
/*=======*/
{
	os_thread_t		thr1, thr2;
	os_thread_id_t		id1, id2;
	ulint			i, j;
	ulint			tm, oldtm;
	ulint*			lp;
	
        printf("-------------------------------------------\n");
	printf("SYNC-TEST 1. Test of mutexes.\n");


	printf("Main thread %ld starts!\n",
		os_thread_get_curr_id());

	osm = os_mutex_create(NULL);	

	os_mutex_enter(osm);
	os_mutex_exit(osm);
	
	os_mutex_free(osm);

	
	mutex_create(&mutex);

	lp = &j;
	
	oldtm = ut_clock();

	for (i = 0; i < 1000000; i++) {
		id1 = os_thread_get_curr_id();
	}

	tm = ut_clock();
	printf("Wall clock time for %ld thread_get_id %ld milliseconds\n",
			i, tm - oldtm);


	oldtm = ut_clock();

	for (i = 0; i < 100000 * UNIV_DBC; i++) {

		mutex_enter(&mutex);
		mutex_exit(&mutex);
	}

	tm = ut_clock();
	printf("Wall clock time for %ld mutex lock-unlock %ld milliseconds\n",
			i, tm - oldtm);

	oldtm = ut_clock();

	for (i = 0; i < 1000000; i++) {

		mutex_fence();
	}

	tm = ut_clock();
	printf("Wall clock time for %ld fences %ld milliseconds\n",
			i, tm - oldtm);

	mutex_enter(&mutex);

	mutex_list_print_info();

	ut_ad(1 == mutex_n_reserved());
	ut_ad(FALSE == sync_all_freed());
	
	thr1 = os_thread_create(thread1,
				  NULL,
				  &id1);

	printf("Thread1 created, id %ld \n", id1);

	thr2 = os_thread_create(thread2,
				  NULL,
				  &id2);

	printf("Thread2 created, id %ld \n", id2);
	

	j = 0;
	
	for (i = 1; i < 20000000; i++) {
		j += i;
	}

	sync_print();

	sync_array_validate(sync_primary_wait_array);
	
	printf("Main thread releases now mutex!\n");

	mutex_exit(&mutex);

	os_thread_wait(thr2);

	os_thread_wait(thr1);
}

/******************************************************************
Test function for possible convoy problem. */

void 
test2(void)
/*=======*/
{
	os_thread_t		thr1, thr2, thr3, thr4, thr5;
	os_thread_id_t		id1, id2, id3, id4, id5;
	ulint			tm, oldtm;
	ulint			n1, n2, n3, n4, n5;

        printf("-------------------------------------------\n");
	printf("SYNC-TEST 2. Test of possible convoy problem.\n");

	printf("System call count %lu\n", mutex_system_call_count);
	
	mutex_create(&mutex1);
	mutex_create(&mutex2);
	mutex_create(&mutex3);
	mutex_create(&mutex4);

	switch_count = 0;
	
	oldtm = ut_clock();

	n1 = 1;
	
	thr1 = os_thread_create(thread_n,
				  &n1,
				  &id1);

	os_thread_wait(thr1);


	tm = ut_clock();
	printf("Wall clock time for single thread %ld milliseconds\n",
			tm - oldtm);
	printf("System call count %lu\n", mutex_system_call_count);

	switch_count = 0;
			
	oldtm = ut_clock();

	n1 = 1;
	thr1 = os_thread_create(thread_n,
				  &n1,
				  &id1);
	n2 = 2;
	thr2 = os_thread_create(thread_n,
				  &n2,
				  &id2);
	n3 = 3;
	thr3 = os_thread_create(thread_n,
				  &n3,
				  &id3);
	n4 = 4;
	thr4 = os_thread_create(thread_n,
				  &n4,
				  &id4);
	n5 = 5;
	thr5 = os_thread_create(thread_n,
				  &n5,
				  &id5);

				  
	os_thread_wait(thr1);
	os_thread_wait(thr2);
	os_thread_wait(thr3);
	os_thread_wait(thr4);
	os_thread_wait(thr5);


	tm = ut_clock();
	printf("Wall clock time for 5 threads %ld milliseconds\n",
			tm - oldtm);
	printf("%ld thread switches occurred\n", switch_count);
	
	printf("If this is not 5 x single thread time, possibly convoy!\n");

	printf("System call count %lu\n", mutex_system_call_count);
}

/******************************************************************
Test function for possible exclusion failure. */

void 
test3(void)
/*=======*/
{
	os_thread_t		thr1, thr2;
	os_thread_id_t		id1, id2;

        printf("-------------------------------------------\n");
	printf("SYNC-TEST 3. Test of possible exclusion failure.\n");

	glob_count = 0;
	glob_inc = 1;
	
	thr1 = os_thread_create(thread_x,
				  NULL,
				  &id1);
	thr2 = os_thread_create(thread_x,
				  NULL,
				  &id2);

	os_thread_wait(thr2);
	os_thread_wait(thr1);

	ut_a(glob_count == 400000 * UNIV_DBC);
}

/******************************************************************
Test function for measuring the spin wait loop cycle time. */

void 
test4(void)
/*=======*/
{
volatile ulint*	ptr;
	ulint	i, tm, oldtm;	

        printf("-------------------------------------------\n");
	printf("SYNC-TEST 4. Test of spin wait loop cycle time.\n");
	printf("Use this time to set the SYNC_SPIN_ROUNDS constant.\n");


	glob_inc = 1;
	
	ptr = &glob_inc;
	
	oldtm = ut_clock();

	i = 0;

	while ((*ptr != 0) && (i < 10000000)) {
		i++;
	}
	
	tm = ut_clock();
	printf("Wall clock time for %ld cycles %ld milliseconds\n",
			i, tm - oldtm);
}

/********************************************************************
Start function for s-lock thread in test5. */
ulint
thread_srw(void* arg)
/*==============*/
{
	ulint	i, j;
	void*	arg2;

	arg2 = arg;

	printf("Thread_srw started!\n");

	rw_lock_s_lock(&rw1);

	printf("Thread_srw has now s-lock!\n");

	j = 0;
	
	for (i = 1; i < 1000000; i++) {
		j += i;
	}

	printf("Thread_srw releases now the s-lock!\n");
	
	rw_lock_s_unlock(&rw1);

	return(j);
}

/********************************************************************
Start function for x-lock thread in test5. */
ulint
thread_xrw(void* arg)
/*==============*/
{
	ulint	i, j;
	void*	arg2;

	arg2 = arg;

	printf("Thread_xrw started!\n");

	rw_lock_x_lock(&rw1);

	printf("Thread_xrw has now x-lock!\n");

	j = 0;
	
	for (i = 1; i < 1000000; i++) {
		j += i;
	}

	printf("Thread_xrw releases now the x-lock!\n");
	
	rw_lock_x_unlock(&rw1);

	return(j);
}


void 
test5(void)
/*=======*/
{
	os_thread_t		thr1, thr2;
	os_thread_id_t		id1, id2;
	ulint			i, j;
	ulint			tm, oldtm;
	
        printf("-------------------------------------------\n");
	printf("SYNC-TEST 5. Test of read-write locks.\n");


	printf("Main thread %ld starts!\n",
		os_thread_get_curr_id());

	
	rw_lock_create(&rw1);

	oldtm = ut_clock();


	for (i = 0; i < 10000 * UNIV_DBC * UNIV_DBC; i++) {

		rw_lock_s_lock(&rw1);

		rw_lock_s_unlock(&rw1);

	}

	tm = ut_clock();
	printf("Wall clock time for %ld rw s-lock-unlock %ld milliseconds\n",
			i, tm - oldtm);


	oldtm = ut_clock();


	for (i = 0; i < 10000 * UNIV_DBC * UNIV_DBC; i++) {

		mutex_enter(&mutex);
		rc++;
		mutex_exit(&mutex);

		mutex_enter(&mutex);
		rc--;
		mutex_exit(&mutex);
	}

	tm = ut_clock();
	printf("Wall clock time for %ld rw test %ld milliseconds\n",
			i, tm - oldtm);



	oldtm = ut_clock();

	for (i = 0; i < 10000 * UNIV_DBC * UNIV_DBC; i++) {

		rw_lock_x_lock(&rw1);
		rw_lock_x_unlock(&rw1);

	}

	tm = ut_clock();
	printf("Wall clock time for %ld rw x-lock-unlock %ld milliseconds\n",
			i, tm - oldtm);


	/* Test recursive x-locking */
	for (i = 0; i < 10000; i++) {
		rw_lock_x_lock(&rw1);
	}

	for (i = 0; i < 10000; i++) {

		rw_lock_x_unlock(&rw1);
	}

	/* Test recursive s-locking */
	for (i = 0; i < 10000; i++) {

		rw_lock_s_lock(&rw1);
	}

	for (i = 0; i < 10000; i++) {

		rw_lock_s_unlock(&rw1);
	}

	rw_lock_s_lock(&rw1);

	ut_ad(1 == rw_lock_n_locked());

	mem_print_info();

	rw_lock_list_print_info();

	thr2 = os_thread_create(thread_xrw,
				  NULL,
				  &id2);

	printf("Thread_xrw created, id %ld \n", id2);
	

	thr1 = os_thread_create(thread_srw,
				  NULL,
				  &id1);

	printf("Thread_srw created, id %ld \n", id1);

	j = 0;
	
	for (i = 1; i < 10000000; i++) {
		j += i;
	}

	rw_lock_list_print_info();

	sync_array_validate(sync_primary_wait_array);
	
	printf("Main thread releases now rw-lock!\n");

	rw_lock_s_unlock(&rw1);

	os_thread_wait(thr2);

	os_thread_wait(thr1);

	sync_array_print_info(sync_primary_wait_array);
}

/********************************************************************
Start function for the competing s-threads in test6. The function tests
the behavior lock-coupling through 4 rw-locks. */

ulint
thread_qs(volatile void* arg)
/*========================*/
{
	ulint	i, j, k, n;

	arg = arg;
	
	n = os_thread_get_curr_id();

	printf("S-Thread %ld started, thread id %lu\n", n,
		os_thread_get_curr_id());

	for (k = 0; k < 1000 * UNIV_DBC; k++) {

		if (qprint)
		printf("S-Thread %ld starts round %ld!\n", n, k);
			
		rw_lock_s_lock(&rw1);

		if (qprint)	
		printf("S-Thread %ld got lock 1 on round %ld!\n", n, k);
		 
		
		if (last_thr != n) {
			switch_count++;
			last_thr = n;
		}
		
		j = 0;
	
		for (i = 1; i < 400; i++) {
			j += i;
		}
	
		rw_lock_s_lock(&rw2);

		if (qprint)	
		printf("S-Thread %ld got lock 2 on round %ld!\n", n, k);
		 

		rw_lock_s_unlock(&rw1);

		if (qprint)	
		printf("S-Thread %ld released lock 1 on round %ld!\n", n, k);
		 

		for (i = 1; i < 400; i++) {
			j += i;
		}
		rw_lock_s_lock(&rw3);

		if (qprint)	
		printf("S-Thread %ld got lock 3 on round %ld!\n", n, k);
		 

		rw_lock_s_unlock(&rw2);
		if (qprint)	
		printf("S-Thread %ld released lock 2 on round %ld!\n", n, k);
		 

		for (i = 1; i < 400; i++) {
			j += i;
		}
		rw_lock_s_lock(&rw4);

		if (qprint)	
		printf("S-Thread %ld got lock 4 on round %ld!\n", n, k);
		 

		rw_lock_s_unlock(&rw3);
		if (qprint)	
		printf("S-Thread %ld released lock 3 on round %ld!\n", n, k);
		 

		for (i = 1; i < 400; i++) {
			j += i;
		}

		rw_lock_s_unlock(&rw4);
		if (qprint)	
		printf("S-Thread %ld released lock 4 on round %ld!\n", n, k);
		 
	}

	printf("S-Thread %ld exits!\n", n);
	
	return(j);
}

/********************************************************************
Start function for the competing x-threads in test6. The function tests
the behavior lock-coupling through 4 rw-locks. */

ulint
thread_qx(volatile void* arg)
/*========================*/
{
	ulint	i, j, k, n;

	arg = arg;

	n = os_thread_get_curr_id();

	printf("X-Thread %ld started, thread id %lu\n", n,
		os_thread_get_curr_id());

	for (k = 0; k < 1000 * UNIV_DBC; k++) {
		
		if (qprint)	
		printf("X-Thread %ld round %ld!\n", n, k);
		 
			
		rw_lock_x_lock(&rw1);
		if (qprint)	
		printf("X-Thread %ld got lock 1 on round %ld!\n", n, k);
		 

		if (last_thr != n) {
			switch_count++;
			last_thr = n;
		}
		
		j = 0;
	
		for (i = 1; i < 400; i++) {
			j += i;
		}
	
		rw_lock_x_lock(&rw2);
		if (qprint)	
		printf("X-Thread %ld got lock 2 on round %ld!\n", n, k);
		 

		rw_lock_x_unlock(&rw1);
		if (qprint)	
		printf("X-Thread %ld released lock 1 on round %ld!\n", n, k);
		 

		for (i = 1; i < 400; i++) {
			j += i;
		}
		rw_lock_x_lock(&rw3);
		if (qprint)	
		printf("X-Thread %ld got lock 3 on round %ld!\n", n, k);
		 

		rw_lock_x_unlock(&rw2);
		if (qprint)	
		printf("X-Thread %ld released lock 2 on round %ld!\n", n, k);
		 

		for (i = 1; i < 400; i++) {
			j += i;
		}
		rw_lock_x_lock(&rw4);
		if (qprint)	
		printf("X-Thread %ld got lock 4 on round %ld!\n", n, k);

		rw_lock_x_unlock(&rw3);
		if (qprint)	
		printf("X-Thread %ld released lock 3 on round %ld!\n", n, k);
		 

		for (i = 1; i < 400; i++) {
			j += i;
		}

		rw_lock_x_unlock(&rw4);
		if (qprint)	
		printf("X-Thread %ld released lock 4 on round %ld!\n", n, k);
		 
	}

	printf("X-Thread %ld exits!\n", n);
	
	return(j);
}

/******************************************************************
Test function for possible queuing problems with rw-locks. */

void 
test6(void)
/*=======*/
{
	os_thread_t		thr1, thr2, thr3, thr4, thr5;
	os_thread_id_t		id1, id2, id3, id4, id5;
	ulint			tm, oldtm;
	ulint			n1, n2, n3, n4, n5;

        printf("-------------------------------------------\n");
	printf(
	"SYNC-TEST 6. Test of possible queuing problems with rw-locks.\n");
/*
	sync_array_print_info(sync_primary_wait_array);
*/

	rw_lock_create(&rw2);
	rw_lock_create(&rw3);
	rw_lock_create(&rw4);

	switch_count = 0;
	

	oldtm = ut_clock();

	n1 = 1;
	
	thr1 = os_thread_create(thread_qs,
				  &n1,
				  &id1);

	os_thread_wait(thr1);


	tm = ut_clock();
	printf("Wall clock time for single s-lock thread %ld milliseconds\n",
			tm - oldtm);

	oldtm = ut_clock();

	n1 = 1;
	
	thr1 = os_thread_create(thread_qx,
				  &n1,
				  &id1);

	os_thread_wait(thr1);


	tm = ut_clock();
	printf("Wall clock time for single x-lock thread %ld milliseconds\n",
			tm - oldtm);

	switch_count = 0;
			
	oldtm = ut_clock();

	
	n1 = 1;
	thr1 = os_thread_create(thread_qx,
				  &n1,
				  &id1);

	n2 = 2;
	thr2 = os_thread_create(thread_qs,
				  &n2,
				  &id2);

	n3 = 3;
	thr3 = os_thread_create(thread_qx,
				  &n3,
				  &id3);


	n4 = 4;
	thr4 = os_thread_create(thread_qs,
				  &n4,
				  &id4);
				  
	n5 = 5;
	thr5 = os_thread_create(thread_qx,
				  &n5,
				  &id5);

	os_thread_wait(thr1);

	os_thread_wait(thr2);

	os_thread_wait(thr3);

	os_thread_wait(thr4);

	os_thread_wait(thr5);


	tm = ut_clock();
	printf("Wall clock time for 5 threads %ld milliseconds\n",
			tm - oldtm);
	printf("at least %ld thread switches occurred\n", switch_count);
	
	printf(
	"If this is not 2 x s-thread + 3 x x-thread time, possibly convoy!\n");

	rw_lock_list_print_info();

	sync_array_print_info(sync_primary_wait_array);

}

/********************************************************************
Start function for thread in test7. */
ulint
ip_thread(void* arg)
/*================*/
{
	ulint	i, j;
	void*	arg2;
	ulint	ret;
	ulint	tm, oldtm;
	
	arg2 = arg;

	printf("Thread started!\n");

	oldtm = ut_clock();

	ret = ip_mutex_enter(iph, 100000);

/*	ut_a(ret == SYNC_TIME_EXCEEDED);
*/	
	tm = ut_clock();

	printf("Wall clock time for wait failure %ld ms\n", tm - oldtm);

	ret = ip_mutex_enter(iph, SYNC_INFINITE_TIME);

	ut_a(ret == 0);
	
	printf("Thread owns now the ip mutex!\n");

	j = 0;
	
	for (i = 1; i < 1000000; i++) {
		j += i;
	}

	printf("Thread releases now the ip mutex!\n");
	
	ip_mutex_exit(iph);

	return(j);
}

/*********************************************************************
Test for interprocess mutex. */
void 
test7(void)
/*=======*/
{
	os_thread_t		thr1;
	os_thread_id_t		id1;
	ulint			i, j;
	ulint			tm, oldtm;
	
        printf("-------------------------------------------\n");
	printf("SYNC-TEST 7. Test of ip mutex.\n");


	printf("Main thread %ld starts!\n",
		os_thread_get_curr_id());

	ip_mutex_create(&ip_mutex, "IPMUTEX", &iph);
	
	oldtm = ut_clock();

	for (i = 0; i < 100000 * UNIV_DBC; i++) {

		ip_mutex_enter(iph, SYNC_INFINITE_TIME);
		ip_mutex_exit(iph);
	}

	tm = ut_clock();
	printf("Wall clock time for %ld ip mutex lock-unlock %ld ms\n",
			i, tm - oldtm);


	ip_mutex_enter(iph, SYNC_INFINITE_TIME);

	thr1 = os_thread_create(ip_thread,
				  NULL,
				  &id1);

	printf("Thread created, id %ld \n", id1);


	j = 0;
	
	for (i = 1; i < 100000000; i++) {
		j += i;
	}

	printf("Main thread releases now ip mutex!\n");

	ip_mutex_exit(iph);

	os_thread_wait(thr1);

	ip_mutex_free(iph);
}

/********************************************************************
Start function for the competing threads in test8. The function tests
the behavior lock-coupling through 4 ip mutexes. */

ulint
thread_ipn(volatile void* arg)
/*========================*/
{
	ulint	i, j, k, n;

	n = *((ulint*)arg);

	printf("Thread %ld started!\n", n);

	for (k = 0; k < 2000 * UNIV_DBC; k++) {
	
		ip_mutex_enter(iph1, SYNC_INFINITE_TIME);

		if (last_thr != n) {
			switch_count++;
			last_thr = n;
		}
		
		j = 0;
	
		for (i = 1; i < 400; i++) {
			j += i;
		}
	
		ip_mutex_enter(iph2, SYNC_INFINITE_TIME);

		ip_mutex_exit(iph1);

		for (i = 1; i < 400; i++) {
			j += i;
		}
		ip_mutex_enter(iph3, SYNC_INFINITE_TIME);

		ip_mutex_exit(iph2);

		for (i = 1; i < 400; i++) {
			j += i;
		}
		ip_mutex_enter(iph4, SYNC_INFINITE_TIME);

		ip_mutex_exit(iph3);

		for (i = 1; i < 400; i++) {
			j += i;
		}

		ip_mutex_exit(iph4);
	}

	printf("Thread %ld exits!\n", n);
	
	return(j);
}

/******************************************************************
Test function for ip mutex. */

void 
test8(void)
/*=======*/
{
	os_thread_t		thr1, thr2, thr3, thr4, thr5;
	os_thread_id_t		id1, id2, id3, id4, id5;
	ulint			tm, oldtm;
	ulint			n1, n2, n3, n4, n5;

        printf("-------------------------------------------\n");
	printf("SYNC-TEST 8. Test for ip mutex.\n");


	ip_mutex_create(&ip_mutex1, "jhfhk", &iph1);
	ip_mutex_create(&ip_mutex2, "jggfg", &iph2);
	ip_mutex_create(&ip_mutex3, "hfdx", &iph3);
	ip_mutex_create(&ip_mutex4, "kjghg", &iph4);

	switch_count = 0;
	
	oldtm = ut_clock();

	n1 = 1;
	
	thr1 = os_thread_create(thread_ipn,
				  &n1,
				  &id1);

	os_thread_wait(thr1);


	tm = ut_clock();
	printf("Wall clock time for single thread %lu milliseconds\n",
			tm - oldtm);

	switch_count = 0;
			
	oldtm = ut_clock();

	n1 = 1;
	thr1 = os_thread_create(thread_ipn,
				  &n1,
				  &id1);
	n2 = 2;
	thr2 = os_thread_create(thread_ipn,
				  &n2,
				  &id2);
	n3 = 3;
	thr3 = os_thread_create(thread_ipn,
				  &n3,
				  &id3);
	n4 = 4;
	thr4 = os_thread_create(thread_ipn,
				  &n4,
				  &id4);
	n5 = 5;
	thr5 = os_thread_create(thread_ipn,
				  &n5,
				  &id5);

	os_thread_wait(thr1);
	os_thread_wait(thr2);
	os_thread_wait(thr3);
	os_thread_wait(thr4);
	os_thread_wait(thr5);


	tm = ut_clock();
	printf("Wall clock time for 5 threads %ld milliseconds\n",
			tm - oldtm);
	printf("%ld thread switches occurred\n", switch_count);
	
	printf("If this is not 5 x single thread time, possibly convoy!\n");

	ip_mutex_free(iph1);
	ip_mutex_free(iph2);
	ip_mutex_free(iph3);
	ip_mutex_free(iph4);
}


/********************************************************************
Start function for s-lock thread in test9. */
ulint
thread_srw9(void* arg)
/*==================*/
{
	void*	arg2;

	arg2 = arg;

	printf("Thread_srw9 started!\n");

	rw_lock_x_lock(&rw10);

	printf("Thread_srw9 has now x-lock on rw10, wait for mutex!\n");

	mutex_enter(&mutex9);

	return(0);
}

/********************************************************************
Start function for x-lock thread in test9. */
ulint
thread_xrw9(void* arg)
/*==================*/
{
	void*	arg2;

	arg2 = arg;

	printf("Thread_xrw started!\n");

	mutex_enter(&mutex9);
	printf("Thread_xrw9 has now mutex9, wait for rw9!\n");
	
	rw_lock_x_lock(&rw9);

	return(0);
}

void 
test9(void)
/*=======*/
{
	os_thread_t		thr1, thr2;
	os_thread_id_t		id1, id2;
	
        printf("-------------------------------------------\n");
	printf("SYNC-TEST 9. Test of deadlock detection.\n");


	printf("Main thread %ld starts!\n",
		os_thread_get_curr_id());

	rw_lock_create(&rw9);
	rw_lock_create(&rw10);
	mutex_create(&mutex9);

	rw_lock_s_lock(&rw9);
	printf("Main thread has now s-lock on rw9\n");
		
	thr2 = os_thread_create(thread_xrw9,
				  NULL,
				  &id2);

	printf("Thread_xrw9 created, id %ld \n", id2);

	os_thread_sleep(1000000);

	thr1 = os_thread_create(thread_srw9,
				  NULL,
				  &id1);

	printf("Thread_srw9 created, id %ld \n", id1);

	os_thread_sleep(1000000);

	sync_array_print_info(sync_primary_wait_array);

	printf("Now we should have a deadlock of 3 threads:\n");

	rw_lock_s_lock(&rw10);
}

void 
test10(void)
/*=======*/
{
        printf("-------------------------------------------\n");
	printf("SYNC-TEST 10. Test of deadlock detection on self-deadlock.\n");


	printf("Main thread %ld starts!\n",
		os_thread_get_curr_id());

	mutex_create(&mutex9);

	printf("Now we should have a deadlock of this thread on mutex:\n");

	mutex_enter(&mutex9);
	mutex_enter(&mutex9);
}

void 
test11(void)
/*=======*/
{
        printf("-------------------------------------------\n");
	printf("SYNC-TEST 11. Test of deadlock detection on self-deadlock.\n");


	printf("Main thread %ld starts!\n",
		os_thread_get_curr_id());

	rw_lock_create(&rw9);

	printf("Now we should have a deadlock of this thread on X-lock:\n");

	rw_lock_x_lock(&rw9);
	rw_lock_s_lock_gen(&rw9, 567);
}


/************************************************************************
Main test function. */

void 
main(void) 
/*======*/
{
	ulint	tm, oldtm;

	sync_init();	
	mem_init();
	
	oldtm = ut_clock();

	test1();

	test2();
	
	test3();

	test4();

	test5();
	
	test6();

	test7();

	test8();	

	/* This test SHOULD result in assert on deadlock! */
/*	test9();*/

	/* This test SHOULD result in assert on deadlock! */
/*	test10();*/

	/* This test SHOULD result in assert on deadlock! */
/*	test11();*/

	ut_ad(0 == mutex_n_reserved());
	ut_ad(0 == rw_lock_n_locked());
	ut_ad(sync_all_freed());

	
	ut_ad(mem_all_freed());
	
	sync_close();
	
	tm = ut_clock();
	printf("Wall clock time for test %ld milliseconds\n", tm - oldtm);
	printf("System call count %lu\n", mutex_system_call_count);
	printf("TESTS COMPLETED SUCCESSFULLY!\n");
} 


