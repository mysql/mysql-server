/************************************************************************
Test for the transaction system

(c) 1994-1997 Innobase Oy

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
#include "srv0srv.h"
#include "que0que.h"
#include "com0com.h"
#include "usr0sess.h"
#include "lock0lock.h"
#include "trx0roll.h"
#include "row0ins.h"
#include "row0upd.h"

os_file_t	files[1000];

mutex_t		ios_mutex;
ulint		ios;
ulint		n[10];

mutex_t		incs_mutex;
ulint		incs;

byte		bigbuf[1000000];

#define N_SPACES	1
#define N_FILES		1
#define FILE_SIZE	1024 	/* must be > 512 */
#define POOL_SIZE	512
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
		} else {
			ut_a(os_file_set_size(files[i], 8192 * FILE_SIZE, 0));
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
	
	index = dict_mem_index_create("TS_TABLE1", "IND1", 0, DICT_CLUSTERED,
									2);
	dict_mem_index_add_field(index, "COL1", 0);
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
	/* CREATE SECONDARY INDEX */
	
	index = dict_mem_index_create("TS_TABLE1", "IND2", 0, 0, 2);

	dict_mem_index_add_field(index, "COL2", 0);
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
	/* CREATE ANOTHER SECONDARY INDEX */
	
	index = dict_mem_index_create("TS_TABLE1", "IND3", 0, 0, 2);

	dict_mem_index_add_field(index, "COL2", 0);
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

	return(0);
}

/*********************************************************************
Another test for table creation. */

ulint
test1_5(
/*====*/
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
	printf("TEST 1.5. CREATE TABLE WITH 3 COLUMNS AND WITH 1 INDEX\n");

	heap = mem_heap_create(512);

	com_endpoint = (com_endpoint_t*)heap;	/* This is a dummy non-NULL
						value */
	mutex_enter(&kernel_mutex);	

	sess = sess_open(ut_dulint_zero, com_endpoint, (byte*)"user1", 6);
	
	trx = sess->trx;

	mutex_exit(&kernel_mutex);

	ut_a(trx_start(trx, ULINT_UNDEFINED));	

	table = dict_mem_table_create("TS_TABLE2", 0, 3);

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
	
	index = dict_mem_index_create("TS_TABLE2", "IND1", 0, DICT_CLUSTERED,
									2);
	dict_mem_index_add_field(index, "COL1", 0);
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

	return(0);
}

/*********************************************************************
Another test for table creation. */

ulint
test1_6(
/*====*/
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
	printf("TEST 1.5. CREATE TABLE WITH 3 COLUMNS AND WITH 1 INDEX\n");

	heap = mem_heap_create(512);

	com_endpoint = (com_endpoint_t*)heap;	/* This is a dummy non-NULL
						value */
	mutex_enter(&kernel_mutex);	

	sess = sess_open(ut_dulint_zero, com_endpoint, (byte*)"user1", 6);
	
	trx = sess->trx;

	mutex_exit(&kernel_mutex);

	ut_a(trx_start(trx, ULINT_UNDEFINED));	

	table = dict_mem_table_create("TS_TABLE3", 0, 3);

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
	
	index = dict_mem_index_create("TS_TABLE3", "IND1", 0, DICT_CLUSTERED,
									2);
	dict_mem_index_add_field(index, "COL1", 0);
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

	return(0);
}

/*********************************************************************
Test for inserts. */

ulint
test2(
/*==*/
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
	ulint		rnd;
	dtuple_t*	row;
	byte		buf[100];
	ulint		count = 0;
	ins_node_t*	node;
	
	printf("-------------------------------------------------\n");
	printf("TEST 2. MASSIVE INSERT\n");

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

	mem_print_info();

	oldtm = ut_clock();

	for (i = 0; i < *((ulint*)arg); i++) {

		rnd = (rnd + 1) % 200000;
		
		dtuple_gen_test_tuple3(row, rnd, DTUPLE_TEST_FIXED30, buf);

		mutex_enter(&kernel_mutex);	

		ut_a(
		thr == que_fork_start_command(fork, SESS_COMM_EXECUTE, 0));

		mutex_exit(&kernel_mutex);

		que_run_threads(thr);
	}

	tm = ut_clock();
	printf("Wall time for %lu inserts %lu milliseconds\n", i, tm - oldtm);

	mem_print_info();

/*	dict_table_print_by_name("TS_TABLE1"); */
	/*-------------------------------------*/
	/* ROLLBACK */
#ifdef notdefined
	
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
	/*-------------------------------------*/
#endif
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

	if (count < 1) {
		goto loop;
	}
	return(0);
}

