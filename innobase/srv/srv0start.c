/************************************************************************
Starts the InnoDB database server

(c) 1996-2000 Innobase Oy

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
#include "buf0rea.h"
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
#include "row0mysql.h"
#include "lock0lock.h"
#include "ibuf0ibuf.h"
#include "pars0pars.h"
#include "btr0sea.h"
#include "srv0start.h"
#include "que0que.h"

ibool           srv_startup_is_before_trx_rollback_phase = FALSE;
ibool           srv_is_being_started = FALSE;
ibool           srv_was_started      = FALSE;

ibool		measure_cont	= FALSE;

os_file_t	files[1000];

mutex_t		ios_mutex;
ulint		ios;

ulint		n[SRV_MAX_N_IO_THREADS + 5];
os_thread_id_t	thread_ids[SRV_MAX_N_IO_THREADS + 5];

#define SRV_N_PENDING_IOS_PER_THREAD 	OS_AIO_N_PENDING_IOS_PER_THREAD
#define SRV_MAX_N_PENDING_SYNC_IOS	100

/* The following limit may be too big in some old operating systems:
we may get an assertion failure in os0file.c */

#define SRV_MAX_N_OPEN_FILES		500

#define SRV_LOG_SPACE_FIRST_ID		1000000000

/************************************************************************
I/o-handler thread function. */
static

#ifndef __WIN__
void*
#else
ulint
#endif
io_handler_thread(
/*==============*/
	void*	arg)
{
	ulint	segment;
	ulint	i;
	
	segment = *((ulint*)arg);

/*	printf("Io handler thread %lu starts\n", segment); */

	for (i = 0;; i++) {
		fil_aio_wait(segment);

		mutex_enter(&ios_mutex);
		ios++;
		mutex_exit(&ios_mutex);
	}

#ifndef __WIN__
	return(NULL);
#else
	return(0);
#endif
}

#ifdef __WIN__
#define SRV_PATH_SEPARATOR	"\\"
#else
#define SRV_PATH_SEPARATOR	"/"
#endif

/*************************************************************************
Normalizes a directory path for Windows: converts slashes to backslashes. */
static
void
srv_normalize_path_for_win(
/*=======================*/
	char*	str)	/* in/out: null-terminated character string */
{
#ifdef __WIN__
	ulint	i;

	for (i = 0; i < ut_strlen(str); i++) {

		if (str[i] == '/') {
			str[i] = '\\';
		}
	}
#endif
}
	
/*************************************************************************
Adds a slash or a backslash to the end of a string if it is missing
and the string is not empty. */
static
char*
srv_add_path_separator_if_needed(
/*=============================*/
			/* out, own: string which has the separator if the
			string is not empty */
	char*	str)	/* in: null-terminated character string */
{
	char*	out_str;

	if (ut_strlen(str) == 0) {

		return(str);
	}

	if (str[ut_strlen(str) - 1] == SRV_PATH_SEPARATOR[0]) {
		out_str = ut_malloc(ut_strlen(str) + 1);
		
		sprintf(out_str, "%s", str);

		return(out_str);
	}
		
	out_str = ut_malloc(ut_strlen(str) + 2);
		
	sprintf(out_str, "%s%s", str, SRV_PATH_SEPARATOR);

	return(out_str);
}

