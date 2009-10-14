/* Copyright (c) 2005 PrimeBase Technologies GmbH
 *
 * PrimeBase XT
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * 2005-01-15	Paul McCullagh
 *
 * H&G2JCtL
 */

#include "xt_config.h"

#ifdef DRIZZLED
#include <bitset>
#endif

#include <string.h>
#include <stdio.h>

#include "pthread_xt.h"
#include "hashtab_xt.h"
#include "filesys_xt.h"
#include "database_xt.h"
#include "memory_xt.h"
#include "heap_xt.h"
#include "datalog_xt.h"
#include "strutil_xt.h"
#include "util_xt.h"
#include "trace_xt.h"

#ifdef DEBUG
//#define XT_TEST_XACT_OVERFLOW
#endif

#ifndef NAME_MAX
#define NAME_MAX 128
#endif

/*
 * -----------------------------------------------------------------------
 * GLOBALS
 */

xtPublic xtLogOffset		xt_db_log_file_threshold;
xtPublic size_t				xt_db_log_buffer_size;
xtPublic size_t				xt_db_transaction_buffer_size;
xtPublic size_t				xt_db_checkpoint_frequency;
xtPublic off_t				xt_db_data_log_threshold;
xtPublic size_t				xt_db_data_file_grow_size;
xtPublic size_t				xt_db_row_file_grow_size;
xtPublic int				xt_db_garbage_threshold;
xtPublic int				xt_db_log_file_count;
xtPublic int				xt_db_auto_increment_mode;		/* 0 = MySQL compatible, 1 = PrimeBase Compatible. */
xtPublic int				xt_db_offline_log_function;		/* 0 = recycle logs, 1 = delete logs, 2 = keep logs */
xtPublic int				xt_db_sweeper_priority;			/* 0 = low (default), 1 = normal, 2 = high */

xtPublic XTSortedListPtr	xt_db_open_db_by_id = NULL;
xtPublic XTHashTabPtr		xt_db_open_databases = NULL;
xtPublic time_t				xt_db_approximate_time = 0;		/* A "fast" alternative timer (not too accurate). */

static xtDatabaseID				db_next_id = 1;
static volatile XTOpenFilePtr	db_lock_file = NULL;

/*
 * -----------------------------------------------------------------------
 * LOCK/UNLOCK INSTALLATION
 */

xtPublic void xt_lock_installation(XTThreadPtr self, char *installation_path)
{
	char			file_path[PATH_MAX];
	char			buffer[101];
	size_t			red_size;
	llong			pid;
	xtBool			cd = pbxt_crash_debug;

	xt_strcpy(PATH_MAX, file_path, installation_path);
	xt_add_pbxt_file(PATH_MAX, file_path, "no-debug");
	if (xt_fs_exists(file_path))
		pbxt_crash_debug = FALSE;
	xt_strcpy(PATH_MAX, file_path, installation_path);
	xt_add_pbxt_file(PATH_MAX, file_path, "crash-debug");
	if (xt_fs_exists(file_path))
		pbxt_crash_debug = TRUE;

	if (pbxt_crash_debug != cd) {
		if (pbxt_crash_debug)
			xt_logf(XT_NT_WARNING, "Crash debugging has been turned on ('crash-debug' file exists)\n");
		else
			xt_logf(XT_NT_WARNING, "Crash debugging has been turned off ('no-debug' file exists)\n");
	}
	else if (pbxt_crash_debug)
		xt_logf(XT_NT_WARNING, "Crash debugging is enabled\n");

	/* Moved the lock file out of the pbxt directory so that
	 * it is possible to drop the pbxt database!
	 */
	xt_strcpy(PATH_MAX, file_path, installation_path);
	xt_add_dir_char(PATH_MAX, file_path);
	xt_strcat(PATH_MAX, file_path, "pbxt-lock");
	db_lock_file = xt_open_file(self, file_path, XT_FS_CREATE | XT_FS_MAKE_PATH);

	try_(a) {
		if (!xt_lock_file(self, db_lock_file)) {
			xt_logf(XT_NT_ERROR, "A server appears to already be running\n");
			xt_logf(XT_NT_ERROR, "The file: %s, is locked\n", file_path);
			xt_throw_xterr(XT_CONTEXT, XT_ERR_SERVER_RUNNING);
		}
		if (!xt_pread_file(db_lock_file, 0, 100, 0, buffer, &red_size, &self->st_statistics.st_rec, self))
			xt_throw(self);
		if (red_size > 0) {
			buffer[red_size] = 0;
#ifdef XT_WIN
			pid = (llong) _atoi64(buffer);
#else
			pid = atoll(buffer);
#endif
			/* Problem with this code is, after a restart
			 * the process ID's are reused.
			 * If some system process grabs the proc id that
			 * the server had on the last run, then
			 * the database will not start.
			if (xt_process_exists((xtProcID) pid)) {
				xt_logf(XT_NT_ERROR, "A server appears to already be running, process ID: %lld\n", pid);
				xt_logf(XT_NT_ERROR, "Remove the file: %s, if this is not the case\n", file_path);
				xt_throw_xterr(XT_CONTEXT, XT_ERR_SERVER_RUNNING);
			}
			*/
			xt_logf(XT_NT_INFO, "The server was not shutdown correctly, recovery required\n");
#ifdef XT_BACKUP_BEFORE_RECOVERY
			if (pbxt_crash_debug) {
				/* The server was not shut down correctly. Make a backup before
				 * we start recovery.
				 */
				char extension[100];

				for (int i=1;;i++) {
					xt_strcpy(PATH_MAX, file_path, installation_path);
					xt_remove_dir_char(file_path);
					sprintf(extension, "-recovery-%d", i);
					xt_strcat(PATH_MAX, file_path, extension);
					if (!xt_fs_exists(file_path))
						break;
				}
				xt_logf(XT_NT_INFO, "In order to reproduce recovery errors a backup of the installation\n");
				xt_logf(XT_NT_INFO, "will be made to:\n");
				xt_logf(XT_NT_INFO, "%s\n", file_path);
				xt_logf(XT_NT_INFO, "Copy in progress...\n");
				xt_fs_copy_dir(self, installation_path, file_path);
				xt_logf(XT_NT_INFO, "Copy OK\n");
			}
#endif
		}

		sprintf(buffer, "%lld", (llong) xt_getpid());
		xt_set_eof_file(self, db_lock_file, 0);
		if (!xt_pwrite_file(db_lock_file, 0, strlen(buffer), buffer, &self->st_statistics.st_rec, self))
			xt_throw(self);
	}
	catch_(a) {
		xt_close_file(self, db_lock_file);
		db_lock_file = NULL;
		xt_throw(self);
	}
	cont_(a);
}

