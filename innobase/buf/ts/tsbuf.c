/************************************************************************
The test module for the file system and buffer manager

(c) 1995 Innobase Oy

Created 11/16/1995 Heikki Tuuri
*************************************************************************/

#include "string.h"

#include "os0thread.h"
#include "os0file.h"
#include "ut0ut.h"
#include "ut0byte.h"
#include "sync0sync.h"
#include "mem0mem.h"
#include "fil0fil.h"
#include "mtr0mtr.h"
#include "mtr0log.h"
#include "log0log.h"
#include "mach0data.h"
#include "..\buf0buf.h"
#include "..\buf0flu.h"
#include "..\buf0lru.h"

os_file_t	files[1000];

mutex_t		ios_mutex;
ulint		ios;
ulint		n[10];

mutex_t		incs_mutex;
ulint		incs;

#define N_SPACES	1
#define N_FILES		1
#define FILE_SIZE	4000
#define POOL_SIZE	1000
#define	COUNTER_OFFSET	1500

#define LOOP_SIZE	150
#define	N_THREADS	5


ulint zero = 0;

buf_frame_t*	bl_arr[POOL_SIZE];

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

	printf("Io handler thread %lu starts\n", segment);

	for (i = 0;; i++) {
		ret = fil_aio_wait(segment, &mess);
		ut_a(ret);

		buf_page_io_complete((buf_block_t*)mess);
		
		mutex_enter(&ios_mutex);
		ios++;
		mutex_exit(&ios_mutex);
		
	}

	return(0);
}

/*************************************************************************
This thread reports the status of sync system. */

ulint
info_thread(
/*========*/
	void*	arg)
{
	ulint	segment;
	
	segment = *((ulint*)arg);

	for (;;) {
		sync_print();
		os_aio_print();
		printf("Debug stop threads == %lu\n", ut_dbg_stop_threads);
		os_thread_sleep(30000000);		
	}

	return(0);
}

/*************************************************************************
Creates the files for the file system test and inserts them to
the file system. */

void
create_files(void)
/*==============*/
{
	bool		ret;
	ulint		i, k;
	char		name[20];
	os_thread_t	thr[5];
	os_thread_id_t	id[5];
	ulint		err;

	printf("--------------------------------------------------------\n");
	printf("Create or open database files\n");

	strcpy(name, "tsfile00");

	for (k = 0; k < N_SPACES; k++) {
	for (i = 0; i < N_FILES; i++) {

		name[9] = (char)((ulint)'0' + k);
		name[10] = (char)((ulint)'0' + i);
	
		files[i] = os_file_create(name, OS_FILE_CREATE,
					OS_FILE_TABLESPACE, &ret);

		if (ret == FALSE) {
			err = os_file_get_last_error();
			if (err != OS_FILE_ALREADY_EXISTS) {
				printf("OS error %lu in file creation\n", err);
				ut_error;
			}
	
			files[i] = os_file_create(
				name, OS_FILE_OPEN,
						OS_FILE_TABLESPACE, &ret);

			ut_a(ret);
		}

		ret = os_file_close(files[i]);
		ut_a(ret);

		if (i == 0) {
			fil_space_create(name, k, OS_FILE_TABLESPACE);
		}

		ut_a(fil_validate());

		fil_node_create(name, FILE_SIZE, k);
	}
	}

	ios = 0;

	mutex_create(&ios_mutex);
	
	for (i = 0; i < 5; i++) {
		n[i] = i;

		thr[i] = os_thread_create(handler_thread, n + i, id + i);
	}
/*
	n[9] = 9;
	os_thread_create(info_thread, n + 9, id);
*/
}

/************************************************************************
Creates the test database files. */

