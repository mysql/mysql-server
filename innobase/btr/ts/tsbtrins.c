/************************************************************************
Test for the B-tree

(c) 1994-1997 Innobase Oy

Created 2/16/1996 Heikki Tuuri
*************************************************************************/

#include "os0proc.h"
#include "sync0sync.h"
#include "ut0mem.h"
#include "mem0mem.h"
#include "mem0pool.h"
#include "data0data.h"
#include "data0type.h"
#include "dict0dict.h"
#include "buf0buf.h"
#include "os0file.h"
#include "os0thread.h"
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
#include "btr0cur.h"
#include "btr0sea.h"
#include "rem0rec.h"
#include "srv0srv.h"
#include "que0que.h"
#include "com0com.h"
#include "usr0sess.h"
#include "lock0lock.h"
#include "trx0roll.h"
#include "trx0purge.h"
#include "row0ins.h"
#include "row0upd.h"
#include "row0row.h"
#include "row0del.h"
#include "lock0lock.h"
#include "ibuf0ibuf.h"

os_file_t	files[1000];

mutex_t		ios_mutex;
ulint		ios;
ulint		n[10];

mutex_t		incs_mutex;
ulint		incs;

#define N_SPACES	2	/* must be >= 2 */
#define N_FILES		1
#define FILE_SIZE	8096 	/* must be > 512 */
#define POOL_SIZE	1024
#define	IBUF_SIZE	200
#define	COUNTER_OFFSET	1500

#define LOOP_SIZE	150
#define	N_THREADS	5

#define	COUNT		1

ulint zero = 0;

buf_block_t*	bl_arr[POOL_SIZE];

ulint	dummy = 0;

byte	test_buf[8000];

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
Creates the files for the file system test and inserts them to the file
system. */

void
create_files(void)
/*==============*/
{
	bool		ret;
	ulint		i, k;
	char		name[20];
	os_thread_t	thr[10];
	os_thread_id_t	id[10];

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
		} else {
			if (k == 1) {
				ut_a(os_file_set_size(files[i],
						8192 * IBUF_SIZE, 0));
			} else {
				ut_a(os_file_set_size(files[i],
						8192 * FILE_SIZE, 0));
			}
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
	mutex_set_level(&ios_mutex, SYNC_NO_ORDER_CHECK);
	
	for (i = 0; i < 9; i++) {
		n[i] = i;

		thr[i] = os_thread_create(handler_thread, n + i, id + i);
	}
}

/************************************************************************
Inits space headers of spaces 0 and 1. */

void
init_spaces(void)
/*=============*/
{
	mtr_t	mtr;

	mtr_start(&mtr);

	fsp_header_init(0, FILE_SIZE * N_FILES, &mtr);		
	fsp_header_init(1, IBUF_SIZE, &mtr);		

	mtr_commit(&mtr);
}

/*********************************************************************
Test for table creation. */