xtPublic void xt_unlock_installation(XTThreadPtr self, char *installation_path)
{
	if (db_lock_file) {
		char lock_file[PATH_MAX];

		xt_unlock_file(NULL, db_lock_file);
		xt_close_file_ns(db_lock_file);
		db_lock_file = NULL;

		xt_strcpy(PATH_MAX, lock_file, installation_path);
		xt_add_dir_char(PATH_MAX, lock_file);
		xt_strcat(PATH_MAX, lock_file, "pbxt-lock");
		xt_fs_delete(self, lock_file);
	}
}

int *xt_bad_pointer = 0;

void xt_crash_me(void)
{
	if (pbxt_crash_debug)
		*xt_bad_pointer = 123;
}

/*
 * -----------------------------------------------------------------------
 * INIT/EXIT DATABASE
 */

static xtBool db_hash_comp(void *key, void *data)
{
	XTDatabaseHPtr	db = (XTDatabaseHPtr) data;

	return strcmp((char *) key, db->db_name) == 0;
}

static xtHashValue db_hash(xtBool is_key, void *key_data)
{
	XTDatabaseHPtr	db = (XTDatabaseHPtr) key_data;

	if (is_key)
		return xt_ht_hash((char *) key_data);
	return xt_ht_hash(db->db_name);
}

static xtBool db_hash_comp_ci(void *key, void *data)
{
	XTDatabaseHPtr	db = (XTDatabaseHPtr) data;

	return strcasecmp((char *) key, db->db_name) == 0;
}

static xtHashValue db_hash_ci(xtBool is_key, void *key_data)
{
	XTDatabaseHPtr	db = (XTDatabaseHPtr) key_data;

	if (is_key)
		return xt_ht_casehash((char *) key_data);
	return xt_ht_casehash(db->db_name);
}

static void db_hash_free(XTThreadPtr self, void *data)
{
	xt_heap_release(self, (XTDatabaseHPtr) data);
}

static int db_cmp_db_id(struct XTThread *XT_UNUSED(self), register const void *XT_UNUSED(thunk), register const void *a, register const void *b)
{
	xtDatabaseID	db_id = *((xtDatabaseID *) a);
	XTDatabaseHPtr	*db_ptr = (XTDatabaseHPtr *) b;

	if (db_id == (*db_ptr)->db_id)
		return 0;
	if (db_id < (*db_ptr)->db_id)
		return -1;
	return 1;
}

xtPublic void xt_init_databases(XTThreadPtr self)
{
	if (pbxt_ignore_case)
		xt_db_open_databases = xt_new_hashtable(self, db_hash_comp_ci, db_hash_ci, db_hash_free, TRUE, TRUE);
	else
		xt_db_open_databases = xt_new_hashtable(self, db_hash_comp, db_hash, db_hash_free, TRUE, TRUE);
	xt_db_open_db_by_id = xt_new_sortedlist(self, sizeof(XTDatabaseHPtr), 20, 10, db_cmp_db_id, NULL, NULL, FALSE, FALSE);
}

xtPublic void xt_stop_database_threads(XTThreadPtr self, xtBool sync)
{
	u_int			len = 0;
	XTDatabaseHPtr	*dbptr;
	XTDatabaseHPtr	db = NULL;
	
	if (xt_db_open_db_by_id)
		len = xt_sl_get_size(xt_db_open_db_by_id);
	for (u_int i=0; i<len; i++) {
		if ((dbptr = (XTDatabaseHPtr *) xt_sl_item_at(xt_db_open_db_by_id, i))) {
			db = *dbptr;
			if (sync) {
				/* Wait for the sweeper: */
				xt_wait_for_sweeper(self, db, 16);
				
				/* Wait for the writer: */
				xt_wait_for_writer(self, db);

				/* Wait for the checkpointer: */
				xt_wait_for_checkpointer(self, db);
			}
			xt_stop_checkpointer(self, db);
			xt_stop_writer(self, db);
			xt_stop_sweeper(self, db);
			xt_stop_compactor(self, db);
		}
	}
}

xtPublic void xt_exit_databases(XTThreadPtr self)
{
	if (xt_db_open_databases) {
		xt_free_hashtable(self, xt_db_open_databases);
		xt_db_open_databases = NULL;
	}
	if (xt_db_open_db_by_id) {
		xt_free_sortedlist(self, xt_db_open_db_by_id);
		xt_db_open_db_by_id = NULL;
	}
}

xtPublic void xt_create_database(XTThreadPtr self, char *path)
{
	xt_fs_mkdir(self, path);
}

static void db_finalize(XTThreadPtr self, void *x)
{
	XTDatabaseHPtr	db = (XTDatabaseHPtr) x;

	xt_stop_checkpointer(self, db);
	xt_stop_compactor(self, db);
	xt_stop_sweeper(self, db);
	xt_stop_writer(self, db);

	xt_sl_delete(self, xt_db_open_db_by_id, &db->db_id);
	/* 
	 * Important is that xt_db_pool_exit() is called
	 * before xt_xn_exit_db() because xt_xn_exit_db()
	 * frees the checkpoint information which
	 * may be required to shutdown the tables, which
	 * flushes tables, and therefore does a checkpoint.
	 */
	/* This was the previous order of shutdown:
	xt_xn_exit_db(self, db);
	xt_dl_exit_db(self, db);
	xt_db_pool_exit(self, db);
	db->db_indlogs.ilp_exit(self);
	*/

	xt_db_pool_exit(self, db);
	db->db_indlogs.ilp_exit(self); 
	xt_dl_exit_db(self, db);
	xt_xn_exit_db(self, db);
	xt_tab_exit_db(self, db);
	if (db->db_name) {
		xt_free(self, db->db_name);
		db->db_name = NULL;
	}
	if (db->db_main_path) {
		xt_free(self, db->db_main_path);
		db->db_main_path = NULL;
	}
}

