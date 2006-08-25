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
#include "dict0load.h"
#include "trx0sys.h"
#include "dict0crea.h"
#include "btr0btr.h"
#include "btr0pcur.h"
#include "btr0cur.h"
#include "btr0sea.h"
#include "rem0rec.h"
#include "srv0srv.h"
#include "que0que.h"
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

/* Log sequence number immediately after startup */
dulint		srv_start_lsn;
/* Log sequence number at shutdown */
dulint		srv_shutdown_lsn;

#ifdef HAVE_DARWIN_THREADS
# include <sys/utsname.h>
ibool		srv_have_fullfsync = FALSE;
#endif

ibool		srv_start_raw_disk_in_use  = FALSE;

static ibool	srv_start_has_been_called  = FALSE;

ulint           srv_sizeof_trx_t_in_ha_innodb_cc;

ibool           srv_startup_is_before_trx_rollback_phase = FALSE;
ibool           srv_is_being_started = FALSE;
static ibool	srv_was_started      = FALSE;

/* At a shutdown the value first climbs to SRV_SHUTDOWN_CLEANUP
and then to SRV_SHUTDOWN_LAST_PHASE */
ulint		srv_shutdown_state = 0;

ibool		measure_cont	= FALSE;

static os_file_t	files[1000];

static mutex_t		ios_mutex;
static ulint		ios;

static ulint		n[SRV_MAX_N_IO_THREADS + 5];
static os_thread_id_t	thread_ids[SRV_MAX_N_IO_THREADS + 5];

/* We use this mutex to test the return value of pthread_mutex_trylock
   on successful locking. HP-UX does NOT return 0, though Linux et al do. */
static os_fast_mutex_t	srv_os_test_mutex;

/* Name of srv_monitor_file */
static char*	srv_monitor_file_name;

#define SRV_N_PENDING_IOS_PER_THREAD 	OS_AIO_N_PENDING_IOS_PER_THREAD
#define SRV_MAX_N_PENDING_SYNC_IOS	100


/* Avoid warnings when using purify */

#ifdef HAVE_purify
static int inno_bcmp(register const char *s1, register const char *s2,
                     register uint len)
{
  while (len-- != 0 && *s1++ == *s2++) ;
  return len+1;
}
#define memcmp(A,B,C) inno_bcmp((A),(B),(C))
#endif

/*************************************************************************
Reads the data files and their sizes from a character string given in
the .cnf file. */

ibool
srv_parse_data_file_paths_and_sizes(
/*================================*/
					/* out: TRUE if ok, FALSE if parsing
					error */
	char*	str,			/* in: the data file path string */
	char***	data_file_names,	/* out, own: array of data file
					names */
	ulint**	data_file_sizes,	/* out, own: array of data file sizes
					in megabytes */
	ulint**	data_file_is_raw_partition,/* out, own: array of flags
					showing which data files are raw
					partitions */
	ulint*	n_data_files,		/* out: number of data files */
	ibool*	is_auto_extending,	/* out: TRUE if the last data file is
					auto-extending */
	ulint*	max_auto_extend_size)	/* out: max auto extend size for the
					last file if specified, 0 if not */
{
	char*	input_str;
	char*	endp;
	char*	path;
	ulint	size;
	ulint	i	= 0;

	*is_auto_extending = FALSE;
	*max_auto_extend_size = 0;

	input_str = str;
	
	/* First calculate the number of data files and check syntax:
	path:size[M | G];path:size[M | G]... . Note that a Windows path may
	contain a drive name and a ':'. */

	while (*str != '\0') {
		path = str;

		while ((*str != ':' && *str != '\0')
		       || (*str == ':'
			   && (*(str + 1) == '\\' || *(str + 1) == '/'
					     || *(str + 1) == ':'))) {
			str++;
		}

		if (*str == '\0') {
			return(FALSE);
		}

		str++;

		size = strtoul(str, &endp, 10);

		str = endp;

		if (*str != 'M' && *str != 'G') {
			size = size / (1024 * 1024);
		} else if (*str == 'G') {
		        size = size * 1024;
			str++;
		} else {
		        str++;
		}

	        if (0 == memcmp(str, ":autoextend", (sizeof ":autoextend") - 1)) {

			str += (sizeof ":autoextend") - 1;

	        	if (0 == memcmp(str, ":max:", (sizeof ":max:") - 1)) {

				str += (sizeof ":max:") - 1;

				size = strtoul(str, &endp, 10);

				str = endp;

				if (*str != 'M' && *str != 'G') {
					size = size / (1024 * 1024);
				} else if (*str == 'G') {
		        		size = size * 1024;
					str++;
				} else {
		        		str++;
				}
			}

			if (*str != '\0') {

				return(FALSE);
			}
		}

	        if (strlen(str) >= 6
			   && *str == 'n'
			   && *(str + 1) == 'e' 
		           && *(str + 2) == 'w') {
		  	str += 3;
		}

	        if (*str == 'r' && *(str + 1) == 'a' && *(str + 2) == 'w') {
		  	str += 3;
		}

		if (size == 0) {
			return(FALSE);
		}

		i++;

		if (*str == ';') {
			str++;
		} else if (*str != '\0') {

			return(FALSE);
		}
	}

	if (i == 0) {
		/* If innodb_data_file_path was defined it must contain
		at least one data file definition */

		return(FALSE);
	}
	
	*data_file_names = (char**)ut_malloc(i * sizeof(void*));
	*data_file_sizes = (ulint*)ut_malloc(i * sizeof(ulint));
	*data_file_is_raw_partition = (ulint*)ut_malloc(i * sizeof(ulint));

	*n_data_files = i;

	/* Then store the actual values to our arrays */

	str = input_str;
	i = 0;

	while (*str != '\0') {
		path = str;

		/* Note that we must step over the ':' in a Windows path;
		a Windows path normally looks like C:\ibdata\ibdata1:1G, but
		a Windows raw partition may have a specification like
		\\.\C::1Gnewraw or \\.\PHYSICALDRIVE2:1Gnewraw */

		while ((*str != ':' && *str != '\0')
		       || (*str == ':'
			   && (*(str + 1) == '\\' || *(str + 1) == '/'
			        || *(str + 1) == ':'))) {
			str++;
		}

		if (*str == ':') {
			/* Make path a null-terminated string */
			*str = '\0';
			str++;
		}

		size = strtoul(str, &endp, 10);

		str = endp;

		if ((*str != 'M') && (*str != 'G')) {
			size = size / (1024 * 1024);
		} else if (*str == 'G') {
		        size = size * 1024;
			str++;
		} else {
		        str++;
		}

		(*data_file_names)[i] = path;
		(*data_file_sizes)[i] = size;

	        if (0 == memcmp(str, ":autoextend", (sizeof ":autoextend") - 1)) {

			*is_auto_extending = TRUE;

			str += (sizeof ":autoextend") - 1;

	        	if (0 == memcmp(str, ":max:", (sizeof ":max:") - 1)) {

				str += (sizeof ":max:") - 1;

				size = strtoul(str, &endp, 10);

				str = endp;

				if (*str != 'M' && *str != 'G') {
					size = size / (1024 * 1024);
				} else if (*str == 'G') {
		        		size = size * 1024;
					str++;
				} else {
		        		str++;
				}

				*max_auto_extend_size = size;
			}

			if (*str != '\0') {

				return(FALSE);
			}
		}
		
		(*data_file_is_raw_partition)[i] = 0;

	        if (strlen(str) >= 6
			   && *str == 'n'
			   && *(str + 1) == 'e' 
		           && *(str + 2) == 'w') {
		  	str += 3;
		  	(*data_file_is_raw_partition)[i] = SRV_NEW_RAW;
		}

		if (*str == 'r' && *(str + 1) == 'a' && *(str + 2) == 'w') {
		 	str += 3;
		  
		  	if ((*data_file_is_raw_partition)[i] == 0) {
		    		(*data_file_is_raw_partition)[i] = SRV_OLD_RAW;
		  	}		  
		}

		i++;

		if (*str == ';') {
			str++;
		}
	}

	return(TRUE);
}

