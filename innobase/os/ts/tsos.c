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
#include "ut0mem.h"
#include "sync0sync.h"
#include "mem0mem.h"

#define _WIN32_WINNT	0x0400
#include "n:\program files\devstudio\vc\include\windows.h"
#include "n:\program files\devstudio\vc\include\winbase.h"

ulint	last_thr = 1;

byte	global_buf[4000000];

ulint*	cache_buf;

os_file_t	file;
os_file_t	file2;

os_event_t	gl_ready;

mutex_t		ios_mutex;
ulint		ios;
ulint		rnd	= 9837497;

/********************************************************************
Start function for threads in test1. */

ulint
thread(void* arg)
/*==============*/
{
	ulint	i;
	void*	arg2;
	ulint	count = 0;
	ulint	n;
	ulint	rnd_loc;
	byte	local_buf[2000];
	
	arg2 = arg;

	n = *((ulint*)arg);

/*	printf("Thread %lu started!\n", n); */
	
	for (i = 0; i < 8000; i++) {

		rnd_loc = rnd;
		rnd += 763482469;	
		
		ut_memcpy(global_buf + (rnd_loc % 1500000) + 8200, local_buf,
									2000);
		if (last_thr != n) {
			count++;
			last_thr = n;
		}

		if (i % 32 == 0) {
			os_thread_yield();
		}
	}

	printf("Thread %lu exits: %lu thread switches noticed\n", n, count);

	return(0);
}

/*********************************************************************
Test of the speed of wait for multiple events. */

void
testa1(void)
/*========*/
{
	ulint		i;
	os_event_t	arr[64];
	ulint		tm, oldtm;

	printf("-------------------------------------------------\n");
	printf("TEST A1. Speed of waits\n");
	
	for (i = 0; i < 64; i++) {
		arr[i] = os_event_create(NULL);
		ut_a(arr[i]);
	}

	os_event_set(arr[1]);
	
	oldtm = ut_clock();

	for (i = 0; i < 10000; i++) {
		os_event_wait_multiple(4, arr);
	}

	tm = ut_clock();

	printf("Wall clock time for %lu multiple waits %lu millisecs\n",
		i, tm - oldtm);

	oldtm = ut_clock();

	for (i = 0; i < 10000; i++) {
		os_event_wait(arr[1]);
	}

	tm = ut_clock();

	printf("Wall clock time for %lu single waits %lu millisecs\n",
		i, tm - oldtm);

	
	for (i = 0; i < 64; i++) {
		os_event_free(arr[i]);
	}
}
	
/*********************************************************************
Test for threads. */

void 
test1(void)
/*=======*/
{
	os_thread_t	thr[64];
	os_thread_id_t	id[64];
	ulint		n[64];
	ulint		tm, oldtm;
	ulint		i, j;
	
        printf("-------------------------------------------\n");
	printf("OS-TEST 1. Test of thread switching through yield\n");

	printf("Main thread %lu starts!\n", os_thread_get_curr_id());

    for (j = 0; j < 2; j++) {

	oldtm = ut_clock();

	for (i = 0; i < 64; i++) {
		n[i] = i;
	
		thr[i] = os_thread_create(thread, n + i, id + i);
/*		printf("Thread %lu created, id %lu \n", i, id[i]); */
	}

	for (i = 0; i < 64; i++) {
		os_thread_wait(thr[i]);
	}

	tm = ut_clock();
	printf("Wall clock time for test %lu milliseconds\n", tm - oldtm);

	oldtm = ut_clock();

	for (i = 0; i < 64; i++) {

		thr[5] = os_thread_create(thread, n + 5, id + 5);

/*		printf("Thread created, id %lu \n", id[5]); */

		os_thread_wait(thr[5]);
	}

	tm = ut_clock();
	printf("Wall clock time for single thread test %lu milliseconds\n",
								tm - oldtm);
    }
}

/*********************************************************************
Test for shared memory and process switching through yield. */