static void db_onrelease(XTThreadPtr self, void *XT_UNUSED(x))
{
	/* Signal threads waiting for exclusive use of the database: */
	if (xt_db_open_databases)	// The database may already be closed.
		xt_ht_signal(self, xt_db_open_databases);
}

xtPublic void xt_add_pbxt_file(size_t size, char *path, const char *file)
{
	xt_add_dir_char(size, path);
	xt_strcat(size, path, "pbxt");
	xt_add_dir_char(size, path);
	xt_strcat(size, path, file);
}

xtPublic void xt_add_location_file(size_t size, char *path)
{
	xt_add_dir_char(size, path);
	xt_strcat(size, path, "pbxt");
	xt_add_dir_char(size, path);
	xt_strcat(size, path, "location");
}

xtPublic void xt_add_pbxt_dir(size_t size, char *path)
{
	xt_add_dir_char(size, path);
	xt_strcat(size, path, "pbxt");
}

xtPublic void xt_add_system_dir(size_t size, char *path)
{
	xt_add_dir_char(size, path);
	xt_strcat(size, path, "pbxt");
	xt_add_dir_char(size, path);
	xt_strcat(size, path, "system");
}

xtPublic void xt_add_data_dir(size_t size, char *path)
{
	xt_add_dir_char(size, path);
	xt_strcat(size, path, "pbxt");
	xt_add_dir_char(size, path);
	xt_strcat(size, path, "data");
}

/*
 * I have a problem here. I cannot rely on the path given to xt_get_database() to be
 * consistant. When called from ha_create_table() the path is not modified.
 * However when called from ha_open() the path is first transformed by a call to
 * fn_format(). I have given an example from a stack trace below.
 *
 * In this case the odd path comes from the option:
 * --tmpdir=/Users/build/Development/mysql/debug-mysql/mysql-test/var//tmp
 *
 * #3  0x001a3818 in ha_pbxt::create(char const*, st_table*, st_ha_create_information*) 
 *     (this=0x2036898, table_path=0xf0060bd0 "/users/build/development/mysql/debug-my
 *     sql/mysql-test/var//tmp/#sql5718_1_0.frm", table_arg=0xf00601c0,
 *     create_info=0x2017410) at ha_pbxt.cc:2323
 * #4  0x00140d74 in ha_create_table(char const*, st_ha_create_information*, bool) 
 *     (name=0xf0060bd0 "/users/build/development/mysql/debug-mysql/mysql-te
 *     st/var//tmp/#sql5718_1_0.frm", create_info=0x2017410, 
 *     update_create_info=false) at handler.cc:1387
 *
 * #4  0x0013f7a4 in handler::ha_open(char const*, int, int) (this=0x203ba98, 
 *     name=0xf005eb70 "/users/build/development/mysql/debug-mysql/mysql-te
 *     st/var/tmp/#sql5718_1_1", mode=2, test_if_locked=2) at handler.cc:993
 * #5  0x000cd900 in openfrm(char const*, char const*, unsigned, unsigned, 
 *     unsigned, st_table*) (name=0xf005f260 "/users/build/development/mys
 *     ql/debug-mysql/mysql-test/var//tmp/#sql5718_1_1.frm", 
 *     alias=0xf005fb90 "#sql-5718_1", db_stat=7, prgflag=44, 
 *     ha_open_flags=0, outparam=0x2039e18) at table.cc:771
 *
 * As a result, I no longer use the entire path as the key to find a database.
 * Just the last component of the path (i.e. the database name) should be
 * sufficient!?
 */
xtPublic XTDatabaseHPtr xt_get_database(XTThreadPtr self, char *path, xtBool multi_path)
{
	XTDatabaseHPtr	db = NULL;
	char			db_path[PATH_MAX];
	char			db_name[NAME_MAX];
	xtBool			multi_path_db = FALSE;

	/* A database may not be in use when this is called. */
	ASSERT(!self->st_database);
	xt_ht_lock(self, xt_db_open_databases);
	pushr_(xt_ht_unlock, xt_db_open_databases);

	xt_strcpy(PATH_MAX, db_path, path);
	xt_add_location_file(PATH_MAX, db_path);
	if (multi_path || xt_fs_exists(db_path))
		multi_path_db = TRUE;

	xt_strcpy(PATH_MAX, db_path, path);
	xt_remove_dir_char(db_path);
	xt_strcpy(NAME_MAX, db_name, xt_last_directory_of_path(db_path));

	db = (XTDatabaseHPtr) xt_ht_get(self, xt_db_open_databases, db_name);
	if (!db) {
		pushsr_(db, xt_heap_release, (XTDatabaseHPtr) xt_heap_new(self, sizeof(XTDatabaseRec), db_finalize));
		xt_heap_set_release_callback(self, db, db_onrelease);
		db->db_id = db_next_id++;
		db->db_name = xt_dup_string(self, db_name);
		db->db_main_path = xt_dup_string(self, db_path);
		db->db_multi_path = multi_path_db;
#ifdef XT_TEST_XACT_OVERFLOW
		/* Test transaction ID overflow: */
		db->db_xn_curr_id = 0xFFFFFFFF - 30;
#endif
		xt_db_pool_init(self, db);
		xt_tab_init_db(self, db);
		xt_dl_init_db(self, db);

		/* Initialize the index logs: */
		db->db_indlogs.ilp_init(self, db, XT_INDEX_WRITE_BUFFER_SIZE); 

		xt_xn_init_db(self, db);
		xt_sl_insert(self, xt_db_open_db_by_id, &db->db_id, &db);

		xt_start_sweeper(self, db);
		xt_start_compactor(self, db);
		xt_start_writer(self, db);
		xt_start_checkpointer(self, db);

		popr_();
		xt_ht_put(self, xt_db_open_databases, db);

		/* The recovery process could attach parts of the open
		 * database to the thread!
		 */
		xt_unuse_database(self, self);

	}
	xt_heap_reference(self, db);
	freer_();

	/* {INDEX-RECOV_ROWID}
	 * Wait for sweeper to finish processing possibly
	 * unswept transactions after recovery.
	 * This is required because during recovery for
	 * all index entries written the row_id is set.
	 *
	 * When the row ID is set, this means that the row
	 * is "clean". i.e. visible to all transactions.
	 *
	 * Obviously this is not necessary the case for all
	 * index entries recovered. For example, 
	 * transactions that still need to be swept may be
	 * rolled back.
	 *
	 * As a result, we have to wait the the sweeper
	 * to complete. Only then can we be sure that
	 * all index entries that are not visible have
	 * been removed.
	 *
	 * {OPEN-DB-SWEEPER-WAIT}
	 * This has been moved to after the release of the open
	 * database lock because:
	 *
	 * - We are waiting for the sweeper which may run out of
	 * record cache.
	 * - If it runs out of cache it well wait
	 * for the freeer thread.
	 * - For the freeer thread to be able to work it needs
	 * to open the database.
	 * - To open the database it needs the open database
	 * lock.
	 */
	pushr_(xt_heap_release, db);
	xt_wait_for_sweeper(self, db, 0);
	popr_();

	return db;
}