/*********************************************************************
Test for updates. */

ulint
test3(
/*==*/
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
	ulint		rnd;
	dtuple_t*	row;
	dtuple_t*	entry;
	byte		buf[100];
	ulint		count = 0;
	btr_pcur_t	pcur;
	upd_t*		update;
	upd_field_t*	ufield;
	dict_tree_t*	tree;
	dict_index_t*	index;
	mtr_t		mtr;
	upd_node_t*	node;
	byte*		ptr;
	ulint		len;
	ulint		err;

	UT_NOT_USED(arg);
	
	printf("-------------------------------------------------\n");
	printf("TEST 3. UPDATES\n");

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
	/* INSERT */
	fork = que_fork_create(NULL, NULL, QUE_FORK_INSERT, heap);
	fork->trx = trx;

	thr = que_thr_create(fork, fork, heap);

	table = dict_table_get("TS_TABLE1", trx);

	row = dtuple_create(heap, 3 + DATA_N_SYS_COLS);

	dict_table_copy_types(row, table);
	
	thr->child = ins_node_create(fork, thr, row, table, heap);

	mutex_enter(&kernel_mutex);	

	que_graph_publish(fork, trx->sess);

	trx->graph = fork;

	mutex_exit(&kernel_mutex);

	rnd = 0;

	oldtm = ut_clock();

	for (i = 0; i < 3; i++) {
		
		dtuple_gen_test_tuple3(row, i, DTUPLE_TEST_FIXED30, buf);

		mutex_enter(&kernel_mutex);	

		ut_a(
		thr == que_fork_start_command(fork, SESS_COMM_EXECUTE, 0));

		mutex_exit(&kernel_mutex);

		que_run_threads(thr);
	}

	tm = ut_clock();
	printf("Wall time for %lu inserts %lu milliseconds\n", i, tm - oldtm);

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

	dict_table_print_by_name("TS_TABLE1");
	/*-------------------------------------*/
	/* UPDATE ROWS */
	ut_a(trx_start(trx, ULINT_UNDEFINED));	

	fork = que_fork_create(NULL, NULL, QUE_FORK_UPDATE, heap);
	fork->trx = trx;

	thr = que_thr_create(fork, fork, heap);

	table = dict_table_get("TS_TABLE1", trx);

	row = dtuple_create(heap, 3 + DATA_N_SYS_COLS);

	dict_table_copy_types(row, table);
	
	update = upd_create(1, heap);

	node = upd_node_create(fork, thr, table, &pcur, update, heap);
	thr->child = node;

	node->cmpl_info = 0;

	mutex_enter(&kernel_mutex);

	que_graph_publish(fork, trx->sess);

	trx->graph = fork;

	mutex_exit(&kernel_mutex);

	dtuple_gen_test_tuple3(row, 1, DTUPLE_TEST_FIXED30, buf);
	entry = dtuple_create(heap, 2);
	dfield_copy(dtuple_get_nth_field(entry, 0),
					dtuple_get_nth_field(row, 0));
	dfield_copy(dtuple_get_nth_field(entry, 1),
					dtuple_get_nth_field(row, 1));

	index = dict_table_get_first_index(table);
	tree = dict_index_get_tree(index);

	btr_pcur_set_mtr(&pcur, &mtr);
	
	mtr_start(&mtr);
	
	btr_pcur_open(tree, entry, PAGE_CUR_G, BTR_SEARCH_LEAF, &pcur, &mtr);

	btr_pcur_store_position(&pcur, &mtr);

	err = lock_clust_rec_read_check_and_lock(0, btr_pcur_get_rec(&pcur),
							index, LOCK_X, thr);
	ut_a(err == DB_SUCCESS);
	
	btr_pcur_commit(&pcur);	

	ufield = upd_get_nth_field(update, 0);

	ufield->col_no = 2;
	dfield_set_data(&(ufield->new_val), "updated field", 14);

	mutex_enter(&kernel_mutex);	

	ut_a(
	thr == que_fork_start_command(fork, SESS_COMM_EXECUTE, 0));

	mutex_exit(&kernel_mutex);

	que_run_threads(thr);

	mtr_start(&mtr);

	ut_a(btr_pcur_restore_position(BTR_SEARCH_LEAF, &pcur, &mtr));

	ptr = rec_get_nth_field(btr_pcur_get_rec(&pcur), 5, &len);

	ut_a(ut_memcmp(ptr, "updated field", 14) == 0);

	btr_pcur_commit(&pcur);

	dict_table_print_by_name("TS_TABLE1");

	ufield = upd_get_nth_field(update, 0);

	ufield->col_no = 0;
	dfield_set_data(&(ufield->new_val), "31415926", 9);

	mutex_enter(&kernel_mutex);	

	ut_a(
	thr == que_fork_start_command(fork, SESS_COMM_EXECUTE, 0));

	mutex_exit(&kernel_mutex);

	que_run_threads(thr);

	dict_table_print_by_name("TS_TABLE1");
	/*-------------------------------------*/
	/* ROLLBACK */
#ifdef notdefined
	
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
	printf("Wall time for rollback of %lu updates %lu milliseconds\n",
		i, tm - oldtm);
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
#endif
	dict_table_print_by_name("TS_TABLE1");
	count++;

	if (count < 1) {
		goto loop;
	}
	return(0);
}