/*************************************************************************
Creates or opens the log files. */
static
ulint
open_or_create_log_file(
/*====================*/
					/* out: DB_SUCCESS or error code */
	ibool	create_new_db,		/* in: TRUE if we should create a
					new database */
	ibool*	log_file_created,	/* out: TRUE if new log file
					created */
	ulint	k,			/* in: log group number */
	ulint	i)			/* in: log file number in group */
{
	ibool	ret;
	ulint	arch_space_id;
	ulint	size;
	ulint	size_high;
	char	name[10000];

	UT_NOT_USED(create_new_db);

	*log_file_created = FALSE;

	srv_normalize_path_for_win(srv_log_group_home_dirs[k]);
	srv_log_group_home_dirs[k] = srv_add_path_separator_if_needed(
						srv_log_group_home_dirs[k]);

	sprintf(name, "%s%s%lu", srv_log_group_home_dirs[k], "ib_logfile", i);

	files[i] = os_file_create(name, OS_FILE_CREATE, OS_FILE_NORMAL,
						OS_LOG_FILE, &ret);
	if (ret == FALSE) {
		if (os_file_get_last_error() != OS_FILE_ALREADY_EXISTS) {
			fprintf(stderr,
			"InnoDB: Error in creating or opening %s\n", name);
				
			return(DB_ERROR);
		}

		files[i] = os_file_create(
					name, OS_FILE_OPEN, OS_FILE_AIO,
							OS_LOG_FILE, &ret);
		if (!ret) {
			fprintf(stderr,
			"InnoDB: Error in opening %s\n", name);
				
			return(DB_ERROR);
		}

		ret = os_file_get_size(files[i], &size, &size_high);
		ut_a(ret);
		
		if (size != UNIV_PAGE_SIZE * srv_log_file_size
							|| size_high != 0) {
			fprintf(stderr,
			"InnoDB: Error: log file %s is of different size\n"
			"InnoDB: than specified in the .cnf file!\n", name);
				
			return(DB_ERROR);
		}					
	} else {
		*log_file_created = TRUE;
					
		fprintf(stderr,
		"InnoDB: Log file %s did not exist: new to be created\n",
									name);
		fprintf(stderr, "InnoDB: Setting log file %s size to %lu\n",
			             name, UNIV_PAGE_SIZE * srv_log_file_size);

		ret = os_file_set_size(name, files[i],
					UNIV_PAGE_SIZE * srv_log_file_size, 0);
		if (!ret) {
			fprintf(stderr,
		"InnoDB: Error in creating %s: probably out of disk space\n",
			name);

			return(DB_ERROR);
		}
	}

	ret = os_file_close(files[i]);
	ut_a(ret);

	if (i == 0) {
		/* Create in memory the file space object
		which is for this log group */
				
		fil_space_create(name,
		2 * k + SRV_LOG_SPACE_FIRST_ID, FIL_LOG);
	}

	ut_a(fil_validate());

	fil_node_create(name, srv_log_file_size,
					2 * k + SRV_LOG_SPACE_FIRST_ID);

	/* If this is the first log group, create the file space object
	for archived logs */

	if (k == 0 && i == 0) {
		arch_space_id = 2 * k + 1 + SRV_LOG_SPACE_FIRST_ID;

	    	fil_space_create("arch_log_space", arch_space_id,
								FIL_LOG);
	} else {
		arch_space_id = ULINT_UNDEFINED;
	}

	if (i == 0) {
		log_group_init(k, srv_n_log_files,
				srv_log_file_size * UNIV_PAGE_SIZE,
				2 * k + SRV_LOG_SPACE_FIRST_ID,
				arch_space_id);
	}

	return(DB_SUCCESS);
}