/*************************************************************************
Reads log group home directories from a character string given in
the .cnf file. */

ibool
srv_parse_log_group_home_dirs(
/*==========================*/
					/* out: TRUE if ok, FALSE if parsing
					error */
	char*	str,			/* in: character string */
	char***	log_group_home_dirs)	/* out, own: log group home dirs */
{
	char*	input_str;
	char*	path;
	ulint	i	= 0;

	input_str = str;
	
	/* First calculate the number of directories and check syntax:
	path;path;... */

	while (*str != '\0') {
		path = str;

		while (*str != ';' && *str != '\0') {
			str++;
		}

		i++;

		if (*str == ';') {
			str++;
		} else if (*str != '\0') {

			return(FALSE);
		}
	}

	if (i != 1) {
		/* If innodb_log_group_home_dir was defined it must
		contain exactly one path definition under current MySQL */
		
		return(FALSE);
	}
	
	*log_group_home_dirs = (char**) ut_malloc(i * sizeof(void*));

	/* Then store the actual values to our array */

	str = input_str;
	i = 0;

	while (*str != '\0') {
		path = str;

		while (*str != ';' && *str != '\0') {
			str++;
		}

		if (*str == ';') {
			*str = '\0';
			str++;
		}

		(*log_group_home_dirs)[i] = path;

		i++;
	}

	return(TRUE);
}

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

#ifdef UNIV_DEBUG_THREAD_CREATION
	fprintf(stderr, "Io handler thread %lu starts, id %lu\n", segment,
			os_thread_pf(os_thread_get_curr_id()));
#endif
	for (i = 0;; i++) {
		fil_aio_wait(segment);

		mutex_enter(&ios_mutex);
		ios++;
		mutex_exit(&ios_mutex);
	}

	/* We count the number of threads in os_thread_exit(). A created
	thread should always use that to exit and not use return() to exit.
	The thread actually never comes here because it is exited in an
	os_event_wait(). */

	os_thread_exit(NULL);

#ifndef __WIN__
	return(NULL);				/* Not reached */
#else
	return(0);
#endif
}

#ifdef __WIN__
#define SRV_PATH_SEPARATOR	'\\'
#else
#define SRV_PATH_SEPARATOR	'/'
#endif

/*************************************************************************
Normalizes a directory path for Windows: converts slashes to backslashes. */

void
srv_normalize_path_for_win(
/*=======================*/
	char*	str __attribute__((unused)))	/* in/out: null-terminated
							   character string */
{
#ifdef __WIN__
	for (; *str; str++) {

		if (*str == '/') {
			*str = '\\';
		}
	}
#endif
}
	
/*************************************************************************
Adds a slash or a backslash to the end of a string if it is missing
and the string is not empty. */

char*
srv_add_path_separator_if_needed(
/*=============================*/
			/* out: string which has the separator if the
			string is not empty */
	char*	str)	/* in: null-terminated character string */
{
	char*	out_str;
	ulint	len	= ut_strlen(str);

	if (len == 0 || str[len - 1] == SRV_PATH_SEPARATOR) {

		return(str);
	}

	out_str = ut_malloc(len + 2);
	memcpy(out_str, str, len);
	out_str[len] = SRV_PATH_SEPARATOR;
	out_str[len + 1] = 0;

	return(out_str);
}

/*************************************************************************
Calculates the low 32 bits when a file size which is given as a number
database pages is converted to the number of bytes. */
static
ulint
srv_calc_low32(
/*===========*/
				/* out: low 32 bytes of file size when
				expressed in bytes */
	ulint	file_size)	/* in: file size in database pages */
{
	return(0xFFFFFFFFUL & (file_size << UNIV_PAGE_SIZE_SHIFT));
}

/*************************************************************************
Calculates the high 32 bits when a file size which is given as a number
database pages is converted to the number of bytes. */
static
ulint
srv_calc_high32(
/*============*/
				/* out: high 32 bytes of file size when
				expressed in bytes */
	ulint	file_size)	/* in: file size in database pages */
{
	return(file_size >> (32 - UNIV_PAGE_SIZE_SHIFT));
}