void 
test2(void)
/*=======*/
{
	os_shm_t	shm;
	ulint		tm, oldtm;
	ulint*		pr_no;
	ulint		count;
	ulint		i;
	bool		ret;
	os_process_t	proc;
	os_process_id_t	proc_id;
	
        printf("-------------------------------------------\n");
	printf("OS-TEST 2. Test of process switching through yield\n");

	shm = os_shm_create(1000, "TSOS_SHM");

	pr_no = os_shm_map(shm);

	*pr_no = 1;
	count = 0;

	ret = os_process_create("tsosaux.exe", NULL, &proc, &proc_id);

	printf("Last error: %lu\n", os_thread_get_last_error());

	ut_a(ret);

	printf("Process 1 starts test!\n");
	
	oldtm = ut_clock();

	for (i = 0; i < 500000; i++) {
		if (*pr_no != 1) {
			count++;
			*pr_no = 1;
		}

		os_thread_yield();
	}

	tm = ut_clock();

	printf("Process 1 finishes test: %lu process switches noticed\n",
		count);
	
	printf("Wall clock time for test %lu milliseconds\n", tm - oldtm);

	os_shm_unmap(shm);

	os_shm_free(shm);
}

#ifdef notdefined

/*********************************************************************
Test for asynchronous file io. */

void 
test3(void)
/*=======*/
{
	ulint	i;
	ulint	j;
	void*	mess;
	bool	ret;
	void*	buf;
	ulint	rnd;
	ulint	addr[64];
	ulint	serv[64];
	ulint	tm, oldtm;
	
        printf("-------------------------------------------\n");
	printf("OS-TEST 3. Test of asynchronous file io\n");

	/* Align the buffer for file io */

	buf = (void*)(((ulint)global_buf + 6300) & (~0xFFF)); 

	rnd = ut_time();

	rnd = rnd * 3416133;

	printf("rnd seed %lu\n", rnd % 4900);

	oldtm = ut_clock();
	
	for (i = 0; i < 32; i++) {
	
		ret = os_aio_read(file, buf, 8192 * (rnd % 4900), 0,
						8192, (void*)i);
		ut_a(ret);
		rnd += 1;
		ret = os_aio_wait(0, &mess);
		ut_a(ret);
	}

	tm = ut_clock();
	printf("Wall clock time for synchr. io %lu milliseconds\n",
			tm - oldtm);

	rnd = rnd * 3416133;

	printf("rnd seed %lu\n", rnd % 5000);

	oldtm = ut_clock();

    for (j = 0; j < 5; j++) {

	rnd = rnd + 3416133; 
	
	for (i = 0; i < 16; i++) {
		ret = os_aio_read(file, buf, 8192 * (rnd % 5000), 0, 8192,
					(void*)i);
		addr[i] = rnd % 5000;
		ut_a(ret);
		rnd += 1;
	}

	
	for (i = 0; i < 16; i++) {
		ret = os_aio_read(file, buf, 8192 * (rnd % 5000), 0, 8192,
					(void*)i);
		addr[i] = rnd % 5000;
		ut_a(ret);
		rnd += 1;
	}
	
	rnd = rnd + 3416133; 

	for (i = 0; i < 32; i++) {
		ret = os_aio_wait(0, &mess);
		ut_a(ret);
		ut_a((ulint)mess < 64);
		serv[(ulint)mess] = i;
	}
    }
	tm = ut_clock();
	printf("Wall clock time for aio %lu milliseconds\n", tm - oldtm);

	rnd = rnd * 3416133;

	printf("rnd seed %lu\n", rnd % 4900);

	oldtm = ut_clock();

for (j = 0; j < 5; j++) {
	
	rnd = rnd + 3416133;

	for (i = 0; i < 1; i++) {
		ret = os_aio_read(file, buf, 8192 * (rnd % 4900), 0,
						64 * 8192, (void*)i);
		ut_a(ret);
		rnd += 4;
		ret = os_aio_wait(0, &mess);
		ut_a(ret);
		ut_a((ulint)mess < 64);
	}
}
	tm = ut_clock();
	printf("Wall clock time for synchr. io %lu milliseconds\n",
			tm - oldtm);


/*
	for (i = 0; i < 63; i++) {
		printf("read %lu addr %lu served as %lu\n",
			i, addr[i], serv[i]);		
	}
*/	

	ut_a(ret);
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
	ulint		i;
	ulint		j;
	bool		ret;
	void*		buf;
	ulint		rnd;
	ulint		tm, oldtm;
	os_thread_t	thr[5];
	os_thread_id_t	id[5];
	ulint		n[5];
	
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

for (j = 0; j < 128; j++) {


	for (i = 0; i < 32; i++) {
		ret = os_aio_read(file, (byte*)buf + 8192 * (rnd % 100),
				8192 * (rnd % 4096), 0,
							8192, (void*)i);
		ut_a(ret);
		rnd += 1; 
	}

/*
	rnd += 67475941;

	for (i = 0; i < 1; i++) {
		ret = os_aio_read(file2, buf, 8192 * (rnd % 5000), 0,
						8192, (void*)i);
		ut_a(ret);
		rnd += 1;
	}
*/
}
	ret = os_aio_read(file, buf, 8192 * (rnd % 4096), 0, 8192,
							(void*)3333);
	ut_a(ret);

	ut_a(!os_aio_all_slots_free());
/*
	printf("Starting flush!\n");
	ret = os_file_flush(file);
	ut_a(ret);
	printf("Ending flush!\n");
*/
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
	ulint	i;
	void*	buf;
	void*	mess;

	buf = (void*)(((ulint)global_buf + 6300) & (~0xFFF)); 

	os_aio_init(160, 5);
	file = os_file_create("j:\\tsfile2", OS_FILE_CREATE, OS_FILE_TABLESPACE,
				&ret);

	if (ret == FALSE) {
		ut_a(os_file_get_last_error() == OS_FILE_ALREADY_EXISTS);
	
		file = os_file_create("j:\\tsfile2", OS_FILE_OPEN,
			OS_FILE_TABLESPACE, &ret);

		ut_a(ret);
	} else {
	
		for (i = 0; i < 4100; i++) {
			ret = os_aio_write(file, buf, 8192 * i, 0, 8192, NULL);
			ut_a(ret);

			ret = os_aio_wait(0, &mess);

			ut_a(ret);
			ut_a(mess == NULL);
		}
	}

	file2 = os_file_create("F:\\tmp\\tsfile", OS_FILE_CREATE,
				OS_FILE_TABLESPACE,
				&ret);

	if (ret == FALSE) {
		ut_a(os_file_get_last_error() == OS_FILE_ALREADY_EXISTS);
	
		file2 = os_file_create("F:\\tmp\\tsfile", OS_FILE_OPEN,
			OS_FILE_TABLESPACE, &ret);

		ut_a(ret);
	} else {
	
		for (i = 0; i < 5000; i++) {
			ret = os_aio_write(file2, buf, 8192 * i, 0, 8192, NULL);
			ut_a(ret);

			ret = os_aio_wait(0, &mess);

			ut_a(ret);
			ut_a(mess == NULL);
		}
	}
}