/*********************************************************************
Test for massive updates. */

ulint
test4(
/*==*/
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
	ulint		j;
	ulint		rnd;
	dtuple_t*	row;
	dtuple_t*	entry;
	byte		buf[100];
	ulint		count = 0;
	btr_pcur_t	pcur;
	upd_t*		update;
	upd_field_t*	ufield;
	dict_tree_t*	tree;
	dict_index_t*	index;
	mtr_t		mtr;
	upd_node_t*	node;
	byte*		ptr;
	ulint		len;
	ulint		err;

	printf("-------------------------------------------------\n");
	printf("TEST 4. MASSIVE UPDATES\n");

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
	/* INSERT */
	fork = que_fork_create(NULL, NULL, QUE_FORK_INSERT, heap);
	fork->trx = trx;

	thr = que_thr_create(fork, fork, heap);

	table = dict_table_get("TS_TABLE1", trx);

	row = dtuple_create(heap, 3 + DATA_N_SYS_COLS);

	dict_table_copy_types(row, table);
	
	thr->child = ins_node_create(fork, thr, row, table, heap);

	mutex_enter(&kernel_mutex);	

	que_graph_publish(fork, trx->sess);

	trx->graph = fork;

	mutex_exit(&kernel_mutex);

	rnd = 0;

	oldtm = ut_clock();

	for (i = 0; i < *((ulint*)arg); i++) {
		
		dtuple_gen_test_tuple3(row, i, DTUPLE_TEST_FIXED30, buf);

		mutex_enter(&kernel_mutex);	

		ut_a(
		thr == que_fork_start_command(fork, SESS_COMM_EXECUTE, 0));

		mutex_exit(&kernel_mutex);

		que_run_threads(thr);
	}

	tm = ut_clock();
	printf("Wall time for %lu inserts %lu milliseconds\n", i, tm - oldtm);

#ifdef notdefined
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

	dict_table_print_by_name("TS_TABLE1");
	/*-------------------------------------*/
	/* UPDATE ROWS */
	ut_a(trx_start(trx, ULINT_UNDEFINED));	
#endif
	fork = que_fork_create(NULL, NULL, QUE_FORK_UPDATE, heap);
	fork->trx = trx;

	thr = que_thr_create(fork, fork, heap);

	table = dict_table_get("TS_TABLE1", trx);

	row = dtuple_create(heap, 3 + DATA_N_SYS_COLS);

	dict_table_copy_types(row, table);
	
	update = upd_create(1, heap);

	node = upd_node_create(fork, thr, table, &pcur, update, heap);
	thr->child = node;

	node->cmpl_info = 0;

	mutex_enter(&kernel_mutex);

	que_graph_publish(fork, trx->sess);

	trx->graph = fork;

	mutex_exit(&kernel_mutex);

   for (j = 0; j < 2; j++) {
    for (i = 0; i < *((ulint*)arg); i++) {
	
	dtuple_gen_test_tuple3(row, i, DTUPLE_TEST_FIXED30, buf);
	entry = dtuple_create(heap, 2);
	dfield_copy(dtuple_get_nth_field(entry, 0),
					dtuple_get_nth_field(row, 0));
	dfield_copy(dtuple_get_nth_field(entry, 1),
					dtuple_get_nth_field(row, 1));

	index = dict_table_get_first_index(table);
	tree = dict_index_get_tree(index);

	btr_pcur_set_mtr(&pcur, &mtr);
	
	mtr_start(&mtr);
	
	btr_pcur_open(tree, entry, PAGE_CUR_G, BTR_SEARCH_LEAF, &pcur, &mtr);

	btr_pcur_store_position(&pcur, &mtr);

	err = lock_clust_rec_read_check_and_lock(0, btr_pcur_get_rec(&pcur),
							index, LOCK_X, thr);
	ut_a(err == DB_SUCCESS);
	
	btr_pcur_commit(&pcur);	

	ufield = upd_get_nth_field(update, 0);

	ufield->col_no = 2;
	dfield_set_data(&(ufield->new_val), "updated field", 14);

	mutex_enter(&kernel_mutex);	

	ut_a(
	thr == que_fork_start_command(fork, SESS_COMM_EXECUTE, 0));

	mutex_exit(&kernel_mutex);

	que_run_threads(thr);

    } /* for (i = ... */
   }
	mtr_start(&mtr);

	ut_a(btr_pcur_restore_position(BTR_SEARCH_LEAF, &pcur, &mtr));

	ptr = rec_get_nth_field(btr_pcur_get_rec(&pcur), 5, &len);

	ut_a(ut_memcmp(ptr, "updated field", 14) == 0);

	btr_pcur_commit(&pcur);

	dict_table_print_by_name("TS_TABLE1");

	ufield = upd_get_nth_field(update, 0);

	ufield->col_no = 0;
	dfield_set_data(&(ufield->new_val), "31415926", 9);

	mutex_enter(&kernel_mutex);	

	ut_a(
	thr == que_fork_start_command(fork, SESS_COMM_EXECUTE, 0));

	mutex_exit(&kernel_mutex);

	que_run_threads(thr);

	dict_table_print_by_name("TS_TABLE1");
	/*-------------------------------------*/
	/* ROLLBACK */
	
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
	printf("Wall time for rollback of %lu updates %lu milliseconds\n",
		i, tm - oldtm);
#ifdef notdefined
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
#endif
	dict_table_print_by_name("TS_TABLE1");
	count++;

	if (count < 1) {
		goto loop;
	}
	return(0);
}

