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
#include "..\buf0buf.h"
#include "..\buf0buf.h1"
#include "..\buf0buf.h2"
#include "..\buf0flu.h"
#include "..\buf0lru.h"
#include "mtr0buf.h"
#include "mtr0log.h"
#include "fsp0fsp.h"
#include "log0log.h"

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
	mtr_t			mtr; 
	
	oldtm = ut_clock();

	for (i = 0; i < 1; i++) {
		for (j = 0; j < 4096; j++) {
			mtr_start(&mtr);
			if (j == 0) {
				fsp_header_init(i, 4096, &mtr);

				block = mtr_page_get(i, j, NULL, &mtr);
			} else { 
				block = mtr_page_create(i, j, &mtr);
			}

			frame = buf_block_get_frame(block);

			mtr_page_x_lock(block, &mtr);

			mlog_write_ulint(frame + FIL_PAGE_PREV,
					j - 1, MLOG_4BYTES, &mtr);
			
			mlog_write_ulint(frame + FIL_PAGE_NEXT,
					j + 1, MLOG_4BYTES, &mtr);
			
			mlog_write_ulint(frame + FIL_PAGE_OFFSET,
					j, MLOG_4BYTES, &mtr);
			
			mtr_commit(&mtr);
		}
	}

	tm = ut_clock();
	printf("Wall clock time for test %lu milliseconds\n", tm - oldtm);

	/* Flush the pool of dirty pages by reading low-offset pages */
	for (i = 0; i < 1000; i++) {

		mtr_start(&mtr);
		block = mtr_page_get(0, i, NULL, &mtr);

		frame = buf_block_get_frame(block);

		mtr_page_s_lock(block, &mtr);

		ut_a(mtr_read_ulint(frame + FIL_PAGE_OFFSET, MLOG_4BYTES,
				   &mtr) == i);
			
		mtr_commit(&mtr);
	}

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
	bool		ret;
	ulint		i, k;
	char		name[20];
	os_thread_t	thr[5];
	os_thread_id_t	id[5];

	strcpy(name, "j:\\tsfile1");

	for (k = 0; k < 1; k++) {
	for (i = 0; i < 4; i++) {

		name[9] = (char)((ulint)'0' + i);
	
		files[i] = os_file_create(name, OS_FILE_CREATE,
					OS_FILE_TABLESPACE, &ret);

		if (ret == FALSE) {
			ut_a(os_file_get_last_error() ==
						OS_FILE_ALREADY_EXISTS);
	
			files[i] = os_file_create(
				name, OS_FILE_OPEN,
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

		fil_node_create(name, 4096, k);
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

			buf_page_s_lock(block);

			ut_a(*((ulint*)(frame + 16)) == j);
			
			buf_page_s_unlock(block);

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

		buf_page_s_lock(block);

		ut_a(*((ulint*)(frame + 16)) == j);
			
		buf_page_s_unlock(block);

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

		buf_page_s_lock(block);

		ut_a(*((ulint*)(frame + 16)) == i);
			
		buf_page_s_unlock(block);

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

		buf_page_s_lock(block);

		ut_a(*((ulint*)(frame + 16)) == j);
			
		buf_page_s_unlock(block);

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

		buf_page_s_lock(block);

		ut_a(*((ulint*)(frame + 16)) == i);
			
		buf_page_s_unlock(block);

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

		buf_page_s_lock(block);

		ut_a(*((ulint*)(frame + 16)) == j);
			
		buf_page_s_unlock(block);

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
		buf_page_s_lock(block);

		buf_page_s_unlock(block);
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
		
		buf_page_get(0, i, NULL);
/*
		buf_page_s_lock(block);

		buf_page_s_unlock(block);
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
Test for file space management. */

void
test5(void)
/*=======*/
{
	mtr_t		mtr;
	ulint		seg_page;
	ulint		new_page;
	ulint		seg_page2;
	ulint		new_page2;
	buf_block_t*	block;
	bool		finished;
	ulint		i;
	ulint		reserved;
	ulint		used;
	ulint		tm, oldtm;

	os_thread_sleep(1000000);

	buf_validate();

	buf_print();
	
	mtr_start(&mtr);

	seg_page = fseg_create(0, 0, 1000, 555, &mtr);

	mtr_commit(&mtr);

	os_thread_sleep(1000000);
	buf_validate();
	printf("Segment created: header page %lu\n", seg_page);

	mtr_start(&mtr);

	block = mtr_page_get(0, seg_page, NULL, &mtr);
	
	new_page = fseg_alloc_free_page(buf_block_get_frame(block) + 1000,
					2, FSP_UP, &mtr);	
	
	mtr_commit(&mtr);

	buf_validate();
	buf_print();
	printf("Segment page allocated %lu\n", new_page);

	finished = FALSE;

	while (!finished) {
	
		mtr_start(&mtr);

		block = mtr_page_get(0, seg_page, NULL, &mtr);
	
		finished = fseg_free_step(
			buf_block_get_frame(block) + 1000, &mtr);	
	
		mtr_commit(&mtr);
	}

	/***********************************************/
	os_thread_sleep(1000000);
	buf_validate();
	buf_print();
	mtr_start(&mtr);

	seg_page = fseg_create(0, 0, 1000, 557, &mtr);

	mtr_commit(&mtr);

	ut_a(seg_page == 1);
	
	printf("Segment created: header page %lu\n", seg_page);

	new_page = seg_page;
	for (i = 0; i < 1023; i++) {

		mtr_start(&mtr);

		block = mtr_page_get(0, seg_page, NULL, &mtr);
	
		new_page = fseg_alloc_free_page(
					buf_block_get_frame(block) + 1000,
					new_page + 1, FSP_UP, &mtr);
		if (i < FSP_EXTENT_SIZE - 1) {
			ut_a(new_page == 2 + i);
		} else {
			ut_a(new_page == i + FSP_EXTENT_SIZE + 1);
		}	

		printf("%lu %lu; ", i, new_page);
		if (i % 10 == 0) {
			printf("\n");
		}
	
		mtr_commit(&mtr);
	}

	buf_print();
	buf_validate();

	mtr_start(&mtr);

	block = mtr_page_get(0, seg_page, NULL, &mtr);

	mtr_page_s_lock(block, &mtr);
	
	reserved = fseg_n_reserved_pages(buf_block_get_frame(block) + 1000,
					&used, &mtr);

	ut_a(used == 1024);	
	ut_a(reserved >= 1024);	

	printf("Pages used in segment %lu reserved by segment %lu \n",
		used, reserved);
	
	mtr_commit(&mtr);

	finished = FALSE;

	while (!finished) {

		mtr_start(&mtr);

		block = mtr_page_get(0, seg_page, NULL, &mtr);
	
		finished = fseg_free_step(
			buf_block_get_frame(block) + 1000, &mtr);	
	
		mtr_commit(&mtr);
	}

	buf_print();
	buf_validate();

	/***********************************************/

	mtr_start(&mtr);

	seg_page = fseg_create(0, 0, 1000, 557, &mtr);

	mtr_commit(&mtr);

	ut_a(seg_page == 1);

	mtr_start(&mtr);

	seg_page2 = fseg_create(0, 0, 1000, 558, &mtr);

	mtr_commit(&mtr);

	ut_a(seg_page2 == 2);
	
	new_page = seg_page;
	new_page2 = seg_page2;

	for (;;) {

		mtr_start(&mtr);

		block = mtr_page_get(0, seg_page, NULL, &mtr);
	
		new_page = fseg_alloc_free_page(
					buf_block_get_frame(block) + 1000,
					new_page + 1, FSP_UP, &mtr);

		printf("1:%lu %lu; ", i, new_page);
		if (i % 10 == 0) {
			printf("\n");
		}
	
		new_page = fseg_alloc_free_page(
					buf_block_get_frame(block) + 1000,
					new_page + 1, FSP_UP, &mtr);

		printf("1:%lu %lu; ", i, new_page);
		if (i % 10 == 0) {
			printf("\n");
		}

		mtr_commit(&mtr);

		mtr_start(&mtr);

		block = mtr_page_get(0, seg_page2, NULL, &mtr);
	
		new_page2 = fseg_alloc_free_page(
					buf_block_get_frame(block) + 1000,
					new_page2 + 1, FSP_UP, &mtr);

		printf("2:%lu %lu; ", i, new_page2);
		if (i % 10 == 0) {
			printf("\n");
		}
	
		mtr_commit(&mtr);

		if (new_page2 == FIL_NULL) {
			break;
		}
	}

	mtr_start(&mtr);

	block = mtr_page_get(0, seg_page, NULL, &mtr);

	mtr_page_s_lock(block, &mtr);
	
	reserved = fseg_n_reserved_pages(buf_block_get_frame(block) + 1000,
					&used, &mtr);

	printf("Pages used in segment 1 %lu, reserved by segment %lu \n",
		used, reserved);
	
	mtr_commit(&mtr);

	mtr_start(&mtr);

	block = mtr_page_get(0, seg_page2, NULL, &mtr);

	mtr_page_s_lock(block, &mtr);
	
	reserved = fseg_n_reserved_pages(buf_block_get_frame(block) + 1000,
					&used, &mtr);

	printf("Pages used in segment 2 %lu, reserved by segment %lu \n",
		used, reserved);
	
	mtr_commit(&mtr);
	
	for (;;) {

		mtr_start(&mtr);

		block = mtr_page_get(0, seg_page, NULL, &mtr);
	
		fseg_free_step(
			      buf_block_get_frame(block) + 1000, &mtr);
	
		block = mtr_page_get(0, seg_page2, NULL, &mtr);
	
		finished = fseg_free_step(
			      buf_block_get_frame(block) + 1000, &mtr);

		mtr_commit(&mtr);

		if (finished) {
			break;
		}
	}

	mtr_start(&mtr);

	seg_page2 = fseg_create(0, 0, 1000, 558, &mtr);

	mtr_commit(&mtr);

	i = 0;
	for (;;) {
		mtr_start(&mtr);

		block = mtr_page_get(0, seg_page2, NULL, &mtr);
	
		new_page2 = fseg_alloc_free_page(
					buf_block_get_frame(block) + 1000,
					557, FSP_DOWN, &mtr);

		printf("%lu %lu; ", i, new_page2);
		mtr_commit(&mtr);

		if (new_page2 == FIL_NULL) {
			break;
		}
		i++;
	}

	for (;;) {

		mtr_start(&mtr);

		block = mtr_page_get(0, seg_page, NULL, &mtr);
	
		finished = fseg_free_step(
			      buf_block_get_frame(block) + 1000, &mtr);
	
		mtr_commit(&mtr);

		if (finished) {
			break;
		}
	}

	for (;;) {

		mtr_start(&mtr);

		block = mtr_page_get(0, seg_page2, NULL, &mtr);
	
		finished = fseg_free_step(
			      buf_block_get_frame(block) + 1000, &mtr);
	
		mtr_commit(&mtr);

		if (finished) {
			break;
		}
	}

	
	/***************************************/
	
	oldtm = ut_clock();

    for (i = 0; i < 1000; i++) {	
	mtr_start(&mtr);

	seg_page = fseg_create(0, 0, 1000, 555, &mtr);

	mtr_commit(&mtr);

	mtr_start(&mtr);

	block = mtr_page_get(0, seg_page, NULL, &mtr);
	
	new_page = fseg_alloc_free_page(buf_block_get_frame(block) + 1000,
					2, FSP_UP, &mtr);	
	
	mtr_commit(&mtr);

	finished = FALSE;

	while (!finished) {
	
		mtr_start(&mtr);

		block = mtr_page_get(0, seg_page, NULL, &mtr);
	
		finished = fseg_free_step(
			buf_block_get_frame(block) + 1000, &mtr);	
	
		mtr_commit(&mtr);
	}
    }

	tm = ut_clock();
	printf("Wall clock time for %lu seg crea+free %lu millisecs\n",
			i, tm - oldtm);

	buf_validate();

	buf_flush_batch(BUF_FLUSH_LIST, 500);

	os_thread_sleep(1000000);
	
	buf_all_freed();
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
	log_init();
	
	buf_validate();

	ut_a(fil_validate());
	
	create_files();

	create_db();

	buf_validate();

	test5();
/*
	test1();

	test3();

	test4();

	test2();
*/
	buf_validate();

	n = buf_flush_batch(BUF_FLUSH_LIST, 500);

	os_thread_sleep(1000000);
	
	buf_all_freed();
	
	free_system();	
	
	tm = ut_clock();
	printf("Wall clock time for test %lu milliseconds\n", tm - oldtm);
	printf("TESTS COMPLETED SUCCESSFULLY!\n");
} 