ulint
test1(
/*==*/
	void*	arg)
{
	sess_t*		sess;
	com_endpoint_t*	com_endpoint;
	mem_heap_t*	heap;
	dict_index_t*	index;	
	dict_table_t*	table;
	que_fork_t*	fork;
	que_thr_t*	thr;
	trx_t*		trx;

	UT_NOT_USED(arg);

	printf("-------------------------------------------------\n");
	printf("TEST 1. CREATE TABLE WITH 3 COLUMNS AND WITH 3 INDEXES\n");

	heap = mem_heap_create(512);

	com_endpoint = (com_endpoint_t*)heap;	/* This is a dummy non-NULL
						value */
	mutex_enter(&kernel_mutex);	

	sess = sess_open(ut_dulint_zero, com_endpoint, (byte*)"user1", 6);
	
	trx = sess->trx;

	mutex_exit(&kernel_mutex);

	ut_a(trx_start(trx, ULINT_UNDEFINED));	

	table = dict_mem_table_create("TS_TABLE1", 0, 3);

	dict_mem_table_add_col(table, "COL1", DATA_VARCHAR,
						DATA_ENGLISH, 10, 0);
	dict_mem_table_add_col(table, "COL2", DATA_VARCHAR,
						DATA_ENGLISH, 10, 0);
	dict_mem_table_add_col(table, "COL3", DATA_VARCHAR,
						DATA_ENGLISH, 100, 0);
	/*------------------------------------*/
	/* CREATE TABLE */

	fork = que_fork_create(NULL, NULL, QUE_FORK_EXECUTE, heap);
	fork->trx = trx;

	thr = que_thr_create(fork, fork, heap);

	thr->child = tab_create_graph_create(fork, thr, table, heap);

	mutex_enter(&kernel_mutex);	

	que_graph_publish(fork, trx->sess);

	trx->graph = fork;

	ut_a(thr == que_fork_start_command(fork, SESS_COMM_EXECUTE, 0));
	
	mutex_exit(&kernel_mutex);

	que_run_threads(thr);

/*	dict_table_print_by_name("SYS_TABLES");
	dict_table_print_by_name("SYS_COLUMNS"); */
	/*-------------------------------------*/
	/* CREATE CLUSTERED INDEX */
	
	index = dict_mem_index_create("TS_TABLE1", "IND1", 0,
					DICT_UNIQUE | DICT_CLUSTERED, 1);
	dict_mem_index_add_field(index, "COL1", 0);

	ut_a(mem_heap_validate(index->heap));

	fork = que_fork_create(NULL, NULL, QUE_FORK_EXECUTE, heap);
	fork->trx = trx;

	thr = que_thr_create(fork, fork, heap);

	thr->child = ind_create_graph_create(fork, thr, index, heap);

	mutex_enter(&kernel_mutex);	

	que_graph_publish(fork, trx->sess);

	trx->graph = fork;

	ut_a(thr == que_fork_start_command(fork, SESS_COMM_EXECUTE, 0));
	
	mutex_exit(&kernel_mutex);

	que_run_threads(thr);

/*	dict_table_print_by_name("SYS_INDEXES");
	dict_table_print_by_name("SYS_FIELDS"); */

	/*-------------------------------------*/
	/* CREATE SECONDARY INDEX */
	
	index = dict_mem_index_create("TS_TABLE1", "IND2", 0, 0, 1);

	dict_mem_index_add_field(index, "COL2", 0);

	ut_a(mem_heap_validate(index->heap));

	fork = que_fork_create(NULL, NULL, QUE_FORK_EXECUTE, heap);
	fork->trx = trx;

	thr = que_thr_create(fork, fork, heap);

	thr->child = ind_create_graph_create(fork, thr, index, heap);

	mutex_enter(&kernel_mutex);	

	que_graph_publish(fork, trx->sess);

	trx->graph = fork;

	ut_a(thr == que_fork_start_command(fork, SESS_COMM_EXECUTE, 0));
	
	mutex_exit(&kernel_mutex);

	que_run_threads(thr);

/*	dict_table_print_by_name("SYS_INDEXES");
	dict_table_print_by_name("SYS_FIELDS"); */

	/*-------------------------------------*/
	/* CREATE ANOTHER SECONDARY INDEX */
	
	index = dict_mem_index_create("TS_TABLE1", "IND3", 0, 0, 1);

	dict_mem_index_add_field(index, "COL2", 0);

	ut_a(mem_heap_validate(index->heap));

	fork = que_fork_create(NULL, NULL, QUE_FORK_EXECUTE, heap);
	fork->trx = trx;

	thr = que_thr_create(fork, fork, heap);

	thr->child = ind_create_graph_create(fork, thr, index, heap);

	mutex_enter(&kernel_mutex);	

	que_graph_publish(fork, trx->sess);

	trx->graph = fork;

	ut_a(thr == que_fork_start_command(fork, SESS_COMM_EXECUTE, 0));
	
	mutex_exit(&kernel_mutex);

	que_run_threads(thr);

#ifdef notdefined
	/*-------------------------------------*/
	/* CREATE YET ANOTHER SECONDARY INDEX */
	
	index = dict_mem_index_create("TS_TABLE1", "IND4", 0, 0, 1);

	dict_mem_index_add_field(index, "COL2", 0);

	ut_a(mem_heap_validate(index->heap));

	fork = que_fork_create(NULL, NULL, QUE_FORK_EXECUTE, heap);
	fork->trx = trx;

	thr = que_thr_create(fork, fork, heap);

	thr->child = ind_create_graph_create(fork, thr, index, heap);

	mutex_enter(&kernel_mutex);	

	que_graph_publish(fork, trx->sess);

	trx->graph = fork;

	ut_a(thr == que_fork_start_command(fork, SESS_COMM_EXECUTE, 0));
	
	mutex_exit(&kernel_mutex);

	que_run_threads(thr);
#endif
/*	dict_table_print_by_name("SYS_INDEXES");
	dict_table_print_by_name("SYS_FIELDS"); */

	return(0);
}