/*********************************************************************
Init TS_TABLE2 for TPC-A transaction. */

ulint
test4_5(
/*====*/
	void*	arg)
{
	sess_t*		sess;
	com_endpoint_t*	com_endpoint;
	mem_heap_t*	heap;
	que_fork_t*	fork;
	dict_table_t*	table;
	que_thr_t*	thr;
	trx_t*		trx;
	ulint		i;
	dtuple_t*	row;
	byte		buf[100];
	
	arg = arg;

	printf("-------------------------------------------------\n");
	printf("TEST 4_5. INIT FOR TPC-A\n");

	heap = mem_heap_create(512);

	com_endpoint = (com_endpoint_t*)heap;	/* This is a dummy non-NULL
						value */
	mutex_enter(&kernel_mutex);	

	sess = sess_open(ut_dulint_zero, com_endpoint, (byte*)"user1", 6);
	
	trx = sess->trx;

	mutex_exit(&kernel_mutex);

	ut_a(trx_start(trx, ULINT_UNDEFINED));	

	/*-------------------------------------*/
	/* INSERT INTO TABLE TO UPDATE */

    for (i = 0; i < 100; i++) {
	fork = que_fork_create(NULL, NULL, QUE_FORK_INSERT, heap);
	fork->trx = trx;

	thr = que_thr_create(fork, fork, heap);

	table = dict_table_get("TS_TABLE2", trx);

	row = dtuple_create(heap, 3 + DATA_N_SYS_COLS);

	dict_table_copy_types(row, table);
	
	thr->child = ins_node_create(fork, thr, row, table, heap);

	mutex_enter(&kernel_mutex);	

	que_graph_publish(fork, trx->sess);

	trx->graph = fork;

	mutex_exit(&kernel_mutex);

	dtuple_gen_test_tuple3(row, i, DTUPLE_TEST_FIXED30, buf);

	mutex_enter(&kernel_mutex);	

	ut_a(thr == que_fork_start_command(fork, SESS_COMM_EXECUTE, 0));

	mutex_exit(&kernel_mutex);

	que_run_threads(thr);

    }
/*	dict_table_print_by_name("TS_TABLE2"); */

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

	que_run_threads(thr);

	/*-----------------------------------*/

	return(0);
}

