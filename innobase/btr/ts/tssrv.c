/************************************************************************
Test for the server

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
#include "row0sel.h"
#include "row0upd.h"
#include "row0row.h"
#include "lock0lock.h"
#include "ibuf0ibuf.h"
#include "pars0pars.h"
#include "btr0sea.h"

bool	measure_cont	= FALSE;

os_file_t	files[1000];

mutex_t		ios_mutex;
ulint		ios;
ulint		n[10];

mutex_t		incs_mutex;
ulint		incs;

byte		rnd_buf[67000];

ulint		glob_var1	= 0;
ulint		glob_var2	= 0;

mutex_t		mutex2;

mutex_t		test_mutex1;
mutex_t		test_mutex2;

mutex_t*	volatile mutexes;

bool		always_false	= FALSE;

ulint*		test_array;

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
	bool	ret;
	ulint	i, k;
	char	name[20];

	printf("--------------------------------------------------------\n");
	printf("Create or open log files\n");

	strcpy(name, "logfile00");

	for (k = 0; k < srv_n_log_groups; k++) {

	    for (i = 0; i < srv_n_log_files; i++) {

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
						8192 * srv_log_file_size, 0));
		}

		ret = os_file_close(files[i]);
		ut_a(ret);

		if (i == 0) {
			fil_space_create(name, k + 100, FIL_LOG);
		}

		ut_a(fil_validate());

		fil_node_create(name, srv_log_file_size, k + 100);
	    }
	    
	    fil_space_create(name, k + 200, FIL_LOG);
	    	    
	    log_group_init(k, srv_n_log_files,
				srv_log_file_size * UNIV_PAGE_SIZE,
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
	ulint		i, k;
	char		name[20];
	os_thread_t	thr[10];
	os_thread_id_t	id[10];

	printf("--------------------------------------------------------\n");
	printf("Create or open database files\n");
	
	strcpy(name, "tsfile00");

	for (k = 0; k < 2 * srv_n_spaces; k += 2) {
	    for (i = 0; i < srv_n_files; i++) {

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
					UNIV_PAGE_SIZE * srv_file_size, 0));
			/* Initialize the file contents to a random value */
			/*		
			for (j = 0; j < srv_file_size; j++) {

				for (c = 0; c < UNIV_PAGE_SIZE; c++) {

					rnd_buf[c] = 0xFF;
				}

				os_file_write(files[i], rnd_buf,
						UNIV_PAGE_SIZE * j, 0,
						UNIV_PAGE_SIZE);
			}
			*/
		}

		ret = os_file_close(files[i]);
		ut_a(ret);

		if (i == 0) {
			fil_space_create(name, k, FIL_TABLESPACE);
		}

		ut_a(fil_validate());

		fil_node_create(name, srv_file_size, k);
	    }
	}

	ios = 0;

	mutex_create(&ios_mutex);
	mutex_set_level(&ios_mutex, SYNC_NO_ORDER_CHECK);

	/* Create i/o-handler threads: */	

	for (i = 0; i < 9; i++) {
		n[i] = i;

		thr[i] = os_thread_create(handler_thread, n + i, id + i);
	}
}

/************************************************************************
Inits space header of space. */

void
init_spaces(void)
/*=============*/
{
	mtr_t	mtr;

	mtr_start(&mtr);

	fsp_header_init(0, srv_file_size * srv_n_files, &mtr);		

	mtr_commit(&mtr);
}

/*********************************************************************
This thread is used to measure contention of latches. */

ulint
test_measure_cont(
/*==============*/
	void*	arg)
{
	ulint	i, j;
	ulint	pcount, kcount, s_scount, s_xcount, s_mcount, lcount;
	ulint	t1count;
	ulint	t2count;

	UT_NOT_USED(arg);

	printf("Starting contention measurement\n");
	
	for (i = 0; i < 1000; i++) {

		pcount = 0;
		kcount = 0;
		s_scount = 0;
		s_xcount = 0;
		s_mcount = 0;
		lcount = 0;
		t1count	= 0;
		t2count	= 0;

		for (j = 0; j < 100; j++) {

		    if (srv_measure_by_spin) {
		    	ut_delay(ut_rnd_interval(0, 20000));
		    } else {
		    	os_thread_sleep(20000);
		    }

		    if (kernel_mutex.lock_word) {
			kcount++;
		    }

		    if (buf_pool->mutex.lock_word) {
		    	pcount++;
		    }

		    if (log_sys->mutex.lock_word) {
		    	lcount++;
		    }

		    if (btr_search_latch.reader_count) {
		    	s_scount++;
		    }

		    if (btr_search_latch.writer != RW_LOCK_NOT_LOCKED) {
		    	s_xcount++;
		    }

		    if (btr_search_latch.mutex.lock_word) {
		    	s_mcount++;
		    }

		    if (test_mutex1.lock_word) {
		    	t1count++;
		    }
		    
		    if (test_mutex2.lock_word) {
		    	t2count++;
		    }
		}

		printf(
	"Mutex res. l %lu, p %lu, k %lu s x %lu s s %lu s mut %lu of %lu\n",
		lcount, pcount, kcount, s_xcount, s_scount, s_mcount, j);

		sync_print_wait_info();

		printf(
    "log i/o %lu n non sea %lu n succ %lu n h fail %lu\n",
			log_sys->n_log_ios, btr_cur_n_non_sea,
			btr_search_n_succ, btr_search_n_hash_fail);
	}

	return(0);
}