xtPublic XTDatabaseHPtr xt_get_database_by_id(XTThreadPtr self, xtDatabaseID db_id)
{
	XTDatabaseHPtr	*dbptr;
	XTDatabaseHPtr	db = NULL;

	xt_ht_lock(self, xt_db_open_databases);
	pushr_(xt_ht_unlock, xt_db_open_databases);
	if ((dbptr = (XTDatabaseHPtr *) xt_sl_find(self, xt_db_open_db_by_id, &db_id))) {
		db = *dbptr;
		xt_heap_reference(self, db);
	}
	freer_(); // xt_ht_unlock(xt_db_open_databases)
	return db;
}

xtPublic void xt_check_database(XTThreadPtr self)
{
	xt_check_tables(self);
	/*
	xt_check_handlefiles(self, db);
	*/
}

xtPublic void xt_drop_database(XTThreadPtr self, XTDatabaseHPtr	db)
{
	char			path[PATH_MAX];
	char			db_name[NAME_MAX];
	XTOpenDirPtr	od;
	char			*file;
	XTTablePathPtr	*tp_ptr;

	xt_ht_lock(self, xt_db_open_databases);
	pushr_(xt_ht_unlock, xt_db_open_databases);

	/* Shutdown the database daemons: */
	xt_stop_checkpointer(self, db);
	xt_stop_sweeper(self, db);
	xt_stop_compactor(self, db);
	xt_stop_writer(self, db);

	/* Remove the database from the directory: */
	xt_strcpy(NAME_MAX, db_name, db->db_name);
	xt_ht_del(self, xt_db_open_databases, db_name);

	/* Release the lock on the database directory: */
	freer_(); // xt_ht_unlock(xt_db_open_databases)

	/* Delete the transaction logs: */
	xt_xlog_delete_logs(self, db);

	/* Delete the data logs: */
	xt_dl_delete_logs(self, db);

	for (u_int i=0; i<xt_sl_get_size(db->db_table_paths); i++) {

		tp_ptr = (XTTablePathPtr *) xt_sl_item_at(db->db_table_paths, i);

		xt_strcpy(PATH_MAX, path, (*tp_ptr)->tp_path);

		/* Delete all files in the database: */
		pushsr_(od, xt_dir_close, xt_dir_open(self, path, NULL));
		while (xt_dir_next(self, od)) {
			file = xt_dir_name(self, od);
			if (xt_ends_with(file, ".xtr") ||
				xt_ends_with(file, ".xtd") ||
				xt_ends_with(file, ".xti") ||
				xt_ends_with(file, ".xt"))
			{
				xt_add_dir_char(PATH_MAX, path);
				xt_strcat(PATH_MAX, path, file);
				xt_fs_delete(self, path);
				xt_remove_last_name_of_path(path);
			}
		}
		freer_(); // xt_dir_close(od)
		
	}
	if (!db->db_multi_path) {
		xt_strcpy(PATH_MAX, path, db->db_main_path);
		xt_add_pbxt_dir(PATH_MAX, path);
		if (!xt_fs_rmdir(NULL, path))
			xt_log_and_clear_exception(self);
	}
}

/*
 * Open/use a database.
 */
xtPublic void xt_open_database(XTThreadPtr self, char *path, xtBool multi_path)
{
	XTDatabaseHPtr db;

	/* We cannot get a database, without unusing the current
	 * first. The reason is that the restart process will
	 * partially set the current database!
	 */
	xt_unuse_database(self, self);
	db = xt_get_database(self, path, multi_path);
	pushr_(xt_heap_release, db);
	xt_use_database(self, db, XT_FOR_USER);
	freer_();	// xt_heap_release(self, db);	
}

/* This function can only be called if you do not already have a database in
 * use. This is because to get a database pointer you are not allowed
 * to have a database in use!
 */
xtPublic void xt_use_database(XTThreadPtr self, XTDatabaseHPtr db, int what_for)
{
	/* Check if a transaction is in progress. If so,
	 * we cannot change the database!
	 */
	if (self->st_xact_data || self->st_database)
		xt_throw_xterr(XT_CONTEXT, XT_ERR_CANNOT_CHANGE_DB);

	xt_heap_reference(self, db);
	self->st_database = db;
#ifdef XT_WAIT_FOR_CLEANUP
	self->st_last_xact = 0;
	for (int i=0; i<XT_MAX_XACT_BEHIND; i++) {
		self->st_prev_xact[i] = db->db_xn_curr_id;
	}
#endif
	xt_xn_init_thread(self, what_for);
}

xtPublic void xt_unuse_database(XTThreadPtr self, XTThreadPtr other_thr)
{
	/* Abort the transacion if it belongs exclusively to this thread. */
	xt_lock_mutex(self, &other_thr->t_lock);
	pushr_(xt_unlock_mutex, &other_thr->t_lock);

	xt_xn_exit_thread(other_thr);
	if (other_thr->st_database) {
		xt_heap_release(self, other_thr->st_database);
		other_thr->st_database = NULL;
	}
	
	freer_();
}