/*********************************************************************
Test for TPC-A transaction. */

ulint
test5(
/*==*/
	void*	arg)
{
	ulint		tm, oldtm;
	sess_t*		sess;
	com_endpoint_t*	com_endpoint;
	mem_heap_t*	heap;
	que_fork_t*	fork1;
	que_fork_t*	fork2;
	que_fork_t*	cfork;
	dict_table_t*	table;
	dict_table_t*	table2;
	que_thr_t*	thr;
	trx_t*		trx;
	ulint		i;
	dtuple_t*	row;
	dtuple_t*	entry;
	byte		buf[100];
	ulint		count 		= 0;
	btr_pcur_t	pcur;
	upd_t*		update;
	upd_field_t*	ufield;
	dict_tree_t*	tree;
	dict_index_t*	index;
	mtr_t		mtr;
	upd_node_t*	node;
	ulint		err;
	ins_node_t*	inode;
	
	arg = arg;

	printf("-------------------------------------------------\n");
	printf("TEST 5. TPC-A %lu \n", *((ulint*)arg));

	oldtm = ut_clock();
	
	heap = mem_heap_create(512);

	com_endpoint = (com_endpoint_t*)heap;	/* This is a dummy non-NULL
						value */
	mutex_enter(&kernel_mutex);	

	sess = sess_open(ut_dulint_zero, com_endpoint, (byte*)"user1", 6);
	
	trx = sess->trx;

	mutex_exit(&kernel_mutex);

	ut_a(trx_start(trx, ULINT_UNDEFINED));	
	/*-----------------------------------*/

	fork1 = que_fork_create(NULL, NULL, QUE_FORK_INSERT, heap);
	fork1->trx = trx;

	thr = que_thr_create(fork1, fork1, heap);

	table = dict_table_get("TS_TABLE3", trx);

	row = dtuple_create(heap, 3 + DATA_N_SYS_COLS);

	dict_table_copy_types(row, table);

	inode = ins_node_create(fork1, thr, row, table, heap);
	
	thr->child = inode;

	row_ins_init_sys_fields_at_sql_compile(inode->row, inode->table, heap);
	row_ins_init_sys_fields_at_sql_prepare(inode->row, inode->table, trx);

	inode->init_all_sys_fields = FALSE;

	mutex_enter(&kernel_mutex);	

	que_graph_publish(fork1, trx->sess);

	trx->graph = fork1;

	mutex_exit(&kernel_mutex);

	fork2 = que_fork_create(NULL, NULL, QUE_FORK_UPDATE, heap);
	fork2->trx = trx;

	thr = que_thr_create(fork2, fork2, heap);

	table2 = dict_table_get("TS_TABLE2", trx);

	update = upd_create(1, heap);

	entry = dtuple_create(heap, 2);
	dfield_copy(dtuple_get_nth_field(entry, 0),
					dtuple_get_nth_field(row, 0));
	dfield_copy(dtuple_get_nth_field(entry, 1),
					dtuple_get_nth_field(row, 1));
	
	node = upd_node_create(fork2, thr, table2, &pcur, update, heap);
	thr->child = node;

	node->cmpl_info = UPD_NODE_NO_ORD_CHANGE | UPD_NODE_NO_SIZE_CHANGE;

	mutex_enter(&kernel_mutex);

	que_graph_publish(fork2, trx->sess);

	trx->graph = fork2;

	mutex_exit(&kernel_mutex);

	cfork = que_fork_create(NULL, NULL, QUE_FORK_EXECUTE, heap);
	cfork->trx = trx;

	thr = que_thr_create(cfork, cfork, heap);

	thr->child = commit_node_create(cfork, thr, heap);

	oldtm = ut_clock();
loop:
	/*-------------------------------------*/
	/* INSERT */

/*	printf("Trx %lu %lu starts, thr %lu\n",
						ut_dulint_get_low(trx->id),
						(ulint)trx,
						*((ulint*)arg)); */

	dtuple_gen_test_tuple3(row, count, DTUPLE_TEST_FIXED30, buf);

	mutex_enter(&kernel_mutex);	

	thr = que_fork_start_command(fork1, SESS_COMM_EXECUTE, 0);

	mutex_exit(&kernel_mutex);

	que_run_threads(thr);

	/*-------------------------------------*/
	/* 3 UPDATES */

    for (i = 0; i < 3; i++) {
	
	dtuple_gen_search_tuple3(entry, *((ulint*)arg), buf);

	index = dict_table_get_first_index(table2);
	tree = dict_index_get_tree(index);

	btr_pcur_set_mtr(&pcur, &mtr);
	
	mtr_start(&mtr);
	
	btr_pcur_open(tree, entry, PAGE_CUR_G, BTR_MODIFY_LEAF, &pcur, &mtr);

/*	btr_pcur_store_position(&pcur, &mtr); */

	err = lock_clust_rec_read_check_and_lock(0, btr_pcur_get_rec(&pcur),
							index, LOCK_X, thr);
	ut_a(err == DB_SUCCESS);
	
	ufield = upd_get_nth_field(update, 0);

	ufield->col_no = 2;
	dfield_set_data(&(ufield->new_val),
				"updated field1234567890123456", 30);

	mutex_enter(&kernel_mutex);	

	thr = que_fork_start_command(fork2, SESS_COMM_EXECUTE, 0);

	mutex_exit(&kernel_mutex);

	que_run_threads(thr);

    } /* for (i = ... */

	/*-------------------------------------*/
	/* COMMIT */
#ifdef notdefined
	mutex_enter(&kernel_mutex);	

	thr = que_fork_start_command(cfork, SESS_COMM_EXECUTE, 0);
	
	mutex_exit(&kernel_mutex);

	que_run_threads(thr);

/*	printf("Trx %lu %lu committed\n", ut_dulint_get_low(trx->id),
								(ulint)trx); */
#endif
	count++;

	if (count < 1000) {
		ut_a(trx_start(trx, ULINT_UNDEFINED));	

		goto loop;
	}

	tm = ut_clock();
	printf("Wall time for TPC-A %lu trxs %lu milliseconds\n",
					count, tm - oldtm);

	/*-------------------------------------*/
/*	dict_table_print_by_name("TS_TABLE2"); 
	dict_table_print_by_name("TS_TABLE3"); */

	return(0);
}

