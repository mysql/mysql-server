/************************************************************************
The test module for the file system

(c) 1995 Innobase Oy

Created 10/29/1995 Heikki Tuuri
*************************************************************************/

#include "os0thread.h"
#include "os0file.h"
#include "ut0ut.h"
#include "sync0sync.h"
#include "mem0mem.h"
#include "..\fil0fil.h"

ulint	last_thr = 1;

byte	global_buf[10000000];
byte	global_buf2[20000];

os_file_t	files[1000];

os_event_t	gl_ready;

mutex_t		ios_mutex;
ulint		ios;

/*********************************************************************
Test for synchronous file io. */

void 
test1(void)
/*=======*/
{
	ulint			i, j;
	void*			mess;
	bool			ret;
	void*			buf;
	ulint			rnd, rnd3;
	ulint			tm, oldtm;
	
        printf("-------------------------------------------\n");
	printf("FIL-TEST 1. Test of synchronous file io\n");

	/* Align the buffer for file io */

	buf = (void*)(((ulint)global_buf + 6300) & (~0xFFF)); 

	rnd = ut_time();
	rnd3 = ut_time();

	rnd = rnd * 3416133;
	rnd3 = rnd3 * 6576681;

	oldtm = ut_clock();

	for (j = 0; j < 300; j++) {
	   for (i = 0; i < (rnd3 % 15); i++) {
		fil_read((rnd % 1000) / 100, rnd % 100, 0, 8192, buf, NULL);

		ut_a(fil_validate());

		ret = fil_aio_wait(0, &mess);
		ut_a(ret);

		ut_a(fil_validate());

		ut_a(*((ulint*)buf) == rnd % 1000);
		
		rnd += 1;
	   }
           rnd = rnd + 3416133;
	   rnd3 = rnd3 + 6576681;
	}
	   
	tm = ut_clock();
	printf("Wall clock time for synchr. io %lu milliseconds\n",
			tm - oldtm);

}

/************************************************************************
Io-handler thread function. */

ulint
handler_thread(
/*===========*/
	void*	arg)
{
	ulint	segment;
	void*	mess;
	void*	buf;
	ulint	i;
	bool	ret;
	
	segment = *((ulint*)arg);

	buf = (void*)(((ulint)global_buf + 6300) & (~0xFFF)); 

	printf("Thread %lu starts\n", segment);

	for (i = 0;; i++) {
		ret = fil_aio_wait(segment, &mess);
		ut_a(ret);

		if ((ulint)mess == 3333) {
			os_event_set(gl_ready);
		} else {
		     ut_a((ulint)mess ==
			*((ulint*)((byte*)buf + 8192 * (ulint)mess)));
		}
		
		mutex_enter(&ios_mutex);
		ios++;
		mutex_exit(&ios_mutex);
		
		ut_a(ret);
/*		printf("Message for thread %lu %lu\n", segment,
						(ulint)mess); */
	}

	return(0);
}

/************************************************************************
Test of io-handler threads */

