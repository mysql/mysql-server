/************************************************************************
Test for the B-tree

(c) 1996-1997 Innobase Oy

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
#include "buf0flu.h"
#include "os0file.h"
#include "os0thread.h"
#include "fil0fil.h"
#include "fsp0fsp.h"
#include "rem0rec.h"
#include "rem0cmp.h"
#include "mtr0mtr.h"
#include "log0log.h"
#include "log0recv.h"
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

byte		bigbuf[1000000];

#define N_SPACES	2
#define N_FILES		1
#define FILE_SIZE	512 	/* must be > 512 */
#define POOL_SIZE	500
#define	COUNTER_OFFSET	1500

#define N_LOG_GROUPS	2	
#define N_LOG_FILES	3
#define LOG_FILE_SIZE	500

#define LOOP_SIZE	150
#define	N_THREADS	5

#define	COUNT		1

ulint zero = 0;

buf_block_t*	bl_arr[POOL_SIZE];

ulint	dummy = 0;

byte	rnd_buf[256 * 256];

/************************************************************************
Io-handler thread function. */

ulint
handler_thread(
/*===========*/
	void*	arg)
{
	ulint	segment;
	ulint	i;
	
	segment = *((ulint*)arg);

	printf("Io handler thread %lu starts\n", segment);

	for (i = 0;; i++) {
		fil_aio_wait(segment);

		mutex_enter(&ios_mutex);
		ios++;
		mutex_exit(&ios_mutex);
		
	}

	return(0);
}

/*************************************************************************
Creates or opens the log files. */

void
create_log_files(void)
/*==================*/
{
	bool		ret;
	ulint		i, k;
	char		name[20];

	printf("--------------------------------------------------------\n");
	printf("Create or open log files\n");

	strcpy(name, "logfile00");

	for (k = 0; k < N_LOG_GROUPS; k++) {
	    for (i = 0; i < N_LOG_FILES; i++) {

		name[6] = (char)((ulint)'0' + k);
		name[7] = (char)((ulint)'0' + i);
	
		files[i] = os_file_create(name, OS_FILE_CREATE, OS_FILE_AIO,
									&ret);
		if (ret == FALSE) {
			ut_a(os_file_get_last_error() ==
						OS_FILE_ALREADY_EXISTS);
	
			files[i] = os_file_create(
				name, OS_FILE_OPEN, OS_FILE_AIO, &ret);
			ut_a(ret);
		} else {
			ut_a(os_file_set_size(files[i],
						8192 * LOG_FILE_SIZE, 0));
		}

		ret = os_file_close(files[i]);
		ut_a(ret);

		if (i == 0) {
			fil_space_create(name, k + 100, FIL_LOG);
		}

		ut_a(fil_validate());

		fil_node_create(name, LOG_FILE_SIZE, k + 100);
	    }
	    
	    fil_space_create(name, k + 200, FIL_LOG);
	    	    
	    log_group_init(k, N_LOG_FILES, LOG_FILE_SIZE * UNIV_PAGE_SIZE,
							k + 100, k + 200);
	}
}

/*************************************************************************
Creates the files for the file system test and inserts them to the file
system. */