/********************************************************************
Main test function. */

void 
main(void) 
/*======*/
{
	ulint	tm, oldtm;
	os_thread_id_t	id[5];
	ulint	n1000[5];
	ulint	i;
	ulint	n5000	= 500;

	srv_boot("initfile");
	os_aio_init(160, 5);
	fil_init(25);
	buf_pool_init(POOL_SIZE, POOL_SIZE);
	fsp_init();
	log_init();

	create_files();
	init_space();

	sess_sys_init_at_db_start();
	
	trx_sys_create();
	
	lock_sys_create(1024);
	
	dict_create();

	oldtm = ut_clock();
	
	ut_rnd_set_seed(19);

	test1(NULL);
	test1_5(NULL);
	test1_6(NULL);
	test4_5(NULL);

	for (i = 1; i < 5; i++) {
		n1000[i] = i;
		id[i] = id[i];
/*		os_thread_create(test5, n1000 + i, id + i); */
	}

/*	mem_print_info(); */

/*	test2(&n5000); */

	n5000 = 30;

	test5(&n5000);

	n5000 = 30;
/*	test5(&n5000); */
	
/*	mem_print_info(); */
	
/*	dict_table_print_by_name("TS_TABLE1"); */

	tm = ut_clock();
	printf("Wall time for test %lu milliseconds\n", tm - oldtm);
	printf("TESTS COMPLETED SUCCESSFULLY!\n");
} 
