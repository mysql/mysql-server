/************************************************************************
The test module for the file system and buffer manager

(c) 1995 Innobase Oy

Created 11/16/1995 Heikki Tuuri
*************************************************************************/

#include "os0thread.h"
#include "os0file.h"
#include "ut0ut.h"
#include "sync0sync.h"
#include "mem0mem.h"
#include "fil0fil.h"
#include "..\buf0buf.h"
#include "..\buf0flu.h"
#include "..\buf0lru.h"

os_file_t	files[1000];

mutex_t		ios_mutex;
ulint		ios;
ulint		n[5];


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
		ret = fil_aio_wait(segment, &mess);
		ut_a(ret);

		buf_page_io_complete((buf_block_t*)mess);
		
		mutex_enter(&ios_mutex);
		ios++;
		mutex_exit(&ios_mutex);
		
	}

	return(0);
}

/************************************************************************
Creates the test database files. */

void 
create_db(void)
/*===========*/
{
	ulint			i;
	buf_block_t*		block;
	byte*			frame;
	ulint			j;
	ulint			tm, oldtm;

	oldtm = ut_clock();

	for (i = 0; i < 1; i++) {
		for (j = 0; j < 4096; j++) {
			block = buf_page_create(i, j);

			frame = buf_block_get_frame(block);

			rw_lock_x_lock(buf_page_get_lock(block));

			if (j > 0) {
				fil_page_set_prev(frame, j - 1);
			} else {
				fil_page_set_prev(frame, 0);
			}

			if (j < 4095) {
				fil_page_set_next(frame, j + 1);
			} else {
				fil_page_set_next(frame, 0);
			}

			*((ulint*)(frame + 16)) = j;
			
			buf_page_note_modification(block);
			
			rw_lock_x_unlock(buf_page_get_lock(block));

			buf_page_release(block);
		}
	}

	tm = ut_clock();
	printf("Wall clock time for test %lu milliseconds\n", tm - oldtm);

/*	buf_LRU_print(); */

	/* Flush the pool of dirty pages by reading low-offset pages */
	for (i = 0; i < 1000; i++) {
		
		block = buf_page_get(0, i, NULL);

		frame = buf_block_get_frame(block);

		rw_lock_s_lock(buf_page_get_lock(block));

		ut_a(*((ulint*)(frame + 16)) == i);
			
		rw_lock_s_unlock(buf_page_get_lock(block));

		buf_page_release(block);
	}

/*	buf_LRU_print(); */

	os_thread_sleep(1000000);

	ut_a(buf_all_freed());
}
	
/*************************************************************************
Creates the files for the file system test and inserts them to
the file system. */

void
create_files(void)
/*==============*/
{
	bool	ret;
	ulint	i, k;
	char	name[10];
	os_thread_t		thr[5];
	os_thread_id_t		id[5];
	
	name[0] = 't';
	name[1] = 's';
	name[2] = 'f';
	name[3] = 'i';
	name[4] = 'l';
	name[5] = 'e';
	name[8] = '\0';

	for (k = 0; k < 1; k++) {
	for (i = 0; i < 1; i++) {

		name[6] = (char)(k + (ulint)'a');
		name[7] = (char)(i + (ulint)'a');
	
		files[i] = os_file_create("j:\\tsfile4", OS_FILE_CREATE,
					OS_FILE_TABLESPACE, &ret);

		if (ret == FALSE) {
			ut_a(os_file_get_last_error() ==
						OS_FILE_ALREADY_EXISTS);
	
			files[i] = os_file_create(
				"j:\\tsfile4", OS_FILE_OPEN,
						OS_FILE_TABLESPACE, &ret);

			ut_a(ret);
		}

		ret = os_file_set_size(files[i], 4096 * 8192, 0);
		ut_a(ret);		
		
		ret = os_file_close(files[i]);
		ut_a(ret);

		if (i == 0) {
			fil_space_create("noname", k, OS_FILE_TABLESPACE);
		}

		ut_a(fil_validate());

		fil_node_create("j:\\tsfile4", 4096, k);
	}
	}

	ios = 0;

	mutex_create(&ios_mutex);
	
	for (i = 0; i < 5; i++) {
		n[i] = i;

		thr[i] = os_thread_create(handler_thread, n + i, id + i);
	}
}

/************************************************************************
Reads the test database files. */

void 
test1(void)
/*=======*/
{
	ulint			i, j, k;
	buf_block_t*		block;
	byte*			frame;
	ulint			tm, oldtm;

	buf_flush_batch(BUF_FLUSH_LIST, 1000);

	os_thread_sleep(1000000);

	buf_all_freed();
	
	oldtm = ut_clock();
	
	for (k = 0; k < 1; k++) {
	for (i = 0; i < 1; i++) {
		for (j = 0; j < 409; j++) {
			block = buf_page_get(i, j, NULL);

			frame = buf_block_get_frame(block);

			rw_lock_s_lock(buf_page_get_lock(block));

			ut_a(*((ulint*)(frame + 16)) == j);
			
			rw_lock_s_unlock(buf_page_get_lock(block));

			buf_page_release(block);
		}
	}
	}

	tm = ut_clock();
	printf("Wall clock time for test %lu milliseconds\n", tm - oldtm);

}

/************************************************************************
Reads the test database files. */

