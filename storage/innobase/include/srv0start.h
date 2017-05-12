/*****************************************************************************

Copyright (c) 1995, 2017, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

*****************************************************************************/

/**************************************************//**
@file include/srv0start.h
Starts the Innobase database server

Created 10/10/1995 Heikki Tuuri
*******************************************************/

#ifndef srv0start_h
#define srv0start_h

#include "univ.i"
#include "log0log.h"
#include "ut0byte.h"

// Forward declaration
struct dict_table_t;

#ifndef UNIV_DEBUG
# define RECOVERY_CRASH(x) do {} while(0)
#else
# define RECOVERY_CRASH(x) do {						\
	if (srv_force_recovery_crash == x) {				\
		fprintf(stderr, "innodb_force_recovery_crash=%lu\n",	\
			srv_force_recovery_crash);			\
		fflush(stderr);						\
		_exit(3);						\
	}								\
} while (0)
#endif /* UNIV_DEBUG */

/** Log 'spaces' have id's >= this */
#define SRV_LOG_SPACE_FIRST_ID		0xFFFFFFF0UL

/** If buffer pool is less than the size,
only one buffer pool instance is used. */
#define BUF_POOL_SIZE_THRESHOLD		(1024 * 1024 * 1024)

/*********************************************************************//**
Parse temporary tablespace configuration.
@return true if ok, false on parse error */
bool
srv_parse_temp_data_file_paths_and_sizes(
/*=====================================*/
	char*	str);	/*!< in/out: the data file path string */
/*********************************************************************//**
Frees the memory allocated by srv_parse_data_file_paths_and_sizes()
and srv_parse_log_group_home_dirs(). */
void
srv_free_paths_and_sizes(void);
/*==========================*/
/*********************************************************************//**
Adds a slash or a backslash to the end of a string if it is missing
and the string is not empty.
@return string which has the separator if the string is not empty */
char*
srv_add_path_separator_if_needed(
/*=============================*/
	char*	str);	/*!< in: null-terminated character string */
#ifndef UNIV_HOTBACKUP
/** Start InnoDB.
@param[in]	create_new_db	whether to create a new database
@return DB_SUCCESS or error code */
dberr_t
srv_start(
	bool	create_new_db);

/** On a restart, initialize the remaining InnoDB subsystems so that
any tables (including data dictionary tables) can be accessed. */
void
srv_dict_recover_on_restart();

/** Start up the remaining InnoDB service threads. */
void
srv_start_threads();

/** Shut down all InnoDB background tasks that may look up objects in
the data dictionary. */
void
srv_pre_dd_shutdown();

/** Shut down the InnoDB database. */
void
srv_shutdown();

/*************************************************************//**
Copy the file path component of the physical file to parameter. It will
copy up to and including the terminating path separator.
@return number of bytes copied or ULINT_UNDEFINED if destination buffer
	is smaller than the path to be copied. */
ulint
srv_path_copy(
/*==========*/
	char*		dest,		/*!< out: destination buffer */
	ulint		dest_len,	/*!< in: max bytes to copy */
	const char*	basedir,	/*!< in: base directory */
	const char*	table_name)	/*!< in: source table name */
	MY_ATTRIBUTE((warn_unused_result));

/** Get the meta-data filename from the table name for a
single-table tablespace.
@param[in]	table		table object
@param[out]	filename	filename
@param[in]	max_len		filename max length */
void
srv_get_meta_data_filename(
	dict_table_t*	table,
	char*		filename,
	ulint		max_len);

/** Get the encryption-data filename from the table name for a
single-table tablespace.
@param[in]	table		table object
@param[out]	filename	filename
@param[in]	max_len		filename max length */
void
srv_get_encryption_data_filename(
	dict_table_t*	table,
	char*		filename,
	ulint		max_len);

/** Log sequence number at shutdown */
extern	lsn_t	srv_shutdown_lsn;
/** Log sequence number immediately after startup */
extern	lsn_t	srv_start_lsn;

/** true if the server is being started */
extern	bool	srv_is_being_started;
/** true if SYS_TABLESPACES is available for lookups */
extern	bool	srv_sys_tablespaces_open;
/** true if the server is being started, before rolling back any
incomplete transactions */
extern	bool	srv_startup_is_before_trx_rollback_phase;
#ifdef UNIV_DEBUG
/** true if srv_pre_dd_shutdown() has been completed */
extern	bool	srv_is_being_shutdown;
#endif /* UNIV_DEBUG */

/** TRUE if a raw partition is in use */
extern	ibool	srv_start_raw_disk_in_use;

/** Shutdown state */
enum srv_shutdown_t {
	SRV_SHUTDOWN_NONE = 0,	/*!< Database running normally */
	SRV_SHUTDOWN_CLEANUP,	/*!< Cleaning up in
				logs_empty_and_mark_files_at_shutdown() */
	SRV_SHUTDOWN_FLUSH_PHASE,/*!< At this phase the master and the
				purge threads must have completed their
				work. Once we enter this phase the
				page_cleaner can clean up the buffer
				pool and exit */
	SRV_SHUTDOWN_LAST_PHASE,/*!< Last phase after ensuring that
				the buffer pool can be freed: flush
				all file spaces and close all files */
	SRV_SHUTDOWN_EXIT_THREADS/*!< Exit all threads */
};

/** At a shutdown this value climbs from SRV_SHUTDOWN_NONE to
SRV_SHUTDOWN_CLEANUP and then to SRV_SHUTDOWN_LAST_PHASE, and so on */
extern	enum srv_shutdown_t	srv_shutdown_state;

#endif /* !UNIV_HOTBACKUP */

/** Call exit(3) */
void
srv_fatal_error()
	MY_ATTRIBUTE((noreturn));

#endif