/************************************************************************
Test of synchronous io */

void 
test5(void)
/*=======*/
{
	ulint		i, j, k;
	bool		ret;
	void*		buf;
	ulint		rnd = 0;
	ulint		tm = 0;
	ulint		oldtm = 0;
	os_file_t	files[1000];
	char		name[5];
	ulint		err;
	
        printf("-------------------------------------------\n");
	printf("OS-TEST 5. Test of creating and opening of many files\n");

	/* Align the buffer for file io */

	buf = (void*)(((ulint)global_buf + 6300) & (~0xFFF)); 

	name[2] = '.';
	name[3] = 'd';
	name[4] = '\0';

	oldtm = ut_clock();
	
	for (j = 0; j < 20; j++) {
	   for (i = 0; i < 20; i++) {
		name[0] = (char)(i + (ulint)'A');
		name[1] = (char)(j + (ulint)'A');
		files[j * 20 + i] = os_file_create(name, OS_FILE_CREATE,
			OS_FILE_NORMAL, &ret);
		if (!ret) {
			err = os_file_get_last_error();
		}
		ut_a(ret);
	  }
	}

	for (k = 0; k < i * j; k++) {
		ret = os_file_close(files[k]);
		ut_a(ret);
	}

	for (j = 0; j < 20; j++) {
	   for (i = 0; i < 20; i++) {
		name[0] = (char)(i + (ulint)'A');
		name[1] = (char)(j + (ulint)'A');
		ret = os_file_delete(name);
		ut_a(ret);
	  }
	}

	tm = ut_clock();
	printf("Wall clock time for test %lu milliseconds\n", tm - oldtm);
}