void 
test2(void)
/*=======*/
{
	ulint			i, j, k, rnd;
	buf_block_t*		block;
	byte*			frame;
	ulint			tm, oldtm;

	oldtm = ut_clock();

	rnd = 123;

	for (k = 0; k < 100; k++) {
		rnd += 23651;
		rnd = rnd % 4096;
	
		i = rnd / 4096; 
		j = rnd % 2048;
		
		block = buf_page_get(i, j, NULL);

		frame = buf_block_get_frame(block);

		rw_lock_s_lock(buf_page_get_lock(block));

		ut_a(*((ulint*)(frame + 16)) == j);
			
		rw_lock_s_unlock(buf_page_get_lock(block));

		buf_page_release(block);
	}

	tm = ut_clock();
	printf("Wall clock time for random read %lu milliseconds\n",
		tm - oldtm);
}

/************************************************************************
Reads the test database files. */

void 
test4(void)
/*=======*/
{
	ulint			i, j, k, rnd;
	buf_block_t*		block;
	byte*			frame;
	ulint			tm, oldtm;

	/* Flush the pool of high-offset pages */
	for (i = 0; i < 1000; i++) {
		
		block = buf_page_get(0, i, NULL);

		frame = buf_block_get_frame(block);

		rw_lock_s_lock(buf_page_get_lock(block));

		ut_a(*((ulint*)(frame + 16)) == i);
			
		rw_lock_s_unlock(buf_page_get_lock(block));

		buf_page_release(block);
	}

	printf("Test starts\n");
	
	oldtm = ut_clock();

	rnd = 123;

	for (k = 0; k < 400; k++) {

		rnd += 4357;
	
		i = 0;
		j = 1001 + rnd % 3000;
		
		block = buf_page_get(i, j, NULL);

		frame = buf_block_get_frame(block);

		rw_lock_s_lock(buf_page_get_lock(block));

		ut_a(*((ulint*)(frame + 16)) == j);
			
		rw_lock_s_unlock(buf_page_get_lock(block));

		buf_page_release(block);
	}

	tm = ut_clock();
	printf(
	   "Wall clock time for %lu random no read-ahead %lu milliseconds\n",
		k, tm - oldtm);

	/* Flush the pool of high-offset pages */
	for (i = 0; i < 1000; i++) {
		
		block = buf_page_get(0, i, NULL);

		frame = buf_block_get_frame(block);

		rw_lock_s_lock(buf_page_get_lock(block));

		ut_a(*((ulint*)(frame + 16)) == i);
			
		rw_lock_s_unlock(buf_page_get_lock(block));

		buf_page_release(block);
	}

	printf("Test starts\n");

	oldtm = ut_clock();

	rnd = 123;

	for (k = 0; k < 400; k++) {

		rnd += 4357;
	
		i = 0;
		j = 1001 + rnd % 400;
		
		block = buf_page_get(i, j, NULL);

		frame = buf_block_get_frame(block);

		rw_lock_s_lock(buf_page_get_lock(block));

		ut_a(*((ulint*)(frame + 16)) == j);
			
		rw_lock_s_unlock(buf_page_get_lock(block));

		buf_page_release(block);
	}

	tm = ut_clock();
	printf(
	   "Wall clock time for %lu random read-ahead %lu milliseconds\n",
		k, tm - oldtm);

}

/************************************************************************
Tests speed of CPU algorithms. */

void 
test3(void)
/*=======*/
{
	ulint			i, j;
	buf_block_t*		block;
	ulint			tm, oldtm;

	for (i = 0; i < 400; i++) {
		
		block = buf_page_get(0, i, NULL);

		buf_page_release(block);
	}

	os_thread_sleep(2000000);

	oldtm = ut_clock();

	for (j = 0; j < 500; j++) {
	for (i = 0; i < 200; i++) {
		
		block = buf_page_get(0, i, NULL);

/*
		rw_lock_s_lock(buf_page_get_lock(block));

		rw_lock_s_unlock(buf_page_get_lock(block));
*/

		buf_page_release(block);

	}
	}

	tm = ut_clock();
	printf("Wall clock time for %lu page get-release %lu milliseconds\n",
			i * j, tm - oldtm);

	oldtm = ut_clock();

	for (j = 0; j < 500; j++) {
	for (i = 0; i < 200; i++) {
		
		buf_block_get(block);
/*
		rw_lock_s_lock(buf_page_get_lock(block));

		rw_lock_s_unlock(buf_page_get_lock(block));
*/
		buf_page_release(block);
	}
	}

	tm = ut_clock();
	printf("Wall clock time for %lu block get-release %lu milliseconds\n",
			i * j, tm - oldtm);


	oldtm = ut_clock();

	for (i = 0; i < 100000; i++) {
		block = buf_block_alloc();
		buf_block_free(block);
	}

	tm = ut_clock();
	printf("Wall clock time for %lu block alloc-free %lu milliseconds\n",
			i, tm - oldtm);

	ha_print_info(buf_pool->page_hash);
}

/************************************************************************
Frees the spaces in the file system. */

void
free_system(void)
/*=============*/
{
	ulint	i;

	for (i = 0; i < 1; i++) {
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
	ulint	n;

	oldtm = ut_clock();
	
	os_aio_init(160, 5);
	sync_init();
	mem_init();
	fil_init(26);	/* Allow 25 open files at a time */
	buf_pool_init(1000, 1000);
	
	buf_validate();

	ut_a(fil_validate());
	
	create_files();

	create_db();

	buf_validate();

	test1();

	test3();

	test4();

	test2();

	buf_validate();

	n = buf_flush_batch(BUF_FLUSH_LIST, 500);

	os_thread_sleep(1000000);
	
	buf_all_freed();
	
	free_system();	
	
	tm = ut_clock();
	printf("Wall clock time for test %lu milliseconds\n", tm - oldtm);
	printf("TESTS COMPLETED SUCCESSFULLY!\n");
} 