xtPublic void xt_db_init_thread(XTThreadPtr XT_UNUSED(self), XTThreadPtr XT_UNUSED(new_thread))
{
#ifdef XT_IMPLEMENT_NO_ACTION
	memset(&new_thread->st_restrict_list, 0, sizeof(XTBasicListRec));
	new_thread->st_restrict_list.bl_item_size = sizeof(XTRestrictItemRec);
#endif
}

xtPublic void xt_db_exit_thread(XTThreadPtr self)
{
#ifdef XT_IMPLEMENT_NO_ACTION
	xt_bl_free(NULL, &self->st_restrict_list);
#endif
	xt_unuse_database(self, self);
}

/*
 * -----------------------------------------------------------------------
 * OPEN TABLE POOL
 */

#ifdef UNUSED_CODE
static void check_free_list(XTDatabaseHPtr db)
{
	XTOpenTablePtr	ot;
	u_int			cnt = 0;

	ot = db->db_ot_pool.otp_mr_used;
	if (ot)
		ASSERT_NS(!ot->ot_otp_mr_used);
	ot = db->db_ot_pool.otp_lr_used;
	if (ot)
		ASSERT_NS(!ot->ot_otp_lr_used);
	while (ot) {
		cnt++;
		ot = ot->ot_otp_mr_used;
	}
	ASSERT_NS(cnt == db->db_ot_pool.otp_total_free);
}
#endif

xtPublic void xt_db_pool_init(XTThreadPtr self, XTDatabaseHPtr db)
{
	memset(&db->db_ot_pool, 0, sizeof(XTAllTablePoolsRec));
	xt_init_mutex_with_autoname(self, &db->db_ot_pool.opt_lock);
	xt_init_cond(self, &db->db_ot_pool.opt_cond);
}

xtPublic void xt_db_pool_exit(XTThreadPtr self, XTDatabaseHPtr db)
{
	XTOpenTablePoolPtr	table_pool, tmp;
	XTOpenTablePtr		ot, tmp_ot;

	xt_free_mutex(&db->db_ot_pool.opt_lock);
	xt_free_cond(&db->db_ot_pool.opt_cond);
	
	for (u_int i=0; i<XT_OPEN_TABLE_POOL_HASH_SIZE; i++) {
		table_pool = db->db_ot_pool.otp_hash[i];
		while (table_pool) {
			tmp = table_pool->opt_next_hash;
			ot = table_pool->opt_free_list;
			while (ot) {
				tmp_ot = ot->ot_otp_next_free;
				ot->ot_thread = self;
				xt_close_table(ot, TRUE, FALSE);
				ot = tmp_ot;
			}
			xt_free(self, table_pool);
			table_pool = tmp;
		}
	}
}

static XTOpenTablePoolPtr db_get_open_table_pool(XTDatabaseHPtr db, xtTableID tab_id)
{
	XTOpenTablePoolPtr	table_pool;
	u_int				hash;

	hash = tab_id % XT_OPEN_TABLE_POOL_HASH_SIZE;
	table_pool = db->db_ot_pool.otp_hash[hash];
	while (table_pool) {
		if (table_pool->opt_tab_id == tab_id)
			return table_pool;
		table_pool = table_pool->opt_next_hash;
	}
	
	if (!(table_pool = (XTOpenTablePoolPtr) xt_malloc_ns(sizeof(XTOpenTablePoolRec))))
		return NULL;

	table_pool->opt_db = db;
	table_pool->opt_tab_id = tab_id;
	table_pool->opt_total_open = 0;
	table_pool->opt_locked = FALSE;
	table_pool->opt_flushing = 0;
	table_pool->opt_free_list = NULL;
	table_pool->opt_next_hash = db->db_ot_pool.otp_hash[hash];
	db->db_ot_pool.otp_hash[hash] = table_pool;
	
	return table_pool;
}

static void db_free_open_table_pool(XTThreadPtr self, XTOpenTablePoolPtr table_pool)
{
	if (!table_pool->opt_locked && !table_pool->opt_flushing && !table_pool->opt_total_open) {
		XTOpenTablePoolPtr	ptr, pptr = NULL;
		u_int				hash;

		hash = table_pool->opt_tab_id % XT_OPEN_TABLE_POOL_HASH_SIZE;
		ptr = table_pool->opt_db->db_ot_pool.otp_hash[hash];
		while (ptr) {
			if (ptr == table_pool)
				break;
			pptr = ptr;
			ptr = ptr->opt_next_hash;
		}
		
		if (ptr == table_pool) {
			if (pptr)
				pptr->opt_next_hash = table_pool->opt_next_hash;
			else
				table_pool->opt_db->db_ot_pool.otp_hash[hash] = table_pool->opt_next_hash;
		}

		xt_free(self, table_pool);
	}
}