void 
test2(void)
/*=======*/
{
	ulint			i;
	ulint			j;
	void*			buf;
	ulint			rnd, rnd3;
	ulint			tm, oldtm;
	os_thread_t		thr[5];
	os_thread_id_t		id[5];
	ulint			n[5];
	
	/* Align the buffer for file io */

	buf = (void*)(((ulint)global_buf + 6300) & (~0xFFF)); 

	gl_ready = os_event_create(NULL);
	ios = 0;

	mutex_create(&ios_mutex);
	
	for (i = 0; i < 5; i++) {
		n[i] = i;

		thr[i] = os_thread_create(handler_thread, n + i, id + i);
	}
	
        printf("-------------------------------------------\n");
	printf("FIL-TEST 2. Test of asynchronous file io\n");

	rnd = ut_time();
	rnd3 = ut_time();

	rnd = rnd * 3416133;
	rnd3 = rnd3 * 6576681;

	oldtm = ut_clock();

	for (j = 0; j < 300; j++) {
	   for (i = 0; i < (rnd3 % 15); i++) {
		fil_read((rnd % 1000) / 100, rnd % 100, 0, 8192,
				(void*)((byte*)buf + 8192 * (rnd % 1000)),
				(void*)(rnd % 1000));

		rnd += 1;
	   }
	   ut_a(fil_validate());
           rnd = rnd + 3416133;
	   rnd3 = rnd3 + 6576681;
	}

	ut_a(!os_aio_all_slots_free());	   

	tm = ut_clock();
	printf("Wall clock time for asynchr. io %lu milliseconds\n",
			tm - oldtm);

	fil_read(5, 25, 0, 8192,
		(void*)((byte*)buf + 8192 * 1000),
		(void*)3333);

	tm = ut_clock();

	ut_a(fil_validate());

	printf("All ios queued! N ios: %lu\n", ios);

	printf("Wall clock time for test %lu milliseconds\n", tm - oldtm);
	
	os_event_wait(gl_ready);

	tm = ut_clock();
	printf("N ios: %lu\n", ios);
	printf("Wall clock time for test %lu milliseconds\n", tm - oldtm);

	os_thread_sleep(2000000);

	printf("N ios: %lu\n", ios);

	ut_a(fil_validate());
	ut_a(os_aio_all_slots_free());
}

/*************************************************************************
Creates the files for the file system test and inserts them to
the file system. */

void
create_files(void)
/*==============*/
{
	bool	ret;
	ulint	i, j, k, n;
	void*	buf;
	void*	mess;
	char	name[10];
	
	buf = (void*)(((ulint)global_buf2 + 6300) & (~0xFFF)); 

	name[0] = 't';
	name[1] = 's';
	name[2] = 'f';
	name[3] = 'i';
	name[4] = 'l';
	name[5] = 'e';
	name[8] = '\0';

	for (k = 0; k < 10; k++) {
	for (i = 0; i < 20; i++) {

		name[6] = (char)(k + (ulint)'a');
		name[7] = (char)(i + (ulint)'a');
	
		files[i] = os_file_create(name, OS_FILE_CREATE,
					OS_FILE_TABLESPACE, &ret);

		if (ret == FALSE) {
			ut_a(os_file_get_last_error() ==
						OS_FILE_ALREADY_EXISTS);
	
			files[i] = os_file_create(name, OS_FILE_OPEN,
						OS_FILE_TABLESPACE, &ret);

			ut_a(ret);
		} else {
		
			for (j = 0; j < 5; j++) {
				for (n = 0; n < 8192 / sizeof(ulint); n++) {
					*((ulint*)buf + n) =
						k * 100 + i * 5 + j;
				}
				
				ret = os_aio_write(files[i], buf, 8192 * j,
							0, 8192, NULL);
				ut_a(ret);

				ret = os_aio_wait(0, &mess);

				ut_a(ret);
				ut_a(mess == NULL);
			}
		}
		
		ret = os_file_close(files[i]);
		ut_a(ret);

		if (i == 0) {
			fil_space_create("noname", k, OS_FILE_TABLESPACE);
		}

		ut_a(fil_validate());

		fil_node_create(name, 5, k);
	}
	}
}

/************************************************************************
Frees the spaces in the file system. */

void
free_system(void)
/*=============*/
{
	ulint	i;

	for (i = 0; i < 10; i++) {
		fil_space_free(i);
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

	os_aio_init(160, 5);
	sync_init();
	mem_init();
	fil_init(2);	/* Allow only 2 open files at a time */

	ut_a(fil_validate());
	
	create_files();
	
	test1();

	test2();

	free_system();	
	
	tm = ut_clock();
	printf("Wall clock time for test %lu milliseconds\n", tm - oldtm);
	printf("TESTS COMPLETED SUCCESSFULLY!\n");
} 