/*********************************************************************
This thread is used to test contention of latches. */

ulint
test_sync(
/*======*/
	void*	arg)
{
	ulint	tm, oldtm;
	ulint	i, j;
	ulint	sum;
	ulint	rnd		= ut_rnd_gen_ulint();
	ulint	mut_ind;
	byte*	ptr;
	
	UT_NOT_USED(arg);

	printf("Starting mutex reservation test\n");

	oldtm = ut_clock();			

	sum = 0;
	rnd = 87354941;

	for (i = 0; i < srv_test_n_loops; i++) {
	
		for (j = 0; j < srv_test_n_free_rnds; j++) {
			rnd += 423087123;

			sum += test_array[rnd % (256 * srv_test_array_size)];
		}

		rnd += 43605677;

		mut_ind = rnd % srv_test_n_mutexes;

		mutex_enter(mutexes + mut_ind);
		
		for (j = 0; j < srv_test_n_reserved_rnds; j++) {
			rnd += 423087121;

			sum += test_array[rnd % (256 * srv_test_array_size)];
		}
		
		mutex_exit(mutexes + mut_ind);

		if (srv_test_cache_evict) {
			ptr = (byte*)(mutexes + mut_ind);

			for (j = 0; j < 4; j++) {
				ptr += 256 * 1024;
				sum += *((ulint*)ptr);
			}
		}
	}
	
	if (always_false) {
		printf("%lu", sum);
	}
	
	tm = ut_clock();	

	printf("Wall time for res. test %lu milliseconds\n", tm - oldtm);
	
	return(0);
}

/********************************************************************
Main test function. */

void 
main(void) 
/*======*/
{
	os_thread_id_t	thread_ids[1000];
	ulint		tm, oldtm;
	ulint		rnd;
	ulint		i, sum;
	byte*		ptr;
/*	mutex_t		mutex; */
	
	log_do_write = TRUE;
/*	yydebug = TRUE; */

	srv_boot("srv_init");

	os_aio_init(576, 9, 100);

	fil_init(25);

	buf_pool_init(srv_pool_size, srv_pool_size);

	fsp_init();
	log_init();

	lock_sys_create(srv_lock_table_size);

	create_files();
	create_log_files();
	
	init_spaces();

	sess_sys_init_at_db_start();
	
	trx_sys_create();

	dict_create();

	log_make_checkpoint_at(ut_dulint_max);

	printf("Hotspot semaphore addresses k %lx, p %lx, l %lx, s %lx\n",
		&kernel_mutex, &(buf_pool->mutex),
				&(log_sys->mutex), &btr_search_latch);
	
	if (srv_measure_contention) {
		os_thread_create(&test_measure_cont, NULL, thread_ids + 999);
	}

	if (!srv_log_archive_on) {

		ut_a(DB_SUCCESS == log_archive_noarchivelog());
	}

/*
	mutex_create(&mutex);

	oldtm = ut_clock();

	for (i = 0; i < 2000000; i++) {
	
		mutex_enter(&mutex);

		mutex_exit(&mutex);
	}

	tm = ut_clock();

	printf("Wall clock time for %lu mutex enter %lu milliseconds\n",
							i, tm - oldtm);
*/
	if (srv_test_sync) {
		if (srv_test_nocache) {
        		mutexes = os_mem_alloc_nocache(srv_test_n_mutexes
        						* sizeof(mutex_t));
		} else {
        		mutexes = mem_alloc(srv_test_n_mutexes
        						* sizeof(mutex_t));
        	}

        	sum = 0;

		rnd = 492314896;

		oldtm = ut_clock();

		for (i = 0; i < 4000000; i++) {

			rnd += 85967944;

			ptr = ((byte*)(mutexes)) + (rnd % (srv_test_n_mutexes
        					    	* sizeof(mutex_t)));
			sum += *((ulint*)ptr);
		}

		tm = ut_clock();

		printf(
		"Wall clock time for %lu random access %lu milliseconds\n",
							i, tm - oldtm);
		if (always_false) {
			printf("%lu", sum);
		}
	
		test_array = mem_alloc(4 * 256 * srv_test_array_size);

		for (i = 0; i < srv_test_n_mutexes; i++) {

			mutex_create(mutexes + i);
		}
	
		for (i = 0; i < srv_test_n_threads; i++) {
			os_thread_create(&test_sync, NULL, thread_ids + i);
		}
	}

	srv_master_thread(NULL);

	printf("TESTS COMPLETED SUCCESSFULLY!\n");

	os_process_exit(0);
}