#ifndef UNIV_HOTBACKUP
/*************************************************************************
Creates or opens the log files and closes them. */
static
ulint
open_or_create_log_file(
/*====================*/
					/* out: DB_SUCCESS or error code */
        ibool   create_new_db,          /* in: TRUE if we should create a
                                        new database */
	ibool*	log_file_created,	/* out: TRUE if new log file
					created */
	ibool	log_file_has_been_opened,/* in: TRUE if a log file has been
					opened before: then it is an error
					to try to create another log file */
	ulint	k,			/* in: log group number */
	ulint	i)			/* in: log file number in group */
{
	ibool	ret;
	ulint	size;
	ulint	size_high;
	char	name[10000];

	UT_NOT_USED(create_new_db);

	*log_file_created = FALSE;

	srv_normalize_path_for_win(srv_log_group_home_dirs[k]);
	srv_log_group_home_dirs[k] = srv_add_path_separator_if_needed(
						srv_log_group_home_dirs[k]);

	ut_a(strlen(srv_log_group_home_dirs[k]) <
		(sizeof name) - 10 - sizeof "ib_logfile");
	sprintf(name, "%s%s%lu", srv_log_group_home_dirs[k], "ib_logfile", (ulong) i);

	files[i] = os_file_create(name, OS_FILE_CREATE, OS_FILE_NORMAL,
						OS_LOG_FILE, &ret);
	if (ret == FALSE) {
		if (os_file_get_last_error(FALSE) != OS_FILE_ALREADY_EXISTS
#ifdef UNIV_AIX
		   	/* AIX 5.1 after security patch ML7 may have errno set
			to 0 here, which causes our function to return 100;
			work around that AIX problem */
		   && os_file_get_last_error(FALSE) != 100
#endif
		) {
			fprintf(stderr,
			"InnoDB: Error in creating or opening %s\n", name);
				
			return(DB_ERROR);
		}

		files[i] = os_file_create(name, OS_FILE_OPEN, OS_FILE_AIO,
							OS_LOG_FILE, &ret);
		if (!ret) {
			fprintf(stderr,
			"InnoDB: Error in opening %s\n", name);
				
			return(DB_ERROR);
		}

		ret = os_file_get_size(files[i], &size, &size_high);
		ut_a(ret);
		
		if (size != srv_calc_low32(srv_log_file_size)
		    || size_high != srv_calc_high32(srv_log_file_size)) {
		    	
			fprintf(stderr,
"InnoDB: Error: log file %s is of different size %lu %lu bytes\n"
"InnoDB: than specified in the .cnf file %lu %lu bytes!\n",
				name, (ulong) size_high, (ulong) size,
				(ulong) srv_calc_high32(srv_log_file_size),
				(ulong) srv_calc_low32(srv_log_file_size));
				
			return(DB_ERROR);
		}					
	} else {
		*log_file_created = TRUE;

	    	ut_print_timestamp(stderr);

		fprintf(stderr,
		"  InnoDB: Log file %s did not exist: new to be created\n",
									name);
		if (log_file_has_been_opened) {

			return(DB_ERROR);
		}

		fprintf(stderr, "InnoDB: Setting log file %s size to %lu MB\n",
			             name, (ulong) srv_log_file_size
			>> (20 - UNIV_PAGE_SIZE_SHIFT));

		fprintf(stderr,
	    "InnoDB: Database physically writes the file full: wait...\n");

		ret = os_file_set_size(name, files[i],
					srv_calc_low32(srv_log_file_size),
					srv_calc_high32(srv_log_file_size));
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
				2 * k + SRV_LOG_SPACE_FIRST_ID, FALSE);
#ifdef UNIV_LOG_ARCHIVE
	/* If this is the first log group, create the file space object
	for archived logs.
	Under MySQL, no archiving ever done. */

	if (k == 0 && i == 0) {
		arch_space_id = 2 * k + 1 + SRV_LOG_SPACE_FIRST_ID;

	    	fil_space_create("arch_log_space", arch_space_id, FIL_LOG);
	} else {
		arch_space_id = ULINT_UNDEFINED;
	}
#endif /* UNIV_LOG_ARCHIVE */
	if (i == 0) {
		log_group_init(k, srv_n_log_files,
				srv_log_file_size * UNIV_PAGE_SIZE,
				2 * k + SRV_LOG_SPACE_FIRST_ID,
				SRV_LOG_SPACE_FIRST_ID + 1); /* dummy arch
								space id */
	}

	return(DB_SUCCESS);
}

