/************************************************************************
The test module for the thread management of Innobase

(c) 1995 Innobase Oy

Created 10/5/1995 Heikki Tuuri
*************************************************************************/

#include "../thr0loc.h"
#include "sync0sync.h"
#include "mem0mem.h"
#include "os0thread.h"
#include "os0sync.h"
#include "ut0ut.h"

ulint		val	= 500;
os_event_t	event;

/******************************************************************
Thread start function in test1. */

ulint
thread1(
/*====*/
	void*	arg)
{
	ulint	n;

	thr_local_create();
	
	n = *((ulint*)arg);

	printf("Thread %lu starts\n", n);

	thr_local_set_slot_no(os_thread_get_curr_id(), n);

	ut_a(n == thr_local_get_slot_no(os_thread_get_curr_id()));

	os_event_wait(event);

	thr_local_free();

	os_thread_exit(0);

	return(0);
}

/******************************************************************
Test function for local storage. */

void 
test1(void)
/*=======*/
{
	os_thread_t		thr1, thr2, thr3, thr4, thr5;
	os_thread_id_t		id1, id2, id3, id4, id5;
	ulint			tm, oldtm;
	ulint			n1, n2, n3, n4, n5;

        printf("-------------------------------------------\n");
	printf("THR-TEST 1. Test of local storage\n");

	event = os_event_create(NULL);
	
	oldtm = ut_clock();

	n1 = 1;
	thr1 = os_thread_create(thread1,
				  &n1,
				  &id1);
	n2 = 2;
	thr2 = os_thread_create(thread1,
				  &n2,
				  &id2);
	n3 = 3;
	thr3 = os_thread_create(thread1,
				  &n3,
				  &id3);
	n4 = 4;
	thr4 = os_thread_create(thread1,
				  &n4,
				  &id4);
	n5 = 5;
	thr5 = os_thread_create(thread1,
				  &n5,
				  &id5);

	os_thread_sleep(500000);

	ut_a(n1 == thr_local_get_slot_no(id1));
	ut_a(n2 == thr_local_get_slot_no(id2));
	ut_a(n3 == thr_local_get_slot_no(id3));
	ut_a(n4 == thr_local_get_slot_no(id4));
	ut_a(n5 == thr_local_get_slot_no(id5));

	os_event_set(event);

	os_thread_wait(thr1);
	os_thread_wait(thr2);
	os_thread_wait(thr3);
	os_thread_wait(thr4);
	os_thread_wait(thr5);

	tm = ut_clock();
	printf("Wall clock time for 5 threads %ld milliseconds\n",
			tm - oldtm);
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
	thr_local_init();

	oldtm = ut_clock();

	test1();

	thr_local_close();
	
	tm = ut_clock();
	printf("Wall clock time for test %lu milliseconds\n", tm - oldtm);
	printf("TESTS COMPLETED SUCCESSFULLY!\n");
} 
