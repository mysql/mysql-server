/* Copyright (c) 2005 PrimeBase Technologies GmbH
 *
 * Derived from ha_example.h
 * Copyright (C) 2003 MySQL AB
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA	02111-1307	USA
 *
 * 2005-11-10	Paul McCullagh
 *
 */

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif

#include "xt_config.h"

#if defined(XT_WIN)
#include <windows.h>
#endif

#include <stdlib.h>
#include <time.h>

#ifdef DRIZZLED
#include <drizzled/common.h>
#include <drizzled/plugin.h>
#include <mysys/my_alloc.h>
#include <mysys/hash.h>
#include <drizzled/field.h>
#include <drizzled/current_session.h>
#include <drizzled/data_home.h>
#include <drizzled/error.h>
#include <drizzled/table.h>
#include <drizzled/field/timestamp.h>
#include <drizzled/server_includes.h>
extern "C" char **session_query(Session *session);
#define my_strdup(a,b) strdup(a)
#else
#include "mysql_priv.h"
#include <mysql/plugin.h>
#endif

#include "ha_pbxt.h"
#include "ha_xtsys.h"

#include "strutil_xt.h"
#include "database_xt.h"
#include "cache_xt.h"
#include "trace_xt.h"
#include "heap_xt.h"
#include "myxt_xt.h"
#include "datadic_xt.h"
#ifdef PBMS_ENABLED
#include "pbms_enabled.h"
#endif
#include "tabcache_xt.h"
#include "systab_xt.h"
#include "xaction_xt.h"

#ifdef DEBUG
//#define XT_USE_SYS_PAR_DEBUG_SIZES
//#define PBXT_HANDLER_TRACE
//#define PBXT_TRACE_RETURN
//#define XT_PRINT_INDEX_OPT
//#define XT_SHOW_DUMPS_TRACE
//#define XT_UNIT_TEST
//#define LOAD_TABLE_ON_OPEN
//#define CHECK_TABLE_LOADS

/* Enable to trace the statements executed by the engine: */
//#define TRACE_STATEMENTS

/* Enable to print the trace to the stdout, instead of
 * to the trace log.
 */
//#define PRINT_STATEMENTS
#endif

#ifndef DRIZZLED
static handler	*pbxt_create_handler(handlerton *hton, TABLE_SHARE *table, MEM_ROOT *mem_root);
static int		pbxt_init(void *p);
static int		pbxt_end(void *p);
static int		pbxt_panic(handlerton *hton, enum ha_panic_function flag);
static void		pbxt_drop_database(handlerton *hton, char *path);
static int		pbxt_close_connection(handlerton *hton, THD* thd);
static int		pbxt_commit(handlerton *hton, THD *thd, bool all);
static int		pbxt_rollback(handlerton *hton, THD *thd, bool all);
#endif
static void		ha_aquire_exclusive_use(XTThreadPtr self, XTSharePtr share, ha_pbxt *mine);
static void		ha_release_exclusive_use(XTThreadPtr self, XTSharePtr share);
static void		ha_close_open_tables(XTThreadPtr self, XTSharePtr share, ha_pbxt *mine);

extern void		xt_xres_start_database_recovery(XTThreadPtr self);

#ifdef TRACE_STATEMENTS

#ifdef PRINT_STATEMENTS
#define STAT_TRACE(y, x)		printf("%s: %s\n", y ? y->t_name : "-unknown-", x)
#else
#define STAT_TRACE(y, x)		xt_ttraceq(y, x)
#endif

#else

#define STAT_TRACE(y, x)

#endif

#ifdef PBXT_HANDLER_TRACE
#define PBXT_ALLOW_PRINTING

#define XT_TRACE_CALL()				do { XTThreadPtr s = xt_get_self(); printf("%s %s\n", s ? s->t_name : "-unknown-", __FUNC__); } while (0)
#ifdef PBXT_TRACE_RETURN
#define XT_RETURN(x)				do { printf("%d\n", (int) (x)); return (x); } while (0)
#define XT_RETURN_VOID				do { printf("out\n"); return; } while (0)
#else
#define XT_RETURN(x)				return (x)
#define XT_RETURN_VOID				return
#endif

#else

#define XT_TRACE_CALL()
#define XT_RETURN(x)				return (x)
#define XT_RETURN_VOID				return

#endif

#ifdef PBXT_ALLOW_PRINTING
#define XT_PRINT0(y, x)				do { XTThreadPtr s = (y); printf("%s " x, s ? s->t_name : "-unknown-"); } while (0)
#define XT_PRINT1(y, x, a)			do { XTThreadPtr s = (y); printf("%s " x, s ? s->t_name : "-unknown-", a); } while (0)
#define XT_PRINT2(y, x, a, b)		do { XTThreadPtr s = (y); printf("%s " x, s ? s->t_name : "-unknown-", a, b); } while (0)
#define XT_PRINT3(y, x, a, b, c)	do { XTThreadPtr s = (y); printf("%s " x, s ? s->t_name : "-unknown-", a, b, c); } while (0)
#else
#define XT_PRINT0(y, x)
#define XT_PRINT1(y, x, a)
#define XT_PRINT2(y, x, a, b)
#define XT_PRINT3(y, x, a, b, c)
#endif


#define TS(x)					(x)->s

handlerton				*pbxt_hton;
bool					pbxt_inited = false;		// Variable for checking the init state of hash
xtBool					pbxt_ignore_case = true;
const char				*pbxt_extensions[]= { ".xtr", ".xtd", ".xtl", ".xti", ".xt", "", NULL };
#ifdef XT_CRASH_DEBUG
xtBool					pbxt_crash_debug = TRUE;
#else
xtBool					pbxt_crash_debug = FALSE;
#endif

/* Variables for pbxt share methods */
static xt_mutex_type	pbxt_database_mutex;		// Prevent a database from being opened while it is being dropped
static XTHashTabPtr		pbxt_share_tables;			// Hash used to track open tables
XTDatabaseHPtr			pbxt_database = NULL;		// The global open database
static char				*pbxt_index_cache_size;
static char				*pbxt_record_cache_size;
static char				*pbxt_log_cache_size;
static char				*pbxt_log_file_threshold;
static char				*pbxt_transaction_buffer_size;
static char				*pbxt_log_buffer_size;
static char				*pbxt_checkpoint_frequency;
static char				*pbxt_data_log_threshold;
static char				*pbxt_data_file_grow_size;
static char				*pbxt_row_file_grow_size;
static int				pbxt_max_threads;

#ifdef DEBUG
#define XT_SHARE_LOCK_WAIT		5000
#else
#define XT_SHARE_LOCK_WAIT		500
#endif

/* 
 * Lock timeout in 1/1000ths of a second
 */
#define XT_SHARE_LOCK_TIMEOUT	30000

/*
 * -----------------------------------------------------------------------
 * SYSTEM VARIABLES
 *
 */
 
//#define XT_FOR_TEAMDRIVE

typedef struct HAVarParams {
	const char		*vp_var;						/* Variable name. */
	const char		*vp_def;						/* Default value. */
	const char		*vp_min;						/* Minimum allowed value. */
	const char		*vp_max4;						/* Maximum allowed value on 32-bit processors. */
	const char		*vp_max8;						/* Maximum allowed value on 64-bit processors. */
} HAVarParamsRec, *HAVarParamsPtr;

#ifdef XT_USE_SYS_PAR_DEBUG_SIZES
static HAVarParamsRec vp_index_cache_size = { "pbxt_index_cache_size", "32MB", "8MB", "2GB", "2000GB" };
static HAVarParamsRec vp_record_cache_size = { "pbxt_record_cache_size", "32MB", "8MB", "2GB", "2000GB" };
static HAVarParamsRec vp_log_cache_size = { "pbxt_log_cache_size", "16MB", "4MB", "2GB", "2000GB" };
static HAVarParamsRec vp_checkpoint_frequency = { "pbxt_checkpoint_frequency", "28MB", "512K", "1GB", "24GB" };
static HAVarParamsRec vp_log_file_threshold = { "pbxt_log_file_threshold", "32MB", "1MB", "2GB", "256TB" };
static HAVarParamsRec vp_transaction_buffer_size = { "pbxt_transaction_buffer_size", "1MB", "128K", "1GB", "24GB" };
static HAVarParamsRec vp_log_buffer_size = { "pbxt_log_buffer_size", "256K", "128K", "1GB", "24GB" };
static HAVarParamsRec vp_data_log_threshold = { "pbxt_data_log_threshold", "400K", "400K", "2GB", "256TB" };
static HAVarParamsRec vp_data_file_grow_size = { "pbxt_data_file_grow_size", "2MB", "128K", "1GB", "2GB" };
static HAVarParamsRec vp_row_file_grow_size = { "pbxt_row_file_grow_size", "256K", "32K", "1GB", "2GB" };
#define XT_DL_DEFAULT_XLOG_COUNT		3
#define XT_DL_DEFAULT_GARBAGE_LEVEL		10
#else
static HAVarParamsRec vp_index_cache_size = { "pbxt_index_cache_size", "32MB", "8MB", "2GB", "2000GB" };
static HAVarParamsRec vp_record_cache_size = { "pbxt_record_cache_size", "32MB", "8MB", "2GB", "2000GB" };
static HAVarParamsRec vp_log_cache_size = { "pbxt_log_cache_size", "16MB", "4MB", "2GB", "2000GB" };
static HAVarParamsRec vp_checkpoint_frequency = { "pbxt_checkpoint_frequency", "28MB", "512K", "1GB", "24GB" };
static HAVarParamsRec vp_log_file_threshold = { "pbxt_log_file_threshold", "32MB", "1MB", "2GB", "256TB" };
static HAVarParamsRec vp_transaction_buffer_size = { "pbxt_transaction_buffer_size", "1MB", "128K", "1GB", "24GB" };
static HAVarParamsRec vp_log_buffer_size = { "pbxt_log_buffer_size", "256K", "128K", "1GB", "24GB" };
static HAVarParamsRec vp_data_log_threshold = { "pbxt_data_log_threshold", "64MB", "1MB", "2GB", "256TB" };
static HAVarParamsRec vp_data_file_grow_size = { "pbxt_data_file_grow_size", "2MB", "128K", "1GB", "2GB" };
static HAVarParamsRec vp_row_file_grow_size = { "pbxt_row_file_grow_size", "256K", "32K", "1GB", "2GB" };
#define XT_DL_DEFAULT_XLOG_COUNT		3
#define XT_DL_DEFAULT_GARBAGE_LEVEL		50
#endif

#define XT_AUTO_INCREMENT_DEF			0

#ifdef XT_MAC
#ifdef DEBUG
/* For debugging on the Mac, we check the re-use logs: */
#define XT_OFFLINE_LOG_FUNCTION_DEF		XT_RECYCLE_LOGS
#else
#define XT_OFFLINE_LOG_FUNCTION_DEF		XT_DELETE_LOGS
#endif
#else
#define XT_OFFLINE_LOG_FUNCTION_DEF		XT_RECYCLE_LOGS
#endif

/* TeamDrive, uses special auto-increment, and
 * we keep the logs for the moment:
 */
#ifdef XT_FOR_TEAMDRIVE
#undef XT_OFFLINE_LOG_FUNCTION_DEF
#define XT_OFFLINE_LOG_FUNCTION_DEF		XT_KEEP_LOGS
//#undef XT_AUTO_INCREMENT_DEF
//#define XT_AUTO_INCREMENT_DEF			1
#endif

/*
 * -----------------------------------------------------------------------
 * SHARED TABLE DATA
 *
 */

static xtBool ha_hash_comp(void *key, void *data)
{
	XTSharePtr	share = (XTSharePtr) data;

	return strcmp((char *) key, share->sh_table_path->ps_path) == 0;
}

static xtHashValue ha_hash(xtBool is_key, void *key_data)
{
	XTSharePtr	share = (XTSharePtr) key_data;

	if (is_key)
		return xt_ht_hash((char *) key_data);
	return xt_ht_hash(share->sh_table_path->ps_path);
}

static xtBool ha_hash_comp_ci(void *key, void *data)
{
	XTSharePtr	share = (XTSharePtr) data;

	return strcasecmp((char *) key, share->sh_table_path->ps_path) == 0;
}

static xtHashValue ha_hash_ci(xtBool is_key, void *key_data)
{
	XTSharePtr	share = (XTSharePtr) key_data;

	if (is_key)
		return xt_ht_casehash((char *) key_data);
	return xt_ht_casehash(share->sh_table_path->ps_path);
}

static void ha_open_share(XTThreadPtr self, XTShareRec *share, xtBool *tabled_opened)
{
	xt_lock_mutex(self, (xt_mutex_type *) share->sh_ex_mutex);
	pushr_(xt_unlock_mutex, share->sh_ex_mutex);

	if (!share->sh_table) {
		share->sh_table = xt_use_table(self, share->sh_table_path, FALSE, FALSE, tabled_opened);
		share->sh_dic_key_count = share->sh_table->tab_dic.dic_key_count;
		share->sh_dic_keys = share->sh_table->tab_dic.dic_keys;
		share->sh_recalc_selectivity = FALSE;
	}

	freer_(); // xt_ht_unlock(pbxt_share_tables)
}

static void ha_close_share(XTThreadPtr self, XTShareRec *share)
{
	XTTableHPtr tab;

	if ((tab = share->sh_table)) {
		/* Save this, in case the share is re-opened. */
		share->sh_min_auto_inc = tab->tab_auto_inc;

		xt_heap_release(self, tab);
		share->sh_table = NULL;
	}

	/* This are only references: */
	share->sh_dic_key_count = 0;
	share->sh_dic_keys = NULL;
}

static void ha_cleanup_share(XTThreadPtr self, XTSharePtr share)
{
	ha_close_share(self, share);

	if (share->sh_table_path) {
		xt_free(self, share->sh_table_path);
		share->sh_table_path = NULL;
	}

	if (share->sh_ex_cond) {
		thr_lock_delete(&share->sh_lock);
		xt_delete_cond(self, (xt_cond_type *) share->sh_ex_cond);
		share->sh_ex_cond = NULL;
	}

	if (share->sh_ex_mutex) {
		xt_delete_mutex(self, (xt_mutex_type *) share->sh_ex_mutex);
		share->sh_ex_mutex = NULL;
	}

	xt_free(self, share);
}

static void ha_hash_free(XTThreadPtr self, void *data)
{
	XTSharePtr	share = (XTSharePtr) data;

	ha_cleanup_share(self, share);
}

/*
 * This structure contains information that is common to all handles.
 * (i.e. it is table specific).
 */
static XTSharePtr ha_get_share(XTThreadPtr self, const char *table_path, bool open_table, xtBool *tabled_opened)
{
	XTShareRec	*share;

	enter_();
	xt_ht_lock(self, pbxt_share_tables);
	pushr_(xt_ht_unlock, pbxt_share_tables);

	// Check if the table exists...
	if (!(share = (XTSharePtr) xt_ht_get(self, pbxt_share_tables, (void *) table_path))) {
		share = (XTSharePtr) xt_calloc(self, sizeof(XTShareRec));		
		pushr_(ha_cleanup_share, share);

		share->sh_ex_mutex = (xt_mutex_type *) xt_new_mutex(self);
		share->sh_ex_cond = (xt_cond_type *) xt_new_cond(self);

		thr_lock_init(&share->sh_lock);

		share->sh_use_count = 0;
		share->sh_table_path = (XTPathStrPtr) xt_dup_string(self, table_path);

		if (open_table)
			ha_open_share(self, share, tabled_opened);

		popr_(); // Discard ha_cleanup_share(share);

		xt_ht_put(self, pbxt_share_tables, share);
	}

	share->sh_use_count++;
	freer_(); // xt_ht_unlock(pbxt_share_tables)

	return_(share);
}

/*
 * Free shared information.
 */
static void ha_unget_share(XTThreadPtr self, XTSharePtr share)
{
	xt_ht_lock(self, pbxt_share_tables);
	pushr_(xt_ht_unlock, pbxt_share_tables);

	if (!--share->sh_use_count)
		xt_ht_del(self, pbxt_share_tables, share->sh_table_path);

	freer_(); // xt_ht_unlock(pbxt_share_tables)
}

static xtBool ha_unget_share_removed(XTThreadPtr self, XTSharePtr share)
{
	xtBool removed = FALSE;

	xt_ht_lock(self, pbxt_share_tables);
	pushr_(xt_ht_unlock, pbxt_share_tables);

	if (!--share->sh_use_count) {
		removed = TRUE;
		xt_ht_del(self, pbxt_share_tables, share->sh_table_path);
	}

	freer_(); // xt_ht_unlock(pbxt_share_tables)
	return removed;
}

/*
 * -----------------------------------------------------------------------
 * PUBLIC FUNCTIONS
 *
 */

xtPublic void xt_ha_unlock_table(XTThreadPtr self, void *share)
{
	ha_release_exclusive_use(self, (XTSharePtr) share);
	ha_unget_share(self, (XTSharePtr) share);
}

xtPublic void xt_ha_close_global_database(XTThreadPtr self)
{
	if (pbxt_database) {
		xt_heap_release(self, pbxt_database);
		pbxt_database = NULL;
	}
}

/*
 * Open a PBXT database given the path of a table.
 * This function also returns the name of the table.
 *
 * We use the pbxt_database_mutex to lock this
 * operation to make sure it does not occur while
 * some other thread is doing a "closeall".
 */
xtPublic void xt_ha_open_database_of_table(XTThreadPtr self, XTPathStrPtr XT_UNUSED(table_path))
{
#ifdef XT_USE_GLOBAL_DB
	if (!self->st_database) {
		if (!pbxt_database) {
			xt_open_database(self, mysql_real_data_home, TRUE);
			pbxt_database = self->st_database;
			xt_heap_reference(self, pbxt_database);
		}
		else
			xt_use_database(self, pbxt_database, XT_FOR_USER);
	}
#else
	char db_path[PATH_MAX];

	xt_strcpy(PATH_MAX, db_path, (char *) table_path);
	xt_remove_last_name_of_path(db_path);
	xt_remove_dir_char(db_path);

	if (self->st_database && xt_tab_compare_paths(self->st_database->db_name, xt_last_name_of_path(db_path)) == 0)
		/* This thread already has this database open! */
		return;

	/* Auto commit before changing the database: */
	if (self->st_xact_data) {
		/* PMC - This probably indicates something strange is happening:
		 *
		 * This sequence generates this error:
		 *
		 * delimiter |
		 * 
		 * create temporary table t3 (id int)|
		 * 
		 * create function f10() returns int
		 * begin
		 *   drop temporary table if exists t3;
		 *   create temporary table t3 (id int) engine=myisam;
		 *   insert into t3 select id from t4;
		 *   return (select count(*) from t3);
		 * end|
		 * 
		 * select f10()|
		 *
		 * An error is generated because the same thread is used
		 * to open table t4 (at the start of the functions), and
		 * then to drop table t3. To drop t3 we need to
		 * switch the database, so we land up here!
		 */
		xt_throw_xterr(XT_CONTEXT, XT_ERR_CANNOT_CHANGE_DB);
		/*
		 if (!xt_xn_commit(self))
		 	throw_();
		 */
	}

	xt_lock_mutex(self, &pbxt_database_mutex);
	pushr_(xt_unlock_mutex, &pbxt_database_mutex);
	xt_open_database(self, db_path, FALSE);
	freer_(); // xt_unlock_mutex(&pbxt_database_mutex);
#endif
}

xtPublic XTThreadPtr xt_ha_set_current_thread(THD *thd, XTExceptionPtr e)
{
	XTThreadPtr	self;
	static int	ha_thread_count = 0, ha_id;

	if (!(self = (XTThreadPtr) *thd_ha_data(thd, pbxt_hton))) {
//		const			Security_context *sctx;
		char			name[120];
		char			ha_id_str[50];

		ha_id = ++ha_thread_count;
		sprintf(ha_id_str, "_%d", ha_id);
		xt_strcpy(120,name,"user"); // TODO: Fix this hack
/*
		sctx = &thd->main_security_ctx;

		if (sctx->user) {
			xt_strcpy(120, name, sctx->user);
			xt_strcat(120, name, "@");
		}
		else
			*name = 0;
		if (sctx->host)
			xt_strcat(120, name, sctx->host);
		else if (sctx->ip)
			xt_strcat(120, name, sctx->ip);
		else if (thd->proc_info)
			xt_strcat(120, name, (char *) thd->proc_info);
		else
			xt_strcat(120, name, "system");
*/
		xt_strcat(120, name, ha_id_str);
		if (!(self = xt_create_thread(name, FALSE, TRUE, e)))
			return NULL;

		self->st_xact_mode = XT_XACT_REPEATABLE_READ;

		*thd_ha_data(thd, pbxt_hton) = (void *) self;
	}
	return self;
}

xtPublic void xt_ha_close_connection(THD* thd)
{
	XTThreadPtr		self;

	if ((self = (XTThreadPtr) *thd_ha_data(thd, pbxt_hton))) {
		*thd_ha_data(thd, pbxt_hton) = NULL;
		xt_free_thread(self);
	}
}

xtPublic XTThreadPtr xt_ha_thd_to_self(THD *thd)
{
	return (XTThreadPtr) *thd_ha_data(thd, pbxt_hton);
}

/* The first bit is 1. */
static u_int ha_get_max_bit(MX_BITMAP *map)
{
	my_bitmap_map	*data_ptr = map->bitmap;
	my_bitmap_map	*end_ptr = map->last_word_ptr;
	my_bitmap_map	b;
	u_int			cnt = map->n_bits;

	for (; end_ptr >= data_ptr; end_ptr--) {
		if ((b = *end_ptr)) {
			my_bitmap_map mask;
			
			if (end_ptr == map->last_word_ptr && map->last_word_mask)
				mask = map->last_word_mask >> 1;
			else
				mask = 0x80000000;
			while (!(b & mask)) {
				b = b << 1;
				/* Should not happen, but if it does, we hang! */
				if (!b)
					return map->n_bits;
				cnt--;
			}
			return cnt;
		}
		if (end_ptr == map->last_word_ptr)
			cnt = ((cnt-1) / 32) * 32;
		else
			cnt -= 32;
	}
	return 0;
}

/*
 * -----------------------------------------------------------------------
 * SUPPORT FUNCTIONS
 *
 */

/*
 * In PBXT, as in MySQL: thread == connection.
 *
 * So we simply attach a PBXT thread to a MySQL thread.
 */
static XTThreadPtr ha_set_current_thread(THD *thd, int *err)
{
	XTThreadPtr		self;
	XTExceptionRec	e;

	if (!(self = xt_ha_set_current_thread(thd, &e))) {
		xt_log_exception(NULL, &e, XT_LOG_DEFAULT);
		*err = e.e_xt_err;
		return NULL;
	}
	return self;
}

xtPublic int xt_ha_pbxt_to_mysql_error(int xt_err)
{
	switch (xt_err) {
		case XT_NO_ERR:
			return(0);
		case XT_ERR_DUPLICATE_KEY:
				return HA_ERR_FOUND_DUPP_KEY;
		case XT_ERR_DEADLOCK:
				return HA_ERR_LOCK_DEADLOCK;
		case XT_ERR_RECORD_CHANGED:
			/* If we generate HA_ERR_RECORD_CHANGED instead of HA_ERR_LOCK_WAIT_TIMEOUT
			 * then sysbench does not work because it does not handle this error.
			 */
			//return HA_ERR_LOCK_WAIT_TIMEOUT; // but HA_ERR_RECORD_CHANGED is the correct error for a optimistic lock failure.
			return HA_ERR_RECORD_CHANGED;
		case XT_ERR_LOCK_TIMEOUT:
			return HA_ERR_LOCK_WAIT_TIMEOUT;
		case XT_ERR_TABLE_IN_USE:
				return HA_ERR_WRONG_COMMAND;
		case XT_ERR_TABLE_NOT_FOUND:
			return HA_ERR_NO_SUCH_TABLE;
		case XT_ERR_TABLE_EXISTS:
			return HA_ERR_TABLE_EXIST;
		case XT_ERR_CANNOT_CHANGE_DB:
			return ER_TRG_IN_WRONG_SCHEMA;
		case XT_ERR_COLUMN_NOT_FOUND:
			return HA_ERR_CANNOT_ADD_FOREIGN;
		case XT_ERR_NO_REFERENCED_ROW:
		case XT_ERR_REF_TABLE_NOT_FOUND:
		case XT_ERR_REF_TYPE_WRONG:
			return HA_ERR_NO_REFERENCED_ROW;
		case XT_ERR_ROW_IS_REFERENCED:
			return HA_ERR_ROW_IS_REFERENCED;
		case XT_ERR_COLUMN_IS_NOT_NULL:
		case XT_ERR_INCORRECT_NO_OF_COLS:
		case XT_ERR_FK_ON_TEMP_TABLE:
		case XT_ERR_FK_REF_TEMP_TABLE:
			return HA_ERR_CANNOT_ADD_FOREIGN;
		case XT_ERR_DUPLICATE_FKEY:
			return HA_ERR_FOREIGN_DUPLICATE_KEY;
		case XT_ERR_RECORD_DELETED:
			return HA_ERR_RECORD_DELETED;
	}
	return(-1);			// Unknown error
}