static XTOpenTablePoolPtr db_lock_table_pool(XTThreadPtr self, XTDatabaseHPtr db, xtTableID tab_id, xtBool flush_table, xtBool wait_for_open)
{
	XTOpenTablePoolPtr	table_pool;
	XTOpenTablePtr		ot, tmp_ot;

	xt_lock_mutex(self, &db->db_ot_pool.opt_lock);
	pushr_(xt_unlock_mutex, &db->db_ot_pool.opt_lock);

	if (!(table_pool = db_get_open_table_pool(db, tab_id)))
		xt_throw(self);

	/* Wait for the lock: */
	while (table_pool->opt_locked) {
		xt_timed_wait_cond(self, &db->db_ot_pool.opt_cond, &db->db_ot_pool.opt_lock, 2000);
		if (!(table_pool = db_get_open_table_pool(db, tab_id)))
			xt_throw(self);
	}

	/* Lock it: */
	table_pool->opt_locked = TRUE;

	if (flush_table) {
		table_pool->opt_flushing++;
		freer_(); // xt_unlock_mutex(db_ot_pool.opt_lock)

		pushr_(xt_db_unlock_table_pool, table_pool);
		/* During this time, background processes can use the
		 * pool!
		 *
		 * May also do a flush, but this is now taken care
		 * of here [*10*]
		 */
		if ((ot = xt_db_open_pool_table(self, db, tab_id, NULL, TRUE))) {
			pushr_(xt_db_return_table_to_pool, ot);
			xt_sync_flush_table(self, ot);
			freer_(); //xt_db_return_table_to_pool_foreground(ot);
		}

		popr_(); // Discard xt_db_unlock_table_pool_no_lock(table_pool)

		xt_lock_mutex(self, &db->db_ot_pool.opt_lock);
		pushr_(xt_unlock_mutex, &db->db_ot_pool.opt_lock);
		table_pool->opt_flushing--;
	}
	
	/* Free all open tables not in use: */
	ot = table_pool->opt_free_list;
	table_pool->opt_free_list = NULL;
	while (ot) {
		tmp_ot = ot->ot_otp_next_free;

		/* Remove from MRU list: */
		if (db->db_ot_pool.otp_lr_used == ot)
			db->db_ot_pool.otp_lr_used = ot->ot_otp_mr_used;
		if (db->db_ot_pool.otp_mr_used == ot)
			db->db_ot_pool.otp_mr_used = ot->ot_otp_lr_used;
		if (ot->ot_otp_lr_used)
			ot->ot_otp_lr_used->ot_otp_mr_used = ot->ot_otp_mr_used;
		if (ot->ot_otp_mr_used)
			ot->ot_otp_mr_used->ot_otp_lr_used = ot->ot_otp_lr_used;

		if (db->db_ot_pool.otp_lr_used)
			db->db_ot_pool.otp_free_time = db->db_ot_pool.otp_lr_used->ot_otp_free_time;
		
		ASSERT_NS(db->db_ot_pool.otp_total_free > 0);
		db->db_ot_pool.otp_total_free--;

		/* Close the table: */
		ASSERT(table_pool->opt_total_open > 0);
		table_pool->opt_total_open--;

		ot->ot_thread = self;
		xt_close_table(ot, table_pool->opt_total_open == 0, FALSE);

		/* Go to the next: */
		ot = tmp_ot;
	}

	/* Wait for other to close: */
	if (wait_for_open) {
		while (table_pool->opt_total_open > 0) {
			xt_timed_wait_cond_ns(&db->db_ot_pool.opt_cond, &db->db_ot_pool.opt_lock, 2000);
		}
	}

	freer_(); // xt_unlock_mutex(db_ot_pool.opt_lock)
	return table_pool;
}

xtPublic XTOpenTablePoolPtr xt_db_lock_table_pool_by_name(XTThreadPtr self, XTDatabaseHPtr db, XTPathStrPtr tab_name, xtBool no_load, xtBool flush_table, xtBool missing_ok, xtBool wait_for_open, XTTableHPtr *ret_tab)
{
	XTOpenTablePoolPtr	table_pool;
	XTTableHPtr			tab;
	xtTableID			tab_id;

	pushsr_(tab, xt_heap_release, xt_use_table(self, tab_name, no_load, missing_ok, NULL));
	if (!tab) {
		freer_(); // xt_heap_release(tab)
		return NULL;
	}

	tab_id = tab->tab_id;

	if (ret_tab) {
		*ret_tab = tab;
		table_pool = db_lock_table_pool(self, db, tab_id, flush_table, wait_for_open);
		popr_(); // Discard xt_heap_release(tab)
		return table_pool;
	}

	freer_(); // xt_heap_release(tab)
	return db_lock_table_pool(self, db, tab_id, flush_table, wait_for_open);
}

xtPublic void xt_db_wait_for_open_tables(XTThreadPtr self, XTOpenTablePoolPtr table_pool)
{
	XTDatabaseHPtr db = table_pool->opt_db;

	xt_lock_mutex(self, &db->db_ot_pool.opt_lock);
	pushr_(xt_unlock_mutex, &db->db_ot_pool.opt_lock);

	/* Wait for other to close: */
	while (table_pool->opt_total_open > 0) {
		xt_timed_wait_cond(self, &db->db_ot_pool.opt_cond, &db->db_ot_pool.opt_lock, 2000);
	}

	freer_(); // xt_unlock_mutex(db_ot_pool.opt_lock)
}

xtPublic void xt_db_unlock_table_pool(XTThreadPtr self, XTOpenTablePoolPtr table_pool)
{
	XTDatabaseHPtr db;

	if (!table_pool)
		return;

	db = table_pool->opt_db;
	xt_lock_mutex(self, &db->db_ot_pool.opt_lock);
	pushr_(xt_unlock_mutex, &db->db_ot_pool.opt_lock);

	table_pool->opt_locked = FALSE;
	xt_broadcast_cond(self, &db->db_ot_pool.opt_cond);
	db_free_open_table_pool(NULL, table_pool);

	freer_(); // xt_unlock_mutex(db_ot_pool.opt_lock)
}

xtPublic XTOpenTablePtr xt_db_open_table_using_tab(XTTableHPtr tab, XTThreadPtr thread)
{
	XTDatabaseHPtr		db = tab->tab_db;
	XTOpenTablePoolPtr	table_pool;
	XTOpenTablePtr		ot;

	xt_lock_mutex_ns(&db->db_ot_pool.opt_lock);

	if (!(table_pool = db_get_open_table_pool(db, tab->tab_id)))
		goto failed;

	while (table_pool->opt_locked) {
		if (!xt_timed_wait_cond_ns(&db->db_ot_pool.opt_cond, &db->db_ot_pool.opt_lock, 2000))
			goto failed_1;
		if (!(table_pool = db_get_open_table_pool(db, tab->tab_id)))
			goto failed;
	}

	if ((ot = table_pool->opt_free_list)) {
		/* Remove from the free list: */
		table_pool->opt_free_list = ot->ot_otp_next_free;
		
		/* Remove from MRU list: */
		if (db->db_ot_pool.otp_lr_used == ot)
			db->db_ot_pool.otp_lr_used = ot->ot_otp_mr_used;
		if (db->db_ot_pool.otp_mr_used == ot)
			db->db_ot_pool.otp_mr_used = ot->ot_otp_lr_used;
		if (ot->ot_otp_lr_used)
			ot->ot_otp_lr_used->ot_otp_mr_used = ot->ot_otp_mr_used;
		if (ot->ot_otp_mr_used)
			ot->ot_otp_mr_used->ot_otp_lr_used = ot->ot_otp_lr_used;

		if (db->db_ot_pool.otp_lr_used)
			db->db_ot_pool.otp_free_time = db->db_ot_pool.otp_lr_used->ot_otp_free_time;

		ASSERT_NS(db->db_ot_pool.otp_total_free > 0);
		db->db_ot_pool.otp_total_free--;

		ot->ot_thread = thread;
		goto done_ok;
	}

	if ((ot = xt_open_table(tab))) {
		ot->ot_thread = thread;
		table_pool->opt_total_open++;
	}

	done_ok:
	db_free_open_table_pool(NULL, table_pool);
	xt_unlock_mutex_ns(&db->db_ot_pool.opt_lock);
	return ot;

	failed_1:
	db_free_open_table_pool(NULL, table_pool);

	failed:
	xt_unlock_mutex_ns(&db->db_ot_pool.opt_lock);
	return NULL;
}

