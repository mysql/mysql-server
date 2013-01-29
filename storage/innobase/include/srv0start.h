/*****************************************************************************

Copyright (c) 1995, 2012, Oracle and/or its affiliates. All Rights Reserved.

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

#ifdef __WIN__
#define SRV_PATH_SEPARATOR	'\\'
#else
#define SRV_PATH_SEPARATOR	'/'
#endif

/*********************************************************************//**
Normalizes a directory path for Windows: converts slashes to backslashes. */
UNIV_INTERN
void
srv_normalize_path_for_win(
/*=======================*/
	char*	str);	/*!< in/out: null-terminated character string */
/*********************************************************************//**
Reads the data files and their sizes from a character string given in
the .cnf file.
@return	TRUE if ok, FALSE on parse error */
UNIV_INTERN
ibool
srv_parse_data_file_paths_and_sizes(
/*================================*/
	char*	str);	/*!< in/out: the data file path string */
/*********************************************************************//**
Frees the memory allocated by srv_parse_data_file_paths_and_sizes()
and srv_parse_log_group_home_dirs(). */
UNIV_INTERN
void
srv_free_paths_and_sizes(void);
/*==========================*/
/*********************************************************************//**
Adds a slash or a backslash to the end of a string if it is missing
and the string is not empty.
@return	string which has the separator if the string is not empty */
UNIV_INTERN
char*
srv_add_path_separator_if_needed(
/*=============================*/
	char*	str);	/*!< in: null-terminated character string */
#ifndef UNIV_HOTBACKUP
/****************************************************************//**
Starts Innobase and creates a new database if database files
are not found and the user wants.
@return	DB_SUCCESS or error code */
UNIV_INTERN
dberr_t
innobase_start_or_create_for_mysql(void);
/*====================================*/
/****************************************************************//**
Shuts down the Innobase database.
@return	DB_SUCCESS or error code */
UNIV_INTERN
dberr_t
innobase_shutdown_for_mysql(void);

/********************************************************************
Signal all per-table background threads to shutdown, and wait for them to do
so. */
UNIV_INTERN
void
srv_shutdown_table_bg_threads(void);
/*=============================*/

/*************************************************************//**
Copy the file path component of the physical file to parameter. It will
copy up to and including the terminating path separator.
@return number of bytes copied or ULINT_UNDEFINED if destination buffer
	is smaller than the path to be copied. */
UNIV_INTERN
ulint
srv_path_copy(
/*==========*/
	char*		dest,		/*!< out: destination buffer */
	ulint		dest_len,	/*!< in: max bytes to copy */
	const char*	basedir,	/*!< in: base directory */
	const char*	table_name)	/*!< in: source table name */
	__attribute__((nonnull, warn_unused_result));

/*****************************************************************//**
Get the meta-data filename from the table name. */
UNIV_INTERN
void
srv_get_meta_data_filename(
/*======================*/
	dict_table_t*	table,		/*!< in: table */
	char*			filename,	/*!< out: filename */
	ulint			max_len)	/*!< in: filename max length */
	__attribute__((nonnull));

/** Log sequence number at shutdown */
extern	lsn_t	srv_shutdown_lsn;
/** Log sequence number immediately after startup */
extern	lsn_t	srv_start_lsn;

#ifdef HAVE_DARWIN_THREADS
/** TRUE if the F_FULLFSYNC option is available */
extern	ibool	srv_have_fullfsync;
#endif

/** TRUE if the server is being started */
extern	ibool	srv_is_being_started;
/** TRUE if the server was successfully started */
extern	ibool	srv_was_started;
/** TRUE if the server is being started, before rolling back any
incomplete transactions */
extern	ibool	srv_startup_is_before_trx_rollback_phase;

/** TRUE if a raw partition is in use */
extern	ibool	srv_start_raw_disk_in_use;


/** Shutdown state */
enum srv_shutdown_state {
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
extern	enum srv_shutdown_state	srv_shutdown_state;
#endif /* !UNIV_HOTBACKUP */

/** Log 'spaces' have id's >= this */
#define SRV_LOG_SPACE_FIRST_ID		0xFFFFFFF0UL

#endif