void 
create_db(void)
/*===========*/
{
	ulint			i;
	byte*			frame;
	ulint			j;
	ulint			tm, oldtm;
	mtr_t			mtr;
	
	printf("--------------------------------------------------------\n");
	printf("Write database pages\n");

	oldtm = ut_clock();

	for (i = 0; i < N_SPACES; i++) {
		for (j = 0; j < FILE_SIZE * N_FILES; j++) {
			mtr_start(&mtr);
			mtr_set_log_mode(&mtr, MTR_LOG_NONE);

			frame = buf_page_create(i, j, &mtr);
			buf_page_get(i, j, RW_X_LATCH, &mtr);

			if (j > FILE_SIZE * N_FILES - 64 * 2 - 1) {
				mlog_write_ulint(frame + FIL_PAGE_PREV, j - 5,
						MLOG_4BYTES, &mtr);
				mlog_write_ulint(frame + FIL_PAGE_NEXT, j - 7,
						MLOG_4BYTES, &mtr);
			} else {
				mlog_write_ulint(frame + FIL_PAGE_PREV, j - 1,
						MLOG_4BYTES, &mtr);
				mlog_write_ulint(frame + FIL_PAGE_NEXT, j + 1,
						MLOG_4BYTES, &mtr);
			}
					
			mlog_write_ulint(frame + FIL_PAGE_OFFSET, j,
						MLOG_4BYTES, &mtr);
			mlog_write_ulint(frame + FIL_PAGE_SPACE, i,
						MLOG_4BYTES, &mtr);
			mlog_write_ulint(frame + COUNTER_OFFSET, 0,
						MLOG_4BYTES, &mtr);

			mtr_commit(&mtr);
		}
	}

	tm = ut_clock();
	printf("Wall clock time for test %lu milliseconds\n", tm - oldtm);

	printf("--------------------------------------------------------\n");
	printf("TEST 1 A. Test of page creation when page resides in buffer\n");
	for (i = 0; i < N_SPACES; i++) {
		for (j = FILE_SIZE * N_FILES - 200;
					j < FILE_SIZE * N_FILES; j++) {
			mtr_start(&mtr);
			mtr_set_log_mode(&mtr, MTR_LOG_NONE);

			frame = buf_page_create(i, j, &mtr);
			buf_page_get(i, j, RW_X_LATCH, &mtr);

			mlog_write_ulint(frame + FIL_PAGE_PREV,
					j - 1, MLOG_4BYTES, &mtr);

			mlog_write_ulint(frame + FIL_PAGE_NEXT,
					j + 1, MLOG_4BYTES, &mtr);

			mlog_write_ulint(frame + FIL_PAGE_OFFSET, j,
					MLOG_4BYTES, &mtr);
			mlog_write_ulint(frame + FIL_PAGE_SPACE, i,
					MLOG_4BYTES, &mtr);
			mtr_commit(&mtr);
		}
	}

	printf("--------------------------------------------------------\n");
	printf("TEST 1 B. Flush pages\n");

	buf_flush_batch(BUF_FLUSH_LIST, POOL_SIZE / 2);
	buf_validate();

	printf("--------------------------------------------------------\n");
	printf("TEST 1 C. Allocate POOL_SIZE blocks to flush pages\n");

	buf_validate();
	/* Flush the pool of dirty pages */
	for (i = 0; i < POOL_SIZE; i++) {

		bl_arr[i] = buf_frame_alloc();
	}
	buf_validate();
	buf_LRU_print();

	for (i = 0; i < POOL_SIZE; i++) {

		buf_frame_free(bl_arr[i]);
	}

	buf_validate();
	ut_a(buf_all_freed());

	mtr_start(&mtr);
	frame = buf_page_get(0, 313, RW_S_LATCH, &mtr);
#ifdef UNIV_ASYNC_IO
	ut_a(buf_page_io_query(buf_block_align(frame)) == TRUE);
#endif
	mtr_commit(&mtr);
}
	
/************************************************************************
Reads the test database files. */

