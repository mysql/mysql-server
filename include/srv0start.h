/******************************************************
Starts the Innobase database server

(c) 1995-2000 Innobase Oy

Created 10/10/1995 Heikki Tuuri
*******************************************************/


#ifndef srv0start_h
#define srv0start_h

#include "univ.i"
#include "ut0byte.h"

/*************************************************************************
Normalizes a directory path for Windows: converts slashes to backslashes. */

void
srv_normalize_path_for_win(
/*=======================*/
	char*	str);	/* in/out: null-terminated character string */
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
	ulint*	max_auto_extend_size);	/* out: max auto extend size for the
					last file if specified, 0 if not */
/*************************************************************************
Reads log group home directories from a character string given in
the .cnf file. */

ibool
srv_parse_log_group_home_dirs(
/*==========================*/
					/* out: TRUE if ok, FALSE if parsing
					error */
	char*	str,			/* in: character string */
	char***	log_group_home_dirs);	/* out, own: log group home dirs */
/*************************************************************************
Adds a slash or a backslash to the end of a string if it is missing
and the string is not empty. */

char*
srv_add_path_separator_if_needed(
/*=============================*/
			/* out: string which has the separator if the
			string is not empty */
	char*	str);	/* in: null-terminated character string */
/********************************************************************
Starts Innobase and creates a new database if database files
are not found and the user wants. Server parameters are
read from a file of name "srv_init" in the ib_home directory. */

int
innobase_start_or_create_for_mysql(void);
/*====================================*/
				/* out: DB_SUCCESS or error code */
/********************************************************************
Shuts down the Innobase database. */
int
innobase_shutdown_for_mysql(void);
/*=============================*/
				/* out: DB_SUCCESS or error code */
extern	dulint	srv_shutdown_lsn;
extern	dulint	srv_start_lsn;

#ifdef __NETWARE__
void set_panic_flag_for_netware(void);
#endif

#ifdef HAVE_DARWIN_THREADS
extern	ibool	srv_have_fullfsync;
#endif

extern  ulint   srv_sizeof_trx_t_in_ha_innodb_cc;

extern  ibool   srv_is_being_started;
extern	ibool	srv_startup_is_before_trx_rollback_phase;
extern	ibool	srv_is_being_shut_down;

extern  ibool	srv_start_raw_disk_in_use;

/* At a shutdown the value first climbs from 0 to SRV_SHUTDOWN_CLEANUP
and then to SRV_SHUTDOWN_LAST_PHASE, and so on */

extern  ulint   srv_shutdown_state;

#define SRV_SHUTDOWN_CLEANUP	   1
#define SRV_SHUTDOWN_LAST_PHASE	   2
#define SRV_SHUTDOWN_EXIT_THREADS  3

/* Log 'spaces' have id's >= this */
#define SRV_LOG_SPACE_FIRST_ID		0xFFFFFFF0UL

#endif