xtPublic int xt_ha_pbxt_thread_error_for_mysql(THD *XT_UNUSED(thd), const XTThreadPtr self, int ignore_dup_key)
{
	int xt_err = self->t_exception.e_xt_err;

	XT_PRINT2(self, "xt_ha_pbxt_thread_error_for_mysql xt_err=%d auto commit=%d\n", (int) xt_err, (int) self->st_auto_commit);
	switch (xt_err) {
		case XT_NO_ERR:
			break;
		case XT_ERR_DUPLICATE_KEY:
		case XT_ERR_DUPLICATE_FKEY:
			/* Let MySQL call rollback as and when it wants to for duplicate
			 * key.
			 *
			 * In addition, we are not allowed to do an auto-rollback
			 * inside a sub-statement (function() or procedure())
			 * For example:
			 * 
			 * delimiter |
			 *
			 * create table t3 (c1 char(1) primary key not null)|
			 * 
			 * create function bug12379()
			 *   returns integer
			 * begin
			 *    insert into t3 values('X');
			 *    insert into t3 values('X');
			 *    return 0;
			 * end|
			 * 
			 * --error 1062
			 * select bug12379()|
			 *
			 *
			 * Not doing an auto-rollback should solve this problem in the
			 * case of duplicate key (but not in others - like deadlock)!
			 * I don't think this situation is handled correctly by MySQL.
			 */

			/* If we are in auto-commit mode (and we are not ignoring
			 * duplicate keys) then rollback the transaction automatically.
			 */
			if (!ignore_dup_key && self->st_auto_commit)
				goto abort_transaction;
			break;
		case XT_ERR_DEADLOCK:
		case XT_ERR_NO_REFERENCED_ROW:
		case XT_ERR_ROW_IS_REFERENCED:
			goto abort_transaction;
		case XT_ERR_RECORD_CHANGED:
			/* MySQL also handles the locked error. NOTE: There is no automatic
			 * rollback!
			 */
			break;
		default:
			xt_log_exception(self, &self->t_exception, XT_LOG_DEFAULT);
			abort_transaction:
			/* PMC 2006-08-30: It should be that this is not necessary!
			 *
			 * It is only necessary to call ha_rollback() if the engine
			 * aborts the transaction.
			 *
			 * On the other hand, I shouldn't need to rollback the
			 * transaction because, if I return an error, MySQL
			 * should do it for me.
			 *
			 * Unfortunately, when auto-commit is off, MySQL does not
			 * rollback automatically (for example when a deadlock
			 * is provoked).
			 *
			 * And when we have a multi update we cannot rely on this
			 * either (see comment above).
			 */
			if (self->st_xact_data) {
				/*
				 * GOTCHA:
				 * A result of the "st_abort_trans = TRUE" below is that
				 * the following code results in an empty set.
				 * The reason is "ignore_dup_key" is not set so
				 * the duplicate key leads to an error which causes
				 * the transaction to be aborted.
				 * The delayed inserts are all execute in one transaction.
				 * 
				 * CREATE TABLE t1 (
				 * c1 INT(11) NOT NULL AUTO_INCREMENT,
				 * c2 INT(11) DEFAULT NULL,
				 * PRIMARY KEY (c1)
				 * );
				 * SET insert_id= 14;
				 * INSERT DELAYED INTO t1 VALUES(NULL, 11), (NULL, 12);
				 * INSERT DELAYED INTO t1 VALUES(14, 91);
				 * INSERT DELAYED INTO t1 VALUES (NULL, 92), (NULL, 93);
				 * FLUSH TABLE t1;
				 * SELECT * FROM t1;
				 */
				if (self->st_lock_count == 0) {
					/* No table locks, must rollback immediately
					 * (there will be no possibility later!
					 */
					XT_PRINT1(self, "xt_xn_rollback xt_err=%d\n", xt_err);
					if (!xt_xn_rollback(self))
						xt_log_exception(self, &self->t_exception, XT_LOG_DEFAULT);
				}
				else {
					/* Locks are held on tables.
					 * Only rollback after locks are released.
					 */
					self->st_auto_commit = TRUE;
					self->st_abort_trans = TRUE;
				}
#ifdef xxxx
/* DBUG_ASSERT(thd->transaction.stmt.ha_list == NULL ||
              trans == &thd->transaction.stmt); in handler.cc now
 * fails, and I don't know if this function can be called anymore! */
				/* Cause any other DBs to do a rollback as well... */
				if (thd) {
					/*
					 * GOTCHA:
					 * This is a BUG in MySQL. I cannot rollback a transaction if
					 * pb_mysql_thd->in_sub_stmt! But I must....?!
					 */
#ifdef MYSQL_SERVER
					if (!thd->in_sub_stmt)
						ha_rollback(thd);
#endif
				}
#endif
			}
			break;
	}
	return xt_ha_pbxt_to_mysql_error(xt_err);
}

static void ha_conditional_close_database(XTThreadPtr self, XTThreadPtr other_thr, void *db)
{
	if (other_thr->st_database == (XTDatabaseHPtr) db)
		xt_unuse_database(self, other_thr);
}

/*
 * This is only called from drop database, so we know that
 * no thread is actually using the database. This means that it
 * must be safe to close the database.
 */
xtPublic void xt_ha_all_threads_close_database(XTThreadPtr self, XTDatabaseHPtr db)
{
	xt_lock_mutex(self, &pbxt_database_mutex);
	pushr_(xt_unlock_mutex, &pbxt_database_mutex);
	xt_do_to_all_threads(self, ha_conditional_close_database, db);
	freer_(); // xt_unlock_mutex(&pbxt_database_mutex);
}

static int ha_log_pbxt_thread_error_for_mysql(int ignore_dup_key)
{
	return xt_ha_pbxt_thread_error_for_mysql(current_thd, myxt_get_self(), ignore_dup_key);
}

/*
 * -----------------------------------------------------------------------
 * STATIC HOOKS
 *
 */
static xtWord8 ha_set_variable(char **value, HAVarParamsPtr vp)
{
	xtWord8	result;
	xtWord8	mi, ma;
	char	*mm;

	if (!*value)
		*value = getenv(vp->vp_var);
	if (!*value)
		*value = (char *) vp->vp_def;
	result = xt_byte_size_to_int8(*value);
	mi = (xtWord8) xt_byte_size_to_int8(vp->vp_min);
	if (result < mi) {
		result = mi;
		*value = (char *) vp->vp_min;
	}
	if (sizeof(size_t) == 8)
		mm = (char *) vp->vp_max8;
	else
		mm = (char *) vp->vp_max4;
	ma = (xtWord8) xt_byte_size_to_int8(mm);
	if (result > ma) {
		result = ma;
		*value = mm;
	}
	return result;
}

static void pbxt_call_init(XTThreadPtr self)
{
	xtInt8	index_cache_size;
	xtInt8	record_cache_size;
	xtInt8	log_cache_size;
	xtInt8	log_file_threshold;
	xtInt8	transaction_buffer_size;
	xtInt8	log_buffer_size;
	xtInt8	checkpoint_frequency;
	xtInt8	data_log_threshold;
	xtInt8	data_file_grow_size;
	xtInt8	row_file_grow_size;

	xt_logf(XT_NT_INFO, "PrimeBase XT (PBXT) Engine %s loaded...\n", xt_get_version());
	xt_logf(XT_NT_INFO, "Paul McCullagh, PrimeBase Technologies GmbH, http://www.primebase.org\n");

	index_cache_size = ha_set_variable(&pbxt_index_cache_size, &vp_index_cache_size);
	record_cache_size = ha_set_variable(&pbxt_record_cache_size, &vp_record_cache_size);
	log_cache_size = ha_set_variable(&pbxt_log_cache_size, &vp_log_cache_size);
	log_file_threshold = ha_set_variable(&pbxt_log_file_threshold, &vp_log_file_threshold);
	transaction_buffer_size = ha_set_variable(&pbxt_transaction_buffer_size, &vp_transaction_buffer_size);
	log_buffer_size = ha_set_variable(&pbxt_log_buffer_size, &vp_log_buffer_size);
	checkpoint_frequency = ha_set_variable(&pbxt_checkpoint_frequency, &vp_checkpoint_frequency);
	data_log_threshold = ha_set_variable(&pbxt_data_log_threshold, &vp_data_log_threshold);
	data_file_grow_size = ha_set_variable(&pbxt_data_file_grow_size, &vp_data_file_grow_size);
	row_file_grow_size = ha_set_variable(&pbxt_row_file_grow_size, &vp_row_file_grow_size);

	xt_db_log_file_threshold = (xtLogOffset) log_file_threshold;
	xt_db_log_buffer_size = (size_t) xt_align_offset(log_buffer_size, 512);
	xt_db_transaction_buffer_size = (size_t) xt_align_offset(transaction_buffer_size, 512);
	xt_db_checkpoint_frequency = (size_t) checkpoint_frequency;
	xt_db_data_log_threshold = (off_t) data_log_threshold;
	xt_db_data_file_grow_size = (size_t) data_file_grow_size;
	xt_db_row_file_grow_size = (size_t) row_file_grow_size;

	pbxt_ignore_case = lower_case_table_names != 0;
	if (pbxt_ignore_case)
		pbxt_share_tables = xt_new_hashtable(self, ha_hash_comp_ci, ha_hash_ci, ha_hash_free, TRUE, FALSE);
	else
		pbxt_share_tables = xt_new_hashtable(self, ha_hash_comp, ha_hash, ha_hash_free, TRUE, FALSE);

	xt_thread_wait_init(self);
	xt_fs_init(self);
	xt_lock_installation(self, mysql_real_data_home);
	XTSystemTableShare::startUp(self);
	xt_init_databases(self);
	xt_ind_init(self, (size_t) index_cache_size);
	xt_tc_init(self, (size_t) record_cache_size);
	xt_xlog_init(self, (size_t) log_cache_size);
}

static void pbxt_call_exit(XTThreadPtr self)
{
	xt_logf(XT_NT_INFO, "PrimeBase XT Engine shutdown...\n");

#ifdef TRACE_STATEMENTS
	xt_dump_trace();
#endif
#ifdef XT_USE_GLOBAL_DB
	xt_ha_close_global_database(self);
#endif
#ifdef DEBUG
	//xt_stop_database_threads(self, FALSE);
	xt_stop_database_threads(self, TRUE);
#else
	xt_stop_database_threads(self, TRUE);
#endif
	/* This will tell the freeer to quit ASAP: */
	xt_quit_freeer(self);
	/* We conditional stop the freeer here, because if we are
	 * in startup, then the free will be hanging.
	 * {FREEER-HANG}
	 *
	 * This problem has been solved by MySQL!
	 */
	xt_stop_freeer(self);
	xt_exit_databases(self);
	XTSystemTableShare::shutDown(self);
	xt_xlog_exit(self);
	xt_tc_exit(self);
	xt_ind_exit(self);
	xt_unlock_installation(self, mysql_real_data_home);
	xt_fs_exit(self);
	xt_thread_wait_exit(self);
	if (pbxt_share_tables) {
		xt_free_hashtable(self, pbxt_share_tables);
		pbxt_share_tables = NULL;
	}
}

/*
 * Shutdown the PBXT sub-system.
 */
static void ha_exit(XTThreadPtr self)
{
	/* Wrap things up... */
	xt_unuse_database(self, self);	/* Just in case the main thread has a database in use (for testing)? */
	/* This may cause the streaming engine to cleanup connections and 
	 * tables belonging to this engine. This in turn may require some of
	 * the stuff below (like xt_create_thread() called from pbxt_close_table()! */
#ifdef PBMS_ENABLED
	pbms_finalize();
#endif
	pbxt_call_exit(self);
	xt_exit_threading(self);
	xt_exit_memory();
	xt_exit_logging();
	xt_p_mutex_destroy(&pbxt_database_mutex);		
	pbxt_inited = false;
}

/*
 * Outout the PBXT status. Return FALSE on error.
 */
#ifdef DRIZZLED
bool PBXTStorageEngine::show_status(Session *thd, stat_print_fn *stat_print, enum ha_stat_type)
#else
static bool pbxt_show_status(handlerton *XT_UNUSED(hton), THD* thd, 
                          stat_print_fn* stat_print,
                          enum ha_stat_type XT_UNUSED(stat_type))
#endif
{
	XTThreadPtr			self;	
	int					err = 0;
	XTStringBufferRec	strbuf = { 0, 0, 0 };
	bool				not_ok = FALSE;

	if (!(self = ha_set_current_thread(thd, &err)))
		return FALSE;

#ifdef XT_SHOW_DUMPS_TRACE
	//if (pbxt_database)
	//	xt_dump_xlogs(pbxt_database, 0);
	xt_trace("// %s - dump\n", xt_trace_clock_diff(NULL));
	xt_dump_trace();
#endif
#ifdef XT_TRACK_CONNECTIONS
	xt_dump_conn_tracking();
#endif

	try_(a) {
		myxt_get_status(self, &strbuf);
	}
	catch_(a) {
		not_ok = TRUE;
	}
	cont_(a);

	if (!not_ok) {
		if (stat_print(thd, "PBXT", 4, "", 0, strbuf.sb_cstring, strbuf.sb_len))
			not_ok = TRUE;
	}
	xt_sb_set_size(self, &strbuf, 0);

	return not_ok;
}

/*
 * Initialize the PBXT sub-system.
 *
 * return 1 on error, else 0.
 */
#ifdef DRIZZLED
static int pbxt_init(PluginRegistry &registry)
#else
static int pbxt_init(void *p)
#endif
{
	int init_err = 0;

	XT_TRACE_CALL();

	if (sizeof(xtWordPS) != sizeof(void *)) {
		printf("PBXT: This won't work, I require that sizeof(xtWordPS) == sizeof(void *)!\n");
		XT_RETURN(1);
	}

	/* GOTCHA: This will "detect" if are loading the plug-in
	 * with different --with-debug option to MySQL.
	 *
	 * In this case, you will get an error when loading the
	 * library that some symbol was not found.
	 */
	void *dummy = my_malloc(100, MYF(0));
	my_free((byte *) dummy, MYF(0));

 	if (!pbxt_inited) {
		XTThreadPtr self = NULL;

 		xt_p_mutex_init_with_autoname(&pbxt_database_mutex, NULL);

#ifdef DRIZZLED
		pbxt_hton= new PBXTStorageEngine(std::string("PBXT"));
		registry.add(pbxt_hton);
#else
		pbxt_hton = (handlerton *) p;
		pbxt_hton->state = SHOW_OPTION_YES;
		pbxt_hton->db_type = DB_TYPE_PBXT; // Wow! I have my own!
		pbxt_hton->close_connection = pbxt_close_connection; /* close_connection, cleanup thread related data. */
		pbxt_hton->commit = pbxt_commit; /* commit */
		pbxt_hton->rollback = pbxt_rollback; /* rollback */
		pbxt_hton->create = pbxt_create_handler; /* Create a new handler */
		pbxt_hton->drop_database = pbxt_drop_database; /* Drop a database */
		pbxt_hton->panic = pbxt_panic; /* Panic call */
		pbxt_hton->show_status = pbxt_show_status;
		pbxt_hton->flags = HTON_NO_FLAGS; /* HTON_CAN_RECREATE - Without this flags TRUNCATE uses delete_all_rows() */
#endif
		if (!xt_init_logging())					/* Initialize logging */
			goto error_1;

#ifdef PBMS_ENABLED
		PBMSResultRec result;
		if (!pbms_initialize("PBXT", false, &result)) {
			xt_logf(XT_NT_ERROR, "pbms_initialize() Error: %s", result.mr_message);
			goto error_2;
		}
#endif

		if (!xt_init_memory())					/* Initialize memory */
			goto error_3;

		/* +7 assumes:
		 * We are not using multiple database, and:
		 * +1 Main thread.
		 * +1 Compactor thread
		 * +1 Writer thread
		 * +1 Checkpointer thread
		 * +1 Sweeper thread
		 * +1 Free'er thread
		 * +1 Temporary thread (e.g. TempForClose, TempForEnd)
		 */
#ifndef DRIZZLED
		if (pbxt_max_threads == 0)
			pbxt_max_threads = max_connections + 7;
#endif
		self = xt_init_threading(pbxt_max_threads);				/* Create the main self: */
		if (!self)
			goto error_3;

 		pbxt_inited = true;

		try_(a) {
			/* Initialize all systems */
			pbxt_call_init(self);

			/* Conditional unit test: */
#ifdef XT_UNIT_TEST
			//xt_unit_test_create_threads(self);
			xt_unit_test_read_write_locks(self);
			//xt_unit_test_mutex_locks(self);
#endif

			/* {OPEN-DB-SWEEPER-WAIT}
			 * I have to start the freeer before I open and recover the database
			 * because it we run out of cache while waiting for the sweeper
			 * we will hang!
			 */
			xt_start_freeer(self);

#ifdef XT_USE_GLOBAL_DB
			/* Open the global database. */
			ASSERT(!pbxt_database);
			{
				THD *curr_thd = current_thd;
				THD *thd = NULL;

#ifndef DRIZZLED
				extern myxt_mutex_t LOCK_plugin;

				/* {MYSQL QUIRK}
				 * I have to release this lock for PBXT recovery to
				 * work, because it needs to open .frm files.
				 * So, I unlock, but during INSTALL PLUGIN this is
				 * risky, because we are in multi-threaded
				 * mode!
				 *
				 * Although, as far as I can tell from the MySQL code,
				 * INSTALL PLUGIN should still work ok, during
				 * concurrent access, because we are not
				 * relying on pointer/memory that may be changed by
				 * other users.
				 *
				 * Only real problem, 2 threads try to load the same
				 * plugin at the same time.
				 */
				myxt_mutex_unlock(&LOCK_plugin);
#endif

				/* Can't do this here yet, because I need a THD! */
				try_(b) {
					/* {MYSQL QUIRK}
					 * Sometime we have a THD,
					 * sometimes we don't.
					 * So far, I have noticed that during INSTALL PLUGIN,
					 * we have one, otherwize not.
					 */
					if (!curr_thd) {
						if (!(thd = (THD *) myxt_create_thread()))
							xt_throw(self);
					}

					xt_xres_start_database_recovery(self);
				}
				catch_(b) {
					/* It is possible that the error was reset by cleanup code.
					 * Set a generic error code in that case.
					 */
					/* PMC - This is not necessary in because exceptions are 
					 * now preserved, in exception handler cleanup.
					*/
					if (!self->t_exception.e_xt_err)
						xt_register_error(XT_REG_CONTEXT, XT_SYSTEM_ERROR, 0, "Initialization failed"); 
					xt_log_exception(self, &self->t_exception, XT_LOG_DEFAULT);
					init_err = 1;
				}
				cont_(b);

				if (thd)
					myxt_destroy_thread(thd, FALSE);
#ifndef DRIZZLED
				myxt_mutex_lock(&LOCK_plugin);
#endif
			}
#endif
		}
		catch_(a) {
			xt_log_exception(self, &self->t_exception, XT_LOG_DEFAULT);
			init_err = 1;
		}
		cont_(a);

		if (init_err) {
			/* {FREEER-HANG} The free-er will be hung in:
				#0	0x91fc6a2e in semaphore_wait_signal_trap
				#1	0x91fce505 in pthread_mutex_lock
				#2	0x00489633 in safe_mutex_lock at thr_mutex.c:149
				#3	0x002dfca9 in plugin_thdvar_init at sql_plugin.cc:2398
				#4	0x000d6a12 in THD::init at sql_class.cc:715
				#5	0x000de9d3 in THD::THD at sql_class.cc:597
				#6	0x000debe1 in THD::THD at sql_class.cc:631
				#7	0x00e207a4 in myxt_create_thread at myxt_xt.cc:2666
				#8	0x00e3134b in tabc_fr_run_thread at tabcache_xt.cc:982
				#9	0x00e422ca in thr_main at thread_xt.cc:1006
				#10	0x91ff7c55 in _pthread_start
				#11	0x91ff7b12 in thread_start
			 *
			 * so it is not good trying to stop it here!
			 *
			 * With regard to this problem, see {OPEN-DB-SWEEPER-WAIT}
			 * Due to this problem, I will probably have to hack
			 * the mutex so that the freeer can get started...
			 *
			 * NOPE! problem has gone in 6.0.9. Also not a problem in
			 * 5.1.29.
			 */
			
			/* {OPEN-DB-SWEEPER-WAIT} 
			 * I have to stop the freeer here because it was
			 * started before opening the database.
			 */

			/* {FREEER-HANG-ON-INIT-ERROR}
			 * pbxt_init is called with LOCK_plugin and if it fails and tries to exit
			 * the freeer here it hangs because the freeer calls THD::~THD which tries
			 * to aquire the same lock and hangs. OTOH MySQL calls pbxt_end() after
			 * an unsuccessful call to pbxt_init, so we defer cleaup, except 
			 * releasing 'self'
			 */
			xt_free_thread(self);
			goto error_3;
		}
		xt_free_thread(self);
 	}
	XT_RETURN(init_err);

	error_3:
#ifdef PBMS_ENABLED
	pbms_finalize();

	error_2:
#endif

	error_1:
	XT_RETURN(1);
}

#ifdef DRIZZLED
static int pbxt_end(PluginRegistry &registry)
#else
static int pbxt_end(void *)
#endif
{
	XTThreadPtr		self;
	int				err = 0;

	XT_TRACE_CALL();

	if (pbxt_inited) {
		XTExceptionRec	e;

		/* This flag also means "shutting down". */
		pbxt_inited = FALSE; 
		self = xt_create_thread("TempForEnd", FALSE, TRUE, &e);
		if (self) {
			self->t_main = TRUE;
			ha_exit(self);
		}
	}

#ifdef DRIZZLED
	registry.remove(pbxt_hton);
#endif
	XT_RETURN(err);
}

#ifndef DRIZZLED
static int pbxt_panic(handlerton *hton, enum ha_panic_function flag)
{
	return pbxt_end(hton);
}
#endif

/*
 * Kill the PBXT thread associated with the MySQL thread.
 */