xtPublic xtBool xt_db_open_pool_table_ns(XTOpenTablePtr *ret_ot, XTDatabaseHPtr db, xtTableID tab_id)
{
	XTThreadPtr	self = xt_get_self();
	xtBool		ok = TRUE;

	try_(a) {
		*ret_ot = xt_db_open_pool_table(self, db, tab_id, NULL, FALSE);
	}
	catch_(a) {
		ok = FALSE;
	}
	cont_(a);
	return ok;
}

xtPublic XTOpenTablePtr xt_db_open_pool_table(XTThreadPtr self, XTDatabaseHPtr db, xtTableID tab_id, int *result, xtBool i_am_background)
{
	XTOpenTablePtr		ot;
	XTOpenTablePoolPtr	table_pool;
	int					r;
	XTTableHPtr			tab;

	xt_lock_mutex(self, &db->db_ot_pool.opt_lock);
	pushr_(xt_unlock_mutex, &db->db_ot_pool.opt_lock);

	if (!(table_pool = db_get_open_table_pool(db, tab_id)))
		xt_throw(self);

	/* Background processes do not have to wait while flushing!
	 *
	 * I think I did this so that the background process would
	 * not hang during flushing. Exact reason currently
	 * unknown.
	 *
	 * This led to the situation that the checkpointer
	 * could flush at the same time as a user process
	 * which was flushing due to a rename.
	 *
	 * This led to the situation described here: [*10*],
	 * which is now fixed.
	 */
	while (table_pool->opt_locked && !(i_am_background && table_pool->opt_flushing)) {
		xt_timed_wait_cond(self, &db->db_ot_pool.opt_cond, &db->db_ot_pool.opt_lock, 2000);
		if (!(table_pool = db_get_open_table_pool(db, tab_id)))
			xt_throw(self);
	}

	/* Moved from above, because db_get_open_table_pool() may return a different
	 * pool on each call!
	*/
	pushr_(db_free_open_table_pool, table_pool);	
	
	if ((ot = table_pool->opt_free_list)) {
		/* Remove from the free list: */
		table_pool->opt_free_list = ot->ot_otp_next_free;
		
		/* Remove from MRU list: */
		if (db->db_ot_pool.otp_lr_used == ot)
			db->db_ot_pool.otp_lr_used = ot->ot_otp_mr_used;
		if (db->db_ot_pool.otp_mr_used == ot)
			db->db_ot_pool.otp_mr_used = ot->ot_otp_lr_used;
		if (ot->ot_otp_lr_used)
			ot->ot_otp_lr_used->ot_otp_mr_used = ot->ot_otp_mr_used;
		if (ot->ot_otp_mr_used)
			ot->ot_otp_mr_used->ot_otp_lr_used = ot->ot_otp_lr_used;

		if (db->db_ot_pool.otp_lr_used)
			db->db_ot_pool.otp_free_time = db->db_ot_pool.otp_lr_used->ot_otp_free_time;

		ASSERT(db->db_ot_pool.otp_total_free > 0);
		db->db_ot_pool.otp_total_free--;

		freer_(); // db_free_open_table_pool(table_pool)
		freer_(); // xt_unlock_mutex(&db->db_ot_pool.opt_lock)
		ot->ot_thread = self;
		return ot;
	}

	r = xt_use_table_by_id(self, &tab, db, tab_id);
	if (result) {
		if (r != XT_TAB_OK) {
			*result = r;
			freer_(); // db_free_open_table_pool(table_pool)
			freer_(); // xt_unlock_mutex(&db->db_ot_pool.opt_lock)
			return NULL;
		}
	}
	else {
		switch (r) {
			case XT_TAB_NOT_FOUND:
				/* The table no longer exists, ignore the change: */
				freer_(); // db_free_open_table_pool(table_pool)
				freer_(); // xt_unlock_mutex(&db->db_ot_pool.opt_lock)
				return NULL;
			case XT_TAB_NO_DICTIONARY:
				xt_throw_ulxterr(XT_CONTEXT, XT_ERR_NO_DICTIONARY, (u_long) tab_id);
			case XT_TAB_POOL_CLOSED:
				xt_throw_ulxterr(XT_CONTEXT, XT_ERR_TABLE_LOCKED, (u_long) tab_id);
			default:
				break;
		}
	}

	/* xt_use_table_by_id returns a referenced tab! */
	pushr_(xt_heap_release, tab);
	if ((ot = xt_open_table(tab))) {
		ot->ot_thread = self;
		table_pool->opt_total_open++;
	}
	freer_(); // xt_release_heap(tab)

	freer_(); // db_free_open_table_pool(table_pool)
	freer_(); // xt_unlock_mutex(&db->db_ot_pool.opt_lock)
	return ot;
}

xtPublic void xt_db_return_table_to_pool(XTThreadPtr XT_UNUSED(self), XTOpenTablePtr ot)
{
	xt_db_return_table_to_pool_ns(ot);
}