void 
test1(void)
/*=======*/
{
	ulint			i, j, k, c;
	byte*			frame;
	ulint			tm, oldtm;
	mtr_t			mtr;
	
	printf("--------------------------------------------------------\n");
	printf("TEST 1 D. Read linearly database files\n");

	oldtm = ut_clock();
	
	for (k = 0; k < 1; k++) {
	for (i = 0; i < N_SPACES; i++) {
		for (j = 0; j < N_FILES * FILE_SIZE; j++) {
			mtr_start(&mtr);
		
			frame = buf_page_get(i, j, RW_S_LATCH, &mtr);

			ut_a(mtr_read_ulint(frame + FIL_PAGE_OFFSET,
					   	MLOG_4BYTES, &mtr)
					== j);
			ut_a(mtr_read_ulint(frame + FIL_PAGE_SPACE,
						MLOG_4BYTES, &mtr)
					== i);

			mtr_commit(&mtr);			
		}
	}
	}

	tm = ut_clock();
	printf("Wall clock time for %lu pages %lu milliseconds\n",
			k * i * j, tm - oldtm);
	buf_validate();

	printf("--------------------------------------------------------\n");
	printf("TEST 1 E. Read linearly downward database files\n");

	oldtm = ut_clock();

	c = 0;
	
	for (k = 0; k < 1; k++) {
	for (i = 0; i < N_SPACES; i++) {
		for (j = ut_min(1000, FILE_SIZE - 1); j > 0; j--) {
			mtr_start(&mtr);
		
			frame = buf_page_get(i, j, RW_S_LATCH, &mtr);
			c++;
			
			ut_a(mtr_read_ulint(frame + FIL_PAGE_OFFSET,
						MLOG_4BYTES, &mtr)
					== j);
			ut_a(mtr_read_ulint(frame + FIL_PAGE_SPACE,
						MLOG_4BYTES, &mtr)
					== i);
			

			ut_a(buf_page_io_query(buf_block_align(frame))
			     == FALSE);
			     
			mtr_commit(&mtr);
		}
	}
	}

	tm = ut_clock();
	printf("Wall clock time for %lu pages %lu milliseconds\n",
			c, tm - oldtm);
	buf_validate();
}

/************************************************************************
Reads the test database files. */

void 
test2(void)
/*=======*/
{
	ulint			i, j, k;
	byte*			frame;
	ulint			tm, oldtm;
	mtr_t			mtr;
	
	printf("--------------------------------------------------------\n");
	printf("TEST 2. Read randomly database files\n");

	oldtm = ut_clock();

	for (k = 0; k < 100; k++) {
		i = ut_rnd_gen_ulint() % N_SPACES;
		j = ut_rnd_gen_ulint() % (N_FILES * FILE_SIZE);

		mtr_start(&mtr);		

		frame = buf_page_get(i, j, RW_S_LATCH, &mtr);
			
		ut_a(mtr_read_ulint(frame + FIL_PAGE_OFFSET,
						MLOG_4BYTES, &mtr)
					== j);
		ut_a(mtr_read_ulint(frame + FIL_PAGE_SPACE,
						MLOG_4BYTES, &mtr)
					== i);
			
		mtr_commit(&mtr);
	}

	tm = ut_clock();
	printf("Wall clock time for random %lu read %lu milliseconds\n",
		k, tm - oldtm);
}

/************************************************************************
Reads the test database files. */