/************************************************************************
Test of synchronous io */

void 
test6(void)
/*=======*/
{
	ulint		i, j;
	bool		ret;
	void*		buf;
	ulint		rnd = 0;
	ulint		tm = 0;
	ulint		oldtm = 0;
	os_file_t	s_file;

        printf("-------------------------------------------\n");
	printf("OS-TEST 6. Test of synchronous io\n");

	buf = (void*)(((ulint)global_buf + 6300) & (~0xFFF)); 

	ret = os_file_close(file);
	ut_a(ret);

	ret = os_file_close(file2);
	ut_a(ret);
	
	s_file = os_file_create("tsfile", OS_FILE_OPEN,
					OS_FILE_NORMAL, &ret);
	if (!ret) {
		printf("Error no %lu\n", os_file_get_last_error());
	}
	
	ut_a(ret);

	rnd = ut_time() * 6346353;
	
	oldtm = ut_clock();

   for (j = 0; j < 100; j++) {

	rnd += 8072791;

	for (i = 0; i < 32; i++) {
		ret = os_file_read(s_file, buf, 8192 * (rnd % 5000), 0,
							8192);
		ut_a(ret);
		rnd += 1;
	}
   }

	tm = ut_clock();

	printf("Wall clock time for test %lu milliseconds\n", tm - oldtm);
}

/************************************************************************
Test of file size operations. */

void 
test7(void)
/*=======*/
{
	bool		ret;
	os_file_t	f;
	ulint		len;
	ulint		high;
	
        printf("-------------------------------------------\n");
	printf("OS-TEST 7. Test of setting and getting file size\n");

	
	f = os_file_create("sizefile", OS_FILE_CREATE, OS_FILE_TABLESPACE,
									&ret);
	ut_a(ret);

	ret = os_file_get_size(f, &len, &high);
	ut_a(ret);

	ut_a(len == 0);
	ut_a(high == 0);

	ret = os_file_set_size(f, 5000000, 0);
	ut_a(ret);

	ret = os_file_get_size(f, &len, &high);
	ut_a(ret);

	ut_a(len == 5000000);
	ut_a(high == 0);
	
	ret = os_file_set_size(f, 4000000, 0);
	ut_a(ret);

	ret = os_file_get_size(f, &len, &high);
	ut_a(ret);

	ut_a(len == 4000000);
	ut_a(high == 0);
	
	ret = os_file_close(f);
	ut_a(ret);

	ret = os_file_delete("sizefile");
	ut_a(ret);
}
#endif

/************************************************************************
Main test function. */

void 
main(void) 
/*======*/
{
	ulint			tm, oldtm;
	ulint			i;
	CRITICAL_SECTION	cs;
	ulint			sum;
	ulint			rnd;

	cache_buf = VirtualAlloc(NULL, 4 * 1024, MEM_COMMIT,
					PAGE_READWRITE /* | PAGE_NOCACHE */);
	oldtm = ut_clock();

	sum = 0;
	rnd = 0;
	
	for (i = 0; i < 1000000; i++) {
	
		sum += cache_buf[rnd * (16)];

		rnd += 1;

		if (rnd > 7) {
			rnd = 0;
		}
	}

	tm = ut_clock();

	printf("Wall clock time for cache test %lu milliseconds\n", tm - oldtm);

	InterlockedExchange(&i, 5);
	
	InitializeCriticalSection(&cs);

	oldtm = ut_clock();

	for (i = 0; i < 10000000; i++) {
	
		TryEnterCriticalSection(&cs);

		LeaveCriticalSection(&cs);
	}

	tm = ut_clock();
	printf("Wall clock time for test %lu milliseconds\n", tm - oldtm);

	testa1();

	test1();

/*	test2(); */

/*	init_aio(); */
/*
	test3();
*/
/*	test4();

	test5(); 

	test6();

	test7(); */
	
	printf("TESTS COMPLETED SUCCESSFULLY!\n");
} 