/*************************************************************************
Creates or opens database data files and closes them. */
static
ulint
open_or_create_data_files(
/*======================*/
				/* out: DB_SUCCESS or error code */
	ibool*	create_new_db,	/* out: TRUE if new database should be
								created */
#ifdef UNIV_LOG_ARCHIVE
	ulint*	min_arch_log_no,/* out: min of archived log numbers in data
				files */
	ulint*	max_arch_log_no,/* out: */
#endif /* UNIV_LOG_ARCHIVE */
	dulint*	min_flushed_lsn,/* out: min of flushed lsn values in data
				files */
	dulint*	max_flushed_lsn,/* out: */
	ulint*	sum_of_new_sizes)/* out: sum of sizes of the new files added */
{
	ibool	ret;
	ulint	i;
	ibool	one_opened	= FALSE;
	ibool	one_created	= FALSE;
	ulint	size;
	ulint	size_high;
	ulint	rounded_size_pages;
	char	name[10000];

	if (srv_n_data_files >= 1000) {
		fprintf(stderr, "InnoDB: can only have < 1000 data files\n"
				"InnoDB: you have defined %lu\n",
				(ulong) srv_n_data_files);
		return(DB_ERROR);
	}

	*sum_of_new_sizes = 0;
	
	*create_new_db = FALSE;

	srv_normalize_path_for_win(srv_data_home);
	srv_data_home = srv_add_path_separator_if_needed(srv_data_home);

	for (i = 0; i < srv_n_data_files; i++) {
		srv_normalize_path_for_win(srv_data_file_names[i]);

		ut_a(strlen(srv_data_home) + strlen(srv_data_file_names[i])
			< (sizeof name) - 1);
		sprintf(name, "%s%s", srv_data_home, srv_data_file_names[i]);
	
		if (srv_data_file_is_raw_partition[i] == 0) {

			/* First we try to create the file: if it already
			exists, ret will get value FALSE */

			files[i] = os_file_create(name, OS_FILE_CREATE,
					OS_FILE_NORMAL, OS_DATA_FILE, &ret);

			if (ret == FALSE && os_file_get_last_error(FALSE) !=
						OS_FILE_ALREADY_EXISTS
#ifdef UNIV_AIX
		   		/* AIX 5.1 after security patch ML7 may have
				errno set to 0 here, which causes our function
				to return 100; work around that AIX problem */
		   	    && os_file_get_last_error(FALSE) != 100
#endif
			) {
				fprintf(stderr,
				"InnoDB: Error in creating or opening %s\n",
				name);

				return(DB_ERROR);
			}
		} else if (srv_data_file_is_raw_partition[i] == SRV_NEW_RAW) {
			/* The partition is opened, not created; then it is
			written over */

			srv_start_raw_disk_in_use = TRUE;
			srv_created_new_raw = TRUE;

			files[i] = os_file_create(
				name, OS_FILE_OPEN_RAW, OS_FILE_NORMAL,
							OS_DATA_FILE, &ret);
			if (!ret) {
				fprintf(stderr,
				"InnoDB: Error in opening %s\n", name);

				return(DB_ERROR);
			}
		} else if (srv_data_file_is_raw_partition[i] == SRV_OLD_RAW) {
			srv_start_raw_disk_in_use = TRUE;

			ret = FALSE;
		} else {
			ut_a(0);
		}

		if (ret == FALSE) {
			/* We open the data file */

			if (one_created) {
				fprintf(stderr,
	"InnoDB: Error: data files can only be added at the end\n");
				fprintf(stderr,
	"InnoDB: of a tablespace, but data file %s existed beforehand.\n",
				name);
				return(DB_ERROR);
			}
				
			if (srv_data_file_is_raw_partition[i] == SRV_OLD_RAW) {
				files[i] = os_file_create(
					name, OS_FILE_OPEN_RAW, OS_FILE_NORMAL,
							 OS_DATA_FILE, &ret);
			} else if (i == 0) {
				files[i] = os_file_create(
					name, OS_FILE_OPEN_RETRY,
							OS_FILE_NORMAL,
							OS_DATA_FILE, &ret);
			} else {
				files[i] = os_file_create(
					name, OS_FILE_OPEN, OS_FILE_NORMAL,
							 OS_DATA_FILE, &ret);
			}

			if (!ret) {
				fprintf(stderr,
				"InnoDB: Error in opening %s\n", name);
				os_file_get_last_error(TRUE);

				return(DB_ERROR);
			}

			if (srv_data_file_is_raw_partition[i] == SRV_OLD_RAW) {

				goto skip_size_check;
			}

			ret = os_file_get_size(files[i], &size, &size_high);
			ut_a(ret);
			/* Round size downward to megabytes */
		
			rounded_size_pages = (size / (1024 * 1024)
							+ 4096 * size_high)
					     << (20 - UNIV_PAGE_SIZE_SHIFT);

			if (i == srv_n_data_files - 1
				    && srv_auto_extend_last_data_file) {

				if (srv_data_file_sizes[i] >
				    		rounded_size_pages
				    	   || (srv_last_file_size_max > 0
				    	      && srv_last_file_size_max <
				    	       rounded_size_pages)) {
				    	       	
					fprintf(stderr,
"InnoDB: Error: auto-extending data file %s is of a different size\n"
"InnoDB: %lu pages (rounded down to MB) than specified in the .cnf file:\n"
"InnoDB: initial %lu pages, max %lu (relevant if non-zero) pages!\n",
		  name, (ulong) rounded_size_pages,
		  (ulong) srv_data_file_sizes[i],
		  (ulong) srv_last_file_size_max);

					return(DB_ERROR);
				}
				    	     
				srv_data_file_sizes[i] = rounded_size_pages;
			}
				
			if (rounded_size_pages != srv_data_file_sizes[i]) {

				fprintf(stderr,
"InnoDB: Error: data file %s is of a different size\n"
"InnoDB: %lu pages (rounded down to MB)\n"
"InnoDB: than specified in the .cnf file %lu pages!\n", name,
					       (ulong) rounded_size_pages,
					       (ulong) srv_data_file_sizes[i]);
				
				return(DB_ERROR);
			}
skip_size_check:
			fil_read_flushed_lsn_and_arch_log_no(files[i],
					one_opened,
#ifdef UNIV_LOG_ARCHIVE
					min_arch_log_no, max_arch_log_no,
#endif /* UNIV_LOG_ARCHIVE */
					min_flushed_lsn, max_flushed_lsn);
			one_opened = TRUE;
		} else {
		        /* We created the data file and now write it full of
			zeros */

			one_created = TRUE;

			if (i > 0) {
	    			ut_print_timestamp(stderr);
				fprintf(stderr, 
		"  InnoDB: Data file %s did not exist: new to be created\n",
									name);
			} else {
				fprintf(stderr, 
 		"InnoDB: The first specified data file %s did not exist:\n"
		"InnoDB: a new database to be created!\n", name);
				*create_new_db = TRUE;
			}
			
	    		ut_print_timestamp(stderr);
			fprintf(stderr, 
				"  InnoDB: Setting file %s size to %lu MB\n",
			       name, (ulong) (srv_data_file_sizes[i]
				      >> (20 - UNIV_PAGE_SIZE_SHIFT)));

			fprintf(stderr,
	"InnoDB: Database physically writes the file full: wait...\n");

			ret = os_file_set_size(name, files[i],
				srv_calc_low32(srv_data_file_sizes[i]),
				srv_calc_high32(srv_data_file_sizes[i]));

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

		if (srv_data_file_is_raw_partition[i]) {

		        fil_node_create(name, srv_data_file_sizes[i], 0, TRUE);
		} else {
		        fil_node_create(name, srv_data_file_sizes[i], 0,
									FALSE);
		}
	}

	ios = 0;

	mutex_create(&ios_mutex);
	mutex_set_level(&ios_mutex, SYNC_NO_ORDER_CHECK);

	return(DB_SUCCESS);
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
	buf_pool_t*	ret;
	ibool	create_new_db;
	ibool	log_file_created;
	ibool	log_created	= FALSE;
	ibool	log_opened	= FALSE;
	dulint	min_flushed_lsn;
	dulint	max_flushed_lsn;
#ifdef UNIV_LOG_ARCHIVE
	ulint	min_arch_log_no;
	ulint	max_arch_log_no;
#endif /* UNIV_LOG_ARCHIVE */
	ulint   sum_of_new_sizes;
	ulint	sum_of_data_file_sizes;
	ulint	tablespace_size_in_header;
	ulint	err;
	ulint	i;
	ibool	srv_file_per_table_original_value  = srv_file_per_table;
	mtr_t   mtr;
#ifdef HAVE_DARWIN_THREADS
# ifdef F_FULLFSYNC
	/* This executable has been compiled on Mac OS X 10.3 or later.
	Assume that F_FULLFSYNC is available at run-time. */
	srv_have_fullfsync = TRUE;
# else /* F_FULLFSYNC */
	/* This executable has been compiled on Mac OS X 10.2
	or earlier.  Determine if the executable is running
	on Mac OS X 10.3 or later. */
	struct utsname utsname;
	if (uname(&utsname)) {
		fputs("InnoDB: cannot determine Mac OS X version!\n", stderr);
	} else {
		srv_have_fullfsync = strcmp(utsname.release, "7.") >= 0;
	}
	if (!srv_have_fullfsync) {
		fputs(
"InnoDB: On Mac OS X, fsync() may be broken on internal drives,\n"
"InnoDB: making transactions unsafe!\n", stderr);
	}
# endif /* F_FULLFSYNC */
#endif /* HAVE_DARWIN_THREADS */

	if (sizeof(ulint) != sizeof(void*)) {
		fprintf(stderr,
"InnoDB: Error: size of InnoDB's ulint is %lu, but size of void* is %lu.\n"
"InnoDB: The sizes should be the same so that on a 64-bit platform you can\n"
"InnoDB: allocate more than 4 GB of memory.",
			(ulong)sizeof(ulint), (ulong)sizeof(void*));
	}

	srv_file_per_table = FALSE; /* system tables are created in tablespace
									0 */
#ifdef UNIV_DEBUG
	fprintf(stderr,
"InnoDB: !!!!!!!!!!!!!! UNIV_DEBUG switched on !!!!!!!!!!!!!!!\n"); 
#endif

#ifdef UNIV_SYNC_DEBUG
	fprintf(stderr,
"InnoDB: !!!!!!!!!!!!!! UNIV_SYNC_DEBUG switched on !!!!!!!!!!!!!!!\n"); 
#endif

#ifdef UNIV_SEARCH_DEBUG
	fprintf(stderr,
"InnoDB: !!!!!!!!!!!!!! UNIV_SEARCH_DEBUG switched on !!!!!!!!!!!!!!!\n"); 
#endif

#ifdef UNIV_MEM_DEBUG
	fprintf(stderr,
"InnoDB: !!!!!!!!!!!!!! UNIV_MEM_DEBUG switched on !!!!!!!!!!!!!!!\n"); 
#endif

#ifdef UNIV_SIMULATE_AWE
	fprintf(stderr,
"InnoDB: !!!!!!!!!!!!!! UNIV_SIMULATE_AWE switched on !!!!!!!!!!!!!!!!!\n");
#endif
        if (srv_sizeof_trx_t_in_ha_innodb_cc != (ulint)sizeof(trx_t)) {
	        fprintf(stderr,
  "InnoDB: Error: trx_t size is %lu in ha_innodb.cc but %lu in srv0start.c\n"
  "InnoDB: Check that pthread_mutex_t is defined in the same way in these\n"
  "InnoDB: compilation modules. Cannot continue.\n",
		 (ulong)  srv_sizeof_trx_t_in_ha_innodb_cc,
		 (ulong) sizeof(trx_t));
		return(DB_ERROR);
	}

	/* Since InnoDB does not currently clean up all its internal data
	   structures in MySQL Embedded Server Library server_end(), we
	   print an error message if someone tries to start up InnoDB a
	   second time during the process lifetime. */

	if (srv_start_has_been_called) {
	        fprintf(stderr,
"InnoDB: Error:startup called second time during the process lifetime.\n"
"InnoDB: In the MySQL Embedded Server Library you cannot call server_init()\n"
"InnoDB: more than once during the process lifetime.\n");
	}

	srv_start_has_been_called = TRUE;

#ifdef UNIV_DEBUG
	log_do_write = TRUE;
#endif /* UNIV_DEBUG */
/*	yydebug = TRUE; */

	srv_is_being_started = TRUE;
        srv_startup_is_before_trx_rollback_phase = TRUE;
	os_aio_use_native_aio = FALSE;

#if !defined(__WIN2000__) && !defined(UNIV_SIMULATE_AWE)
	if (srv_use_awe) {

	        fprintf(stderr,
"InnoDB: Error: You have specified innodb_buffer_pool_awe_mem_mb\n"
"InnoDB: in my.cnf, but AWE can only be used in Windows 2000 and later.\n"
"InnoDB: To use AWE, InnoDB must be compiled with __WIN2000__ defined.\n");

	        return(DB_ERROR);
	}
#endif

#ifdef __WIN__
	if (os_get_os_version() == OS_WIN95
	    || os_get_os_version() == OS_WIN31
	    || os_get_os_version() == OS_WINNT) {

	  	/* On Win 95, 98, ME, Win32 subsystem for Windows 3.1,
		and NT use simulated aio. In NT Windows provides async i/o,
		but when run in conjunction with InnoDB Hot Backup, it seemed
		to corrupt the data files. */

	  	os_aio_use_native_aio = FALSE;
	} else {
	  	/* On Win 2000 and XP use async i/o */
	  	os_aio_use_native_aio = TRUE;
	}
#endif	
        if (srv_file_flush_method_str == NULL) {
        	/* These are the default options */

		srv_unix_file_flush_method = SRV_UNIX_FDATASYNC;

		srv_win_file_flush_method = SRV_WIN_IO_UNBUFFERED;
#ifndef __WIN__        
	} else if (0 == ut_strcmp(srv_file_flush_method_str, "fdatasync")) {
	  	srv_unix_file_flush_method = SRV_UNIX_FDATASYNC;

	} else if (0 == ut_strcmp(srv_file_flush_method_str, "O_DSYNC")) {
	  	srv_unix_file_flush_method = SRV_UNIX_O_DSYNC;

	} else if (0 == ut_strcmp(srv_file_flush_method_str, "O_DIRECT")) {
	  	srv_unix_file_flush_method = SRV_UNIX_O_DIRECT;

	} else if (0 == ut_strcmp(srv_file_flush_method_str, "littlesync")) {
	  	srv_unix_file_flush_method = SRV_UNIX_LITTLESYNC;

	} else if (0 == ut_strcmp(srv_file_flush_method_str, "nosync")) {
	  	srv_unix_file_flush_method = SRV_UNIX_NOSYNC;
#else
	} else if (0 == ut_strcmp(srv_file_flush_method_str, "normal")) {
	  	srv_win_file_flush_method = SRV_WIN_IO_NORMAL;
	  	os_aio_use_native_aio = FALSE;

	} else if (0 == ut_strcmp(srv_file_flush_method_str, "unbuffered")) {
	  	srv_win_file_flush_method = SRV_WIN_IO_UNBUFFERED;
	  	os_aio_use_native_aio = FALSE;

	} else if (0 == ut_strcmp(srv_file_flush_method_str,
							"async_unbuffered")) {
	  	srv_win_file_flush_method = SRV_WIN_IO_UNBUFFERED;	
#endif
	} else {
	  	fprintf(stderr, 
          	"InnoDB: Unrecognized value %s for innodb_flush_method\n",
          				srv_file_flush_method_str);
	  	return(DB_ERROR);
	}

	/* Note that the call srv_boot() also changes the values of
	srv_pool_size etc. to the units used by InnoDB internally */

        /* Set the maximum number of threads which can wait for a semaphore
        inside InnoDB: this is the 'sync wait array' size, as well as the
	maximum number of threads that can wait in the 'srv_conc array' for
	their time to enter InnoDB. */

#if defined(__WIN__) || defined(__NETWARE__)

/* Create less event semaphores because Win 98/ME had difficulty creating
40000 event semaphores.
Comment from Novell, Inc.: also, these just take a lot of memory on
NetWare. */
        srv_max_n_threads = 1000;
#else
        if (srv_pool_size >= 1000 * 1024) {
                                  /* Here we still have srv_pool_size counted
                                  in kilobytes (in 4.0 this was in bytes)
				  srv_boot() converts the value to
                                  pages; if buffer pool is less than 1000 MB,
                                  assume fewer threads. */
                srv_max_n_threads = 50000;

        } else if (srv_pool_size >= 8 * 1024) {

                srv_max_n_threads = 10000;
        } else {
		srv_max_n_threads = 1000;       /* saves several MB of memory,
                                                especially in 64-bit
                                                computers */
        }
#endif
	err = srv_boot(); /* This changes srv_pool_size to units of a page */

	if (err != DB_SUCCESS) {

		return((int) err);
	}

	mutex_create(&srv_monitor_file_mutex);
	mutex_set_level(&srv_monitor_file_mutex, SYNC_NO_ORDER_CHECK);
	if (srv_innodb_status) {
		srv_monitor_file_name = mem_alloc(
				strlen(fil_path_to_mysql_datadir) +
				20 + sizeof "/innodb_status.");
		sprintf(srv_monitor_file_name, "%s/innodb_status.%lu",
			fil_path_to_mysql_datadir, os_proc_get_number());
		srv_monitor_file = fopen(srv_monitor_file_name, "w+");
		if (!srv_monitor_file) {
			fprintf(stderr, "InnoDB: unable to create %s: %s\n",
				srv_monitor_file_name, strerror(errno));
			return(DB_ERROR);
		}
	} else {
		srv_monitor_file_name = NULL;
		srv_monitor_file = os_file_create_tmpfile();
		if (!srv_monitor_file) {
			return(DB_ERROR);
		}
	}

	mutex_create(&srv_dict_tmpfile_mutex);
	mutex_set_level(&srv_dict_tmpfile_mutex, SYNC_DICT_OPERATION);
	srv_dict_tmpfile = os_file_create_tmpfile();
	if (!srv_dict_tmpfile) {
		return(DB_ERROR);
	}

	mutex_create(&srv_misc_tmpfile_mutex);
	mutex_set_level(&srv_misc_tmpfile_mutex, SYNC_ANY_LATCH);
	srv_misc_tmpfile = os_file_create_tmpfile();
	if (!srv_misc_tmpfile) {
		return(DB_ERROR);
	}

	/* Restrict the maximum number of file i/o threads */
	if (srv_n_file_io_threads > SRV_MAX_N_IO_THREADS) {

		srv_n_file_io_threads = SRV_MAX_N_IO_THREADS;
	}

	if (!os_aio_use_native_aio) {
 		/* In simulated aio we currently have use only for 4 threads */
		srv_n_file_io_threads = 4;

		os_aio_init(8 * SRV_N_PENDING_IOS_PER_THREAD
						* srv_n_file_io_threads,
					srv_n_file_io_threads,
					SRV_MAX_N_PENDING_SYNC_IOS);
	} else {
		os_aio_init(SRV_N_PENDING_IOS_PER_THREAD
						* srv_n_file_io_threads,
					srv_n_file_io_threads,
					SRV_MAX_N_PENDING_SYNC_IOS);
	}
	
	fil_init(srv_max_n_open_files);

	if (srv_use_awe) {
		fprintf(stderr,
"InnoDB: Using AWE: Memory window is %lu MB and AWE memory is %lu MB\n",
		(ulong) (srv_awe_window_size / ((1024 * 1024) / UNIV_PAGE_SIZE)),
		(ulong) (srv_pool_size / ((1024 * 1024) / UNIV_PAGE_SIZE)));

		/* We must disable adaptive hash indexes because they do not
		tolerate remapping of pages in AWE */
		
		srv_use_adaptive_hash_indexes = FALSE;
		ret = buf_pool_init(srv_pool_size, srv_pool_size,
							srv_awe_window_size);
	} else {
		ret = buf_pool_init(srv_pool_size, srv_pool_size,
							srv_pool_size);
	}

	if (ret == NULL) {
		fprintf(stderr,
"InnoDB: Fatal error: cannot allocate the memory for the buffer pool\n");

		return(DB_ERROR);
	}

	fsp_init();
	log_init();
	
	lock_sys_create(srv_lock_table_size);

	/* Create i/o-handler threads: */

	for (i = 0; i < srv_n_file_io_threads; i++) {
		n[i] = i;

		os_thread_create(io_handler_thread, n + i, thread_ids + i);
    	}

#ifdef UNIV_LOG_ARCHIVE
	if (0 != ut_strcmp(srv_log_group_home_dirs[0], srv_arch_dir)) {
		fprintf(stderr,
	"InnoDB: Error: you must set the log group home dir in my.cnf the\n"
	"InnoDB: same as log arch dir.\n");

		return(DB_ERROR);
	}
#endif /* UNIV_LOG_ARCHIVE */

	if (srv_n_log_files * srv_log_file_size >= 262144) {
		fprintf(stderr,
		"InnoDB: Error: combined size of log files must be < 4 GB\n");

		return(DB_ERROR);
	}

	sum_of_new_sizes = 0;
	
	for (i = 0; i < srv_n_data_files; i++) {
#ifndef __WIN__
		if (sizeof(off_t) < 5 && srv_data_file_sizes[i] >= 262144) {
		 	fprintf(stderr,
	"InnoDB: Error: file size must be < 4 GB with this MySQL binary\n"
	"InnoDB: and operating system combination, in some OS's < 2 GB\n");

		  	return(DB_ERROR);
		}
#endif
		sum_of_new_sizes += srv_data_file_sizes[i];
	}

	if (sum_of_new_sizes < 640) {
		  fprintf(stderr,
		  "InnoDB: Error: tablespace size must be at least 10 MB\n");

		  return(DB_ERROR);
	}

	err = open_or_create_data_files(&create_new_db,
#ifdef UNIV_LOG_ARCHIVE
					&min_arch_log_no, &max_arch_log_no,
#endif /* UNIV_LOG_ARCHIVE */
					&min_flushed_lsn, &max_flushed_lsn,
					&sum_of_new_sizes);
	if (err != DB_SUCCESS) {
	        fprintf(stderr,
"InnoDB: Could not open or create data files.\n"
"InnoDB: If you tried to add new data files, and it failed here,\n"
"InnoDB: you should now edit innodb_data_file_path in my.cnf back\n"
"InnoDB: to what it was, and remove the new ibdata files InnoDB created\n"
"InnoDB: in this failed attempt. InnoDB only wrote those files full of\n"
"InnoDB: zeros, but did not yet use them in any way. But be careful: do not\n"
"InnoDB: remove old data files which contain your precious data!\n");

		return((int) err);
	}

#ifdef UNIV_LOG_ARCHIVE
	srv_normalize_path_for_win(srv_arch_dir);
	srv_arch_dir = srv_add_path_separator_if_needed(srv_arch_dir);
#endif /* UNIV_LOG_ARCHIVE */
		
	for (i = 0; i < srv_n_log_files; i++) {
		err = open_or_create_log_file(create_new_db, &log_file_created,
							     log_opened, 0, i);
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
	"InnoDB: All log files must be created also in database creation.\n"
	"InnoDB: If you want bigger or smaller log files, shut down the\n"
	"InnoDB: database and make sure there were no errors in shutdown.\n"
	"InnoDB: Then delete the existing log files. Edit the .cnf file\n"
	"InnoDB: and start the database again.\n");

			return(DB_ERROR);
		}
	}

	/* Open all log files and data files in the system tablespace: we
	keep them open until database shutdown */

	fil_open_log_and_system_tablespace_files();

	if (log_created && !create_new_db
#ifdef UNIV_LOG_ARCHIVE
		&& !srv_archive_recovery
#endif /* UNIV_LOG_ARCHIVE */
	) {
		if (ut_dulint_cmp(max_flushed_lsn, min_flushed_lsn) != 0
#ifdef UNIV_LOG_ARCHIVE
				|| max_arch_log_no != min_arch_log_no
#endif /* UNIV_LOG_ARCHIVE */
		) {
			fprintf(stderr, 
		"InnoDB: Cannot initialize created log files because\n"
		"InnoDB: data files were not in sync with each other\n"
		"InnoDB: or the data files are corrupt.\n");

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

#ifdef UNIV_LOG_ARCHIVE
		/* Do not + 1 arch_log_no because we do not use log
		archiving */
		recv_reset_logs(max_flushed_lsn, max_arch_log_no, TRUE);
#else
		recv_reset_logs(max_flushed_lsn, TRUE);
#endif /* UNIV_LOG_ARCHIVE */

		mutex_exit(&(log_sys->mutex));
	}

	if (create_new_db) {
		mtr_start(&mtr);

		fsp_header_init(0, sum_of_new_sizes, &mtr);		

		mtr_commit(&mtr);

		trx_sys_create();
		dict_create();
                srv_startup_is_before_trx_rollback_phase = FALSE;

#ifdef UNIV_LOG_ARCHIVE
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

		/* Initialize the fsp free limit global variable in the log
		system */
		fsp_header_get_free_limit(0);

		recv_recovery_from_archive_finish();
#endif /* UNIV_LOG_ARCHIVE */
	} else {
		/* We always try to do a recovery, even if the database had
		been shut down normally: this is the normal startup path */
		
		err = recv_recovery_from_checkpoint_start(LOG_CHECKPOINT,
							ut_dulint_max,
							min_flushed_lsn,
							max_flushed_lsn);
		if (err != DB_SUCCESS) {

			return(DB_ERROR);
		}

		/* Since the insert buffer init is in dict_boot, and the
		insert buffer is needed in any disk i/o, first we call
		dict_boot(). Note that trx_sys_init_at_db_start() only needs
		to access space 0, and the insert buffer at this stage already
		works for space 0. */

		dict_boot();
		trx_sys_init_at_db_start();

		if (srv_force_recovery < SRV_FORCE_NO_IBUF_MERGE) {
			/* The following call is necessary for the insert
			buffer to work with multiple tablespaces. We must
			know the mapping between space id's and .ibd file
			names.

			In a crash recovery, we check that the info in data
			dictionary is consistent with what we already know
			about space id's from the call of
			fil_load_single_table_tablespaces().

			In a normal startup, we create the space objects for
			every table in the InnoDB data dictionary that has
			an .ibd file.
	
			We also determine the maximum tablespace id used.

			TODO: We may have incomplete transactions in the
			data dictionary tables. Does that harm the scanning of
			the data dictionary below? */

			dict_check_tablespaces_and_store_max_id(
							recv_needed_recovery);
		}

                srv_startup_is_before_trx_rollback_phase = FALSE;

		/* Initialize the fsp free limit global variable in the log
		system */
		fsp_header_get_free_limit(0);

		/* recv_recovery_from_checkpoint_finish needs trx lists which
		are initialized in trx_sys_init_at_db_start(). */

		recv_recovery_from_checkpoint_finish();
	}
	
	if (!create_new_db && sum_of_new_sizes > 0) {
		/* New data file(s) were added */
		mtr_start(&mtr);

		fsp_header_inc_size(0, sum_of_new_sizes, &mtr);		

		mtr_commit(&mtr);

		/* Immediately write the log record about increased tablespace
		size to disk, so that it is durable even if mysqld would crash
		quickly */

		log_buffer_flush_to_disk();
	}

#ifdef UNIV_LOG_ARCHIVE
	/* Archiving is always off under MySQL */
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
#endif /* UNIV_LOG_ARCHIVE */

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

#ifdef UNIV_DEBUG
        /* Wait a while so that the created threads have time to suspend
	themselves before we switch sync debugging on; otherwise a thread may
	execute mutex_enter() before the checks are on, and mutex_exit() after
	the checks are on, which will cause an assertion failure in sync
	debug. */

        os_thread_sleep(3000000);
#endif
	sync_order_checks_on = TRUE;

        if (trx_doublewrite == NULL) {
		/* Create the doublewrite buffer to a new tablespace */

		trx_sys_create_doublewrite_buf();
	}

	err = dict_create_or_check_foreign_constraint_tables();

	if (err != DB_SUCCESS) {
		return((int)DB_ERROR);
	}
	
	/* Create the master thread which does purge and other utility
	operations */

	os_thread_create(&srv_master_thread, NULL, thread_ids + 1 +
							SRV_MAX_N_IO_THREADS);
#ifdef UNIV_DEBUG
	/* buf_debug_prints = TRUE; */
#endif /* UNIV_DEBUG */
	sum_of_data_file_sizes = 0;
	
	for (i = 0; i < srv_n_data_files; i++) {
		sum_of_data_file_sizes += srv_data_file_sizes[i];
	}

	tablespace_size_in_header = fsp_header_get_tablespace_size(0);

	if (!srv_auto_extend_last_data_file
		&& sum_of_data_file_sizes != tablespace_size_in_header) {

		fprintf(stderr,
"InnoDB: Error: tablespace size stored in header is %lu pages, but\n"
"InnoDB: the sum of data file sizes is %lu pages\n",
 			(ulong) tablespace_size_in_header,
			(ulong) sum_of_data_file_sizes);

		if (srv_force_recovery == 0
		    && sum_of_data_file_sizes < tablespace_size_in_header) {
			/* This is a fatal error, the tail of a tablespace is
			missing */

			fprintf(stderr,
"InnoDB: Cannot start InnoDB. The tail of the system tablespace is\n"
"InnoDB: missing. Have you edited innodb_data_file_path in my.cnf in an\n"
"InnoDB: inappropriate way, removing ibdata files from there?\n"
"InnoDB: You can set innodb_force_recovery=1 in my.cnf to force\n"
"InnoDB: a startup if you are trying to recover a badly corrupt database.\n");

			return(DB_ERROR);
		}
	}

	if (srv_auto_extend_last_data_file
		&& sum_of_data_file_sizes < tablespace_size_in_header) {

		fprintf(stderr,
"InnoDB: Error: tablespace size stored in header is %lu pages, but\n"
"InnoDB: the sum of data file sizes is only %lu pages\n",
 			(ulong) tablespace_size_in_header,
			(ulong) sum_of_data_file_sizes);

		if (srv_force_recovery == 0) {

			fprintf(stderr,
"InnoDB: Cannot start InnoDB. The tail of the system tablespace is\n"
"InnoDB: missing. Have you edited innodb_data_file_path in my.cnf in an\n"
"InnoDB: inappropriate way, removing ibdata files from there?\n"
"InnoDB: You can set innodb_force_recovery=1 in my.cnf to force\n"
"InnoDB: a startup if you are trying to recover a badly corrupt database.\n");

			return(DB_ERROR);
		}
	}

	/* Check that os_fast_mutexes work as expected */
	os_fast_mutex_init(&srv_os_test_mutex);

	if (0 != os_fast_mutex_trylock(&srv_os_test_mutex)) {
	        fprintf(stderr,
"InnoDB: Error: pthread_mutex_trylock returns an unexpected value on\n"
"InnoDB: success! Cannot continue.\n");
	        exit(1);
	}

	os_fast_mutex_unlock(&srv_os_test_mutex);

        os_fast_mutex_lock(&srv_os_test_mutex);

	os_fast_mutex_unlock(&srv_os_test_mutex);

	os_fast_mutex_free(&srv_os_test_mutex);

	if (srv_print_verbose_log) {
	  	ut_print_timestamp(stderr);
	  	fprintf(stderr,
"  InnoDB: Started; log sequence number %lu %lu\n",
			(ulong) ut_dulint_get_high(srv_start_lsn),
			(ulong) ut_dulint_get_low(srv_start_lsn));
	}

	if (srv_force_recovery > 0) {
		fprintf(stderr,
		"InnoDB: !!! innodb_force_recovery is set to %lu !!!\n",
			(ulong) srv_force_recovery);
	}

	fflush(stderr);

	if (trx_doublewrite_must_reset_space_ids) {
		/* Actually, we did not change the undo log format between
		4.0 and 4.1.1, and we would not need to run purge to
		completion. Note also that the purge algorithm in 4.1.1
		can process the the history list again even after a full
		purge, because our algorithm does not cut the end of the
		history list in all cases so that it would become empty
		after a full purge. That mean that we may purge 4.0 type
		undo log even after this phase.
		
		The insert buffer record format changed between 4.0 and
		4.1.1. It is essential that the insert buffer is emptied
		here! */

		fprintf(stderr,
"InnoDB: You are upgrading to an InnoDB version which allows multiple\n"
"InnoDB: tablespaces. Wait that purge and insert buffer merge run to\n"
"InnoDB: completion...\n");
		for (;;) {
			os_thread_sleep(1000000);

			if (0 == strcmp(srv_main_thread_op_info,
					"waiting for server activity")) {

				ut_a(ibuf_is_empty());
				
				break;
			}
		}
		fprintf(stderr,
"InnoDB: Full purge and insert buffer merge completed.\n");

	        trx_sys_mark_upgraded_to_multiple_tablespaces();

		fprintf(stderr,
"InnoDB: You have now successfully upgraded to the multiple tablespaces\n"
"InnoDB: format. You should NOT DOWNGRADE to an earlier version of\n"
"InnoDB: InnoDB! But if you absolutely need to downgrade, see\n"
"InnoDB: http://dev.mysql.com/doc/refman/5.0/en/multiple-tablespaces.html\n"
"InnoDB: for instructions.\n");
	}

	if (srv_force_recovery == 0) {
		/* In the insert buffer we may have even bigger tablespace
		id's, because we may have dropped those tablespaces, but
		insert buffer merge has not had time to clean the records from
		the ibuf tree. */

		ibuf_update_max_tablespace_id();
	}

	srv_file_per_table = srv_file_per_table_original_value;

	return((int) DB_SUCCESS);
}