#ifdef DRIZZLED
int PBXTStorageEngine::close_connection(Session *thd)
{
	PBXTStorageEngine * const hton = this;
#else
static int pbxt_close_connection(handlerton *hton, THD* thd)
{
#endif
	XTThreadPtr		self;

	XT_TRACE_CALL();
	if ((self = (XTThreadPtr) *thd_ha_data(thd, hton))) {
		*thd_ha_data(thd, hton) = NULL;
		/* Required because freeing the thread could cause
		 * free of database which could call xt_close_file_ns()!
		 */
		xt_set_self(self);
		xt_free_thread(self);
	}
	return 0;
}

/*
 * Currently does nothing because it was all done
 * when the last PBXT table was removed from the 
 * database.
 */
#ifdef DRIZZLED
void PBXTStorageEngine::drop_database(char *)
#else
static void pbxt_drop_database(handlerton *XT_UNUSED(hton), char *XT_UNUSED(path))
#endif
{
	XT_TRACE_CALL();
}

/*
 * NOTES ON TRANSACTIONS:
 *
 * 1. If self->st_lock_count == 0 and transaction can be ended immediately.
 *    If not, we must wait until the last lock is released on the last handler
 *    to ensure that the tables are flushed before the transaction is
 *    committed or aborted.
 *
 * 2. all (below) indicates, within a BEGIN/END (i.e. auto_commit off) whether
 *    the statement or the entire transation is being terminated.
 *    We currently ignore statement termination.
 * 
 * 3. If in BEGIN/END we must call ha_rollback() if we abort the transaction
 *    internally.
 */

/*
 * Commit the PBXT transaction of the given thread.
 * thd is the MySQL thread structure.
 * pbxt_thr is a pointer the the PBXT thread structure.
 *
 */
#ifdef DRIZZLED
int PBXTStorageEngine::commit(Session *thd, bool all)
{
	PBXTStorageEngine * const hton = this;
#else
static int pbxt_commit(handlerton *hton, THD *thd, bool all)
{
#endif
	int			err = 0;
	XTThreadPtr	self;

	if ((self = (XTThreadPtr) *thd_ha_data(thd, hton))) {
		XT_PRINT1(self, "pbxt_commit all=%d\n", all);

		if (self->st_xact_data) {
			/* There are no table locks, commit immediately in all cases
			 * except when this is a statement commit with an explicit
			 * transaction (!all && !self->st_auto_commit).
			 */
			if (all || self->st_auto_commit) {
				XT_PRINT0(self, "xt_xn_commit\n");

				if (!xt_xn_commit(self))
					err = xt_ha_pbxt_thread_error_for_mysql(thd, self, FALSE);
			}
		}
		if (!all)
			self->st_stat_trans = FALSE;
	}
	return err;
}

#ifdef DRIZZLED
int PBXTStorageEngine::rollback(Session *thd, bool all)
{
	PBXTStorageEngine * const hton = this;
#else
static int pbxt_rollback(handlerton *hton, THD *thd, bool all)
{
#endif
	int			err = 0;
	XTThreadPtr	self;

	if ((self = (XTThreadPtr) *thd_ha_data(thd, hton))) {
		XT_PRINT1(self, "pbxt_rollback all=%d\n", all);

		if (self->st_xact_data) {
			/* There are no table locks, rollback immediately in all cases
			 * except when this is a statement commit with an explicit
			 * transaction (!all && !self->st_auto_commit).
			 *
			 * Note, the only reason for a rollback of a operation is
			 * due to an error. In this case PBXT has already
			 * undone the effects of the operation.
			 *
			 * However, this is not the same as statement rollback
			 * which can involve a number of operations.
			 *
			 * TODO: Implement statement rollback.
			 */
			if (all || self->st_auto_commit) {
				XT_PRINT0(self, "xt_xn_rollback\n");
				if (!xt_xn_rollback(self))
					err = xt_ha_pbxt_thread_error_for_mysql(thd, self, FALSE);
			}
		}
		if (!all)
			self->st_stat_trans = FALSE;
	}
	return 0;
}

#ifdef DRIZZLED
handler *PBXTStorageEngine::create(TABLE_SHARE *table, MEM_ROOT *mem_root)
{
	PBXTStorageEngine * const hton = this;
#else
static handler *pbxt_create_handler(handlerton *hton, TABLE_SHARE *table, MEM_ROOT *mem_root)
{
#endif
	if (table && XTSystemTableShare::isSystemTable(table->path.str))
		return new (mem_root) ha_xtsys(hton, table);
	else
		return new (mem_root) ha_pbxt(hton, table);
}

/*
 * -----------------------------------------------------------------------
 * HANDLER LOCKING FUNCTIONS
 *
 * These functions are used get a lock on all handles of a particular table.
 *
 */

static void ha_add_to_handler_list(XTThreadPtr self, XTSharePtr share, ha_pbxt *handler)
{
	xt_lock_mutex(self, (xt_mutex_type *) share->sh_ex_mutex);
	pushr_(xt_unlock_mutex, share->sh_ex_mutex);

	handler->pb_ex_next = share->sh_handlers;
	handler->pb_ex_prev = NULL;
	if (share->sh_handlers)
		share->sh_handlers->pb_ex_prev = handler;
	share->sh_handlers = handler;

	freer_(); // xt_unlock_mutex(share->sh_ex_mutex)
}

static void ha_remove_from_handler_list(XTThreadPtr self, XTSharePtr share, ha_pbxt *handler)
{
	xt_lock_mutex(self, (xt_mutex_type *) share->sh_ex_mutex);
	pushr_(xt_unlock_mutex, share->sh_ex_mutex);

	/* Move front pointer: */
	if (share->sh_handlers == handler)
		share->sh_handlers = handler->pb_ex_next;

	/* Remove from list: */
	if (handler->pb_ex_prev)
		handler->pb_ex_prev->pb_ex_next = handler->pb_ex_next;
	if (handler->pb_ex_next)
		handler->pb_ex_next->pb_ex_prev = handler->pb_ex_prev;

	freer_(); // xt_unlock_mutex(share->sh_ex_mutex)
}

/*
 * Aquire exclusive use of a table, by waiting for all
 * threads to complete use of all handlers of the table.
 * At the same time we hold up all threads
 * that want to use handlers belonging to the table.
 *
 * But we do not hold up threads that close the handlers.
 */
static void ha_aquire_exclusive_use(XTThreadPtr self, XTSharePtr share, ha_pbxt *mine)
{
	ha_pbxt	*handler;
	time_t	end_time = time(NULL) + XT_SHARE_LOCK_TIMEOUT / 1000;

	XT_PRINT1(self, "ha_aquire_exclusive_use %s PBXT X lock\n", share->sh_table_path->ps_path);
	/* GOTCHA: It is possible to hang here, if you hold
	 * onto the sh_ex_mutex lock, before we really
	 * have the exclusive lock (i.e. before all
	 * handlers are no longer in use.
	 * The reason is, because reopen() is not possible
	 * when some other thread holds sh_ex_mutex.
	 * So this can prevent a thread from completing its
	 * use of a handler, when prevents exclusive use
	 * here.
	 */
	xt_lock_mutex(self, (xt_mutex_type *) share->sh_ex_mutex);
	pushr_(xt_unlock_mutex, share->sh_ex_mutex);

	/* Wait until we can get an exclusive lock: */
	while (share->sh_table_lock) {
		xt_timed_wait_cond(self, (xt_cond_type *) share->sh_ex_cond, (xt_mutex_type *) share->sh_ex_mutex, XT_SHARE_LOCK_WAIT);
		if (time(NULL) > end_time) {
			freer_(); // xt_unlock_mutex(share->sh_ex_mutex)
			xt_throw_taberr(XT_CONTEXT, XT_ERR_LOCK_TIMEOUT, share->sh_table_path);
		}
	}

	/* This tells readers (and other exclusive lockers) that someone has an exclusive lock. */
	share->sh_table_lock = TRUE;
	
	/* Wait for all open handlers use count to go to 0 */	
	retry:
	handler = share->sh_handlers;
	while (handler) {
		if (handler == mine || !handler->pb_ex_in_use)
			handler = handler->pb_ex_next;
		else {
			/* Wait a bit, and try again: */
			xt_timed_wait_cond(self, (xt_cond_type *) share->sh_ex_cond, (xt_mutex_type *) share->sh_ex_mutex, XT_SHARE_LOCK_WAIT);
			if (time(NULL) > end_time) {
				freer_(); // xt_unlock_mutex(share->sh_ex_mutex)
				xt_throw_taberr(XT_CONTEXT, XT_ERR_LOCK_TIMEOUT, share->sh_table_path);
			}
			/* Handler may have been freed, check from the begining again: */
			goto retry;
		}
	}

	freer_(); // xt_unlock_mutex(share->sh_ex_mutex)
}

/*
 * If you have exclusively locked the table, you can close all handler
 * open tables.
 *
 * Call ha_close_open_tables() to get an exclusive lock.
 */
static void ha_close_open_tables(XTThreadPtr self, XTSharePtr share, ha_pbxt *mine)
{
	ha_pbxt *handler;

	xt_lock_mutex(self, (xt_mutex_type *) share->sh_ex_mutex);
	pushr_(xt_unlock_mutex, share->sh_ex_mutex);

	/* Now that we know no handler is in use, we can close all the
	 * open tables...
	 */
	handler = share->sh_handlers;
	while (handler) {
		if (handler != mine && handler->pb_open_tab) {
			xt_db_return_table_to_pool_ns(handler->pb_open_tab);
			handler->pb_open_tab = NULL;
		}
		handler = handler->pb_ex_next;
	}

	freer_(); // xt_unlock_mutex(share->sh_ex_mutex)
}

#ifdef PBXT_ALLOW_PRINTING
static void ha_release_exclusive_use(XTThreadPtr self, XTSharePtr share)
#else
static void ha_release_exclusive_use(XTThreadPtr XT_UNUSED(self), XTSharePtr share)
#endif
{
	XT_PRINT1(self, "ha_release_exclusive_use %s PBXT X UNLOCK\n", share->sh_table_path->ps_path);
	xt_lock_mutex_ns((xt_mutex_type *) share->sh_ex_mutex);
	share->sh_table_lock = FALSE;
	xt_broadcast_cond_ns((xt_cond_type *) share->sh_ex_cond);
	xt_unlock_mutex_ns((xt_mutex_type *) share->sh_ex_mutex);
}

static xtBool ha_wait_for_shared_use(ha_pbxt *mine, XTSharePtr share)
{
	time_t	end_time = time(NULL) + XT_SHARE_LOCK_TIMEOUT / 1000;

	XT_PRINT1(xt_get_self(), "ha_wait_for_shared_use %s share lock wait...\n", share->sh_table_path->ps_path);
	mine->pb_ex_in_use = 0;
	xt_lock_mutex_ns((xt_mutex_type *) share->sh_ex_mutex);
	while (share->sh_table_lock) {
		/* Wake up the exclusive locker (may be waiting). He can try to continue: */
		xt_broadcast_cond_ns((xt_cond_type *) share->sh_ex_cond);

		if (!xt_timed_wait_cond(NULL, (xt_cond_type *) share->sh_ex_cond, (xt_mutex_type *) share->sh_ex_mutex, XT_SHARE_LOCK_WAIT)) {
			xt_unlock_mutex_ns((xt_mutex_type *) share->sh_ex_mutex);
			return FAILED;
		}

		if (time(NULL) > end_time) {
			xt_unlock_mutex_ns((xt_mutex_type *) share->sh_ex_mutex);
			xt_register_taberr(XT_REG_CONTEXT, XT_ERR_LOCK_TIMEOUT, share->sh_table_path);
			return FAILED;
		}
	}
	mine->pb_ex_in_use = 1;
	xt_unlock_mutex_ns((xt_mutex_type *) share->sh_ex_mutex);
	return OK;
}

xtPublic int ha_pbxt::reopen()
{
	THD				*thd = current_thd;
	int				err = 0;
	XTThreadPtr		self;	
	xtBool			tabled_opened = FALSE;

	if (!(self = ha_set_current_thread(thd, &err)))
		return xt_ha_pbxt_to_mysql_error(err);

	try_(a) {
		xt_ha_open_database_of_table(self, pb_share->sh_table_path);

		ha_open_share(self, pb_share, &tabled_opened);

		if (!(pb_open_tab = xt_db_open_table_using_tab(pb_share->sh_table, self)))
			xt_throw(self);
		pb_open_tab->ot_thread = self;

		if (tabled_opened) {
#ifdef LOAD_TABLE_ON_OPEN
			xt_tab_load_table(self, pb_open_tab);
#else
			xt_tab_load_row_pointers(self, pb_open_tab);
#endif
			xt_ind_set_index_selectivity(self, pb_open_tab);
			/* If the number of rows is less than 150 we will recalculate the
			 * selectity of the indices, as soon as the number of rows
			 * exceeds 200 (see [**])
			 */
			pb_share->sh_recalc_selectivity = (pb_share->sh_table->tab_row_eof_id - 1 - pb_share->sh_table->tab_row_fnum) < 150;
		}

		/* I am not doing this anymore because it was only required
		 * for DELETE FROM table;, which is now implemented
		 * by deleting each row.
		 * TRUNCATE TABLE does not preserve the counter value.
		 */
		//init_auto_increment(pb_share->sh_min_auto_inc);
		init_auto_increment(0);
	}
	catch_(a) {
		err = xt_ha_pbxt_thread_error_for_mysql(thd, self, pb_ignore_dup_key);
	}
	cont_(a);
	
	return err;
}

/*
 * -----------------------------------------------------------------------
 * INFORMATION SCHEMA FUNCTIONS
 *
 */

int pbxt_statistics_fill_table(THD *thd, TABLE_LIST *tables, COND *cond)
{
	XTThreadPtr		self;	
	int				err = 0;

	if (!(self = ha_set_current_thread(thd, &err)))
		return xt_ha_pbxt_to_mysql_error(err);
	try_(a) {
		err = myxt_statistics_fill_table(self, thd, tables, cond, system_charset_info);
	}
	catch_(a) {
		err = xt_ha_pbxt_thread_error_for_mysql(thd, self, FALSE);
	}
	cont_(a);
	return err;
}

ST_FIELD_INFO pbxt_statistics_fields_info[]=
{
	{ "ID",		4,	MYSQL_TYPE_LONG,		0, 0, "The ID of the statistic", SKIP_OPEN_TABLE},
	{ "Name",	40, MYSQL_TYPE_STRING,		0, 0, "The name of the statistic", SKIP_OPEN_TABLE},
	{ "Value",	8,	MYSQL_TYPE_LONGLONG,	0, 0, "The accumulated value", SKIP_OPEN_TABLE},
	{ 0,		0,	MYSQL_TYPE_STRING,		0, 0, 0, SKIP_OPEN_TABLE}
};

#ifdef DRIZZLED
static InfoSchemaTable	*pbxt_statistics_table;

int pbxt_init_statitics(PluginRegistry &registry)
#else
int pbxt_init_statitics(void *p)
#endif
{
#ifdef DRIZZLED
	pbxt_statistics_table = (InfoSchemaTable *)xt_calloc_ns(sizeof(InfoSchemaTable));
	pbxt_statistics_table->table_name= "PBXT_STATISTICS";
	registry.add(pbxt_statistics_table);
#else
	ST_SCHEMA_TABLE *pbxt_statistics_table = (ST_SCHEMA_TABLE *) p;
#endif
	pbxt_statistics_table->fields_info = pbxt_statistics_fields_info;
	pbxt_statistics_table->fill_table = pbxt_statistics_fill_table;

#if defined(XT_WIN) && defined(XT_COREDUMP)
	void register_crash_filter();

	if (pbxt_crash_debug)
		register_crash_filter();
#endif

	return 0;
}

#ifdef DRIZZLED
int pbxt_exit_statitics(PluginRegistry &registry)
#else
int pbxt_exit_statitics(void *XT_UNUSED(p))
#endif
{
#ifdef DRIZZLED
	registry.remove(pbxt_statistics_table);
	xt_free_ns(pbxt_statistics_table);
#endif
	return(0);
}

/*
 * -----------------------------------------------------------------------
 * DYNAMIC HOOKS
 *
 */

ha_pbxt::ha_pbxt(handlerton *hton, TABLE_SHARE *table_arg) : handler(hton, table_arg)
{
	pb_share = NULL;
	pb_open_tab = NULL;
	pb_key_read = FALSE;
	pb_ignore_dup_key = 0;
	pb_lock_table = FALSE;
	pb_table_locked = 0;
	pb_ex_next = NULL;
	pb_ex_prev = NULL;
	pb_ex_in_use = 0;
	pb_in_stat = FALSE;
}

/*
 * If frm_error() is called then we will use this to to find out what file extentions
 * exist for the storage engine. This is also used by the default rename_table and
 * delete_table method in handler.cc.
 */
const char **ha_pbxt::bas_ext() const
{
	return pbxt_extensions;
}

/*
 * Specify the caching type: HA_CACHE_TBL_NONTRANSACT, HA_CACHE_TBL_NOCACHE
 * HA_CACHE_TBL_ASKTRANSACT, HA_CACHE_TBL_TRANSACT
 */
MX_UINT8_T ha_pbxt::table_cache_type()
{
	return HA_CACHE_TBL_TRANSACT; /* Use transactional query cache */
}

MX_TABLE_TYPES_T ha_pbxt::table_flags() const
{
	return (
		/* We need this flag because records are not packed
		 * into a table which means #ROWID != offset
		 */
		HA_REC_NOT_IN_SEQ |
		/* Since PBXT caches read records itself, I believe
		 * this to be the case.
		 */
		HA_FAST_KEY_READ |
		/*
		 * I am assuming a "key" means a unique index.
		 * Of course a primary key does not allow nulls.
		 */
		HA_NULL_IN_KEY |
		/*
		 * This is necessary because a MySQL blob can be
		 * fairly small.
		 */
		HA_CAN_INDEX_BLOBS |
		/*
		 * Due to transactional influences, this will be
		 * the case.
		 * Although the count is good enough for practical
		 * purposes!
		HA_NOT_EXACT_COUNT |
		 */
		/*
		 * This basically means we have a file with the name of
		 * database table (which we do).
		 */
		HA_FILE_BASED |
		/*
		 * Not sure what this does (but MyISAM and InnoDB have it)?!
		 * Could it mean that we support the handler functions.
		 */
		HA_CAN_SQL_HANDLER |
		/*
		 * This is not true, we cannot insert delayed, but a
		 * really cannot see what's wrong with inserting normally
		 * when asked to insert delayed!
		 * And the functionallity is required to pass the alter_table
		 * test.
		 *
		 * Disabled because of MySQL bug #40505
		 */
		/*HA_CAN_INSERT_DELAYED |*/
#if MYSQL_VERSION_ID > 50119
		/* We can do row logging, but not statement, because
		 * MVCC is not serializable!
		 */
		HA_BINLOG_ROW_CAPABLE |
#endif
		/*
		 * Auto-increment is allowed on a partial key.
		 */
		HA_AUTO_PART_KEY);
}

/*
 * The following query from the DBT1 test is VERY slow
 * if we do not set HA_READ_ORDER.
 * The reason is that it must scan all duplicates, then
 * sort.
 *
 * SELECT o_id, o_carrier_id, o_entry_d, o_ol_cnt
 * FROM orders FORCE INDEX (o_w_id)
 * WHERE o_w_id = 2
   * AND o_d_id = 1
   * AND o_c_id = 500
 * ORDER BY o_id DESC limit 1;
 *
 */
#define FLAGS_ARE_READ_DYNAMICALLY

MX_ULONG_T ha_pbxt::index_flags(uint XT_UNUSED(inx), uint XT_UNUSED(part), bool XT_UNUSED(all_parts)) const
{
	/* It would be nice if the dynamic version of this function works,
	 * but it does not. MySQL loads this information when the table is openned,
	 * and then it is fixed.
	 *
	 * The problem is, I have had to remove the HA_READ_ORDER option although
	 * it applies to PBXT. PBXT returns entries in index order during an index
	 * scan in _almost_ all cases.
	 *
	 * A number of cases are demostrated here: [(11)]
	 *
	 * If involves the following conditions:
	 * - a SELECT FOR UPDATE, UPDATE or DELETE statement
	 * - an ORDER BY, or join that requires the sort order
	 * - another transaction which updates the index while it is being
	 *   scanned.
	 *
	 * In this "obscure" case, the index scan may return index
	 * entries in the wrong order.
	 */
#ifdef FLAGS_ARE_READ_DYNAMICALLY
	/* If were are in an update (SELECT FOR UPDATE, UPDATE or DELETE), then
	 * it may be that we return the rows from an index in the wrong
	 * order! This is due to the fact that update reads wait for transactions
	 * to commit and this means that index entries may change position during
	 * the scan!
	 */
	if (pb_open_tab && pb_open_tab->ot_for_update)
		return (HA_READ_NEXT | HA_READ_PREV | HA_READ_RANGE | HA_KEYREAD_ONLY);
	/* If I understand HA_KEYREAD_ONLY then this means I do not
	 * need to fetch the record associated with an index
	 * key.
	 */
	return (HA_READ_NEXT | HA_READ_PREV | HA_READ_ORDER | HA_READ_RANGE | HA_KEYREAD_ONLY);
#else
	return (HA_READ_NEXT | HA_READ_PREV | HA_READ_RANGE | HA_KEYREAD_ONLY);
#endif
}

void ha_pbxt::internal_close(THD *thd, struct XTThread *self)
{
	if (pb_share) {
		xtBool			removed;
		XTOpenTablePtr	ot;

		try_(a) {
			/* This lock must be held when we remove the handler's
			 * open table because ha_close_open_tables() can run
			 * concurrently.
			 */
			xt_lock_mutex_ns(pb_share->sh_ex_mutex);
			if ((ot = pb_open_tab)) {
				pb_open_tab->ot_thread = self;
				if (self->st_database != pb_open_tab->ot_table->tab_db)
					xt_ha_open_database_of_table(self, pb_share->sh_table_path);
				pb_open_tab = NULL;
				pushr_(xt_db_return_table_to_pool, ot);
			}
			xt_unlock_mutex_ns(pb_share->sh_ex_mutex);

			ha_remove_from_handler_list(self, pb_share, this);

			/* Someone may be waiting for me to complete: */
			xt_broadcast_cond_ns((xt_cond_type *) pb_share->sh_ex_cond);

			removed = ha_unget_share_removed(self, pb_share);

			if (ot) {
				/* Flush the table if this was the last handler: */
				/* This is not necessary but has the affect that
				 * FLUSH TABLES; does a checkpoint!
				 */
				if (removed) {
					/* GOTCHA:
					 * This was killing performance as the number of threads increased!
					 *
					 * When MySQL runs out of table handlers because the table
					 * handler cache is too small, it starts to close handlers.
					 * (open_cache.records > table_cache_size)
					 *
					 * Which can lead to closing all handlers for a particular table.
					 *
					 * It does this while holding lock_OPEN!
					 * So this code below leads to a sync operation while lock_OPEN
					 * is held. The result is that the whole server comes to a stop.
					 */
					if (!thd || thd_sql_command(thd) == SQLCOM_FLUSH) // FLUSH TABLES
						xt_sync_flush_table(self, ot);
				}
				freer_(); // xt_db_return_table_to_pool(ot);
			}
		}
		catch_(a) {
			xt_log_and_clear_exception(self);
		}
		cont_(a);

		pb_share = NULL;
	}
}

/*
 * Used for opening tables. The name will be the name of the file.
 * A table is opened when it needs to be opened. For instance
 * when a request comes in for a select on the table (tables are not
 * open and closed for each request, they are cached).

 * Called from handler.cc by handler::ha_open(). The server opens all tables by
 * calling ha_open() which then calls the handler specific open().
 */
int ha_pbxt::open(const char *table_path, int XT_UNUSED(mode), uint XT_UNUSED(test_if_locked))
{
	THD			*thd = current_thd;
	int			err = 0;
	XTThreadPtr	self;
	xtBool		tabled_opened = FALSE;

	ref_length = XT_RECORD_OFFS_SIZE;

	if (!(self = ha_set_current_thread(thd, &err)))
		return xt_ha_pbxt_to_mysql_error(err);

	XT_PRINT1(self, "ha_pbxt::open %s\n", table_path);

	pb_ex_in_use = 1;
	try_(a) {
		xt_ha_open_database_of_table(self, (XTPathStrPtr) table_path);

		pb_share = ha_get_share(self, table_path, true, &tabled_opened);
		ha_add_to_handler_list(self, pb_share, this);
		if (pb_share->sh_table_lock) {
			if (!ha_wait_for_shared_use(this, pb_share))
				xt_throw(self);
		}

		ha_open_share(self, pb_share, &tabled_opened);

		thr_lock_data_init(&pb_share->sh_lock, &pb_lock, NULL);
		if (!(pb_open_tab = xt_db_open_table_using_tab(pb_share->sh_table, self)))
			xt_throw(self);
		pb_open_tab->ot_thread = self;

		if (tabled_opened) {
#ifdef LOAD_TABLE_ON_OPEN
			xt_tab_load_table(self, pb_open_tab);
#else
			xt_tab_load_row_pointers(self, pb_open_tab);
#endif
			xt_ind_set_index_selectivity(self, pb_open_tab);
			pb_share->sh_recalc_selectivity = (pb_share->sh_table->tab_row_eof_id - 1 - pb_share->sh_table->tab_row_fnum) < 150;
		}

		init_auto_increment(0);
	}
	catch_(a) {
		err = xt_ha_pbxt_thread_error_for_mysql(thd, self, pb_ignore_dup_key);
		internal_close(thd, self);
	}
	cont_(a);

	if (!err)
		info(HA_STATUS_NO_LOCK | HA_STATUS_VARIABLE | HA_STATUS_CONST);

	pb_ex_in_use = 0;
	if (pb_share) {
		/* Someone may be waiting for me to complete: */
		if (pb_share->sh_table_lock)
			xt_broadcast_cond_ns((xt_cond_type *) pb_share->sh_ex_cond);
	}
	return err;
}


/*
	Closes a table. We call the free_share() function to free any resources
	that we have allocated in the "shared" structure.

	Called from sql_base.cc, sql_select.cc, and table.cc.
	In sql_select.cc it is only used to close up temporary tables or during
	the process where a temporary table is converted over to being a
	myisam table.
	For sql_base.cc look at close_data_tables().
*/
int ha_pbxt::close(void)
{
	THD						*thd = current_thd;
	volatile int			err = 0;
	volatile XTThreadPtr	self;

	if (thd)
		self = ha_set_current_thread(thd, (int *) &err);
	else {
		XTExceptionRec e;

		if (!(self = xt_create_thread("TempForClose", FALSE, TRUE, &e))) {
			xt_log_exception(NULL, &e, XT_LOG_DEFAULT);
			return 0;
		}
	}

	XT_PRINT1(self, "ha_pbxt::close %s\n", pb_share && pb_share->sh_table_path->ps_path ? pb_share->sh_table_path->ps_path : "unknown");

	if (self) {
		try_(a) {
			internal_close(thd, self);
		}
		catch_(a) {
			err = xt_ha_pbxt_thread_error_for_mysql(thd, self, pb_ignore_dup_key);
		}
		cont_(a);

		if (!thd)
			xt_free_thread(self);
	}
	else
		xt_log(XT_NS_CONTEXT, XT_LOG_WARNING, "Unable to release table reference\n");
		
	return err;
}

void ha_pbxt::init_auto_increment(xtWord8 min_auto_inc)
{
	XTTableHPtr	tab;
	xtWord8		nr = 0;
	int			err;

	/* Get the value of the auto-increment value by
	 * loading the highest value from the index...
	 */
	tab = pb_open_tab->ot_table;

	/* Cannot do this if the index version is bad! */
	if (tab->tab_dic.dic_disable_index)
		return;

	xt_spinlock_lock(&tab->tab_ainc_lock);
	if (table->found_next_number_field && !tab->tab_auto_inc) {
		Field		*tmp_fie = table->next_number_field;
		THD			*tmp_thd = table->in_use;
		xtBool		xn_started = FALSE;
		XTThreadPtr	self = pb_open_tab->ot_thread;

		/*
		 * A table may be opened by a thread with a running
		 * transaction!
		 * Since get_auto_increment() does not do an update,
		 * it should be OK to use the transaction we already
		 * have to get the next auto-increment value.
		 */
		if (!self->st_xact_data) {
			self->st_xact_mode = XT_XACT_REPEATABLE_READ;
			self->st_ignore_fkeys = FALSE;
			self->st_auto_commit = TRUE;
			self->st_table_trans = FALSE;
			self->st_abort_trans = FALSE;
			self->st_stat_ended = FALSE;
			self->st_stat_trans = FALSE;
			self->st_is_update = FALSE;
			if (!xt_xn_begin(self)) {
				xt_spinlock_unlock(&tab->tab_ainc_lock);
				xt_throw(self);
			}
			xn_started = TRUE;
		}

		/* Setup the conditions for the next call! */
		table->in_use = current_thd;
		table->next_number_field = table->found_next_number_field;

		extra(HA_EXTRA_KEYREAD);
		table->mark_columns_used_by_index_no_reset(TS(table)->next_number_index, table->read_set);
		column_bitmaps_signal();
 		index_init(TS(table)->next_number_index, 0);
		if (!TS(table)->next_number_key_offset) {
			// Autoincrement at key-start
			err = index_last(table->record[1]);
			if (!err)
				/* {PRE-INC} */
				nr = (xtWord8) table->next_number_field->val_int_offset(TS(table)->rec_buff_length);
		}
		else {
			/* Do an index scan to find the largest value! */
			/* The standard method will not work because it forces
			 * us to lock that table!
			 */
			xtWord8 val;

			err = index_first(table->record[1]);
			while (!err) {
				/* {PRE-INC} */
				val = (xtWord8) table->next_number_field->val_int_offset(TS(table)->rec_buff_length);
				if (val > nr)
					nr = val;
				err = index_next(table->record[1]);
			}
		}

		index_end();
		extra(HA_EXTRA_NO_KEYREAD);

		/* {PRE-INC}
		 * I have changed this from post increment to pre-increment!
		 * The reason is:
		 * When using post increment we are not able to return
		 * the last valid value in the range.
		 *
		 * Here the test example:
		 *
		 * drop table if exists t1;
		 * create table t1 (i tinyint unsigned not null auto_increment primary key) engine=pbxt;
		 * insert into t1 set i = 254;
		 * insert into t1 set i = null;
		 *
		 * With post-increment, this last insert fails because on post increment
		 * the value overflows!
		 *
		 * Pre-increment means we store the current max, and increment
		 * before returning the next value.
		 *
		 * This will work in this situation.
		 */
		tab->tab_auto_inc = nr;
		if (tab->tab_auto_inc < tab->tab_dic.dic_min_auto_inc)
			tab->tab_auto_inc = tab->tab_dic.dic_min_auto_inc-1;
		if (tab->tab_auto_inc < min_auto_inc)
			tab->tab_auto_inc = min_auto_inc-1;

		/* Restore the changed values: */
		table->next_number_field = tmp_fie;
		table->in_use = tmp_thd;

		if (xn_started)
			xt_xn_commit(self);
	}
	xt_spinlock_unlock(&tab->tab_ainc_lock);
}

void ha_pbxt::get_auto_increment(MX_ULONGLONG_T offset, MX_ULONGLONG_T increment,
                                 MX_ULONGLONG_T XT_UNUSED(nb_desired_values),
                                 MX_ULONGLONG_T *first_value,
                                 MX_ULONGLONG_T *nb_reserved_values)
{
	register XTTableHPtr	tab;
	MX_ULONGLONG_T			nr, nr_less_inc;

	ASSERT_NS(pb_ex_in_use);

	tab = pb_open_tab->ot_table;

	/* {PRE-INC}
	 * Assume that nr contains the last value returned!
	 * We will increment and then return the value.
	 */
	xt_spinlock_lock(&tab->tab_ainc_lock);
	nr = (MX_ULONGLONG_T) tab->tab_auto_inc;
	nr_less_inc = nr;
	if (nr < offset)
		nr = offset;
	else if (increment > 1 && ((nr - offset) % increment) != 0)
		nr += increment - ((nr - offset) % increment);
	else
		nr += increment;
	if (table->next_number_field->cmp((const unsigned char *)&nr_less_inc, (const unsigned char *)&nr) < 0)
		tab->tab_auto_inc = (xtWord8) (nr);
	else
		nr = ~0;	/* indicate error to the caller */
	xt_spinlock_unlock(&tab->tab_ainc_lock);

	*first_value = nr;
	*nb_reserved_values = 1;
}

/* GOTCHA: We need to use signed value here because of the test
 * (from auto_increment.test):
 * create table t1 (a int not null auto_increment primary key);
 * insert into t1 values (NULL);
 * insert into t1 values (-1);
 * insert into t1 values (NULL);
 */
void ha_pbxt::set_auto_increment(Field *nr)
{
	register XTTableHPtr	tab;
	MX_ULONGLONG_T			nr_int_val;
	
	nr_int_val = nr->val_int();
	tab = pb_open_tab->ot_table;

	if (nr->cmp((const unsigned char *)&tab->tab_auto_inc) > 0) {
		xt_spinlock_lock(&tab->tab_ainc_lock);

		if (nr->cmp((const unsigned char *)&tab->tab_auto_inc) > 0) {
			/* {PRE-INC}
			 * We increment later, so just set the value!
			MX_ULONGLONG_T nr_int_val_plus_one = nr_int_val + 1;
			if (nr->cmp((const unsigned char *)&nr_int_val_plus_one) < 0)
				tab->tab_auto_inc = nr_int_val_plus_one;
			else
			 */
			tab->tab_auto_inc = nr_int_val;
		}
		xt_spinlock_unlock(&tab->tab_ainc_lock);
	}

	if (xt_db_auto_increment_mode == 1) {
		if (nr_int_val > (MX_ULONGLONG_T) tab->tab_dic.dic_min_auto_inc) {
			/* Do this every 100 calls: */
#ifdef DEBUG
			tab->tab_dic.dic_min_auto_inc = nr_int_val + 5;
#else
			tab->tab_dic.dic_min_auto_inc = nr_int_val + 100;
#endif
			pb_open_tab->ot_thread = xt_get_self();
			if (!xt_tab_write_min_auto_inc(pb_open_tab))
				xt_log_and_clear_exception(pb_open_tab->ot_thread);
		}
	}
}

/*
static void dump_buf(unsigned char *buf, int len)
{
	int i;
	
	for (i=0; i<len; i++) printf("%2c", buf[i] <= 127 ? buf[i] : '.');
	printf("\n");
	for (i=0; i<len; i++) printf("%02x", buf[i]);
	printf("\n");
}
*/

/*
 * write_row() inserts a row. No extra() hint is given currently if a bulk load
 * is happeneding. buf() is a byte array of data. You can use the field
 * information to extract the data from the native byte array type.
 * Example of this would be:
 * for (Field **field=table->field ; *field ; field++)
 * {
 *		...
 * }

 * See ha_tina.cc for an example of extracting all of the data as strings.
 * ha_berekly.cc has an example of how to store it intact by "packing" it
 * for ha_berkeley's own native storage type.

 * See the note for update_row() on auto_increments and timestamps. This
 * case also applied to write_row().

 * Called from item_sum.cc, item_sum.cc, sql_acl.cc, sql_insert.cc,
 * sql_insert.cc, sql_select.cc, sql_table.cc, sql_udf.cc, and sql_update.cc.
 */
int ha_pbxt::write_row(byte *buf)
{
	int err = 0;

	ASSERT_NS(pb_ex_in_use);

	XT_PRINT1(pb_open_tab->ot_thread, "ha_pbxt::write_row %s\n", pb_share->sh_table_path->ps_path);
	XT_DISABLED_TRACE(("INSERT tx=%d val=%d\n", (int) pb_open_tab->ot_thread->st_xact_data->xd_start_xn_id, (int) XT_GET_DISK_4(&buf[1])));
	//statistic_increment(ha_write_count,&LOCK_status);
#ifdef PBMS_ENABLED
	PBMSResultRec result;
	err = pbms_write_row_blobs(table, buf, &result);
	if (err) {
		xt_logf(XT_NT_ERROR, "pbms_write_row_blobs() Error: %s", result.mr_message);
		return err;
	}
#endif

	/* GOTCHA: I have a huge problem with the transaction statement.
	 * It is not ALWAYS committed (I mean ha_commit_trans() is
	 * not always called - for example in SELECT).
	 *
	 * If I call trans_register_ha() but ha_commit_trans() is not called
	 * then MySQL thinks a transaction is still running (while
	 * I have committed the auto-transaction in ha_pbxt::external_lock()).
	 *
	 * This causes all kinds of problems, like transactions
	 * are killed when they should not be.
	 *
	 * To prevent this, I only inform MySQL that a transaction
	 * has beens started when an update is performed. I have determined that
	 * ha_commit_trans() is only guarenteed to be called if an update is done. 
	 */
	if (!pb_open_tab->ot_thread->st_stat_trans) {
		trans_register_ha(pb_mysql_thd, FALSE, pbxt_hton);
		XT_PRINT0(pb_open_tab->ot_thread, "ha_pbxt::write_row trans_register_ha all=FALSE\n");
		pb_open_tab->ot_thread->st_stat_trans = TRUE;
	}

	xt_xlog_check_long_writer(pb_open_tab->ot_thread);

	if (table->timestamp_field_type & TIMESTAMP_AUTO_SET_ON_INSERT)
		table->timestamp_field->set_time();

	if (table->next_number_field && buf == table->record[0]) {
		int update_err = update_auto_increment();
		if (update_err) {
			ha_log_pbxt_thread_error_for_mysql(pb_ignore_dup_key);
			err = update_err;
			goto done;
		}
		set_auto_increment(table->next_number_field);
	}

	if (!xt_tab_new_record(pb_open_tab, (xtWord1 *) buf)) {
		err = ha_log_pbxt_thread_error_for_mysql(pb_ignore_dup_key);

		/*
		 * This is needed to allow the same row to be updated multiple times in case of bulk REPLACE.
		 * This happens during execution of LOAD DATA...REPLACE MySQL first tries to INSERT the row 
		 * and if it gets dup-key error it tries UPDATE, so the same row can be overwriten multiple 
		 * times within the same statement
		 */
		if (err == HA_ERR_FOUND_DUPP_KEY && pb_open_tab->ot_thread->st_is_update)
			pb_open_tab->ot_thread->st_update_id++;
	}

	done:
#ifdef PBMS_ENABLED
	pbms_completed(table, (err == 0));
#endif
	return err;
}

#ifdef UNUSED_CODE
static int equ_bin(const byte *a, const char *b)
{
	while (*a && *b) {
		if (*a != *b)
			return 0;
		a++;
		b++;
	}
	return 1;
}
static void dump_bin(const byte *a_in, int offset, int len_in)
{
	const byte	*a = a_in;
	int			len = len_in;
	
	a += offset;
	while (len > 0) {
		xt_trace("%02X", (int) *a);
		a++;
		len--;
	}
	xt_trace("==");
	a = a_in;
	len = len_in;
	a += offset;
	while (len > 0) {
		xt_trace("%c", (*a > 8 && *a < 127) ? *a : '.');
		a++;
		len--;
	}
	xt_trace("\n");
}
#endif

/*
 * Yes, update_row() does what you expect, it updates a row. old_data will have
 * the previous row record in it, while new_data will have the newest data in
 * it. Keep in mind that the server can do updates based on ordering if an ORDER BY
 * clause was used. Consecutive ordering is not guarenteed.
 *
 * Called from sql_select.cc, sql_acl.cc, sql_update.cc, and sql_insert.cc.
 */
int ha_pbxt::update_row(const byte * old_data, byte * new_data)
{
	int						err = 0;
	register XTThreadPtr	self = pb_open_tab->ot_thread;

	ASSERT_NS(pb_ex_in_use);

	XT_PRINT1(self, "ha_pbxt::update_row %s\n", pb_share->sh_table_path->ps_path);
	XT_DISABLED_TRACE(("UPDATE tx=%d val=%d\n", (int) self->st_xact_data->xd_start_xn_id, (int) XT_GET_DISK_4(&new_data[1])));
	//statistic_increment(ha_update_count,&LOCK_status);

	if (!self->st_stat_trans) {
		trans_register_ha(pb_mysql_thd, FALSE, pbxt_hton);
		XT_PRINT0(self, "ha_pbxt::update_row trans_register_ha all=FALSE\n");
		self->st_stat_trans = TRUE;
	}

	xt_xlog_check_long_writer(self);

	if (!self->st_is_update) {
		self->st_is_update = TRUE;
		self->st_update_id++;
	}

	if (table->timestamp_field_type & TIMESTAMP_AUTO_SET_ON_UPDATE)
		table->timestamp_field->set_time();

#ifdef PBMS_ENABLED
	PBMSResultRec result;

	err = pbms_delete_row_blobs(table, old_data, &result);
	if (err) {
		xt_logf(XT_NT_ERROR, "update_row:pbms_delete_row_blobs() Error: %s", result.mr_message);
		return err;
	}
	err = pbms_write_row_blobs(table, new_data, &result);
	if (err) { 
		xt_logf(XT_NT_ERROR, "update_row:pbms_write_row_blobs() Error: %s", result.mr_message);
		goto pbms_done;
	}
#endif

	/* GOTCHA: We need to check the auto-increment value on update
	 * because of the following test (which fails for InnoDB) -
	 * auto_increment.test:
	 * create table t1 (a int not null auto_increment primary key, val int);
	 * insert into t1 (val) values (1);
	 * update t1 set a=2 where a=1;
	 * insert into t1 (val) values (1);
	 */
	if (table->found_next_number_field && new_data == table->record[0]) {
		MX_LONGLONG_T	nr;
		my_bitmap_map	*old_map;

		old_map = mx_tmp_use_all_columns(table, table->read_set);
		nr = table->found_next_number_field->val_int();
		set_auto_increment(table->found_next_number_field);
		mx_tmp_restore_column_map(table, old_map);
	}

	if (!xt_tab_update_record(pb_open_tab, (xtWord1 *) old_data, (xtWord1 *) new_data))
		err = ha_log_pbxt_thread_error_for_mysql(pb_ignore_dup_key);

	pb_open_tab->ot_table->tab_locks.xt_remove_temp_lock(pb_open_tab, TRUE);
	
#ifdef PBMS_ENABLED
	pbms_done:
	pbms_completed(table, (err == 0));
#endif

	return err;
}

/*
 * This will delete a row. buf will contain a copy of the row to be deleted.
 * The server will call this right after the current row has been called (from
 * either a previous rnd_next() or index call).
 *
 * Called in sql_acl.cc and sql_udf.cc to manage internal table information.
 * Called in sql_delete.cc, sql_insert.cc, and sql_select.cc. In sql_select it is
 * used for removing duplicates while in insert it is used for REPLACE calls.
*/
int ha_pbxt::delete_row(const byte * buf)
{
	int err = 0;

	ASSERT_NS(pb_ex_in_use);

	XT_PRINT1(pb_open_tab->ot_thread, "ha_pbxt::delete_row %s\n", pb_share->sh_table_path->ps_path);
	XT_DISABLED_TRACE(("DELETE tx=%d val=%d\n", (int) pb_open_tab->ot_thread->st_xact_data->xd_start_xn_id, (int) XT_GET_DISK_4(&buf[1])));
	//statistic_increment(ha_delete_count,&LOCK_status);

#ifdef PBMS_ENABLED
	PBMSResultRec result;

	err = pbms_delete_row_blobs(table, buf, &result);
	if (err) {
		xt_logf(XT_NT_ERROR, "pbms_delete_row_blobs() Error: %s", result.mr_message);
		return err;
	}
#endif

	if (!pb_open_tab->ot_thread->st_stat_trans) {
		trans_register_ha(pb_mysql_thd, FALSE, pbxt_hton);
		XT_PRINT0(pb_open_tab->ot_thread, "ha_pbxt::delete_row trans_register_ha all=FALSE\n");
		pb_open_tab->ot_thread->st_stat_trans = TRUE;
	}

	xt_xlog_check_long_writer(pb_open_tab->ot_thread);

	if (!xt_tab_delete_record(pb_open_tab, (xtWord1 *) buf))
		err = ha_log_pbxt_thread_error_for_mysql(pb_ignore_dup_key);

	pb_open_tab->ot_table->tab_locks.xt_remove_temp_lock(pb_open_tab, TRUE);

#ifdef PBMS_ENABLED
	pbms_completed(table, (err == 0));
#endif
	return err;
}

/*
 * -----------------------------------------------------------------------
 * INDEX METHODS
 */

/*
 * This looks like a hack, but actually, it is OK.
 * It depends on the setup done by the super-class. It involves an extra
 * range check that we need to do if a "new" record is returned during
 * an index scan.
 *
 * A new record is returned if a row is updated (by another transaction)
 * during the index scan. If an update is detected, then the scan stops
 * and waits for the transaction to end.
 *
 * If the transaction commits, then the updated row is returned instead
 * of the row it would have returned when doing a consistant read
 * (repeatable read).
 *
 * These new records can appear out of index order, and may not even
 * belong to the index range that we are concerned with.
 *
 * Notice that there is not check for the start of the range. It appears
 * that this is not necessary, MySQL seems to have no problem ignoring
 * such values.
 *
 * A number of test have been given below which demonstrate the use
 * of the function.
 *
 * They also demonstrate the ORDER BY problem described here: [(11)].
 *
 * DROP TABLE IF EXISTS test_tab, test_tab_1, test_tab_2;
 * CREATE TABLE test_tab (ID int primary key, Value int, Name varchar(20), index(Value, Name)) ENGINE=pbxt;
 * INSERT test_tab values(1, 1, 'A');
 * INSERT test_tab values(2, 1, 'B');
 * INSERT test_tab values(3, 1, 'C');
 * INSERT test_tab values(4, 2, 'D');
 * INSERT test_tab values(5, 2, 'E');
 * INSERT test_tab values(6, 2, 'F');
 * INSERT test_tab values(7, 2, 'G');
 * 
 * select * from test_tab where value = 1 order by value, name for update;
 * 
 * -- Test: 1
 * -- C1
 * begin;
 * select * from test_tab where id = 5 for update;
 * 
 * -- C2
 * begin;
 * select * from test_tab where value = 2 order by value, name for update;
 * 
 * -- C1
 * update test_tab set value = 3 where id = 6;
 * commit;
 * 
 * -- Test: 2
 * -- C1
 * begin;
 * select * from test_tab where id = 5 for update;
 * 
 * -- C2
 * begin;
 * select * from test_tab where value >= 2 order by value, name for update;
 * 
 * -- C1
 * update test_tab set value = 3 where id = 6;
 * commit;
 * 
 * -- Test: 3
 * -- C1
 * begin;
 * select * from test_tab where id = 5 for update;
 * 
 * -- C2
 * begin;
 * select * from test_tab where value = 2 order by value, name for update;
 * 
 * -- C1
 * update test_tab set value = 1 where id = 6;
 * commit;
 */

int ha_pbxt::xt_index_in_range(register XTOpenTablePtr XT_UNUSED(ot), register XTIndexPtr ind,
	register XTIdxSearchKeyPtr search_key, xtWord1 *buf)
{
	/* If search key is given, this means we want an exact match. */
	if (search_key) {
		xtWord1 key_buf[XT_INDEX_MAX_KEY_SIZE];

		myxt_create_key_from_row(ind, key_buf, buf, NULL);
		search_key->sk_on_key = myxt_compare_key(ind, search_key->sk_key_value.sv_flags, search_key->sk_key_value.sv_length,
			search_key->sk_key_value.sv_key, key_buf) == 0;
		return search_key->sk_on_key;
	}

	/* Otherwise, check the end of the range. */
	if (end_range)
		return compare_key(end_range) <= 0;
	return 1;
}

int ha_pbxt::xt_index_next_read(register XTOpenTablePtr ot, register XTIndexPtr ind, xtBool key_only,
	register XTIdxSearchKeyPtr search_key, byte *buf)
{
	xt_xlog_check_long_writer(ot->ot_thread);

	if (key_only) {
		/* We only need to read the data from the key: */
		while (ot->ot_curr_rec_id) {
			if (search_key && !search_key->sk_on_key)
				break;

			switch (xt_tab_visible(ot)) {
				case FALSE:
					if (xt_idx_next(ot, ind, search_key))
						break;
				case XT_ERR:
					goto failed;
				case XT_NEW:
					if (!xt_idx_read(ot, ind, (xtWord1 *) buf))
						goto failed;
					if (xt_index_in_range(ot, ind, search_key, buf)) {
						return 0;
					}
					if (!xt_idx_next(ot, ind, search_key))
						goto failed;
					break;
				case XT_RETRY:
					/* We cannot start from the beginning again, if we have
					 * already output rows!
					 * And we need the orginal search key.
					 *
					 * The case in which this occurs is:
					 *
					 * T1: UPDATE tbl_file SET GlobalID = 'DBCD5C4514210200825501089884844_6M' WHERE ID = 39
					 * Locks a particular row.
					 *
					 * T2: SELECT ID,Flags FROM tbl_file WHERE SpaceID = 1 AND Path = '/zi/America/' AND 
					 * Name = 'Cuiaba' AND Flags IN ( 0,1,4,5 ) FOR UPDATE
					 * scans the index and stops on the lock (of the before image) above.
					 *
					 * T1 quits, the sweeper deletes the record updated by T1?!
					 * BUG: Cleanup should wait until T2 is complete!
					 *
					 * T2 continues, and returns XT_RETRY.
					 *
					 * At this stage T2 has already returned some rows, so it may not retry from the
					 * start. Instead it tries to locate the last record it tried to lock.
					 * This record is gone (or not visible), so it finds the next one.
					 *
					 * POTENTIAL BUG: If cleanup does not wait until T2 is complete, then
					 * I may miss the update record, if it is moved before the index scan
					 * position.
					 */
					if (!pb_ind_row_count && search_key) {
						if (!xt_idx_search(pb_open_tab, ind, search_key))
							return ha_log_pbxt_thread_error_for_mysql(pb_ignore_dup_key);
					}
					else {
						if (!xt_idx_research(pb_open_tab, ind))
							goto failed;
					}
					break;
				default:
					if (!xt_idx_read(ot, ind, (xtWord1 *) buf))
						goto failed;
					return 0;
			}
		}
	}
	else {
		while (ot->ot_curr_rec_id) {
			if (search_key && !search_key->sk_on_key)
				break;

			switch (xt_tab_read_record(ot, (xtWord1 *) buf)) {
				case FALSE:
					XT_DISABLED_TRACE(("not visi tx=%d rec=%d\n", (int) ot->ot_thread->st_xact_data->xd_start_xn_id, (int) ot->ot_curr_rec_id));
					if (xt_idx_next(ot, ind, search_key))
						break;
				case XT_ERR:
					goto failed;
				case XT_NEW:
					if (xt_index_in_range(ot, ind, search_key, buf))
						return 0;
					if (!xt_idx_next(ot, ind, search_key))
						goto failed;
					break;
				case XT_RETRY:
					if (!pb_ind_row_count && search_key) {
						if (!xt_idx_search(pb_open_tab, ind, search_key))
							return ha_log_pbxt_thread_error_for_mysql(pb_ignore_dup_key);
					}
					else {
						if (!xt_idx_research(pb_open_tab, ind))
							goto failed;
					}
					break;
				default:
					XT_DISABLED_TRACE(("visible tx=%d rec=%d\n", (int) ot->ot_thread->st_xact_data->xd_start_xn_id, (int) ot->ot_curr_rec_id));
					return 0;
			}
		}
	}
	return HA_ERR_END_OF_FILE;

	failed:
	return ha_log_pbxt_thread_error_for_mysql(FALSE);
}

int ha_pbxt::xt_index_prev_read(XTOpenTablePtr ot, XTIndexPtr ind, xtBool key_only,
	register XTIdxSearchKeyPtr search_key, byte *buf)
{
	if (key_only) {
		/* We only need to read the data from the key: */
		while (ot->ot_curr_rec_id) {
			if (search_key && !search_key->sk_on_key)
				break;

			switch (xt_tab_visible(ot)) {
				case FALSE:
					if (xt_idx_prev(ot, ind, search_key))
						break;
				case XT_ERR:
					goto failed;
				case XT_NEW:
					if (!xt_idx_read(ot, ind, (xtWord1 *) buf))
						goto failed;
					if (xt_index_in_range(ot, ind, search_key, buf))
						return 0;
					if (!xt_idx_next(ot, ind, search_key))
						goto failed;
					break;
				case XT_RETRY:
					if (!pb_ind_row_count && search_key) {
						if (!xt_idx_search_prev(pb_open_tab, ind, search_key))
							return ha_log_pbxt_thread_error_for_mysql(pb_ignore_dup_key);
					}
					else {
						if (!xt_idx_research(pb_open_tab, ind))
							goto failed;
					}
					break;
				default:
					if (!xt_idx_read(ot, ind, (xtWord1 *) buf))
						goto failed;
					return 0;
			}
		}
	}
	else {
		/* We need to read the entire record: */
		while (ot->ot_curr_rec_id) {
			if (search_key && !search_key->sk_on_key)
				break;

			switch (xt_tab_read_record(ot, (xtWord1 *) buf)) {
				case FALSE:
					if (xt_idx_prev(ot, ind, search_key))
						break;
				case XT_ERR:
					goto failed;
				case XT_NEW:
					if (xt_index_in_range(ot, ind, search_key, buf))
						return 0;
					if (!xt_idx_next(ot, ind, search_key))
						goto failed;
					break;
				case XT_RETRY:
					if (!pb_ind_row_count && search_key) {
						if (!xt_idx_search_prev(pb_open_tab, ind, search_key))
							return ha_log_pbxt_thread_error_for_mysql(pb_ignore_dup_key);
					}
					else {
						if (!xt_idx_research(pb_open_tab, ind))
							goto failed;
					}
					break;
				default:
					return 0;
			}
		}
	}
	return HA_ERR_END_OF_FILE;

	failed:
	return ha_log_pbxt_thread_error_for_mysql(FALSE);
}

int ha_pbxt::index_init(uint idx, bool XT_UNUSED(sorted))
{
	XTIndexPtr ind;

	/* select count(*) from smalltab_PBXT;
	 * ignores the error below, and continues to
	 * call index_first!
	 */
	active_index = idx;

	if (pb_open_tab->ot_table->tab_dic.dic_disable_index) {
		xt_tab_set_index_error(pb_open_tab->ot_table);
		return ha_log_pbxt_thread_error_for_mysql(pb_ignore_dup_key);
	}

	/* The number of columns required: */
	if (pb_open_tab->ot_is_modify) {

		pb_open_tab->ot_cols_req = table->read_set->MX_BIT_SIZE();
#ifdef XT_PRINT_INDEX_OPT
		ind = (XTIndexPtr) pb_share->sh_dic_keys[idx];

		printf("index_init %s index %d cols req=%d/%d read_bits=%X write_bits=%X index_bits=%X\n", pb_open_tab->ot_table->tab_name->ps_path, (int) idx, pb_open_tab->ot_cols_req, pb_open_tab->ot_cols_req, (int) *table->read_set->bitmap, (int) *table->write_set->bitmap, (int) *ind->mi_col_map.bitmap);
#endif
	}
	else {
		pb_open_tab->ot_cols_req = ha_get_max_bit(table->read_set);

		/* Check for index coverage!
		 *
		 * Given the following table:
		 *
		 * CREATE TABLE `customer` (
		 * `c_id` int(11) NOT NULL DEFAULT '0',
		 * `c_d_id` int(11) NOT NULL DEFAULT '0',
		 * `c_w_id` int(11) NOT NULL DEFAULT '0',
		 * `c_first` varchar(16) DEFAULT NULL,
		 * `c_middle` char(2) DEFAULT NULL,
		 * `c_last` varchar(16) DEFAULT NULL,
		 * `c_street_1` varchar(20) DEFAULT NULL,
		 * `c_street_2` varchar(20) DEFAULT NULL,
		 * `c_city` varchar(20) DEFAULT NULL,
		 * `c_state` char(2) DEFAULT NULL,
		 * `c_zip` varchar(9) DEFAULT NULL,
		 * `c_phone` varchar(16) DEFAULT NULL,
		 * `c_since` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
		 * `c_credit` char(2) DEFAULT NULL,
		 * `c_credit_lim` decimal(24,12) DEFAULT NULL,
		 * `c_discount` double DEFAULT NULL,
		 * `c_balance` decimal(24,12) DEFAULT NULL,
		 * `c_ytd_payment` decimal(24,12) DEFAULT NULL,
		 * `c_payment_cnt` double DEFAULT NULL,
		 * `c_delivery_cnt` double DEFAULT NULL,
		 * `c_data` text,
		 * PRIMARY KEY (`c_w_id`,`c_d_id`,`c_id`),
		 * KEY `c_w_id` (`c_w_id`,`c_d_id`,`c_last`,`c_first`,`c_id`)
		 * ) ENGINE=PBXT;
		 *
		 * MySQL does not recognize index coverage on the followin select:
		 *
		 * SELECT c_id FROM customer WHERE c_w_id = 3 AND c_d_id = 8 AND 
		 * c_last = 'EINGATIONANTI' ORDER BY c_first ASC LIMIT 1;
		 *
		 * TODO: Find out why this is necessary, MyISAM does not
		 * seem to have this problem!
		 */
		ind = (XTIndexPtr) pb_share->sh_dic_keys[idx];
		if (MX_BIT_IS_SUBSET(table->read_set, &ind->mi_col_map))
			pb_key_read = TRUE;
#ifdef XT_PRINT_INDEX_OPT
		printf("index_init %s index %d cols req=%d/%d read_bits=%X write_bits=%X index_bits=%X converage=%d\n", pb_open_tab->ot_table->tab_name->ps_path, (int) idx, pb_open_tab->ot_cols_req, table->read_set->MX_BIT_SIZE(), (int) *table->read_set->bitmap, (int) *table->write_set->bitmap, (int) *ind->mi_col_map.bitmap, (int) (MX_BIT_IS_SUBSET(table->read_set, &ind->mi_col_map) != 0));
#endif
	}
	
	xt_xlog_check_long_writer(pb_open_tab->ot_thread);

	pb_open_tab->ot_thread->st_statistics.st_scan_index++;
	return 0;
}

int ha_pbxt::index_end()
{
	int err = 0;

	XT_TRACE_CALL();

	XTThreadPtr thread = pb_open_tab->ot_thread;

	/*
	 * the assertion below is not always held, because the sometimes handler is unlocked
	 * before this function is called
	 */
	/*ASSERT_NS(pb_ex_in_use);*/

	if (pb_open_tab->ot_ind_rhandle) {
		xt_ind_release_handle(pb_open_tab->ot_ind_rhandle, FALSE, thread);
		pb_open_tab->ot_ind_rhandle = NULL;
	}

	/*
	 * make permanent the lock for the last scanned row
	 */
	if (pb_open_tab)
		pb_open_tab->ot_table->tab_locks.xt_make_lock_permanent(pb_open_tab, &thread->st_lock_list);

	xt_xlog_check_long_writer(thread);

	active_index = MAX_KEY;
	XT_RETURN(err);
}

#ifdef XT_TRACK_RETURNED_ROWS
void ha_start_scan(XTOpenTablePtr ot, u_int index)
{
	xt_ttracef(ot->ot_thread, "SCAN %d:%d\n", (int) ot->ot_table->tab_id, (int) index);
	ot->ot_rows_ret_curr = 0;
	for (u_int i=0; i<ot->ot_rows_ret_max; i++)
		ot->ot_rows_returned[i] = 0;
}

void ha_return_row(XTOpenTablePtr ot, u_int index)
{
	xt_ttracef(ot->ot_thread, "%d:%d ROW=%d:%d\n",
		(int) ot->ot_table->tab_id, (int) index, (int) ot->ot_curr_row_id, (int) ot->ot_curr_rec_id);
	ot->ot_rows_ret_curr++;
	if (ot->ot_curr_row_id >= ot->ot_rows_ret_max) {
		if (!xt_realloc_ns((void **) &ot->ot_rows_returned, (ot->ot_curr_row_id+1) * sizeof(xtRecordID)))
			ASSERT_NS(FALSE);
		memset(&ot->ot_rows_returned[ot->ot_rows_ret_max], 0, (ot->ot_curr_row_id+1 - ot->ot_rows_ret_max) * sizeof(xtRecordID));
		ot->ot_rows_ret_max = ot->ot_curr_row_id+1;
	}
	if (!ot->ot_curr_row_id || !ot->ot_curr_rec_id || ot->ot_rows_returned[ot->ot_curr_row_id]) {
		char *sql = *thd_query(current_thd);

		xt_ttracef(ot->ot_thread, "DUP %d:%d %s\n",
			(int) ot->ot_table->tab_id, (int) index, *thd_query(current_thd));
		xt_dump_trace();
		printf("ERROR: row=%d rec=%d newr=%d, already returned!\n", (int) ot->ot_curr_row_id, (int) ot->ot_rows_returned[ot->ot_curr_row_id], (int) ot->ot_curr_rec_id);
		printf("ERROR: %s\n", sql);
#ifdef XT_WIN
		FatalAppExit(0, "Debug Me!");
#endif
	}
	else
		ot->ot_rows_returned[ot->ot_curr_row_id] = ot->ot_curr_rec_id;
}
#endif

int ha_pbxt::index_read_xt(byte * buf, uint idx, const byte *key, uint key_len, enum ha_rkey_function find_flag)
{
	int					err = 0;
	XTIndexPtr			ind;
	int					prefix = 0;
	XTIdxSearchKeyRec	search_key;

#ifdef XT_TRACK_RETURNED_ROWS
	ha_start_scan(pb_open_tab, idx);
#endif

	/* This call starts a search on this handler! */
	pb_ind_row_count = 0;

	ASSERT_NS(pb_ex_in_use);

	XT_PRINT1(pb_open_tab->ot_thread, "ha_pbxt::index_read_xt %s\n", pb_share->sh_table_path->ps_path);
	XT_DISABLED_TRACE(("search tx=%d val=%d update=%d\n", (int) pb_open_tab->ot_thread->st_xact_data->xd_start_xn_id, (int) XT_GET_DISK_4(key), pb_modified));
	ind = (XTIndexPtr) pb_share->sh_dic_keys[idx];

	switch (find_flag) {
		case HA_READ_PREFIX_LAST:
		case HA_READ_PREFIX_LAST_OR_PREV:
			prefix = SEARCH_PREFIX;
		case HA_READ_BEFORE_KEY:
		case HA_READ_KEY_OR_PREV: // I assume you want to be positioned on the last entry in the key duplicate list!! 
			xt_idx_prep_key(ind, &search_key, ((find_flag == HA_READ_BEFORE_KEY) ? 0 : XT_SEARCH_AFTER_KEY) | prefix, (xtWord1 *) key, (size_t) key_len);
			if (!xt_idx_search_prev(pb_open_tab, ind, &search_key))
				err = ha_log_pbxt_thread_error_for_mysql(pb_ignore_dup_key);
			else
				err = xt_index_prev_read(pb_open_tab, ind, pb_key_read,
					(find_flag == HA_READ_PREFIX_LAST) ? &search_key : NULL, buf);
			break;
		case HA_READ_PREFIX:
			prefix = SEARCH_PREFIX;
		case HA_READ_KEY_EXACT:
		case HA_READ_KEY_OR_NEXT:
		case HA_READ_AFTER_KEY:
		default:
			xt_idx_prep_key(ind, &search_key, ((find_flag == HA_READ_AFTER_KEY) ? XT_SEARCH_AFTER_KEY : 0) | prefix, (xtWord1 *) key, key_len);
			if (!xt_idx_search(pb_open_tab, ind, &search_key))
				err = ha_log_pbxt_thread_error_for_mysql(pb_ignore_dup_key);
			else {
				err = xt_index_next_read(pb_open_tab, ind, pb_key_read,
					(find_flag == HA_READ_KEY_EXACT || find_flag == HA_READ_PREFIX) ? &search_key : NULL, buf);
				if (err == HA_ERR_END_OF_FILE && find_flag == HA_READ_AFTER_KEY)
					err = HA_ERR_KEY_NOT_FOUND;			
			}
			break;
	}

	pb_ind_row_count++;
#ifdef XT_TRACK_RETURNED_ROWS
	if (!err)
		ha_return_row(pb_open_tab, idx);
#endif
	XT_DISABLED_TRACE(("search tx=%d val=%d err=%d\n", (int) pb_open_tab->ot_thread->st_xact_data->xd_start_xn_id, (int) XT_GET_DISK_4(key), err));
	if (err)
		table->status = STATUS_NOT_FOUND;
	else {
		pb_open_tab->ot_thread->st_statistics.st_row_select++;
		table->status = 0;
	}
	return err;
}

/*
 * Positions an index cursor to the index specified in the handle. Fetches the
 * row if available. If the key value is null, begin at the first key of the
 * index.
 */
int ha_pbxt::index_read(byte * buf, const byte * key, uint key_len, enum ha_rkey_function find_flag)
{
	//statistic_increment(ha_read_key_count,&LOCK_status);
	return index_read_xt(buf, active_index, key, key_len, find_flag);
}

int ha_pbxt::index_read_idx(byte * buf, uint idx, const byte *key, uint key_len, enum ha_rkey_function find_flag)
{
	//statistic_increment(ha_read_key_count,&LOCK_status);
	return index_read_xt(buf, idx, key, key_len, find_flag);
}

int ha_pbxt::index_read_last(byte * buf, const byte * key, uint key_len)
{
	//statistic_increment(ha_read_key_count,&LOCK_status);
	return index_read_xt(buf, active_index, key, key_len, HA_READ_PREFIX_LAST);
}

/*
 * Used to read forward through the index.
 */
int ha_pbxt::index_next(byte * buf)
{
	int			err = 0;
	XTIndexPtr	ind;

	XT_TRACE_CALL();
	//statistic_increment(ha_read_next_count,&LOCK_status);
	ASSERT_NS(pb_ex_in_use);

	ind = (XTIndexPtr) pb_share->sh_dic_keys[active_index];

	if (!xt_idx_next(pb_open_tab, ind, NULL))
		err = ha_log_pbxt_thread_error_for_mysql(pb_ignore_dup_key);
	else
		err = xt_index_next_read(pb_open_tab, ind, pb_key_read, NULL, buf);

	pb_ind_row_count++;
#ifdef XT_TRACK_RETURNED_ROWS
	if (!err)
		ha_return_row(pb_open_tab, active_index);
#endif
	if (err)
		table->status = STATUS_NOT_FOUND;
	else {
		pb_open_tab->ot_thread->st_statistics.st_row_select++;
		table->status = 0;
	}
	XT_RETURN(err);
}

/*
 * I have implemented this because there is currently a
 * bug in handler::index_next_same().
 *
 * drop table if exists t1;
 * CREATE TABLE t1 (a int, b int, primary key(a,b))
 * PARTITION BY KEY(b,a) PARTITIONS 2;
 * insert into t1 values (0,0),(1,1),(2,2),(3,3),(4,4),(5,5),(6,6);
 * select * from t1 where a = 4;
 * 
 */
int ha_pbxt::index_next_same(byte * buf, const byte *key, uint length)
{
	int					err = 0;
	XTIndexPtr			ind;
	XTIdxSearchKeyRec	search_key;

	XT_TRACE_CALL();
	//statistic_increment(ha_read_next_count,&LOCK_status);
	ASSERT_NS(pb_ex_in_use);

	ind = (XTIndexPtr) pb_share->sh_dic_keys[active_index];

	search_key.sk_key_value.sv_flags = HA_READ_KEY_EXACT;
	search_key.sk_key_value.sv_rec_id = 0;
	search_key.sk_key_value.sv_row_id = 0;
	search_key.sk_key_value.sv_key = search_key.sk_key_buf;
	search_key.sk_key_value.sv_length = myxt_create_key_from_key(ind, search_key.sk_key_buf, (xtWord1 *) key, (u_int) length);
	search_key.sk_on_key = TRUE;

	if (!xt_idx_next(pb_open_tab, ind, &search_key))
		err = ha_log_pbxt_thread_error_for_mysql(pb_ignore_dup_key);
	else
		err = xt_index_next_read(pb_open_tab, ind, pb_key_read, &search_key, buf);

	pb_ind_row_count++;
#ifdef XT_TRACK_RETURNED_ROWS
	if (!err)
		ha_return_row(pb_open_tab, active_index);
#endif
	if (err)
		table->status = STATUS_NOT_FOUND;
	else {
		pb_open_tab->ot_thread->st_statistics.st_row_select++;
		table->status = 0;
	}
	XT_RETURN(err);
}

/*
 * Used to read backwards through the index.
 */
int ha_pbxt::index_prev(byte * buf)
{
	int			err = 0;
	XTIndexPtr	ind;

	XT_TRACE_CALL();
	//statistic_increment(ha_read_prev_count,&LOCK_status);
	ASSERT_NS(pb_ex_in_use);

	ind = (XTIndexPtr) pb_share->sh_dic_keys[active_index];

	if (!xt_idx_prev(pb_open_tab, ind, NULL))
		err = ha_log_pbxt_thread_error_for_mysql(pb_ignore_dup_key);
	else
		err = xt_index_prev_read(pb_open_tab, ind, pb_key_read, NULL, buf);

	pb_ind_row_count++;
#ifdef XT_TRACK_RETURNED_ROWS
	if (!err)
		ha_return_row(pb_open_tab, active_index);
#endif
	if (err)
		table->status = STATUS_NOT_FOUND;
	else {
		pb_open_tab->ot_thread->st_statistics.st_row_select++;
		table->status = 0;
	}
	XT_RETURN(err);
}

/*
 * index_first() asks for the first key in the index.
 */
int ha_pbxt::index_first(byte * buf)
{
	int					err = 0;
	XTIndexPtr			ind;
	XTIdxSearchKeyRec	search_key;

	XT_TRACE_CALL();
	//statistic_increment(ha_read_first_count,&LOCK_status);
	ASSERT_NS(pb_ex_in_use);

#ifdef XT_TRACK_RETURNED_ROWS
	ha_start_scan(pb_open_tab, active_index);
#endif
	pb_ind_row_count = 0;

	ind = (XTIndexPtr) pb_share->sh_dic_keys[active_index];

	xt_idx_prep_key(ind, &search_key, XT_SEARCH_FIRST_FLAG, NULL, 0);
	if (!xt_idx_search(pb_open_tab, ind, &search_key))
		err = ha_log_pbxt_thread_error_for_mysql(pb_ignore_dup_key);
	else
		err = xt_index_next_read(pb_open_tab, ind, pb_key_read, NULL, buf);

	pb_ind_row_count++;
#ifdef XT_TRACK_RETURNED_ROWS
	if (!err)
		ha_return_row(pb_open_tab, active_index);
#endif
	if (err)
		table->status = STATUS_NOT_FOUND;
	else {
		pb_open_tab->ot_thread->st_statistics.st_row_select++;
		table->status = 0;
	}
	XT_RETURN(err);
}

/*
 * index_last() asks for the last key in the index.
 */
int ha_pbxt::index_last(byte * buf)
{
	int					err = 0;
	XTIndexPtr			ind;
	XTIdxSearchKeyRec	search_key;

	XT_TRACE_CALL();
	//statistic_increment(ha_read_last_count,&LOCK_status);
	ASSERT_NS(pb_ex_in_use);

#ifdef XT_TRACK_RETURNED_ROWS
	ha_start_scan(pb_open_tab, active_index);
#endif
	pb_ind_row_count = 0;

	ind = (XTIndexPtr) pb_share->sh_dic_keys[active_index];

	xt_idx_prep_key(ind, &search_key, XT_SEARCH_AFTER_LAST_FLAG, NULL, 0);
	if (!xt_idx_search_prev(pb_open_tab, ind, &search_key))
		err = ha_log_pbxt_thread_error_for_mysql(pb_ignore_dup_key);
	else
		err = xt_index_prev_read(pb_open_tab, ind, pb_key_read, NULL, buf);

	pb_ind_row_count++;
#ifdef XT_TRACK_RETURNED_ROWS
	if (!err)
		ha_return_row(pb_open_tab, active_index);
#endif
	if (err)
		table->status = STATUS_NOT_FOUND;
	else {
		pb_open_tab->ot_thread->st_statistics.st_row_select++;
		table->status = 0;
	}
	XT_RETURN(err);
}

/*
 * -----------------------------------------------------------------------
 * RAMDOM/SEQUENTIAL READ METHODS
 */
 
/*
 * rnd_init() is called when the system wants the storage engine to do a table
 * scan.
 * See the example in the introduction at the top of this file to see when
 * rnd_init() is called.
 *
 * Called from filesort.cc, records.cc, sql_handler.cc, sql_select.cc, sql_table.cc,
 * and sql_update.cc.
 */
int ha_pbxt::rnd_init(bool scan)
{
	int err = 0;

	XT_PRINT1(pb_open_tab->ot_thread, "ha_pbxt::rnd_init %s\n", pb_share->sh_table_path->ps_path);
	XT_DISABLED_TRACE(("seq scan tx=%d\n", (int) pb_open_tab->ot_thread->st_xact_data->xd_start_xn_id));

	/* Call xt_tab_seq_exit() to make sure the resources used by the previous
	 * scan are freed. In particular make sure cache page ref count is decremented.
	 * This is needed as rnd_init() can be called mulitple times w/o matching calls 
	 * to rnd_end(). Our experience is that currently this is done in queries like:
	 *
	 * SELECT t1.c1,t2.c1 FROM t1 LEFT JOIN t2 USING (c1);
	 * UPDATE t1 LEFT JOIN t2 USING (c1) SET t1.c1 = t2.c1 WHERE t1.c1 = t2.c1;
	 *
	 * when scanning inner tables. It is important to understand that in such case
	 * multiple calls to rnd_init() are not semantically equal to a new query. For
	 * example we cannot make row locks permanent as we do in rnd_end(), as 
	 * ha_pbxt::unlock_row still can be called.
	 */
	xt_tab_seq_exit(pb_open_tab);

	/* The number of columns required: */
	if (pb_open_tab->ot_is_modify)
		pb_open_tab->ot_cols_req = table->read_set->MX_BIT_SIZE();
	else {
		pb_open_tab->ot_cols_req = ha_get_max_bit(table->read_set);

		/*
		 * in case of queries like SELECT COUNT(*) FROM t
		 * table->read_set is empty. Otoh, ot_cols_req == 0 can be treated
		 * as "all columns" by some internal code (see e.g. myxt_load_row), 
		 * which makes such queries very ineffective for the records with 
		 * extended part. Setting column count to 1 makes sure that the 
		 * extended part will not be acessed in most cases.
		 */

		if (pb_open_tab->ot_cols_req == 0)
			pb_open_tab->ot_cols_req = 1;
	}

	ASSERT_NS(pb_ex_in_use);
	if (scan) {
		if (!xt_tab_seq_init(pb_open_tab))
			err = ha_log_pbxt_thread_error_for_mysql(pb_ignore_dup_key);
	}
	else
		xt_tab_seq_reset(pb_open_tab);

	xt_xlog_check_long_writer(pb_open_tab->ot_thread);

	return err;
}

int ha_pbxt::rnd_end()
{
	XT_TRACE_CALL();

	/*
	 * make permanent the lock for the last scanned row
	 */
	XTThreadPtr thread = pb_open_tab->ot_thread;
	if (pb_open_tab)
		pb_open_tab->ot_table->tab_locks.xt_make_lock_permanent(pb_open_tab, &thread->st_lock_list);

	xt_xlog_check_long_writer(thread);

	xt_tab_seq_exit(pb_open_tab);
	XT_RETURN(0);
}

/*
 * This is called for each row of the table scan. When you run out of records
 * you should return HA_ERR_END_OF_FILE. Fill buff up with the row information.
 * The Field structure for the table is the key to getting data into buf
 * in a manner that will allow the server to understand it.
 *
 * Called from filesort.cc, records.cc, sql_handler.cc, sql_select.cc, sql_table.cc,
 * and sql_update.cc.
 */
int ha_pbxt::rnd_next(byte *buf)
{
	int		err = 0;
	xtBool	eof;

	XT_TRACE_CALL();
	ASSERT_NS(pb_ex_in_use);
	//statistic_increment(ha_read_rnd_next_count, &LOCK_status);
	xt_xlog_check_long_writer(pb_open_tab->ot_thread);

	if (!xt_tab_seq_next(pb_open_tab, (xtWord1 *) buf, &eof))
		err = ha_log_pbxt_thread_error_for_mysql(pb_ignore_dup_key);
	else if (eof)
		err = HA_ERR_END_OF_FILE;

	if (err)
		table->status = STATUS_NOT_FOUND;
	else {
		pb_open_tab->ot_thread->st_statistics.st_row_select++;
		table->status = 0;
	}
	XT_RETURN(err);
}

/*
 * position() is called after each call to rnd_next() if the data needs
 * to be ordered. You can do something like the following to store
 * the position:
 * ha_store_ptr(ref, ref_length, current_position);
 *
 * The server uses ref to store data. ref_length in the above case is
 * the size needed to store current_position. ref is just a byte array
 * that the server will maintain. If you are using offsets to mark rows, then
 * current_position should be the offset. If it is a primary key like in
 * BDB, then it needs to be a primary key.
 *
 * Called from filesort.cc, sql_select.cc, sql_delete.cc and sql_update.cc.
 */
void ha_pbxt::position(const byte *XT_UNUSED(record))
{
	XT_TRACE_CALL();
	ASSERT_NS(pb_ex_in_use);
	/*
	 * I changed this from using little endian to big endian.
	 *
	 * The reason is because sometime the pointer are sorted.
	 * When they are are sorted a binary compare is used.
	 * A binary compare sorts big endian values correctly!
	 *
	 * Take the followin example:
	 *
	 * create table t1 (a int, b text);
	 * insert into t1 values (1, 'aa'), (1, 'bb'), (1, 'cc');
	 * select group_concat(b) from t1 group by a;
	 *
	 * With little endian pointers the result is:
	 * aa,bb,cc
	 *
	 * With big-endian pointer the result is:
	 * aa,cc,bb
	 *
	 */
	(void) ASSERT_NS(XT_RECORD_OFFS_SIZE == 4);
	mi_int4store((xtWord1 *) ref, pb_open_tab->ot_curr_rec_id);
	XT_RETURN_VOID;
}

/*
 * Given the #ROWID retrieve the record.
 *
 * Called from filesort.cc records.cc sql_insert.cc sql_select.cc sql_update.cc.
 */
int ha_pbxt::rnd_pos(byte * buf, byte *pos)
{
	int err = 0;

	XT_TRACE_CALL();
	ASSERT_NS(pb_ex_in_use);
	//statistic_increment(ha_read_rnd_count, &LOCK_status);
	XT_PRINT1(pb_open_tab->ot_thread, "ha_pbxt::rnd_pos %s\n", pb_share->sh_table_path->ps_path);

	pb_open_tab->ot_curr_rec_id = mi_uint4korr((xtWord1 *) pos);
	switch (xt_tab_dirty_read_record(pb_open_tab, (xtWord1 *) buf)) {
		case FALSE:
			err = ha_log_pbxt_thread_error_for_mysql(pb_ignore_dup_key);
			break;
		default:
			break;
	}		

	if (err)
		table->status = STATUS_NOT_FOUND;
	else {
		pb_open_tab->ot_thread->st_statistics.st_row_select++;
		table->status = 0;
	}
	XT_RETURN(err);
}

/*
 * -----------------------------------------------------------------------
 * INFO METHODS
 */
 
/*
	::info() is used to return information to the optimizer.
	Currently this table handler doesn't implement most of the fields
	really needed. SHOW also makes use of this data
	Another note, you will probably want to have the following in your
	code:
	if (records < 2)
		records = 2;
	The reason is that the server will optimize for cases of only a single
	record. If in a table scan you don't know the number of records
	it will probably be better to set records to two so you can return
	as many records as you need.
	Along with records a few more variables you may wish to set are:
		records
		deleted
		data_file_length
		index_file_length
		delete_length
		check_time
	Take a look at the public variables in handler.h for more information.

	Called in:
		filesort.cc
		ha_heap.cc
		item_sum.cc
		opt_sum.cc
		sql_delete.cc
		sql_delete.cc
		sql_derived.cc
		sql_select.cc
		sql_select.cc
		sql_select.cc
		sql_select.cc
		sql_select.cc
		sql_show.cc
		sql_show.cc
		sql_show.cc
		sql_show.cc
		sql_table.cc
		sql_union.cc
		sql_update.cc

*/
#if MYSQL_VERSION_ID < 50114
void ha_pbxt::info(uint flag)
#else
int ha_pbxt::info(uint flag)
#endif
{
	XTOpenTablePtr	ot;
	int				in_use;

	XT_TRACE_CALL();
	
	if (!(in_use = pb_ex_in_use)) {
		pb_ex_in_use = 1;
		if (pb_share && pb_share->sh_table_lock) {
			/* If some thread has an exclusive lock, then
			 * we wait for the lock to be removed:
			 */
#if MYSQL_VERSION_ID < 50114
			ha_wait_for_shared_use(this, pb_share);
			pb_ex_in_use = 1;
#else
			if (!ha_wait_for_shared_use(this, pb_share))
				return ha_log_pbxt_thread_error_for_mysql(pb_ignore_dup_key);
#endif
		}
	}

	if ((ot = pb_open_tab)) {
		if (flag & HA_STATUS_VARIABLE) {
			stats.deleted = ot->ot_table->tab_row_fnum;
			stats.records = (ha_rows) (ot->ot_table->tab_row_eof_id - 1 - stats.deleted);
			stats.data_file_length = xt_rec_id_to_rec_offset(ot->ot_table, ot->ot_table->tab_rec_eof_id);
			stats.index_file_length = xt_ind_node_to_offset(ot->ot_table, ot->ot_table->tab_ind_eof);
			stats.delete_length = ot->ot_table->tab_rec_fnum * ot->ot_rec_size;
			//check_time = info.check_time;
			stats.mean_rec_length = ot->ot_rec_size;
		}

		if (flag & HA_STATUS_CONST) {
			ha_rows		rec_per_key;
			XTIndexPtr	ind;
			TABLE_SHARE	*share= TS(table);

			stats.max_data_file_length = 0x00FFFFFF;
			stats.max_index_file_length = 0x00FFFFFF;
			//stats.create_time = info.create_time;
			ref_length = XT_RECORD_OFFS_SIZE;
			//share->db_options_in_use = info.options;
			stats.block_size = XT_INDEX_PAGE_SIZE;

			if (share->tmp_table == NO_TMP_TABLE)
#if MYSQL_VERSION_ID > 60005
#define WHICH_MUTEX			LOCK_ha_data
#else
#define WHICH_MUTEX			mutex
#endif

#ifdef SAFE_MUTEX

#if MYSQL_VERSION_ID < 60000
#if MYSQL_VERSION_ID < 50123
				safe_mutex_lock(&share->mutex,__FILE__,__LINE__);
#else
				safe_mutex_lock(&share->mutex,0,__FILE__,__LINE__);
#endif
#else
#if MYSQL_VERSION_ID < 60004
				safe_mutex_lock(&share->mutex,__FILE__,__LINE__);
#else
				safe_mutex_lock(&share->WHICH_MUTEX,0,__FILE__,__LINE__);
#endif
#endif

#else // SAFE_MUTEX

#ifdef MY_PTHREAD_FASTMUTEX
				my_pthread_fastmutex_lock(&share->WHICH_MUTEX);
#else
				pthread_mutex_lock(&share->WHICH_MUTEX);
#endif

#endif // SAFE_MUTEX
#ifdef DRIZZLED
			set_prefix(share->keys_in_use, share->keys);
			share->keys_for_keyread&= share->keys_in_use;
#else
			share->keys_in_use.set_prefix(share->keys);
			//share->keys_in_use.intersect_extended(info.key_map);
			share->keys_for_keyread.intersect(share->keys_in_use);
			//share->db_record_offset = info.record_offset;
#endif
			for (u_int i = 0; i < share->keys; i++) {
				ind = pb_share->sh_dic_keys[i];

				rec_per_key = 0;
				if (ind->mi_seg_count == 1 && (ind->mi_flags & HA_NOSAME))
					rec_per_key = 1;
				else {
					rec_per_key = 1;	
				}
				for (u_int j = 0; j < table->key_info[i].key_parts; j++)
	 				table->key_info[i].rec_per_key[j] = (ulong) rec_per_key;
			}
			if (share->tmp_table == NO_TMP_TABLE)
#ifdef SAFE_MUTEX
				safe_mutex_unlock(&share->WHICH_MUTEX,__FILE__,__LINE__);
#else
#ifdef MY_PTHREAD_FASTMUTEX
				pthread_mutex_unlock(&share->WHICH_MUTEX.mutex);
#else
				pthread_mutex_unlock(&share->WHICH_MUTEX);
#endif
#endif
	  		/*
			 Set data_file_name and index_file_name to point at the symlink value
			 if table is symlinked (Ie;  Real name is not same as generated name)
	   		*/
	   		/*
			data_file_name = index_file_name = 0;
			fn_format(name_buff, file->filename, "", MI_NAME_DEXT, 2);
			if (strcmp(name_buff, info.data_file_name))
				data_file_name = info.data_file_name;
			strmov(fn_ext(name_buff), MI_NAME_IEXT);
			if (strcmp(name_buff, info.index_file_name))
				index_file_name = info.index_file_name;
			*/
		}

 		if (flag & HA_STATUS_ERRKEY)
	 		errkey = ot->ot_err_index_no;

		/* {PRE-INC}
		 * We assume they want the next value to be returned!
		 *
		 * At least, this is what works for the following code:
		 *
		 * create table t1 (a int auto_increment primary key)
		 * auto_increment=100
		 * engine=pbxt
		 * partition by list (a)
		 * (partition p0 values in (1, 98,99, 100, 101));
		 * create index inx on t1 (a);
		 * insert into t1 values (null);
		 * select * from t1;
		 */
		if (flag & HA_STATUS_AUTO)
			stats.auto_increment_value = (ulonglong) ot->ot_table->tab_auto_inc+1;
	}
	else
		errkey = (uint) -1;

	if (!in_use) {
		pb_ex_in_use = 0;
		if (pb_share) {
			/* Someone may be waiting for me to complete: */
			if (pb_share->sh_table_lock)
				xt_broadcast_cond_ns((xt_cond_type *) pb_share->sh_ex_cond);
		}
	}
#if MYSQL_VERSION_ID < 50114
	XT_RETURN_VOID;
#else
	XT_RETURN(0);
#endif
}

/*
 * extra() is called whenever the server wishes to send a hint to
 * the storage engine. The myisam engine implements the most hints.
 * ha_innodb.cc has the most exhaustive list of these hints.
 */
int ha_pbxt::extra(enum ha_extra_function operation)
{
	int err = 0;

	XT_PRINT2(xt_get_self(), "ha_pbxt::extra %s  operation=%d\n", pb_share->sh_table_path->ps_path, operation);

	switch (operation) {
		case HA_EXTRA_RESET_STATE:
			pb_key_read = FALSE;
			pb_ignore_dup_key = 0;
			/* As far as I can tell, this function is called for
			 * every table at the end of a statement.
			 *
			 * So, during a LOCK TABLES ... UNLOCK TABLES, I use
			 * this to find the end of a statement.
			 * start_stmt() indicates the start of a statement,
			 * and is also called once for each table in the
			 * statement.
			 *
			 * So the statement boundary is indicated by 
			 * self->st_stat_count == 0
			 *
			 * GOTCHA: I cannot end the transaction here!
			 * I must end it in start_stmt().
			 * The reason is because there are situations
			 * where this would end a transaction that
			 * was begin by external_lock().
			 *
			 * An example of this is when a function
			 * is called when doing CREATE TABLE SELECT.
			 */
			if (pb_in_stat) {
				/* NOTE: pb_in_stat is just used to avoid getting
				 * self, if it is not necessary!!
				 */
				XTThreadPtr self;

				pb_in_stat = FALSE;

				if (!(self = ha_set_current_thread(pb_mysql_thd, &err)))
					return xt_ha_pbxt_to_mysql_error(err);

				if (self->st_stat_count > 0) {
					self->st_stat_count--;
					if (self->st_stat_count == 0)
						self->st_stat_ended = TRUE;
				}

				/* This is the end of a statement, I can turn any locks into perminant locks now: */
				if (pb_open_tab)
					pb_open_tab->ot_table->tab_locks.xt_make_lock_permanent(pb_open_tab, &self->st_lock_list);
			}
			if (pb_open_tab)
				pb_open_tab->ot_for_update = 0;
			break;
		case HA_EXTRA_KEYREAD:
			/* This means we so not need to read the entire record. */
			pb_key_read = TRUE;
			break;
		case HA_EXTRA_NO_KEYREAD:
			pb_key_read = FALSE;
			break;
		case HA_EXTRA_IGNORE_DUP_KEY:
			/* NOTE!!! Calls to extra(HA_EXTRA_IGNORE_DUP_KEY) can be nested!
			 * In fact, the calls are from different threads, so
			 * strictly speaking I should protect this variable!!
			 * Here is the sequence that produces the duplicate call:
			 *
			 * drop table if exists t1;
			 * CREATE TABLE t1 (x int not null, y int, primary key (x)) engine=pbxt;
			 * insert into t1 values (1, 3), (4, 1);
			 * replace DELAYED into t1 (x, y) VALUES (4, 2);
			 * select * from t1 order by x;
			 *
			 */
			pb_ignore_dup_key++;
			break;
		case HA_EXTRA_NO_IGNORE_DUP_KEY:
			pb_ignore_dup_key--;
			break;
		case HA_EXTRA_KEYREAD_PRESERVE_FIELDS:
			/* MySQL needs all fields */
			pb_key_read = FALSE;
			break;
		default:
			break;
	}

	return err;
}


/*
 * Deprecated and likely to be removed in the future. Storage engines normally
 * just make a call like:
 * ha_pbxt::extra(HA_EXTRA_RESET);
 * to handle it.
 */
int ha_pbxt::reset(void)
{
	XT_TRACE_CALL();
	extra(HA_EXTRA_RESET_STATE);
	XT_RETURN(0);
}

void ha_pbxt::unlock_row()
{
	XT_TRACE_CALL();
	if (pb_open_tab)
		pb_open_tab->ot_table->tab_locks.xt_remove_temp_lock(pb_open_tab, FALSE);
}

/*
 * Used to delete all rows in a table. Both for cases of truncate and
 * for cases where the optimizer realizes that all rows will be
 * removed as a result of a SQL statement.
 *
 * Called from item_sum.cc by Item_func_group_concat::clear(),
 * Item_sum_count_distinct::clear(), and Item_func_group_concat::clear().
 * Called from sql_delete.cc by mysql_delete().
 * Called from sql_select.cc by JOIN::reinit().
 * Called from sql_union.cc by st_select_lex_unit::exec().
 */
int ha_pbxt::delete_all_rows()
{
	THD				*thd = current_thd;
	int				err = 0;
	XTThreadPtr		self;
	XTDDTable		*tab_def = NULL;
	char			path[PATH_MAX];

	XT_TRACE_CALL();

	if (thd_sql_command(thd) != SQLCOM_TRUNCATE) {
		/* Just like InnoDB we only handle TRUNCATE TABLE
		 * by recreating the table.
		 * DELETE FROM t must be handled by deleting
		 * each row because it may be part of a transaction,
		 * and there may be foreign key actions.
		 */
		XT_RETURN (my_errno = HA_ERR_WRONG_COMMAND);
	}

	if (!(self = ha_set_current_thread(thd, &err)))
		return xt_ha_pbxt_to_mysql_error(err);

	try_(a) {
		XTDictionaryRec dic;

		memset(&dic, 0, sizeof(dic));

		dic = pb_share->sh_table->tab_dic;
		xt_strcpy(PATH_MAX, path, pb_share->sh_table->tab_name->ps_path);

		if ((tab_def = dic.dic_table))
			tab_def->reference();

		if (!(thd_test_options(thd,OPTION_NO_FOREIGN_KEY_CHECKS)))
			tab_def->deleteAllRows(self);

		/* We should have a table lock! */
		//ASSERT(pb_lock_table);
		if (!pb_table_locked) {
			ha_aquire_exclusive_use(self, pb_share, this);
			pushr_(ha_release_exclusive_use, pb_share);
		}
		ha_close_open_tables(self, pb_share, NULL);

		/* This is required in the case of delete_all_rows, because we must
		 * ensure that the handlers no longer reference the old
		 * table, so that it will not be used again. The table
		 * must be re-openned, because the ID has changed!
		 *
		 * 0.9.86+ Must check if this is still necessary.
		 *
		 * the ha_close_share(self, pb_share) call was moved from above
		 * (before tab_def = dic.dic_table), because of a crash.
		 * Test case:
		 *
		 * set storage_engine = pbxt;
		 * create table t1 (s1 int primary key);
		 * insert into t1 values (1);
		 * create table t2 (s1 int, foreign key (s1) references t1 (s1));
		 * insert into t2 values (1); 
		 * truncate table t1; -- this should fail because of FK constraint
		 * alter table t1 engine = myisam; -- this caused crash
		 *
		 */
		ha_close_share(self, pb_share);

		/* MySQL documentation requires us to reset auto increment value to 1
		 * on truncate even if the table was created with a different value. 
		 * This is also consistent with other engines.
		 */
		dic.dic_min_auto_inc = 1;

		xt_create_table(self, (XTPathStrPtr) path, &dic);
		if (!pb_table_locked)
			freer_(); // ha_release_exclusive_use(pb_share)
	}
	catch_(a) {
		err = xt_ha_pbxt_thread_error_for_mysql(thd, self, pb_ignore_dup_key);
	}
	cont_(a);

	if (tab_def)
		tab_def->release(self);

	XT_RETURN(err);
}

/*
 * TODO: Implement!
 * Assuming a key (a,b,c)
 * 
 * rec_per_key[0] = SELECT COUNT(*)/COUNT(DISTINCT a) FROM t;
 * rec_per_key[1] = SELECT COUNT(*)/COUNT(DISTINCT a,b) FROM t;
 * rec_per_key[2] = SELECT COUNT(*)/COUNT(DISTINCT a,b,c) FROM t;
 *
 * After this is implemented, the selectivity can serve as
 * a quick estimate of records_in_range().
 *
 * After you have done this, you need to redo the index_merge*
 * tests. Restore the standard result to check if we
 * now agree with the MyISAM strategy.
 * 
 */
int ha_pbxt::analyze(THD *thd, HA_CHECK_OPT *XT_UNUSED(check_opt))
{
	int				err = 0;
	XTDatabaseHPtr	db;
	xtXactID		my_xn_id;
	xtXactID		clean_xn_id = 0;
	uint			cnt = 10;

	XT_TRACE_CALL();

	if (!pb_open_tab) {
		if ((err = reopen()))
			XT_RETURN(err);
	}

	/* Wait until the sweeper is no longer busy!
	 * If you want an accurate count(*) value, then call
	 * ANALYZE TABLE first. This function waits until the
	 * sweeper has completed.
	 */
	db = pb_open_tab->ot_table->tab_db;
	
	/*
	 * Wait until everything is cleaned up before this transaction.
	 * But this will only work if the we quit out transaction!
	 *
	 * GOTCHA: When a PBXT table is partitioned, then analyze() is
	 * called for each component. The first calls xt_xn_commit().
	 * All following calls have no transaction!:
	 *
	 * CREATE TABLE t1 (a int)
	 * PARTITION BY LIST (a)
	 * (PARTITION x1 VALUES IN (10), PARTITION x2 VALUES IN (20));
	 * 
	 * analyze table t1;
	 * 
	 */
	if (pb_open_tab->ot_thread && pb_open_tab->ot_thread->st_xact_data) {
		my_xn_id = pb_open_tab->ot_thread->st_xact_data->xd_start_xn_id;
		XT_PRINT0(xt_get_self(), "xt_xn_commit\n");
		xt_xn_commit(pb_open_tab->ot_thread);
	}
	else
		my_xn_id = db->db_xn_to_clean_id;

	while ((!db->db_sw_idle || xt_xn_is_before(db->db_xn_to_clean_id, my_xn_id)) && !thd_killed(thd)) {
		xt_busy_wait();

		/*
		 * It is possible that the sweeper gets stuck because
		 * it has no dictionary information!
		 * As in the example below.
		 *
		 * create table t4 (
		 *   pk_col int auto_increment primary key, a1 char(64), a2 char(64), b char(16), c char(16) not null, d char(16), dummy char(64) default ' '
		 * ) engine=pbxt;
		 *
		 * insert into t4 (a1, a2, b, c, d, dummy) select * from t1;
		 * 
		 * create index idx12672_0 on t4 (a1);
		 * create index idx12672_1 on t4 (a1,a2,b,c);
		 * create index idx12672_2 on t4 (a1,a2,b);
		 * analyze table t1;
		 */
		if (db->db_sw_idle) {
			/* This will make sure we don't wait forever: */
			if (clean_xn_id != db->db_xn_to_clean_id) {
				clean_xn_id = db->db_xn_to_clean_id;
				cnt = 10;
			}
			else {
				cnt--;
				if (!cnt)
					break;
			}
			xt_wakeup_sweeper(db);
		}
	}

	XT_RETURN(err);
}

int ha_pbxt::repair(THD *XT_UNUSED(thd), HA_CHECK_OPT *XT_UNUSED(check_opt))
{
	return(HA_ADMIN_TRY_ALTER);
}

/*
 * This is mapped to "ALTER TABLE tablename TYPE=PBXT", which rebuilds
 * the table in MySQL.
 */
int ha_pbxt::optimize(THD *XT_UNUSED(thd), HA_CHECK_OPT *XT_UNUSED(check_opt))
{
	return(HA_ADMIN_TRY_ALTER);
}

#ifdef DEBUG
extern int pbxt_mysql_trace_on;
#endif

int ha_pbxt::check(THD* thd, HA_CHECK_OPT* XT_UNUSED(check_opt))
{
	int				err = 0;
	XTThreadPtr		self;

	if (!(self = ha_set_current_thread(thd, &err)))
		return xt_ha_pbxt_to_mysql_error(err);
	if (self->st_lock_count)
		ASSERT(self->st_xact_data);

	if (!pb_table_locked) {
		ha_aquire_exclusive_use(self, pb_share, this);
		pushr_(ha_release_exclusive_use, pb_share);
	}

#ifdef CHECK_TABLE_LOADS
	xt_tab_load_table(self, pb_open_tab);
#endif
	xt_check_table(self, pb_open_tab);

	if (!pb_table_locked)
		freer_(); // ha_release_exclusive_use(pb_share)

	//pbxt_mysql_trace_on = TRUE;
	return 0;
}

/*
 * This function is called:
 * For each table in LOCK TABLES,
 * OR
 * For each table in a statement.
 *
 * It is called with F_UNLCK:
 * in UNLOCK TABLES
 * OR
 * at the end of a statement.
 *
 */
xtPublic int ha_pbxt::external_lock(THD *thd, int lock_type)
{
	int				err = 0;
	XTThreadPtr		self;
	
	if (!(self = ha_set_current_thread(thd, &err)))
		return xt_ha_pbxt_to_mysql_error(err);

	/* F_UNLCK is set when this function is called at end
	 * of statement or UNLOCK TABLES
	 */
	if (lock_type == F_UNLCK) {
		/* This is not TRUE if external_lock() FAILED!
		 * Can we rely on external_unlock being called when
		 * external_lock() fails? Currently yes, but it does
		 * not make sense!
		ASSERT_NS(pb_ex_in_use);
		*/

		XT_PRINT1(self, "ha_pbxt::EXTERNAL_LOCK %s lock_type=UNLOCK\n", pb_share->sh_table_path->ps_path);

		/* Make any temporary locks on this table permanent.
		 *
		 * This is required here because of the following example:
		 * create table t1 (a int NOT NULL, b int, primary key (a));
		 * create table t2 (a int NOT NULL, b int, primary key (a));
		 * insert into t1 values (0, 10),(1, 11),(2, 12);
		 * insert into t2 values (1, 21),(2, 22),(3, 23);
		 * update t1 set b= (select b from t2 where t1.a = t2.a);
		 * update t1 set b= (select b from t2 where t1.a = t2.a);
		 * select * from t1;
		 * drop table t1, t2;
		 *
		 */

		/* GOTCHA! It's weird, but, if this function returns an error
		 * on lock, then UNLOCK is called?!
		 * This should not be done, because if lock fails, it should be
		 * assumed that no UNLOCK is required.
		 * Basically, I have to assume that some code will presume this,
		 * although the function lock_external() calls unlock, even
		 * when lock fails.
		 * The result is, that my lock count can go wrong. So I could
		 * change the lock method, and increment the lock count, even
		 * if it fails. However, the consequences are more serious,
		 * if some code decides not to call UNLOCK after lock fails.
		 * The result is that I would have a permanent too high lock,
		 * count and nothing will work.
		 * So instead, I handle the fact that I might too many unlocks
		 * here.
		 */
		if (self->st_lock_count > 0)
			self->st_lock_count--;
		if (!self->st_lock_count) {
			/* This section handles "auto-commit"... */

#ifdef XT_IMPLEMENT_NO_ACTION
			/* {NO-ACTION-BUG}
			 * This is required here because it marks the end of a statement.
			 * If we are in a non-auto-commit mode, then we cannot
			 * wait for st_is_update to be set by the begining of a new transaction.
			 */
			if (self->st_restrict_list.bl_count) {
				if (!xt_tab_restrict_rows(&self->st_restrict_list, self))
					err = xt_ha_pbxt_thread_error_for_mysql(thd, self, pb_ignore_dup_key);
			}
#endif

			if (self->st_xact_data) {
				if (self->st_auto_commit) {
					/*
					 * Normally I could assume that if the transaction
					 * has not been aborted by now, then it should be committed.
					 *
					 * Unfortunately, this is not the case!
					 *
					 * create table t1 (id int primary key) engine = pbxt;
					 * create table t2 (id int) engine = pbxt;
					 * 
					 * insert into t1 values ( 1 ) ;
					 * insert into t1 values ( 2 ) ;
					 * insert into t2 values ( 1 ) ;
					 * insert into t2 values ( 2 ) ;
					 * 
					 * --This statement is returns an error calls ha_autocommit_or_rollback():
					 * update t1 set t1.id=1 where t1.id=2;
					 * 
					 * --This statement is returns no error and calls ha_autocommit_or_rollback():
					 * update t1,t2 set t1.id=3, t2.id=3 where t1.id=2 and t2.id = t1.id;
					 * 
					 * --But this statement returns an error and does not call ha_autocommit_or_rollback():
					 * update t1,t2 set t1.id=1, t2.id=1 where t1.id=3 and t2.id = t1.id;
					 * 
					 * The result is, I cannot rely on ha_autocommit_or_rollback() being called :(
					 * So I have to abort myself here...
					 */
					if (pb_open_tab)
						pb_open_tab->ot_table->tab_locks.xt_make_lock_permanent(pb_open_tab, &self->st_lock_list);

					if (self->st_abort_trans) {
						XT_PRINT0(self, "xt_xn_rollback in unlock\n");
						if (!xt_xn_rollback(self))
							err = xt_ha_pbxt_thread_error_for_mysql(thd, self, pb_ignore_dup_key);
					}
					else {
						XT_PRINT0(self, "xt_xn_commit in unlock\n");
						if (!xt_xn_commit(self))
							err = xt_ha_pbxt_thread_error_for_mysql(thd, self, pb_ignore_dup_key);
					}
				}
			}

			/* If the previous statement was "for update", then set the visibilty
			 * so that non- for update SELECTs will see what the for update select
			 * (or update statement) just saw.
			 */
			if (pb_open_tab) {
				if (pb_open_tab->ot_for_update) {
					self->st_visible_time = self->st_database->db_xn_end_time;
					pb_open_tab->ot_for_update = 0;
				}

				if (pb_share->sh_recalc_selectivity) {
					if ((pb_share->sh_table->tab_row_eof_id - 1 - pb_share->sh_table->tab_row_fnum) >= 200) {
						/* [**] */
						pb_share->sh_recalc_selectivity = FALSE;
						xt_ind_set_index_selectivity(self, pb_open_tab);
						pb_share->sh_recalc_selectivity = (pb_share->sh_table->tab_row_eof_id - 1 - pb_share->sh_table->tab_row_fnum) < 150;
					}
				}
			}

			if (self->st_stat_modify)
				self->st_statistics.st_stat_write++;
			else
				self->st_statistics.st_stat_read++;
			self->st_stat_modify = FALSE;
		}

		if (pb_table_locked) {
			pb_table_locked--;
			if (!pb_table_locked)
				ha_release_exclusive_use(self, pb_share);
		}

		/* No longer in use: */
		pb_ex_in_use = 0;
		/* Someone may be waiting for me to complete: */
		if (pb_share->sh_table_lock)
			xt_broadcast_cond_ns((xt_cond_type *) pb_share->sh_ex_cond);
	}
	else {
		XT_PRINT2(self, "ha_pbxt::EXTERNAL_LOCK %s lock_type=%d\n", pb_share->sh_table_path->ps_path, lock_type);
		
		if (pb_lock_table) {

			pb_ex_in_use = 1;
			try_(a) {
				if (!pb_table_locked)
					ha_aquire_exclusive_use(self, pb_share, this);
				pb_table_locked++;

				ha_close_open_tables(self, pb_share, this);

				if (!pb_share->sh_table) {
					xt_ha_open_database_of_table(self, pb_share->sh_table_path);

					ha_open_share(self, pb_share, NULL);
				}
			}
			catch_(a) {
				err = xt_ha_pbxt_thread_error_for_mysql(thd, self, pb_ignore_dup_key);
				pb_ex_in_use = 0;
				goto complete;
			}
			cont_(a);
		}
		else {
			pb_ex_in_use = 1;
			if (pb_share->sh_table_lock && !pb_table_locked) {
				/* If some thread has an exclusive lock, then
				 * we wait for the lock to be removed:
				 */
				if (!ha_wait_for_shared_use(this, pb_share)) {
					err = ha_log_pbxt_thread_error_for_mysql(pb_ignore_dup_key);
					goto complete;
				}
			}

			if (!pb_open_tab) {
				if ((err = reopen())) {
					pb_ex_in_use = 0;
					goto complete;
				}
			}

			/* Set the current thread for this open table: */
			pb_open_tab->ot_thread = self;

			/* If this is a set, then it is in UPDATE/DELETE TABLE ...
			 * or SELECT ... FOR UPDATE
			 */	
			pb_open_tab->ot_is_modify = FALSE;
			if ((pb_open_tab->ot_for_update = (lock_type == F_WRLCK))) {
				switch ((int) thd_sql_command(thd)) {
					case SQLCOM_DELETE:
					case SQLCOM_DELETE_MULTI:
						/* turn DELETE IGNORE into normal DELETE. The IGNORE option causes problems because 
						 * when a record is deleted we add an xlog record which we cannot "rollback" later
						 * when we find that an FK-constraint has failed. 
						 */
						thd->lex->ignore = false;
					case SQLCOM_UPDATE:
					case SQLCOM_UPDATE_MULTI:
					case SQLCOM_REPLACE:
					case SQLCOM_REPLACE_SELECT:
					case SQLCOM_INSERT:
					case SQLCOM_INSERT_SELECT:
						pb_open_tab->ot_is_modify = TRUE;
						self->st_stat_modify = TRUE;
						break;
					case SQLCOM_CREATE_TABLE:
					case SQLCOM_CREATE_INDEX:
					case SQLCOM_ALTER_TABLE:
					case SQLCOM_TRUNCATE:
					case SQLCOM_DROP_TABLE:
					case SQLCOM_DROP_INDEX:
					case SQLCOM_LOAD:
					case SQLCOM_REPAIR:
					case SQLCOM_OPTIMIZE:
						self->st_stat_modify = TRUE;
						break;
				}
			}

			if (pb_open_tab->ot_is_modify && pb_open_tab->ot_table->tab_dic.dic_disable_index) {
				xt_tab_set_index_error(pb_open_tab->ot_table);
				err = ha_log_pbxt_thread_error_for_mysql(pb_ignore_dup_key);
				goto complete;
			}
		}

		/* Record the associated MySQL thread: */
		pb_mysql_thd = thd;

		if (self->st_database != pb_share->sh_table->tab_db) {				
			try_(b) {
				/* PBXT does not permit multiple databases us one statement,
				 * or in a single transaction!
				 *
				 * Example query:
				 *
				 * update mysqltest_1.t1, mysqltest_2.t2 set a=10,d=10;
				 */
				if (self->st_lock_count > 0)
					xt_throw_xterr(XT_CONTEXT, XT_ERR_MULTIPLE_DATABASES);

				xt_ha_open_database_of_table(self, pb_share->sh_table_path);
			}
			catch_(b) {
				err = xt_ha_pbxt_thread_error_for_mysql(thd, self, pb_ignore_dup_key);
				pb_ex_in_use = 0;
				goto complete;
			}
			cont_(b);
		}

		/* See (***) */
		self->st_is_update = FALSE;

		/* Auto begin a transaction (if one is not already running): */
		if (!self->st_xact_data) {
			/* Transaction mode numbers must be identical! */
			(void) ASSERT_NS(ISO_READ_UNCOMMITTED == XT_XACT_UNCOMMITTED_READ);
			(void) ASSERT_NS(ISO_SERIALIZABLE == XT_XACT_SERIALIZABLE);

			self->st_xact_mode = thd_tx_isolation(thd) <= ISO_READ_COMMITTED ? XT_XACT_COMMITTED_READ : XT_XACT_REPEATABLE_READ;
			self->st_ignore_fkeys = (thd_test_options(thd,OPTION_NO_FOREIGN_KEY_CHECKS)) != 0;
			self->st_auto_commit = (thd_test_options(thd, (OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN))) == 0;
			self->st_table_trans = thd_sql_command(thd) == SQLCOM_LOCK_TABLES;
			self->st_abort_trans = FALSE;
			self->st_stat_ended = FALSE;
			self->st_stat_trans = FALSE;
			XT_PRINT0(self, "xt_xn_begin\n");
			if (!xt_xn_begin(self)) {
				err = xt_ha_pbxt_thread_error_for_mysql(thd, self, pb_ignore_dup_key);
				pb_ex_in_use = 0;
				goto complete;
			}

			/*
			 * (**) GOTCHA: trans_register_ha() is not mentioned in the documentation.
			 * It must be called to inform MySQL that we have a transaction (see start_stmt).
			 *
			 * Here are some tests that confirm whether things are done correctly:
			 *
			 * drop table if exists t1, t2;
			 * create table t1 (c1 int);
			 * insert t1 values (1);
			 * select * from t1;
			 * rename table t1 to t2;
			 *
			 * rename will generate an error if MySQL thinks a transaction is
			 * still running.
			 *
			 * create table t1 (a text character set utf8, b text character set latin1);
			 * insert t1 values (0x4F736E616272C3BC636B, 0x4BF66C6E);
			 * select * from t1;
			 * --exec $MYSQL_DUMP --tab=$MYSQLTEST_VARDIR/tmp/ test
			 * --exec $MYSQL test < $MYSQLTEST_VARDIR/tmp/t1.sql
			 * --exec $MYSQL_IMPORT test $MYSQLTEST_VARDIR/tmp/t1.txt
			 * select * from t1;
			 *
			 * This test forces a begin transaction in start_stmt()
			 *
			 * drop tables if exists t1;
			 * create table t1 (c1 int);
			 * lock tables t1 write;
			 * insert t1 values (1);
			 * insert t1 values (2);
			 * unlock tables;
			 *
			 * The second select will return an empty result of the
			 * MySQL is not informed that a transaction is running (auto-commit 
			 * in external_lock comes too late)!
			 *
			 */
			if (!self->st_auto_commit) {
				trans_register_ha(thd, TRUE, pbxt_hton);
				XT_PRINT0(self, "ha_pbxt::external_lock trans_register_ha all=TRUE\n");
			}
		}

		if (lock_type == F_WRLCK || self->st_xact_mode < XT_XACT_REPEATABLE_READ)
			self->st_visible_time = self->st_database->db_xn_end_time;

#ifdef TRACE_STATEMENTS
		if (self->st_lock_count == 0)
			STAT_TRACE(self, *thd_query(thd));
#endif
		self->st_lock_count++;
	}

	complete:
	return err;
}

/*
 * This function is called for each table in a statement
 * after LOCK TABLES has been used.
 *
 * Currently I only use this function to set the
 * current thread of the table handle. 
 *
 * GOTCHA: The prototype of start_stmt() has changed
 * from version 4.1 to 5.1!
 */
int ha_pbxt::start_stmt(THD *thd, thr_lock_type lock_type)
{
	int				err = 0;
	XTThreadPtr		self;

	ASSERT_NS(pb_ex_in_use);

	if (!(self = ha_set_current_thread(thd, &err)))
		return xt_ha_pbxt_to_mysql_error(err);

	XT_PRINT2(self, "ha_pbxt::start_stmt %s lock_type=%d\n", pb_share->sh_table_path->ps_path, (int) lock_type);

	if (!pb_open_tab) {
		if ((err = reopen()))
			goto complete;
	}

	ASSERT_NS(pb_open_tab->ot_thread == self);
	ASSERT_NS(thd == pb_mysql_thd);
	ASSERT_NS(self->st_database == pb_open_tab->ot_table->tab_db);

	if (self->st_stat_ended) {
		self->st_stat_ended = FALSE;
		self->st_stat_trans = FALSE;

#ifdef XT_IMPLEMENT_NO_ACTION
		if (self->st_restrict_list.bl_count) {
			if (!xt_tab_restrict_rows(&self->st_restrict_list, self)) {
				err = xt_ha_pbxt_thread_error_for_mysql(pb_mysql_thd, self, pb_ignore_dup_key);
			}
		}
#endif

		/* This section handles "auto-commit"... */
		if (self->st_xact_data && self->st_auto_commit && self->st_table_trans) {
			if (self->st_abort_trans) {
				XT_PRINT0(self, "xt_xn_rollback\n");
				if (!xt_xn_rollback(self))
					err = xt_ha_pbxt_thread_error_for_mysql(pb_mysql_thd, self, pb_ignore_dup_key);
			}
			else {
				XT_PRINT0(self, "xt_xn_commit\n");
				if (!xt_xn_commit(self))
					err = xt_ha_pbxt_thread_error_for_mysql(pb_mysql_thd, self, pb_ignore_dup_key);
			}
		}

		if (self->st_stat_modify)
			self->st_statistics.st_stat_write++;
		else
			self->st_statistics.st_stat_read++;
		self->st_stat_modify = FALSE;

		/* If the previous statement was "for update", then set the visibilty
		 * so that non- for update SELECTs will see what the for update select
		 * (or update statement) just saw.
		 */
		if (pb_open_tab->ot_for_update)
			self->st_visible_time = self->st_database->db_xn_end_time;
	}

	pb_open_tab->ot_for_update =
		(lock_type != TL_READ && 
		 lock_type != TL_READ_WITH_SHARED_LOCKS &&
#ifndef DRIZZLED
		 lock_type != TL_READ_HIGH_PRIORITY && 
#endif
		 lock_type != TL_READ_NO_INSERT);
	pb_open_tab->ot_is_modify = FALSE;
	if (pb_open_tab->ot_for_update) {
		switch ((int) thd_sql_command(thd)) {
			case SQLCOM_UPDATE:
			case SQLCOM_UPDATE_MULTI:
			case SQLCOM_DELETE:
			case SQLCOM_DELETE_MULTI:
			case SQLCOM_REPLACE:
			case SQLCOM_REPLACE_SELECT:
			case SQLCOM_INSERT:
			case SQLCOM_INSERT_SELECT:
				pb_open_tab->ot_is_modify = TRUE;
				self->st_stat_modify = TRUE;
				break;
			case SQLCOM_CREATE_TABLE:
			case SQLCOM_CREATE_INDEX:
			case SQLCOM_ALTER_TABLE:
			case SQLCOM_TRUNCATE:
			case SQLCOM_DROP_TABLE:
			case SQLCOM_DROP_INDEX:
			case SQLCOM_LOAD:
			case SQLCOM_REPAIR:
			case SQLCOM_OPTIMIZE:
				self->st_stat_modify = TRUE;
				break;
		}
	}


	/* (***) This is required at this level!
	 * No matter how often it is called, it is still the start of a
	 * statement. We need to make sure statements that are NOT mistaken
	 * for different type of statement.
	 *
	 * Here is an example:
	 * select * from t1 where data = getcount("bar")
	 *
	 * If the procedure getcount() addresses another table.
	 * then open and close of the statements in getcount()
	 * are nested within an open close of the select t1
	 * statement.
	 */
	self->st_is_update = FALSE;

	/* See comment (**) */
	if (!self->st_xact_data) {
		self->st_xact_mode = thd_tx_isolation(thd) <= ISO_READ_COMMITTED ? XT_XACT_COMMITTED_READ : XT_XACT_REPEATABLE_READ;
		self->st_ignore_fkeys = (thd_test_options(thd, OPTION_NO_FOREIGN_KEY_CHECKS)) != 0;
		self->st_auto_commit = (thd_test_options(thd,(OPTION_NOT_AUTOCOMMIT | OPTION_BEGIN))) == 0;
		/* self->st_table_trans = not set here! */
		self->st_abort_trans = FALSE;
		self->st_stat_ended = FALSE;
		self->st_stat_trans = FALSE;
		XT_PRINT0(self, "xt_xn_begin\n");
		if (!xt_xn_begin(self)) {
			err = xt_ha_pbxt_thread_error_for_mysql(thd, self, pb_ignore_dup_key);
			goto complete;
		}
		if (!self->st_auto_commit) {
			trans_register_ha(thd, TRUE, pbxt_hton);
			XT_PRINT0(self, "ha_pbxt::start_stmt trans_register_ha all=TRUE\n");
		}
	}

	if (pb_open_tab->ot_for_update || self->st_xact_mode < XT_XACT_REPEATABLE_READ)
		self->st_visible_time = self->st_database->db_xn_end_time;

	pb_in_stat = TRUE;

	self->st_stat_count++;

	complete:
	return err;
}

/*
 * The idea with handler::store_lock() is the following:
 *
 * The statement decided which locks we should need for the table
 * for updates/deletes/inserts we get WRITE locks, for SELECT... we get
 * read locks.
 *
 * Before adding the lock into the table lock handler (see thr_lock.c)
 * mysqld calls store lock with the requested locks. Store lock can now
 * modify a write lock to a read lock (or some other lock), ignore the
 * lock (if we don't want to use MySQL table locks at all) or add locks
 * for many tables (like we do when we are using a MERGE handler).
 *
 * When releasing locks, store_lock() are also called. In this case one
 * usually doesn't have to do anything.
 *
 * In some exceptional cases MySQL may send a request for a TL_IGNORE;
 * This means that we are requesting the same lock as last time and this
 * should also be ignored. (This may happen when someone does a flush
 * table when we have opened a part of the tables, in which case mysqld
 * closes and reopens the tables and tries to get the same locks at last
 * time). In the future we will probably try to remove this.
 *
 * Called from lock.cc by get_lock_data().
 */
THR_LOCK_DATA **ha_pbxt::store_lock(THD *thd, THR_LOCK_DATA **to, enum thr_lock_type lock_type)
{
	if (lock_type != TL_IGNORE && pb_lock.type == TL_UNLOCK) {
		/* Set to TRUE for operations that require a table lock: */
		switch (thd_sql_command(thd)) {
			case SQLCOM_TRUNCATE:
				/* GOTCHA:
				 * The problem is, if I do not do this, then
				 * TRUNCATE TABLE deadlocks with a normal update of the table!
				 * The reason is:
				 *
				 * external_lock() is called before MySQL actually locks the
				 * table. In external_lock(), the table is shared locked,
				 * by indicating that the handler is in use.
				 *
				 * Then later, in delete_all_rows(), a exclusive lock must be
				 * obtained. If an UPDATE or INSERT has also gained a shared
				 * lock in the meantime, then TRUNCATE TABLE hangs.
				 *
				 * By setting pb_lock_table we indicate that an exclusive lock
				 * should be gained in external_lock().
				 *
				 * This is the locking behaviour:
				 *
				 * TRUNCATE TABLE:
				 * XT SHARE LOCK (mysql_lock_tables calls external_lock)
				 * MySQL WRITE LOCK (mysql_lock_tables)
				 * ...
				 * XT EXCLUSIVE LOCK (delete_all_rows)
				 *
				 * INSERT:
				 * XT SHARED LOCK (mysql_lock_tables calls external_lock)
				 * MySQL WRITE_ALLOW_WRITE LOCK (mysql_lock_tables)
				 *
				 * If the locking for INSERT is done in the ... phase
				 * above, then we have a deadlock because 
				 * WRITE_ALLOW_WRITE conflicts with WRITE.
				 *
				 * Making TRUNCATE TABLE take a WRITE_ALLOW_WRITE LOCK, will
				 * not solve the problem because then 2 TRUNCATE TABLES
				 * can deadlock due to lock escalation.
				 *
				 * What may work is if MySQL were to lock BEFORE calling
				 * external_lock()!
				 *
				 * However, using this method, TRUNCATE TABLE does deadlock
				 * with other operations such as ALTER TABLE!
				 *
				 * This is handled with a lock timeout. Assuming 
				 * TRUNCATE TABLE will be mixed with DML this is the
				 * best solution!
				 */
				pb_lock_table = TRUE;
				break;
			default:
				pb_lock_table = FALSE;
				break;
		}

#ifdef PBXT_HANDLER_TRACE
		pb_lock.type = lock_type;
#endif
		/* GOTCHA: Before it was OK to weaken the lock after just checking
		 * that !thd->in_lock_tables. However, when starting a procedure, MySQL
		 * simulates a LOCK TABLES statement.
		 *
		 * So we need to be more specific here, and check what the actual statement
		 * type. Before doing this I got a deadlock (undetected) on the following test.
		 * However, now we get a failed assertion in ha_rollback_trans():
		 * TODO: Check this with InnoDB!
		 *
		 * DBUG_ASSERT(0);
		 * my_error(ER_COMMIT_NOT_ALLOWED_IN_SF_OR_TRG, MYF(0));
		 *
		 * drop table if exists t3;
		 * create table t3 (a smallint primary key) engine=pbxt;
		 * insert into t3 (a) values (40);
		 * insert into t3 (a) values (50);
		 * 
		 * delimiter |
		 * 
		 * drop function if exists t3_update|
		 * 
		 * create function t3_update() returns int
		 * begin
		 *   insert into t3 values (10);
		 *   return 100;
		 * end|
		 * 
		 * delimiter ;
		 * 
		 * CONN 1:
		 * 
		 * begin;
		 * update t3 set a = 5 where a = 50;
		 * 
		 * CONN 2:
		 * 
		 * begin;
		 * update t3 set a = 4 where a = 40;
		 * 
		 * CONN 1:
		 * 
		 * update t3 set a = 4 where a = 40; // Hangs waiting CONN 2.
		 * 
		 * CONN 2:
		 * 
		 * select t3_update(); // Hangs waiting for table lock.
		 * 
		 */
		if ((lock_type >= TL_WRITE_CONCURRENT_INSERT && lock_type <= TL_WRITE) && 
			!(thd_in_lock_tables(thd) && thd_sql_command(thd) == SQLCOM_LOCK_TABLES) &&
			!thd_tablespace_op(thd) &&
			thd_sql_command(thd) != SQLCOM_TRUNCATE &&
			thd_sql_command(thd) != SQLCOM_OPTIMIZE &&
			thd_sql_command(thd) != SQLCOM_CREATE_TABLE) {
			lock_type = TL_WRITE_ALLOW_WRITE;
		}

		/* In queries of type INSERT INTO t1 SELECT ... FROM t2 ...
		 * MySQL would use the lock TL_READ_NO_INSERT on t2, and that
		 * would conflict with TL_WRITE_ALLOW_WRITE, blocking all inserts
		 * to t2. Convert the lock to a normal read lock to allow
		 * concurrent inserts to t2.
		 * 
		 * (This one from InnoDB)

                 * Stewart: removed SQLCOM_CALL, not sure of implications.
		 */
		if (lock_type == TL_READ_NO_INSERT &&
			(!thd_in_lock_tables(thd)
#ifndef DRIZZLED
			 || thd_sql_command(thd) == SQLCOM_CALL
#endif
			))
		{
			lock_type = TL_READ;
		}

		XT_PRINT3(xt_get_self(), "ha_pbxt::store_lock %s %d->%d\n", pb_share->sh_table_path->ps_path, pb_lock.type, lock_type);
		pb_lock.type = lock_type;
	}
#ifdef PBXT_HANDLER_TRACE
	else {
		XT_PRINT3(xt_get_self(), "ha_pbxt::store_lock %s %d->%d (ignore/unlock)\n", pb_share->sh_table_path->ps_path, lock_type, lock_type);
	}
#endif
	*to++= &pb_lock;
	return to;
}

/*
 * Used to delete a table. By the time delete_table() has been called all
 * opened references to this table will have been closed (and your globally
 * shared references released. The variable name will just be the name of
 * the table. You will need to remove any files you have created at this point.
 *
 * Called from handler.cc by delete_table and ha_create_table(). Only used
 * during create if the table_flag HA_DROP_BEFORE_CREATE was specified for
 * the storage engine.
*/
int ha_pbxt::delete_table(const char *table_path)
{
	THD				*thd = current_thd;
	int				err = 0;
	XTThreadPtr		self = NULL;
	XTSharePtr		share;

	STAT_TRACE(self, *thd_query(thd));
	XT_PRINT1(self, "ha_pbxt::delete_table %s\n", table_path);

	if (XTSystemTableShare::isSystemTable(table_path))
		return delete_system_table(table_path);

	if (!(self = ha_set_current_thread(thd, &err)))
		return xt_ha_pbxt_to_mysql_error(err);

	self->st_ignore_fkeys = (thd_test_options(thd, OPTION_NO_FOREIGN_KEY_CHECKS)) != 0;

	try_(a) {
		xt_ha_open_database_of_table(self, (XTPathStrPtr) table_path);

		ASSERT(xt_get_self() == self);
		try_(b) {
			/* NOTE: MySQL does not drop a table by first locking it!
			 * We also cannot use pb_share because the handler used
			 * to delete a table is not openned correctly.
			 */
			share = ha_get_share(self, table_path, false, NULL);
			pushr_(ha_unget_share, share);
			ha_aquire_exclusive_use(self, share, NULL);
			pushr_(ha_release_exclusive_use, share);
			ha_close_open_tables(self, share, NULL);

			xt_drop_table(self, (XTPathStrPtr) table_path, thd_sql_command(thd) == SQLCOM_DROP_DB);

			freer_(); // ha_release_exclusive_use(share)
			freer_(); // ha_unget_share(share)
		}
		catch_(b) {
			/* In MySQL if the table does not exist, just log the error and continue. This is
 			 * needed to delete table in the case when CREATE TABLE fails and no PBXT disk
 			 * structures were created. 
 			 * Drizzle unlike MySQL iterates over all handlers and tries to delete table. It
 			 * stops after when a handler returns TRUE, so in Drizzle we need to report error.  
			 */
#ifndef DRIZZLED
			if (self->t_exception.e_xt_err == XT_ERR_TABLE_NOT_FOUND)
				xt_log_and_clear_exception(self);
			else
#endif
				throw_();
		}
		cont_(b);

		/*
		 * If there are no more PBXT tables in the database, we
		 * "drop the database", which deletes all PBXT resources
		 * in the database.
		 */
		/* We now only drop the pbxt system data,
		 * when the PBXT database is dropped.
		 */
#ifndef XT_USE_GLOBAL_DB
		if (!xt_table_exists(self->st_database)) {
			xt_ha_all_threads_close_database(self, self->st_database);
			xt_drop_database(self, self->st_database);
			xt_unuse_database(self, self);
			xt_ha_close_global_database(self);
		}
#endif
	}
	catch_(a) {
		err = xt_ha_pbxt_thread_error_for_mysql(thd, self, pb_ignore_dup_key);
#ifdef DRIZZLED
		if (err == HA_ERR_NO_SUCH_TABLE)
			err = ENOENT;
#endif
	}
	cont_(a);
	
#ifdef PBMS_ENABLED
	/* Call pbms_delete_table_with_blobs() last because it cannot be undone. */
	if (!err) {
		PBMSResultRec result;

		if (pbms_delete_table_with_blobs(table_path, &result)) {
			xt_logf(XT_NT_WARNING, "pbms_delete_table_with_blobs() Error: %s", result.mr_message);
		}
		
		pbms_completed(NULL, true);
	}
#endif

	return err;
}

int ha_pbxt::delete_system_table(const char *table_path)
{
	THD				*thd = current_thd;
	XTExceptionRec	e;
	int				err = 0;
	XTThreadPtr		self;

	if (!(self = xt_ha_set_current_thread(thd, &e)))
		return xt_ha_pbxt_to_mysql_error(e.e_xt_err);

	try_(a) {
		xt_ha_open_database_of_table(self, (XTPathStrPtr) table_path);

		if (xt_table_exists(self->st_database))
			xt_throw_xterr(XT_CONTEXT, XT_ERR_PBXT_TABLE_EXISTS);

		XTSystemTableShare::setSystemTableDeleted(table_path);

		if (!XTSystemTableShare::doesSystemTableExist()) {
			xt_ha_all_threads_close_database(self, self->st_database);
			xt_drop_database(self, self->st_database);
			xt_unuse_database(self, self);
			xt_ha_close_global_database(self);
		}
	}
	catch_(a) {
		err = xt_ha_pbxt_thread_error_for_mysql(thd, self, FALSE);
	}
	cont_(a);

	return err;
}

/*
 * Renames a table from one name to another from alter table call.
 * This function can be used to move a table from one database to
 * another.
 */
int ha_pbxt::rename_table(const char *from, const char *to)
{
	THD				*thd = current_thd;
	int				err = 0;
	XTThreadPtr		self;
	XTSharePtr		share;
	XTDatabaseHPtr	to_db;

	XT_TRACE_CALL();

	if (XTSystemTableShare::isSystemTable(from))
		return rename_system_table(from, to);

	if (!(self = ha_set_current_thread(thd, &err)))
		return xt_ha_pbxt_to_mysql_error(err);

	XT_PRINT2(self, "ha_pbxt::rename_table %s -> %s\n", from, to);

#ifdef PBMS_ENABLED
	PBMSResultRec result;

	err = pbms_rename_table_with_blobs(from, to, &result);
	if (err) {
		xt_logf(XT_NT_ERROR, "pbms_rename_table_with_blobs() Error: %s", result.mr_message);
		return err;
	}
#endif

	try_(a) {
		xt_ha_open_database_of_table(self, (XTPathStrPtr) to);
		to_db = self->st_database;

		xt_ha_open_database_of_table(self, (XTPathStrPtr) from);

		if (self->st_database != to_db)
			xt_throw_xterr(XT_CONTEXT, XT_ERR_CANNOT_CHANGE_DB);

		/*
		 * NOTE: MySQL does not lock before calling rename table!
		 *
		 * We cannot use pb_share because rename_table() is
		 * called without correctly initializing
		 * the handler!
		 */
		share = ha_get_share(self, from, true, NULL);
		pushr_(ha_unget_share, share);
		ha_aquire_exclusive_use(self, share, NULL);
		pushr_(ha_release_exclusive_use, share);
		ha_close_open_tables(self, share, NULL);

		self->st_ignore_fkeys = (thd_test_options(thd, OPTION_NO_FOREIGN_KEY_CHECKS)) != 0;
		xt_rename_table(self, (XTPathStrPtr) from, (XTPathStrPtr) to);

		freer_(); // ha_release_exclusive_use(share)
		freer_(); // ha_unget_share(share)

		/*
		 * If there are no more PBXT tables in the database, we
		 * "drop the database", which deletes all PBXT resources
		 * in the database.
		 */
#ifdef XT_USE_GLOBAL_DB
		/* We now only drop the pbxt system data,
		 * when the PBXT database is dropped.
		 */
		if (!xt_table_exists(self->st_database)) {
			xt_ha_all_threads_close_database(self, self->st_database);
			xt_drop_database(self, self->st_database);
		}
#endif
	}
	catch_(a) {
		err = xt_ha_pbxt_thread_error_for_mysql(thd, self, pb_ignore_dup_key);
	}
	cont_(a);
	
#ifdef PBMS_ENABLED
	pbms_completed(NULL, (err == 0));
#endif

	XT_RETURN(err);
}

int ha_pbxt::rename_system_table(const char *XT_UNUSED(from), const char *XT_UNUSED(to))
{
	return ER_NOT_SUPPORTED_YET;
}

uint ha_pbxt::max_supported_key_length() const
{
	return XT_INDEX_MAX_KEY_SIZE;
}

uint ha_pbxt::max_supported_key_part_length() const
{
	/* There is a little overhead in order to fit! */
	return XT_INDEX_MAX_KEY_SIZE-4;
}

/*
 * Called in test_quick_select to determine if indexes should be used.
 *
 * As far as I can tell, time is measured in "disk reads". So the
 * calculation below means the system reads about 20 rows per read.
 *
 * For example a sequence scan uses a read buffer which reads a
 * number of rows at once, or a sequential scan can make use
 * of the cache (so it need to read less).
 */
double ha_pbxt::scan_time()
{
	double result = (double) (stats.records + stats.deleted) / 38.0 + 2;
	return result;
}

/*
 * The next method will never be called if you do not implement indexes.
 */
double ha_pbxt::read_time(uint XT_UNUSED(index), uint ranges, ha_rows rows)
{
	double result = rows2double(ranges+rows);
	return result;
}

/*
 * Given a starting key, and an ending key estimate the number of rows that
 * will exist between the two. end_key may be empty which in case determine
 * if start_key matches any rows.
 * 
 * Called from opt_range.cc by check_quick_keys().
 *
 */
ha_rows ha_pbxt::records_in_range(uint inx, key_range *min_key, key_range *max_key)
{
	XTIndexPtr		ind;
	key_part_map	keypart_map;
	u_int			segement = 0;
	ha_rows			result;

	if (min_key)
		keypart_map = min_key->keypart_map;
	else if (max_key)
		keypart_map = max_key->keypart_map;
	else
		return 1;
	ind = (XTIndexPtr) pb_share->sh_dic_keys[inx];
	
	while (keypart_map & 1) {
		segement++;
		keypart_map = keypart_map >> 1;
	}

	if (segement < 1 || segement > ind->mi_seg_count)
		result = 1;
	else
		result = ind->mi_seg[segement-1].is_recs_in_range;
#ifdef XT_PRINT_INDEX_OPT
	printf("records_in_range %s index %d cols req=%d/%d read_bits=%X write_bits=%X index_bits=%X --> %d\n", pb_open_tab->ot_table->tab_name->ps_path, (int) inx, segement, ind->mi_seg_count, (int) *table->read_set->bitmap, (int) *table->write_set->bitmap, (int) *ind->mi_col_map.bitmap, (int) result);
#endif
	return result;
}

/*
 * create() is called to create a table/database. The variable name will have the name
 * of the table. When create() is called you do not need to worry about opening
 * the table. Also, the FRM file will have already been created so adjusting
 * create_info will not do you any good. You can overwrite the frm file at this
 * point if you wish to change the table definition, but there are no methods
 * currently provided for doing that.

 * Called from handle.cc by ha_create_table().
*/
int ha_pbxt::create(const char *table_path, TABLE *table_arg, HA_CREATE_INFO *create_info)
{
	THD				*thd = current_thd;
	int				err = 0;
	XTThreadPtr		self;
	XTDDTable		*tab_def = NULL;
	XTDictionaryRec	dic;

	memset(&dic, 0, sizeof(dic));

	XT_TRACE_CALL();

	if (!(self = ha_set_current_thread(thd, &err)))
		return xt_ha_pbxt_to_mysql_error(err);

	STAT_TRACE(self, *thd_query(thd));
	XT_PRINT1(self, "ha_pbxt::create %s\n", table_path);

	try_(a) {
		xt_ha_open_database_of_table(self, (XTPathStrPtr) table_path);

		for (uint i=0; i<TS(table_arg)->keys; i++) {
			if (table_arg->key_info[i].key_length > XT_INDEX_MAX_KEY_SIZE)
				xt_throw_sulxterr(XT_CONTEXT, XT_ERR_KEY_TOO_LARGE, table_arg->key_info[i].name, (u_long) XT_INDEX_MAX_KEY_SIZE);
		}

		/* ($) auto_increment_value will be zero if 
		 * AUTO_INCREMENT is not used. Otherwise
		 * Query was ALTER TABLE ... AUTO_INCREMENT = x; or 
		 * CREATE TABLE ... AUTO_INCREMENT = x;
		 */
		tab_def = xt_ri_create_table(self, true, (XTPathStrPtr) table_path, *thd_query(thd), myxt_create_table_from_table(self, table_arg));
		tab_def->checkForeignKeys(self, create_info->options & HA_LEX_CREATE_TMP_TABLE);

		dic.dic_table = tab_def;
		dic.dic_my_table = table_arg;
		dic.dic_tab_flags = (create_info->options & HA_LEX_CREATE_TMP_TABLE) ? XT_TAB_FLAGS_TEMP_TAB : 0;
		dic.dic_min_auto_inc = (xtWord8) create_info->auto_increment_value; /* ($) */
		dic.dic_def_ave_row_size = (xtWord8) table_arg->s->avg_row_length;
		myxt_setup_dictionary(self, &dic);

		/*
		 * We used to ignore the value of foreign_key_checks flag and allowed creation
		 * of tables with "hanging" references. Now we validate FKs if foreign_key_checks != 0
		 */
		self->st_ignore_fkeys = (thd_test_options(thd, OPTION_NO_FOREIGN_KEY_CHECKS)) != 0;

		/*
		 * Previously I set delete_if_exists=TRUE because
		 * CREATE TABLE was being used to TRUNCATE.
		 * This was due to the flag HTON_CAN_RECREATE.
		 * Now I could set delete_if_exists=FALSE, but
		 * leaving it TRUE should not cause any problems.
		 */
		xt_create_table(self, (XTPathStrPtr) table_path, &dic);
	}
	catch_(a) {
		if (tab_def)
			tab_def->finalize(self);
		dic.dic_table = NULL;
		err = xt_ha_pbxt_thread_error_for_mysql(thd, self, pb_ignore_dup_key);
	}
	cont_(a);

	/* Free the dictionary, but not 'table_arg'! */
	dic.dic_my_table = NULL;
	myxt_free_dictionary(self, &dic);

	XT_RETURN(err);
}

void ha_pbxt::update_create_info(HA_CREATE_INFO *create_info)
{
	XTOpenTablePtr	ot;

	if ((ot = pb_open_tab)) {
		if (!(create_info->used_fields & HA_CREATE_USED_AUTO)) {
			/* Fill in the minimum auto-increment value! */
			create_info->auto_increment_value = ot->ot_table->tab_dic.dic_min_auto_inc;
		}
	}
}

char *ha_pbxt::get_foreign_key_create_info()
{
	THD					*thd = current_thd;
	int					err = 0;
	XTThreadPtr			self;
	XTStringBufferRec	tab_def = { 0, 0, 0 };

	if (!(self = ha_set_current_thread(thd, &err))) {
		xt_ha_pbxt_to_mysql_error(err);
		return NULL;
	}

	if (!pb_open_tab) {
		if ((err = reopen()))
			return NULL;
	}

	if (!pb_open_tab->ot_table->tab_dic.dic_table)
		return NULL;

	try_(a) {
		pb_open_tab->ot_table->tab_dic.dic_table->loadForeignKeyString(self, &tab_def);
	}
	catch_(a) {
		xt_sb_set_size(self, &tab_def, 0);
		err = xt_ha_pbxt_thread_error_for_mysql(thd, self, pb_ignore_dup_key);
	}
	cont_(a);

	return tab_def.sb_cstring;
}

void ha_pbxt::free_foreign_key_create_info(char* str)
{
	xt_free(NULL, str);
}

bool ha_pbxt::get_error_message(int XT_UNUSED(error), String *buf)
{
	THD				*thd = current_thd;
	int				err = 0;
	XTThreadPtr		self;

	if (!(self = ha_set_current_thread(thd, &err)))
		return FALSE;

	if (!self->t_exception.e_xt_err)
		return FALSE;

	buf->copy(self->t_exception.e_err_msg, strlen(self->t_exception.e_err_msg), system_charset_info);
	return TRUE;
}

/* 
 * get info about FKs of the currently open table
 * used in 
 * 1. REPLACE; is > 0 if table is referred by a FOREIGN KEY 
 * 2. INFORMATION_SCHEMA tables: TABLE_CONSTRAINTS, REFERENTIAL_CONSTRAINTS
 * Return value: as of 5.1.24 it's ignored
 */

int ha_pbxt::get_foreign_key_list(THD *thd, List<FOREIGN_KEY_INFO> *f_key_list)
{
	int err = 0;
	XTThreadPtr	self;
	const char *action;

	if (!(self = ha_set_current_thread(thd, &err))) {
		return xt_ha_pbxt_to_mysql_error(err);
	}

	try_(a) {
		XTDDTable *table_dic = pb_open_tab->ot_table->tab_dic.dic_table;

		if (table_dic == NULL)
			xt_throw_errno(XT_CONTEXT, XT_ERR_NO_DICTIONARY);

		for (int i = 0, sz = table_dic->dt_fkeys.size(); i < sz; i++) {
			FOREIGN_KEY_INFO *fk_info= new	// assumed that C++ exceptions are disabled
				(thd_alloc(thd, sizeof(FOREIGN_KEY_INFO))) FOREIGN_KEY_INFO;

			if (fk_info == NULL)
				xt_throw_errno(XT_CONTEXT, XT_ENOMEM);

			XTDDForeignKey *fk = table_dic->dt_fkeys.itemAt(i);

			const char *path = fk->fk_ref_tab_name->ps_path;
			const char *ref_tbl_name = path + strlen(path);

			while (ref_tbl_name != path && !XT_IS_DIR_CHAR(*ref_tbl_name)) 
				ref_tbl_name--;

			const char * ref_db_name = ref_tbl_name - 1;

			while (ref_db_name != path && !XT_IS_DIR_CHAR(*ref_db_name)) 
				ref_db_name--;

			ref_tbl_name++;
			ref_db_name++;

			fk_info->forein_id = thd_make_lex_string(thd, 0,
				fk->co_name, (uint) strlen(fk->co_name), 1);

			fk_info->referenced_db = thd_make_lex_string(thd, 0,
				ref_db_name, (uint) (ref_tbl_name - ref_db_name - 1), 1);

			fk_info->referenced_table = thd_make_lex_string(thd, 0,
				ref_tbl_name, (uint) strlen(ref_tbl_name), 1);

			fk_info->referenced_key_name = NULL;			

			XTIndex *ix = fk->getReferenceIndexPtr();
			if (ix == NULL) /* can be NULL if another thread changes referenced table at the moment */
				continue;
			
			XTDDTable *ref_table = fk->fk_ref_table;

			// might be a self-reference
			if ((ref_table == NULL) 
				&& (xt_tab_compare_names(path, table_dic->dt_table->tab_name->ps_path) == 0)) {
				ref_table = table_dic;
			}

			if (ref_table != NULL) {
				const XTList<XTDDIndex>& ix_list = ref_table->dt_indexes;
				for (int j = 0, sz2 = ix_list.size(); j < sz2; j++) {
					XTDDIndex *ddix = ix_list.itemAt(j);
					if (ddix->in_index ==  ix->mi_index_no) {
						const char *ix_name = 
							ddix->co_name ? ddix->co_name : ddix->co_ind_name;
						fk_info->referenced_key_name = thd_make_lex_string(thd, 0,
							ix_name, (uint) strlen(ix_name), 1);
						break;
					}
				}
			}

			action = XTDDForeignKey::actionTypeToString(fk->fk_on_delete);
			fk_info->delete_method = thd_make_lex_string(thd, 0,
				action, (uint) strlen(action), 1);
			action = XTDDForeignKey::actionTypeToString(fk->fk_on_update);
			fk_info->update_method = thd_make_lex_string(thd, 0,
				action, (uint) strlen(action), 1);

			const XTList<XTDDColumnRef>& cols = fk->co_cols;
			for (int j = 0, sz2 = cols.size(); j < sz2; j++) {
				XTDDColumnRef *col_ref= cols.itemAt(j);
				fk_info->foreign_fields.push_back(thd_make_lex_string(thd, 0,
					col_ref->cr_col_name, (uint) strlen(col_ref->cr_col_name), 1));
			}

			const XTList<XTDDColumnRef>& ref_cols = fk->fk_ref_cols;
			for (int j = 0, sz2 = ref_cols.size(); j < sz2; j++) {
				XTDDColumnRef *col_ref= ref_cols.itemAt(j);
				fk_info->referenced_fields.push_back(thd_make_lex_string(thd, 0,
					col_ref->cr_col_name, (uint) strlen(col_ref->cr_col_name), 1));
			}

			f_key_list->push_back(fk_info);
		}
	}
	catch_(a) {
		err = xt_ha_pbxt_thread_error_for_mysql(thd, self, pb_ignore_dup_key);
	}
	cont_(a);

	return err; 
}

uint ha_pbxt::referenced_by_foreign_key()
{
	XTDDTable *table_dic = pb_open_tab->ot_table->tab_dic.dic_table;

	if (!table_dic)
		return 0;
	/* Check the list of referencing tables: */
	return table_dic->dt_trefs ? 1 : 0;
}


struct st_mysql_sys_var
{
	MYSQL_PLUGIN_VAR_HEADER;
};

#if MYSQL_VERSION_ID < 60000
#if MYSQL_VERSION_ID >= 50124
#define USE_CONST_SAVE
#endif
#else
#if MYSQL_VERSION_ID >= 60005
#define USE_CONST_SAVE
#endif
#endif

#ifdef USE_CONST_SAVE
static void pbxt_record_cache_size_func(THD *XT_UNUSED(thd), struct st_mysql_sys_var *var, void *tgt, const void *save)
#else
static void pbxt_record_cache_size_func(THD *XT_UNUSED(thd), struct st_mysql_sys_var *var, void *tgt, void *save)
#endif
{
	xtInt8	record_cache_size;

	char *old= *(char **) tgt;
	*(char **)tgt= *(char **) save;
	if (var->flags & PLUGIN_VAR_MEMALLOC)
	{
		*(char **)tgt= my_strdup(*(char **) save, MYF(0));
		my_free(old, MYF(0));
	}
	record_cache_size = ha_set_variable(&pbxt_record_cache_size, &vp_record_cache_size);
	xt_tc_set_cache_size((size_t) record_cache_size);
#ifdef DEBUG
	char buffer[200];

	sprintf(buffer, "pbxt_record_cache_size=%llu\n", (u_llong) record_cache_size);
	xt_logf(XT_NT_INFO, buffer);
#endif
}

#ifndef DRIZZLED
struct st_mysql_storage_engine pbxt_storage_engine = {
	MYSQL_HANDLERTON_INTERFACE_VERSION
};
static st_mysql_information_schema pbxt_statitics = {
	MYSQL_INFORMATION_SCHEMA_INTERFACE_VERSION
};
#endif

#if MYSQL_VERSION_ID >= 50118
static MYSQL_SYSVAR_STR(index_cache_size, pbxt_index_cache_size,
  PLUGIN_VAR_READONLY,
  "The amount of memory allocated to the index cache, used only to cache index data.",
  NULL, NULL, NULL);

static MYSQL_SYSVAR_STR(record_cache_size, pbxt_record_cache_size,
  PLUGIN_VAR_READONLY, // PLUGIN_VAR_OPCMDARG | PLUGIN_VAR_MEMALLOC,
  "The amount of memory allocated to the record cache used to cache table data.",
  NULL, pbxt_record_cache_size_func, NULL);

static MYSQL_SYSVAR_STR(log_cache_size, pbxt_log_cache_size,
  PLUGIN_VAR_READONLY,
  "The amount of memory allocated to the transaction log cache used to cache transaction log data.",
  NULL, NULL, NULL);

static MYSQL_SYSVAR_STR(log_file_threshold, pbxt_log_file_threshold,
  PLUGIN_VAR_READONLY,
  "The size of a transaction log before rollover, and a new log is created.",
  NULL, NULL, NULL);

static MYSQL_SYSVAR_STR(transaction_buffer_size, pbxt_transaction_buffer_size,
  PLUGIN_VAR_READONLY,
  "The size of the global transaction log buffer (the engine allocates 2 buffers of this size).",
  NULL, NULL, NULL);

static MYSQL_SYSVAR_STR(log_buffer_size, pbxt_log_buffer_size,
  PLUGIN_VAR_READONLY,
  "The size of the buffer used to cache data from transaction and data logs during sequential scans, or when writing a data log.",
  NULL, NULL, NULL);

static MYSQL_SYSVAR_STR(checkpoint_frequency, pbxt_checkpoint_frequency,
  PLUGIN_VAR_READONLY,
  "The size of the transaction data buffer which is allocate by each thread.",
  NULL, NULL, NULL);

static MYSQL_SYSVAR_STR(data_log_threshold, pbxt_data_log_threshold,
  PLUGIN_VAR_READONLY,
  "The maximum size of a data log file.",
  NULL, NULL, NULL);

static MYSQL_SYSVAR_STR(data_file_grow_size, pbxt_data_file_grow_size,
  PLUGIN_VAR_READONLY,
  "The amount by which the handle data files (.xtd) grow.",
  NULL, NULL, NULL);

static MYSQL_SYSVAR_STR(row_file_grow_size, pbxt_row_file_grow_size,
  PLUGIN_VAR_READONLY,
  "The amount by which the row pointer files (.xtr) grow.",
  NULL, NULL, NULL);

static MYSQL_SYSVAR_INT(garbage_threshold, xt_db_garbage_threshold,
	PLUGIN_VAR_OPCMDARG,
	"The percentage of garbage in a repository file before it is compacted.",
	NULL, NULL, XT_DL_DEFAULT_GARBAGE_LEVEL, 0, 100, 1);

static MYSQL_SYSVAR_INT(log_file_count, xt_db_log_file_count,
	PLUGIN_VAR_OPCMDARG,
	"The minimum number of transaction logs used.",
	NULL, NULL, XT_DL_DEFAULT_XLOG_COUNT, 1, 20000, 1);

static MYSQL_SYSVAR_INT(auto_increment_mode, xt_db_auto_increment_mode,
	PLUGIN_VAR_OPCMDARG,
	"The auto-increment mode, 0 = MySQL standard (default), 1 = previous ID's never reused.",
	NULL, NULL, XT_AUTO_INCREMENT_DEF, 0, 1, 1);

/* {RN145} */
static MYSQL_SYSVAR_INT(offline_log_function, xt_db_offline_log_function,
	PLUGIN_VAR_OPCMDARG,
	"Determines what happens to transaction logs when the are moved offline, 0 = recycle logs (default), 1 = delete logs (default on Mac OS X), 2 = keep logs.",
	NULL, NULL, XT_OFFLINE_LOG_FUNCTION_DEF, 0, 2, 1);

/* {RN150} */
static MYSQL_SYSVAR_INT(sweeper_priority, xt_db_sweeper_priority,
	PLUGIN_VAR_OPCMDARG,
	"Determines the priority of the background sweeper process, 0 = low (default), 1 = normal (same as user threads), 2 = high.",
	NULL, NULL, XT_PRIORITY_LOW, XT_PRIORITY_LOW, XT_PRIORITY_HIGH, 1);

#ifdef DRIZZLED
static MYSQL_SYSVAR_INT(max_threads, pbxt_max_threads,
	PLUGIN_VAR_OPCMDARG,
	"The maximum number of threads used by PBXT",
	NULL, NULL, 500, 20, 20000, 1);
#else
static MYSQL_SYSVAR_INT(max_threads, pbxt_max_threads,
	PLUGIN_VAR_OPCMDARG,
	"The maximum number of threads used by PBXT, 0 = set according to MySQL max_connections.",
	NULL, NULL, 0, 0, 20000, 1);
#endif

static struct st_mysql_sys_var* pbxt_system_variables[] = {
  MYSQL_SYSVAR(index_cache_size),
  MYSQL_SYSVAR(record_cache_size),
  MYSQL_SYSVAR(log_cache_size),
  MYSQL_SYSVAR(log_file_threshold),
  MYSQL_SYSVAR(transaction_buffer_size),
  MYSQL_SYSVAR(log_buffer_size),
  MYSQL_SYSVAR(checkpoint_frequency),
  MYSQL_SYSVAR(data_log_threshold),
  MYSQL_SYSVAR(data_file_grow_size),
  MYSQL_SYSVAR(row_file_grow_size),
  MYSQL_SYSVAR(garbage_threshold),
  MYSQL_SYSVAR(log_file_count),
  MYSQL_SYSVAR(auto_increment_mode),
  MYSQL_SYSVAR(offline_log_function),
  MYSQL_SYSVAR(sweeper_priority),
  MYSQL_SYSVAR(max_threads),
  NULL
};
#endif

#ifdef DRIZZLED
drizzle_declare_plugin(pbxt)
#else
mysql_declare_plugin(pbxt)
#endif
{
#ifndef DRIZZLED
	MYSQL_STORAGE_ENGINE_PLUGIN,
	&pbxt_storage_engine,
#endif
	"PBXT",
#ifdef DRIZZLED
	"1.0",
#endif
	"Paul McCullagh, PrimeBase Technologies GmbH",
	"High performance, multi-versioning transactional engine",
	PLUGIN_LICENSE_GPL,
	pbxt_init, /* Plugin Init */
	pbxt_end, /* Plugin Deinit */
#ifndef DRIZZLED
	0x0001 /* 0.1 */,
#endif
	NULL,                       /* status variables                */
#if MYSQL_VERSION_ID >= 50118
	pbxt_system_variables,		/* system variables                */
#else
	NULL,
#endif
	NULL						/* config options                  */
},
{
#ifndef DRIZZLED
	MYSQL_INFORMATION_SCHEMA_PLUGIN,
	&pbxt_statitics,
#endif
	"PBXT_STATISTICS",
#ifdef DRIZZLED
	"1.0",
#endif
	"Paul McCullagh, PrimeBase Technologies GmbH",
	"PBXT internal system statitics",
	PLUGIN_LICENSE_GPL,
	pbxt_init_statitics,						/* plugin init */
	pbxt_exit_statitics,						/* plugin deinit */
#ifndef DRIZZLED
	0x0005,
#endif
	NULL,										/* status variables */
	NULL,										/* system variables */
	NULL										/* config options */
}
#ifdef DRIZZLED
drizzle_declare_plugin_end;
#else
mysql_declare_plugin_end;
#endif

#if defined(XT_WIN) && defined(XT_COREDUMP)

/*
 * WINDOWS CORE DUMP SUPPORT
 *
 * MySQL supports core dumping on Windows with --core-file command line option. 
 * However it creates dumps with the MiniDumpNormal option which saves only stack traces.
 *
 * We instead (or in addition) create dumps with MiniDumpWithoutOptionalData option
 * which saves all available information. To enable core dumping enable XT_COREDUMP
 * at compile time.
 * In addition, pbxt_crash_debug must be set to TRUE which is the case if XT_CRASH_DEBUG
 * is defined.
 * This switch is also controlled by creating a file called "no-debug" or "crash-debug"
 * in the pbxt database directory.
 */

typedef enum _MINIDUMP_TYPE {
    MiniDumpNormal                         = 0x0000,
    MiniDumpWithDataSegs                   = 0x0001,
    MiniDumpWithFullMemory                 = 0x0002,
    MiniDumpWithHandleData                 = 0x0004,
    MiniDumpFilterMemory                   = 0x0008,
    MiniDumpScanMemory                     = 0x0010,
    MiniDumpWithUnloadedModules            = 0x0020,
    MiniDumpWithIndirectlyReferencedMemory = 0x0040,
    MiniDumpFilterModulePaths              = 0x0080,
    MiniDumpWithProcessThreadData          = 0x0100,
    MiniDumpWithPrivateReadWriteMemory     = 0x0200,
} MINIDUMP_TYPE;

typedef struct _MINIDUMP_EXCEPTION_INFORMATION {
    DWORD ThreadId;
    PEXCEPTION_POINTERS ExceptionPointers;
    BOOL ClientPointers;
} MINIDUMP_EXCEPTION_INFORMATION, *PMINIDUMP_EXCEPTION_INFORMATION;

typedef BOOL (WINAPI *MINIDUMPWRITEDUMP)(
	HANDLE hProcess, 
	DWORD dwPid, 
	HANDLE hFile, 
	MINIDUMP_TYPE DumpType,
	void *ExceptionParam,
	void *UserStreamParam,
	void *CallbackParam
	);

char base_path[_MAX_PATH] = {0};
char dump_path[_MAX_PATH] = {0};

void core_dump(struct _EXCEPTION_POINTERS *pExceptionInfo)
{
	SECURITY_ATTRIBUTES	sa = { sizeof(SECURITY_ATTRIBUTES), 0, 0 };
	int i;
	HMODULE hDll = NULL;
	HANDLE hFile;
	MINIDUMPWRITEDUMP pDump;
	char *end_ptr = base_path;

	MINIDUMP_EXCEPTION_INFORMATION ExInfo, *ExInfoPtr = NULL;

	if (pExceptionInfo) {
		ExInfo.ThreadId = GetCurrentThreadId();
		ExInfo.ExceptionPointers = pExceptionInfo;
		ExInfo.ClientPointers = NULL;
		ExInfoPtr = &ExInfo;
	}

	end_ptr = base_path + strlen(base_path);

	strcat(base_path, "DBGHELP.DLL" );
	hDll = LoadLibrary(base_path);
	*end_ptr = 0;
	if (hDll==NULL) {
		int err;
		err = HRESULT_CODE(GetLastError());
		hDll = LoadLibrary( "DBGHELP.DLL" );
		if (hDll==NULL) {
			err = HRESULT_CODE(GetLastError());
			return;
		}
	}

	pDump = (MINIDUMPWRITEDUMP)GetProcAddress( hDll, "MiniDumpWriteDump" );
	if (!pDump) {
		int err;
		err = HRESULT_CODE(GetLastError());
		return;
	}

	for (i = 1; i < INT_MAX; i++) {
		sprintf(dump_path, "%sPBXTCore%08d.dmp", base_path, i);
		hFile = CreateFile( dump_path, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_NEW,
							FILE_ATTRIBUTE_NORMAL, NULL );

		if ( hFile != INVALID_HANDLE_VALUE )
			break;

		if (HRESULT_CODE(GetLastError()) == ERROR_FILE_EXISTS )
			continue;

		return;
	}

	// write the dump
	BOOL bOK = pDump( GetCurrentProcess(), GetCurrentProcessId(), hFile, 
		MiniDumpWithPrivateReadWriteMemory, ExInfoPtr, NULL, NULL );

	CloseHandle(hFile);
}

LONG crash_filter( struct _EXCEPTION_POINTERS *pExceptionInfo )
{
	core_dump(pExceptionInfo);
	return EXCEPTION_EXECUTE_HANDLER;
}

void register_crash_filter()
{
	SetUnhandledExceptionFilter( (LPTOP_LEVEL_EXCEPTION_FILTER) crash_filter );
}

#endif // XT_WIN && XT_COREDUMP