/*************************************************************************
Creates or opens database data files. */
static
ulint
open_or_create_data_files(
/*======================*/
				/* out: DB_SUCCESS or error code */
	ibool*	create_new_db,	/* out: TRUE if new database should be
								created */
	dulint*	min_flushed_lsn,/* out: min of flushed lsn values in data
				files */
	ulint*	min_arch_log_no,/* out: min of archived log numbers in data
				files */
	dulint*	max_flushed_lsn,/* out: */
	ulint*	max_arch_log_no,/* out: */
	ulint*	sum_of_new_sizes)/* out: sum of sizes of the new files added */
{
	ibool	ret;
	ulint	i;
	ibool	one_opened	= FALSE;
	ibool	one_created	= FALSE;
	ulint	size;
	ulint	size_high;
	char	name[10000];

	if (srv_n_data_files >= 1000) {
		fprintf(stderr, "InnoDB: can only have < 1000 data files\n"
				"InnoDB: you have defined %lu\n",
				srv_n_data_files);
		return(DB_ERROR);
	}

	*sum_of_new_sizes = 0;
	
	*create_new_db = FALSE;

	srv_normalize_path_for_win(srv_data_home);
	srv_data_home = srv_add_path_separator_if_needed(srv_data_home);

	for (i = 0; i < srv_n_data_files; i++) {
		srv_normalize_path_for_win(srv_data_file_names[i]);

		sprintf(name, "%s%s", srv_data_home, srv_data_file_names[i]);
	
		files[i] = os_file_create(name, OS_FILE_CREATE,
					OS_FILE_NORMAL, OS_DATA_FILE, &ret);

		if (srv_data_file_is_raw_partition[i] == SRV_NEW_RAW) {
			/* The partition is opened, not created; then it is
			written over */

			srv_created_new_raw = TRUE;

			files[i] = os_file_create(
				name, OS_FILE_OPEN, OS_FILE_NORMAL,
						OS_DATA_FILE, &ret);
			if (!ret) {
				fprintf(stderr,
				"InnoDB: Error in opening %s\n", name);

				return(DB_ERROR);
			}
		} else if (srv_data_file_is_raw_partition[i] == SRV_OLD_RAW) {
			ret = FALSE;
		}

		if (ret == FALSE) {
			if (srv_data_file_is_raw_partition[i] != SRV_OLD_RAW
			    && os_file_get_last_error() !=
						OS_FILE_ALREADY_EXISTS) {
				fprintf(stderr,
				"InnoDB: Error in creating or opening %s\n",
				name);

				return(DB_ERROR);
			}

			if (one_created) {
				fprintf(stderr,
	"InnoDB: Error: data files can only be added at the end\n");
				fprintf(stderr,
	"InnoDB: of a tablespace, but data file %s existed beforehand.\n",
				name);
				return(DB_ERROR);
			}
				
			files[i] = os_file_create(
				name, OS_FILE_OPEN, OS_FILE_NORMAL,
						OS_DATA_FILE, &ret);
			if (!ret) {
				fprintf(stderr,
				"InnoDB: Error in opening %s\n", name);
				os_file_get_last_error();

				return(DB_ERROR);
			}

			if (srv_data_file_is_raw_partition[i] != SRV_OLD_RAW) {
			
				ret = os_file_get_size(files[i], &size,
								&size_high);
				ut_a(ret);
		
				if (size !=
					UNIV_PAGE_SIZE * srv_data_file_sizes[i]
		    					|| size_high != 0) {
					fprintf(stderr,
			"InnoDB: Error: data file %s is of different size\n"
			"InnoDB: than specified in the .cnf file!\n", name);
				
					return(DB_ERROR);
				}
			}

			fil_read_flushed_lsn_and_arch_log_no(files[i],
					one_opened,
					min_flushed_lsn, min_arch_log_no,
					max_flushed_lsn, max_arch_log_no);
			one_opened = TRUE;
		} else {
			one_created = TRUE;

			if (i > 0) {
				fprintf(stderr, 
		"InnoDB: Data file %s did not exist: new to be created\n",
									name);
			} else {
				fprintf(stderr, 
 		"InnoDB: The first specified data file %s did not exist:\n"
		"InnoDB: a new database to be created!\n", name);
				*create_new_db = TRUE;
			}
			
			fprintf(stderr, "InnoDB: Setting file %s size to %lu\n",
			       name, UNIV_PAGE_SIZE * srv_data_file_sizes[i]);

			fprintf(stderr,
	    "InnoDB: Database physically writes the file full: wait...\n");

			ret = os_file_set_size(name, files[i],
				UNIV_PAGE_SIZE * srv_data_file_sizes[i], 0);

			if (!ret) {
				fprintf(stderr, 
	"InnoDB: Error in creating %s: probably out of disk space\n", name);

				return(DB_ERROR);
			}

			*sum_of_new_sizes = *sum_of_new_sizes
						+ srv_data_file_sizes[i];
		}

		ret = os_file_close(files[i]);
		ut_a(ret);

		if (i == 0) {
			fil_space_create(name, 0, FIL_TABLESPACE);
		}

		ut_a(fil_validate());

		fil_node_create(name, srv_data_file_sizes[i], 0);
	}

	ios = 0;

	mutex_create(&ios_mutex);
	mutex_set_level(&ios_mutex, SYNC_NO_ORDER_CHECK);

	return(DB_SUCCESS);
}

/*********************************************************************
This thread is used to measure contention of latches. */
static
ulint
test_measure_cont(
/*==============*/
	void*	arg)
{
	ulint	i, j;
	ulint	pcount, kcount, s_scount, s_xcount, s_mcount, lcount;

	UT_NOT_USED(arg);

	fprintf(stderr, "Starting contention measurement\n");
	
	for (i = 0; i < 1000; i++) {

		pcount = 0;
		kcount = 0;
		s_scount = 0;
		s_xcount = 0;
		s_mcount = 0;
		lcount = 0;

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
		}

		fprintf(stderr, 
	"Mutex res. l %lu, p %lu, k %lu s x %lu s s %lu s mut %lu of %lu\n",
		lcount, pcount, kcount, s_xcount, s_scount, s_mcount, j);

		sync_print_wait_info();

		fprintf(stderr, 
    "log i/o %lu n non sea %lu n succ %lu n h fail %lu\n",
			log_sys->n_log_ios, btr_cur_n_non_sea,
			btr_search_n_succ, btr_search_n_hash_fail);
	}

	return(0);
}

