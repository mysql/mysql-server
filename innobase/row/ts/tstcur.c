/************************************************************************
Test for the index system

(c) 1994-1996 Innobase Oy

Created 2/16/1996 Heikki Tuuri
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
#include "fsp0fsp.h"
#include "rem0rec.h"
#include "rem0cmp.h"
#include "mtr0mtr.h"
#include "log0log.h"
#include "page0page.h"
#include "page0cur.h"
#include "trx0trx.h"
#include "dict0boot.h"
#include "trx0sys.h"
#include "dict0crea.h"
#include "btr0btr.h"
#include "btr0pcur.h"
#include "rem0rec.h"
#include "..\tcur0ins.h"

os_file_t	files[1000];

mutex_t		ios_mutex;
ulint		ios;
ulint		n[10];

mutex_t		incs_mutex;
ulint		incs;

byte		bigbuf[1000000];

#define N_SPACES	1
#define N_FILES		1
#define FILE_SIZE	4000 	/* must be > 512 */
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

	strcpy(name, "tsfile00");

	for (k = 0; k < N_SPACES; k++) {
	for (i = 0; i < N_FILES; i++) {

		name[6] = (char)((ulint)'0' + k);
		name[7] = (char)((ulint)'0' + i);
	
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

/************************************************************************
Inits space header of space 0. */

void
init_space(void)
/*============*/
{
	mtr_t		mtr;

	printf("Init space header\n");
	
	mtr_start(&mtr);

	fsp_header_init(0, FILE_SIZE * N_FILES, &mtr);		

	mtr_commit(&mtr);
}

/*********************************************************************
Test for index page. */

void 
test1(void)
/*=======*/
{
	dtuple_t*	tuple;
	mem_heap_t*	heap;
	mem_heap_t*	heap2;
	ulint		rnd	= 0;
	dict_index_t*	index;	
	dict_table_t*	table;
	byte		buf[16];
	ulint		i, j;
	ulint		tm, oldtm;
	trx_t*		trx;
/*	dict_tree_t*	tree;*/
	btr_pcur_t	pcur;
	btr_pcur_t	pcur2;
	mtr_t		mtr;
	mtr_t		mtr2;
	byte*		field;
	ulint		len;
	dtuple_t*	search_tuple;
	dict_tree_t*	index_tree;
	rec_t*		rec;

	UT_NOT_USED(len);
	UT_NOT_USED(field);
	UT_NOT_USED(pcur2);
/*	
	printf("\n\n\nPress 2 x enter to start test\n");
	
	while (EOF == getchar()) {
		
	}

	getchar();
*/	
	printf("-------------------------------------------------\n");
	printf("TEST 1. CREATE TABLE WITH 3 COLUMNS AND WITH 3 INDEXES\n");

	heap = mem_heap_create(1024);
	heap2 = mem_heap_create(1024);

	trx = trx_start(ULINT_UNDEFINED);	

	table = dict_mem_table_create("TS_TABLE1", 0, 3);

	dict_mem_table_add_col(table, "COL1", DATA_VARCHAR,
						DATA_ENGLISH, 10, 0);
	dict_mem_table_add_col(table, "COL2", DATA_VARCHAR,
						DATA_ENGLISH, 10, 0);
	dict_mem_table_add_col(table, "COL3", DATA_VARCHAR,
						DATA_ENGLISH, 100, 0);

	ut_a(TRUE == dict_create_table(table, trx));

	index = dict_mem_index_create("TS_TABLE1", "IND1", 75046,
							DICT_CLUSTERED, 2);

	dict_mem_index_add_field(index, "COL1", 0);
	dict_mem_index_add_field(index, "COL2", 0);

	ut_a(mem_heap_validate(index->heap));
	
	ut_a(TRUE == dict_create_index(index, trx));

	trx_commit(trx);

	trx = trx_start(ULINT_UNDEFINED);	

	index = dict_mem_index_create("TS_TABLE1", "IND2", 0, DICT_UNIQUE, 1);

	dict_mem_index_add_field(index, "COL2", 0);

	ut_a(mem_heap_validate(index->heap));
	
	ut_a(TRUE == dict_create_index(index, trx));

	trx_commit(trx);

	trx = trx_start(ULINT_UNDEFINED);	

	index = dict_mem_index_create("TS_TABLE1", "IND3", 0, DICT_UNIQUE, 1);

	dict_mem_index_add_field(index, "COL2", 0);

	ut_a(mem_heap_validate(index->heap));
	
	ut_a(TRUE == dict_create_index(index, trx));

	trx_commit(trx);
/*
	tree = dict_index_get_tree(dict_table_get_first_index(table));

	btr_print_tree(tree, 10);
*/
	dict_table_print(table);

	/*---------------------------------------------------------*/
/*
	printf("\n\n\nPress 2 x enter to continue test\n");
	
	while (EOF == getchar()) {
		
	}
	getchar();
*/	
	printf("-------------------------------------------------\n");
	printf("TEST 2. INSERT 1 ROW TO THE TABLE\n");

	trx = trx_start(ULINT_UNDEFINED);
	
	tuple = dtuple_create(heap, 3);

	table = dict_table_get("TS_TABLE1", trx);

	dtuple_gen_test_tuple3(tuple, 0, buf);
	tcur_insert(tuple, table, heap2, trx);

	trx_commit(trx);
/*
	tree = dict_index_get_tree(dict_table_get_first_index(table));

	btr_print_tree(tree, 10);
*/
/*
	printf("\n\n\nPress 2 x enter to continue test\n");
	
	while (EOF == getchar()) {
		
	}
	getchar();
*/	
	printf("-------------------------------------------------\n");
	printf("TEST 3. INSERT MANY ROWS TO THE TABLE IN A SINGLE TRX\n");

	rnd = 0;
	oldtm = ut_clock();
	
	trx = trx_start(ULINT_UNDEFINED);
	for (i = 0; i < 300 * UNIV_DBC * UNIV_DBC; i++) {

		if (i % 5000 == 0) {
			/* dict_table_print(table);
			buf_print();
			buf_LRU_print();
			printf("%lu rows inserted\n", i); */
		}

		table = dict_table_get("TS_TABLE1", trx);
		
		if (i == 2180) {
			rnd = rnd % 200000;
		}

		rnd = (rnd + 1) % 200000;
		
		dtuple_gen_test_tuple3(tuple, rnd, buf);

		tcur_insert(tuple, table, heap2, trx);

		mem_heap_empty(heap2);

		if (i % 4 == 3) {
		}
	}
	trx_commit(trx);

	tm = ut_clock();
	printf("Wall time for test %lu milliseconds\n", tm - oldtm);
	printf("%lu rows inserted\n", i);
/*
	printf("\n\n\nPress 2 x enter to continue test\n");
	
	while (EOF == getchar()) {
		
	}
	getchar();
*/	
	printf("-------------------------------------------------\n");
	printf("TEST 4. PRINT PART OF CONTENTS OF EACH INDEX TREE\n");

/*
	mem_print_info();
*/

/*
	tree = dict_index_get_tree(dict_table_get_first_index(table));

	btr_print_tree(tree, 10);

	tree = dict_index_get_tree(dict_table_get_next_index(
				dict_table_get_first_index(table)));

	btr_print_tree(tree, 5);
*/	
/*
	printf("\n\n\nPress 2 x enter to continue test\n");

	while (EOF == getchar()) {
		
	}
	getchar();
*/
/*	mem_print_info(); */

	os_thread_sleep(5000000);

   for (j = 0; j < 5; j++) {
	printf("-------------------------------------------------\n");
	printf("TEST 5. CALCULATE THE JOIN OF THE TABLE WITH ITSELF\n");

	i = 0;

	oldtm = ut_clock();

	mtr_start(&mtr);

	index_tree = dict_index_get_tree(UT_LIST_GET_FIRST(table->indexes));

	search_tuple = dtuple_create(heap, 2);

	dtuple_gen_search_tuple3(search_tuple, i, buf);
	
	btr_pcur_open(index_tree, search_tuple, PAGE_CUR_GE,
						BTR_SEARCH_LEAF, &pcur, &mtr);

	ut_a(btr_pcur_move_to_next(&pcur, &mtr));

	while (!btr_pcur_is_after_last_in_tree(&pcur, &mtr)) {
	
		if (i % 20000 == 0) {
			printf("%lu rows joined\n", i);
		}

		index_tree = dict_index_get_tree(
					UT_LIST_GET_FIRST(table->indexes));

		rec = btr_pcur_get_rec(&pcur);

		rec_copy_prefix_to_dtuple(search_tuple, rec, 2, heap2);

		mtr_start(&mtr2);		

		btr_pcur_open(index_tree, search_tuple, PAGE_CUR_GE,
						BTR_SEARCH_LEAF, &pcur2, &mtr2);

		btr_pcur_move_to_next(&pcur2, &mtr2);

		rec = btr_pcur_get_rec(&pcur2);

		field = rec_get_nth_field(rec, 1, &len);

		ut_a(len == 8);

		ut_a(ut_memcmp(field, dfield_get_data(
					dtuple_get_nth_field(search_tuple, 1)),
				len) == 0);
				
		btr_pcur_close(&pcur2, &mtr);

		mem_heap_empty(heap2);

		mtr_commit(&mtr2);

		btr_pcur_store_position(&pcur, &mtr);
		mtr_commit(&mtr);

		mtr_start(&mtr);

		btr_pcur_restore_position(BTR_SEARCH_LEAF, &pcur, &mtr);

		btr_pcur_move_to_next(&pcur, &mtr);
		i++;
	}

	btr_pcur_close(&pcur, &mtr);
	mtr_commit(&mtr);

	tm = ut_clock();
	printf("Wall time for test %lu milliseconds\n", tm - oldtm);
	printf("%lu rows joined\n", i);
   }

	oldtm = ut_clock();

/*
	printf("\n\n\nPress 2 x enter to continue test\n");
	
	while (EOF == getchar()) {
		
	}
	getchar();
*/	
	printf("-------------------------------------------------\n");
	printf("TEST 6. INSERT MANY ROWS TO THE TABLE IN SEPARATE TRXS\n");

	rnd = 200000;
	
	for (i = 0; i < 350; i++) {

		if (i % 4 == 0) {
		}
		trx = trx_start(ULINT_UNDEFINED);

		table = dict_table_get("TS_TABLE1", trx);
		
		if (i == 2180) {
			rnd = rnd % 200000;
		}

		rnd = (rnd + 1) % 200000;
		
		dtuple_gen_test_tuple3(tuple, rnd, buf);

		tcur_insert(tuple, table, heap2, trx);

		trx_commit(trx);

		mem_heap_empty(heap2);
		if (i % 4 == 3) {
		}
	}

	tm = ut_clock();
	printf("Wall time for test %lu milliseconds\n", tm - oldtm);
	printf("%lu rows inserted in %lu transactions\n", i, i);
/*
	printf("\n\n\nPress 2 x enter to continue test\n");
	
	while (EOF == getchar()) {
		
	}
	getchar();
*/	
	printf("-------------------------------------------------\n");
	printf("TEST 7. PRINT MEMORY ALLOCATION INFO\n");

	mem_print_info();
/*
	printf("\n\n\nPress 2 x enter to continue test\n");
	
	while (EOF == getchar()) {
		
	}
	getchar();
*/	
	printf("-------------------------------------------------\n");
	printf("TEST 8. PRINT SEMAPHORE INFO\n");

	sync_print();

	

#ifdef notdefined
	rnd = 90000;

	oldtm = ut_clock();
	
	for (i = 0; i < 1000 * UNIV_DBC * UNIV_DBC; i++) {
	
		mtr_start(&mtr);

		if (i == 50000) {
			rnd = rnd % 200000;
		}

		rnd = (rnd + 595659561) % 200000;
		
		dtuple_gen_test_tuple3(tuple, rnd, buf);

		btr_pcur_open(tree, tuple, PAGE_CUR_GE,
					BTR_SEARCH_LEAF, &cursor, &mtr);

		mtr_commit(&mtr);
	}

	tm = ut_clock();
	printf("Wall time for test %lu milliseconds\n", tm - oldtm);

	rnd = 0;

	oldtm = ut_clock();
	
	for (i = 0; i < 1000 * UNIV_DBC * UNIV_DBC; i++) {
	
		mtr_start(&mtr);

		rnd = (rnd + 35608971) % 200000 + 1;
		
		dtuple_gen_test_tuple3(tuple, rnd, buf);

		mtr_commit(&mtr);
	}

	tm = ut_clock();
	printf("Wall time for test %lu milliseconds\n", tm - oldtm);
	
/*	btr_print_tree(tree, 3); */

#endif
	mem_heap_free(heap);
}


#ifdef notdefined
	
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
	
	tuple = dtuple_create(heap, 2);

	oldtm = ut_clock();
	
	rnd = 677;
	for (i = 0; i < 4 * UNIV_DBC * UNIV_DBC; i++) {

		mtr_start(&mtr);

		block = buf_page_create(0, 5, &mtr);
		buf_page_x_lock(block, &mtr);

		frame = buf_block_get_frame(block);

		page = page_create(frame, &mtr);

		for (j = 0; j < 250; j++) {
			rnd = (rnd + 54841) % 1000;
			dtuple_gen_test_tuple2(tuple, rnd, buf);
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
	for (i = 0; i < 4 * UNIV_DBC * UNIV_DBC; i++) {
		mtr_start(&mtr);

		block = buf_page_create(0, 5, &mtr);
		buf_page_x_lock(block, &mtr);

		frame = buf_block_get_frame(block);

		page = page_create(frame, &mtr);

		for (j = 0; j < 250; j++) {
			rnd = (rnd + 54841) % 1000;
			dtuple_gen_test_tuple2(tuple, rnd, buf);
		}
		mtr_commit(&mtr);
	}

	tm = ut_clock();
	printf(
	"Wall time for %lu empty loops with page create %lu milliseconds\n",
			i * j, tm - oldtm);

	oldtm = ut_clock();
	
	for (i = 0; i < 4 * UNIV_DBC * UNIV_DBC; i++) {

		mtr_start(&mtr);

		block = buf_page_create(0, 5, &mtr);
		buf_page_x_lock(block, &mtr);

		frame = buf_block_get_frame(block);

		page = page_create(frame, &mtr);

		rnd = 100;
		for (j = 0; j < 250; j++) {
			rnd = (rnd + 1) % 1000;
			dtuple_gen_test_tuple2(tuple, rnd, buf);
			page_cur_search(page, tuple, PAGE_CUR_G, &cursor);

			rec = page_cur_insert_rec(&cursor, tuple, NULL, &mtr);
			ut_a(rec);
		}
		mtr_commit(&mtr);
	}

	tm = ut_clock();
	printf(
	"Wall time for sequential insertion of %lu recs %lu milliseconds\n",
			i * j, tm - oldtm);


	oldtm = ut_clock();
	
	for (i = 0; i < 4 * UNIV_DBC * UNIV_DBC; i++) {
		mtr_start(&mtr);

		block = buf_page_create(0, 5, &mtr);
		buf_page_x_lock(block, &mtr);

		frame = buf_block_get_frame(block);

		page = page_create(frame, &mtr);

		rnd = 500;
		for (j = 0; j < 250; j++) {
			rnd = (rnd - 1) % 1000;
			dtuple_gen_test_tuple2(tuple, rnd, buf);
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
	
	for (i = 0; i < 4 * UNIV_DBC * UNIV_DBC; i++) {
		mtr_start(&mtr);

		block = buf_page_create(0, 5, &mtr);
		buf_page_x_lock(block, &mtr);

		frame = buf_block_get_frame(block);

		page = page_create(frame, &mtr);

		rnd = 677;

		for (j = 0; j < 250; j++) {
			rnd = (rnd + 54841) % 1000;
			dtuple_gen_test_tuple2(tuple, rnd, buf);
			page_cur_search(page, tuple, PAGE_CUR_G, &cursor);

			rec = page_cur_insert_rec(&cursor, tuple, NULL, &mtr);
			ut_a(rec);
		}

		rnd = 677;
		for (j = 0; j < 250; j++) {
			rnd = (rnd + 54841) % 1000;
			dtuple_gen_test_tuple2(tuple, rnd, buf);
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

	for (j = 0; j < 250; j++) {
		rnd = (rnd + 54841) % 1000;
		dtuple_gen_test_tuple2(tuple, rnd, buf);
		page_cur_search(page, tuple, PAGE_CUR_G, &cursor);

		rec = page_cur_insert_rec(&cursor, tuple, NULL, &mtr);
		ut_a(rec);
	}
	ut_a(page_validate(page, index));
	mtr_print(&mtr);
	
	oldtm = ut_clock();
	
	for (i = 0; i < 4 * UNIV_DBC * UNIV_DBC; i++) {
		rnd = 677;
		for (j = 0; j < 250; j++) {
			rnd = (rnd + 54841) % 1000;
			dtuple_gen_test_tuple2(tuple, rnd, buf);
			page_cur_search(page, tuple, PAGE_CUR_G, &cursor);
		}
	}

	tm = ut_clock();
	printf("Wall time for search of %lu recs %lu milliseconds\n",
			i * j, tm - oldtm);

	oldtm = ut_clock();
	
	for (i = 0; i < 4 * UNIV_DBC * UNIV_DBC; i++) {
		rnd = 677;
		for (j = 0; j < 250; j++) {
			rnd = (rnd + 54841) % 1000;
			dtuple_gen_test_tuple2(tuple, rnd, buf);
		}
	}

	tm = ut_clock();
	printf("Wall time for %lu empty loops %lu milliseconds\n",
			i * j, tm - oldtm);
	mtr_commit(&mtr);
}

#endif

/********************************************************************
Main test function. */

void 
main(void) 
/*======*/
{
	ulint	tm, oldtm;
	mtr_t	mtr;

	sync_init();
	mem_init();
	os_aio_init(160, 5);
	fil_init(25);
	buf_pool_init(POOL_SIZE, POOL_SIZE);
	fsp_init();
	log_init();

	create_files();
	init_space();

	mtr_start(&mtr);

	trx_sys_create(&mtr);
	dict_create(&mtr);

	mtr_commit(&mtr);

	
	oldtm = ut_clock();
	
	ut_rnd_set_seed(19);

	test1();

/*	mem_print_info(); */
	
	tm = ut_clock();
	printf("Wall time for test %lu milliseconds\n", tm - oldtm);
	printf("TESTS COMPLETED SUCCESSFULLY!\n");
} 