/********************************************************************
Shuts down the InnoDB database. */

int
innobase_shutdown_for_mysql(void) 
/*=============================*/
				/* out: DB_SUCCESS or error code */
{
	ulint   i;
#ifdef __NETWARE__
	extern ibool panic_shutdown;
#endif
        if (!srv_was_started) {
	  	if (srv_is_being_started) {
	    		ut_print_timestamp(stderr);
            		fprintf(stderr, 
"  InnoDB: Warning: shutting down a not properly started\n"
"                 InnoDB: or created database!\n");
	  	}

	  	return(DB_SUCCESS);
	}

	/* 1. Flush the buffer pool to disk, write the current lsn to
	the tablespace header(s), and copy all log data to archive.
	The step 1 is the real InnoDB shutdown. The remaining steps 2 - ...
	just free data structures after the shutdown. */


	if (srv_fast_shutdown == 2) {
	        ut_print_timestamp(stderr);
		fprintf(stderr,
"  InnoDB: MySQL has requested a very fast shutdown without flushing "
"the InnoDB buffer pool to data files. At the next mysqld startup "
"InnoDB will do a crash recovery!\n");
	}

#ifdef __NETWARE__
	if(!panic_shutdown)
#endif 
	logs_empty_and_mark_files_at_shutdown();

	if (srv_conc_n_threads != 0) {
		fprintf(stderr,
		"InnoDB: Warning: query counter shows %ld queries still\n"
		"InnoDB: inside InnoDB at shutdown\n",
		srv_conc_n_threads);
	}

	/* 2. Make all threads created by InnoDB to exit */

	srv_shutdown_state = SRV_SHUTDOWN_EXIT_THREADS;

        /* In a 'very fast' shutdown, we do not need to wait for these threads
        to die; all which counts is that we flushed the log; a 'very fast'
        shutdown is essentially a crash. */

	if (srv_fast_shutdown == 2) {
		return(DB_SUCCESS);
	}

	/* All threads end up waiting for certain events. Put those events
	to the signaled state. Then the threads will exit themselves in
	os_thread_event_wait(). */

	for (i = 0; i < 1000; i++) {
	        /* NOTE: IF YOU CREATE THREADS IN INNODB, YOU MUST EXIT THEM
	        HERE OR EARLIER */
		
		/* a. Let the lock timeout thread exit */
		os_event_set(srv_lock_timeout_thread_event);		

		/* b. srv error monitor thread exits automatically, no need
		to do anything here */

		/* c. We wake the master thread so that it exits */
		srv_wake_master_thread();

		/* d. Exit the i/o threads */

		os_aio_wake_all_threads_at_shutdown();

		os_mutex_enter(os_sync_mutex);

		if (os_thread_count == 0) {
		        /* All the threads have exited or are just exiting;
			NOTE that the threads may not have completed their
			exit yet. Should we use pthread_join() to make sure
			they have exited? Now we just sleep 0.1 seconds and
			hope that is enough! */

			os_mutex_exit(os_sync_mutex);

			os_thread_sleep(100000);

			break;
		}

		os_mutex_exit(os_sync_mutex);

		os_thread_sleep(100000);
	}

	if (i == 1000) {
	        fprintf(stderr,
"InnoDB: Warning: %lu threads created by InnoDB had not exited at shutdown!\n",
		      (ulong) os_thread_count);
	}

	if (srv_monitor_file) {
		fclose(srv_monitor_file);
		srv_monitor_file = 0;
		if (srv_monitor_file_name) {
			unlink(srv_monitor_file_name);
			mem_free(srv_monitor_file_name);
		}
	}
	if (srv_dict_tmpfile) {
		fclose(srv_dict_tmpfile);
		srv_dict_tmpfile = 0;
	}

	if (srv_misc_tmpfile) {
		fclose(srv_misc_tmpfile);
		srv_misc_tmpfile = 0;
	}

	mutex_free(&srv_monitor_file_mutex);
	mutex_free(&srv_dict_tmpfile_mutex);
	mutex_free(&srv_misc_tmpfile_mutex);

	/* 3. Free all InnoDB's own mutexes and the os_fast_mutexes inside
	them */
	sync_close();

	/* 4. Free the os_conc_mutex and all os_events and os_mutexes */

	srv_free();
	os_sync_free();

	/* Check that all read views are closed except read view owned
	by a purge. */

	if (UT_LIST_GET_LEN(trx_sys->view_list) > 1) {
	        fprintf(stderr,
"InnoDB: Error: all read views were not closed before shutdown:\n"
"InnoDB: %lu read views open \n",
		UT_LIST_GET_LEN(trx_sys->view_list) - 1);
	}

	/* 5. Free all allocated memory and the os_fast_mutex created in
	ut0mem.c */

	ut_free_all_mem();

	if (os_thread_count != 0
	    || os_event_count != 0
	    || os_mutex_count != 0
	    || os_fast_mutex_count != 0) {
	        fprintf(stderr,
"InnoDB: Warning: some resources were not cleaned up in shutdown:\n"
"InnoDB: threads %lu, events %lu, os_mutexes %lu, os_fast_mutexes %lu\n",
			(ulong) os_thread_count, (ulong) os_event_count,
			(ulong) os_mutex_count, (ulong) os_fast_mutex_count);
	}

	if (dict_foreign_err_file) {
		fclose(dict_foreign_err_file);
	}
	if (lock_latest_err_file) {
		fclose(lock_latest_err_file);
	}

	if (srv_print_verbose_log) {
	        ut_print_timestamp(stderr);
	        fprintf(stderr,
"  InnoDB: Shutdown completed; log sequence number %lu %lu\n",
			       (ulong) ut_dulint_get_high(srv_shutdown_lsn),
			       (ulong) ut_dulint_get_low(srv_shutdown_lsn));
	}

	return((int) DB_SUCCESS);
}

#ifdef __NETWARE__
void set_panic_flag_for_netware()
{
	extern ibool panic_shutdown;
	panic_shutdown = TRUE;
}
#endif /* __NETWARE__ */
#endif /* !UNIV_HOTBACKUP */
