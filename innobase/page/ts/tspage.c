/************************************************************************
The test for the index page

(c) 1994-1996 Innobase Oy

Created 1/31/1994 Heikki Tuuri
*************************************************************************/

#include "sync0sync.h"
#include "ut0mem.h"
#include "mem0mem.h"
#include "data0data.h"
#include "data0type.h"
#include "dict0dict.h"
#include "buf0buf.h"
#include "os0file.h"
#include "fil0fil.h"
#include "rem0rec.h"
#include "rem0cmp.h"
#include "mtr0mtr.h"
#include "log0log.h"
#include "..\page0page.h"
#include "..\page0cur.h"

os_file_t	files[1000];

mutex_t		ios_mutex;
ulint		ios;
ulint		n[10];

mutex_t		incs_mutex;
ulint		incs;

byte		bigbuf[1000000];

#define N_SPACES	1
#define N_FILES		2
#define FILE_SIZE	1000 	/* must be > 512 */
#define POOL_SIZE	1000
#define	COUNTER_OFFSET	1500

#define LOOP_SIZE	150
#define	N_THREADS	5


ulint zero = 0;

buf_block_t*	bl_arr[POOL_SIZE];

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

	printf("--------------------------------------------------------\n");
	printf("Create or open database files\n");

	strcpy(name, "j:\\tsfile00");

	for (k = 0; k < N_SPACES; k++) {
	for (i = 0; i < N_FILES; i++) {

		name[9] = (char)((ulint)'0' + k);
		name[10] = (char)((ulint)'0' + i);
	
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
}

/*********************************************************************
Test for index page. */

void 
test1(void)
/*=======*/
{
	page_t*		page;
	dtuple_t*	tuple;
	mem_heap_t*	heap;
	ulint		i;
	ulint		rnd	= 0;
	rec_t*		rec;
	page_cur_t	cursor;
	dict_index_t*	index;	
	dict_table_t*	table;
	buf_block_t*	block;
	buf_frame_t*	frame;
	mtr_t		mtr;
	
	printf("-------------------------------------------------\n");
	printf("TEST 1. Basic test\n");

	heap = mem_heap_create(0);
	
	table = dict_table_create("TS_TABLE1", 3);

	dict_table_add_col(table, "COL1", DATA_VARCHAR, DATA_ENGLISH, 10, 0);
	dict_table_add_col(table, "COL2", DATA_VARCHAR, DATA_ENGLISH, 10, 0);
	dict_table_add_col(table, "COL3", DATA_VARCHAR, DATA_ENGLISH, 10, 0);

	ut_a(0 == dict_table_publish(table));

	index = dict_index_create("TS_TABLE1", "IND1", 0, 3, 0);

	dict_index_add_field(index, "COL1", 0);
	dict_index_add_field(index, "COL2", 0);
	dict_index_add_field(index, "COL3", 0);

	ut_a(0 == dict_index_publish(index));

	index = dict_index_get("TS_TABLE1", "IND1");
	ut_a(index);

	tuple = dtuple_create(heap, 3);

	mtr_start(&mtr);

	block = buf_page_create(0, 5, &mtr);
	buf_page_x_lock(block, &mtr);

	frame = buf_block_get_frame(block);

	page = page_create(frame, &mtr);

	for (i = 0; i < 512; i++) {

		rnd = (rnd + 534671) % 512;

		if (i % 27 == 0) {
			ut_a(page_validate(page, index));
		}
	
		dtuple_gen_test_tuple(tuple, rnd);

/*		dtuple_print(tuple);*/

		page_cur_search(page, tuple, PAGE_CUR_G, &cursor);

		rec = page_cur_insert_rec(&cursor, tuple, NULL, &mtr);

		ut_a(rec);

		rec_validate(rec);
/*		page_print_list(page, 151); */
	}

/*	page_print_list(page, 151); */

	ut_a(page_validate(page, index));
	ut_a(page_get_n_recs(page) == 512);

	for (i = 0; i < 512; i++) {

		rnd = (rnd + 7771) % 512;
	
		if (i % 27 == 0) {
			ut_a(page_validate(page, index));
		}

		dtuple_gen_test_tuple(tuple, rnd);

/*		dtuple_print(tuple);*/

		page_cur_search(page, tuple, PAGE_CUR_G, &cursor);

		page_cur_delete_rec(&cursor, &mtr);

		ut_a(rec);

		rec_validate(rec);
/*		page_print_list(page, 151); */
	}

	ut_a(page_get_n_recs(page) == 0);

	ut_a(page_validate(page, index));
	page = page_create(frame, &mtr);

	rnd = 311;
	
	for (i = 0; i < 512; i++) {

		rnd = (rnd + 1) % 512;

		if (i % 27 == 0) {
			ut_a(page_validate(page, index));
		}
	
		dtuple_gen_test_tuple(tuple, rnd);

/*		dtuple_print(tuple);*/

		page_cur_search(page, tuple, PAGE_CUR_G, &cursor);

		rec = page_cur_insert_rec(&cursor, tuple, NULL, &mtr);

		ut_a(rec);

		rec_validate(rec);
/*		page_print_list(page, 151); */
	}

	ut_a(page_validate(page, index));
	ut_a(page_get_n_recs(page) == 512);

	rnd = 217;

	for (i = 0; i < 512; i++) {

		rnd = (rnd + 1) % 512;
	
		if (i % 27 == 0) {
			ut_a(page_validate(page, index));
		}

		dtuple_gen_test_tuple(tuple, rnd);

/*		dtuple_print(tuple);*/

		page_cur_search(page, tuple, PAGE_CUR_G, &cursor);

		page_cur_delete_rec(&cursor, &mtr);

		ut_a(rec);

		rec_validate(rec);
/*		page_print_list(page, 151); */
	}

	ut_a(page_validate(page, index));
	ut_a(page_get_n_recs(page) == 0);
	page = page_create(frame, &mtr);

	rnd = 291;
	
	for (i = 0; i < 512; i++) {

		rnd = (rnd - 1) % 512;

		if (i % 27 == 0) {
			ut_a(page_validate(page, index));
		}
	
		dtuple_gen_test_tuple(tuple, rnd);

/*		dtuple_print(tuple);*/

		page_cur_search(page, tuple, PAGE_CUR_G, &cursor);

		rec = page_cur_insert_rec(&cursor, tuple, NULL, &mtr);

		ut_a(rec);

		rec_validate(rec);
/*		page_print_list(page, 151); */
	}

	ut_a(page_validate(page, index));
	ut_a(page_get_n_recs(page) == 512);

	rnd = 277;

	for (i = 0; i < 512; i++) {

		rnd = (rnd - 1) % 512;
	
		if (i % 27 == 0) {
			ut_a(page_validate(page, index));
		}

		dtuple_gen_test_tuple(tuple, rnd);

/*		dtuple_print(tuple);*/

		page_cur_search(page, tuple, PAGE_CUR_G, &cursor);

		page_cur_delete_rec(&cursor, &mtr);

		ut_a(rec);

		rec_validate(rec);
/*		page_print_list(page, 151); */
	}

	ut_a(page_validate(page, index));
	ut_a(page_get_n_recs(page) == 0);

	mtr_commit(&mtr);
	mem_heap_free(heap);
}

/*********************************************************************
Test for index page. */

void 
test2(void)
/*=======*/
{
	page_t*		page;
	dtuple_t*	tuple;
	mem_heap_t*	heap;
	ulint		i, j;
	ulint		rnd	= 0;
	rec_t*		rec;
	page_cur_t	cursor;
	dict_index_t*	index;	
	dict_table_t*	table;
	buf_block_t*	block;
	buf_frame_t*	frame;
	ulint		tm, oldtm;
	byte		buf[8];
	mtr_t		mtr;
	mtr_t		mtr2;
	
	printf("-------------------------------------------------\n");
	printf("TEST 2. Speed test\n");
	
	oldtm = ut_clock();
	
	for (i = 0; i < 1000 * UNIV_DBC * UNIV_DBC; i++) {
		ut_memcpy(bigbuf, bigbuf + 800, 800);
	}

	tm = ut_clock();
	printf("Wall time for %lu mem copys of 800 bytes %lu millisecs\n",
			i, tm - oldtm);

	oldtm = ut_clock();

	rnd = 0;
	for (i = 0; i < 1000 * UNIV_DBC * UNIV_DBC; i++) {
		ut_memcpy(bigbuf + rnd, bigbuf + rnd + 800, 800);
		rnd += 1600;
		if (rnd > 995000) {
			rnd = 0;
		}
	}

	tm = ut_clock();
	printf("Wall time for %lu mem copys of 800 bytes %lu millisecs\n",
			i, tm - oldtm);
			
	heap = mem_heap_create(0);
	
	table = dict_table_create("TS_TABLE2", 2);

	dict_table_add_col(table, "COL1", DATA_VARCHAR, DATA_ENGLISH, 10, 0);
	dict_table_add_col(table, "COL2", DATA_VARCHAR, DATA_ENGLISH, 10, 0);

	ut_a(0 == dict_table_publish(table));

	index = dict_index_create("TS_TABLE2", "IND2", 0, 2, 0);

	dict_index_add_field(index, "COL1", 0);
	dict_index_add_field(index, "COL2", 0);

	ut_a(0 == dict_index_publish(index));

	index = dict_index_get("TS_TABLE2", "IND2");
	ut_a(index);
	
	tuple = dtuple_create(heap, 3);

	oldtm = ut_clock();
	
	rnd = 677;
	for (i = 0; i < 5 * UNIV_DBC * UNIV_DBC; i++) {

		mtr_start(&mtr);

		block = buf_page_create(0, 5, &mtr);
		buf_page_x_lock(block, &mtr);

		frame = buf_block_get_frame(block);

		page = page_create(frame, &mtr);

		for (j = 0; j < 200; j++) {
			rnd = (rnd + 54841) % 1000;
			dtuple_gen_test_tuple3(tuple, rnd, buf);
			page_cur_search(page, tuple, PAGE_CUR_G, &cursor);

			rec = page_cur_insert_rec(&cursor, tuple, NULL, &mtr);
			ut_a(rec);
		}
		mtr_commit(&mtr);
	}

	tm = ut_clock();
	printf("Wall time for insertion of %lu recs %lu milliseconds\n",
			i * j, tm - oldtm);

	mtr_start(&mtr);

	block = buf_page_get(0, 5, &mtr);
	buf_page_s_lock(block, &mtr);

	page = buf_block_get_frame(block);
	ut_a(page_validate(page, index));
	mtr_commit(&mtr);
	
	oldtm = ut_clock();
	
	rnd = 677;
	for (i = 0; i < 5 * UNIV_DBC * UNIV_DBC; i++) {
		mtr_start(&mtr);

		block = buf_page_create(0, 5, &mtr);
		buf_page_x_lock(block, &mtr);

		frame = buf_block_get_frame(block);

		page = page_create(frame, &mtr);

		for (j = 0; j < 200; j++) {
			rnd = (rnd + 54841) % 1000;
			dtuple_gen_test_tuple3(tuple, rnd, buf);
		}
		mtr_commit(&mtr);
	}

	tm = ut_clock();
	printf(
	"Wall time for %lu empty loops with page create %lu milliseconds\n",
			i * j, tm - oldtm);

	oldtm = ut_clock();
	
	for (i = 0; i < 5 * UNIV_DBC * UNIV_DBC; i++) {

	
		mtr_start(&mtr);

		block = buf_page_create(0, 5, &mtr);
		buf_page_x_lock(block, &mtr);

		frame = buf_block_get_frame(block);

		page = page_create(frame, &mtr);

		mtr_commit(&mtr);

		rnd = 100;
		for (j = 0; j < 200; j++) {
			mtr_start(&mtr2);

			block = buf_page_get(0, 5, &mtr2);
			buf_page_x_lock(block, &mtr2);

			page = buf_block_get_frame(block);

			rnd = (rnd + 1) % 1000;
			dtuple_gen_test_tuple3(tuple, rnd, buf);
			page_cur_search(page, tuple, PAGE_CUR_G, &cursor);

			rec = page_cur_insert_rec(&cursor, tuple, NULL, &mtr2);
			ut_a(rec);

			mtr_commit(&mtr2);
		}
	}

	tm = ut_clock();
	printf(
	"Wall time for sequential insertion of %lu recs %lu milliseconds\n",
			i * j, tm - oldtm);

	mtr_start(&mtr);

	block = buf_page_get(0, 5, &mtr);
	buf_page_s_lock(block, &mtr);

	page = buf_block_get_frame(block);
	ut_a(page_validate(page, index));
	mtr_commit(&mtr);

	oldtm = ut_clock();
	
	for (i = 0; i < 5 * UNIV_DBC * UNIV_DBC; i++) {
		mtr_start(&mtr);

		block = buf_page_create(0, 5, &mtr);
		buf_page_x_lock(block, &mtr);

		frame = buf_block_get_frame(block);

		page = page_create(frame, &mtr);

		rnd = 500;
		for (j = 0; j < 200; j++) {
			rnd = (rnd - 1) % 1000;
			dtuple_gen_test_tuple3(tuple, rnd, buf);
			page_cur_search(page, tuple, PAGE_CUR_G, &cursor);

			rec = page_cur_insert_rec(&cursor, tuple, NULL, &mtr);
			ut_a(rec);
		}
		mtr_commit(&mtr);
	}

	tm = ut_clock();
	printf(
	"Wall time for descend. seq. insertion of %lu recs %lu milliseconds\n",
			i * j, tm - oldtm);

	oldtm = ut_clock();
	
	for (i = 0; i < 5 * UNIV_DBC * UNIV_DBC; i++) {
		mtr_start(&mtr);

		block = buf_page_create(0, 5, &mtr);
		buf_page_x_lock(block, &mtr);

		frame = buf_block_get_frame(block);

		page = page_create(frame, &mtr);

		rnd = 677;

		for (j = 0; j < 200; j++) {
			rnd = (rnd + 54841) % 1000;
			dtuple_gen_test_tuple3(tuple, rnd, buf);
			page_cur_search(page, tuple, PAGE_CUR_G, &cursor);

			rec = page_cur_insert_rec(&cursor, tuple, NULL, &mtr);
			ut_a(rec);
		}

		rnd = 677;
		for (j = 0; j < 200; j++) {
			rnd = (rnd + 54841) % 1000;
			dtuple_gen_test_tuple3(tuple, rnd, buf);
			page_cur_search(page, tuple, PAGE_CUR_G, &cursor);

			page_cur_delete_rec(&cursor, &mtr);
		}
		ut_a(page_get_n_recs(page) == 0);

		mtr_commit(&mtr);
	}

	tm = ut_clock();
	printf("Wall time for insert and delete of %lu recs %lu milliseconds\n",
			i * j, tm - oldtm);

	mtr_start(&mtr);

	block = buf_page_create(0, 5, &mtr);
	buf_page_x_lock(block, &mtr);

	frame = buf_block_get_frame(block);

	page = page_create(frame, &mtr);

	rnd = 677;

	for (j = 0; j < 200; j++) {
		rnd = (rnd + 54841) % 1000;
		dtuple_gen_test_tuple3(tuple, rnd, buf);
		page_cur_search(page, tuple, PAGE_CUR_G, &cursor);

		rec = page_cur_insert_rec(&cursor, tuple, NULL, &mtr);
		ut_a(rec);
	}
	ut_a(page_validate(page, index));
	mtr_print(&mtr);
	
	oldtm = ut_clock();
	
	for (i = 0; i < 5 * UNIV_DBC * UNIV_DBC; i++) {
		rnd = 677;
		for (j = 0; j < 200; j++) {
/*			rnd = (rnd + 54841) % 1000;*/
			dtuple_gen_test_tuple3(tuple, rnd, buf);
			page_cur_search(page, tuple, PAGE_CUR_G, &cursor);
		}
	}

	tm = ut_clock();
	printf("Wall time for search of %lu recs %lu milliseconds\n",
			i * j, tm - oldtm);

	oldtm = ut_clock();
	
	for (i = 0; i < 5 * UNIV_DBC * UNIV_DBC; i++) {
		rnd = 677;
		for (j = 0; j < 200; j++) {
			rnd = (rnd + 54841) % 1000;
			dtuple_gen_test_tuple3(tuple, rnd, buf);
		}
	}

	tm = ut_clock();
	printf("Wall time for %lu empty loops %lu milliseconds\n",
			i * j, tm - oldtm);
	mtr_commit(&mtr);
}

/********************************************************************
Main test function. */

void 
main(void) 
/*======*/
{
	ulint	tm, oldtm;

	sync_init();
	mem_init();
	os_aio_init(160, 5);
	fil_init(25);
	buf_pool_init(100, 100);
	dict_init();
	log_init();
	
	create_files();
	
	oldtm = ut_clock();
	
	ut_rnd_set_seed(19);

	test1();
	test2();

	mem_print_info();
	
	tm = ut_clock();
	printf("Wall time for test %lu milliseconds\n", tm - oldtm);
	printf("TESTS COMPLETED SUCCESSFULLY!\n");
} 