void 
test3(void)
/*=======*/
{
	ulint			i, j, k;
	byte*			frame;
	ulint			tm, oldtm;
	ulint			rnd;
	mtr_t			mtr;
	
	if (FILE_SIZE < POOL_SIZE + 3050 + ut_dbg_zero) {
		return;
	}

	printf("Flush the pool of high-offset pages\n");
	
	/* Flush the pool of high-offset pages */
	for (i = 0; i < POOL_SIZE; i++) {

		mtr_start(&mtr);
		
		frame = buf_page_get(0, i, RW_S_LATCH, &mtr);

		mtr_commit(&mtr);
	}
	buf_validate();

	printf("--------------------------------------------------------\n");
	printf("TEST 3. Read randomly database pages, no read-ahead\n");
	
	oldtm = ut_clock();

	rnd = 123;

	for (k = 0; k < 400; k++) {
		rnd += 23477;
	
		i = 0;
		j = POOL_SIZE + 10 + rnd % 3000;
		
		mtr_start(&mtr);

		frame = buf_page_get(i, j, RW_S_LATCH, &mtr);
			
		ut_a(mtr_read_ulint(frame + FIL_PAGE_OFFSET,
						MLOG_4BYTES, &mtr)
					== j);
		ut_a(mtr_read_ulint(frame + FIL_PAGE_SPACE,
						MLOG_4BYTES, &mtr)
					== i);
		mtr_commit(&mtr);
	}

	tm = ut_clock();
	printf(
	   "Wall clock time for %lu random no read-ahead %lu milliseconds\n",
		k, tm - oldtm);

	buf_validate();
	printf("Flush the pool of high-offset pages\n");
	/* Flush the pool of high-offset pages */
	for (i = 0; i < POOL_SIZE; i++) {

		mtr_start(&mtr);
	
		frame = buf_page_get(0, i, RW_S_LATCH, &mtr);

		mtr_commit(&mtr);
	}

	buf_validate();
	printf("--------------------------------------------------------\n");
	printf("TEST 3 B. Read randomly database pages, random read-ahead\n");

	oldtm = ut_clock();

	rnd = 123;
	for (k = 0; k < 400; k++) {
		rnd += 23477;

		i = 0;
		j = POOL_SIZE + 10 + rnd % 400;

		mtr_start(&mtr);
		
		frame = buf_page_get(i, j, RW_S_LATCH, &mtr);

		ut_a(mtr_read_ulint(frame + FIL_PAGE_OFFSET,
						MLOG_4BYTES, &mtr)
					== j);
		ut_a(mtr_read_ulint(frame + FIL_PAGE_SPACE,
						MLOG_4BYTES, &mtr)
					== i);
		mtr_commit(&mtr);
	}

	tm = ut_clock();
	printf(
	   "Wall clock time for %lu random read-ahead %lu milliseconds\n",
		k, tm - oldtm);
}

/************************************************************************
Tests speed of CPU algorithms. */

void 
test4(void)
/*=======*/
{
	ulint			i, j;
	ulint			tm, oldtm;
	mtr_t			mtr;
	buf_frame_t*		frame;
	
	os_thread_sleep(2000000);

	printf("--------------------------------------------------------\n");
	printf("TEST 4. Speed of CPU algorithms\n");

	oldtm = ut_clock();

	for (j = 0; j < 1000; j++) {

	    mtr_start(&mtr);
	    for (i = 0; i < 20; i++) {

		frame = buf_page_get(0, i, RW_S_LATCH, &mtr);
	    }
	    mtr_commit(&mtr);
	}

	tm = ut_clock();
	printf("Wall clock time for %lu page get-release %lu milliseconds\n",
			i * j, tm - oldtm);

	buf_validate();

	oldtm = ut_clock();

	for (i = 0; i < 10000; i++) {
		frame = buf_frame_alloc();
		buf_frame_free(frame);
	}

	tm = ut_clock();
	printf("Wall clock time for %lu block alloc-free %lu milliseconds\n",
			i, tm - oldtm);

	ha_print_info(buf_pool->page_hash);
	buf_print();
}

/************************************************************************
Tests various points of code. */

void 
test5(void)
/*=======*/
{
	buf_frame_t*		frame;
	fil_addr_t		addr;
	ulint			space;
	mtr_t			mtr;
	
	printf("--------------------------------------------------------\n");
	printf("TEST 5. Various tests \n");

	mtr_start(&mtr);
	
	frame = buf_page_get(0, 313, RW_S_LATCH, &mtr);

	ut_a(buf_frame_get_space_id(frame) == 0);
	ut_a(buf_frame_get_page_no(frame) == 313);

	ut_a(buf_frame_align(frame + UNIV_PAGE_SIZE - 1) == frame);
	ut_a(buf_frame_align(frame) == frame);

	ut_a(buf_block_align(frame + UNIV_PAGE_SIZE - 1) ==
						buf_block_align(frame));

	buf_ptr_get_fsp_addr(frame + UNIV_PAGE_SIZE - 1, &space, &addr);

	ut_a(addr.page == 313)
	ut_a(addr.boffset == UNIV_PAGE_SIZE - 1);
	ut_a(space == 0);

	mtr_commit(&mtr);
}

