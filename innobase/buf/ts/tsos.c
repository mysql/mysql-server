/************************************************************************
The test module for the operating system interface

(c) 1995 Innobase Oy

Created 9/27/1995 Heikki Tuuri
*************************************************************************/


#include "../os0thread.h"
#include "../os0shm.h"
#include "../os0proc.h"
#include "../os0sync.h"
#include "../os0file.h"
#include "ut0ut.h"
#include "sync0sync.h"
#include "mem0mem.h"

ulint	last_thr = 1;

byte	global_buf[1000000];

os_file_t	file;
os_file_t	file2;

os_event_t	gl_ready;

mutex_t		ios_mutex;
ulint		ios;

/************************************************************************
Io-handler thread function. */

ulint
handler_thread(
/*===========*/
	void*	arg)
{
	ulint	segment;
	void*	mess;
	ulint	i;
	bool	ret;
	
	segment = *((ulint*)arg);

	printf("Thread %lu starts\n", segment);

	for (i = 0;; i++) {
		ret = os_aio_wait(segment, &mess);

		mutex_enter(&ios_mutex);
		ios++;
		mutex_exit(&ios_mutex);
		
		ut_a(ret);
/*		printf("Message for thread %lu %lu\n", segment,
						(ulint)mess); */
		if ((ulint)mess == 3333) {
			os_event_set(gl_ready);
		}
	}

	return(0);
}

/************************************************************************
Test of io-handler threads */

void 
test4(void)
/*=======*/
{
	ulint			i;
	bool			ret;
	void*			buf;
	ulint			rnd;
	ulint			tm, oldtm;

	os_thread_t		thr[5];
	os_thread_id_t		id[5];
	ulint			n[5];
	
        printf("-------------------------------------------\n");
	printf("OS-TEST 4. Test of asynchronous file io\n");

	/* Align the buffer for file io */

	buf = (void*)(((ulint)global_buf + 6300) & (~0xFFF)); 

	gl_ready = os_event_create(NULL);
	ios = 0;

	sync_init();
	mem_init();

	mutex_create(&ios_mutex);
	
	for (i = 0; i < 5; i++) {
		n[i] = i;

		thr[i] = os_thread_create(handler_thread, n + i, id + i);
	}

	rnd = 0;
	
	oldtm = ut_clock();

	for (i = 0; i < 4096; i++) {
		ret = os_aio_read(file, (byte*)buf + 8192 * (rnd % 100),
				8192 * (rnd % 4096), 0,
							8192, (void*)i);
		ut_a(ret);
		rnd += 1;
	}

	ret = os_aio_read(file, buf, 8192 * (rnd % 1024), 0, 8192,
							(void*)3333);
	ut_a(ret);

	ut_a(!os_aio_all_slots_free());

	tm = ut_clock();

	printf("All ios queued! N ios: %lu\n", ios);

	printf("Wall clock time for test %lu milliseconds\n", tm - oldtm);
	
	os_event_wait(gl_ready);

	tm = ut_clock();
	printf("N ios: %lu\n", ios);
	printf("Wall clock time for test %lu milliseconds\n", tm - oldtm);

	os_thread_sleep(2000000);

	printf("N ios: %lu\n", ios);

	ut_a(os_aio_all_slots_free());
}

/*************************************************************************
Initializes the asyncronous io system for tests. */

void
init_aio(void)
/*==========*/
{
	bool	ret;
	void*	buf;

	buf = (void*)(((ulint)global_buf + 6300) & (~0xFFF)); 

	os_aio_init(160, 5);
	file = os_file_create("j:\\tsfile4", OS_FILE_CREATE, OS_FILE_TABLESPACE,
				&ret);

	if (ret == FALSE) {
		ut_a(os_file_get_last_error() == OS_FILE_ALREADY_EXISTS);
	
		file = os_file_create("j:\\tsfile4", OS_FILE_OPEN,
			OS_FILE_TABLESPACE, &ret);

		ut_a(ret);
	}
}

/************************************************************************
Main test function. */

void 
main(void) 
/*======*/
{
	ulint	tm, oldtm;

	oldtm = ut_clock();

	init_aio();

	test4();

	tm = ut_clock();
	printf("Wall clock time for test %lu milliseconds\n", tm - oldtm);
	printf("TESTS COMPLETED SUCCESSFULLY!\n");
} 