/********************************************************************
Starts InnoDB and creates a new database if database files
are not found and the user wants. Server parameters are
read from a file of name "srv_init" in the ib_home directory. */

int
innobase_start_or_create_for_mysql(void)
/*====================================*/
				/* out: DB_SUCCESS or error code */
{
	ibool	create_new_db;
	ibool	log_file_created;
	ibool	log_created	= FALSE;
	ibool	log_opened	= FALSE;
	dulint	min_flushed_lsn;
	dulint	max_flushed_lsn;
	ulint	min_arch_log_no;
	ulint	max_arch_log_no;
	ibool	start_archive;
	ulint   sum_of_new_sizes;
	ulint	err;
	ulint	i;
	ulint	k;
	mtr_t   mtr;

	log_do_write = TRUE;
/*	yydebug = TRUE; */

	srv_is_being_started = TRUE;
        srv_startup_is_before_trx_rollback_phase = TRUE;

	if (0 == ut_strcmp(srv_unix_file_flush_method_str, "fdatasync")) {
	  	srv_unix_file_flush_method = SRV_UNIX_FDATASYNC;

	} else if (0 == ut_strcmp(srv_unix_file_flush_method_str, "O_DSYNC")) {
	  	srv_unix_file_flush_method = SRV_UNIX_O_DSYNC;

	} else if (0 == ut_strcmp(srv_unix_file_flush_method_str,
				  "littlesync")) {
	  	srv_unix_file_flush_method = SRV_UNIX_LITTLESYNC;

	} else if (0 == ut_strcmp(srv_unix_file_flush_method_str, "nosync")) {
	  	srv_unix_file_flush_method = SRV_UNIX_NOSYNC;
	} else {
	  	fprintf(stderr, 
          	"InnoDB: Unrecognized value %s for innodb_flush_method\n",
          				srv_unix_file_flush_method_str);
	  	return(DB_ERROR);
	}

	/*
	printf("srv_unix set to %lu\n", srv_unix_file_flush_method);
	*/
	os_aio_use_native_aio = srv_use_native_aio;

	err = srv_boot();

	if (err != DB_SUCCESS) {

		return((int) err);
	}

	/* Restrict the maximum number of file i/o threads */
	if (srv_n_file_io_threads > SRV_MAX_N_IO_THREADS) {
		srv_n_file_io_threads = SRV_MAX_N_IO_THREADS;
	}

#if !(defined(WIN_ASYNC_IO) || defined(POSIX_ASYNC_IO))
	/* In simulated aio we currently have use only for 4 threads */

	os_aio_use_native_aio = FALSE;

	srv_n_file_io_threads = 4;
#endif

#ifdef __WIN__
	if (os_get_os_version() == OS_WIN95
	    || os_get_os_version() == OS_WIN31) {

	  	/* On Win 95, 98, ME, and Win32 subsystem for Windows 3.1 use
	     	simulated aio */

	  	os_aio_use_native_aio = FALSE;
	  	srv_n_file_io_threads = 4;
	} else {
	  	/* On NT and Win 2000 always use aio */
	  	os_aio_use_native_aio = TRUE;
	}
#endif
	if (!os_aio_use_native_aio) {
		os_aio_init(4 * SRV_N_PENDING_IOS_PER_THREAD
						* srv_n_file_io_threads,
					srv_n_file_io_threads,
					SRV_MAX_N_PENDING_SYNC_IOS);
	} else {
		os_aio_init(SRV_N_PENDING_IOS_PER_THREAD
						* srv_n_file_io_threads,
					srv_n_file_io_threads,
					SRV_MAX_N_PENDING_SYNC_IOS);
	}
	
	fil_init(SRV_MAX_N_OPEN_FILES);

	buf_pool_init(srv_pool_size, srv_pool_size);

	fsp_init();
	log_init();
	
	lock_sys_create(srv_lock_table_size);

#ifdef POSIX_ASYNC_IO
	if (os_aio_use_native_aio) {
		/* There is only one thread per async io array:
		one for ibuf i/o, one for log i/o, one for ordinary reads,
		one for ordinary writes; we need only 4 i/o threads */

		srv_n_file_io_threads = 4;
	}
#endif
	/* Create i/o-handler threads: */

	for (i = 0; i < srv_n_file_io_threads; i++) {
		n[i] = i;

		os_thread_create(io_handler_thread, n + i, thread_ids + i);
    	}

	if (0 != ut_strcmp(srv_log_group_home_dirs[0], srv_arch_dir)) {
		fprintf(stderr,
	"InnoDB: Error: you must set the log group home dir in my.cnf the\n"
	"InnoDB: same as log arch dir.\n");

		return(DB_ERROR);
	}

	sum_of_new_sizes = 0;

	for (i = 0; i < srv_n_data_files; i++) {
		if (srv_data_file_sizes[i] >= 262144) {
		 	fprintf(stderr,
	"InnoDB: Error: file size must be < 4 GB, or on some OS's < 2 GB\n");

		  	return(DB_ERROR);
		}

		sum_of_new_sizes += srv_data_file_sizes[i];
	}

	if (sum_of_new_sizes < 640) {
		  fprintf(stderr,
		  "InnoDB: Error: tablespace size must be at least 10 MB\n");

		  return(DB_ERROR);
	}

	err = open_or_create_data_files(&create_new_db,
					&min_flushed_lsn, &min_arch_log_no,
					&max_flushed_lsn, &max_arch_log_no,
					&sum_of_new_sizes);
	if (err != DB_SUCCESS) {

	        fprintf(stderr, "InnoDB: Could not open data files\n");

		return((int) err);
	}

	if (!create_new_db) {
		/* If we are using the doublewrite method, we will
		check if there are half-written pages in data files,
		and restore them from the doublewrite buffer if
		possible */
		
		trx_sys_doublewrite_restore_corrupt_pages();
	}

	srv_normalize_path_for_win(srv_arch_dir);
	srv_arch_dir = srv_add_path_separator_if_needed(srv_arch_dir);

	for (k = 0; k < srv_n_log_groups; k++) {

		for (i = 0; i < srv_n_log_files; i++) {

			err = open_or_create_log_file(create_new_db,
						&log_file_created, k, i);
			if (err != DB_SUCCESS) {

				return((int) err);
			}

			if (log_file_created) {
				log_created = TRUE;
			} else {
				log_opened = TRUE;
			}

			if ((log_opened && create_new_db)
			    		|| (log_opened && log_created)) {
				fprintf(stderr, 
	"InnoDB: Error: all log files must be created at the same time.\n"
	"InnoDB: If you want bigger or smaller log files,\n"
	"InnoDB: shut down the database and make sure there\n"
	"InnoDB: were no errors in shutdown.\n"
	"InnoDB: Then delete the existing log files. Edit the .cnf file\n"
	"InnoDB: and start the database again.\n");

				return(DB_ERROR);
			}
			
		}
	}

	if (log_created && !create_new_db && !srv_archive_recovery) {

		if (ut_dulint_cmp(max_flushed_lsn, min_flushed_lsn) != 0
				|| max_arch_log_no != min_arch_log_no) {
			fprintf(stderr, 
		"InnoDB: Cannot initialize created log files because\n"
		"InnoDB: data files were not in sync with each other\n"
		"InnoDB: or the data files are corrupt./n");

			return(DB_ERROR);
		}

		if (ut_dulint_cmp(max_flushed_lsn, ut_dulint_create(0, 1000))
		    < 0) {
		    	fprintf(stderr,
		"InnoDB: Cannot initialize created log files because\n"
		"InnoDB: data files are corrupt, or new data files were\n"
		"InnoDB: created when the database was started previous\n"
		"InnoDB: time but the database was not shut down\n"
		"InnoDB: normally after that.\n");

			return(DB_ERROR);
		}

		mutex_enter(&(log_sys->mutex));

		recv_reset_logs(ut_dulint_align_down(max_flushed_lsn,
					OS_FILE_LOG_BLOCK_SIZE),
					max_arch_log_no + 1, TRUE);
		
		mutex_exit(&(log_sys->mutex));
	}

	sess_sys_init_at_db_start();

	if (create_new_db) {
		mtr_start(&mtr);

		fsp_header_init(0, sum_of_new_sizes, &mtr);		

		mtr_commit(&mtr);

		trx_sys_create();
		dict_create();
                srv_startup_is_before_trx_rollback_phase = FALSE;

	} else if (srv_archive_recovery) {
		fprintf(stderr,
	"InnoDB: Starting archive recovery from a backup...\n");
	
		err = recv_recovery_from_archive_start(
					min_flushed_lsn,
					srv_archive_recovery_limit_lsn,
					min_arch_log_no);
		if (err != DB_SUCCESS) {

			return(DB_ERROR);
		}

		/* Since ibuf init is in dict_boot, and ibuf is needed
		in any disk i/o, first call dict_boot */

		dict_boot();

		trx_sys_init_at_db_start();
		
                srv_startup_is_before_trx_rollback_phase = FALSE;

		recv_recovery_from_archive_finish();
	} else {
		/* We always try to do a recovery, even if the database had
		been shut down normally */
		
		err = recv_recovery_from_checkpoint_start(LOG_CHECKPOINT,
							ut_dulint_max,
							min_flushed_lsn,
							max_flushed_lsn);
		if (err != DB_SUCCESS) {

			return(DB_ERROR);
		}

		/* Since ibuf init is in dict_boot, and ibuf is needed
		in any disk i/o, first call dict_boot */
		dict_boot();
		trx_sys_init_at_db_start();

		/* The following needs trx lists which are initialized in
		trx_sys_init_at_db_start */

                srv_startup_is_before_trx_rollback_phase = FALSE;
		recv_recovery_from_checkpoint_finish();
	}
	
	if (!create_new_db && sum_of_new_sizes > 0) {
		/* New data file(s) were added */
		mtr_start(&mtr);

		fsp_header_inc_size(0, sum_of_new_sizes, &mtr);		

		mtr_commit(&mtr);
	}

	log_make_checkpoint_at(ut_dulint_max, TRUE);

	if (!srv_log_archive_on) {
		ut_a(DB_SUCCESS == log_archive_noarchivelog());
	} else {
		mutex_enter(&(log_sys->mutex));

		start_archive = FALSE;

		if (log_sys->archiving_state == LOG_ARCH_OFF) {
			start_archive = TRUE;
		}

		mutex_exit(&(log_sys->mutex));

		if (start_archive) {
			ut_a(DB_SUCCESS == log_archive_archivelog());
		}
	}

	if (srv_measure_contention) {
	  	/* os_thread_create(&test_measure_cont, NULL, thread_ids +
                             	     SRV_MAX_N_IO_THREADS); */
	}

	/* fprintf(stderr, "Max allowed record size %lu\n",
				page_get_free_space_of_empty() / 2); */

	/* Create the thread which watches the timeouts for lock waits
	and prints InnoDB monitor info */
	
	os_thread_create(&srv_lock_timeout_and_monitor_thread, NULL,
					thread_ids + 2 + SRV_MAX_N_IO_THREADS);	

	/* Create the thread which warns of long semaphore waits */
	os_thread_create(&srv_error_monitor_thread, NULL,
					thread_ids + 3 + SRV_MAX_N_IO_THREADS);	

	srv_was_started = TRUE;
	srv_is_being_started = FALSE;

	sync_order_checks_on = TRUE;

	if (srv_use_doublewrite_buf && trx_doublewrite == NULL) {
		trx_sys_create_doublewrite_buf();
	}

	err = dict_create_or_check_foreign_constraint_tables();

	if (err != DB_SUCCESS) {
		return((int)DB_ERROR);
	}

	/* Create the master thread which monitors the database
	server, and does purge and other utility operations */

	os_thread_create(&srv_master_thread, NULL, thread_ids + 1 +
							SRV_MAX_N_IO_THREADS);
	/* buf_debug_prints = TRUE; */
	
	if (srv_print_verbose_log)
	{
	  ut_print_timestamp(stderr);
	  fprintf(stderr, "  InnoDB: Started\n");
	}
	return((int) DB_SUCCESS);
}

/********************************************************************
Shuts down the InnoDB database. */

int
innobase_shutdown_for_mysql(void) 
/*=============================*/
				/* out: DB_SUCCESS or error code */
{
        if (!srv_was_started) {
	  	if (srv_is_being_started) {
	    		ut_print_timestamp(stderr);
            		fprintf(stderr, 
	"  InnoDB: Warning: shutting down a not properly started\n");
	    		ut_print_timestamp(stderr);
            		fprintf(stderr, 
	"  InnoDB: or created database!\n");
	  	}

	  	return(DB_SUCCESS);
	}

	/* Flush buffer pool to disk, write the current lsn to
	the tablespace header(s), and copy all log data to archive */

	logs_empty_and_mark_files_at_shutdown();

	ut_free_all_mem();
	
	return((int) DB_SUCCESS);
}