/*********************************************************************
Another test for inserts. */

ulint
test2_1(
/*====*/
	void*	arg)
{
	ulint		tm, oldtm;
	sess_t*		sess;
	com_endpoint_t*	com_endpoint;
	mem_heap_t*	heap;
	que_fork_t*	fork;
	dict_table_t*	table;
	que_thr_t*	thr;
	trx_t*		trx;
	ulint		i;
	byte		buf[100];
	ins_node_t*	node;
	ulint		count = 0;
	ulint		rnd;
	dtuple_t*	row;
	dict_index_t*	index;
/*	dict_tree_t*	tree;
	dtuple_t*	entry;
	btr_pcur_t	pcur;
	mtr_t		mtr; */
	
	printf("-------------------------------------------------\n");
	printf("TEST 2.1. MASSIVE ASCENDING INSERT\n");

	heap = mem_heap_create(512);

	com_endpoint = (com_endpoint_t*)heap;	/* This is a dummy non-NULL
						value */
	mutex_enter(&kernel_mutex);	

	sess = sess_open(ut_dulint_zero, com_endpoint, (byte*)"user1", 6);
	
	trx = sess->trx;

	mutex_exit(&kernel_mutex);
loop:
	ut_a(trx_start(trx, ULINT_UNDEFINED));	

	/*-------------------------------------*/
	/* MASSIVE INSERT */
	fork = que_fork_create(NULL, NULL, QUE_FORK_INSERT, heap);
	fork->trx = trx;

	thr = que_thr_create(fork, fork, heap);

	table = dict_table_get("TS_TABLE1", trx);

	row = dtuple_create(heap, 3 + DATA_N_SYS_COLS);

	dict_table_copy_types(row, table);

	node = ins_node_create(fork, thr, row, table, heap);
	
	thr->child = node;

	row_ins_init_sys_fields_at_sql_compile(node->row, node->table, heap);
	row_ins_init_sys_fields_at_sql_prepare(node->row, node->table, trx);

	node->init_all_sys_fields = FALSE;

	mutex_enter(&kernel_mutex);	

	que_graph_publish(fork, trx->sess);

	trx->graph = fork;

	mutex_exit(&kernel_mutex);

	rnd = 0;

	log_print();
	
	oldtm = ut_clock();

	for (i = 0; i < *((ulint*)arg); i++) {

		dtuple_gen_test_tuple3(row, rnd, DTUPLE_TEST_FIXED30, buf);

		mutex_enter(&kernel_mutex);	

		ut_a(
		thr == que_fork_start_command(fork, SESS_COMM_EXECUTE, 0));

		mutex_exit(&kernel_mutex);

		que_run_threads(thr);

		if (i % 5000 == 0) {
			/* ibuf_print(); */
			/* buf_print(); */

			/* buf_print_io(); */
			/*
			tm = ut_clock();
			printf("Wall time for %lu inserts %lu milliseconds\n",
							i, tm - oldtm); */
		}

		rnd = rnd + 1;
	}

	tm = ut_clock();
	printf("Wall time for %lu inserts %lu milliseconds\n", i, tm - oldtm);

	log_print();

/*	dict_table_print_by_name("TS_TABLE1"); */

/*	ibuf_print(); */

	index = index;

	index = dict_table_get_first_index(table);

	if (zero) {
		btr_search_index_print_info(index);
	}

	btr_validate_tree(dict_index_get_tree(index));
	
#ifdef notdefined
	index = dict_table_get_next_index(index);

	if (zero) {
		btr_search_index_print_info(index);
	}

	btr_validate_tree(dict_index_get_tree(index));

	index = dict_table_get_next_index(index);

/*	btr_search_index_print_info(index); */

	btr_validate_tree(dict_index_get_tree(index));

/*	dict_table_print_by_name("TS_TABLE1"); */

	/* Check inserted entries */

	btr_search_print_info();

	entry = dtuple_create(heap, 1);
	dtuple_gen_search_tuple3(entry, 0, buf);

	mtr_start(&mtr);
	
	index = dict_table_get_first_index(table);
	tree = dict_index_get_tree(index);

	btr_pcur_open(index, entry, PAGE_CUR_L, BTR_SEARCH_LEAF, &pcur, &mtr);
	ut_a(btr_pcur_is_before_first_in_tree(&pcur, &mtr));

	for (i = 0; i < *((ulint*)arg); i++) {
		ut_a(btr_pcur_move_to_next(&pcur, &mtr));

		dtuple_gen_search_tuple3(entry, i, buf);

		ut_a(0 == cmp_dtuple_rec(entry, btr_pcur_get_rec(&pcur)));
	}	

	ut_a(!btr_pcur_move_to_next(&pcur, &mtr));
	ut_a(btr_pcur_is_after_last_in_tree(&pcur, &mtr));

	btr_pcur_close(&pcur);
	mtr_commit(&mtr);
	
	printf("Validating tree\n");
	btr_validate_tree(tree);
	printf("Validated\n");
#endif
	/*-------------------------------------*/
	/* ROLLBACK */
	
#ifdef notdefined
/*	btr_validate_tree(tree); */

	fork = que_fork_create(NULL, NULL, QUE_FORK_EXECUTE, heap);
	fork->trx = trx;

	thr = que_thr_create(fork, fork, heap);

	thr->child = roll_node_create(fork, thr, heap);

	mutex_enter(&kernel_mutex);	

	que_graph_publish(fork, trx->sess);

	trx->graph = fork;

	ut_a(thr == que_fork_start_command(fork, SESS_COMM_EXECUTE, 0));
	
	mutex_exit(&kernel_mutex);

	oldtm = ut_clock();

	que_run_threads(thr);

	tm = ut_clock();
	printf("Wall time for rollback of %lu inserts %lu milliseconds\n",
		i, tm - oldtm);

	os_thread_sleep(1000000);

/*	dict_table_print_by_name("TS_TABLE1"); */

	dtuple_gen_search_tuple3(entry, 0, buf);

	mtr_start(&mtr);
	
	btr_pcur_open(index, entry, PAGE_CUR_L, BTR_SEARCH_LEAF, &pcur, &mtr);
	ut_a(btr_pcur_is_before_first_in_tree(&pcur, &mtr));

	ut_a(!btr_pcur_move_to_next(&pcur, &mtr));
	
	ut_a(btr_pcur_is_after_last_in_tree(&pcur, &mtr));

	btr_pcur_close(&pcur);
	mtr_commit(&mtr);

	btr_search_print_info();
#endif
	/*-------------------------------------*/
	/* COMMIT */
	fork = que_fork_create(NULL, NULL, QUE_FORK_EXECUTE, heap);
	fork->trx = trx;

	thr = que_thr_create(fork, fork, heap);

	thr->child = commit_node_create(fork, thr, heap);

	mutex_enter(&kernel_mutex);	

	que_graph_publish(fork, trx->sess);

	trx->graph = fork;

	ut_a(thr == que_fork_start_command(fork, SESS_COMM_EXECUTE, 0));
	
	mutex_exit(&kernel_mutex);

	oldtm = ut_clock();
	
	que_run_threads(thr);

	tm = ut_clock();
	printf("Wall time for commit %lu milliseconds\n", tm - oldtm);
	/*-------------------------------------*/

	count++;
/*	btr_validate_tree(tree); */

	if (count < 1) {
		goto loop;
	}

	mem_heap_free(heap);

	return(0);
}