/************************************************************************
Random test thread function. */

ulint
random_thread(
/*===========*/
	void*	arg)
{
	ulint		n;
	ulint		i, j, r, t, p, sp, count;
	ulint		s;
	buf_frame_t*	arr[POOL_SIZE / N_THREADS];
	buf_frame_t*	frame;
	mtr_t		mtr;
	mtr_t		mtr2;
	
	n = *((ulint*)arg);

	printf("Random test thread %lu starts\n", os_thread_get_curr_id());

	for (i = 0; i < 30; i++) {
	   t = ut_rnd_gen_ulint() % 10;
	   r = ut_rnd_gen_ulint() % 100;
	   s = ut_rnd_gen_ulint() % (POOL_SIZE / N_THREADS);
	   p = ut_rnd_gen_ulint();
	   sp = ut_rnd_gen_ulint() % N_SPACES;

	   if (i % 100 == 0) {
	   	printf("Thr %lu tst %lu starts\n", os_thread_get_curr_id(), t);
	   }
	   ut_a(buf_validate());

	   mtr_start(&mtr);
	   if (t == 6) {
	   	/* Allocate free blocks */
	   	for (j = 0; j < s; j++) {
	   		arr[j] = buf_frame_alloc();
	   		ut_a(arr[j]);
	   	}
	   	for (j = 0; j < s; j++) {
	   		buf_frame_free(arr[j]);
	   	}
	   } else if (t == 9) {
/*	   	buf_flush_batch(BUF_FLUSH_LIST, 30); */
	   	
	   } else if (t == 7) {
	   	/* x-lock many blocks */
	   	for (j = 0; j < s; j++) {
	   		arr[j] = buf_page_get(sp, (p + j)
	   					% (N_FILES * FILE_SIZE),
	   					RW_X_LATCH,
	   					&mtr);
	   		ut_a(arr[j]);
	   		if (j > 0) {
	   			ut_a(arr[j] != arr[j - 1]);
	   		}
	   	}
	   	ut_a(buf_validate());
	   } else if (t == 8) {
	   	/* s-lock many blocks */
	   	for (j = 0; j < s; j++) {
	   		arr[j] = buf_page_get(sp, (p + j)
						% (N_FILES * FILE_SIZE),
						RW_S_LATCH,
							&mtr);
	   		ut_a(arr[j]);
	   		if (j > 0) {
	   			ut_a(arr[j] != arr[j - 1]);
	   		}
	   	}
	   } else if (t <= 2) {
	   	for (j = 0; j < r; j++) {
			/* Read pages */
			mtr_start(&mtr2);
			frame = buf_page_get(sp,
					p % (N_FILES * FILE_SIZE),
					RW_S_LATCH, &mtr2);

			ut_a(mtr_read_ulint(frame + FIL_PAGE_OFFSET,
						MLOG_4BYTES, &mtr2)
					== p % (N_FILES * FILE_SIZE));
			ut_a(mtr_read_ulint(frame + FIL_PAGE_SPACE,
						MLOG_4BYTES, &mtr2)
					== sp);
			mtr_commit(&mtr2);
			if (t == 0) {
				p++;	/* upward */
			} else if (t == 1) {
				p--;	/* downward */
			} else if (t == 2) {
				p = ut_rnd_gen_ulint(); /* randomly */
			}
		}
	   } else if (t <= 5) {
	   	for (j = 0; j < r; j++) {
			/* Write pages */
			mtr_start(&mtr2);
			frame = buf_page_get(sp, p % (N_FILES * FILE_SIZE),
						RW_X_LATCH, &mtr2);
			count = 1 + mtr_read_ulint(frame + COUNTER_OFFSET,
						MLOG_4BYTES, &mtr2);
			mutex_enter(&incs_mutex);
			incs++;
			mutex_exit(&incs_mutex);
			mlog_write_ulint(frame + COUNTER_OFFSET, count,
						MLOG_4BYTES, &mtr2);
			mtr_commit(&mtr2);
			if (t == 3) {
				p++;	/* upward */
			} else if (t == 4) {
				p--;	/* downward */
			} else if (t == 5) {
				p = ut_rnd_gen_ulint(); /* randomly */
			}
		}
	   } /* if t = */

	   mtr_commit(&mtr);
/*	   printf("Thr %lu tst %lu ends ", os_thread_get_curr_id(), t); */
	   ut_a(buf_validate());
	} /* for i */
	printf("\nRandom test thread %lu exits\n", os_thread_get_curr_id());
	return(0);
}