xtPublic void xt_db_return_table_to_pool_ns(XTOpenTablePtr ot)
{
	XTOpenTablePoolPtr	table_pool;
	XTDatabaseHPtr		db = ot->ot_table->tab_db;
	xtBool				flush_table = TRUE;

	/* No open table returned to the pool should still
	 * have a cache handle!
	 */
	ASSERT_NS(!ot->ot_ind_rhandle);
	xt_lock_mutex_ns(&db->db_ot_pool.opt_lock);

	if (!(table_pool = db_get_open_table_pool(db, ot->ot_table->tab_id)))
		goto failed;

	if (table_pool->opt_locked && !table_pool->opt_flushing) {
		/* Table will be closed below: */
		if (table_pool->opt_total_open > 1)
			flush_table = FALSE;
	}
	else {
		/* Put it on the free list: */
		db->db_ot_pool.otp_total_free++;

		ot->ot_otp_next_free = table_pool->opt_free_list;
		table_pool->opt_free_list = ot;

		/* This is the time the table was freed: */
		ot->ot_otp_free_time = xt_db_approximate_time;

		/* Add to most recently used: */
		if ((ot->ot_otp_lr_used = db->db_ot_pool.otp_mr_used))
			db->db_ot_pool.otp_mr_used->ot_otp_mr_used = ot;
		ot->ot_otp_mr_used = NULL;
		db->db_ot_pool.otp_mr_used = ot;
		if (!db->db_ot_pool.otp_lr_used) {
			db->db_ot_pool.otp_lr_used = ot;
			db->db_ot_pool.otp_free_time = ot->ot_otp_free_time;
		}

		ot = NULL;
	}

	if (ot) {
		xt_unlock_mutex_ns(&db->db_ot_pool.opt_lock);
		xt_close_table(ot, flush_table, FALSE);

		/* assume that table_pool cannot be invalidated in between as we have table_pool->opt_total_open > 0 */
		xt_lock_mutex_ns(&db->db_ot_pool.opt_lock);
		table_pool->opt_total_open--;
	}

	db_free_open_table_pool(NULL, table_pool);

	if (!xt_broadcast_cond_ns(&db->db_ot_pool.opt_cond))
		goto failed;
	xt_unlock_mutex_ns(&db->db_ot_pool.opt_lock);
	
	return;

	failed:
	xt_unlock_mutex_ns(&db->db_ot_pool.opt_lock);
	if (ot)
		xt_close_table(ot, TRUE, FALSE);
	xt_log_and_clear_exception_ns();
}

//#define TEST_FREE_OPEN_TABLES

#ifdef DEBUG
#undef XT_OPEN_TABLE_FREE_TIME
#define XT_OPEN_TABLE_FREE_TIME			5
#endif

xtPublic void xt_db_free_unused_open_tables(XTThreadPtr self, XTDatabaseHPtr db)
{
	XTOpenTablePoolPtr	table_pool;
	size_t				count;
	XTOpenTablePtr		ot;
	xtBool				flush_table = TRUE;
	u_int				table_count;

	/* A quick check of the oldest free table: */
	if (xt_db_approximate_time < db->db_ot_pool.otp_free_time + XT_OPEN_TABLE_FREE_TIME)
		return;

	table_count = db->db_table_by_id ? xt_sl_get_size(db->db_table_by_id) : 0;
	count = table_count * 3;
	if (count < 20)
		count = 20;
#ifdef TEST_FREE_OPEN_TABLES
	count = 10;
#endif
	if (db->db_ot_pool.otp_total_free > count) {
		XTOpenTablePtr	ptr, pptr;

		count = table_count * 2;
		if (count < 10)
			count = 10;
#ifdef TEST_FREE_OPEN_TABLES
		count = 5;
#endif
		xt_lock_mutex(self, &db->db_ot_pool.opt_lock);
		pushr_(xt_unlock_mutex, &db->db_ot_pool.opt_lock);

		while (db->db_ot_pool.otp_total_free > count) {
			ASSERT_NS(db->db_ot_pool.otp_lr_used);
			if (!(ot = db->db_ot_pool.otp_lr_used))
				break;

			/* Check how long the open table has been free: */
			if (xt_db_approximate_time < ot->ot_otp_free_time + XT_OPEN_TABLE_FREE_TIME)
				break;

			ot->ot_thread = self;

			/* Remove from MRU list: */
			db->db_ot_pool.otp_lr_used = ot->ot_otp_mr_used;
			if (db->db_ot_pool.otp_mr_used == ot)
				db->db_ot_pool.otp_mr_used = ot->ot_otp_lr_used;
			if (ot->ot_otp_lr_used)
				ot->ot_otp_lr_used->ot_otp_mr_used = ot->ot_otp_mr_used;
			if (ot->ot_otp_mr_used)
				ot->ot_otp_mr_used->ot_otp_lr_used = ot->ot_otp_lr_used;

			if (db->db_ot_pool.otp_lr_used)
				db->db_ot_pool.otp_free_time = db->db_ot_pool.otp_lr_used->ot_otp_free_time;

			ASSERT(db->db_ot_pool.otp_total_free > 0);
			db->db_ot_pool.otp_total_free--;

			if (!(table_pool = db_get_open_table_pool(db, ot->ot_table->tab_id)))
				xt_throw(self);

			/* Find the open table in the table pool,
			 * and remove it from the list:
			 */
			pptr = NULL;
			ptr = table_pool->opt_free_list;
			while (ptr) {
				if (ptr == ot)
					break;
				pptr = ptr;
				ptr = ptr->ot_otp_next_free;
			}

			ASSERT_NS(ptr == ot);
			if (ptr == ot) {
				if (pptr)
					pptr->ot_otp_next_free = ot->ot_otp_next_free;
				else
					table_pool->opt_free_list = ot->ot_otp_next_free;
			}

			ASSERT_NS(table_pool->opt_total_open > 0);
			table_pool->opt_total_open--;
			if (table_pool->opt_total_open > 0)
				flush_table = FALSE;
			else
				flush_table = TRUE;

			db_free_open_table_pool(self, table_pool);

			freer_();

			/* Close the table, but not
			 * while holding the lock.
			 */
			xt_close_table(ot, flush_table, FALSE);

			xt_lock_mutex(self, &db->db_ot_pool.opt_lock);
			pushr_(xt_unlock_mutex, &db->db_ot_pool.opt_lock);
		}

		freer_();
	}
}