void
create_files(void)
/*==============*/
{
	bool		ret;
	ulint		i, k, j, c;
	char		name[20];
	os_thread_t	thr[10];
	os_thread_id_t	id[10];

	printf("--------------------------------------------------------\n");
	printf("Create or open database files\n");
	
	strcpy(name, "tsfile00");

	for (k = 0; k < 2 * N_SPACES; k += 2) {
	for (i = 0; i < N_FILES; i++) {

		name[6] = (char)((ulint)'0' + k);
		name[7] = (char)((ulint)'0' + i);
	
		files[i] = os_file_create(name, OS_FILE_CREATE,
						OS_FILE_NORMAL, &ret);
		if (ret == FALSE) {
			ut_a(os_file_get_last_error() ==
						OS_FILE_ALREADY_EXISTS);
	
			files[i] = os_file_create(
				name, OS_FILE_OPEN, OS_FILE_NORMAL, &ret);
			ut_a(ret);
		} else {
			ut_a(os_file_set_size(files[i],
					UNIV_PAGE_SIZE * FILE_SIZE, 0));
			/* Initialize the file contents to a random value */
						
			for (j = 0; j < FILE_SIZE; j++) {
				for (c = 0; c < UNIV_PAGE_SIZE; c++) {
					rnd_buf[c] = 0xFF;
						/*(byte)
						(ut_rnd_gen_ulint() % 256); */
				}

				os_file_write(files[i], rnd_buf,
						UNIV_PAGE_SIZE * j, 0,
						UNIV_PAGE_SIZE);
			}
		}

		ret = os_file_close(files[i]);
		ut_a(ret);

		if (i == 0) {
			fil_space_create(name, k, FIL_TABLESPACE);
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
Inits space headers of spaces 0 and 2. */

void
init_spaces(void)
/*=============*/
{
	mtr_t	mtr;

	mtr_start(&mtr);

	fsp_header_init(0, FILE_SIZE * N_FILES, &mtr);		
	fsp_header_init(2, FILE_SIZE * N_FILES, &mtr);

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

	dict_table_print_by_name("SYS_TABLES");
	dict_table_print_by_name("SYS_COLUMNS");
	/*-------------------------------------*/
	/* CREATE CLUSTERED INDEX */
	
	index = dict_mem_index_create("TS_TABLE1", "IND1", 0,
					/*DICT_UNIQUE |*/ DICT_CLUSTERED, 1);
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
	
	index = dict_mem_index_create("TS_TABLE2", "IND1", 0,
					DICT_CLUSTERED | DICT_UNIQUE, 1);

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
	printf("TEST 1.6. CREATE TABLE WITH 3 COLUMNS AND WITH 1 INDEX\n");

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
Another test for table creation. */

ulint
test1_7(
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
	printf("TEST 1.7. CREATE TABLE WITH 12 COLUMNS AND WITH 1 INDEX\n");

	heap = mem_heap_create(512);

	com_endpoint = (com_endpoint_t*)heap;	/* This is a dummy non-NULL
						value */
	mutex_enter(&kernel_mutex);	

	sess = sess_open(ut_dulint_zero, com_endpoint, (byte*)"user1", 6);
	
	trx = sess->trx;

	mutex_exit(&kernel_mutex);

	ut_a(trx_start(trx, ULINT_UNDEFINED));	

	table = dict_mem_table_create("TS_TABLE4", 0, 12);

	dict_mem_table_add_col(table, "COL1", DATA_VARCHAR,
						DATA_ENGLISH, 10, 0);
	dict_mem_table_add_col(table, "COL2", DATA_VARCHAR,
						DATA_ENGLISH, 10, 0);
	dict_mem_table_add_col(table, "COL3", DATA_VARCHAR,
						DATA_ENGLISH, 100, 0);
	dict_mem_table_add_col(table, "COL4", DATA_VARCHAR,
						DATA_ENGLISH, 100, 0);
	dict_mem_table_add_col(table, "COL5", DATA_VARCHAR,
						DATA_ENGLISH, 100, 0);
	dict_mem_table_add_col(table, "COL6", DATA_VARCHAR,
						DATA_ENGLISH, 100, 0);
	dict_mem_table_add_col(table, "COL7", DATA_VARCHAR,
						DATA_ENGLISH, 100, 0);
	dict_mem_table_add_col(table, "COL8", DATA_VARCHAR,
						DATA_ENGLISH, 100, 0);
	dict_mem_table_add_col(table, "COL9", DATA_VARCHAR,
						DATA_ENGLISH, 100, 0);
	dict_mem_table_add_col(table, "COL10", DATA_VARCHAR,
						DATA_ENGLISH, 100, 0);
	dict_mem_table_add_col(table, "COL11", DATA_VARCHAR,
						DATA_ENGLISH, 100, 0);
	dict_mem_table_add_col(table, "COL12", DATA_VARCHAR,
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
	
	index = dict_mem_index_create("TS_TABLE4", "IND1", 0,
					DICT_CLUSTERED | DICT_UNIQUE, 1);

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
	dict_tree_t*	tree;
	que_thr_t*	thr;
	trx_t*		trx;
	ulint		i;
	ulint		rnd;
	dtuple_t*	row;
	byte		buf[100];
	ulint		count = 0;
	ins_node_t*	node;
	dict_index_t*	index;
/*	ulint		size; */
	dtuple_t*	entry;
	btr_pcur_t	pcur;
	mtr_t		mtr;
	
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

	btr_search_print_info();

	/*-------------------------------------*/
	/* MASSIVE RANDOM INSERT */
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

	oldtm = ut_clock();

	for (i = 0; i < *((ulint*)arg); i++) {

		rnd = (rnd + 7857641) % 200000;
		
		dtuple_gen_test_tuple3(row, rnd, DTUPLE_TEST_RND30, buf);

		mutex_enter(&kernel_mutex);	

		ut_a(
		thr == que_fork_start_command(fork, SESS_COMM_EXECUTE, 0));

		mutex_exit(&kernel_mutex);

		que_run_threads(thr);

		if (i % 100 == 0) {
			printf(
		"********************************Inserted %lu rows\n", i);
			ibuf_print();
		}
	}

	tm = ut_clock();
	printf("Wall time for %lu inserts %lu milliseconds\n", i, tm - oldtm);
/*
	for (i = 0; i < 10; i++) {
		size = ibuf_contract(TRUE);
		
		printf("%lu bytes will be contracted\n", size);

		os_thread_sleep(1000000);
	}
*/
/*	index = dict_table_get_next_index(dict_table_get_first_index(table));
	tree = dict_index_get_tree(index);
	btr_validate_tree(tree);

	index = dict_table_get_next_index(index);
	tree = dict_index_get_tree(index);
	btr_validate_tree(tree);
*/
/*	dict_table_print_by_name("TS_TABLE1"); */

	btr_search_print_info();

	/* Check inserted entries */

	rnd = 0;

	entry = dtuple_create(heap, 1);

	for (i = 0; i <  1 /* *((ulint*)arg) */; i++) {

		rnd = (rnd +  7857641) % 200000;
		dtuple_gen_search_tuple3(entry, rnd, buf);

		index = dict_table_get_first_index(table);
		tree = dict_index_get_tree(index);

		mtr_start(&mtr);
	
		btr_pcur_open(index, entry, PAGE_CUR_LE, BTR_SEARCH_LEAF, &pcur,
									&mtr);
		ut_a(0 == cmp_dtuple_rec(entry, btr_pcur_get_rec(&pcur)));

		btr_pcur_close(&pcur);
		mtr_commit(&mtr);
	}

/*	btr_validate_tree(tree); */

/*	btr_print_tree(tree, 5); */

#ifdef notdefined
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
	printf("Wall time for rollback of %lu inserts %lu milliseconds\n",
		i, tm - oldtm);

	os_thread_sleep(1000000);

	btr_validate_tree(tree);

/*	btr_search_print_info();
	dict_table_print_by_name("TS_TABLE1"); */
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
#endif	
	/*-------------------------------------*/
	count++;

	if (count < 1) {
		goto loop;
	}

	mem_heap_free(heap);
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
/*	buf_frame_t*	frame_table[2000];
	dict_tree_t*	tree;
	dict_index_t*	index;
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

	rnd = 0;

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

	oldtm = ut_clock();

	for (i = 0; i < *((ulint*)arg); i++) {

		dtuple_gen_test_tuple3(row, rnd, DTUPLE_TEST_FIXED30, buf);

		mutex_enter(&kernel_mutex);	

		ut_a(
		thr == que_fork_start_command(fork, SESS_COMM_EXECUTE, 0));

		mutex_exit(&kernel_mutex);

		que_run_threads(thr);

		if (i % 5000 == 0) {

			/* fsp_print(0); */
			/* ibuf_print(); */
			/* buf_print(); */

			/* buf_print_io(); */
			
			tm = ut_clock();
			/*
			printf("Wall time for %lu inserts %lu milliseconds\n",
							i, tm - oldtm); */
		}

		rnd = rnd + 1;
	}

	tm = ut_clock();
	printf("Wall time for %lu inserts %lu milliseconds\n", i, tm - oldtm);

#ifdef notdefined
/*	dict_table_print_by_name("TS_TABLE1"); */

	ibuf_print();

	index = dict_table_get_first_index(table);

	btr_search_index_print_info(index);

	btr_validate_tree(dict_index_get_tree(index));
	
	index = dict_table_get_next_index(index);

	btr_search_index_print_info(index);

	btr_validate_tree(dict_index_get_tree(index));

	index = dict_table_get_next_index(index);

	btr_search_index_print_info(index);

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

	/*-------------------------------------*/
	/* ROLLBACK */
	
/*	btr_validate_tree(tree); */

	for (i = 0; i < POOL_SIZE - 1; i++) {
		frame_table[i] = buf_frame_alloc(FALSE);
	}

	for (i = 0; i < POOL_SIZE - 1; i++) {
		buf_frame_free(frame_table[i]); 
	}

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
#endif	
#ifdef notdefined
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

/*********************************************************************
Another test for inserts. */

ulint
test2_2(
/*====*/
	void*	arg)
{
	ulint		tm, oldtm;
	sess_t*		sess;
	com_endpoint_t*	com_endpoint;
	mem_heap_t*	heap;
	que_fork_t*	fork;
	dict_table_t*	table;
	dict_index_t*	index;
	dict_tree_t*	tree;
	que_thr_t*	thr;
	trx_t*		trx;
	ulint		i;
	ulint		rnd;
	dtuple_t*	row;
	dtuple_t*	entry;
	byte		buf[100];
	ulint		count = 0;
	ins_node_t*	node;
	btr_pcur_t	pcur;
	mtr_t		mtr;
	
	printf("-------------------------------------------------\n");
	printf("TEST 2.2. MASSIVE DESCENDING INSERT\n");

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

	rnd = *((ulint*)arg) + 1;

	oldtm = ut_clock();

	for (i = 0; i < *((ulint*)arg); i++) {

		rnd = (rnd - 1) % 200000;
		
		dtuple_gen_test_tuple3(row, rnd, DTUPLE_TEST_RND3500, buf);

		mutex_enter(&kernel_mutex);	

		ut_a(
		thr == que_fork_start_command(fork, SESS_COMM_EXECUTE, 0));

		mutex_exit(&kernel_mutex);

		que_run_threads(thr);

		if (i % 1000 == 0) {
/*			printf(
		"********************************Inserted %lu rows\n", i);
			ibuf_print(); */
		}
	}

	tm = ut_clock();
	printf("Wall time for %lu inserts %lu milliseconds\n", i, tm - oldtm);

	/* Check inserted entries */

	entry = dtuple_create(heap, 1);
	dtuple_gen_search_tuple3(entry, 0, buf);

	mtr_start(&mtr);
	
	index = dict_table_get_first_index(table);
	tree = dict_index_get_tree(index);

	btr_pcur_open(index, entry, PAGE_CUR_L, BTR_SEARCH_LEAF, &pcur, &mtr);
	ut_a(btr_pcur_is_before_first_in_tree(&pcur, &mtr));

	for (i = 0; i < *((ulint*)arg); i++) {
		ut_a(btr_pcur_move_to_next(&pcur, &mtr));

		dtuple_gen_search_tuple3(entry, i + 1, buf);

		ut_a(0 == cmp_dtuple_rec(entry, btr_pcur_get_rec(&pcur)));
	}	

	ut_a(!btr_pcur_move_to_next(&pcur, &mtr));
	ut_a(btr_pcur_is_after_last_in_tree(&pcur, &mtr));

	btr_pcur_close(&pcur);
	mtr_commit(&mtr);

	btr_validate_tree(tree);
/*	dict_table_print_by_name("TS_TABLE1"); */
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
	printf("Wall time for rollback of %lu inserts %lu milliseconds\n",
		i, tm - oldtm);

	os_thread_sleep(1000000);

	dtuple_gen_search_tuple3(entry, 0, buf);

	mtr_start(&mtr);
	
	btr_pcur_open(index, entry, PAGE_CUR_L, BTR_SEARCH_LEAF, &pcur, &mtr);
	ut_a(btr_pcur_is_before_first_in_tree(&pcur, &mtr));

	ut_a(!btr_pcur_move_to_next(&pcur, &mtr));
	
	ut_a(btr_pcur_is_after_last_in_tree(&pcur, &mtr));

	btr_pcur_close(&pcur);
	mtr_commit(&mtr);

	/*-------------------------------------*/
#ifdef notdefined
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
#endif	
	/*-------------------------------------*/
	count++;

	if (count < 1) {
		goto loop;
	}

	btr_validate_tree(tree);
	mem_heap_free(heap);
	return(0);
}

/*********************************************************************
Multithreaded test for random inserts. */

ulint
test2mt(
/*====*/
	void*	arg)
{
	ulint		tm, oldtm;
	sess_t*		sess;
	com_endpoint_t*	com_endpoint;
	mem_heap_t*	heap;
	que_fork_t*	fork;
	dict_table_t*	table;
	dict_index_t*	index;
	dict_tree_t*	tree;
	que_thr_t*	thr;
	trx_t*		trx;
	ulint		i;
	ulint		rnd;
	dtuple_t*	row;
	byte		buf[100];
	ulint		count = 0;
	ins_node_t*	node;
	
	printf("-------------------------------------------------\n");
	printf("TEST 2MT. MULTITHREADED RANDOM INSERT\n");

	heap = mem_heap_create(512);

	com_endpoint = (com_endpoint_t*)heap;	/* This is a dummy non-NULL
						value */
	mutex_enter(&kernel_mutex);	

	sess = sess_open(ut_dulint_zero, com_endpoint, (byte*)"user1", 6);
	
	trx = sess->trx;

	mutex_exit(&kernel_mutex);
	rnd = 78675;
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

	oldtm = ut_clock();

	for (i = 0; i < *((ulint*)arg); i++) {

		if (i % 100 == 0) {
			printf("*******Inserted %lu rows\n", i);
/*			buf_print(); */
			ibuf_print();
		}
		
		rnd = (rnd + 7857641) % 500;
		
		dtuple_gen_test_tuple3(row, rnd, DTUPLE_TEST_RND30, buf);

		mutex_enter(&kernel_mutex);	

		ut_a(
		thr == que_fork_start_command(fork, SESS_COMM_EXECUTE, 0));

		mutex_exit(&kernel_mutex);

		que_run_threads(thr);
	}

	tm = ut_clock();
	printf("Wall time for %lu inserts %lu milliseconds\n", i, tm - oldtm);

	index = dict_table_get_first_index(table);
	tree = dict_index_get_tree(index);

	printf("Validating tree\n");
	btr_validate_tree(tree);
	printf("Validated\n");
	
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
	printf("Wall time for rollback of %lu inserts %lu milliseconds\n",
		i, tm - oldtm);

	os_thread_sleep(3000000);
	/*-------------------------------------*/
#ifdef notdefined
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
#endif	
	/*-------------------------------------*/
	count++;

	if (count < COUNT) {
		goto loop;
	}

	return(0);
}

/*********************************************************************
Test for multithreaded sequential inserts. */

ulint
test2_1mt(
/*======*/
	void*	arg)
{
	ulint		tm, oldtm;
	sess_t*		sess;
	com_endpoint_t*	com_endpoint;
	mem_heap_t*	heap;
	que_fork_t*	fork;
	dict_table_t*	table;
	dict_index_t*	index;
	dict_tree_t*	tree;
	que_thr_t*	thr;
	trx_t*		trx;
	ulint		i;
	ulint		rnd;
	dtuple_t*	row;
	byte		buf[100];
	ulint		count = 0;
	ins_node_t*	node;
	
	printf("-------------------------------------------------\n");
	printf("TEST 2.1MT. MULTITHREADED ASCENDING INSERT\n");

	rnd = 8757677;
	
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

	oldtm = ut_clock();

	rnd += 98667501;

	for (i = 0; i < *((ulint*)arg); i++) {

		rnd = (rnd + 1) % 500;
		
		dtuple_gen_test_tuple3(row, rnd, DTUPLE_TEST_FIXED30, buf);

		mutex_enter(&kernel_mutex);	

		ut_a(
		thr == que_fork_start_command(fork, SESS_COMM_EXECUTE, 0));

		mutex_exit(&kernel_mutex);

		que_run_threads(thr);
	}

	tm = ut_clock();
	printf("Wall time for %lu inserts %lu milliseconds\n", i, tm - oldtm);

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
	printf("Wall time for rollback of %lu inserts %lu milliseconds\n",
		i, tm - oldtm);

	os_thread_sleep(3000000);

	index = dict_table_get_first_index(table);
	tree = dict_index_get_tree(index);

	printf("Validating tree\n");
	btr_validate_tree(tree);
	printf("Validated\n");
	
	/*-------------------------------------*/
#ifdef notdefined
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
#endif	
	/*-------------------------------------*/
	count++;

	if (count < COUNT) {
		goto loop;
	}

	return(0);
}

/*********************************************************************
Test for multithreaded sequential inserts. */

ulint
test2_2mt(
/*======*/
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
	printf("TEST 2.2MT. MULTITHREADED DESCENDING INSERT\n");

	rnd = 87677;
	
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

	oldtm = ut_clock();

	rnd += 78667;

	for (i = 0; i < *((ulint*)arg); i++) {

		rnd = (rnd - 1) % 500;
		
		dtuple_gen_test_tuple3(row, rnd, DTUPLE_TEST_RND30, buf);

		mutex_enter(&kernel_mutex);	

		ut_a(
		thr == que_fork_start_command(fork, SESS_COMM_EXECUTE, 0));

		mutex_exit(&kernel_mutex);

		que_run_threads(thr);
	}

	tm = ut_clock();
	printf("Wall time for %lu inserts %lu milliseconds\n", i, tm - oldtm);

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
	printf("Wall time for rollback of %lu inserts %lu milliseconds\n",
		i, tm - oldtm);

	os_thread_sleep(3000000);
	/*-------------------------------------*/
#ifdef notdefined
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
#endif	
	/*-------------------------------------*/
	count++;

	mem_print_info();

	if (count < COUNT) {
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
		dtuple_gen_test_tuple3(row, i, DTUPLE_TEST_RND30, buf);

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

	dtuple_gen_test_tuple3(row, 1, DTUPLE_TEST_RND30, buf);

	entry = dtuple_create(heap, 1);
	dfield_copy(dtuple_get_nth_field(entry, 0),
					dtuple_get_nth_field(row, 0));

	index = dict_table_get_first_index(table);
	tree = dict_index_get_tree(index);

	btr_pcur_set_mtr(&pcur, &mtr);
	
	mtr_start(&mtr);
	
	btr_pcur_open(index, entry, PAGE_CUR_LE, BTR_SEARCH_LEAF, &pcur, &mtr);

	btr_pcur_store_position(&pcur, &mtr);

	ut_a(DB_SUCCESS == lock_table(0, table, LOCK_IX, thr));

	ut_a(DB_SUCCESS == lock_clust_rec_read_check_and_lock(0,
						btr_pcur_get_rec(&pcur),
							index, LOCK_X, thr));
	btr_pcur_commit(&pcur);	

	ufield = upd_get_nth_field(update, 0);

	upd_field_set_col_no(ufield, 2, table);
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

	upd_field_set_col_no(ufield, 0, table);
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
Init for update test. */

ulint
test4_1(void)
/*=========*/
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

	printf("-------------------------------------------------\n");
	printf("TEST 4.1. UPDATE INIT\n");

	heap = mem_heap_create(512);

	com_endpoint = (com_endpoint_t*)heap;	/* This is a dummy non-NULL
						value */
	mutex_enter(&kernel_mutex);	

	sess = sess_open(ut_dulint_zero, com_endpoint, (byte*)"user1", 6);
	
	trx = sess->trx;

	mutex_exit(&kernel_mutex);

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

	for (i = 0; i < 200; i++) {
		
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

	return(0);
}

/*************************************************************************
Checks that the multithreaded update test has rolled back its updates. */

void
test4_2(void)
/*=========*/
{
	dtuple_t*	entry;
	mem_heap_t*	heap;
	mem_heap_t*	heap2;
	mtr_t		mtr;
	byte		buf[32];
	sess_t*		sess;
	com_endpoint_t*	com_endpoint;
	que_fork_t*	fork;
	dict_table_t*	table;
	dict_index_t*	index;
	dict_tree_t*	tree;
	que_thr_t*	thr;
	trx_t*		trx;
	ulint		i;
	dtuple_t*	row;
	btr_pcur_t	pcur;
	rec_t*		rec;
	
	printf("-------------------------------------------------\n");
	printf("TEST 4.2. CHECK UPDATE RESULT\n");

	heap = mem_heap_create(512);

	com_endpoint = (com_endpoint_t*)heap;	/* This is a dummy non-NULL
						value */
	mutex_enter(&kernel_mutex);	

	sess = sess_open(ut_dulint_zero, com_endpoint, (byte*)"user1", 6);
	
	trx = sess->trx;

	mutex_exit(&kernel_mutex);

	ut_a(trx_start(trx, ULINT_UNDEFINED));	

	/*------------------------------------------*/
	
	table = dict_table_get("TS_TABLE1", trx);

	index = dict_table_get_first_index(table);
	
	entry = dtuple_create(heap, 1);
	dtuple_gen_search_tuple3(entry, 0, buf);

	mtr_start(&mtr);
	
	index = dict_table_get_first_index(table);
	tree = dict_index_get_tree(index);

	btr_pcur_open(index, entry, PAGE_CUR_L, BTR_SEARCH_LEAF, &pcur, &mtr);
	ut_a(btr_pcur_is_before_first_in_tree(&pcur, &mtr));

	for (i = 0; i < 200; i++) {
		ut_a(btr_pcur_move_to_next(&pcur, &mtr));

		dtuple_gen_search_tuple3(entry, i, buf);

		rec = btr_pcur_get_rec(&pcur);
		
		ut_a(0 == cmp_dtuple_rec(entry, rec));

		heap2 = mem_heap_create(200);
		
		row = row_build(ROW_COPY_DATA, index, rec, heap2);

		ut_a(30 == dfield_get_len(dtuple_get_nth_field(row, 2)));
		ut_a(0 == ut_memcmp(
			dfield_get_data(dtuple_get_nth_field(row, 2)),
			"12345678901234567890123456789", 30));

		mem_heap_free(heap2);
	}	

	ut_a(!btr_pcur_move_to_next(&pcur, &mtr));
	ut_a(btr_pcur_is_after_last_in_tree(&pcur, &mtr));

	btr_pcur_close(&pcur);
	mtr_commit(&mtr);
	
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
}

/*********************************************************************
Test for massive updates. */

ulint
test4mt(
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
	ulint		rnd;
	dtuple_t*	entry;
	byte		buf[100];
	byte		buf2[4000];
	ulint		count = 0;
	btr_pcur_t	pcur;
	upd_t*		update;
	upd_field_t*	ufield;
	dict_tree_t*	tree;
	dict_index_t*	index;
	mtr_t		mtr;
	upd_node_t*	node;
	ulint		err;
	ulint		thr_no;
	ulint		tuple_no;

	printf("-------------------------------------------------\n");
	printf("TEST 4. MULTITHREADED UPDATES\n");

	thr_no = *((ulint*)arg);

	heap = mem_heap_create(512);

	com_endpoint = (com_endpoint_t*)heap;	/* This is a dummy non-NULL
						value */
	mutex_enter(&kernel_mutex);	

	sess = sess_open(ut_dulint_zero, com_endpoint, (byte*)"user1", 6);
	
	trx = sess->trx;

	mutex_exit(&kernel_mutex);
loop:
	/*-------------------------------------*/
	ut_a(trx_start(trx, ULINT_UNDEFINED));	

	fork = que_fork_create(NULL, NULL, QUE_FORK_UPDATE, heap);
	fork->trx = trx;

	thr = que_thr_create(fork, fork, heap);

	table = dict_table_get("TS_TABLE1", trx);

	update = upd_create(1, heap);

	node = upd_node_create(fork, thr, table, &pcur, update, heap);
	thr->child = node;

	node->cmpl_info = 0;

	mutex_enter(&kernel_mutex);

	que_graph_publish(fork, trx->sess);

	trx->graph = fork;

	mutex_exit(&kernel_mutex);

	rnd = 87607651;

	entry = dtuple_create(heap, 1);
	
	oldtm = ut_clock();

	thr_no = *((ulint*)arg);

	ut_a(DB_SUCCESS == lock_table(0, table, LOCK_IX, thr));

    for (i = 0; i < 999; i++) {

    	rnd += 874681;
	tuple_no = (rnd % 40) * 5 + thr_no;
	
	dtuple_gen_search_tuple3(entry, tuple_no, buf);

	index = dict_table_get_first_index(table);
	tree = dict_index_get_tree(index);

	btr_pcur_set_mtr(&pcur, &mtr);
	
	mtr_start(&mtr);
	
	btr_pcur_open(index, entry, PAGE_CUR_LE, BTR_SEARCH_LEAF, &pcur, &mtr);

	btr_pcur_store_position(&pcur, &mtr);

/*	printf("%lu: thread %lu to update row %lu\n", i, thr_no, tuple_no); */

	err = lock_clust_rec_read_check_and_lock(0, btr_pcur_get_rec(&pcur),
							index, LOCK_X, thr);
	ut_a(err == DB_SUCCESS);
	
	btr_pcur_commit(&pcur);	

	ufield = upd_get_nth_field(update, 0);

	upd_field_set_col_no(ufield, 2, table);
	dfield_set_data(&(ufield->new_val), buf2, rnd % 200);

	mutex_enter(&kernel_mutex);	

	ut_a(
	thr == que_fork_start_command(fork, SESS_COMM_EXECUTE, 0));

	mutex_exit(&kernel_mutex);

	que_run_threads(thr);

    } /* for (i = ... */

	tm = ut_clock();
	printf("Wall time for %lu updates %lu milliseconds\n",
		i, tm - oldtm);

/*	dict_table_print_by_name("TS_TABLE1"); */

	printf("Validating tree\n");
	btr_validate_tree(tree);
	printf("Validated\n");

	lock_validate();
	
/*	lock_print_info(); */
	
/*	mem_print_info(); */

	mem_pool_print_info(mem_comm_pool);

	if ((count == 1) && (thr_no != 4)) {

		return(0);
	}
	
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

	os_thread_sleep(7000000);

	btr_validate_tree(tree);

	ut_a(trx->conc_state != TRX_ACTIVE);
	ut_a(UT_LIST_GET_LEN(trx->trx_locks) == 0);
	
	count++;

	if (count < 2) {

		goto loop;
	}

	return(0);
}

/*********************************************************************
Test for join. */

ulint
test6(
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
	byte		buf[100];
	ulint		count = 0;
	dtuple_t*	entry;
	dict_index_t*	index;
	dict_tree_t*	tree;
	btr_pcur_t	pcur;
	btr_pcur_t	pcur2;
	mtr_t		mtr;
	mtr_t		mtr2;
	ulint		rnd;
	ulint		latch_mode;
	
	printf("-------------------------------------------------\n");
	printf("TEST 6. MASSIVE EQUIJOIN\n");

	heap = mem_heap_create(512);

	com_endpoint = (com_endpoint_t*)heap;	/* This is a dummy non-NULL
						value */
	mutex_enter(&kernel_mutex);	

	sess = sess_open(ut_dulint_zero, com_endpoint, (byte*)"user1", 6);
	
	trx = sess->trx;

	mutex_exit(&kernel_mutex);
loop:
	/*--------------*/
	fork = que_fork_create(NULL, NULL, QUE_FORK_EXECUTE, heap);
	fork->trx = trx;

	thr = que_thr_create(fork, fork, heap);

	thr->child = commit_node_create(fork, thr, heap);
	/*--------------*/

	ut_a(trx_start(trx, ULINT_UNDEFINED));	

	/* Check inserted entries */

	entry = dtuple_create(heap, 1);
	dtuple_gen_search_tuple3(entry, 0, buf);

	mtr_start(&mtr);
	
	table = dict_table_get("TS_TABLE1", trx);
	index = dict_table_get_first_index(table);
	tree = dict_index_get_tree(index);

	oldtm = ut_clock();

	btr_pcur_open(index, entry, PAGE_CUR_L, BTR_SEARCH_LEAF, &pcur, &mtr);

	ut_a(DB_SUCCESS == lock_table(0, table, LOCK_IS, thr));

	rnd = 98651;
	
	for (i = 0; i < *((ulint*)arg); i++) {

		ut_a(btr_pcur_move_to_next(&pcur, &mtr));

		btr_pcur_store_position(&pcur, &mtr);

		ut_a(DB_SUCCESS == lock_clust_rec_cons_read_check(
						btr_pcur_get_rec(&pcur),
						index));

		btr_pcur_commit_specify_mtr(&pcur, &mtr);

		if (i % 1211 == 0) {
			dummy++;
		}

		rnd = 55321;

		dtuple_gen_search_tuple3(entry, rnd % *((ulint*)arg), buf);

/*		if (i == 0) { */
			latch_mode = BTR_SEARCH_LEAF;
/*		} else {
			latch_mode = BTR_SEARCH_LEAF | BTR_GUESS_LATCH;
		} */

		mtr_start(&mtr2);

		btr_pcur_open(index, entry, PAGE_CUR_LE, latch_mode,
								&pcur2, &mtr2);

		ut_a(DB_SUCCESS == lock_clust_rec_cons_read_check(
						btr_pcur_get_rec(&pcur2),
						index));
						
		ut_a(0 == cmp_dtuple_rec(entry, btr_pcur_get_rec(&pcur2)));

		mtr_commit(&mtr2);

		mtr_start(&mtr);

		btr_pcur_restore_position(BTR_SEARCH_LEAF, &pcur, &mtr);
	}	

	ut_a(!btr_pcur_move_to_next(&pcur, &mtr));
	ut_a(btr_pcur_is_after_last_in_tree(&pcur, &mtr));

	btr_pcur_close(&pcur);
	mtr_commit(&mtr);
	
	tm = ut_clock();
	printf("Wall time for join of %lu rows %lu milliseconds\n",
							i, tm - oldtm);
	btr_search_index_print_info(index);
	/*-------------------------------------*/
	/* COMMIT */
	mutex_enter(&kernel_mutex);	

	que_graph_publish(fork, trx->sess);

	trx->graph = fork;

	ut_a(thr == que_fork_start_command(fork, SESS_COMM_EXECUTE, 0));
	
	mutex_exit(&kernel_mutex);

	oldtm = ut_clock();
	
	que_run_threads(thr);

	tm = ut_clock();
/*	printf("Wall time for commit %lu milliseconds\n", tm - oldtm); */

	/*-------------------------------------*/
	count++;
/*	btr_validate_tree(tree); */

	if (count < 3) {
		goto loop;
	}

	mem_heap_free(heap);
	return(0);
}

/*********************************************************************
Test for lock wait. Requires Test 4.1 first. */

ulint
test7(
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
	trx_t*		trx2;
	ulint		rnd;
	dtuple_t*	entry;
	dtuple_t*	row;
	byte		buf[100];
	byte		buf2[4000];
	ulint		count = 0;
	btr_pcur_t	pcur;
	upd_t*		update;
	upd_field_t*	ufield;
	dict_tree_t*	tree;
	dict_index_t*	index;
	mtr_t		mtr;
	upd_node_t*	node;
	ulint		err;
	ulint		thr_no;
	ulint		tuple_no;

	printf("-------------------------------------------------\n");
	printf("TEST 7. LOCK WAIT\n");

	thr_no = *((ulint*)arg);

	heap = mem_heap_create(512);

	com_endpoint = (com_endpoint_t*)heap;	/* This is a dummy non-NULL
						value */
	mutex_enter(&kernel_mutex);	

	sess = sess_open(ut_dulint_zero, com_endpoint, (byte*)"user1", 6);
	
	trx = sess->trx;

	sess = sess_open(ut_dulint_zero, com_endpoint, (byte*)"user1", 6);
	
	trx2 = sess->trx;

	mutex_exit(&kernel_mutex);

	/*-------------------------------------*/
	/* UPDATE by trx */
	ut_a(trx_start(trx, ULINT_UNDEFINED));	
	ut_a(trx_start(trx2, ULINT_UNDEFINED));	

	fork = que_fork_create(NULL, NULL, QUE_FORK_UPDATE, heap);
	fork->trx = trx;

	thr = que_thr_create(fork, fork, heap);

	table = dict_table_get("TS_TABLE1", trx);

	update = upd_create(1, heap);

	node = upd_node_create(fork, thr, table, &pcur, update, heap);
	thr->child = node;

	node->cmpl_info = 0;

	mutex_enter(&kernel_mutex);

	que_graph_publish(fork, trx->sess);

	trx->graph = fork;

	mutex_exit(&kernel_mutex);

	rnd = 87607651;

	entry = dtuple_create(heap, 2);
	
	oldtm = ut_clock();

	thr_no = *((ulint*)arg);

	ut_a(DB_SUCCESS == lock_table(0, table, LOCK_IX, thr));

    	rnd += 874681;
	tuple_no = 3;
	
	dtuple_gen_search_tuple3(entry, tuple_no, buf);

	index = dict_table_get_first_index(table);
	tree = dict_index_get_tree(index);

	btr_pcur_set_mtr(&pcur, &mtr);
	
	mtr_start(&mtr);
	
	btr_pcur_open(index, entry, PAGE_CUR_LE, BTR_SEARCH_LEAF, &pcur, &mtr);

	btr_pcur_store_position(&pcur, &mtr);

	err = lock_clust_rec_read_check_and_lock(0, btr_pcur_get_rec(&pcur),
							index, LOCK_X, thr);
	ut_a(err == DB_SUCCESS);
	
	btr_pcur_commit(&pcur);	

	ufield = upd_get_nth_field(update, 0);

	upd_field_set_col_no(ufield, 2, table);
	dfield_set_data(&(ufield->new_val), buf2, rnd % 1500);

	mutex_enter(&kernel_mutex);	

	ut_a(
	thr == que_fork_start_command(fork, SESS_COMM_EXECUTE, 0));

	mutex_exit(&kernel_mutex);

	que_run_threads(thr);

	tm = ut_clock();
	
/*	dict_table_print_by_name("TS_TABLE1"); */

	printf("Validating tree\n");
	btr_validate_tree(tree);
	printf("Validated\n");

	lock_validate();
	
	lock_print_info();
	
	/*-------------------------------------*/
	/* INSERT by trx2 */
	fork = que_fork_create(NULL, NULL, QUE_FORK_INSERT, heap);
	fork->trx = trx2;

	thr = que_thr_create(fork, fork, heap);

	table = dict_table_get("TS_TABLE1", trx2);

	row = dtuple_create(heap, 3 + DATA_N_SYS_COLS);

	dict_table_copy_types(row, table);
	
	thr->child = ins_node_create(fork, thr, row, table, heap);

	mutex_enter(&kernel_mutex);	

	que_graph_publish(fork, trx2->sess);

	trx2->graph = fork;

	mutex_exit(&kernel_mutex);

	rnd = 0;

	oldtm = ut_clock();

	dtuple_gen_test_tuple3(row, 2, DTUPLE_TEST_FIXED30, buf);

	mutex_enter(&kernel_mutex);	

	ut_a(thr == que_fork_start_command(fork, SESS_COMM_EXECUTE, 0));

	mutex_exit(&kernel_mutex);

	/* Insert should be left to wait until trx releases the row lock */

	que_run_threads(thr);

	tm = ut_clock();

	lock_validate();
	
	lock_print_info();

	/*-------------------------------------*/
	/* COMMIT of trx */
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

	/*-------------------------------------*/
	os_thread_sleep(1000000);

	printf(
	 "trx2 can now continue to do the insert, after trx committed.\n");
	
	printf("Validating tree\n");
	btr_validate_tree(tree);
	printf("Validated\n");

	lock_validate();
	
	lock_print_info();

	dict_table_print_by_name("TS_TABLE1");
	
	return(0);
}

/*********************************************************************
Inserts for TPC-A. */

ulint
test8A(
/*===*/
	void*	arg)
{
	ulint		tm, oldtm;
	sess_t*		sess;
	com_endpoint_t*	com_endpoint;
	mem_heap_t*	heap;
	que_fork_t*	fork;
	dict_table_t*	table;
	dict_index_t*	index;
	dict_tree_t*	tree;
	que_thr_t*	thr;
	trx_t*		trx;
	ulint		i;
	ulint		rnd;
	dtuple_t*	row;
	dtuple_t*	entry;
	byte		buf[100];
	ulint		count = 0;
	ins_node_t*	node;
	btr_pcur_t	pcur;
	mtr_t		mtr;

	UT_NOT_USED(arg);	

	printf("-------------------------------------------------\n");
	printf("TEST 8A. 1000 INSERTS FOR TPC-A\n");

	heap = mem_heap_create(512);

	com_endpoint = (com_endpoint_t*)heap;	/* This is a dummy non-NULL
						value */
	mutex_enter(&kernel_mutex);	

	sess = sess_open(ut_dulint_zero, com_endpoint, (byte*)"user1", 6);
	
	trx = sess->trx;

	mutex_exit(&kernel_mutex);
loop:
	ut_a(trx_start(trx, ULINT_UNDEFINED));	

	btr_search_print_info();

	/*-------------------------------------*/
	fork = que_fork_create(NULL, NULL, QUE_FORK_INSERT, heap);
	fork->trx = trx;

	thr = que_thr_create(fork, fork, heap);

	table = dict_table_get("TS_TABLE2", trx);

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

	oldtm = ut_clock();

	for (i = 0; i < 1000; i++) {
		dtuple_gen_test_tuple_TPC_A(row, rnd, buf);

		rnd = rnd + 1;

		mutex_enter(&kernel_mutex);	

		ut_a(
		thr == que_fork_start_command(fork, SESS_COMM_EXECUTE, 0));

		mutex_exit(&kernel_mutex);

		que_run_threads(thr);
	}

	tm = ut_clock();
	printf("Wall time for %lu inserts %lu milliseconds\n", i, tm - oldtm);

	index = dict_table_get_first_index(table);
	tree = dict_index_get_tree(index);
	btr_validate_tree(tree);

	/* Check inserted entries */
	rnd = 0;

	entry = dtuple_create(heap, 1);

	for (i = 0; i < 1000; i++) {
		dtuple_gen_search_tuple_TPC_A(entry, rnd, buf);

		rnd = rnd + 1;

		index = dict_table_get_first_index(table);
		tree = dict_index_get_tree(index);

		mtr_start(&mtr);
	
		btr_pcur_open(index, entry, PAGE_CUR_LE, BTR_SEARCH_LEAF, &pcur,
									&mtr);
		ut_a(0 == cmp_dtuple_rec(entry, btr_pcur_get_rec(&pcur)));

		btr_pcur_close(&pcur);
		mtr_commit(&mtr);
	}

	btr_validate_tree(tree);

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

/*	dict_table_print_by_name("TS_TABLE2");  */

	mem_heap_free(heap);
	return(0);
}

/*********************************************************************
Test for TPC-A transaction. */

ulint
test8(
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
	ulint		rnd		= 0;
	
	arg = arg;

	printf("-------------------------------------------------\n");
	printf("TEST 8. TPC-A %lu \n", *((ulint*)arg));

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

	ufield = upd_get_nth_field(update, 0);
	
	upd_field_set_col_no(ufield, 1, table2);

	entry = dtuple_create(heap, 1);
	dfield_copy(dtuple_get_nth_field(entry, 0),
					dtuple_get_nth_field(row, 0));
	
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
/*	printf("Round %lu\n", count); */

	/*-------------------------------------*/
	/* INSERT */

/*	printf("Trx %lu %lu starts, thr %lu\n",
						ut_dulint_get_low(trx->id),
						(ulint)trx,
						*((ulint*)arg)); */

	dtuple_gen_test_tuple3(row, count, DTUPLE_TEST_FIXED30, buf);

	ins_node_reset(inode);

	mutex_enter(&kernel_mutex);	

	thr = que_fork_start_command(fork1, SESS_COMM_EXECUTE, 0);

	mutex_exit(&kernel_mutex);

	que_run_threads(thr);

	/*-------------------------------------*/
	/* 3 UPDATES */
	
	ut_a(DB_SUCCESS == lock_table(0, table2, LOCK_IX, thr));

    for (i = 0; i < 3; i++) {
	
	rnd += 876751;

	if (count % 1231 == 0) {
		dummy++;
	}

	dtuple_gen_search_tuple_TPC_A(entry, rnd % 1000, buf);

	index = dict_table_get_first_index(table2);
	tree = dict_index_get_tree(index);

	btr_pcur_set_mtr(&pcur, &mtr);
	
	mtr_start(&mtr);
	
	btr_pcur_open(index, entry, PAGE_CUR_LE, BTR_MODIFY_LEAF, &pcur, &mtr);

/*	ut_a(0 == cmp_dtuple_rec(entry, btr_pcur_get_rec(&pcur))); */

/*	btr_pcur_store_position(&pcur, &mtr); */

	err = lock_clust_rec_read_check_and_lock(0, btr_pcur_get_rec(&pcur),
							index, LOCK_X, thr);
	ut_a(err == DB_SUCCESS);
	
	ufield = upd_get_nth_field(update, 0);

	dfield_set_data(&(ufield->new_val), "1234", 5);

	mutex_enter(&kernel_mutex);	

	thr = que_fork_start_command(fork2, SESS_COMM_EXECUTE, 0);

	mutex_exit(&kernel_mutex);

	que_run_threads(thr);

    } /* for (i = ... */

	/*-------------------------------------*/
	/* COMMIT */

	mutex_enter(&kernel_mutex);	

	thr = que_fork_start_command(cfork, SESS_COMM_EXECUTE, 0);
	
	mutex_exit(&kernel_mutex);

	que_run_threads(thr);

	count++;

	if (count < *((ulint*)arg)) {
		ut_a(trx_start(trx, ULINT_UNDEFINED));

		goto loop;
	}

/*	printf("Trx %lu %lu committed\n", ut_dulint_get_low(trx->id),
								(ulint)trx); */
	tm = ut_clock();
	printf("Wall time for TPC-A %lu trxs %lu milliseconds\n",
					count, tm - oldtm);

	btr_search_index_print_info(index);
	btr_search_index_print_info(dict_table_get_first_index(table));
					
/*	mem_print_info(); */
	/*-------------------------------------*/


/*	dict_table_print_by_name("TS_TABLE2"); 
	dict_table_print_by_name("TS_TABLE3"); */

	return(0);
}

/*********************************************************************
Inserts for TPC-C. */

ulint
test9A(
/*===*/
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
/*	dtuple_t*	entry;
	btr_pcur_t	pcur;
	mtr_t		mtr;
	dict_index_t*	index;
	dict_tree_t*	tree;
*/
	UT_NOT_USED(arg);	

	printf("-------------------------------------------------\n");
	printf("TEST 9A. INSERTS FOR TPC-C\n");

#define TPC_C_TABLE_SIZE	15000

	heap = mem_heap_create(512);

	com_endpoint = (com_endpoint_t*)heap;	/* This is a dummy non-NULL
						value */
	mutex_enter(&kernel_mutex);	

	sess = sess_open(ut_dulint_zero, com_endpoint, (byte*)"user1", 6);
	
	trx = sess->trx;

	mutex_exit(&kernel_mutex);
loop:
	ut_a(trx_start(trx, ULINT_UNDEFINED));	

	btr_search_print_info();

	/*-------------------------------------*/
	fork = que_fork_create(NULL, NULL, QUE_FORK_INSERT, heap);
	fork->trx = trx;

	thr = que_thr_create(fork, fork, heap);

	table = dict_table_get("TS_TABLE4", trx);

	row = dtuple_create(heap, 12 + DATA_N_SYS_COLS);

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

	oldtm = ut_clock();

	for (i = 0; i < TPC_C_TABLE_SIZE; i++) {

		dtuple_gen_test_tuple_TPC_C(row, rnd, buf);

		rnd = rnd + 1;

		mutex_enter(&kernel_mutex);	

		ut_a(
		thr == que_fork_start_command(fork, SESS_COMM_EXECUTE, 0));

		mutex_exit(&kernel_mutex);

		que_run_threads(thr);
	}

	tm = ut_clock();
	printf("Wall time for %lu inserts %lu milliseconds\n", i, tm - oldtm);

#ifdef notdefined	
	index = dict_table_get_first_index(table);
	tree = dict_index_get_tree(index);
	btr_validate_tree(tree);

	/* Check inserted entries */
	rnd = 0;

	entry = dtuple_create(heap, 1);

	for (i = 0; i < TPC_C_TABLE_SIZE; i++) {

		dtuple_gen_search_tuple_TPC_C(entry, rnd, buf);

		rnd = rnd + 1;

		index = dict_table_get_first_index(table);
		tree = dict_index_get_tree(index);

		mtr_start(&mtr);
	
		btr_pcur_open(index, entry, PAGE_CUR_LE, BTR_SEARCH_LEAF, &pcur,
									&mtr);
		ut_a(0 == cmp_dtuple_rec(entry, btr_pcur_get_rec(&pcur)));

		btr_pcur_close(&pcur);
		mtr_commit(&mtr);
	}

	btr_validate_tree(tree);
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

/*	dict_table_print_by_name("TS_TABLE4"); */

/*	mem_heap_free(heap); */
	return(0);
}

/*********************************************************************
Test for TPC-C transaction. Test 9A must be run first to populate table. */

ulint
test9(
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
	ulint		j;
	ulint		i;
	byte*		ptr;
	ulint		len;
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
	ulint		rnd		= 0;
	byte		buf2[240];
	rec_t*		rec;
	
	arg = arg;

	printf("-------------------------------------------------\n");
	printf("TEST 9. TPC-C %lu \n", *((ulint*)arg));

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

	table2 = dict_table_get("TS_TABLE4", trx);

	update = upd_create(3, heap);
	ufield = upd_get_nth_field(update, 0);

	upd_field_set_col_no(ufield, 1, table2);
	ufield = upd_get_nth_field(update, 1);

	upd_field_set_col_no(ufield, 1, table2);
	ufield = upd_get_nth_field(update, 2);

	upd_field_set_col_no(ufield, 1, table2);

	entry = dtuple_create(heap, 1);
	dfield_copy(dtuple_get_nth_field(entry, 0),
					dtuple_get_nth_field(row, 0));
	
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
	ut_a(DB_SUCCESS == lock_table(0, table2, LOCK_IS, thr));
	ut_a(DB_SUCCESS == lock_table(0, table2, LOCK_IX, thr));

/*	printf("Round %lu\n", count); */

for (j = 0; j < 13; j++) {

	/*-------------------------------------*/
	/* SELECT FROM 'ITEM' */
	
	rnd += 876751;

	dtuple_gen_search_tuple_TPC_C(entry, rnd % TPC_C_TABLE_SIZE, buf);

	index = dict_table_get_first_index(table2);
	tree = dict_index_get_tree(index);

	mtr_start(&mtr);
	
	btr_pcur_open(index, entry, PAGE_CUR_LE, BTR_SEARCH_LEAF, &pcur, &mtr);

	ut_a(0 == cmp_dtuple_rec(entry, btr_pcur_get_rec(&pcur)));

	err = lock_clust_rec_read_check_and_lock(0, btr_pcur_get_rec(&pcur),
							index, LOCK_S, thr);
	ut_a(err == DB_SUCCESS);

	rec = btr_pcur_get_rec(&pcur);
	
	for (i = 0; i < 5; i++) {
		ptr = rec_get_nth_field(rec, i + 2, &len);

		ut_memcpy(buf2 + i * 24, ptr, len);
	}		

	mtr_commit(&mtr);

	/*-------------------------------------*/
	/* UPDATE 'STOCK' */
	
	rnd += 876751;

	if (count % 1231 == 0) {
		dummy++;
	}

	dtuple_gen_search_tuple_TPC_C(entry, rnd % TPC_C_TABLE_SIZE, buf);

	index = dict_table_get_first_index(table2);
	tree = dict_index_get_tree(index);

	btr_pcur_set_mtr(&pcur, &mtr);
	
	mtr_start(&mtr);
	
	btr_pcur_open(index, entry, PAGE_CUR_LE, BTR_MODIFY_LEAF, &pcur, &mtr);

	ut_a(0 == cmp_dtuple_rec(entry, btr_pcur_get_rec(&pcur)));

/*	btr_pcur_store_position(&pcur, &mtr); */

	rec = btr_pcur_get_rec(&pcur);
	
	for (i = 0; i < 10; i++) {
		ptr = rec_get_nth_field(rec, i + 2, &len);

		ut_memcpy(buf2 + i * 24, ptr, len);
	}		

/*	btr_pcur_commit(&pcur); */

/*	err = lock_clust_rec_read_check_and_lock(0, btr_pcur_get_rec(&pcur),
							index, LOCK_X, thr); */
	ut_a(DB_SUCCESS == lock_clust_rec_cons_read_check(
						btr_pcur_get_rec(&pcur),
						index));
/*	ut_a(err == DB_SUCCESS); */
	
	ufield = upd_get_nth_field(update, 0);

	dfield_set_data(&(ufield->new_val), "1234", 5);

	ufield = upd_get_nth_field(update, 1);

	dfield_set_data(&(ufield->new_val), "1234", 5);

	ufield = upd_get_nth_field(update, 2);

	dfield_set_data(&(ufield->new_val), "1234", 5);

	mutex_enter(&kernel_mutex);	

	thr = que_fork_start_command(fork2, SESS_COMM_EXECUTE, 0);

	mutex_exit(&kernel_mutex);

	que_run_threads(thr);

	btr_pcur_close(&pcur);
	/*-------------------------------------*/
	/* INSERT INTO 'ORDERLINE' */

/*	printf("Trx %lu %lu starts, thr %lu\n",
						ut_dulint_get_low(trx->id),
						(ulint)trx,
						*((ulint*)arg)); */

	dtuple_gen_test_tuple3(row, count * 13 + j, DTUPLE_TEST_FIXED30, buf);

	ins_node_reset(inode);

	mutex_enter(&kernel_mutex);	

	thr = que_fork_start_command(fork1, SESS_COMM_EXECUTE, 0);

	mutex_exit(&kernel_mutex);

	que_run_threads(thr);

}
	/*-------------------------------------*/
	/* COMMIT */

	mutex_enter(&kernel_mutex);	

	thr = que_fork_start_command(cfork, SESS_COMM_EXECUTE, 0);
	
	mutex_exit(&kernel_mutex);

	que_run_threads(thr);

/*	printf("Trx %lu %lu committed\n", ut_dulint_get_low(trx->id),
								(ulint)trx); */
	count++;

	if (count < *((ulint*)arg)) {
		ut_a(trx_start(trx, ULINT_UNDEFINED));	

		goto loop;
	}

	tm = ut_clock();
	printf("Wall time for TPC-C %lu trxs %lu milliseconds\n",
					count, tm - oldtm);

	btr_search_index_print_info(index);
	btr_search_index_print_info(dict_table_get_first_index(table));
					
/*	mem_print_info(); */
	/*-------------------------------------*/
/*	dict_table_print_by_name("TS_TABLE2"); 
	dict_table_print_by_name("TS_TABLE3"); */

	return(0);
}

/*********************************************************************
Init for purge test. */

ulint
test10_1(
/*=====*/
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
	ulint		thr_no;

	thr_no = *((ulint*)arg);

	printf("-------------------------------------------------\n");
	printf("TEST 10.1. PURGE INIT\n");

	heap = mem_heap_create(512);

	com_endpoint = (com_endpoint_t*)heap;	/* This is a dummy non-NULL
						value */
	mutex_enter(&kernel_mutex);	

	sess = sess_open(ut_dulint_zero, com_endpoint, (byte*)"user1", 6);
	
	trx = sess->trx;

	mutex_exit(&kernel_mutex);

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

	for (i = 0; i < 200; i++) {
		
		dtuple_gen_test_tuple3(row, i * 100 + thr_no,
						DTUPLE_TEST_FIXED30, buf);
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

/*	dict_table_print_by_name("TS_TABLE1"); */

	return(0);
}

/*********************************************************************
Test for purge. */

ulint
test10_2(
/*=====*/
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
	dtuple_t*	entry;
	byte		buf[100];
	byte		buf2[1000];
	ulint		count = 0;
	btr_pcur_t	pcur;
	upd_t*		update;
	upd_field_t*	ufield;
	dict_tree_t*	tree;
	dict_index_t*	index;
	mtr_t		mtr;
	upd_node_t*	node;
	ulint		err;
	ulint		thr_no;
	ulint		tuple_no;

	printf("-------------------------------------------------\n");
	printf("TEST 10.2. PURGE TEST UPDATES\n");

	heap = mem_heap_create(512);

	com_endpoint = (com_endpoint_t*)heap;	/* This is a dummy non-NULL
						value */
	mutex_enter(&kernel_mutex);	

	sess = sess_open(ut_dulint_zero, com_endpoint, (byte*)"user1", 6);
	
	trx = sess->trx;

	mutex_exit(&kernel_mutex);
loop:
	/*-------------------------------------*/
	ut_a(trx_start(trx, ULINT_UNDEFINED));	

	fork = que_fork_create(NULL, NULL, QUE_FORK_UPDATE, heap);
	fork->trx = trx;

	thr = que_thr_create(fork, fork, heap);

	table = dict_table_get("TS_TABLE1", trx);

	update = upd_create(2, heap);

	node = upd_node_create(fork, thr, table, &pcur, update, heap);
	thr->child = node;

	node->cmpl_info = 0;

	mutex_enter(&kernel_mutex);

	que_graph_publish(fork, trx->sess);

	trx->graph = fork;

	mutex_exit(&kernel_mutex);

	rnd = 87607651;

	entry = dtuple_create(heap, 1);
	
	oldtm = ut_clock();

	thr_no = *((ulint*)arg);

	ut_a(DB_SUCCESS == lock_table(0, table, LOCK_IX, thr));

    for (i = 0; i < 200; i++) {

    	tuple_no = i;
	
	dtuple_gen_search_tuple3(entry, tuple_no * 100 + thr_no, buf);

	index = dict_table_get_first_index(table);
	tree = dict_index_get_tree(index);

	btr_pcur_set_mtr(&pcur, &mtr);
	
	mtr_start(&mtr);
	
	btr_pcur_open(index, entry, PAGE_CUR_LE, BTR_SEARCH_LEAF, &pcur, &mtr);

	btr_pcur_store_position(&pcur, &mtr);

/*	printf("Thread %lu to update row %lu\n", thr_no, tuple_no); */

	err = lock_clust_rec_read_check_and_lock(0, btr_pcur_get_rec(&pcur),
							index, LOCK_X, thr);
	ut_a(err == DB_SUCCESS);
	
	btr_pcur_commit(&pcur);	

	ufield = upd_get_nth_field(update, 0);

	upd_field_set_col_no(ufield, 0, table);

	dtuple_gen_search_tuple3(entry, tuple_no * 100 + 10 + thr_no, buf);

	dfield_set_data(&(ufield->new_val), dfield_get_data(
					dtuple_get_nth_field(entry, 0)),
				dfield_get_len(
					dtuple_get_nth_field(entry, 0)));
	ufield = upd_get_nth_field(update, 1);

	upd_field_set_col_no(ufield, 1, table);

	rnd += 98326761;
	
	dfield_set_data(&(ufield->new_val), buf2, rnd % 200);
	
	mutex_enter(&kernel_mutex);	

	ut_a(
	thr == que_fork_start_command(fork, SESS_COMM_EXECUTE, 0));

	mutex_exit(&kernel_mutex);

	que_run_threads(thr);

	fsp_validate(0);

    } /* for (i = ... */

	tm = ut_clock();
	printf("Wall time for %lu updates %lu milliseconds\n",
		i, tm - oldtm);

/*	dict_table_print_by_name("TS_TABLE1"); */

	printf("Validating tree\n");
	btr_validate_tree(tree);
	printf("Validated\n");

/*	lock_print_info(); */
	
/*	mem_print_info(); */

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

	count++;

	if (count < 1) {

		goto loop;
	}

	return(0);
}

/*********************************************************************
Test for purge. */

ulint
test10_2_r(
/*=======*/
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
	dtuple_t*	entry;
	byte		buf[100];
	byte		buf2[1000];
	ulint		count = 0;
	btr_pcur_t	pcur;
	upd_t*		update;
	upd_field_t*	ufield;
	dict_tree_t*	tree;
	dict_index_t*	index;
	mtr_t		mtr;
	upd_node_t*	node;
	ulint		err;
	ulint		thr_no;
	ulint		tuple_no;

	printf("-------------------------------------------------\n");
	printf("TEST 10.2. PURGE TEST UPDATES\n");

	heap = mem_heap_create(512);

	com_endpoint = (com_endpoint_t*)heap;	/* This is a dummy non-NULL
						value */
	mutex_enter(&kernel_mutex);	

	sess = sess_open(ut_dulint_zero, com_endpoint, (byte*)"user1", 6);
	
	trx = sess->trx;

	mutex_exit(&kernel_mutex);
loop:
	/*-------------------------------------*/
	ut_a(trx_start(trx, ULINT_UNDEFINED));	

	fork = que_fork_create(NULL, NULL, QUE_FORK_UPDATE, heap);
	fork->trx = trx;

	thr = que_thr_create(fork, fork, heap);

	table = dict_table_get("TS_TABLE1", trx);

	update = upd_create(2, heap);

	node = upd_node_create(fork, thr, table, &pcur, update, heap);
	thr->child = node;

	node->cmpl_info = 0;

	mutex_enter(&kernel_mutex);

	que_graph_publish(fork, trx->sess);

	trx->graph = fork;

	mutex_exit(&kernel_mutex);

	rnd = 87607651;

	entry = dtuple_create(heap, 1);
	
	oldtm = ut_clock();

	thr_no = *((ulint*)arg);

	ut_a(DB_SUCCESS == lock_table(0, table, LOCK_IX, thr));

    for (i = 0; i < 200; i++) {

    	tuple_no = i;
	
	dtuple_gen_search_tuple3(entry, tuple_no * 100 + thr_no, buf);

	index = dict_table_get_first_index(table);
	tree = dict_index_get_tree(index);

	btr_pcur_set_mtr(&pcur, &mtr);
	
	mtr_start(&mtr);
	
	btr_pcur_open(index, entry, PAGE_CUR_LE, BTR_SEARCH_LEAF, &pcur, &mtr);

	btr_pcur_store_position(&pcur, &mtr);

/*	printf("Thread %lu to update row %lu\n", thr_no, tuple_no); */

	err = lock_clust_rec_read_check_and_lock(0, btr_pcur_get_rec(&pcur),
							index, LOCK_X, thr);
	ut_a(err == DB_SUCCESS);
	
	btr_pcur_commit(&pcur);	

	ufield = upd_get_nth_field(update, 0);

	upd_field_set_col_no(ufield, 0, table);

	dtuple_gen_search_tuple3(entry, tuple_no * 100 + 10 + thr_no, buf);

	dfield_set_data(&(ufield->new_val), dfield_get_data(
					dtuple_get_nth_field(entry, 0)),
				dfield_get_len(
					dtuple_get_nth_field(entry, 0)));
	ufield = upd_get_nth_field(update, 1);

	upd_field_set_col_no(ufield, 1, table);

	rnd += 98326761;
	
	dfield_set_data(&(ufield->new_val), buf2, rnd % 2000);
	
	mutex_enter(&kernel_mutex);	

	ut_a(
	thr == que_fork_start_command(fork, SESS_COMM_EXECUTE, 0));

	mutex_exit(&kernel_mutex);

	que_run_threads(thr);

	fsp_validate(0);

    } /* for (i = ... */

	tm = ut_clock();
	printf("Wall time for %lu updates %lu milliseconds\n",
		i, tm - oldtm);

/*	dict_table_print_by_name("TS_TABLE1"); */

	printf("Validating tree\n");
	btr_validate_tree(tree);
	printf("Validated\n");

/*	lock_print_info(); */
	
	mem_pool_print_info(mem_comm_pool);

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

	os_thread_sleep(2000000);
		
	count++;

	if (count < 1) {

		goto loop;
	}

	return(0);
}

/*********************************************************************
Test for purge. */

ulint
test10_3(
/*=====*/
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
	dtuple_t*	entry;
	byte		buf[100];
	btr_pcur_t	pcur;
	dict_tree_t*	tree;
	dict_index_t*	index;
	mtr_t		mtr;
	del_node_t*	node;
	ulint		err;
	ulint		thr_no;
	ulint		tuple_no;

	printf("-------------------------------------------------\n");
	printf("TEST 10.3. PURGE TEST DELETES\n");

	heap = mem_heap_create(512);

	com_endpoint = (com_endpoint_t*)heap;	/* This is a dummy non-NULL
						value */
	mutex_enter(&kernel_mutex);	

	sess = sess_open(ut_dulint_zero, com_endpoint, (byte*)"user1", 6);
	
	trx = sess->trx;

	mutex_exit(&kernel_mutex);

	/*-------------------------------------*/
	ut_a(trx_start(trx, ULINT_UNDEFINED));	

	fork = que_fork_create(NULL, NULL, QUE_FORK_UPDATE, heap);
	fork->trx = trx;

	thr = que_thr_create(fork, fork, heap);

	table = dict_table_get("TS_TABLE1", trx);

	node = del_node_create(fork, thr, table, &pcur, heap);
	thr->child = node;

	mutex_enter(&kernel_mutex);

	que_graph_publish(fork, trx->sess);

	trx->graph = fork;

	mutex_exit(&kernel_mutex);

	entry = dtuple_create(heap, 1);
	
	oldtm = ut_clock();

	thr_no = *((ulint*)arg);

	ut_a(DB_SUCCESS == lock_table(0, table, LOCK_IX, thr));

    for (i = 0; i < 200; i++) {

    	rnd = i;
	tuple_no = rnd;
	
	dtuple_gen_search_tuple3(entry, tuple_no * 100 + 10 + thr_no, buf);

	index = dict_table_get_first_index(table);
	tree = dict_index_get_tree(index);

	btr_pcur_set_mtr(&pcur, &mtr);
	
	mtr_start(&mtr);
	
	btr_pcur_open(index, entry, PAGE_CUR_LE, BTR_SEARCH_LEAF, &pcur, &mtr);

	btr_pcur_store_position(&pcur, &mtr);

/*	printf("Thread %lu to update row %lu\n", thr_no, tuple_no); */

	err = lock_clust_rec_read_check_and_lock(0, btr_pcur_get_rec(&pcur),
							index, LOCK_X, thr);
	ut_a(err == DB_SUCCESS);
	
	btr_pcur_commit(&pcur);	

	mutex_enter(&kernel_mutex);	

	ut_a(
	thr == que_fork_start_command(fork, SESS_COMM_EXECUTE, 0));

	mutex_exit(&kernel_mutex);

	que_run_threads(thr);

    } /* for (i = ... */

	tm = ut_clock();
	printf("Wall time for %lu delete markings %lu milliseconds\n",
		i, tm - oldtm);

/*	dict_table_print_by_name("TS_TABLE1"); */

	printf("Validating tree\n");
	btr_validate_tree(tree);
	printf("Validated\n");

/*	lock_print_info(); */
	
/*	mem_print_info(); */

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

	return(0);
}

/*********************************************************************
Test for purge. */

ulint
test10_5(
/*=====*/
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
	dtuple_t*	entry;
	byte		buf[100];
	btr_pcur_t	pcur;
	dict_tree_t*	tree;
	dict_index_t*	index;
	mtr_t		mtr;
	del_node_t*	node;
	ulint		err;
	ulint		thr_no;
	ulint		tuple_no;

	printf("-------------------------------------------------\n");
	printf("TEST 10.5. PURGE TEST UNCOMMITTED DELETES\n");

	heap = mem_heap_create(512);

	com_endpoint = (com_endpoint_t*)heap;	/* This is a dummy non-NULL
						value */
	mutex_enter(&kernel_mutex);	

	sess = sess_open(ut_dulint_zero, com_endpoint, (byte*)"user1", 6);
	
	trx = sess->trx;

	mutex_exit(&kernel_mutex);

	/*-------------------------------------*/
	ut_a(trx_start(trx, ULINT_UNDEFINED));	

	fork = que_fork_create(NULL, NULL, QUE_FORK_UPDATE, heap);
	fork->trx = trx;

	thr = que_thr_create(fork, fork, heap);

	table = dict_table_get("TS_TABLE1", trx);

	node = del_node_create(fork, thr, table, &pcur, heap);
	thr->child = node;

	mutex_enter(&kernel_mutex);

	que_graph_publish(fork, trx->sess);

	trx->graph = fork;

	mutex_exit(&kernel_mutex);

	entry = dtuple_create(heap, 1);
	
	oldtm = ut_clock();

	thr_no = *((ulint*)arg);

	ut_a(DB_SUCCESS == lock_table(0, table, LOCK_IX, thr));

    for (i = 0; i < 50; i++) {

    	rnd = i;
	tuple_no = rnd % 100;
	
	dtuple_gen_search_tuple3(entry, tuple_no * 100 + 10 + thr_no, buf);

	index = dict_table_get_first_index(table);
	tree = dict_index_get_tree(index);

	btr_pcur_set_mtr(&pcur, &mtr);
	
	mtr_start(&mtr);
	
	btr_pcur_open(index, entry, PAGE_CUR_LE, BTR_SEARCH_LEAF, &pcur, &mtr);

	btr_pcur_store_position(&pcur, &mtr);

/*	printf("Thread %lu to update row %lu\n", thr_no, tuple_no); */

	err = lock_clust_rec_read_check_and_lock(0, btr_pcur_get_rec(&pcur),
							index, LOCK_X, thr);
	ut_a(err == DB_SUCCESS);
	
	btr_pcur_commit(&pcur);	

	mutex_enter(&kernel_mutex);	

	ut_a(
	thr == que_fork_start_command(fork, SESS_COMM_EXECUTE, 0));

	mutex_exit(&kernel_mutex);

	que_run_threads(thr);

    } /* for (i = ... */

	tm = ut_clock();
	printf("Wall time for %lu delete markings %lu milliseconds\n",
		i, tm - oldtm);

/*	dict_table_print_by_name("TS_TABLE1"); */

	printf("Validating tree\n");
	btr_validate_tree(tree);
	printf("Validated\n");

/*	lock_print_info(); */
	
/*	mem_print_info(); */

	return(0);
}

/*********************************************************************
Multithreaded test for purge. */

ulint
test10mt(
/*=====*/
	void*	arg)
{
	ulint	i;
	ulint	thr_no;

	thr_no = *((ulint*)arg);

	printf("Thread %lu starts purge test\n", thr_no);

	for (i = 0; i < 2; i++) {
		test10_1(arg);

		sync_print();

		fsp_validate(0);

		test10_2_r(arg);
		sync_print();

		test10_2(arg);
		sync_print();

		lock_validate();

		test10_3(arg);
		sync_print();
	}

	printf("Thread %lu ends purge test\n", thr_no);

	return(0);
}	

/*********************************************************************
Purge test. */

ulint
test10_4(
/*=====*/
	void*	arg)
{
	ulint	i;

	UT_NOT_USED(arg);

	printf("-------------------------------------------------\n");
	printf("TEST 10.4. PURGE TEST\n");

	for (i = 0; i < 30; i++) {
		trx_purge();

		printf("%lu pages purged\n", purge_sys->n_pages_handled);

		os_thread_sleep(5000000);
	}
	
/*	dict_table_print_by_name("TS_TABLE1"); */

	return(0);
}

/*********************************************************************
This thread is used to test insert buffer merge. */

ulint
test_ibuf_merge(
/*============*/
	void*	arg)
{
	ulint	sum_sizes;
	ulint	volume;

	ut_ad(arg);

	printf("Starting ibuf merge\n");

	sum_sizes = 0;
	volume = 1;
	
	while (volume) {
		volume = ibuf_contract(FALSE);

		sum_sizes += volume;
	}

	printf("Ibuf merged %lu bytes\n", sum_sizes);
	
	os_thread_sleep(5000000);

	return(0);
}

/*********************************************************************
This thread is used to measure contention of latches. */

ulint
test_measure_cont(
/*==============*/
	void*	arg)
{
	ulint	i, j;
	ulint	count;

	ut_ad(arg);

	printf("Starting contention measurement\n");
	
	for (i = 0; i < 1000; i++) {
		count = 0;

		for (j = 0; j < 100; j++) {
	
			os_thread_sleep(10000);

			if ((&(buf_pool->mutex))->lock_word) {

				count++;
			}
		}

		printf("Mutex reserved %lu of %lu peeks\n", count, j);
	}

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
	char	buf[100];

/*	buf_debug_prints = TRUE; */
	log_do_write = TRUE;
	btr_search_use_hash = TRUE;
	log_debug_writes = TRUE;
	
	srv_boot("initfile");
	os_aio_init(576, 9, 100);
	fil_init(25);
	buf_pool_init(POOL_SIZE, POOL_SIZE);
	fsp_init();
	log_init();
	lock_sys_create(1024);

	create_files();
	create_log_files();
	
	init_spaces();

	sess_sys_init_at_db_start();
	
	trx_sys_create();
	
	dict_create();

	log_make_checkpoint_at(ut_dulint_max);
/*	log_debug_writes = TRUE; */

/*	os_thread_sleep(500000); */

	oldtm = ut_clock();
	
	ut_rnd_set_seed(19);

	test1(NULL);
/*	test1_5(NULL);
	test1_6(NULL);
	test1_7(NULL); */
	
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
	
	log_make_checkpoint_at(ut_dulint_max);

	n2 = 100;

/*	test2_1(&n2);

	log_flush_up_to(ut_dulint_max, LOG_WAIT_ALL_GROUPS); */

/*	sync_print();

	test9A(&n2);

	sync_print();

	log_print();
	
	test9(&n2);

	log_print();
	
	sync_print(); */
/*	test6(&n2); */

/*	test2_2(&n2); */

/*	test3(&n2); */

/*	mem_print_info(); */

	log_archive_stop();
	log_archive_start();

	ut_a(DB_SUCCESS == log_switch_backup_state_on());

	printf("Type: kukkuu<enter>\n");
	scanf("%s", buf);

	ut_a(DB_SUCCESS == log_switch_backup_state_off());

	for (i = 0; i < 2; i++) {

		n1000[i] = 500 + 10 * i;
		id[i] = id[i];
/*
		os_thread_create(test2mt, n1000 + i, id + i);
		os_thread_create(test2_1mt, n1000 + i, id + i);
		os_thread_create(test2_2mt, n1000 + i, id + i);
*/	}

	n2 = 5000;

/*	fsp_print(0); */

	test2_1(&n2);

	for (i = 0; i < 20; i++) {
		log_archive_stop();
		log_archive_start();
	}

/*	test2(&n2);
	test2(&n2); */
	
/*	buf_print();
	ibuf_print();
	rw_lock_list_print_info();
	mutex_list_print_info(); */
	
/*	dict_table_print_by_name("TS_TABLE1"); */

/*	mem_print_info(); */
/*
	n2 = 100;

	test4_1();
	test4_2();

	for (i = 0; i < 2; i++) {
		n1000[i] = i;
		id[i] = id[i];
		os_thread_create(test4mt, n1000 + i, id + i);
	}

	n2 = 4;
	test4mt(&n2);

	log_archive_stop();
	log_archive_start();

	test4mt(&n2);
*/
/*	test4_2(); */
/*
	lock_print_info();
*/
/*	test7(&n2); */

/*	os_thread_sleep(25000000); */

/*	ut_a(DB_SUCCESS == log_switch_backup_state_off()); */

/*	recv_compare_spaces(0, 1, 100); */

	log_flush_up_to(ut_dulint_max, LOG_WAIT_ALL_GROUPS);

	printf("Type: kukkuu<enter>\n");
	scanf("%s", buf);

	buf_flush_batch(BUF_FLUSH_LIST, ULINT_MAX, ut_dulint_max);
	buf_flush_wait_batch_end(BUF_FLUSH_LIST);
	
/*	log_make_checkpoint_at(ut_dulint_max); */

/*	dict_table_print_by_name("TS_TABLE1"); */

/*	buf_print(); */
	
	tm = ut_clock();
	printf("Wall time for test %lu milliseconds\n", tm - oldtm);
	printf("TESTS COMPLETED SUCCESSFULLY!\n");
}
