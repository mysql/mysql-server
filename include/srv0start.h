/*****************************************************************************

Copyright (c) 1995, 2009, Innobase Oy. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA

*****************************************************************************/

/******************************************************
Starts the Innobase database server

Created 10/10/1995 Heikki Tuuri
*******************************************************/

#ifndef srv0start_h
#define srv0start_h

#include "univ.i"
#include "ut0byte.h"

/*************************************************************************
Normalizes a directory path for Windows: converts slashes to backslashes. */
UNIV_INTERN
void
srv_normalize_path_for_win(
/*=======================*/
	char*	str);	/* in/out: null-terminated character string */
/*************************************************************************
Reads the data files and their sizes from a character string given in
the .cnf file. */
UNIV_INTERN
ibool
srv_parse_data_file_paths_and_sizes(
/*================================*/
			/* out: TRUE if ok, FALSE on parse error */
	char*	str);	/* in/out: the data file path string */
/*************************************************************************
Reads log group home directories from a character string given in
the .cnf file. */
UNIV_INTERN
ibool
srv_parse_log_group_home_dirs(
/*==========================*/
			/* out: TRUE if ok, FALSE on parse error */
	char*	str);	/* in/out: character string */
/*************************************************************************
Frees the memory allocated by srv_parse_data_file_paths_and_sizes()
and srv_parse_log_group_home_dirs(). */
UNIV_INTERN
void
srv_free_paths_and_sizes(void);
/*==========================*/
/*************************************************************************
Adds a slash or a backslash to the end of a string if it is missing
and the string is not empty. */
UNIV_INTERN
char*
srv_add_path_separator_if_needed(
/*=============================*/
			/* out: string which has the separator if the
			string is not empty */
	char*	str);	/* in: null-terminated character string */
/********************************************************************
Starts Innobase and creates a new database if database files
are not found and the user wants. */
UNIV_INTERN
int
innobase_start_or_create_for_mysql(void);
/*====================================*/
				/* out: DB_SUCCESS or error code */
/********************************************************************
Shuts down the Innobase database. */
UNIV_INTERN
int
innobase_shutdown_for_mysql(void);
/*=============================*/
				/* out: DB_SUCCESS or error code */
extern	ib_uint64_t	srv_shutdown_lsn;
extern	ib_uint64_t	srv_start_lsn;

#ifdef __NETWARE__
void set_panic_flag_for_netware(void);
#endif

#ifdef HAVE_DARWIN_THREADS
extern	ibool	srv_have_fullfsync;
#endif

extern	ibool	srv_is_being_started;
extern	ibool	srv_was_started;
extern	ibool	srv_startup_is_before_trx_rollback_phase;
extern	ibool	srv_is_being_shut_down;

extern	ibool	srv_start_raw_disk_in_use;

/* At a shutdown the value first climbs from 0 to SRV_SHUTDOWN_CLEANUP
and then to SRV_SHUTDOWN_LAST_PHASE, and so on */

extern	ulint	srv_shutdown_state;

#define SRV_SHUTDOWN_CLEANUP	   1
#define SRV_SHUTDOWN_LAST_PHASE	   2
#define SRV_SHUTDOWN_EXIT_THREADS  3

/* Log 'spaces' have id's >= this */
#define SRV_LOG_SPACE_FIRST_ID		0xFFFFFFF0UL

#endif