/************************************************************************
Random test thread function which reports the rw-lock list. */

ulint
rw_list_thread(
/*===========*/
	void*	arg)
{
	ulint		n;
	ulint		i;
	
	n = *((ulint*)arg);

	printf("\nRw list test thread %lu starts\n", os_thread_get_curr_id());

	for (i = 0; i < 10; i++) {
		os_thread_sleep(3000000);
		rw_lock_list_print_info();
		buf_validate();
	}

	return(0);
}

/*************************************************************************
Performs random operations on the buffer with several threads. */

void
test6(void)
/*=======*/
{
	ulint		i, j;
	os_thread_t	thr[N_THREADS + 1];
	os_thread_id_t	id[N_THREADS + 1];
	ulint		n[N_THREADS + 1];
	ulint		count	= 0;
	buf_frame_t*	frame;
	mtr_t		mtr;
	
	printf("--------------------------------------------------------\n");
	printf("TEST 6. Random multi-thread test on the buffer \n");

	incs = 0;
	mutex_create(&incs_mutex);
	
	for (i = 0; i < N_THREADS; i++) {
		n[i] = i;

		thr[i] = os_thread_create(random_thread, n + i, id + i);
	}
/*
	n[N_THREADS] = N_THREADS;

	thr[N_THREADS] = os_thread_create(rw_list_thread, n + N_THREADS,
					id + N_THREADS);
*/
	for (i = 0; i < N_THREADS; i++) {
		os_thread_wait(thr[i]);
	}

/*	os_thread_wait(thr[N_THREADS]); */

	for (i = 0; i < N_SPACES; i++) {
		for (j = 0; j < N_FILES * FILE_SIZE; j++) {
			mtr_start(&mtr);
		
			frame = buf_page_get(i, j, RW_S_LATCH, &mtr);

			ut_a(mtr_read_ulint(frame + FIL_PAGE_OFFSET,
						MLOG_4BYTES, &mtr)
					== j);
			ut_a(mtr_read_ulint(frame + FIL_PAGE_SPACE,
						MLOG_4BYTES, &mtr)
					== i);

			count += mtr_read_ulint(frame + COUNTER_OFFSET,
						MLOG_4BYTES, &mtr);
			
			mtr_commit(&mtr);
		}
	}

	printf("Count %lu incs %lu\n", count, incs);
	ut_a(count == incs);
}

/************************************************************************
Frees the spaces in the file system. */

void
free_system(void)
/*=============*/
{
	ulint	i;

	for (i = 0; i < N_SPACES; i++) {
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

/*	buf_debug_prints = TRUE; */
	
	oldtm = ut_clock();
	
	os_aio_init(160, 5);
	sync_init();
	mem_init(1500000);
	fil_init(26);	/* Allow 25 open files at a time */
	buf_pool_init(POOL_SIZE, POOL_SIZE);
	log_init();
	
	buf_validate();

	ut_a(fil_validate());
	
	create_files();

	create_db();

	buf_validate();

	test1();
	buf_validate();

	test2();
	buf_validate();

	test3();
	buf_validate();

	test4();

	test5();

	buf_validate();

	test6();

	buf_validate();

	buf_print();
	
	buf_flush_batch(BUF_FLUSH_LIST, POOL_SIZE + 1);
	buf_print();
	buf_validate();

	os_thread_sleep(1000000);
	
	buf_print();
	buf_all_freed();
	
	free_system();	
	
	tm = ut_clock();
	printf("Wall clock time for test %lu milliseconds\n", tm - oldtm);
	printf("TESTS COMPLETED SUCCESSFULLY!\n");
} 