/********************************************************************
Main test function. */

void 
main(void) 
/*======*/
{
	ulint	tm, oldtm;
	os_thread_id_t	id[10];
	ulint	n1000[10];
	ulint	i;
	ulint	n5000	= 500;
	ulint	n2;

/*	buf_debug_prints = TRUE; */
	log_do_write = TRUE;
	
	srv_boot("initfile");
	os_aio_init(576, 9, 100);
	fil_init(25);
	buf_pool_init(POOL_SIZE, POOL_SIZE);
	fsp_init();
	log_init();
	lock_sys_create(1024);

	create_files();

	init_spaces();

	sess_sys_init_at_db_start();
	
	trx_sys_create();
	
	dict_create();

/*	os_thread_sleep(500000); */

	oldtm = ut_clock();
	
	ut_rnd_set_seed(19);

	test1(NULL);
	
/*	for (i = 0; i < 2; i++) {

		n1000[i] = i;
		id[i] = id[i];

		os_thread_create(test10mt, n1000 + i, id + i);
	}
*/
	i = 4;
	
	n1000[i] = i;
	id[i] = id[i];

/*	os_thread_create(test10_4, n1000 + i, id + i); */

	i = 5;
	
/*	test10mt(&i);

	i = 6;

	test10mt(&i);

	trx_purge();
	printf("%lu pages purged\n", purge_sys->n_pages_handled);
	
	dict_table_print_by_name("TS_TABLE1"); */

/*	os_thread_create(test_measure_cont, &n3, id + 0); */

/*	mem_print_info(); */
	
/*	dict_table_print_by_name("TS_TABLE1"); */

	log_flush_up_to(ut_dulint_zero);

	os_thread_sleep(500000);

	n2 = 10000;

	test2_1(&n2);

/*	test9A(&n2);
	test9(&n2); */
	
/*	test6(&n2); */

/*	test2(&n2); */

/*	test2_2(&n2); */

/*	mem_print_info(); */

	for (i = 0; i < 2; i++) {

		n1000[i] = 1000 + 10 * i;
		id[i] = id[i];

/*		os_thread_create(test2mt, n1000 + i, id + i);
		os_thread_create(test2_1mt, n1000 + i, id + i);
		os_thread_create(test2_2mt, n1000 + i, id + i); */
	}

	n2 = 2000;

/*	test2mt(&n2); */

/*	buf_print();
	ibuf_print();
	rw_lock_list_print_info();
	mutex_list_print_info();
	
	dict_table_print_by_name("TS_TABLE1"); */

/*	mem_print_info(); */
	
	n2 = 1000;

/*	test4_1();
	test4_2();

	for (i = 0; i < 2; i++) {
		n1000[i] = i;
		id[i] = id[i];
		os_thread_create(test4mt, n1000 + i, id + i);
	}

	n2 = 4;
	test4mt(&n2);
	test4mt(&n2);
	test4_2();
	lock_print_info(); */

/*	test7(&n2); */

/*	os_thread_sleep(25000000); */

	tm = ut_clock();
	printf("Wall time for test %lu milliseconds\n", tm - oldtm);
	printf("TESTS COMPLETED SUCCESSFULLY!\n");
} 
