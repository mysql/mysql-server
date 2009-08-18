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
 * 2005-02-08	Paul McCullagh
 *
 * H&G2JCtL
 */

#include "xt_config.h"

#include <string.h>
#include <stdio.h>
#ifndef XT_WIN
#include <strings.h>
#endif
#include <ctype.h>
#include <time.h>

#ifdef DRIZZLED
#include <drizzled/common.h>
#include <mysys/thr_lock.h>
#include <drizzled/dtcollation.h>
#include <drizzled/plugin/storage_engine.h>
#else
#include "mysql_priv.h"
#endif

#include "table_xt.h"
#include "database_xt.h"
#include "heap_xt.h"
#include "strutil_xt.h"
#include "myxt_xt.h"
#include "cache_xt.h"
#include "trace_xt.h"
#include "index_xt.h"
#include "restart_xt.h"
#include "systab_xt.h"

#ifdef DEBUG
//#define TRACE_VARIATIONS
//#define TRACE_VARIATIONS_IN_DUP_CHECK
//#define DUMP_CHECK_TABLE
//#define CHECK_INDEX_ON_CHECK_TABLE
//#define TRACE_TABLE_IDS
//#define TRACE_FLUSH
//#define TRACE_CREATE_TABLES
#endif

#define CHECK_TABLE_STATS

#ifdef TRACE_TABLE_IDS
//#define PRINTF		xt_ftracef
#define PRINTF		xt_trace
#endif

/*
 * -----------------------------------------------------------------------
 * Internal structures
 */

#define XT_MAX_TABLE_FILE_NAME_SIZE		(XT_TABLE_NAME_SIZE+6+40)

/*
 * -----------------------------------------------------------------------
 * Compare paths:
 */

/* GOTCHA! The problem:
 *
 * The server uses names like: "./test/my_tab",
 * the BLOB streaming engine uses: "test/my_tab"
 * which leads to the same table being loaded twice.
 */
xtPublic int xt_tab_compare_paths(char *n1, char *n2)
{
	n1 = xt_last_2_names_of_path(n1);
	n2 = xt_last_2_names_of_path(n2);
	if (pbxt_ignore_case)
		return strcasecmp(n1, n2);
	return strcmp(n1, n2);
}

/*
 * This function only compares only the last 2 components of
 * the path because table names must differ in this area.
 */
xtPublic int xt_tab_compare_names(const char *n1, const char *n2)
{
	n1 = xt_last_2_names_of_path(n1);
	n2 = xt_last_2_names_of_path(n2);
	if (pbxt_ignore_case)
		return strcasecmp(n1, n2);
	return strcmp(n1, n2);
}

/*
 * -----------------------------------------------------------------------
 * Private utilities
 */

static xtBool tab_list_comp(void *key, void *data)
{
	XTTableHPtr	tab = (XTTableHPtr) data;

	return strcmp(xt_last_2_names_of_path((char *) key), xt_last_2_names_of_path(tab->tab_name->ps_path)) == 0;
}

static xtHashValue tab_list_hash(xtBool is_key, void *key_data)
{
	XTTableHPtr	tab = (XTTableHPtr) key_data;

	if (is_key)
		return xt_ht_hash(xt_last_2_names_of_path((char *) key_data));
	return xt_ht_hash(xt_last_2_names_of_path(tab->tab_name->ps_path));
}

static xtBool tab_list_comp_ci(void *key, void *data)
{
	XTTableHPtr	tab = (XTTableHPtr) data;

	return strcasecmp(xt_last_2_names_of_path((char *) key), xt_last_2_names_of_path(tab->tab_name->ps_path)) == 0;
}

static xtHashValue tab_list_hash_ci(xtBool is_key, void *key_data)
{
	XTTableHPtr	tab = (XTTableHPtr) key_data;

	if (is_key)
		return xt_ht_casehash(xt_last_2_names_of_path((char *) key_data));
	return xt_ht_casehash(xt_last_2_names_of_path(tab->tab_name->ps_path));
}

static void tab_list_free(XTThreadPtr self, void *data)
{
	XTTableHPtr		tab = (XTTableHPtr) data;
	XTDatabaseHPtr	db = tab->tab_db;
	XTTableEntryPtr	te_ptr;

	/* Remove the reference from the ID list, whem the table is
	 * removed from the name list:
	 */
	if ((te_ptr = (XTTableEntryPtr) xt_sl_find(self, db->db_table_by_id, &tab->tab_id)))
		te_ptr->te_table = NULL;

	if (tab->tab_dic.dic_table)
		tab->tab_dic.dic_table->removeReferences(self);
	xt_heap_release(self, tab);
}

static void tab_close_mapped_files(XTThreadPtr self, XTTableHPtr tab)
{
	if (tab->tab_rec_file) {
		xt_fs_release_file(self, tab->tab_rec_file);
		tab->tab_rec_file = NULL;
	}
	if (tab->tab_row_file) {
		xt_fs_release_file(self, tab->tab_row_file);
		tab->tab_row_file = NULL;
	}
}

static void tab_finalize(XTThreadPtr self, void *x)
{
	XTTableHPtr	tab = (XTTableHPtr) x;

	xt_exit_row_locks(&tab->tab_locks);

	xt_xres_exit_tab(self, tab);

	if (tab->tab_ind_free_list) {
		XTIndFreeListPtr list, flist;
		
		list = tab->tab_ind_free_list;
		while (list) {
			flist = list;
			list = list->fl_next_list;
			xt_free(self, flist);
		}
		tab->tab_ind_free_list = NULL;
	}

	if (tab->tab_ind_file) {
		xt_fs_release_file(self, tab->tab_ind_file);
		tab->tab_ind_file = NULL;
	}
	tab_close_mapped_files(self, tab);

	if (tab->tab_index_head) {
		xt_free(self, tab->tab_index_head);
		tab->tab_index_head = NULL;
	}

#ifdef TRACE_TABLE_IDS
	PRINTF("%s: free TABLE: db=%d tab=%d %s\n", self->t_name, (int) tab->tab_db ? tab->tab_db->db_id : 0, (int) tab->tab_id, 
		tab->tab_name ? xt_last_2_names_of_path(tab->tab_name->ps_path) : "?");
#endif
	if (tab->tab_name) {
		xt_free(self, tab->tab_name);
		tab->tab_name = NULL;
	}
	myxt_free_dictionary(self, &tab->tab_dic);
	if (tab->tab_free_locks) {
		tab->tab_seq.xt_op_seq_exit(self);
		xt_spinlock_free(self, &tab->tab_ainc_lock);
		xt_free_mutex(&tab->tab_rec_flush_lock);
		xt_free_mutex(&tab->tab_ind_flush_lock);
		xt_free_mutex(&tab->tab_dic_field_lock);
		xt_free_mutex(&tab->tab_row_lock);
		xt_free_mutex(&tab->tab_ind_lock);
		xt_free_mutex(&tab->tab_rec_lock);
		for (u_int i=0; i<XT_ROW_RWLOCKS; i++)
			XT_TAB_ROW_FREE_LOCK(self, &tab->tab_row_rwlock[i]);
	}
}

static void tab_onrelease(XTThreadPtr self, void *x)
{
	XTTableHPtr	tab = (XTTableHPtr) x;

	/* Signal threads waiting for exclusive use of the table: */
	if (tab->tab_db->db_tables)
		xt_ht_signal(self, tab->tab_db->db_tables);
}

/*
 * -----------------------------------------------------------------------
 * PUBLIC METHODS
 */

/*
 * This function sets the table name to "", if the file
 * does not belong to XT.
 */
xtPublic char *xt_tab_file_to_name(size_t size, char *tab_name, char *file_name)
{
	char	*cptr;
	size_t	len;

	file_name = xt_last_name_of_path(file_name);
	cptr = file_name + strlen(file_name) - 1;
	while (cptr > file_name && *cptr != '.')
		cptr--;
	if (cptr > file_name && *cptr == '.') {
		if (strcmp(cptr, ".xtl") == 0 || strcmp(cptr, ".xtr") == 0) {
			cptr--;
			while (cptr > file_name && isdigit(*cptr))
				cptr--;
		}
		else {
			const char **ext = pbxt_extensions;
			
			while (*ext) {
				if (strcmp(cptr, *ext) == 0)
					goto ret_name;
				ext++;
			}
			cptr = file_name;
		}
	}

	ret_name:
	len = cptr - file_name;
	if (len > size-1)
		len = size-1;

	memcpy(tab_name, file_name, len);
	tab_name[len] = 0;

	/* Return a pointer to what was removed! */
	return file_name + len;
}

static void tab_get_row_file_name(char *table_name, char *name, xtTableID tab_id)
{
	sprintf(table_name, "%s-%lu.xtr", name, (u_long) tab_id);
}

static void tab_get_data_file_name(char *table_name, char *name, xtTableID XT_UNUSED(tab_id))
{
	sprintf(table_name, "%s.xtd", name);
}

static void tab_get_index_file_name(char *table_name, char *name, xtTableID XT_UNUSED(tab_id))
{
	sprintf(table_name, "%s.xti", name);
}

static void tab_free_by_id(XTThreadPtr self, void *XT_UNUSED(thunk), void *item)
{
	XTTableEntryPtr	te_ptr = (XTTableEntryPtr) item;

	if (te_ptr->te_tab_name) {
		xt_free(self, te_ptr->te_tab_name);
		te_ptr->te_tab_name = NULL;
	}
	te_ptr->te_tab_id = 0;
	te_ptr->te_table = NULL;
}

static int tab_comp_by_id(XTThreadPtr XT_UNUSED(self), register const void *XT_UNUSED(thunk), register const void *a, register const void *b)
{
	xtTableID		te_id = *((xtTableID *) a);
	XTTableEntryPtr	te_ptr = (XTTableEntryPtr) b;

	if (te_id < te_ptr->te_tab_id)
		return -1;
	if (te_id == te_ptr->te_tab_id)
		return 0;
	return 1;
}

static void tab_free_path(XTThreadPtr self, void *XT_UNUSED(thunk), void *item)
{
	XTTablePathPtr	tp_ptr = *((XTTablePathPtr *) item);

	xt_free(self, tp_ptr);
}

static int tab_comp_path(XTThreadPtr XT_UNUSED(self), register const void *XT_UNUSED(thunk), register const void *a, register const void *b)
{
	char			*path = (char *) a;
	XTTablePathPtr	tp_ptr = *((XTTablePathPtr *) b);

	return xt_tab_compare_paths(path, tp_ptr->tp_path);
}

xtPublic void xt_describe_tables_init(XTThreadPtr self, XTDatabaseHPtr db, XTTableDescPtr td)
{
	td->td_db = db;
	td->td_path_idx = 0;
	if (td->td_path_idx < xt_sl_get_size(db->db_table_paths)) {
		XTTablePathPtr *tp_ptr;

		tp_ptr = (XTTablePathPtr *) xt_sl_item_at(db->db_table_paths, td->td_path_idx);
		td->td_tab_path = *tp_ptr;
		td->td_open_dir = xt_dir_open(self, td->td_tab_path->tp_path, "*.xtr");
	}
	else
		td->td_open_dir = NULL;
}

xtPublic xtBool xt_describe_tables_next(XTThreadPtr self, XTTableDescPtr td)
{
	char	*tab_name;
	xtBool	r = FALSE;

	enter_();
	retry:
	if (!td->td_open_dir)
		return_(FALSE);
	try_(a) {
		r = xt_dir_next(self, td->td_open_dir);
	}
	catch_(a) {
		xt_describe_tables_exit(self, td);
		throw_();
	}
	cont_(a);
	if (!r) {
		XTTablePathPtr *tp_ptr;

		if (td->td_path_idx+1 >= xt_sl_get_size(td->td_db->db_table_paths))
			return_(FALSE);

		if (td->td_open_dir)
			xt_dir_close(NULL, td->td_open_dir);
		td->td_open_dir = NULL;

		td->td_path_idx++;
		tp_ptr = (XTTablePathPtr *) xt_sl_item_at(td->td_db->db_table_paths, td->td_path_idx);
		td->td_tab_path = *tp_ptr;
		td->td_open_dir = xt_dir_open(self, td->td_tab_path->tp_path, "*.xtr");
		goto retry;
	}

	tab_name = xt_dir_name(self, td->td_open_dir);
	td->td_file_name = tab_name;
	td->td_tab_id = (xtTableID) xt_file_name_to_id(tab_name);
	xt_tab_file_to_name(XT_TABLE_NAME_SIZE, td->td_tab_name, tab_name);
	return_(TRUE);
}

xtPublic void xt_describe_tables_exit(XTThreadPtr XT_UNUSED(self), XTTableDescPtr td)
{
	if (td->td_open_dir)
		xt_dir_close(NULL, td->td_open_dir);
	td->td_open_dir = NULL;
	td->td_tab_path = NULL;
}

xtPublic void xt_tab_init_db(XTThreadPtr self, XTDatabaseHPtr db)
{
	XTTableDescRec		desc;
	XTTableEntryRec		te_tab;
	XTTableEntryPtr		te_ptr;
	XTTablePathPtr		db_path;
	char				pbuf[PATH_MAX];
	int					len;
	u_int				edx;

	enter_();
	pushr_(xt_tab_exit_db, db);
	if (pbxt_ignore_case)
		db->db_tables = xt_new_hashtable(self, tab_list_comp_ci, tab_list_hash_ci, tab_list_free, TRUE, TRUE);
	else
		db->db_tables = xt_new_hashtable(self, tab_list_comp, tab_list_hash, tab_list_free, TRUE, TRUE);
	db->db_table_by_id = xt_new_sortedlist(self, sizeof(XTTableEntryRec), 20, 20, tab_comp_by_id, db, tab_free_by_id, FALSE, FALSE);
	db->db_table_paths = xt_new_sortedlist(self, sizeof(XTTablePathPtr), 20, 20, tab_comp_path, db, tab_free_path, FALSE, FALSE);

	if (db->db_multi_path) {
		XTOpenFilePtr	of;
		char			*buffer, *ptr, *path;

		xt_strcpy(PATH_MAX, pbuf, db->db_main_path);
		xt_add_location_file(PATH_MAX, pbuf);
		if (xt_fs_exists(pbuf)) {
			of = xt_open_file(self, pbuf, XT_FS_DEFAULT);
			pushr_(xt_close_file, of);
			len = (int) xt_seek_eof_file(self, of);
			buffer = (char *) xt_malloc(self, len + 1);
			pushr_(xt_free, buffer);
			if (!xt_pread_file(of, 0, len, len, buffer, NULL, &self->st_statistics.st_x, self))
				xt_throw(self);
			buffer[len] = 0;
			ptr = buffer;
			while (*ptr) {
				/* Ignore preceeding space: */
				while (*ptr && isspace(*ptr))
					ptr++;
				path = ptr;
				while (*ptr && *ptr != '\n' && *ptr != '\r') {
#ifdef XT_WIN
					/* Undo the conversion below: */
					if (*ptr == '/')
						*ptr = '\\';
#endif
					ptr++;
				}
				if (*path != '#' && ptr > path) {
					len = (int) (ptr - path);
					db_path = (XTTablePathPtr) xt_malloc(self, offsetof(XTTablePathRec, tp_path) + len + 1);
					db_path->tp_tab_count = 0;
					memcpy(db_path->tp_path, path, len);
					db_path->tp_path[len] = 0;
					xt_sl_insert(self, db->db_table_paths, db_path->tp_path, &db_path);
				}
				ptr++;
			}
			freer_(); // xt_free(buffer)
			freer_(); // xt_close_file(of)
		}
	}
	else {
		len = (int) strlen(db->db_main_path);
		db_path = (XTTablePathPtr) xt_malloc(self, offsetof(XTTablePathRec, tp_path) + len + 1);
		db_path->tp_tab_count = 0;
		strcpy(db_path->tp_path, db->db_main_path);
		xt_sl_insert(self, db->db_table_paths, db_path->tp_path, &db_path);
	}

	xt_describe_tables_init(self, db, &desc);
	pushr_(xt_describe_tables_exit, &desc);
	while (xt_describe_tables_next(self, &desc)) {
		te_tab.te_tab_id = desc.td_tab_id;

		if (te_tab.te_tab_id > db->db_curr_tab_id)
			db->db_curr_tab_id = te_tab.te_tab_id;

		te_tab.te_tab_name = xt_dup_string(self, desc.td_tab_name);
		te_tab.te_tab_path = desc.td_tab_path;
		desc.td_tab_path->tp_tab_count++;
		te_tab.te_table = NULL;
		xt_sl_insert(self, db->db_table_by_id, &desc.td_tab_id, &te_tab);
	}
	freer_(); // xt_describe_tables_exit(&desc)

	/* 
	 * The purpose of this code is to ensure that all tables are opened and cached,
	 * which is actually only required if tables have foreign key references.
	 *
	 * In other words, a side affect of this code is that FK references between tables
	 * are registered, and checked.
	 *
	 * Unfortunately we don't know if a table is referenced by a FK, so we have to open
	 * all tables.
	 * 
	 * Cannot open tables in the loop above because db->db_table_by_id which is built 
	 * above is used by xt_use_table_no_lock() 
	 */
	xt_enum_tables_init(&edx);
	while ((te_ptr = xt_enum_tables_next(self, db, &edx))) {
		xt_strcpy(PATH_MAX, pbuf, te_ptr->te_tab_path->tp_path);
		xt_add_dir_char(PATH_MAX, pbuf);
		xt_strcat(PATH_MAX, pbuf, te_ptr->te_tab_name);
		xt_heap_release(self, xt_use_table_no_lock(self, db, (XTPathStrPtr)pbuf, FALSE, FALSE, NULL, NULL));
	}

	popr_(); // Discard xt_tab_exit_db(db)
	exit_();
}

static void tab_save_table_paths(XTThreadPtr self, XTDatabaseHPtr db)
{
	XTTablePathPtr		*tp_ptr;
	XTStringBufferRec	buffer;
	XTOpenFilePtr		of;
	char				path[PATH_MAX];

	memset(&buffer, 0, sizeof(buffer));

	xt_strcpy(PATH_MAX, path, db->db_main_path);
	xt_add_location_file(PATH_MAX, path);

	if (xt_sl_get_size(db->db_table_paths)) {
		pushr_(xt_sb_free, &buffer);
		for (u_int i=0; i<xt_sl_get_size(db->db_table_paths); i++) {
			tp_ptr = (XTTablePathPtr *) xt_sl_item_at(db->db_table_paths, i);
			xt_sb_concat(self, &buffer, (*tp_ptr)->tp_path);
			xt_sb_concat(self, &buffer, "\n");
		}

#ifdef XT_WIN
		/* To make the location file cross-platform (at least
		 * as long as relative paths are used) we replace all '\' 
		 * with '/': */
		char *ptr;
		
		ptr = buffer.sb_cstring;
		while (*ptr) {
			if (*ptr == '\\')
				*ptr = '/';
			ptr++;
		}
#endif

		of = xt_open_file(self, path, XT_FS_CREATE | XT_FS_MAKE_PATH);
		pushr_(xt_close_file, of);
		if (!xt_pwrite_file(of, 0, strlen(buffer.sb_cstring), buffer.sb_cstring, &self->st_statistics.st_x, self))
			xt_throw(self);
		xt_set_eof_file(self, of, strlen(buffer.sb_cstring));
		freer_(); // xt_close_file(of)
		
		freer_(); // xt_sb_free(&buffer);
	}
	else
		xt_fs_delete(NULL, path);
}

static XTTablePathPtr tab_get_table_path(XTThreadPtr self, XTDatabaseHPtr db, XTPathStrPtr tab_name, xtBool save_it)
{
	XTTablePathPtr	*tp, tab_path;
	char			path[PATH_MAX];

	xt_strcpy(PATH_MAX, path, tab_name->ps_path);
	xt_remove_last_name_of_path(path);
	xt_remove_dir_char(path);
	tp = (XTTablePathPtr *) xt_sl_find(self, db->db_table_paths, path);
	if (tp)
		tab_path = *tp;
	else {
		int len = (int) strlen(path);

		tab_path = (XTTablePathPtr) xt_malloc(self, offsetof(XTTablePathRec, tp_path) + len + 1);
		tab_path->tp_tab_count = 0;
		memcpy(tab_path->tp_path, path, len);
		tab_path->tp_path[len] = 0;
		xt_sl_insert(self, db->db_table_paths, tab_path->tp_path, &tab_path);
		if (save_it) {
			tab_save_table_paths(self, db);
			if (xt_sl_get_size(db->db_table_paths) == 1) {
				XTSystemTableShare::createSystemTables(self, db);
			}
		}
	}
	tab_path->tp_tab_count++;
	return tab_path;
}

static void tab_remove_table_path(XTThreadPtr self, XTDatabaseHPtr db, XTTablePathPtr tab_path)
{
	if (tab_path->tp_tab_count > 0) {
		tab_path->tp_tab_count--;
		if (tab_path->tp_tab_count == 0) {
			xt_sl_delete(self, db->db_table_paths, tab_path->tp_path);
			tab_save_table_paths(self, db);
		}
	}
}

static void tab_free_table_path(XTThreadPtr self, XTTablePathPtr tab_path)
{
	XTDatabaseHPtr db = self->st_database;

	tab_remove_table_path(self, db, tab_path);
}

xtPublic void xt_tab_exit_db(XTThreadPtr self, XTDatabaseHPtr db)
{
	if (db->db_tables) {
		xt_free_hashtable(self, db->db_tables);
		db->db_tables = NULL;
	}
	if (db->db_table_by_id) {
		xt_free_sortedlist(self, db->db_table_by_id);
		db->db_table_by_id = NULL;
	}
	if (db->db_table_paths) {
		xt_free_sortedlist(self, db->db_table_paths);
		db->db_table_paths = NULL;
	}
}

static void tab_check_table(XTThreadPtr self, XTTableHPtr XT_UNUSED(tab))
{
	(void) self;
	enter_();
	exit_();
}

xtPublic void xt_check_tables(XTThreadPtr self)
{
	u_int					edx;
	XTTableEntryPtr			te_ptr;
	volatile XTTableHPtr	tab;
	char					path[PATH_MAX];

	enter_();
	xt_logf(XT_INFO, "Check %s: Table...\n", self->st_database->db_main_path);
	xt_enum_tables_init(&edx);
	try_(a) {
		for (;;) {
			xt_ht_lock(self, self->st_database->db_tables);
			pushr_(xt_ht_unlock, self->st_database->db_tables);
			te_ptr = xt_enum_tables_next(self, self->st_database, &edx);
			freer_(); // xt_ht_unlock(db->db_tables)
			if (!te_ptr)
				break;
			xt_strcpy(PATH_MAX, path, te_ptr->te_tab_path->tp_path);
			xt_add_dir_char(PATH_MAX, path);
			xt_strcat(PATH_MAX, path, te_ptr->te_tab_name);
			tab = xt_use_table(self, (XTPathStrPtr) path, FALSE, FALSE, NULL);
			tab_check_table(self, tab);
			xt_heap_release(self, tab);
			tab = NULL;
		}
	}
	catch_(a) {
		if (tab)
			xt_heap_release(self, tab);
		throw_();
	}
	cont_(a);
	exit_();
}

xtPublic xtBool xt_table_exists(XTDatabaseHPtr db)
{
	return xt_sl_get_size(db->db_table_by_id) > 0;
}

/*
 * Enumerate all tables in the current database.
 */

xtPublic void xt_enum_tables_init(u_int *edx)
{
	*edx = 0;
}

xtPublic XTTableEntryPtr xt_enum_tables_next(XTThreadPtr XT_UNUSED(self), XTDatabaseHPtr db, u_int *edx)
{
	XTTableEntryPtr en_ptr;

	if (*edx >= xt_sl_get_size(db->db_table_by_id))
		return NULL;
	en_ptr = (XTTableEntryPtr) xt_sl_item_at(db->db_table_by_id, *edx);
	(*edx)++;
	return en_ptr;
}

xtPublic void xt_enum_files_of_tables_init(XTPathStrPtr tab_name, xtTableID tab_id, XTFilesOfTablePtr ft)
{
	ft->ft_state = 0;
	ft->ft_tab_name = tab_name;
	ft->ft_tab_id = tab_id;
}

xtPublic xtBool xt_enum_files_of_tables_next(XTFilesOfTablePtr ft)
{
	char file_name[XT_MAX_TABLE_FILE_NAME_SIZE];

	retry:
	switch (ft->ft_state) {
		case 0:
			tab_get_row_file_name(file_name, xt_last_name_of_path(ft->ft_tab_name->ps_path), ft->ft_tab_id);
			break;
		case 1:
			tab_get_data_file_name(file_name, xt_last_name_of_path(ft->ft_tab_name->ps_path), ft->ft_tab_id);
			break;
		case 2:
			tab_get_index_file_name(file_name, xt_last_name_of_path(ft->ft_tab_name->ps_path), ft->ft_tab_id);
			break;
		default:
			return FAILED;
	}

	ft->ft_state++;
	xt_strcpy(PATH_MAX, ft->ft_file_path, ft->ft_tab_name->ps_path);
	xt_remove_last_name_of_path(ft->ft_file_path);
	xt_strcat(PATH_MAX, ft->ft_file_path, file_name);
	if (!xt_fs_exists(ft->ft_file_path))
		goto retry;

	return TRUE;
}

static xtBool tab_find_table(XTThreadPtr self, XTDatabaseHPtr db, XTPathStrPtr name, xtTableID *tab_id)
{
	u_int			edx;
	XTTableEntryPtr	te_ptr;
	char			path[PATH_MAX];

	xt_enum_tables_init(&edx);
	while ((te_ptr = xt_enum_tables_next(self, db, &edx))) {
		xt_strcpy(PATH_MAX, path, te_ptr->te_tab_path->tp_path);
		xt_add_dir_char(PATH_MAX, path);
		xt_strcat(PATH_MAX, path, te_ptr->te_tab_name);
		if (xt_tab_compare_names(path, name->ps_path) == 0) {
			*tab_id = te_ptr->te_tab_id;
			return TRUE;
		}
	}
	return FALSE;
}

xtPublic void xt_tab_disable_index(XTTableHPtr tab, u_int ind_error)
{
	tab->tab_dic.dic_disable_index = ind_error;
	xt_tab_set_table_repair_pending(tab);
}

xtPublic void xt_tab_set_index_error(XTTableHPtr tab)
{
	switch (tab->tab_dic.dic_disable_index) {
		case XT_INDEX_OK:
			break;
		case XT_INDEX_TOO_OLD:
			xt_register_taberr(XT_REG_CONTEXT, XT_ERR_INDEX_OLD_VERSION, tab->tab_name);
			break;
		case XT_INDEX_TOO_NEW:
			xt_register_taberr(XT_REG_CONTEXT, XT_ERR_INDEX_NEW_VERSION, tab->tab_name);
			break;
		case XT_INDEX_BAD_BLOCK:
			char number[40];

			sprintf(number, "%d", (int) tab->tab_index_page_size);
			xt_register_i2xterr(XT_REG_CONTEXT, XT_ERR_BAD_IND_BLOCK_SIZE, xt_last_name_of_path(tab->tab_name->ps_path), number);
			break;
		case XT_INDEX_CORRUPTED:
			xt_register_taberr(XT_REG_CONTEXT, XT_ERR_INDEX_CORRUPTED, tab->tab_name);
			break;
		case XT_INDEX_MISSING:
			xt_register_taberr(XT_REG_CONTEXT, XT_ERR_INDEX_MISSING, tab->tab_name);
			break;
	}
}

static void tab_load_index_header(XTThreadPtr self, XTTableHPtr tab, XTOpenFilePtr file, XTPathStrPtr table_name)
{
	XT_NODE_TEMP;
	XTIndexPtr			*ind;
	xtWord1				*data;
	XTIndexFormatDPtr	index_fmt;

	/* Load the pointers: */
	if (tab->tab_index_head)
		xt_free_ns(tab->tab_index_head);
	tab->tab_index_head = (XTIndexHeadDPtr) xt_calloc(self, XT_INDEX_HEAD_SIZE);

	if (file) {
		if (!xt_pread_file(file, 0, XT_INDEX_HEAD_SIZE, 0, tab->tab_index_head, NULL, &self->st_statistics.st_ind, self))
			xt_throw(self);

		tab->tab_index_format_offset = XT_GET_DISK_4(tab->tab_index_head->tp_format_offset_4);
		index_fmt = (XTIndexFormatDPtr) (((xtWord1 *) tab->tab_index_head) + tab->tab_index_format_offset);

		/* If the table version is less than or equal to an incompatible (unsupported
		 * version), or greater than the current version, then we cannot open this table
		 */
		if (XT_GET_DISK_2(index_fmt->if_tab_version_2) <= XT_TAB_INCOMPATIBLE_VERSION ||
			XT_GET_DISK_2(index_fmt->if_tab_version_2) > XT_TAB_CURRENT_VERSION) {
			switch (XT_GET_DISK_2(index_fmt->if_tab_version_2)) {
				case 4: 
					xt_throw_tabcolerr(XT_CONTEXT, XT_ERR_UPGRADE_TABLE, table_name, "0.9.91 Beta");
					break;
				case 3: 
					xt_throw_tabcolerr(XT_CONTEXT, XT_ERR_UPGRADE_TABLE, table_name, "0.9.85 Beta");
					break;
				default:
					xt_throw_taberr(XT_CONTEXT, XT_ERR_BAD_TABLE_VERSION, table_name);
					break;
			}
			return;
		}

		tab->tab_dic.dic_index_ver = XT_GET_DISK_2(index_fmt->if_ind_version_2);
		tab->tab_dic.dic_disable_index = XT_INDEX_OK;

		if (tab->tab_dic.dic_index_ver == 1) {
			tab->tab_index_header_size = 1024 * 16;
			tab->tab_index_page_size = 1024 * 16;
		}
		else {
			tab->tab_index_header_size = XT_GET_DISK_4(tab->tab_index_head->tp_header_size_4);
			tab->tab_index_page_size = XT_GET_DISK_4(index_fmt->if_page_size_4);
		}	

#ifdef XT_USE_LAZY_DELETE
		if (tab->tab_dic.dic_index_ver <= XT_IND_NO_LAZY_DELETE)
			tab->tab_dic.dic_no_lazy_delete = TRUE;
		else
			tab->tab_dic.dic_no_lazy_delete = FALSE;
#else
		tab->tab_dic.dic_no_lazy_delete = TRUE;
#endif

		/* Incorrect version of index is handled by allowing a sequential scan, but no index access.
		 * Recovery with the wrong index type will not recover the indexes, a REPAIR TABLE
		 * will be required!
		 */
		if (tab->tab_dic.dic_index_ver != XT_IND_CURRENT_VERSION) {
			switch (tab->tab_dic.dic_index_ver) {
				case XT_IND_NO_LAZY_DELETE:
				case XT_IND_LAZY_DELETE_OK:
					/* I can handle this type of index. */
					break;
				default:
					if (tab->tab_dic.dic_index_ver < XT_IND_CURRENT_VERSION)
						xt_tab_disable_index(tab, XT_INDEX_TOO_OLD);
					else
						xt_tab_disable_index(tab, XT_INDEX_TOO_NEW);
					break;
			}
		}
		else if (tab->tab_index_page_size != XT_INDEX_PAGE_SIZE)
			xt_tab_disable_index(tab, XT_INDEX_BAD_BLOCK);
	}
	else {
		memset(tab->tab_index_head, 0, XT_INDEX_HEAD_SIZE);
		xt_tab_disable_index(tab, XT_INDEX_MISSING);
		tab->tab_index_header_size = XT_INDEX_HEAD_SIZE;
		tab->tab_index_page_size = XT_INDEX_PAGE_SIZE;
		tab->tab_dic.dic_index_ver = 0;
		tab->tab_index_format_offset = 0;
	}

	
	if (tab->tab_dic.dic_disable_index) {
		xt_tab_set_index_error(tab);
		xt_log_and_clear_exception_ns();
	}

	if (tab->tab_dic.dic_disable_index) {
		/* Reset, as if we have empty indexes.
		 * Flush will wipe things out, of course.
		 * REPAIR TABLE will be required...
		 */
		XT_NODE_ID(tab->tab_ind_eof) = 1;
		XT_NODE_ID(tab->tab_ind_free) = 0;

		ind = tab->tab_dic.dic_keys;
		for (u_int i=0; i<tab->tab_dic.dic_key_count; i++, ind++)
			XT_NODE_ID((*ind)->mi_root) = 0;
	}
	else {
		XT_NODE_ID(tab->tab_ind_eof) = (xtIndexNodeID) XT_GET_DISK_6(tab->tab_index_head->tp_ind_eof_6);
		XT_NODE_ID(tab->tab_ind_free) = (xtIndexNodeID) XT_GET_DISK_6(tab->tab_index_head->tp_ind_free_6);

		data = tab->tab_index_head->tp_data;
		ind = tab->tab_dic.dic_keys;
		for (u_int i=0; i<tab->tab_dic.dic_key_count; i++, ind++) {
			(*ind)->mi_root = XT_GET_NODE_REF(tab, data);
			data += XT_NODE_REF_SIZE;
		}
	}
}

static void tab_load_table_format(XTThreadPtr self, XTOpenFilePtr file, XTPathStrPtr table_name, size_t *ret_format_offset, size_t *ret_head_size, XTDictionaryPtr dic)
{
	XTDiskValue4		size_buf;
	size_t				head_size;
	XTTableFormatDRec	tab_fmt;
	size_t				fmt_size;

	if (!xt_pread_file(file, 0, 4, 4, &size_buf, NULL, &self->st_statistics.st_rec, self))
		xt_throw(self);

	head_size = XT_GET_DISK_4(size_buf);
	*ret_format_offset = head_size;

	/* Load the table format information: */
	if (!xt_pread_file(file, head_size, offsetof(XTTableFormatDRec, tf_definition), offsetof(XTTableFormatDRec, tf_tab_version_2) + 2, &tab_fmt, NULL, &self->st_statistics.st_rec, self))
		xt_throw(self);

	/* If the table version is less than or equal to an incompatible (unsupported
	 * version), or greater than the current version, then we cannot open this table
	 */
	if (XT_GET_DISK_2(tab_fmt.tf_tab_version_2) <= XT_TAB_INCOMPATIBLE_VERSION ||
		XT_GET_DISK_2(tab_fmt.tf_tab_version_2) > XT_TAB_CURRENT_VERSION) {
		switch (XT_GET_DISK_2(tab_fmt.tf_tab_version_2)) {
			case 4: 
				xt_throw_tabcolerr(XT_CONTEXT, XT_ERR_UPGRADE_TABLE, table_name, "0.9.91 Beta");
				break;
			case 3: 
				xt_throw_tabcolerr(XT_CONTEXT, XT_ERR_UPGRADE_TABLE, table_name, "0.9.85 Beta");
				break;
			default:
				xt_throw_taberr(XT_CONTEXT, XT_ERR_BAD_TABLE_VERSION, table_name);
				break;
		}
		return;
	}

	fmt_size = XT_GET_DISK_4(tab_fmt.tf_format_size_4);
	*ret_head_size = XT_GET_DISK_4(tab_fmt.tf_tab_head_size_4);
	dic->dic_rec_size = XT_GET_DISK_4(tab_fmt.tf_rec_size_4);
	dic->dic_rec_fixed = XT_GET_DISK_1(tab_fmt.tf_rec_fixed_1);
	dic->dic_tab_flags = XT_GET_DISK_2(tab_fmt.tf_tab_flags_2);
	dic->dic_min_auto_inc = XT_GET_DISK_8(tab_fmt.tf_min_auto_inc_8);
	if (fmt_size > offsetof(XTTableFormatDRec, tf_definition)) {
		size_t	def_size = fmt_size - offsetof(XTTableFormatDRec, tf_definition);
		char	*def_sql;

		pushsr_(def_sql, xt_free, (char *) xt_malloc(self, def_size));
		if (!xt_pread_file(file, head_size+offsetof(XTTableFormatDRec, tf_definition), def_size, def_size, def_sql, NULL, &self->st_statistics.st_rec, self))
			xt_throw(self);
		dic->dic_table = xt_ri_create_table(self, false, table_name, def_sql, myxt_create_table_from_table(self, dic->dic_my_table));
		freer_(); // xt_free(def_sql)
	}
	else
		dic->dic_table = myxt_create_table_from_table(self, dic->dic_my_table);
}

static void tab_load_table_header(XTThreadPtr self, XTTableHPtr tab, XTOpenFilePtr file)
{
	XTTableHeadDRec	rec_head;

	if (!xt_pread_file(file, 0, sizeof(XTTableHeadDRec), sizeof(XTTableHeadDRec), (xtWord1 *) &rec_head, NULL, &self->st_statistics.st_rec, self))
		xt_throw(self);

	tab->tab_head_op_seq = XT_GET_DISK_4(rec_head.th_op_seq_4);
	tab->tab_head_row_free_id = (xtRowID) XT_GET_DISK_6(rec_head.th_row_free_6);
	tab->tab_head_row_eof_id = (xtRowID) XT_GET_DISK_6(rec_head.th_row_eof_6);
	tab->tab_head_row_fnum = (xtWord4) XT_GET_DISK_6(rec_head.th_row_fnum_6);
	tab->tab_head_rec_free_id = (xtRecordID) XT_GET_DISK_6(rec_head.th_rec_free_6);
	tab->tab_head_rec_eof_id = (xtRecordID) XT_GET_DISK_6(rec_head.th_rec_eof_6);
	tab->tab_head_rec_fnum = (xtWord4) XT_GET_DISK_6(rec_head.th_rec_fnum_6);
}

xtPublic void xt_tab_store_header(XTOpenTablePtr ot, XTTableHeadDPtr rec_head)
{
	XTTableHPtr tab = ot->ot_table;

	XT_SET_DISK_4(rec_head->th_op_seq_4, tab->tab_head_op_seq);
	XT_SET_DISK_6(rec_head->th_row_free_6, tab->tab_head_row_free_id);
	XT_SET_DISK_6(rec_head->th_row_eof_6, tab->tab_head_row_eof_id);
	XT_SET_DISK_6(rec_head->th_row_fnum_6, tab->tab_head_row_fnum);
	XT_SET_DISK_6(rec_head->th_rec_free_6, tab->tab_head_rec_free_id);
	XT_SET_DISK_6(rec_head->th_rec_eof_6, tab->tab_head_rec_eof_id);
	XT_SET_DISK_6(rec_head->th_rec_fnum_6, tab->tab_head_rec_fnum);
}

xtPublic xtBool xt_tab_write_header(XTOpenTablePtr ot, XTTableHeadDPtr rec_head, struct XTThread *thread)
{
	if (!XT_PWRITE_RR_FILE(ot->ot_rec_file, offsetof(XTTableHeadDRec, th_op_seq_4), 40, (xtWord1 *) rec_head->th_op_seq_4, &thread->st_statistics.st_rec, thread))
		return FAILED;
	if (!XT_FLUSH_RR_FILE(ot->ot_rec_file, &thread->st_statistics.st_rec, thread))
		return FAILED;
	return OK;
}

xtPublic xtBool xt_tab_write_min_auto_inc(XTOpenTablePtr ot)
{
	xtWord1		value[8];
	off_t		offset;

	XT_SET_DISK_8(value, ot->ot_table->tab_dic.dic_min_auto_inc);
	offset = ot->ot_table->tab_table_format_offset + offsetof(XTTableFormatDRec, tf_min_auto_inc_8);
	if (!XT_PWRITE_RR_FILE(ot->ot_rec_file, offset, 8, value, &ot->ot_thread->st_statistics.st_rec, ot->ot_thread))
		return FAILED;
	if (!XT_FLUSH_RR_FILE(ot->ot_rec_file, &ot->ot_thread->st_statistics.st_rec, ot->ot_thread))
		return FAILED;
	return OK;
}

/* a helper function to remove table from the open tables hash on exception
 * used in tab_new_handle() below
 */
static void xt_del_from_db_tables_ht(XTThreadPtr self, XTTableHPtr tab)
{
	XTTableEntryPtr	te_ptr;
	XTDatabaseHPtr	db = tab->tab_db;
	xtTableID		tab_id = tab->tab_id;

	/* Oops! should use tab->tab_name, instead of tab! */
	xt_ht_del(self, db->db_tables, tab->tab_name);

	/* Remove the reference from the ID list, when a table is
	 * removed from the table name list:
	 */
	if ((te_ptr = (XTTableEntryPtr) xt_sl_find(self, db->db_table_by_id, &tab_id)))
		te_ptr->te_table = NULL;
}

/*
 * Create a new table handle (i.e. open a table).
 * Return NULL if the table is missing, and it is OK for the table
 * to be missing.
 */
static int tab_new_handle(XTThreadPtr self, XTTableHPtr *r_tab, XTDatabaseHPtr db, xtTableID tab_id, XTPathStrPtr tab_path, xtBool missing_ok, XTDictionaryPtr dic)
{
	char			path[PATH_MAX];
	XTTableHPtr		tab;
	char			file_name[XT_MAX_TABLE_FILE_NAME_SIZE];
	XTOpenFilePtr	of_rec, of_ind;
	XTTableEntryPtr	te_ptr;
	size_t			tab_format_offset;
	size_t			tab_head_size;

	enter_();

	tab = (XTTableHPtr) xt_heap_new(self, sizeof(XTTableHRec), tab_finalize);
	pushr_(xt_heap_release, tab);

	tab->tab_name = (XTPathStrPtr) xt_dup_string(self, tab_path->ps_path);
	tab->tab_db = db;
	tab->tab_id = tab_id;
#ifdef TRACE_TABLE_IDS
	PRINTF("%s: allocated TABLE: db=%d tab=%d %s\n", self->t_name, (int) db->db_id, (int) tab->tab_id, xt_last_2_names_of_path(tab->tab_name->ps_path));
#endif

	if (dic) {
		myxt_move_dictionary(&tab->tab_dic, dic);
		myxt_setup_dictionary(self, &tab->tab_dic);
	}
	else {
		if (!myxt_load_dictionary(self, &tab->tab_dic, db, tab_path)) {
			freer_(); // xt_heap_release(tab)
			return_(XT_TAB_NO_DICTIONARY);
		}
	}

	tab->tab_seq.xt_op_seq_init(self);
	xt_spinlock_init_with_autoname(self, &tab->tab_ainc_lock);
	xt_init_mutex_with_autoname(self, &tab->tab_rec_flush_lock);
	xt_init_mutex_with_autoname(self, &tab->tab_ind_flush_lock);
	xt_init_mutex_with_autoname(self, &tab->tab_dic_field_lock);
	xt_init_mutex_with_autoname(self, &tab->tab_row_lock);
	xt_init_mutex_with_autoname(self, &tab->tab_ind_lock);
	xt_init_mutex_with_autoname(self, &tab->tab_rec_lock);
	for (u_int i=0; i<XT_ROW_RWLOCKS; i++)
		XT_TAB_ROW_INIT_LOCK(self, &tab->tab_row_rwlock[i]);
	tab->tab_free_locks = TRUE;

	xt_strcpy(PATH_MAX, path, tab_path->ps_path);
	xt_remove_last_name_of_path(path);
	tab_get_row_file_name(file_name, xt_last_name_of_path(tab_path->ps_path), tab_id);
	xt_strcat(PATH_MAX, path, file_name);
	tab->tab_row_file = xt_fs_get_file(self, path);

	xt_remove_last_name_of_path(path);
	tab_get_data_file_name(file_name, xt_last_name_of_path(tab_path->ps_path), tab_id);
	xt_strcat(PATH_MAX, path, file_name);
	tab->tab_rec_file = xt_fs_get_file(self, path);

	xt_remove_last_name_of_path(path);
	tab_get_index_file_name(file_name, xt_last_name_of_path(tab_path->ps_path), tab_id);
	xt_strcat(PATH_MAX, path, file_name);
	tab->tab_ind_file = xt_fs_get_file(self, path);

	of_ind = xt_open_file(self, tab->tab_ind_file->fil_path, XT_FS_MISSING_OK);
	if (of_ind) {
		pushr_(xt_close_file, of_ind);
		tab_load_index_header(self, tab, of_ind, tab_path);
		freer_(); // xt_close_file(of_ind)
	}
	else
		tab_load_index_header(self, tab, of_ind, tab_path);

	of_rec = xt_open_file(self, tab->tab_rec_file->fil_path, missing_ok ? XT_FS_MISSING_OK : XT_FS_DEFAULT);
	if (!of_rec) {
		freer_(); // xt_heap_release(tab)
		return_(XT_TAB_NOT_FOUND);
	}
	pushr_(xt_close_file, of_rec);
	tab_load_table_format(self, of_rec, tab_path, &tab_format_offset, &tab_head_size, &tab->tab_dic);
	tab->tab_table_format_offset = tab_format_offset;
	tab->tab_table_head_size = tab_head_size;
	tab->tab_dic.dic_table->dt_table = tab;
	tab_load_table_header(self, tab, of_rec);
	freer_(); // xt_close_file(of_rec)

	tab->tab_seq.xt_op_seq_set(self, tab->tab_head_op_seq+1);
	tab->tab_row_eof_id = tab->tab_head_row_eof_id;
	tab->tab_row_free_id = tab->tab_head_row_free_id;
	tab->tab_row_fnum = tab->tab_head_row_fnum;
	tab->tab_rec_eof_id = tab->tab_head_rec_eof_id;
	tab->tab_rec_free_id = tab->tab_head_rec_free_id;
	tab->tab_rec_fnum = tab->tab_head_rec_fnum;

	tab->tab_rows.xt_tc_setup(tab, sizeof(XTTabRowHeadDRec), sizeof(XTTabRowRefDRec));
	tab->tab_recs.xt_tc_setup(tab, tab_head_size, tab->tab_dic.dic_rec_size);

	xt_xres_init_tab(self, tab);

	if (!xt_init_row_locks(&tab->tab_locks))
		xt_throw(self);

	xt_heap_set_release_callback(self, tab, tab_onrelease);

	tab->tab_repair_pending = xt_tab_is_table_repair_pending(tab);

	popr_(); // Discard xt_heap_release(tab)

	xt_ht_put(self, db->db_tables, tab);

	/* Add a reference to the ID list, when a table is
	 * added to the table name list:
	 */
	if ((te_ptr = (XTTableEntryPtr) xt_sl_find(self, db->db_table_by_id, &tab->tab_id)))
		te_ptr->te_table = tab;

    /* Moved from after xt_init_row_locks() above, so that calling
     * xt_use_table_no_lock() with no_load == FALSE from attachReferences()
     * will work if we have cyclic foreign key references.
     */ 
	if (tab->tab_dic.dic_table) {
		pushr_(xt_del_from_db_tables_ht, tab);
		tab->tab_dic.dic_table->attachReferences(self, db);
		popr_();
	}

	*r_tab = tab;
	return_(XT_TAB_OK);
}


/*
 * Get a reference to a table in the current database. The table reference is valid,
 * as long as the thread is using the database!!!
 */
xtPublic XTTableHPtr xt_use_table_no_lock(XTThreadPtr self, XTDatabaseHPtr db, XTPathStrPtr name, xtBool no_load, xtBool missing_ok, XTDictionaryPtr dic, xtBool *opened)
{
	XTTableHPtr tab;

	if (!db)
		xt_throw_xterr(XT_CONTEXT, XT_ERR_NO_DATABASE_IN_USE);

	tab = (XTTableHPtr) xt_ht_get(self, db->db_tables, name);
	if (!tab && !no_load) {
		xtTableID	tab_id = 0;

		if (!tab_find_table(self, db, name, &tab_id)) {
			if (missing_ok)
				return NULL;
			xt_throw_taberr(XT_CONTEXT, XT_ERR_TABLE_NOT_FOUND, name);
		}

		if (tab_new_handle(self, &tab, db, tab_id, name, FALSE, dic) == XT_TAB_NO_DICTIONARY)
			xt_throw_taberr(XT_CONTEXT, XT_ERR_NO_DICTIONARY, name);

		if (opened)
			*opened = TRUE;
	}
	
	if (tab)
		xt_heap_reference(self, tab);

	return tab;
}

static void tab_close_table(XTOpenTablePtr ot)
{
	xt_ind_free_reserved(ot);

	if (ot->ot_rec_file) {
		XT_CLOSE_RR_FILE_NS(ot->ot_rec_file);
		ot->ot_rec_file = NULL;
		
	}
	if (ot->ot_ind_file) {
		xt_close_file_ns(ot->ot_ind_file);
		ot->ot_ind_file = NULL;
		
	}
	if (ot->ot_row_file) {
		XT_CLOSE_RR_FILE_NS(ot->ot_row_file);
		ot->ot_row_file = NULL;
		
	}
	if (ot->ot_table) {
		xt_heap_release(xt_get_self(), ot->ot_table);
		ot->ot_table = NULL;
	}
	if (ot->ot_ind_rhandle) {
		xt_ind_release_handle(ot->ot_ind_rhandle, FALSE, ot->ot_thread);
		ot->ot_ind_rhandle = NULL;
	}
	if (ot->ot_row_rbuffer) {
		xt_free_ns(ot->ot_row_rbuffer);
		ot->ot_row_rbuf_size = 0;
		ot->ot_row_rbuffer = NULL;
	}
	if (ot->ot_row_wbuffer) {
		xt_free_ns(ot->ot_row_wbuffer);
		ot->ot_row_wbuf_size = 0;
		ot->ot_row_wbuffer = NULL;
	}
#ifdef XT_TRACK_RETURNED_ROWS
	if (ot->ot_rows_returned) {
		xt_free_ns(ot->ot_rows_returned);
		ot->ot_rows_returned = NULL;
	}
	ot->ot_rows_ret_curr = 0;
	ot->ot_rows_ret_max = 0;
#endif
	xt_free(NULL, ot);
}

/*
 * This function locks a particular table by locking the table directory
 * and waiting for all open tables handles to close.
 *
 * Things are a bit complicated because the sweeper must be turned off before
 * the table directory is locked.
 */
static XTOpenTablePoolPtr tab_lock_table(XTThreadPtr self, XTPathStrPtr name, xtBool no_load, xtBool flush_table, xtBool missing_ok, XTTableHPtr *tab)
{
	XTOpenTablePoolPtr	table_pool;
	XTDatabaseHPtr		db = self->st_database;

	enter_();
	/* Lock the table, and close all references: */
	pushsr_(table_pool, xt_db_unlock_table_pool, xt_db_lock_table_pool_by_name(self, db, name, no_load, flush_table, missing_ok, FALSE, tab));
	if (!table_pool) {
		freer_(); // xt_db_unlock_table_pool(db)
		return_(NULL);
	}

	/* Wait for all open tables to close: */
	xt_db_wait_for_open_tables(self, table_pool);

	popr_(); // Discard xt_db_unlock_table_pool(table_pool)
	return_(table_pool);
}

static void tab_delete_table_files(XTThreadPtr self, XTPathStrPtr tab_name, xtTableID tab_id)
{
	XTFilesOfTableRec	ft;

	xt_enum_files_of_tables_init(tab_name, tab_id, &ft);
	while (xt_enum_files_of_tables_next(&ft)) {
		if (!xt_fs_delete(NULL, ft.ft_file_path))
			xt_log_and_clear_exception(self);
	}
}

xtPublic void xt_create_table(XTThreadPtr self, XTPathStrPtr name, XTDictionaryPtr dic)
{
	char				table_name[XT_MAX_TABLE_FILE_NAME_SIZE];
	char				path[PATH_MAX];
	XTDatabaseHPtr		db = self->st_database;
	XTOpenTablePoolPtr	table_pool;
	XTTableHPtr			tab;
	XTTableHPtr			old_tab = NULL;
	xtTableID			old_tab_id = 0;
	xtTableID			tab_id = 0;
	XTTabRowHeadDRec	row_head;
	XTTableHeadDRec		rec_head;
	XTTableFormatDRec	table_fmt;
	XTIndexFormatDPtr	index_fmt;
	XTStringBufferRec	tab_def = { 0, 0, 0 };
	XTTableEntryRec		te_tab;
	XTSortedListInfoRec	li_undo;

#ifdef TRACE_CREATE_TABLES
	printf("CREATE %s\n", name->ps_path);
#endif
	enter_();
	if (strlen(xt_last_name_of_path(name->ps_path)) > XT_TABLE_NAME_SIZE-1)
		xt_throw_taberr(XT_CONTEXT, XT_ERR_NAME_TOO_LONG, name);
	if (!db)
		xt_throw_xterr(XT_CONTEXT, XT_ERR_NO_DATABASE_IN_USE);

	/* Lock to prevent table list change during creation. */
	table_pool = tab_lock_table(self, name, FALSE, TRUE, TRUE, &old_tab);
	pushr_(xt_db_unlock_table_pool, table_pool);
	xt_ht_lock(self, db->db_tables);
	pushr_(xt_ht_unlock, db->db_tables);
	pushr_(xt_heap_release, old_tab);

	/* This must be done before we remove the old table
	 * from the directory, or we will not be able
	 * to find the table, which could is require
	 * for TRUNCATE!
	 */
	if (xt_sl_get_size(db->db_table_by_id) >= XT_MAX_TABLES)
		xt_throw_ulxterr(XT_CONTEXT, XT_ERR_TOO_MANY_TABLES, (u_long) XT_MAX_TABLES);

	tab_id = db->db_curr_tab_id + 1;		

	if (old_tab) {
		old_tab_id = old_tab->tab_id;		
		xt_dl_delete_ext_data(self, old_tab, FALSE, TRUE);
		freer_(); // xt_heap_release(self, old_tab)

		/* For the Windows version this must be done before we
		 * start to delete the underlying files!
		 */
		tab_close_mapped_files(self, old_tab);

		tab_delete_table_files(self, name, old_tab_id);

		/* Remove the PBMS table: */
		ASSERT(xt_get_self() == self);

		/* Remove the table from the directory. It will get a new
		 * ID so the handle in the directory will no longer be valid.
		 */
		xt_ht_del(self, db->db_tables, name);
	}
	else {
		freer_(); // xt_heap_release(self, old_tab)
	}

	/* Add the table to the directory, well remove on error! */
	li_undo.li_sl = db->db_table_by_id;
	li_undo.li_key = &tab_id;
	te_tab.te_tab_id = tab_id;
	te_tab.te_tab_name = xt_dup_string(self, xt_last_name_of_path(name->ps_path));
	te_tab.te_tab_path = tab_get_table_path(self, db, name, TRUE);
	te_tab.te_table = NULL;
	xt_sl_insert(self, db->db_table_by_id, &tab_id, &te_tab);
	pushr_(xt_sl_delete_from_info, &li_undo);

	*path = 0;
	try_(a) {
		XTOpenFilePtr	of_row, of_rec, of_ind;
		off_t			eof;
		size_t			def_len = 0;

		tab = (XTTableHPtr) xt_heap_new(self, sizeof(XTTableHRec), tab_finalize);
		pushr_(xt_heap_release, tab);

		/* The length of the foreign key definition: */
		if (dic->dic_table) {
			dic->dic_table->loadString(self, &tab_def);
			def_len = tab_def.sb_len + 1;
		}

		tab->tab_head_op_seq = 0;
#ifdef DEBUG
		//tab->tab_head_op_seq = 0xFFFFFFFF - 12;
#endif

		/* ------- ROW FILE: */
		xt_strcpy(PATH_MAX, path, name->ps_path);
		xt_remove_last_name_of_path(path);
		tab_get_row_file_name(table_name, xt_last_name_of_path(name->ps_path), tab_id);
		xt_strcat(PATH_MAX, path, table_name);

		of_row = xt_open_file(self, path, XT_FS_CREATE | XT_FS_EXCLUSIVE);
		pushr_(xt_close_file, of_row);
		XT_SET_DISK_4(row_head.rh_magic_4, XT_TAB_ROW_MAGIC);
		if (!xt_pwrite_file(of_row, 0, sizeof(row_head), &row_head, &self->st_statistics.st_rec, self))
			xt_throw(self);
		freer_(); // xt_close_file(of_row)

		(void) ASSERT(sizeof(XTTabRowHeadDRec) == sizeof(XTTabRowRefDRec));
		(void) ASSERT(sizeof(XTTabRowRefDRec) == 1 << XT_TAB_ROW_SHIFTS);

		tab->tab_row_eof_id = 1;
		tab->tab_row_free_id = 0;
		tab->tab_row_fnum = 0;

		tab->tab_head_row_eof_id = 1;
		tab->tab_head_row_free_id = 0;
		tab->tab_head_row_fnum  = 0;

		/* ------------ DATA FILE: */
		xt_remove_last_name_of_path(path);
		tab_get_data_file_name(table_name, xt_last_name_of_path(name->ps_path), tab_id);
		xt_strcat(PATH_MAX, path, table_name);
		of_rec = xt_open_file(self, path, XT_FS_CREATE | XT_FS_EXCLUSIVE);
		pushr_(xt_close_file, of_rec);

		/* Calculate the offset of the first record in the data handle file. */
		eof = sizeof(XTTableHeadDRec) + offsetof(XTTableFormatDRec, tf_definition) + def_len + XT_FORMAT_DEF_SPACE;
		eof = (eof + 1024 - 1) / 1024 * 1024;		// Round to a value divisible by 1024

		tab->tab_table_format_offset = sizeof(XTTableHeadDRec);
		tab->tab_table_head_size = (size_t) eof;

		tab->tab_rec_eof_id = 1;						// This is the first record ID!
		tab->tab_rec_free_id = 0;
		tab->tab_rec_fnum = 0;
		
		tab->tab_head_rec_eof_id = 1;					// The first record ID
		tab->tab_head_rec_free_id = 0;
		tab->tab_head_rec_fnum = 0;

		tab->tab_dic.dic_rec_size = dic->dic_rec_size;
		tab->tab_dic.dic_rec_fixed = dic->dic_rec_fixed;
		tab->tab_dic.dic_tab_flags = dic->dic_tab_flags;
		tab->tab_dic.dic_min_auto_inc = dic->dic_min_auto_inc;
		tab->tab_dic.dic_def_ave_row_size = dic->dic_def_ave_row_size;

		XT_SET_DISK_4(rec_head.th_head_size_4, sizeof(XTTableHeadDRec));
		XT_SET_DISK_4(rec_head.th_op_seq_4, tab->tab_head_op_seq);
		XT_SET_DISK_6(rec_head.th_row_free_6, tab->tab_head_row_free_id);
		XT_SET_DISK_6(rec_head.th_row_eof_6, tab->tab_head_row_eof_id);
		XT_SET_DISK_6(rec_head.th_row_fnum_6, tab->tab_head_row_fnum);
		XT_SET_DISK_6(rec_head.th_rec_free_6, tab->tab_head_rec_free_id);
		XT_SET_DISK_6(rec_head.th_rec_eof_6, tab->tab_head_rec_eof_id);
		XT_SET_DISK_6(rec_head.th_rec_fnum_6, tab->tab_head_rec_fnum);

		if (!xt_pwrite_file(of_rec, 0, sizeof(XTTableHeadDRec), &rec_head, &self->st_statistics.st_rec, self))
			xt_throw(self);

		/* Store the table format: */
		memset(&table_fmt, 0, offsetof(XTTableFormatDRec, tf_definition));
		XT_SET_DISK_4(table_fmt.tf_format_size_4, offsetof(XTTableFormatDRec, tf_definition) + def_len);
		XT_SET_DISK_4(table_fmt.tf_tab_head_size_4, eof);
		XT_SET_DISK_2(table_fmt.tf_tab_version_2, XT_TAB_CURRENT_VERSION);
		XT_SET_DISK_4(table_fmt.tf_rec_size_4, tab->tab_dic.dic_rec_size);
		XT_SET_DISK_1(table_fmt.tf_rec_fixed_1, tab->tab_dic.dic_rec_fixed);
		XT_SET_DISK_2(table_fmt.tf_tab_flags_2, tab->tab_dic.dic_tab_flags);
		XT_SET_DISK_8(table_fmt.tf_min_auto_inc_8, tab->tab_dic.dic_min_auto_inc);

		if (!xt_pwrite_file(of_rec, sizeof(XTTableHeadDRec), offsetof(XTTableFormatDRec, tf_definition), &table_fmt, &self->st_statistics.st_rec, self))
			xt_throw(self);
		if (def_len) {
			if (!xt_pwrite_file(of_rec, sizeof(XTTableHeadDRec) + offsetof(XTTableFormatDRec, tf_definition), def_len, tab_def.sb_cstring, &self->st_statistics.st_rec, self))
				xt_throw(self);
		}

		freer_(); // xt_close_file(of_rec)

		/* ----------- INDEX FILE: */
		xt_remove_last_name_of_path(path);
		tab_get_index_file_name(table_name, xt_last_name_of_path(name->ps_path), tab_id);
		xt_strcat(PATH_MAX, path, table_name);
		of_ind = xt_open_file(self, path, XT_FS_CREATE | XT_FS_EXCLUSIVE);
		pushr_(xt_close_file, of_ind);

		/* This is the size of the index header: */
		tab->tab_index_format_offset = offsetof(XTIndexHeadDRec, tp_data) + dic->dic_key_count * XT_NODE_REF_SIZE;
		if (!(tab->tab_index_head = (XTIndexHeadDPtr) xt_calloc_ns(XT_INDEX_HEAD_SIZE)))
			xt_throw(self);

		XT_NODE_ID(tab->tab_ind_eof) = 1;
		XT_NODE_ID(tab->tab_ind_free) = 0;

		XT_SET_DISK_4(tab->tab_index_head->tp_header_size_4, XT_INDEX_HEAD_SIZE);
		XT_SET_DISK_4(tab->tab_index_head->tp_format_offset_4, tab->tab_index_format_offset);
		XT_SET_DISK_6(tab->tab_index_head->tp_ind_eof_6, XT_NODE_ID(tab->tab_ind_eof));
		XT_SET_DISK_6(tab->tab_index_head->tp_ind_free_6, XT_NODE_ID(tab->tab_ind_free));

		/* Store the index format: */
		index_fmt = (XTIndexFormatDPtr) (((xtWord1 *) tab->tab_index_head) + tab->tab_index_format_offset);
		XT_SET_DISK_4(index_fmt->if_format_size_4, sizeof(XTIndexFormatDRec));
		XT_SET_DISK_2(index_fmt->if_tab_version_2, XT_TAB_CURRENT_VERSION);
		XT_SET_DISK_2(index_fmt->if_ind_version_2, XT_IND_CURRENT_VERSION);
		XT_SET_DISK_1(index_fmt->if_node_ref_size_1, XT_NODE_REF_SIZE);
		XT_SET_DISK_1(index_fmt->if_rec_ref_size_1, XT_RECORD_REF_SIZE);
		XT_SET_DISK_4(index_fmt->if_page_size_4, XT_INDEX_PAGE_SIZE);

		/* Save the header: */
		if (!xt_pwrite_file(of_ind, 0, XT_INDEX_HEAD_SIZE, tab->tab_index_head, &self->st_statistics.st_ind, self))
			xt_throw(self);

		freer_(); // xt_close_file(of_ind)

		/* ------------ */
		/* Log the new table ID! */
		db->db_curr_tab_id = tab_id;
		if (!xt_xn_log_tab_id(self, tab_id)) {
			db->db_curr_tab_id = tab_id - 1;
			xt_throw(self);
		}

		freer_(); // xt_heap_release(tab)

		/* {LOAD-FOR-FKS}
		 * 2008-12-10: Note, there is another problem, example:
		 * set storage_engine = pbxt;
		 * 
		 * CREATE TABLE t1 (s1 INT PRIMARY KEY, s2 INT);
		 * CREATE TABLE t2 (s1 INT PRIMARY KEY, FOREIGN KEY (s1) REFERENCES t1 (s1) ON UPDATE CASCADE);
		 * CREATE TABLE t3 (s1 INT PRIMARY KEY, FOREIGN KEY (s1) REFERENCES t2 (s1) ON UPDATE CASCADE);
		 * 
		 * DROP TABLE IF EXISTS t2,t1;
		 * CREATE TABLE t1 (s1 ENUM('a','b') PRIMARY KEY);
		 * CREATE TABLE t2 (s1 ENUM('A','B'), FOREIGN KEY (s1) REFERENCES t1 (s1));
		 * 
		 * DROP TABLE IF EXISTS t2,t1;
		 * 
		 * In the example above. The second create t2 does not fail, although t3 references it,
		 * and the data types do not match.
		 * 
		 * The main problem is that this error comes on DROP TABLE IF EXISTS t2! Which prevents
		 * the table from being dropped - not good.
		 *
		 * So my idea here is to open the table, and if it fails, then the create table fails
		 * as well.
		 */
		if (!old_tab_id) {
			tab = xt_use_table_no_lock(self, db, name, FALSE, FALSE, NULL, NULL);
			xt_heap_release(self, tab);
		}
	}
	catch_(a) {
		/* Creation failed, delete the table files: */
		if (*path)
			tab_delete_table_files(self, name, tab_id);
		tab_remove_table_path(self, db, te_tab.te_tab_path);
		xt_sb_set_size(self, &tab_def, 0);
		throw_();
	}
	cont_(a);

	xt_sb_set_size(self, &tab_def, 0);

	if (old_tab_id) {
		try_(b) {
			XTTableEntryPtr	te_ptr;

			if ((te_ptr = (XTTableEntryPtr) xt_sl_find(self, db->db_table_by_id, &old_tab_id))) {
				tab_remove_table_path(self, db, te_ptr->te_tab_path);
				xt_sl_delete(self, db->db_table_by_id, &old_tab_id);
			}

			/* Same purpose as above {LOAD-FOR-FKS} (although this should work, 
			 * beacuse this is a TRUNCATE TABLE.
			 */
			tab = xt_use_table_no_lock(self, db, name, FALSE, FALSE, NULL, NULL);
			xt_heap_release(self, tab);
		}
		catch_(b) {
			/* Log this error, but do not return it, because
			 * it just involves the cleanup of the old table,
			 * the new table has been successfully created.
			 */
			xt_log_and_clear_exception(self);
		}
		cont_(b);
	}

	popr_(); // Discard xt_sl_delete_from_info(&li_undo)

	freer_(); // xt_ht_unlock(db->db_tables)
	freer_(); // xt_db_unlock_table_pool(table_pool)

	/* I open the table here, because I cannot rely on MySQL to do
	 * it after a create. This is normally OK, but with foreign keys
	 * tables can be referenced and then they are not opened
	 * before use. In this example, the INSERT opens t2, but t1 is
	 * not opened of the create. As a result the foreign key
	 * reference is not resolved.
	 *
	 * drop table t1, t2;
	 * CREATE TABLE t1
	 * (
	 *  id INT PRIMARY KEY
	 * ) ENGINE=pbxt;
	 * 
	 * CREATE TABLE t2
	 * (
	 *  v INT,
	 *  CONSTRAINT c1 FOREIGN KEY (v) REFERENCES t1(id)
	 * ) ENGINE=pbxt;
	 * 
	 * --error 1452
	 * INSERT INTO t2 VALUES(2);
	 */
	/* this code is not needed anymore as we open tables referred by FKs as necessary during checks
	xt_ht_lock(self, db->db_tables);
	pushr_(xt_ht_unlock, db->db_tables);
	tab = xt_use_table_no_lock(self, db, name, FALSE, FALSE, NULL, NULL);
	freer_(); // xt_ht_unlock(db->db_tables)
	xt_heap_release(self, tab);
	* CHANGED see {LOAD-FOR-FKS} above.
	*/

	exit_();
}

xtPublic void xt_drop_table(XTThreadPtr self, XTPathStrPtr tab_name, xtBool drop_db)
{
	XTDatabaseHPtr		db = self->st_database;
	XTOpenTablePoolPtr	table_pool;
	XTTableHPtr			tab = NULL;
	xtTableID			tab_id = 0;
	xtBool				can_drop = TRUE;

	enter_();

#ifdef TRACE_CREATE_TABLES
	printf("DROP %s\n", tab_name->ps_path);
#endif

	table_pool = tab_lock_table(self, tab_name, FALSE, TRUE, TRUE, &tab);
	pushr_(xt_db_unlock_table_pool, table_pool);
	xt_ht_lock(self, db->db_tables);
	pushr_(xt_ht_unlock, db->db_tables);
	pushr_(xt_heap_release, tab);

	if (table_pool) {
		tab_id = tab->tab_id;	/* tab is not null if returned table_pool is not null */
		/* check if other tables refer this */
		if (!self->st_ignore_fkeys) 
			can_drop = tab->tab_dic.dic_table->checkCanDrop(drop_db);
	}
#ifdef DRIZZLED 
	/* See the comment in ha_pbxt::delete_table regarding different implmentation of DROP TABLE
         * in MySQL and Drizzle
         */
	else {
		xt_throw_xterr(XT_CONTEXT, XT_ERR_TABLE_NOT_FOUND);
	}
#endif

	if (can_drop) {
		if (tab_id) {
			XTTableEntryPtr	te_ptr;

			xt_dl_delete_ext_data(self, tab, FALSE, TRUE);
			freer_(); // xt_heap_release(self, tab)

			/* For the Windows version this must be done before we
			 * start to delete the underlying files!
			 */
			tab_close_mapped_files(self, tab);

			tab_delete_table_files(self, tab_name, tab_id);

			ASSERT(xt_get_self() == self);
			if ((te_ptr = (XTTableEntryPtr) xt_sl_find(self, db->db_table_by_id, &tab_id))) {
				tab_remove_table_path(self, db, te_ptr->te_tab_path);
				xt_sl_delete(self, db->db_table_by_id, &tab_id);
			}
		}
		else {
			freer_(); // xt_heap_release(self, tab)
		}

		xt_ht_del(self, db->db_tables, tab_name);
	}
	else {	/* cannot drop table because of FK dependencies */
		xt_throw_xterr(XT_CONTEXT, XT_ERR_ROW_IS_REFERENCED);
	}

	freer_(); // xt_ht_unlock(db->db_tables)
	freer_(); // xt_db_unlock_table_pool(table_pool)
	exit_();
}

/*
 * Record buffer size:
 * -------------------
 * The size of the record buffer used to hold the row
 * in memory. This buffer size does not include the BLOB data.
 * About 8 bytes (a pointer and a size) is reserved for each BLOB
 * in this buffer.
 *
 * The buffer size includes a number of "NULL" bytes followed by
 * the data area. The NULL bytes contain 1 bit for every column,
 * to indicate of the columns is NULL or not.
 *
 * The size of the buffer is 4/8-byte aligned, so it may be padded
 * at the end.
 *
 * Fixed length rec. len.:
 * -----------------------
 * If the record does not include any BLOBs then this is the size of the
 * fixed length record. The size if the data in the data handle record
 * need never be bigger then this length, if the record does not
 * contain BLOBs. So this should be the maximum size set for
 * AVG_ROW_LENGTH in this case.
 *
 * Handle data record size:
 * ------------------------
 * This is the size of the handle data record. It is the data size
 * plus the "max header size".
 *
 * Min/max header size:
 * The min and max header size of the header in the data handle file.
 * The larger header is used if a record has an extended data (data log
 * file) component.
 *
 * Min/avg/max record size:
 * ------------------------
 * These are variable length records sizes. That is, the size of records
 * when stored in the variable length format. Variable length records
 * do not have fixed fields sizes, instead the fields are packed one
 * after the other, prefixed by a number of size indicator bytes.
 *
 * The average is an estimate of the average record size. This estimate
 * is used if no AVG_ROW_LENGTH is specifically given.
 *
 * If the average estimate is withing 20% of the maximum size of the record,
 * then the record will be handled as a fixed length record.
 *
 * Avg row len set for tab:
 * ------------------------
 * This is the value set using AVG_ROW_LENGTH when the table is declared.
 *
 * Rows fixed length:
 * ------------------
 * YES if the records of this table are handled as a fixed length records.
 * In this case the table records will never have an extended record
 * component.
 *
 * The size of the data area in the handle data record is set to the
 * size of the MySQL data record ("Fixed length rec. len.").
 *
 * It also means that the record format used is identical to the MySQL
 * record format.
 *
 * If the records are not fixed, then the variable length record format
 * is used. Records size are then in the range specified by
 * "Min/avg/max record size".
 *
 * Maximum fixed size:
 * -------------------
 * This is the maximum size of a data log record.
 *
 * Minimum variable size:
 * ------------------------
 * Records below this size are handled as a fixed length record size, unless
 * the AVG_ROW_LENGTH is specifically set.
 */
xtPublic void xt_check_table(XTThreadPtr self, XTOpenTablePtr ot)
{
	XTTableHPtr				tab = ot->ot_table;
	xtRecordID				prec_id;
	XTTabRecExtDPtr			rec_buf = (XTTabRecExtDPtr) ot->ot_row_rbuffer;
	XTactExtRecEntryDRec	ext_rec;
	size_t					log_size;
	xtLogID					log_id;
	xtLogOffset				log_offset;
	xtRecordID				rec_id;
	xtRecordID				prev_rec_id;
	xtXactID				xn_id;
	xtRowID					row_id;
	u_llong					free_rec_count = 0, free_count2 = 0;
	u_llong					delete_rec_count = 0;
	u_llong					alloc_rec_count = 0;
	u_llong					alloc_rec_bytes = 0;
	u_llong					min_comp_rec_len = 0;
	u_llong					max_comp_rec_len = 0;
	size_t					rec_size;
	size_t					row_size;
	u_llong					ext_data_len = 0;

#if defined(DUMP_CHECK_TABLE) || defined(CHECK_TABLE_STATS)
	printf("\nCHECK TABLE: %s\n", tab->tab_name->ps_path);
#endif

	xt_lock_mutex(self, &tab->tab_db->db_co_ext_lock);
	pushr_(xt_unlock_mutex, &tab->tab_db->db_co_ext_lock);

	xt_lock_mutex(self, &tab->tab_rec_lock);
	pushr_(xt_unlock_mutex, &tab->tab_rec_lock);

#ifdef CHECK_TABLE_STATS
	printf("Record buffer size      = %lu\n", (u_long) tab->tab_dic.dic_mysql_buf_size);
	printf("Fixed length rec. len.  = %lu\n", (u_long) tab->tab_dic.dic_mysql_rec_size);
	printf("Handle data record size = %lu\n", (u_long) tab->tab_dic.dic_rec_size);
	printf("Min/max header size     = %d/%d\n", (int) offsetof(XTTabRecFix, rf_data), tab->tab_dic.dic_rec_fixed ? (int) offsetof(XTTabRecFix, rf_data) : (int) offsetof(XTTabRecExtDRec, re_data));
	printf("Min/avg/max record size = %llu/%llu/%llu\n", (u_llong) tab->tab_dic.dic_min_row_size, (u_llong) tab->tab_dic.dic_ave_row_size, (u_llong) tab->tab_dic.dic_max_row_size);
	if (tab->tab_dic.dic_def_ave_row_size)
		printf("Avg row len set for tab = %lu\n", (u_long) tab->tab_dic.dic_def_ave_row_size);
	else
		printf("Avg row len set for tab = not specified\n");
	printf("Rows fixed length       = %s\n", tab->tab_dic.dic_rec_fixed ? "YES" : "NO");
	if (tab->tab_dic.dic_tab_flags & XT_TAB_FLAGS_TEMP_TAB)
		printf("Table type              = TEMP\n");
	if (tab->tab_dic.dic_def_ave_row_size)
		printf("Maximum fixed size      = %lu\n", (u_long) XT_TAB_MAX_FIX_REC_LENGTH_SPEC);
	else
		printf("Maximum fixed size      = %lu\n", (u_long) XT_TAB_MAX_FIX_REC_LENGTH);
	printf("Minimum variable size   = %lu\n", (u_long) XT_TAB_MIN_VAR_REC_LENGTH);
	printf("Minimum auto-increment  = %llu\n", (u_llong) tab->tab_dic.dic_min_auto_inc);
	printf("Number of columns       = %lu\n", (u_long) tab->tab_dic.dic_no_of_cols);
	printf("Number of fixed columns = %lu\n", (u_long) tab->tab_dic.dic_fix_col_count);
	printf("Columns req. for index  = %lu\n", (u_long) tab->tab_dic.dic_ind_cols_req);
	if (tab->tab_dic.dic_ind_rec_len)
		printf("Rec len req. for index  = %llu\n", (u_llong) tab->tab_dic.dic_ind_rec_len);
	printf("Columns req. for blobs  = %lu\n", (u_long) tab->tab_dic.dic_blob_cols_req);
	printf("Number of blob columns  = %lu\n", (u_long) tab->tab_dic.dic_blob_count);
	printf("Number of indices       = %lu\n", (u_long) tab->tab_dic.dic_key_count);
#endif

#ifdef DUMP_CHECK_TABLE
	printf("Records:-\n");
	printf("Free list: %llu (%llu)\n", (u_llong) tab->tab_rec_free_id, (u_llong) tab->tab_rec_fnum);
	printf("EOF:       %llu\n", (u_llong) tab->tab_rec_eof_id);
#endif

	rec_size = XT_REC_EXT_HEADER_SIZE;
	if (rec_size > tab->tab_recs.tci_rec_size)
		rec_size = tab->tab_recs.tci_rec_size;
	rec_id = 1;
	while (rec_id < tab->tab_rec_eof_id) {
		if (!xt_tab_get_rec_data(ot, rec_id, tab->tab_dic.dic_rec_size, ot->ot_row_rbuffer))
			xt_throw(self);

#ifdef DUMP_CHECK_TABLE
		printf("%-4llu ", (u_llong) rec_id);
#endif
		switch (rec_buf->tr_rec_type_1 & XT_TAB_STATUS_MASK) {
			case XT_TAB_STATUS_FREED:
#ifdef DUMP_CHECK_TABLE
				printf("======== ");
#endif
				free_rec_count++;
				break;
			case XT_TAB_STATUS_DELETE:
#ifdef DUMP_CHECK_TABLE
				printf("delete   ");
#endif
				delete_rec_count++;
				break;
			case XT_TAB_STATUS_FIXED:
#ifdef DUMP_CHECK_TABLE
				printf("record-F ");
#endif
				alloc_rec_count++;
				row_size = myxt_store_row_length(ot, (char *) ot->ot_row_rbuffer + XT_REC_FIX_HEADER_SIZE);
				alloc_rec_bytes += row_size;
				if (!min_comp_rec_len || row_size < min_comp_rec_len)
					min_comp_rec_len = row_size;
				if (row_size > max_comp_rec_len)
					max_comp_rec_len = row_size;
				break;
			case XT_TAB_STATUS_VARIABLE:
#ifdef DUMP_CHECK_TABLE
				printf("record-V ");
#endif
				alloc_rec_count++;
				row_size = myxt_load_row_length(ot, tab->tab_dic.dic_rec_size, ot->ot_row_rbuffer + XT_REC_FIX_HEADER_SIZE, NULL);
				alloc_rec_bytes += row_size;
				if (!min_comp_rec_len || row_size < min_comp_rec_len)
					min_comp_rec_len = row_size;
				if (row_size > max_comp_rec_len)
					max_comp_rec_len = row_size;
				break;
			case XT_TAB_STATUS_EXT_DLOG:
#ifdef DUMP_CHECK_TABLE
				printf("record-X ");
#endif
				alloc_rec_count++;
				ext_data_len += XT_GET_DISK_4(rec_buf->re_log_dat_siz_4);
				row_size = XT_GET_DISK_4(rec_buf->re_log_dat_siz_4) + ot->ot_rec_size - XT_REC_EXT_HEADER_SIZE;
				alloc_rec_bytes += row_size;
				if (!min_comp_rec_len || row_size < min_comp_rec_len)
					min_comp_rec_len = row_size;
				if (row_size > max_comp_rec_len)
					max_comp_rec_len = row_size;
				break;
		}
#ifdef DUMP_CHECK_TABLE
		if (rec_buf->tr_rec_type_1 & XT_TAB_STATUS_CLEANED_BIT)
			printf("C");
		else
			printf(" ");
#endif
		prev_rec_id = XT_GET_DISK_4(rec_buf->tr_prev_rec_id_4);
		xn_id = XT_GET_DISK_4(rec_buf->tr_xact_id_4);
		row_id = XT_GET_DISK_4(rec_buf->tr_row_id_4);
		switch (rec_buf->tr_rec_type_1 & XT_TAB_STATUS_MASK) {
			case XT_TAB_STATUS_FREED:
#ifdef DUMP_CHECK_TABLE
				printf(" prev=%-3llu (xact=%-3llu row=%lu)\n", (u_llong) prev_rec_id, (u_llong) xn_id, (u_long) row_id);
#endif
				break;
			case XT_TAB_STATUS_EXT_DLOG:
#ifdef DUMP_CHECK_TABLE
				printf(" prev=%-3llu  xact=%-3llu row=%lu  Xlog=%lu Xoff=%llu Xsiz=%lu\n", (u_llong) prev_rec_id, (u_llong) xn_id, (u_long) row_id, (u_long) XT_GET_DISK_2(rec_buf->re_log_id_2), (u_llong) XT_GET_DISK_6(rec_buf->re_log_offs_6), (u_long) XT_GET_DISK_4(rec_buf->re_log_dat_siz_4));
#endif

				log_size = XT_GET_DISK_4(rec_buf->re_log_dat_siz_4);
				XT_GET_LOG_REF(log_id, log_offset, rec_buf);
				if (!self->st_dlog_buf.dlb_read_log(log_id, log_offset, offsetof(XTactExtRecEntryDRec, er_data), (xtWord1 *) &ext_rec, self))
					xt_log_and_clear_exception(self);
				else {
					size_t		log_size2;
					xtTableID	curr_tab_id;
					xtRecordID	curr_rec_id;

					log_size2 = XT_GET_DISK_4(ext_rec.er_data_size_4);
					curr_tab_id = XT_GET_DISK_4(ext_rec.er_tab_id_4);
					curr_rec_id = XT_GET_DISK_4(ext_rec.er_rec_id_4);
					if (log_size2 != log_size || curr_tab_id != tab->tab_id || curr_rec_id != rec_id) {
						xt_logf(XT_INFO, "Table %s: record %llu, extended record %lu:%llu not valid\n", tab->tab_name, (u_llong) rec_id, (u_long) log_id, (u_llong) log_offset);
					}
				}
				break;
			default:
#ifdef DUMP_CHECK_TABLE
				printf(" prev=%-3llu  xact=%-3llu row=%lu\n", (u_llong) prev_rec_id, (u_llong) xn_id, (u_long) row_id);
#endif
				break;
		}
		rec_id++;
	}
	
#ifdef CHECK_TABLE_STATS
	if (!tab->tab_dic.dic_rec_fixed)
		printf("Extendend data length   = %llu\n", ext_data_len);
	
	if (alloc_rec_count) {
		printf("Minumum comp. rec. len. = %llu\n", (u_llong) min_comp_rec_len);
		printf("Average comp. rec. len. = %llu\n", (u_llong) ((double) alloc_rec_bytes / (double) alloc_rec_count + (double) 0.5));
		printf("Maximum comp. rec. len. = %llu\n", (u_llong) max_comp_rec_len);
	}
	printf("Free record count       = %llu\n", (u_llong) free_rec_count);
	printf("Deleted record count    = %llu\n", (u_llong) delete_rec_count);
	printf("Allocated record count  = %llu\n", (u_llong) alloc_rec_count);
#endif
	if (tab->tab_rec_fnum != free_rec_count)
		xt_logf(XT_INFO, "Table %s: incorrect number of free blocks, %llu, should be: %llu\n", tab->tab_name, (u_llong) free_rec_count, (u_llong) tab->tab_rec_fnum);

	/* Checking the free list: */
	prec_id = 0;
	rec_id = tab->tab_rec_free_id;
	while (rec_id) {
		if (rec_id >= tab->tab_rec_eof_id) {
			xt_logf(XT_INFO, "Table %s: invalid reference on free list: %llu, ", tab->tab_name, (u_llong) rec_id);
			if (prec_id)
				xt_logf(XT_INFO, "reference by: %llu\n", (u_llong) prec_id);
			else
				xt_logf(XT_INFO, "reference by list head pointer\n");
			break;
		}
		if (!xt_tab_get_rec_data(ot, rec_id, XT_REC_FIX_HEADER_SIZE, (xtWord1 *) rec_buf)) {
			xt_log_and_clear_exception(self);
			break;
		}
		if ((rec_buf->tr_rec_type_1 & XT_TAB_STATUS_MASK) != XT_TAB_STATUS_FREED)
			xt_logf(XT_INFO, "Table %s: record, %llu, on free list is not free\n", tab->tab_name, (u_llong) rec_id);
		free_count2++;
		prec_id = rec_id;
		rec_id = XT_GET_DISK_4(rec_buf->tr_prev_rec_id_4);
	}
	if (free_count2 < free_rec_count)
		xt_logf(XT_INFO, "Table %s: not all free blocks (%llu) on free list: %llu\n", tab->tab_name, (u_llong) free_rec_count, (u_llong) free_count2);

	freer_(); // xt_unlock_mutex_ns(&tab->tab_rec_lock);

	xtRefID ref_id;

	xt_lock_mutex(self, &tab->tab_row_lock);
	pushr_(xt_unlock_mutex, &tab->tab_row_lock);

#ifdef DUMP_CHECK_TABLE
	printf("Rows:-\n");
	printf("Free list: %llu (%llu)\n", (u_llong) tab->tab_row_free_id, (u_llong) tab->tab_row_fnum);
	printf("EOF:       %llu\n", (u_llong) tab->tab_row_eof_id);
#endif

	rec_id = 1;
	while (rec_id < tab->tab_row_eof_id) {
		if (!tab->tab_rows.xt_tc_read_4(ot->ot_row_file, rec_id, &ref_id, self))
			xt_throw(self);
#ifdef DUMP_CHECK_TABLE
		printf("%-3llu ", (u_llong) rec_id);
#endif
#ifdef DUMP_CHECK_TABLE
		if (ref_id == 0)
			printf("====== 0\n");
		else
			printf("in use %llu\n", (u_llong) ref_id);
#endif
		rec_id++;
	}

	freer_(); // xt_unlock_mutex(&tab->tab_row_lock);

#ifdef CHECK_INDEX_ON_CHECK_TABLE
	xt_check_indices(ot);
#endif
	freer_(); // xt_unlock_mutex(&tab->tab_db->db_co_ext_lock);
}

xtPublic void xt_rename_table(XTThreadPtr self, XTPathStrPtr old_name, XTPathStrPtr new_name)
{
	XTDatabaseHPtr		db = self->st_database;
	XTOpenTablePoolPtr	table_pool;
	XTTableHPtr			tab = NULL;
	char				table_name[XT_MAX_TABLE_FILE_NAME_SIZE];
	char				*postfix;
	XTFilesOfTableRec	ft;
	XTDictionaryRec		dic;
	xtTableID			tab_id;
	XTTableEntryPtr		te_ptr;
	char				*te_new_name;
	XTTablePathPtr		te_new_path;
	XTTablePathPtr		te_old_path;
	char				to_path[PATH_MAX];

	memset(&dic, 0, sizeof(dic));

#ifdef TRACE_CREATE_TABLES
	printf("RENAME %s --> %s\n", old_name->ps_path, new_name->ps_path);
#endif
	if (strlen(xt_last_name_of_path(new_name->ps_path)) > XT_TABLE_NAME_SIZE-1)
		xt_throw_taberr(XT_CONTEXT, XT_ERR_NAME_TOO_LONG, new_name);

	/* MySQL renames the table while it is in use. Here is
	 * the sequence:
	 *
	 * OPEN tab1
	 * CREATE tmp_tab
	 * OPEN tmp_tab
	 * COPY tab1 -> tmp_tab
	 * CLOSE tmp_tab
	 * RENAME tab1 -> tmp2_tab
	 * RENAME tmp_tab -> tab1
	 * CLOSE tab1 (tmp2_tab)
	 * DELETE tmp2_tab
	 * OPEN tab1
	 *
	 * Since the table is open when it is renamed, I cannot
	 * get exclusive use of the table for this operation.
	 *
	 * So instead we just make sure that the sweeper is not
	 * using the table.
	 */
	table_pool = tab_lock_table(self, old_name, FALSE, TRUE, FALSE, &tab);
	pushr_(xt_db_unlock_table_pool, table_pool);
	xt_ht_lock(self, db->db_tables);
	pushr_(xt_ht_unlock, db->db_tables);
	tab_id = tab->tab_id;
	myxt_move_dictionary(&dic, &tab->tab_dic);
	pushr_(myxt_free_dictionary, &dic);
	pushr_(xt_heap_release, tab);

	/* Unmap the memory mapped table files: 
	 * For windows this must be done before we
	 * can rename the files.
	 */
	tab_close_mapped_files(self, tab);

	freer_(); // xt_heap_release(self, old_tab)

	/* Create the new name and path: */
	te_new_name = xt_dup_string(self, xt_last_name_of_path(new_name->ps_path));
	pushr_(xt_free, te_new_name);
	te_new_path = tab_get_table_path(self, db, new_name, FALSE);
	pushr_(tab_free_table_path, te_new_path);

	te_ptr = (XTTableEntryPtr) xt_sl_find(self, db->db_table_by_id, &tab_id);

	/* Remove the table from the Database directory: */
	xt_ht_del(self, db->db_tables, old_name);

	xt_enum_files_of_tables_init(old_name, tab_id, &ft);
	while (xt_enum_files_of_tables_next(&ft)) {
		postfix = xt_tab_file_to_name(XT_MAX_TABLE_FILE_NAME_SIZE, table_name, ft.ft_file_path);

		xt_strcpy(PATH_MAX, to_path, new_name->ps_path);
		xt_strcat(PATH_MAX, to_path, postfix);

		if (!xt_fs_rename(NULL, ft.ft_file_path, to_path))
			xt_log_and_clear_exception(self);
	}

	/* Switch the table name and path: */
	xt_free(self, te_ptr->te_tab_name);
	te_ptr->te_tab_name = te_new_name;
	te_old_path = te_ptr->te_tab_path;
	te_ptr->te_tab_path = te_new_path;
	tab_remove_table_path(self, db, te_old_path);

	popr_(); // Discard tab_free_table_path(te_new_path);
	popr_(); // Discard xt_free(te_new_name);

	tab = xt_use_table_no_lock(self, db, new_name, FALSE, FALSE, &dic, NULL);
	/* All renamed tables are considered repaired! */
	xt_tab_table_repaired(tab);
	xt_heap_release(self, tab);

	freer_(); // myxt_free_dictionary(&dic)
	freer_(); // xt_ht_unlock(db->db_tables)
	freer_(); // xt_db_unlock_table_pool(table_pool)
}

xtPublic XTTableHPtr xt_use_table(XTThreadPtr self, XTPathStrPtr name, xtBool no_load, xtBool missing_ok, xtBool *opened)
{
	XTTableHPtr		tab;
	XTDatabaseHPtr	db = self->st_database;

	xt_ht_lock(self, db->db_tables);
	pushr_(xt_ht_unlock, db->db_tables);
	tab = xt_use_table_no_lock(self, db, name, no_load, missing_ok, NULL, opened);
	freer_();
	return tab;
}

xtPublic void xt_sync_flush_table(XTThreadPtr self, XTOpenTablePtr ot)
{
	XTTableHPtr		tab = ot->ot_table;
	XTDatabaseHPtr	db = tab->tab_db;

	/* Wakeup the sweeper:
	 * We want the sweeper to check if there is anything to do,
	 * so we must wake it up.
	 * Once it has done all it can, it will go back to sleep.
	 * This should be good enough.
	 *
	 * NOTE: I all cases, we do not wait if the sweeper is in
	 * error state.
	 */
	if (db->db_sw_idle) {
		u_int check_count = db->db_sw_check_count;

		for (;;) {
			xt_wakeup_sweeper(db);
			if (!db->db_sw_thread || db->db_sw_idle != XT_THREAD_IDLE || check_count != db->db_sw_check_count)
				break;
			xt_sleep_milli_second(10);
		}
	}

	/* Wait for the sweeper to become idle: */
	xt_lock_mutex(self, &db->db_sw_lock);
	pushr_(xt_unlock_mutex, &db->db_sw_lock);
	while (db->db_sw_thread && !db->db_sw_idle) {
		xt_timed_wait_cond(self, &db->db_sw_cond, &db->db_sw_lock, 10);
	}
	freer_(); // xt_unlock_mutex(&db->db_sw_lock)

	/* Wait for the writer to write out all operations on the table:
	 * We also do not wait for the writer if it is in
	 * error state.
	 */
	while (db->db_wr_thread && 
		db->db_wr_idle != XT_THREAD_INERR &&
		XTTableSeq::xt_op_is_before(tab->tab_head_op_seq+1, tab->tab_seq.ts_next_seq)) {
		/* Flush the log, in case this is holding up the
		 * writer!
		 */
		if (!db->db_xlog.xlog_flush(self))
			xt_throw(self);

		xt_lock_mutex(self, &db->db_wr_lock);
		pushr_(xt_unlock_mutex, &db->db_wr_lock);
		db->db_wr_thread_waiting++;
		/*
		 * Wake the writer if it is sleeping. In order to
		 * flush a table we must wait for the writer to complete
		 * committing all the changes in the table to the database.
		 */
		if (db->db_wr_idle) {
			if (!xt_broadcast_cond_ns(&db->db_wr_cond))
				xt_log_and_clear_exception_ns();
		}

		freer_(); // xt_unlock_mutex(&db->db_wr_lock)
		xt_sleep_milli_second(10);

		xt_lock_mutex(self, &db->db_wr_lock);
		pushr_(xt_unlock_mutex, &db->db_wr_lock);
		db->db_wr_thread_waiting--;
		freer_(); // xt_unlock_mutex(&db->db_wr_lock)
	}

	xt_flush_table(self, ot);
}

xtPublic xtBool xt_flush_record_row(XTOpenTablePtr ot, off_t *bytes_flushed, xtBool have_table_lock)
{
	XTTableHeadDRec			rec_head;
	XTTableHPtr				tab = ot->ot_table;
	off_t					to_flush;
	XTCheckPointTablePtr	cp_tab;
	XTCheckPointStatePtr	cp = NULL;

	if (!xt_begin_checkpoint(tab->tab_db, have_table_lock, ot->ot_thread))
		return FAILED;

	xt_lock_mutex_ns(&tab->tab_rec_flush_lock);

	ASSERT_NS(ot->ot_thread == xt_get_self());
	/* Make sure that the table recovery point, in
	 * particular the operation ID is recorded
	 * before all other flush activity!
	 *
	 * This is because only operations after the
	 * recovery point in the header are applied
	 * to the table on recovery.
	 *
	 * So the operation ID is recorded before the
	 * flush activity, and written after all is done.
	 */
	xt_tab_store_header(ot, &rec_head);

#ifdef TRACE_FLUSH
	printf("FLUSH rec/row %d %s\n", (int) tab->tab_bytes_to_flush, tab->tab_name->ps_path);
	fflush(stdout);
#endif
	/* Write the table header: */
	if (tab->tab_flush_pending) {
		tab->tab_flush_pending = FALSE;
		// Want to see how much was to be flushed in the debugger:
		to_flush = tab->tab_bytes_to_flush;
		tab->tab_bytes_to_flush = 0;
		if (bytes_flushed)
			*bytes_flushed += to_flush;
		/* Flush the table data: */
		if (!(tab->tab_dic.dic_tab_flags & XT_TAB_FLAGS_TEMP_TAB)) {
			if (!XT_FLUSH_RR_FILE(ot->ot_rec_file, &ot->ot_thread->st_statistics.st_rec, ot->ot_thread) ||
				!XT_FLUSH_RR_FILE(ot->ot_row_file, &ot->ot_thread->st_statistics.st_rec, ot->ot_thread)) {
				tab->tab_flush_pending = TRUE;
				goto failed;
			}
		}

		/* The header includes the operation number which
		 * must be written AFTER all other data,
		 * because operations will not be applied again.
		 */
		if (!xt_tab_write_header(ot, &rec_head, ot->ot_thread)) {
			tab->tab_flush_pending = TRUE;
			goto failed;
		}
	}

	/* Flush the auto-increment: */
	if (xt_db_auto_increment_mode == 1) {
		if (tab->tab_auto_inc != tab->tab_dic.dic_min_auto_inc) {
			tab->tab_dic.dic_min_auto_inc = tab->tab_auto_inc;
			if (!xt_tab_write_min_auto_inc(ot))
				goto failed;
		}
	}

	/* Mark this table as record/row flushed: */
	cp = &tab->tab_db->db_cp_state;
	xt_lock_mutex_ns(&cp->cp_state_lock);
	if (cp->cp_running) {
		cp_tab = (XTCheckPointTablePtr) xt_sl_find(NULL, cp->cp_table_ids, &tab->tab_id);
		if (cp_tab && (cp_tab->cpt_flushed & XT_CPT_ALL_FLUSHED) != XT_CPT_ALL_FLUSHED) {
			cp_tab->cpt_flushed |= XT_CPT_REC_ROW_FLUSHED;
			if ((cp_tab->cpt_flushed & XT_CPT_ALL_FLUSHED) == XT_CPT_ALL_FLUSHED) {
				ASSERT_NS(cp->cp_flush_count < xt_sl_get_size(cp->cp_table_ids));
				cp->cp_flush_count++;
			}
		}
	}
	xt_unlock_mutex_ns(&cp->cp_state_lock);

#ifdef TRACE_FLUSH
	printf("FLUSH --end-- %s\n", tab->tab_name->ps_path);
	fflush(stdout);
#endif
	xt_unlock_mutex_ns(&tab->tab_rec_flush_lock);

	if (!xt_end_checkpoint(tab->tab_db, ot->ot_thread, NULL))
		return FAILED;
	return OK;
	
	failed:
	xt_unlock_mutex_ns(&tab->tab_rec_flush_lock);
	return FAILED;
}

xtPublic void xt_flush_table(XTThreadPtr self, XTOpenTablePtr ot)
{
	/* GOTCHA [*10*]: This bug was difficult to find.
	 * It occured on Windows in the multi_update
	 * test, sometimes.
	 *
	 * What happens is the checkpointer starts to
	 * flush the table, and gets to the 
	 * XT_FLUSH_RR_FILE part.
	 *
	 * Then a rename occurs, and the user thread
	 * flushes the table, and goes through and
	 * writes the table header, with the most
	 * recent table operation (the last operation
	 * that occurred).
	 *
	 * The checkpointer the completes and
	 * also writes the header, but with old
	 * values (as read in xt_tab_store_header()).
	 *
	 * The then user thread continues, and
	 * reopens the table after rename.
	 * On reopen, it reads the old value from the header,
	 * and sets the current operation number.
	 *
	 * Now there is a problem in the able cache,
	 * because some cache pages have operation numbers
	 * that are greater than current operation
	 * number!
	 *
	 * This later lead to the free-er hanging while
	 * it waited for an operation to be 
	 * written to the disk that never would be.
	 * This is because a page can only be freed when
	 * the head operation number has passed the
	 * page operation number.
	 *
	 * Which indicates that the page has been written
	 * to disk.
	 */

	if (!xt_flush_record_row(ot, NULL, FALSE))
		xt_throw(self);

	/* This was before the table data flush,
	 * (after xt_tab_store_header() above,
	 * but I don't think it makes any difference.
	 * Because in the checkpointer it was at this
	 * position.
	 */
	if (!xt_flush_indices(ot, NULL, FALSE))
		xt_throw(self);

}

xtPublic XTOpenTablePtr tab_open_table(XTTableHPtr tab)
{
	volatile XTOpenTablePtr	ot;
	XTThreadPtr				self;

	if (!(ot = (XTOpenTablePtr) xt_malloc_ns(sizeof(XTOpenTableRec))))
		return NULL;
	memset(ot, 0, offsetof(XTOpenTableRec, ot_ind_wbuf));

	ot->ot_seq_page = NULL;
	ot->ot_seq_data = NULL;

	self = xt_get_self();
	try_(a) {
		xt_heap_reference(self, tab);
		ot->ot_table = tab;
#ifdef XT_USE_ROW_REC_MMAP_FILES
		ot->ot_row_file = xt_open_fmap(self, ot->ot_table->tab_row_file->fil_path, xt_db_row_file_grow_size);
		ot->ot_rec_file = xt_open_fmap(self, ot->ot_table->tab_rec_file->fil_path, xt_db_data_file_grow_size);
#else
		ot->ot_row_file = xt_open_file(self, ot->ot_table->tab_row_file->fil_path, XT_FS_DEFAULT);
		ot->ot_rec_file = xt_open_file(self, ot->ot_table->tab_rec_file->fil_path, XT_FS_DEFAULT);
#endif
#ifdef XT_USE_DIRECT_IO_ON_INDEX
		ot->ot_ind_file = xt_open_file(self, ot->ot_table->tab_ind_file->fil_path, XT_FS_MISSING_OK | XT_FS_DIRECT_IO);
#else
		ot->ot_ind_file = xt_open_file(self, ot->ot_table->tab_ind_file->fil_path, XT_FS_MISSING_OK);
#endif
	}
	catch_(a) {
		;
	}
	cont_(a);

	if (!ot->ot_table || !ot->ot_row_file || !ot->ot_rec_file)
		goto failed;

	if (!(ot->ot_row_rbuffer = (xtWord1 *) xt_malloc_ns(ot->ot_table->tab_dic.dic_rec_size)))
		goto failed;
	ot->ot_row_rbuf_size = ot->ot_table->tab_dic.dic_rec_size;
	if (!(ot->ot_row_wbuffer = (xtWord1 *) xt_malloc_ns(ot->ot_table->tab_dic.dic_rec_size)))
		goto failed;
	ot->ot_row_wbuf_size = ot->ot_table->tab_dic.dic_rec_size;

	/* Cache this stuff to speed access a bit: */
	ot->ot_rec_fixed = ot->ot_table->tab_dic.dic_rec_fixed;
	ot->ot_rec_size = ot->ot_table->tab_dic.dic_rec_size;

	return ot;

	failed:
	tab_close_table(ot);
	return NULL;
}

xtPublic XTOpenTablePtr xt_open_table(XTTableHPtr tab)
{
	return tab_open_table(tab);
}

xtPublic void xt_close_table(XTOpenTablePtr ot, xtBool flush, xtBool have_table_lock)
{
	if (flush) {
		if (!xt_flush_record_row(ot, NULL, have_table_lock))
			xt_log_and_clear_exception_ns();

		if (!xt_flush_indices(ot, NULL, have_table_lock))
			xt_log_and_clear_exception_ns();
	}
	tab_close_table(ot);
}

xtPublic int xt_use_table_by_id(XTThreadPtr self, XTTableHPtr *r_tab, XTDatabaseHPtr db, xtTableID tab_id)
{
	XTTableEntryPtr	te_ptr;
	XTTableHPtr		tab = NULL;
	int				r = XT_TAB_OK;
	char			path[PATH_MAX];

	if (!db)
		xt_throw_xterr(XT_CONTEXT, XT_ERR_NO_DATABASE_IN_USE);
	xt_ht_lock(self, db->db_tables);
	pushr_(xt_ht_unlock, db->db_tables);

	te_ptr = (XTTableEntryPtr) xt_sl_find(self, db->db_table_by_id, &tab_id);
	if (te_ptr) {
		if (!(tab = te_ptr->te_table)) {
			/* Open the table: */
			xt_strcpy(PATH_MAX, path, te_ptr->te_tab_path->tp_path);
			xt_add_dir_char(PATH_MAX, path);
			xt_strcat(PATH_MAX, path, te_ptr->te_tab_name);
			r = tab_new_handle(self, &tab, db, tab_id, (XTPathStrPtr) path, TRUE, NULL);
		}
	}
	else
		r = XT_TAB_NOT_FOUND;

	if (tab)
		xt_heap_reference(self, tab);
	*r_tab = tab;

	freer_(); // xt_ht_unlock(db->db_tables)
	return r;
}

/* The fixed part of the record is already in the row buffer.
 * This function loads the extended part, expanding the row
 * buffer if necessary.
 */
xtPublic xtBool xt_tab_load_ext_data(XTOpenTablePtr ot, xtRecordID load_rec_id, xtWord1 *buffer, u_int cols_req)
{
	size_t					log_size;
	xtLogID					log_id;
	xtLogOffset				log_offset;
	xtWord1					save_buffer[offsetof(XTactExtRecEntryDRec, er_data)];
	xtBool					retried = FALSE;
	XTactExtRecEntryDPtr	ext_data_ptr;
	size_t					log_size2;
	xtTableID				curr_tab_id;
	xtRecordID				curr_rec_id;

	log_size = XT_GET_DISK_4(((XTTabRecExtDPtr) ot->ot_row_rbuffer)->re_log_dat_siz_4);
	XT_GET_LOG_REF(log_id, log_offset, (XTTabRecExtDPtr) ot->ot_row_rbuffer);

	if (ot->ot_rec_size + log_size > ot->ot_row_rbuf_size) {
		if (!xt_realloc_ns((void **) &ot->ot_row_rbuffer, ot->ot_rec_size + log_size))
			return FAILED;
		ot->ot_row_rbuf_size = ot->ot_rec_size + log_size;
	}

	/* Read the extended part first: */
	ext_data_ptr = (XTactExtRecEntryDPtr) (ot->ot_row_rbuffer + ot->ot_rec_size - offsetof(XTactExtRecEntryDRec, er_data));

	/* Save the data which the header will overwrite: */
	memcpy(save_buffer, ext_data_ptr, offsetof(XTactExtRecEntryDRec, er_data));
	
	reread:
	if (!ot->ot_thread->st_dlog_buf.dlb_read_log(log_id, log_offset, offsetof(XTactExtRecEntryDRec, er_data) + log_size, (xtWord1 *) ext_data_ptr, ot->ot_thread))
		goto retry_read;

	log_size2 = XT_GET_DISK_4(ext_data_ptr->er_data_size_4);
	curr_tab_id = XT_GET_DISK_4(ext_data_ptr->er_tab_id_4);
	curr_rec_id = XT_GET_DISK_4(ext_data_ptr->er_rec_id_4);

	if (log_size2 != log_size || curr_tab_id != ot->ot_table->tab_id || curr_rec_id != load_rec_id) {
		/* [(3)] This can happen in the following circumstances:
		 * - A new record is created, but the data log is not
		 * flushed.
		 * - The server quits.
		 * - On restart the transaction is rolled back, but the data record
		 *   was not written, so later a new record could be written at this
		 *   location.
		 * - Later the sweeper tries to cleanup this record, and finds
		 *   that a different record has been written at this position.
		 *
		 * NOTE: Index entries can only be written to disk for records
		 *       that have been committed to the disk, because uncommitted
		 *       records may not exist in order to remove the index entry
		 *       on cleanup.
		 */
		xt_register_xterr(XT_REG_CONTEXT, XT_ERR_BAD_EXT_RECORD);
		goto retry_read;
	}

	/* Restore the saved area: */
	memcpy(ext_data_ptr, save_buffer, offsetof(XTactExtRecEntryDRec, er_data));

	if (retried)
		xt_unlock_mutex_ns(&ot->ot_table->tab_db->db_co_ext_lock);
	return myxt_load_row(ot, ot->ot_row_rbuffer + XT_REC_EXT_HEADER_SIZE, buffer, cols_req);

	retry_read:
	if (!retried) {
		/* (1) It may be that reading the log fails because the garbage collector
		 * has moved the record since we determined the location.
		 * We handle this here, by re-reading the data the garbage collector
		 * would have updated.
		 *
		 * (2) It may also happen that a new record is just being updated or
		 * inserted. It is possible that the handle part of the record
		 * has been written, but not yet the overflow.
		 * This means that repeating the read attempt could work.
		 *
		 * (3) The extended data has been written by another handler and not yet
		 * flushed. This should not happen because on committed extended
		 * records are read, and all data should be flushed before
		 * commit!
		 *
		 * NOTE: (2) above is not a problem when versioning is working
		 * correctly. In this case, we should never try to read the extended
		 * part of an uncommitted record (belonging to some other thread/
		 * transaction).
		 */
		XTTabRecExtDRec	rec_buf;

		xt_lock_mutex_ns(&ot->ot_table->tab_db->db_co_ext_lock);
		retried = TRUE;

		if (!xt_tab_get_rec_data(ot, load_rec_id, XT_REC_EXT_HEADER_SIZE, (xtWord1 *) &rec_buf))
			goto failed;

		XT_GET_LOG_REF(log_id, log_offset, &rec_buf);
		goto reread;
	}

	failed:
	if (retried)
		xt_unlock_mutex_ns(&ot->ot_table->tab_db->db_co_ext_lock);
	return FAILED;
}

xtPublic xtBool xt_tab_put_rec_data(XTOpenTablePtr ot, xtRecordID rec_id, size_t size, xtWord1 *buffer, xtOpSeqNo *op_seq)
{
	register XTTableHPtr	tab = ot->ot_table;

	ASSERT_NS(rec_id);

	return tab->tab_recs.xt_tc_write(ot->ot_rec_file, rec_id, 0, size, buffer, op_seq, TRUE, ot->ot_thread);
}

xtPublic xtBool xt_tab_put_log_op_rec_data(XTOpenTablePtr ot, u_int status, xtRecordID free_rec_id, xtRecordID rec_id, size_t size, xtWord1 *buffer)
{
	register XTTableHPtr	tab = ot->ot_table;
	xtOpSeqNo				op_seq;

	ASSERT_NS(rec_id);

	if (status == XT_LOG_ENT_REC_MOVED) {
		if (!tab->tab_recs.xt_tc_write(ot->ot_rec_file, rec_id, offsetof(XTTabRecExtDRec, re_log_id_2), size, buffer, &op_seq, TRUE, ot->ot_thread))
			return FAILED;
	}
#ifdef DEBUG
	else if (status == XT_LOG_ENT_REC_CLEANED_1) {
		ASSERT_NS(0);	// shouldn't be used anymore
	}
#endif
	else {
		if (!tab->tab_recs.xt_tc_write(ot->ot_rec_file, rec_id, 0, size, buffer, &op_seq, TRUE, ot->ot_thread))
			return FAILED;
	}

	return xt_xlog_modify_table(ot, status, op_seq, free_rec_id, rec_id, size, buffer);
}

xtPublic xtBool xt_tab_put_log_rec_data(XTOpenTablePtr ot, u_int status, xtRecordID free_rec_id, xtRecordID rec_id, size_t size, xtWord1 *buffer, xtOpSeqNo *op_seq)
{
	register XTTableHPtr	tab = ot->ot_table;

	ASSERT_NS(rec_id);

	if (status == XT_LOG_ENT_REC_MOVED) {
		if (!tab->tab_recs.xt_tc_write(ot->ot_rec_file, rec_id, offsetof(XTTabRecExtDRec, re_log_id_2), size, buffer, op_seq, TRUE, ot->ot_thread))
			return FAILED;
	}
	else {
		if (!tab->tab_recs.xt_tc_write(ot->ot_rec_file, rec_id, 0, size, buffer, op_seq, TRUE, ot->ot_thread))
			return FAILED;
	}

	return xt_xlog_modify_table(ot, status, *op_seq, free_rec_id, rec_id, size, buffer);
}

xtPublic xtBool xt_tab_get_rec_data(XTOpenTablePtr ot, xtRecordID rec_id, size_t size, xtWord1 *buffer)
{
	register XTTableHPtr	tab = ot->ot_table;

	ASSERT_NS(rec_id);

	return tab->tab_recs.xt_tc_read(ot->ot_rec_file, rec_id, (size_t) size, buffer, ot->ot_thread);
}

/*
 * Note: this function grants locks even to transactions that
 * are not specifically waiting for this transaction.
 * This is required, because all threads waiting for 
 * a lock should be considered "equal". In other words,
 * they should not have to wait for the "right" transaction
 * before they get the lock, or it will turn into a
 * race to wait for the correct transaction.
 *
 * A transaction T1 can end up waiting for the wrong transaction
 * T2, because T2 has released the lock, and given it to T3.
 * Of course, T1 will wake up soon and realize this, but
 * it is a matter of timing.
 *
 * The main point is that T2 has release the lock because
 * it has ended (see {RELEASING-LOCKS} for more details)
 * and therefore, there is no danger of it claiming the
 * lock again, which can lead to a deadlock if T1 is
 * given the lock instead of T3 in the example above.
 * Then, if T2 tries to regain the lock before T1
 * realizes that it has the lock.
 */
//static xtBool tab_get_lock_after_wait(XTThreadPtr thread, XTLockWaitPtr lw)
//{
//	register XTTableHPtr	tab = lw->lw_ot->ot_table;

	/* {ROW-LIST-LOCK}
	 * I don't believe this lock is required. If it is, please explain why!!
	 * XT_TAB_ROW_READ_LOCK(&tab->tab_row_rwlock[gl->lw_row_id % XT_ROW_RWLOCKS], thread);
	 *
	 * With the old row lock implementation a XT_TAB_ROW_WRITE_LOCK was required because
	 * the row locking did not have its own locks.
	 * The new list locking has its own locks. I was using XT_TAB_ROW_READ_LOCK,
	 * but i don't think this is required.
	 */
//	return tab->tab_locks.xt_set_temp_lock(lw->lw_ot, lw, &lw->lw_thread->st_lock_list);
//}

/*
 * NOTE: Previously this function did not gain the row lock.
 * If this change is a problem, please document why!
 * The previously implementation did wait until no lock was on the
 * row.
 *
 * I am thinking that it is simply a good idea to grab the lock,
 * instead of waiting for no lock, before the retry. But it could
 * result in locking more than required!
 */
static xtBool tab_wait_for_update(register XTOpenTablePtr ot, xtRowID row_id, xtXactID xn_id, XTThreadPtr thread)
{
	XTLockWaitRec	lw;
	XTXactWaitRec	xw;
	xtBool			ok;
				
	xw.xw_xn_id = xn_id;

	lw.lw_thread = thread;
	lw.lw_ot = ot;
	lw.lw_row_id = row_id;
	lw.lw_row_updated = FALSE;

	/* First try to get the lock: */
	if (!ot->ot_table->tab_locks.xt_set_temp_lock(ot, &lw, &thread->st_lock_list))
		return FAILED;
	if (lw.lw_curr_lock != XT_NO_LOCK)
		/* Wait for the lock, then the transaction: */
		ok = xt_xn_wait_for_xact(thread, &xw, &lw);
	else
		/* Just wait for the transaction: */
		ok = xt_xn_wait_for_xact(thread, &xw, NULL);
	
#ifdef DEBUG_LOCK_QUEUE
	ot->ot_table->tab_locks.rl_check(&lw);
#endif
	return ok;
}

/* {WAIT-FOR}
 * XT_OLD - The record is old. No longer visible because there is
 * newer committed record before it in the record list.
 * This is a special case of FALSE (the record is not visible).
 * (see {WAIT-FOR} for details).
 * It is significant because if we find too many of these when
 * searching for records, then we have reason to believe the
 * sweeper is far behind. This can happen in a test like this:
 * runTest(INCREMENT_TEST, 2, INCREMENT_TEST_UPDATE_COUNT);
 * What happens is T1 detects an updated row by T2,
 * but T2 has not committed yet.
 * It waits for T2. T2 commits and updates again before T1
 * can update.
 *
 * Of course if we got a lock on the row when T2 quits, then
 * this would not happen!
 */

/*
 * Is a record visible?
 * Returns TRUE, FALSE, XT_ERR.
 *
 * TRUE - The record is visible.
 * FALSE - The record is not visible.
 * XT_ERR - An exception (error) occurred.
 * XT_NEW - The most recent variation of this row has been returned
 * and is to be used instead of the input!
 * XT_REREAD - Re-read the record, and try again.
 *
 * Basically, a record is visible if it was committed on or before
 * the transactions "visible time" (st_visible_time), and there
 * are no other visible records before this record in the
 * variation chain for the record.
 *
 * This holds in general, but you don't always get to see the
 * visible record (as defined in this sence).
 *
 * On any kind of update (SELECT FOR UPDATE, UPDATE or DELETE), you
 * get to see the most recent variation of the row!
 *
 * So on update, this function will wait if necessary for a recent
 * update to be committed.
 *
 * So an update is a kind of "committed read" with a wait for
 * uncommitted records.
 *
 * The result:
 * - INSERTS may not seen by the update read, depending on when
 *   they occur.
 * - Records may be returned in non-index order.
 * - New records returned must be checked again by an index scan
 *   to make sure they conform to the condition!
 * 
 * CREATE TABLE test_tab (ID int primary key, Value int, Name varchar(20), 
 * index(Value, Name)) ENGINE=pbxt;
 * INSERT test_tab values(4, 2, 'D');
 * INSERT test_tab values(5, 2, 'E');
 * INSERT test_tab values(6, 2, 'F');
 * INSERT test_tab values(7, 2, 'G');
 * 
 * -- C1
 * begin;
 * select * from test_tab where id = 6 for update;
 * -- C2
 * begin;
 * select * from test_tab where value = 2 order by value, name for update;
 * -- C1
 * update test_tab set Name = 'A' where id = 7;
 * commit;
 * -- C2
 * Result order D, E, F, A.
 *
 * But Jim does it like this, so it should be OK.
 */
static int tab_visible(register XTOpenTablePtr ot, XTTabRecHeadDPtr rec_head, xtRecordID *new_rec_id)
{
	XTThreadPtr				thread = ot->ot_thread;
	xtXactID				xn_id;
	XTTabRecHeadDRec		var_head;
	xtRowID					row_id;
	xtRecordID				var_rec_id;
	register XTTableHPtr	tab;
	xtBool					wait = FALSE;
	xtXactID				wait_xn_id = 0;
#ifdef TRACE_VARIATIONS
	char					t_buf[500];
	int						len;
#endif
	int						result = TRUE;
	xtBool					rec_clean;
	xtRecordID				invalid_rec;

	retry:
	/* It can be that between the time that I read the index,
	 * and the time that I try to access the
	 * record, that the record is removed by
	 * the sweeper!
	 */
	if (XT_REC_NOT_VALID(rec_head->tr_rec_type_1))
		return FALSE;

	row_id = XT_GET_DISK_4(rec_head->tr_row_id_4);

	/* This can happen if the row has been removed, and
	 * reused:
	 */
	if (ot->ot_curr_row_id && row_id != ot->ot_curr_row_id)
		return FALSE;

#ifdef TRACE_VARIATIONS
	len = sprintf(t_buf, "row=%d rec=%d ", (int) row_id, (int) ot->ot_curr_rec_id);
#endif
	if (!(rec_clean = XT_REC_IS_CLEAN(rec_head->tr_rec_type_1))) {
		/* The record is not clean, which means it has not been swept.
		 * So we have to check if it is visible.
		 */
		xn_id = XT_GET_DISK_4(rec_head->tr_xact_id_4);
		switch (xt_xn_status(ot, xn_id, ot->ot_curr_rec_id)) {
			case XT_XN_VISIBLE:
				break;
			case XT_XN_NOT_VISIBLE:
				if (ot->ot_for_update) {
					/* It is visible, only if it is an insert,
					 * which means if has no previous variation.
					 * Note, if an insert is updated, the record
					 * should be overwritten (TODO - check this).
					 */
					var_rec_id = XT_GET_DISK_4(rec_head->tr_prev_rec_id_4);
					if (!var_rec_id)
						break;
#ifdef TRACE_VARIATIONS
					if (len <= 450)
						len += sprintf(t_buf+len, "OTHER COMMIT (OVERWRITTEN) T%d\n", (int) xn_id);
					xt_ttracef(thread, "%s", t_buf);
#endif
				}
#ifdef TRACE_VARIATIONS
				else {
					if (len <= 450)
						len += sprintf(t_buf+len, "OTHER COMMIT T%d\n", (int) xn_id);
					xt_ttracef(thread, "%s", t_buf);
				}
#endif
				/* {WAKE-SW}
				 * The record is not visible, although it has been committed.
				 * Clean the transaction ASAP.
				 */
				ot->ot_table->tab_db->db_sw_faster |= XT_SW_DIRTY_RECORD_FOUND;
				return FALSE;
			case XT_XN_ABORTED:
				/* {WAKE-SW}
				 * Reading an aborted record, this transaction
				 * must be cleaned up ASAP!
				 */
				ot->ot_table->tab_db->db_sw_faster |= XT_SW_DIRTY_RECORD_FOUND;
#ifdef TRACE_VARIATIONS
				if (len <= 450)
					len += sprintf(t_buf+len, "ABORTED T%d\n", (int) xn_id);
				xt_ttracef(thread, "%s", t_buf);
#endif
				return FALSE;
			case XT_XN_MY_UPDATE:
				/* This is a record written by this transaction. */
				if (thread->st_is_update) {
					/* Check that it was not written by the current update statement: */
					if (XT_STAT_ID_MASK(thread->st_update_id) == rec_head->tr_stat_id_1) {
#ifdef TRACE_VARIATIONS
						if (len <= 450)
							len += sprintf(t_buf+len, "MY UPDATE IN THIS STATEMENT T%d\n", (int) xn_id);
						xt_ttracef(thread, "%s", t_buf);
#endif
						return FALSE;
					}
				}
				ot->ot_curr_row_id = row_id;
				ot->ot_curr_updated = TRUE;
				if (!(xt_tab_get_row(ot, row_id, &var_rec_id)))
					return XT_ERR;
				/* It is visible if it is at the front of the list.
				 * An update can end up not being at the front of the list
				 * if it is deleted afterwards!
				 */
#ifdef TRACE_VARIATIONS
				if (len <= 450) {
					if (var_rec_id == ot->ot_curr_rec_id)
						len += sprintf(t_buf+len, "MY UPDATE T%d\n", (int) xn_id);
					else
						len += sprintf(t_buf+len, "MY UPDATE (OVERWRITTEN) T%d\n", (int) xn_id);
				}
				xt_ttracef(thread, "%s", t_buf);
#endif
				return var_rec_id == ot->ot_curr_rec_id;
			case XT_XN_OTHER_UPDATE:
				if (ot->ot_for_update) {
					/* If this is an insert, we are interested!
					 * Updated values are handled below. This is because
					 * the changed (new) records returned below are always
					 * followed (in the version chain) by the record
					 * we would have returned (if nothing had changed).
					 *
					 * As a result, we only return records here which have
					 * no "history". 
					 */
					var_rec_id = XT_GET_DISK_4(rec_head->tr_prev_rec_id_4);
					if (!var_rec_id) {
#ifdef TRACE_VARIATIONS
						if (len <= 450)
							len += sprintf(t_buf+len, "OTHER INSERT (WAIT FOR) T%d\n", (int) xn_id);
						xt_ttracef(thread, "%s", t_buf);
#endif
						if (!tab_wait_for_update(ot, row_id, xn_id, thread))
							return XT_ERR;
						if (!xt_tab_get_rec_data(ot, ot->ot_curr_rec_id, sizeof(XTTabRecHeadDRec), (xtWord1 *) &var_head))
							return XT_ERR;
						rec_head = &var_head;
						goto retry;
					}
				}
#ifdef TRACE_VARIATIONS
				if (len <= 450)
					len += sprintf(t_buf+len, "OTHER UPDATE T%d\n", (int) xn_id);
				xt_ttracef(thread, "%s", t_buf);
#endif
				return FALSE;
			case XT_XN_REREAD:
#ifdef TRACE_VARIATIONS
				if (len <= 450)
					len += sprintf(t_buf+len, "REREAD?! T%d\n", (int) xn_id);
				xt_ttracef(thread, "%s", t_buf);
#endif
				return XT_REREAD;
		}
	}

	/* Follow the variation chain until we come to this record.
	 * If it is not the first visible variation then
	 * it is not visible at all. If it in not found on the
	 * variation chain, it is also not visible.
	 */
	tab = ot->ot_table;

	retry_2:

#ifdef XT_USE_LIST_BASED_ROW_LOCKS
	/* The list based row locks used there own locks, so
	 * it is not necessary to get a write lock here.
	 */
	XT_TAB_ROW_READ_LOCK(&tab->tab_row_rwlock[row_id % XT_ROW_RWLOCKS], thread);
#else
	if (ot->ot_for_update)
		XT_TAB_ROW_WRITE_LOCK(&tab->tab_row_rwlock[row_id % XT_ROW_RWLOCKS], thread);
	else
		XT_TAB_ROW_READ_LOCK(&tab->tab_row_rwlock[row_id % XT_ROW_RWLOCKS], thread);
#endif

	invalid_rec = 0;
	retry_3:
	if (!(xt_tab_get_row(ot, row_id, &var_rec_id)))
		goto failed;
#ifdef TRACE_VARIATIONS
	len += sprintf(t_buf+len, "ROW=%d", (int) row_id);
#endif
	while (var_rec_id != ot->ot_curr_rec_id) {
		if (!var_rec_id) {
#ifdef TRACE_VARIATIONS
			xt_ttracef(thread, "row=%d rec=%d NOT VISI not found in list\n", (int) row_id, (int) ot->ot_curr_rec_id);
#endif
			goto not_found;
		}
		if (!xt_tab_get_rec_data(ot, var_rec_id, sizeof(XTTabRecHeadDRec), (xtWord1 *) &var_head))
			goto failed;
#ifdef TRACE_VARIATIONS
		if (len <= 450)
			len += sprintf(t_buf+len, " -> %d(%d)", (int) var_rec_id, (int) var_head.tr_rec_type_1);
#endif
		/* All clean records are visible, by all transactions: */
		if (XT_REC_IS_CLEAN(var_head.tr_rec_type_1)) {
#ifdef TRACE_VARIATIONS
			xt_ttracef(thread, "row=%d rec=%d NOT VISI clean rec found\n", (int) row_id, (int) ot->ot_curr_rec_id);
#endif
			goto not_found;
		}
		if (XT_REC_IS_FREE(var_head.tr_rec_type_1)) {
#ifdef TRACE_VARIATIONS
			xt_ttracef(thread, "row=%d rec=%d NOT VISI free rec found?!\n", (int) row_id, (int) ot->ot_curr_rec_id);
#endif
			/*
			 * After an analysis we came to conclusion that this situation is
			 * possible and valid. It can happen if index scan and row deletion
			 * go in parallel:
			 *
			 *      Client Thread                                Sweeper
			 *      -------------                                -------
			 *   1. start index scan, lock the index file.
			 *                                                2. start row deletion, wait for index lock
			 *   3. unlock the index file, start search for 
			 *      the valid version of the record
			 *                                                4. delete the row, mark record as freed, 
			 *                                                   but not yet cleaned by sweeper
			 *   5. observe the record being freed
			 *
			 * after these steps we can get here, if the record was marked as free after
			 * the tab_visible was entered by the scanning thread. 
			 *
			 */
			if (invalid_rec != var_rec_id) {
				/* This was "var_rec_id = invalid_rec", caused an infinite loop (bug #310184!) */
				invalid_rec = var_rec_id;
				goto retry_3;
			}
			/* Assume end of list. */
			goto not_found;
		}

		/* This can happen if the row has been removed, and
		 * reused:
		 */
		if (row_id != XT_GET_DISK_4(var_head.tr_row_id_4))
			goto not_found;

		xn_id = XT_GET_DISK_4(var_head.tr_xact_id_4);
		/* This variation is visibleif committed before this
		 * transaction started, or updated by this transaction.
		 *
		 * We now know that this is the valid variation for
		 * this record (for this table) for this transaction!
		 * This will not change, unless the transaction
		 * updates the record (again).
		 *
		 * So we can store this information as a hint, if
		 * we see other variations belonging to this record,
		 * then we can ignore them immediately!
		 */
		switch (xt_xn_status(ot, xn_id, var_rec_id)) {
			case XT_XN_VISIBLE:
				/* {WAKE-SW}
				 * We have encountered a record that has been overwritten, if the
				 * record has not been cleaned, then the sweeper is too far
				 * behind!
				 */
				if (!rec_clean)
					ot->ot_table->tab_db->db_sw_faster |= XT_SW_DIRTY_RECORD_FOUND;
#ifdef TRACE_VARIATIONS
				xt_ttracef(thread, "row=%d rec=%d NOT VISI committed rec found\n", (int) row_id, (int) ot->ot_curr_rec_id);
#endif
				goto not_found;
			case XT_XN_NOT_VISIBLE:
				if (ot->ot_for_update) {
					/* Substitute this record for the one we
					 * are reading!!
					 */
					if (result == TRUE) {
						if (XT_REC_IS_DELETE(var_head.tr_rec_type_1))
							result = FALSE;
						else {
							*new_rec_id = var_rec_id;
							result = XT_NEW;
						}
					}
				}
				break;
			case XT_XN_ABORTED:
				/* Ignore the record, it will be removed. */
				break;
			case XT_XN_MY_UPDATE:
#ifdef TRACE_VARIATIONS
				xt_ttracef(thread, "row=%d rec=%d NOT VISI my update found\n", (int) row_id, (int) ot->ot_curr_rec_id);
#endif
				goto not_found;
			case XT_XN_OTHER_UPDATE:
				/* Wait for this update to commit or abort: */
				if (!wait) {
					wait = TRUE;
					wait_xn_id = xn_id;
				}
#ifdef TRACE_VARIATIONS
				if (len <= 450)
					len += sprintf(t_buf+len, "-T%d", (int) wait_xn_id);
#endif
				break;
			case XT_XN_REREAD:
				if (invalid_rec != var_rec_id) {
					invalid_rec = var_rec_id;
					goto retry_3;
				}
				/* Assume end of list. */
#ifdef XT_CRASH_DEBUG
				/* Should not happen! */
				xt_crash_me();
#endif
				goto not_found;
		}
		var_rec_id = XT_GET_DISK_4(var_head.tr_prev_rec_id_4);
	}
#ifdef TRACE_VARIATIONS
	if (len <= 450)
		sprintf(t_buf+len, " -> %d(%d)\n", (int) var_rec_id, (int) rec_head->tr_rec_type_1);
	else
		sprintf(t_buf+len, " ...\n");
	//xt_ttracef(thread, "%s", t_buf);
#endif

	if (ot->ot_for_update) {
		xtBool			ok;
		XTLockWaitRec	lw;

		if (wait) {
			XT_TAB_ROW_UNLOCK(&tab->tab_row_rwlock[row_id % XT_ROW_RWLOCKS], thread);
#ifdef TRACE_VARIATIONS
			xt_ttracef(thread, "T%d WAIT FOR T%d (will retry)\n", (int) thread->st_xact_data->xd_start_xn_id, (int) wait_xn_id);
#endif
			if (!tab_wait_for_update(ot, row_id, wait_xn_id, thread))
				return XT_ERR;
			wait = FALSE;
			wait_xn_id = 0;
			/*
			 * Retry in order to try to avoid missing
			 * any records that we should see in FOR UPDATE
			 * mode.
			 *
			 * We also want to take another look at the record
			 * we just tried to read.
			 *
			 * If it has been updated, then a new record has
			 * been created. This will be detected when we
			 * try to read it again, and XT_NEW will be returned.
			 */
			thread->st_statistics.st_retry_index_scan++;
			return XT_RETRY;
		}

		/* {ROW-LIST-LOCK} */
		lw.lw_thread = thread;
		lw.lw_ot = ot;
		lw.lw_row_id = row_id;
		lw.lw_row_updated = FALSE;
		ok = tab->tab_locks.xt_set_temp_lock(ot, &lw, &thread->st_lock_list);
		XT_TAB_ROW_UNLOCK(&tab->tab_row_rwlock[row_id % XT_ROW_RWLOCKS], thread);
		if (!ok) {
#ifdef DEBUG_LOCK_QUEUE
			ot->ot_table->tab_locks.rl_check(&lw);
#endif
			return XT_ERR;
		}
		if (lw.lw_curr_lock != XT_NO_LOCK) {
#ifdef TRACE_VARIATIONS
			xt_ttracef(thread, "T%d WAIT FOR LOCK(%D) T%d\n", (int) thread->st_xact_data->xd_start_xn_id, (int) lock_type, (int) xn_id);
#endif
			if (!xt_xn_wait_for_xact(thread, NULL, &lw)) {
#ifdef DEBUG_LOCK_QUEUE
				ot->ot_table->tab_locks.rl_check(&lw);
#endif
				return XT_ERR;
			}
#ifdef DEBUG_LOCK_QUEUE
			ot->ot_table->tab_locks.rl_check(&lw);
#endif
#ifdef TRACE_VARIATIONS
			len = sprintf(t_buf, "(retry): row=%d rec=%d ", (int) row_id, (int) ot->ot_curr_rec_id);
#endif
			/* GOTCHA!
			 * Reset the result before we go down the list again, to make sure we
			 * get the latest record!!
			 */
			result = TRUE;
			thread->st_statistics.st_reread_record_list++;
			goto retry_2;
		}
#ifdef DEBUG_LOCK_QUEUE
		ot->ot_table->tab_locks.rl_check(&lw);
#endif
	}
	else {
		XT_TAB_ROW_UNLOCK(&tab->tab_row_rwlock[row_id % XT_ROW_RWLOCKS], thread);
	}

#ifdef TRACE_VARIATIONS
	if (result == XT_NEW)
		xt_ttracef(thread, "row=%d rec=%d RETURN NEW %d\n", (int) row_id, (int) ot->ot_curr_rec_id, (int) *new_rec_id);
	else if (result)
		xt_ttracef(thread, "row=%d rec=%d VISIBLE\n", (int) row_id, (int) ot->ot_curr_rec_id);
	else
		xt_ttracef(thread, "row=%d rec=%d RETURN NOT VISIBLE (NEW)\n", (int) row_id, (int) ot->ot_curr_rec_id);
#endif

	ot->ot_curr_row_id = row_id;
	ot->ot_curr_updated = FALSE;
	return result;

	not_found:
	XT_TAB_ROW_UNLOCK(&tab->tab_row_rwlock[row_id % XT_ROW_RWLOCKS], thread);
	return FALSE;

	failed:
	XT_TAB_ROW_UNLOCK(&tab->tab_row_rwlock[row_id % XT_ROW_RWLOCKS], thread);
	return XT_ERR;
}

/*
 * Return TRUE if the record has been read, and is visible.
 * Return FALSE if the record is not visible.
 * Return XT_ERR if an error occurs.
 */
xtPublic int xt_tab_visible(XTOpenTablePtr ot)
{
	xtRowID				row_id;
	XTTabRecHeadDRec	rec_head;
	xtRecordID			new_rec_id;
	xtBool				read_again = FALSE;
	int					r;

	if ((row_id = ot->ot_curr_row_id)) {
		/* Fast track, do a quick check.
		 * Row ID is only set if this record has been committed,
		 * (and swept).
		 * Check if it is the first on the list!
		 */
		xtRecordID var_rec_id;

		retry:
		if (!(xt_tab_get_row(ot, row_id, &var_rec_id)))
			return XT_ERR;
		if (ot->ot_curr_rec_id == var_rec_id) {
			/* Looks good.. */
			if (ot->ot_for_update) {
				XTThreadPtr		thread = ot->ot_thread;
				XTTableHPtr		tab = ot->ot_table;
				XTLockWaitRec	lw;

				/* {ROW-LIST-LOCK} */
				lw.lw_thread = thread;
				lw.lw_ot = ot;
				lw.lw_row_id = row_id;
				lw.lw_row_updated = FALSE;
				if (!tab->tab_locks.xt_set_temp_lock(ot, &lw, &thread->st_lock_list)) {
#ifdef DEBUG_LOCK_QUEUE
					ot->ot_table->tab_locks.rl_check(&lw);
#endif
					return XT_ERR;
				}
				if (lw.lw_curr_lock != XT_NO_LOCK) {
					if (!xt_xn_wait_for_xact(thread, NULL, &lw)) {
#ifdef DEBUG_LOCK_QUEUE
						ot->ot_table->tab_locks.rl_check(&lw);
#endif
						return XT_ERR;
					}
#ifdef DEBUG_LOCK_QUEUE
					ot->ot_table->tab_locks.rl_check(&lw);
#endif
					goto retry;
				}
#ifdef DEBUG_LOCK_QUEUE
				ot->ot_table->tab_locks.rl_check(&lw);
#endif
			}
			return TRUE;
		}
	}

	reread:
	if (!xt_tab_get_rec_data(ot, ot->ot_curr_rec_id, sizeof(XTTabRecHeadDRec), (xtWord1 *) &rec_head))
		return XT_ERR;

	switch ((r = tab_visible(ot, &rec_head, &new_rec_id))) {
		case XT_NEW:
			ot->ot_curr_rec_id = new_rec_id;
			break;
		case XT_REREAD:
			/* Avoid infinite loop: */
			if (read_again) {
				/* Should not happen! */
#ifdef XT_CRASH_DEBUG
				/* Generate a core dump! */
				xt_crash_me();
#endif
				return FALSE;
			}
			read_again = TRUE;
			goto reread;
		default:
			break;
	}
	return r;
}

/*
 * Read a record, and return one of the following:
 * TRUE - the record has been read, and is visible.
 * FALSE - the record is not visible.
 * XT_ERR - an error occurs.
 * XT_NEW - Means the expected record has been changed.
 * When doing an index scan, the conditions must be checked again!
 */
xtPublic int xt_tab_read_record(register XTOpenTablePtr ot, xtWord1 *buffer)
{
	register XTTableHPtr	tab = ot->ot_table;
	size_t					rec_size = tab->tab_dic.dic_rec_size;
	xtRecordID				new_rec_id;
	int						result;
	xtBool					read_again = FALSE;

	if (!(ot->ot_thread->st_xact_data)) {
		xt_register_xterr(XT_REG_CONTEXT, XT_ERR_NO_TRANSACTION);
		return XT_ERR;
	}

	reread:
	if (!xt_tab_get_rec_data(ot, ot->ot_curr_rec_id, rec_size, ot->ot_row_rbuffer))
		return XT_ERR;

	switch (tab_visible(ot, (XTTabRecHeadDPtr) ot->ot_row_rbuffer, &new_rec_id)) {
		case FALSE:
			return FALSE;
		case XT_ERR:
			return XT_ERR;
		case XT_NEW:
			if (!xt_tab_get_rec_data(ot, new_rec_id, rec_size, ot->ot_row_rbuffer))
				return XT_ERR;
			ot->ot_curr_rec_id = new_rec_id;
			result = XT_NEW;
			break;
		case XT_RETRY:
			return XT_RETRY;
		case XT_REREAD:
			/* Avoid infinite loop: */
			if (read_again) {
				/* Should not happen! */
#ifdef XT_CRASH_DEBUG
				/* Generate a core dump! */
				xt_crash_me();
#endif
				return FALSE;
			}
			read_again = TRUE;
			goto reread;
		default:
			result = OK;
			break;
	}

	if (ot->ot_rec_fixed)
		memcpy(buffer, ot->ot_row_rbuffer + XT_REC_FIX_HEADER_SIZE, rec_size - XT_REC_FIX_HEADER_SIZE);
	else if (ot->ot_row_rbuffer[0] == XT_TAB_STATUS_VARIABLE || ot->ot_row_rbuffer[0] == XT_TAB_STATUS_VAR_CLEAN) {
		if (!myxt_load_row(ot, ot->ot_row_rbuffer + XT_REC_FIX_HEADER_SIZE, buffer, ot->ot_cols_req))
			return XT_ERR;
	}
	else {
		u_int cols_req = ot->ot_cols_req;

		ASSERT_NS(cols_req);
		if (cols_req && cols_req <= tab->tab_dic.dic_fix_col_count) {
			if (!myxt_load_row(ot, ot->ot_row_rbuffer + XT_REC_EXT_HEADER_SIZE, buffer, cols_req))
				return XT_ERR;
		}
		else {
			if (!xt_tab_load_ext_data(ot, ot->ot_curr_rec_id, buffer, cols_req))
				return XT_ERR;
		}
	}

	return result;
}

/*
 * Returns:
 *
 * TRUE/OK - record was read.
 * FALSE/FAILED - An error occurred.
 */
xtPublic int xt_tab_dirty_read_record(register XTOpenTablePtr ot, xtWord1 *buffer)
{
	register XTTableHPtr	tab = ot->ot_table;
	size_t					rec_size = tab->tab_dic.dic_rec_size;

	if (!xt_tab_get_rec_data(ot, ot->ot_curr_rec_id, rec_size, ot->ot_row_rbuffer))
		return FAILED;

	if (XT_REC_NOT_VALID(ot->ot_row_rbuffer[0])) {
		/* Should not happen! */
		xt_register_xterr(XT_REG_CONTEXT, XT_ERR_RECORD_DELETED);
		return FAILED;
	}

	ot->ot_curr_row_id = XT_GET_DISK_4(((XTTabRecHeadDPtr) ot->ot_row_rbuffer)->tr_row_id_4);
	ot->ot_curr_updated =
		(XT_GET_DISK_4(((XTTabRecHeadDPtr) ot->ot_row_rbuffer)->tr_xact_id_4) == ot->ot_thread->st_xact_data->xd_start_xn_id);

	if (ot->ot_rec_fixed)
		memcpy(buffer, ot->ot_row_rbuffer + XT_REC_FIX_HEADER_SIZE, rec_size - XT_REC_FIX_HEADER_SIZE);
	else if (ot->ot_row_rbuffer[0] == XT_TAB_STATUS_VARIABLE || ot->ot_row_rbuffer[0] == XT_TAB_STATUS_VAR_CLEAN) {
		if (!myxt_load_row(ot, ot->ot_row_rbuffer + XT_REC_FIX_HEADER_SIZE, buffer, ot->ot_cols_req))
			return FAILED;
	}
	else {
		u_int cols_req = ot->ot_cols_req;

		ASSERT_NS(cols_req);
		if (cols_req && cols_req <= tab->tab_dic.dic_fix_col_count) {
			if (!myxt_load_row(ot, ot->ot_row_rbuffer + XT_REC_EXT_HEADER_SIZE, buffer, cols_req))
				return FAILED;
		}
		else {
			if (!xt_tab_load_ext_data(ot, ot->ot_curr_rec_id, buffer, cols_req))
				return FAILED;
		}
	}

	return OK;
}

#ifdef XT_USE_ROW_REC_MMAP_FILES
/* Loading into cache is not required,
 * Instead we copy the memory map to load the
 * data.
 */
#define TAB_ROW_LOAD_CACHE		FALSE
#else
#define TAB_ROW_LOAD_CACHE		TRUE
#endif

/*
 * Pull the entire row pointer file into memory.
 */
xtPublic void xt_tab_load_row_pointers(XTThreadPtr self, XTOpenTablePtr ot)
{
	XTTableHPtr	tab = ot->ot_table;
	xtRecordID	eof_rec_id = tab->tab_row_eof_id;
	xtInt8		usage;
	xtWord1		*buffer = NULL;

	/* Check if there is enough cache: */
	usage = xt_tc_get_usage();
	if (xt_tc_get_high() > usage)
		usage = xt_tc_get_high();
	if (usage + ((xtInt8) eof_rec_id * (xtInt8) tab->tab_rows.tci_rec_size) < xt_tc_get_size()) {
		xtRecordID			rec_id;
		size_t				poffset, tfer;
		off_t				offset, end_offset;
		XTTabCachePagePtr	page;
		
		end_offset = xt_row_id_to_row_offset(tab, eof_rec_id);
		rec_id = 1;
		while (rec_id < eof_rec_id) {
			if (!tab->tab_rows.xt_tc_get_page(ot->ot_row_file, rec_id, TAB_ROW_LOAD_CACHE, &page, &poffset, self))
				xt_throw(self);
			if (page)
				tab->tab_rows.xt_tc_release_page(ot->ot_row_file, page, self);
			else {
				xtWord1 *buff_ptr;

				if (!buffer)
					buffer = (xtWord1 *) xt_malloc(self, tab->tab_rows.tci_page_size);
				offset = xt_row_id_to_row_offset(tab, rec_id);
				tfer = tab->tab_rows.tci_page_size;
				if (offset + (off_t) tfer > end_offset)
					tfer = (size_t) (end_offset - offset);
				XT_LOCK_MEMORY_PTR(buff_ptr, ot->ot_row_file, offset, tfer, &self->st_statistics.st_rec, self);
				if (buff_ptr) {
					memcpy(buffer, buff_ptr, tfer);
					XT_UNLOCK_MEMORY_PTR(ot->ot_row_file, buff_ptr, TRUE, self);
				}
			}
			rec_id += tab->tab_rows.tci_rows_per_page;
		}
		if (buffer)
			xt_free(self, buffer);
	}
}

xtPublic void xt_tab_load_table(XTThreadPtr self, XTOpenTablePtr ot)
{
	xt_load_pages(self, ot);
	xt_load_indices(self, ot);
}

xtPublic xtBool xt_tab_load_record(register XTOpenTablePtr ot, xtRecordID rec_id, XTInfoBufferPtr rec_buf)
{
	register XTTableHPtr	tab = ot->ot_table;
	size_t					rec_size = tab->tab_dic.dic_rec_size;

	if (!xt_tab_get_rec_data(ot, rec_id, rec_size, ot->ot_row_rbuffer))
		return FAILED;

	if (XT_REC_NOT_VALID(ot->ot_row_rbuffer[0])) {
		/* Should not happen! */
		XTThreadPtr self = ot->ot_thread;

		xt_log(XT_WARNING, "Recently updated record invalid\n");
		return OK;
	}

	ot->ot_curr_row_id = XT_GET_DISK_4(((XTTabRecHeadDPtr) ot->ot_row_rbuffer)->tr_row_id_4);
	ot->ot_curr_updated =
		(XT_GET_DISK_4(((XTTabRecHeadDPtr) ot->ot_row_rbuffer)->tr_xact_id_4) == ot->ot_thread->st_xact_data->xd_start_xn_id);

	if (ot->ot_rec_fixed) {
		size_t size = rec_size - XT_REC_FIX_HEADER_SIZE;
		if (!xt_ib_alloc(NULL, rec_buf, size))
			return FAILED;
		memcpy(rec_buf->ib_db.db_data, ot->ot_row_rbuffer + XT_REC_FIX_HEADER_SIZE, size);
	}
	else {
		if (!xt_ib_alloc(NULL, rec_buf, tab->tab_dic.dic_mysql_buf_size))
			return FAILED;
		if (ot->ot_row_rbuffer[0] == XT_TAB_STATUS_VARIABLE || ot->ot_row_rbuffer[0] == XT_TAB_STATUS_VAR_CLEAN) {
			if (!myxt_load_row(ot, ot->ot_row_rbuffer + XT_REC_FIX_HEADER_SIZE, rec_buf->ib_db.db_data, ot->ot_cols_req))
				return FAILED;
		}
		else {
			u_int cols_req = ot->ot_cols_req;

			ASSERT_NS(cols_req);
			if (cols_req && cols_req <= tab->tab_dic.dic_fix_col_count) {
				if (!myxt_load_row(ot, ot->ot_row_rbuffer + XT_REC_EXT_HEADER_SIZE, rec_buf->ib_db.db_data, cols_req))
					return FAILED;
			}
			else {
				if (!xt_tab_load_ext_data(ot, ot->ot_curr_rec_id, rec_buf->ib_db.db_data, cols_req))
					return FAILED;
			}
		}
	}

	return OK;
}

xtPublic xtBool xt_tab_free_row(XTOpenTablePtr ot, XTTableHPtr tab, xtRowID row_id)
{
	XTTabRowRefDRec free_row;
	xtRowID			prev_row;
	xtOpSeqNo		op_seq;

	ASSERT_NS(row_id); // Cannot free the header!

	xt_lock_mutex_ns(&tab->tab_row_lock);
	prev_row = tab->tab_row_free_id;
	XT_SET_DISK_4(free_row.rr_ref_id_4, prev_row);
	if (!tab->tab_rows.xt_tc_write(ot->ot_row_file, row_id, 0, sizeof(XTTabRowRefDRec), (xtWord1 *) &free_row, &op_seq, TRUE, ot->ot_thread)) {
		xt_unlock_mutex_ns(&tab->tab_row_lock);
		return FAILED;
	}
	tab->tab_row_free_id = row_id;
	tab->tab_row_fnum++;
	xt_unlock_mutex_ns(&tab->tab_row_lock);

	if (!xt_xlog_modify_table(ot, XT_LOG_ENT_ROW_FREED, op_seq, 0, row_id, sizeof(XTTabRowRefDRec), (xtWord1 *) &free_row))
		return FAILED;

	return OK;
}

static void tab_free_ext_record_on_fail(XTOpenTablePtr ot, xtRecordID rec_id, XTTabRecExtDPtr ext_rec, xtBool log_err)
{
	xtWord4		log_over_size = XT_GET_DISK_4(ext_rec->re_log_dat_siz_4);
	xtLogID		log_id;
	xtLogOffset	log_offset;

	XT_GET_LOG_REF(log_id, log_offset, ext_rec);

	if (!ot->ot_thread->st_dlog_buf.dlb_delete_log(log_id, log_offset, log_over_size, ot->ot_table->tab_id, rec_id, ot->ot_thread)) {
		if (log_err)
			xt_log_and_clear_exception_ns();
	}
}

static void tab_save_exception(XTExceptionPtr e)
{
	XTThreadPtr self = xt_get_self();

	*e = self->t_exception;
}

static void tab_restore_exception(XTExceptionPtr e)
{
	XTThreadPtr self = xt_get_self();

	self->t_exception = *e;
}

/*
 * This function assumes that a record may be partially written.
 * It removes all associated data and references to the record.
 *
 * This function return XT_ERR if an error occurs.
 * TRUE if the record has been removed, and may be freed.
 * FALSE if the record has already been freed. 
 *
 */
xtPublic int xt_tab_remove_record(XTOpenTablePtr ot, xtRecordID rec_id, xtWord1 *rec_data, xtRecordID *prev_var_id, xtBool clean_delete, xtRowID row_id, xtXactID XT_UNUSED(xn_id))
{
	register XTTableHPtr	tab = ot->ot_table;
	size_t					rec_size;
	xtWord1					old_rec_type;
	u_int					cols_req;
	u_int					cols_in_buffer;

	*prev_var_id = 0;

	if (!rec_id)
		return FALSE;

	/*
	 * NOTE: This function uses the read buffer. This should be OK because
	 * the function is only called by the sweeper. The read buffer
	 * is REQUIRED because of the call to xt_tab_load_ext_data()!!!
	 */
	rec_size = tab->tab_dic.dic_rec_size;
	if (!xt_tab_get_rec_data(ot, rec_id, rec_size, ot->ot_row_rbuffer))
		return XT_ERR;
	old_rec_type = ot->ot_row_rbuffer[0];

	/* Check of the record has not already been freed: */
	if (XT_REC_IS_FREE(old_rec_type))
		return FALSE;

	/* This record must belong to the given row: */
	if (XT_GET_DISK_4(((XTTabRecExtDPtr) ot->ot_row_rbuffer)->tr_row_id_4) != row_id)
		return FALSE;

	/* The transaction ID of the record must be BEFORE or equal to the given
	 * transaction ID.
	 *
	 * No, this does not always hold. Because we wait for updates now,
	 * a "younger" transaction can update before an older
	 * transaction.
	 * Commit order determined the actual order in which the transactions
	 * should be replicated. This is determined by the log number of
	 * the commit record!
	if (db->db_xn_curr_id(xn_id, XT_GET_DISK_4(((XTTabRecExtDPtr) ot->ot_row_rbuffer)->tr_xact_id_4)))
		return FALSE;
	 */

	*prev_var_id = XT_GET_DISK_4(((XTTabRecExtDPtr) ot->ot_row_rbuffer)->tr_prev_rec_id_4);

	if (tab->tab_dic.dic_key_count) {
		XTIndexPtr	*ind;

		switch (old_rec_type) {
			case XT_TAB_STATUS_DELETE:
			case XT_TAB_STATUS_DEL_CLEAN:
				rec_size = sizeof(XTTabRecHeadDRec);
				goto set_removed;
			case XT_TAB_STATUS_FIXED:
			case XT_TAB_STATUS_FIX_CLEAN:
				/* We know that for a fixed length record, 
				 * dic_ind_rec_len <= dic_rec_size! */
				rec_size = (size_t) tab->tab_dic.dic_ind_rec_len + XT_REC_FIX_HEADER_SIZE;
				rec_data = ot->ot_row_rbuffer + XT_REC_FIX_HEADER_SIZE;
				break;
			case XT_TAB_STATUS_VARIABLE:
			case XT_TAB_STATUS_VAR_CLEAN:
				cols_req = tab->tab_dic.dic_ind_cols_req;

				cols_in_buffer = cols_req;
				rec_size = myxt_load_row_length(ot, rec_size - XT_REC_FIX_HEADER_SIZE, ot->ot_row_rbuffer + XT_REC_FIX_HEADER_SIZE, &cols_in_buffer);
				if (cols_in_buffer < cols_req)
					rec_size = tab->tab_dic.dic_rec_size;
				else 
					rec_size += XT_REC_FIX_HEADER_SIZE;
				if (!myxt_load_row(ot, ot->ot_row_rbuffer + XT_REC_FIX_HEADER_SIZE, rec_data, cols_req)) {
					xt_log_and_clear_exception_ns();
					goto set_removed;
				}
				break;
			case XT_TAB_STATUS_EXT_DLOG:
			case XT_TAB_STATUS_EXT_CLEAN:
				cols_req = tab->tab_dic.dic_ind_cols_req;

				ASSERT_NS(cols_req);
				cols_in_buffer = cols_req;
				rec_size = myxt_load_row_length(ot, rec_size - XT_REC_EXT_HEADER_SIZE, ot->ot_row_rbuffer + XT_REC_EXT_HEADER_SIZE, &cols_in_buffer);
				if (cols_in_buffer < cols_req) {
					rec_size = tab->tab_dic.dic_rec_size;
					if (!xt_tab_load_ext_data(ot, rec_id, rec_data, cols_req)) {
						/* This is actually quite possible after recovery, see [(3)] */
						if (ot->ot_thread->t_exception.e_xt_err != XT_ERR_BAD_EXT_RECORD &&
							ot->ot_thread->t_exception.e_xt_err != XT_ERR_DATA_LOG_NOT_FOUND)
							xt_log_and_clear_exception_ns();
						goto set_removed;
					}
				}
				else {
					/* All the records we require are in the buffer... */
					rec_size += XT_REC_EXT_HEADER_SIZE;
					if (!myxt_load_row(ot, ot->ot_row_rbuffer + XT_REC_EXT_HEADER_SIZE, rec_data, cols_req)) {
						xt_log_and_clear_exception_ns();
						goto set_removed;
					}
				}
				break;
			default:
				break;
		}

		/* Could this be the case?: This change may only be flushed after the
		 * operation below has been flushed to the log.
		 *
		 * No, remove records are never "undone". The sweeper will delete
		 * the record again if it does not land in the log.
		 *
		 * The fact that the index entries have already been removed is not
		 * a problem.
		 */
		if (!tab->tab_dic.dic_disable_index) {
			ind = tab->tab_dic.dic_keys;
			for (u_int i=0; i<tab->tab_dic.dic_key_count; i++, ind++) {
				if (!xt_idx_delete(ot, *ind, rec_id, rec_data))
					xt_log_and_clear_exception_ns();
			}
		}
	}
	else {
		/* No indices: */
		switch (old_rec_type) {
			case XT_TAB_STATUS_DELETE:
			case XT_TAB_STATUS_DEL_CLEAN:
				rec_size = XT_REC_FIX_HEADER_SIZE;
				break;
			case XT_TAB_STATUS_FIXED:
			case XT_TAB_STATUS_FIX_CLEAN:
			case XT_TAB_STATUS_VARIABLE:
			case XT_TAB_STATUS_VAR_CLEAN:
				rec_size = XT_REC_FIX_HEADER_SIZE;
				break;
			case XT_TAB_STATUS_EXT_DLOG:
			case XT_TAB_STATUS_EXT_CLEAN:
				rec_size = XT_REC_EXT_HEADER_SIZE;
				break;
		}
	}

	set_removed:
	if (XT_REC_IS_EXT_DLOG(old_rec_type)) {
		/* {LOCK-EXT-REC} Lock, and read again to make sure that the
		 * compactor does not change this record, while
		 * we are removing it! */
		xt_lock_mutex_ns(&tab->tab_db->db_co_ext_lock);
		if (!xt_tab_get_rec_data(ot, rec_id, XT_REC_EXT_HEADER_SIZE, ot->ot_row_rbuffer)) {
			xt_unlock_mutex_ns(&tab->tab_db->db_co_ext_lock);
			return FAILED;
		}
		xt_unlock_mutex_ns(&tab->tab_db->db_co_ext_lock);

	}

	xtOpSeqNo			op_seq;
	XTTabRecFreeDPtr	free_rec = (XTTabRecFreeDPtr) ot->ot_row_rbuffer;
	xtRecordID			prev_rec_id;

	/* A record is "clean" deleted if the record was
	 * XT_TAB_STATUS_DELETE which was comitted.
	 * This makes sure that the record will still invalidate
	 * following records in a row.
	 *
	 * Example:
	 *
	 * 1. INSERT A ROW, then DELETE it, assume the sweeper is delayed.
	 *
	 * We now have the sequence row X --> del rec A --> valid rec B.
	 *
	 * 2. A SELECT can still find B. Assume it now goes to check
	 *    if the record is valid, it reads row X, and gets A.
	 *
	 * 3. Now the sweeper gets control and removes X, A and B.
	 *    It frees A with the clean bit.
	 *
	 * 4. Now the SELECT gets control and reads A. Normally a freed record
	 *    would be ignored, and it would go onto B, which would then
	 *    be considered valid (note, even after the free, the next
	 *    pointer is not affected).
	 *
	 * However, because the clean bit has been set, it will stop at A
	 * and consider B invalid (which is the desired result).
	 *
	 * NOTE: We assume it is not possible for A to be allocated and refer
	 * to B, because B is freed before A. This means that B may refer to
	 * A after the next allocation.
	 */

	xtWord1 new_rec_type = XT_TAB_STATUS_FREED | (clean_delete ? XT_TAB_STATUS_CLEANED_BIT : 0);

	xt_lock_mutex_ns(&tab->tab_rec_lock);
	free_rec->rf_rec_type_1 = new_rec_type;
	prev_rec_id = tab->tab_rec_free_id;
	XT_SET_DISK_4(free_rec->rf_next_rec_id_4, prev_rec_id);
	if (!xt_tab_put_rec_data(ot, rec_id, sizeof(XTTabRecFreeDRec), ot->ot_row_rbuffer, &op_seq)) {
		xt_unlock_mutex_ns(&tab->tab_rec_lock);
		return FAILED;
	}
	tab->tab_rec_free_id = rec_id;
	ASSERT_NS(tab->tab_rec_free_id < tab->tab_rec_eof_id);
	tab->tab_rec_fnum++;
	xt_unlock_mutex_ns(&tab->tab_rec_lock);

	free_rec->rf_rec_type_1 = old_rec_type;
	return xt_xlog_modify_table(ot, XT_LOG_ENT_REC_REMOVED_BI, op_seq, (xtRecordID) new_rec_type, rec_id, rec_size, ot->ot_row_rbuffer);
}

static xtRowID tab_new_row(XTOpenTablePtr ot, XTTableHPtr tab)
{
	xtRowID			row_id;
	xtOpSeqNo		op_seq;
	xtRowID			next_row_id = 0;
	u_int			status;

	xt_lock_mutex_ns(&tab->tab_row_lock);
	if ((row_id = tab->tab_row_free_id)) {
		status = XT_LOG_ENT_ROW_NEW_FL;

		if (!tab->tab_rows.xt_tc_read_4(ot->ot_row_file, row_id, &next_row_id, ot->ot_thread)) {
			xt_unlock_mutex_ns(&tab->tab_row_lock);
			return 0;
		}
		tab->tab_row_free_id = next_row_id;
		tab->tab_row_fnum--;
	}
	else {
		status = XT_LOG_ENT_ROW_NEW;
		row_id = tab->tab_row_eof_id;
		if (row_id == 0xFFFFFFFF) {
			xt_unlock_mutex_ns(&tab->tab_row_lock);
			xt_register_xterr(XT_REG_CONTEXT, XT_ERR_MAX_ROW_COUNT);
			return 0;
		}
		if (((row_id - 1) % tab->tab_rows.tci_rows_per_page) == 0) {
			/* By fetching the page now, we avoid reading it later... */
			XTTabCachePagePtr	page;
			XTTabCacheSegPtr	seg;
			size_t				poffset;

			if (!tab->tab_rows.tc_fetch(ot->ot_row_file, row_id, &seg, &page, &poffset, FALSE, ot->ot_thread)) {
				xt_unlock_mutex_ns(&tab->tab_row_lock);
				return 0;
			}
			TAB_CAC_UNLOCK(&seg->tcs_lock, ot->ot_thread->t_id);
		}
		tab->tab_row_eof_id++;
	}
	op_seq = tab->tab_seq.ts_get_op_seq();
	xt_unlock_mutex_ns(&tab->tab_row_lock);

	if (!xt_xlog_modify_table(ot, status, op_seq, next_row_id, row_id, 0, NULL))
		return 0;

	XT_DISABLED_TRACE(("new row tx=%d row=%d\n", (int) ot->ot_thread->st_xact_data->xd_start_xn_id, (int) row_id));
	ASSERT_NS(row_id);
	return row_id;
}

xtPublic xtBool xt_tab_get_row(register XTOpenTablePtr ot, xtRowID row_id, xtRecordID *var_rec_id)
{
	register XTTableHPtr	tab = ot->ot_table;

	(void) ASSERT_NS(sizeof(XTTabRowRefDRec) == 4);

	if (!tab->tab_rows.xt_tc_read_4(ot->ot_row_file, row_id, var_rec_id, ot->ot_thread))
		return FAILED;
	return OK;
}

xtPublic xtBool xt_tab_set_row(XTOpenTablePtr ot, u_int status, xtRowID row_id, xtRecordID var_rec_id)
{
	register XTTableHPtr	tab = ot->ot_table;
	XTTabRowRefDRec			row_buf;
	xtOpSeqNo				op_seq;

	ASSERT_NS(var_rec_id < tab->tab_rec_eof_id);
	XT_SET_DISK_4(row_buf.rr_ref_id_4, var_rec_id);

	if (!tab->tab_rows.xt_tc_write(ot->ot_row_file, row_id, 0, sizeof(XTTabRowRefDRec), (xtWord1 *) &row_buf, &op_seq, TRUE, ot->ot_thread))
		return FAILED;

	return xt_xlog_modify_table(ot, status, op_seq, 0, row_id, sizeof(XTTabRowRefDRec), (xtWord1 *) &row_buf);
}

xtPublic xtBool xt_tab_free_record(XTOpenTablePtr ot, u_int status, xtRecordID rec_id, xtBool clean_delete)
{
	register XTTableHPtr	tab = ot->ot_table;
	XTTabRecHeadDRec		rec_head;
	XTactFreeRecEntryDRec	free_rec;
	xtRecordID				prev_rec_id;

	/* Don't free the record if it is already free! */
	if (!xt_tab_get_rec_data(ot, rec_id, sizeof(XTTabRecHeadDRec), (xtWord1 *) &rec_head))
		return FAILED;

	if (!XT_REC_IS_FREE(rec_head.tr_rec_type_1)) {
		xtOpSeqNo op_seq;

		/* This information will be used to determine if the resources of the record
		 * should be removed.
		 */
		free_rec.fr_stat_id_1 = rec_head.tr_stat_id_1;
		XT_COPY_DISK_4(free_rec.fr_xact_id_4, rec_head.tr_xact_id_4);

		/* A record is "clean" deleted if the record was
		 * XT_TAB_STATUS_DELETE which was comitted.
		 * This makes sure that the record will still invalidate
		 * following records in a row.
		 *
		 * Example:
		 *
		 * 1. INSERT A ROW, then DELETE it, assume the sweeper is delayed.
		 *
		 * We now have the sequence row X --> del rec A --> valid rec B.
		 *
		 * 2. A SELECT can still find B. Assume it now goes to check
		 *    if the record is valid, ti reads row X, and gets A.
		 *
		 * 3. Now the sweeper gets control and removes X, A and B.
		 *    It frees A with the clean bit.
		 *
		 * 4. Now the SELECT gets control and reads A. Normally a freed record
		 *    would be ignored, and it would go onto B, which would then
		 *    be considered valid (note, even after the free, the next
		 *    pointer is not affected).
		 *
		 * However, because the clean bit has been set, it will stop at A
		 * and consider B invalid (which is the desired result).
		 *
		 * NOTE: We assume it is not possible for A to be allocated and refer
		 * to B, because B is freed before A. This means that B may refer to
		 * A after the next allocation.
		 */

		(void) ASSERT_NS(sizeof(XTTabRecFreeDRec) == sizeof(XTactFreeRecEntryDRec) - offsetof(XTactFreeRecEntryDRec, fr_rec_type_1));
		free_rec.fr_rec_type_1 = XT_TAB_STATUS_FREED | (clean_delete ? XT_TAB_STATUS_CLEANED_BIT : 0);
		free_rec.fr_not_used_1 = 0;

		xt_lock_mutex_ns(&tab->tab_rec_lock);
		prev_rec_id = tab->tab_rec_free_id;
		XT_SET_DISK_4(free_rec.fr_next_rec_id_4, prev_rec_id);
		if (!xt_tab_put_rec_data(ot, rec_id, sizeof(XTTabRecFreeDRec), &free_rec.fr_rec_type_1, &op_seq)) {
			xt_unlock_mutex_ns(&tab->tab_rec_lock);
			return FAILED;
		}
		tab->tab_rec_free_id = rec_id;
		ASSERT_NS(tab->tab_rec_free_id < tab->tab_rec_eof_id);
		tab->tab_rec_fnum++;
		xt_unlock_mutex_ns(&tab->tab_rec_lock);

		if (!xt_xlog_modify_table(ot, status, op_seq, rec_id, rec_id, sizeof(XTactFreeRecEntryDRec) - offsetof(XTactFreeRecEntryDRec, fr_stat_id_1), &free_rec.fr_stat_id_1))
			return FAILED;
	}
	return OK;
}

static void tab_free_row_on_fail(XTOpenTablePtr ot, XTTableHPtr tab, xtRowID row_id)
{
	XTExceptionRec e;

	tab_save_exception(&e);
	xt_tab_free_row(ot, tab, row_id);
	tab_restore_exception(&e);
}

static xtBool tab_add_record(XTOpenTablePtr ot, XTTabRecInfoPtr rec_info, u_int status)
{
	register XTTableHPtr	tab = ot->ot_table;
	XTThreadPtr				thread = ot->ot_thread;
	xtRecordID				rec_id;
	xtLogID					log_id;
	xtLogOffset				log_offset;
	xtOpSeqNo				op_seq;
	xtRecordID				next_rec_id = 0;

	if (rec_info->ri_ext_rec) {
		/* Determine where the overflow will go... */
		if (!thread->st_dlog_buf.dlb_get_log_offset(&log_id, &log_offset, rec_info->ri_log_data_size + offsetof(XTactExtRecEntryDRec, er_data), ot->ot_thread))
			return FAILED;
		XT_SET_LOG_REF(rec_info->ri_ext_rec, log_id, log_offset);
	}

	/* Write the record to disk: */
	xt_lock_mutex_ns(&tab->tab_rec_lock);
	if ((rec_id = tab->tab_rec_free_id)) {
		XTTabRecFreeDRec free_block;

		ASSERT_NS(rec_id < tab->tab_rec_eof_id);
		if (!xt_tab_get_rec_data(ot, rec_id, sizeof(XTTabRecFreeDRec), (xtWord1 *) &free_block)) {
			xt_unlock_mutex_ns(&tab->tab_rec_lock);
			return FAILED;
		}
		next_rec_id = XT_GET_DISK_4(free_block.rf_next_rec_id_4);
		tab->tab_rec_free_id = next_rec_id;
			
		tab->tab_rec_fnum--;
		
		/* XT_LOG_ENT_UPDATE --> XT_LOG_ENT_UPDATE_FL */
		/* XT_LOG_ENT_INSERT --> XT_LOG_ENT_INSERT_FL */
		/* XT_LOG_ENT_DELETE --> XT_LOG_ENT_DELETE_FL */
		status += 2;

		if (!xt_tab_put_rec_data(ot, rec_id, rec_info->ri_rec_buf_size, (xtWord1 *) rec_info->ri_fix_rec_buf, &op_seq)) {
			xt_unlock_mutex_ns(&tab->tab_rec_lock);
			return FAILED;
		}
	}
	else {
		xtBool read;

		rec_id = tab->tab_rec_eof_id;
		tab->tab_rec_eof_id++;

		/* If we are writing to a new page (at the EOF)
		 * then we do not need to read the page from the
		 * file because it is new.
		 *
		 * Note that this only works because we are holding
		 * a lock on the record file.
		 */
		read = ((rec_id - 1) % tab->tab_recs.tci_rows_per_page) != 0;

		if (!tab->tab_recs.xt_tc_write(ot->ot_rec_file, rec_id, 0, rec_info->ri_rec_buf_size, (xtWord1 *) rec_info->ri_fix_rec_buf, &op_seq, read, ot->ot_thread)) {
			xt_unlock_mutex_ns(&tab->tab_rec_lock);
			return FAILED;
		}
	}
	xt_unlock_mutex_ns(&tab->tab_rec_lock);

	if (!xt_xlog_modify_table(ot, status, op_seq, next_rec_id, rec_id,  rec_info->ri_rec_buf_size, (xtWord1 *) rec_info->ri_fix_rec_buf))
		return FAILED;

	if (rec_info->ri_ext_rec) {
		/* Write the log buffer overflow: */		
		rec_info->ri_log_buf->er_status_1 = XT_LOG_ENT_EXT_REC_OK;
		XT_SET_DISK_4(rec_info->ri_log_buf->er_data_size_4, rec_info->ri_log_data_size);
		XT_SET_DISK_4(rec_info->ri_log_buf->er_tab_id_4, tab->tab_id);
		XT_SET_DISK_4(rec_info->ri_log_buf->er_rec_id_4, rec_id);
		if (!thread->st_dlog_buf.dlb_append_log(log_id, log_offset, offsetof(XTactExtRecEntryDRec, er_data) + rec_info->ri_log_data_size, (xtWord1 *) rec_info->ri_log_buf, ot->ot_thread)) {
			/* Failed to write the overflow, free the record allocated above: */
			return FAILED;
		}
	}

	XT_DISABLED_TRACE(("new rec tx=%d val=%d\n", (int) thread->st_xact_data->xd_start_xn_id, (int) rec_id));
	rec_info->ri_rec_id = rec_id;
	return OK;
}

static void tab_delete_record_on_fail(XTOpenTablePtr ot, xtRowID row_id, xtRecordID rec_id, XTTabRecHeadDPtr row_ptr, xtWord1 *rec_data, u_int key_count)
{
	XTExceptionRec	e;
	xtBool			log_err = TRUE;
	XTTabRecInfoRec	rec_info;

	tab_save_exception(&e);
	
	if (e.e_xt_err == XT_ERR_DUPLICATE_KEY || 
		e.e_xt_err == XT_ERR_DUPLICATE_FKEY) {
		/* If the error does not cause rollback, then we will ignore the
		 * error if an error occurs in the UNDO!
		 */
		log_err = FALSE;
		tab_restore_exception(&e);
	}
	if (key_count) {
		XTIndexPtr	*ind;

		ind = ot->ot_table->tab_dic.dic_keys;
		for (u_int i=0; i<key_count; i++, ind++) {
			if (!xt_idx_delete(ot, *ind, rec_id, rec_data)) {
				if (log_err)
					xt_log_and_clear_exception_ns();
			}
		}
	}

	if (row_ptr->tr_rec_type_1 == XT_TAB_STATUS_EXT_DLOG || row_ptr->tr_rec_type_1 == XT_TAB_STATUS_EXT_CLEAN)
		tab_free_ext_record_on_fail(ot, rec_id, (XTTabRecExtDPtr) row_ptr, log_err);

	rec_info.ri_fix_rec_buf = (XTTabRecFixDPtr) ot->ot_row_wbuffer;
	rec_info.ri_rec_buf_size = offsetof(XTTabRecFixDRec, rf_data);
	rec_info.ri_ext_rec = NULL;
	rec_info.ri_fix_rec_buf->tr_rec_type_1 = XT_TAB_STATUS_DELETE;
	rec_info.ri_fix_rec_buf->tr_stat_id_1 = 0;
	XT_SET_DISK_4(rec_info.ri_fix_rec_buf->tr_row_id_4, row_id);
	XT_SET_DISK_4(rec_info.ri_fix_rec_buf->tr_prev_rec_id_4, rec_id);
	XT_SET_DISK_4(rec_info.ri_fix_rec_buf->tr_xact_id_4, ot->ot_thread->st_xact_data->xd_start_xn_id);

	if (!tab_add_record(ot, &rec_info, XT_LOG_ENT_DELETE))
		goto failed;

	if (!xt_tab_set_row(ot, XT_LOG_ENT_ROW_ADD_REC, row_id, rec_info.ri_rec_id))
		goto failed;

	if (log_err)
		tab_restore_exception(&e);
	return;

	failed:
	if (log_err)
		xt_log_and_clear_exception_ns();
	else
		tab_restore_exception(&e);
}

/*
 * Wait until all the variations between the start of the chain, and
 * the given record have been rolled-back.
 * If any is committed, register a locked error, and return FAILED.
 */
static xtBool tab_wait_for_rollback(XTOpenTablePtr ot, xtRowID row_id, xtRecordID commit_rec_id)
{
	register XTTableHPtr	tab = ot->ot_table;
	xtRecordID				var_rec_id;
	XTTabRecHeadDRec		var_head;
	xtXactID				xn_id;
	xtRecordID				invalid_rec = 0;
	XTXactWaitRec			xw;

	retry:
	if (!xt_tab_get_row(ot, row_id, &var_rec_id))
		return FAILED;

	while (var_rec_id != commit_rec_id) {
		if (!var_rec_id)
			goto locked;
		if (!xt_tab_get_rec_data(ot, var_rec_id, sizeof(XTTabRecHeadDRec), (xtWord1 *) &var_head))
			return FAILED;
		if (XT_REC_IS_CLEAN(var_head.tr_rec_type_1))
			goto locked;
		if (XT_REC_IS_FREE(var_head.tr_rec_type_1))
			/* Should not happen: */
			goto record_invalid;
		xn_id = XT_GET_DISK_4(var_head.tr_xact_id_4);
		switch (xt_xn_status(ot, xn_id, var_rec_id)) {
			case XT_XN_VISIBLE:
			case XT_XN_NOT_VISIBLE:
				goto locked;
			case XT_XN_ABORTED:
				/* Ingore the record, it will be removed. */
				break;
			case XT_XN_MY_UPDATE:
				/* Should not happen: */
				goto locked;
			case XT_XN_OTHER_UPDATE:
				/* Wait for the transaction to commit or rollback: */
				XT_TAB_ROW_UNLOCK(&tab->tab_row_rwlock[row_id % XT_ROW_RWLOCKS], ot->ot_thread);
				xw.xw_xn_id = xn_id;
				if (!xt_xn_wait_for_xact(ot->ot_thread, &xw, NULL)) {
					XT_TAB_ROW_WRITE_LOCK(&tab->tab_row_rwlock[row_id % XT_ROW_RWLOCKS], ot->ot_thread);
					return FAILED;
				}
				XT_TAB_ROW_WRITE_LOCK(&tab->tab_row_rwlock[row_id % XT_ROW_RWLOCKS], ot->ot_thread);
				goto retry;
			case XT_XN_REREAD:
				goto record_invalid;
		}
		var_rec_id = XT_GET_DISK_4(var_head.tr_prev_rec_id_4);
	}
	return OK;

	locked:
	xt_register_xterr(XT_REG_CONTEXT, XT_ERR_RECORD_CHANGED);
	return FAILED;
	
	record_invalid:
	/* Prevent an infinite loop due to a bad record: */
	if (invalid_rec != var_rec_id) {
		var_rec_id = invalid_rec;
		goto retry;
	}
	/* The record is invalid, it will be "overwritten"... */
#ifdef XT_CRASH_DEBUG
	/* Should not happen! */
	xt_crash_me();
#endif
	return OK;
}

/* Check if a record may be visible:
 * Return TRUE of the record may be visible now.
 * Return XT_MAYBE if the record may be visible in the future (set out_xn_id).
 * Return FALSE of the record is not valid (freed or is a delete record).
 * Return XT_ERR if an error occurred.
 */
xtPublic int xt_tab_maybe_committed(XTOpenTablePtr ot, xtRecordID rec_id, xtXactID *out_xn_id, xtRowID *out_rowid, xtBool *out_updated)
{
	XTTabRecHeadDRec		rec_head;
	xtXactID				rec_xn_id = 0;
	xtBool					wait = FALSE;
	xtXactID				wait_xn_id = 0;
	xtRowID					row_id;
	xtRecordID				var_rec_id;
	xtXactID				xn_id;
	register XTTableHPtr	tab;
#ifdef TRACE_VARIATIONS_IN_DUP_CHECK
	char					t_buf[500];
	int						len;
	char					*t_type = "C";
#endif
	xtRecordID				invalid_rec = 0;

	reread:
	if (!xt_tab_get_rec_data(ot, rec_id, sizeof(XTTabRecHeadDRec), (xtWord1 *) &rec_head))
		return XT_ERR;

	if (XT_REC_NOT_VALID(rec_head.tr_rec_type_1))
		return FALSE;

	if (!XT_REC_IS_CLEAN(rec_head.tr_rec_type_1)) {
		rec_xn_id = XT_GET_DISK_4(rec_head.tr_xact_id_4);
		switch (xt_xn_status(ot, rec_xn_id, rec_id)) {
			case XT_XN_VISIBLE:
#ifdef TRACE_VARIATIONS_IN_DUP_CHECK
				t_type="V";
#endif
				break;
			case XT_XN_NOT_VISIBLE:
#ifdef TRACE_VARIATIONS_IN_DUP_CHECK
				t_type="NV";
#endif
				break;
			case XT_XN_ABORTED:
				return FALSE;
			case XT_XN_MY_UPDATE:
#ifdef TRACE_VARIATIONS_IN_DUP_CHECK
				t_type="My-Upd";
#endif
				break;
			case XT_XN_OTHER_UPDATE:
#ifdef TRACE_VARIATIONS_IN_DUP_CHECK
				t_type="Wait";
#endif
				wait = TRUE;
				wait_xn_id = rec_xn_id;
				break;
			case XT_XN_REREAD:
#ifdef TRACE_VARIATIONS_IN_DUP_CHECK
				t_type="Re-read";
#endif
				/* Avoid infinite loop: */
				if (invalid_rec == rec_id) {
					/* Should not happen! */
#ifdef XT_CRASH_DEBUG
					/* Generate a core dump! */
					xt_crash_me();
#endif
					return FALSE;
				}
				invalid_rec = rec_id;
				goto reread;
		}
	}

	/* Follow the variation chain until we come to this record.
	 * If it is not the first visible variation then
	 * it is not visible at all. If it in not found on the
	 * variation chain, it is also not visible.
	 */
	row_id = XT_GET_DISK_4(rec_head.tr_row_id_4);

	tab = ot->ot_table;
	XT_TAB_ROW_READ_LOCK(&tab->tab_row_rwlock[row_id % XT_ROW_RWLOCKS], ot->ot_thread);

	invalid_rec = 0;
	retry:
	if (!(xt_tab_get_row(ot, row_id, &var_rec_id)))
		goto failed;
#ifdef TRACE_VARIATIONS_IN_DUP_CHECK
	len = sprintf(t_buf, "dup row=%d", (int) row_id);
#endif
	while (var_rec_id != rec_id) {
		if (!var_rec_id)
			goto not_found;
#ifdef TRACE_VARIATIONS_IN_DUP_CHECK
		if (len <= 450)
			len += sprintf(t_buf+len, " -> %d", (int) var_rec_id);
#endif
		if (!xt_tab_get_rec_data(ot, var_rec_id, sizeof(XTTabRecHeadDRec), (xtWord1 *) &rec_head))
			goto failed;
		/* All clean records are visible, by all transactions: */
		if (XT_REC_IS_CLEAN(rec_head.tr_rec_type_1))
			goto not_found;

		if (XT_REC_IS_FREE(rec_head.tr_rec_type_1)) {
			/* Should not happen: */
			if (invalid_rec != var_rec_id) {
				var_rec_id = invalid_rec;
				goto retry;
			}
			/* Assume end of list. */
#ifdef XT_CRASH_DEBUG
			/* Should not happen! */
			xt_crash_me();
#endif
			goto not_found;
		}

		xn_id = XT_GET_DISK_4(rec_head.tr_xact_id_4);
		switch (xt_xn_status(ot, xn_id, var_rec_id)) {
			case XT_XN_VISIBLE:
			case XT_XN_NOT_VISIBLE:
				goto not_found;
			case XT_XN_ABORTED:
				/* Ingore the record, it will be removed. */
#ifdef TRACE_VARIATIONS_IN_DUP_CHECK
				if (len <= 450)
					len += sprintf(t_buf+len, "(T%d-A)", (int) xn_id);
#endif
				break;
			case XT_XN_MY_UPDATE:
				goto not_found;
			case XT_XN_OTHER_UPDATE:
#ifdef TRACE_VARIATIONS_IN_DUP_CHECK
				if (len <= 450)
					len += sprintf(t_buf+len, "(T%d-wait)", (int) xn_id);
#endif
				/* Wait for this update to commit or abort: */
				if (!wait) {
					wait = TRUE;
					wait_xn_id = xn_id;
				}
				break;
			case XT_XN_REREAD:
				if (invalid_rec != var_rec_id) {
					var_rec_id = invalid_rec;
					goto retry;
				}
				/* Assume end of list. */
#ifdef XT_CRASH_DEBUG
				/* Should not happen! */
				xt_crash_me();
#endif
				goto not_found;
		}
		var_rec_id = XT_GET_DISK_4(rec_head.tr_prev_rec_id_4);
	}
#ifdef TRACE_VARIATIONS_IN_DUP_CHECK
	if (len <= 450)
		sprintf(t_buf+len, " -> %d(T%d-%s)\n", (int) var_rec_id, (int) rec_xn_id, t_type);
	else
		sprintf(t_buf+len, " ...(T%d-%s)\n", (int) rec_xn_id, t_type);
#endif

	XT_TAB_ROW_UNLOCK(&tab->tab_row_rwlock[row_id % XT_ROW_RWLOCKS], ot->ot_thread);
	if (wait) {
		*out_xn_id = wait_xn_id;
		return XT_MAYBE;
	}
#ifdef TRACE_VARIATIONS_IN_DUP_CHECK
	xt_ttracef(thread, "%s", t_buf);
#endif
	if (out_rowid) {
		*out_rowid = row_id;
		*out_updated = (rec_xn_id == ot->ot_thread->st_xact_data->xd_start_xn_id);
	}
	return TRUE;

	not_found:
	XT_TAB_ROW_UNLOCK(&tab->tab_row_rwlock[row_id % XT_ROW_RWLOCKS], ot->ot_thread);
	return FALSE;

	failed:
	XT_TAB_ROW_UNLOCK(&tab->tab_row_rwlock[row_id % XT_ROW_RWLOCKS], ot->ot_thread);
	return XT_ERR;
}

xtPublic xtBool xt_tab_new_record(XTOpenTablePtr ot, xtWord1 *rec_buf)
{
	register XTTableHPtr	tab = ot->ot_table;
	register XTThreadPtr	self = ot->ot_thread;
	XTTabRecInfoRec			rec_info;
	xtRowID					row_id;
	u_int					idx_cnt = 0;
	XTIndexPtr				*ind;

	if (!myxt_store_row(ot, &rec_info, (char *) rec_buf))
		goto failed_0;

	/* Get a new row ID: */
	if (!(row_id = tab_new_row(ot, tab)))
		goto failed_0;

	rec_info.ri_fix_rec_buf->tr_stat_id_1 = self->st_update_id;
	XT_SET_DISK_4(rec_info.ri_fix_rec_buf->tr_row_id_4, row_id);
	XT_SET_DISK_4(rec_info.ri_fix_rec_buf->tr_prev_rec_id_4, 0);
	XT_SET_DISK_4(rec_info.ri_fix_rec_buf->tr_xact_id_4, self->st_xact_data->xd_start_xn_id);

	/* Note, it is important that this record is written BEFORE the row
	 * due to the problem distributed here [(5)]
	 */
	if (!tab_add_record(ot, &rec_info, XT_LOG_ENT_INSERT))
		goto failed_1;

#ifdef TRACE_VARIATIONS
	xt_ttracef(self, "insert: row=%d rec=%d T%d\n", (int) row_id, (int) rec_info.ri_rec_id, (int) self->st_xact_data->xd_start_xn_id);
#endif
	if (!xt_tab_set_row(ot, XT_LOG_ENT_ROW_ADD_REC, row_id, rec_info.ri_rec_id))
		goto failed_1;
	XT_DISABLED_TRACE(("set new tx=%d row=%d rec=%d\n", (int) self->st_xact_data->xd_start_xn_id, (int) row_id, (int) rec_info.ri_rec_id));

	/* Add the index references: */
	for (idx_cnt=0, ind=tab->tab_dic.dic_keys; idx_cnt<tab->tab_dic.dic_key_count; idx_cnt++, ind++) {
		if (!xt_idx_insert(ot, *ind, 0, rec_info.ri_rec_id, rec_buf, NULL, FALSE)) {
			ot->ot_err_index_no = (*ind)->mi_index_no;
			goto failed_2;
		}
	}

	/* Do the foreign key stuff: */
	if (ot->ot_table->tab_dic.dic_table->dt_fkeys.size() > 0) {
		if (!ot->ot_table->tab_dic.dic_table->insertRow(ot, rec_buf))
			goto failed_2;
	}

	self->st_statistics.st_row_insert++;
	return OK;	

	failed_2:
	/* Once the row has been inserted, it is to late to remove it!
	 * Now all we can do is delete it!
	 */
	tab_delete_record_on_fail(ot, row_id, rec_info.ri_rec_id, (XTTabRecHeadDPtr) rec_info.ri_fix_rec_buf, rec_buf, idx_cnt);
	goto failed_0;

	failed_1:
	tab_free_row_on_fail(ot, tab, row_id);

	failed_0:
	return FAILED;
}

/* We cannot remove a change we have made to a row while a transaction
 * is running, so we have to undo what we have done by
 * overwriting the record we just created with
 * the before image!
 */
static xtBool tab_overwrite_record_on_fail(XTOpenTablePtr ot, XTTabRecInfoPtr rec_info, xtWord1 *before_buf, xtWord1 *after_buf, u_int idx_cnt)
{
	register XTTableHPtr	tab = ot->ot_table;
	XTTabRecHeadDRec		prev_rec_head;
	u_int					i;
	XTIndexPtr				*ind;
	XTThreadPtr				thread = ot->ot_thread;
	xtLogID					log_id;
	xtLogOffset				log_offset;
	xtRecordID				rec_id = rec_info->ri_rec_id;

	/* Remove the new extended record: */
	if (rec_info->ri_ext_rec)
		tab_free_ext_record_on_fail(ot, rec_id, (XTTabRecExtDPtr) rec_info->ri_fix_rec_buf, TRUE);

	/* Undo index entries of the new record: */
	if (after_buf) {
		for (i=0, ind=tab->tab_dic.dic_keys; i<idx_cnt; i++, ind++) {
			if (!xt_idx_delete(ot, *ind, rec_id, after_buf))
				return FAILED;
		}
	}

	memcpy(&prev_rec_head, rec_info->ri_fix_rec_buf, sizeof(XTTabRecHeadDRec));

	if (!before_buf) {
		/* Can happen if the delete was called from some cascaded action.
		 * And this is better than a crash...
		 *
		 * TODO: to make sure the change will not be applied in case the 
		 * transaction will be commited, we'd need to add a log entry to 
		 * restore the record like it's done for top-level operation. In 
		 * order to do this we'd need to read the before-image of the 
		 * record before modifying it.
		 */
		if (!ot->ot_thread->t_exception.e_xt_err)
			xt_register_xterr(XT_REG_CONTEXT, XT_ERR_NO_BEFORE_IMAGE);
		return FAILED;
	}

	/* Restore the previous record! */
	if (!myxt_store_row(ot, rec_info, (char *) before_buf))
		return FAILED;

	memcpy(rec_info->ri_fix_rec_buf, &prev_rec_head, sizeof(XTTabRecHeadDRec));

	if (rec_info->ri_ext_rec) {
		/* Determine where the overflow will go... */
		if (!thread->st_dlog_buf.dlb_get_log_offset(&log_id, &log_offset, rec_info->ri_log_data_size + offsetof(XTactExtRecEntryDRec, er_data), ot->ot_thread))
			return FAILED;
		XT_SET_LOG_REF(rec_info->ri_ext_rec, log_id, log_offset);
	}

	if (!xt_tab_put_log_op_rec_data(ot, XT_LOG_ENT_REC_MODIFIED, 0, rec_id, rec_info->ri_rec_buf_size, (xtWord1 *) rec_info->ri_fix_rec_buf))
		return FAILED;

	if (rec_info->ri_ext_rec) {
		/* Write the log buffer overflow: */		
		rec_info->ri_log_buf->er_status_1 = XT_LOG_ENT_EXT_REC_OK;
		XT_SET_DISK_4(rec_info->ri_log_buf->er_data_size_4, rec_info->ri_log_data_size);
		XT_SET_DISK_4(rec_info->ri_log_buf->er_tab_id_4, tab->tab_id);
		XT_SET_DISK_4(rec_info->ri_log_buf->er_rec_id_4, rec_id);
		if (!thread->st_dlog_buf.dlb_append_log(log_id, log_offset, offsetof(XTactExtRecEntryDRec, er_data) + rec_info->ri_log_data_size, (xtWord1 *) rec_info->ri_log_buf, ot->ot_thread))
			return FAILED;
	}

	/* Put the index entries back: */
	for (idx_cnt=0, ind=tab->tab_dic.dic_keys; idx_cnt<tab->tab_dic.dic_key_count; idx_cnt++, ind++) {
		if (!xt_idx_insert(ot, *ind, 0, rec_id, before_buf, after_buf, TRUE))
			/* Incomplete restore, there will be a rollback... */
			return FAILED;
	}

	return OK;
}

/*
 * GOTCHA:
 * If a transaction updates the same record over again, we should update
 * in place. This prevents producing unnecessary variations!
 */
static xtBool tab_overwrite_record(XTOpenTablePtr ot, xtWord1 *before_buf, xtWord1 *after_buf)
{
	register XTTableHPtr	tab = ot->ot_table;
	xtRowID					row_id = ot->ot_curr_row_id;
	register XTThreadPtr	self = ot->ot_thread;
	xtRecordID				rec_id = ot->ot_curr_rec_id;
	XTTabRecExtDRec			prev_rec_head;
	XTTabRecInfoRec			rec_info;
	u_int					idx_cnt = 0, i;
	XTIndexPtr				*ind;
	xtLogID					log_id;
	xtLogOffset				log_offset;
	xtBool					prev_ext_rec;

	if (!myxt_store_row(ot, &rec_info, (char *) after_buf))
		goto failed_0;

	/* Read before we overwrite! */
	if (!xt_tab_get_rec_data(ot, rec_id, XT_REC_EXT_HEADER_SIZE, (xtWord1 *) &prev_rec_head))
		goto failed_0;

	prev_ext_rec = prev_rec_head.tr_rec_type_1 & XT_TAB_STATUS_EXT_DLOG;

	if (rec_info.ri_ext_rec) {
		/* Determine where the overflow will go... */
		if (!self->st_dlog_buf.dlb_get_log_offset(&log_id, &log_offset, offsetof(XTactExtRecEntryDRec, er_data) + rec_info.ri_log_data_size, ot->ot_thread))
			goto failed_0;
		XT_SET_LOG_REF(rec_info.ri_ext_rec, log_id, log_offset);
	}

	rec_info.ri_fix_rec_buf->tr_stat_id_1 = self->st_update_id;
	XT_SET_DISK_4(rec_info.ri_fix_rec_buf->tr_row_id_4, row_id);
	XT_COPY_DISK_4(rec_info.ri_fix_rec_buf->tr_prev_rec_id_4, prev_rec_head.tr_prev_rec_id_4);
	XT_SET_DISK_4(rec_info.ri_fix_rec_buf->tr_xact_id_4, self->st_xact_data->xd_start_xn_id);

	/* Remove the index references, that have changed: */
	for (idx_cnt=0, ind=tab->tab_dic.dic_keys; idx_cnt<tab->tab_dic.dic_key_count; idx_cnt++, ind++) {
		if (!xt_idx_delete(ot, *ind, rec_id, before_buf)) {
			goto failed_0;
		}
	}

#ifdef TRACE_VARIATIONS
	xt_ttracef(self, "overwrite: row=%d rec=%d T%d\n", (int) row_id, (int) rec_id, (int) self->st_xact_data->xd_start_xn_id);
#endif
	/* Overwrite the record: */
	if (!xt_tab_put_log_op_rec_data(ot, XT_LOG_ENT_REC_MODIFIED, 0, rec_id, rec_info.ri_rec_buf_size, (xtWord1 *) rec_info.ri_fix_rec_buf))
		goto failed_0;

	if (rec_info.ri_ext_rec) {
		/* Write the log buffer overflow: */		
		rec_info.ri_log_buf->er_status_1 = XT_LOG_ENT_EXT_REC_OK;
		XT_SET_DISK_4(rec_info.ri_log_buf->er_data_size_4, rec_info.ri_log_data_size);
		XT_SET_DISK_4(rec_info.ri_log_buf->er_tab_id_4, tab->tab_id);
		XT_SET_DISK_4(rec_info.ri_log_buf->er_rec_id_4, rec_id);
		if (!self->st_dlog_buf.dlb_append_log(log_id, log_offset, offsetof(XTactExtRecEntryDRec, er_data) + rec_info.ri_log_data_size, (xtWord1 *) rec_info.ri_log_buf, ot->ot_thread))
			goto failed_1;
	}

	/* Add the index references that have changed: */
	for (idx_cnt=0, ind=tab->tab_dic.dic_keys; idx_cnt<tab->tab_dic.dic_key_count; idx_cnt++, ind++) {
		if (!xt_idx_insert(ot, *ind, 0, rec_id, after_buf, before_buf, FALSE)) {
			ot->ot_err_index_no = (*ind)->mi_index_no;
			goto failed_2;
		}
	}

	/* Do the foreign key stuff: */
	if (ot->ot_table->tab_dic.dic_table->dt_trefs || ot->ot_table->tab_dic.dic_table->dt_fkeys.size() > 0) {
		if (!ot->ot_table->tab_dic.dic_table->updateRow(ot, before_buf, after_buf))
			goto failed_2;
	}
	
	/* Delete the previous overflow area: */
	if (prev_ext_rec)
		tab_free_ext_record_on_fail(ot, rec_id, &prev_rec_head, TRUE);

	return OK;

	failed_2:
	/* Remove the new extended record: */
	if (rec_info.ri_ext_rec)
		tab_free_ext_record_on_fail(ot, rec_id, (XTTabRecExtDPtr) rec_info.ri_fix_rec_buf, TRUE);

	/* Restore the previous record! */
	/* Undo index entries: */
	for (i=0, ind=tab->tab_dic.dic_keys; i<idx_cnt; i++, ind++) {
		if (!xt_idx_delete(ot, *ind, rec_id, after_buf))
			goto failed_1;
	}

	/* Restore the record: */
	if (!myxt_store_row(ot, &rec_info, (char *) before_buf))
		goto failed_1;

	if (rec_info.ri_ext_rec)
		memcpy(rec_info.ri_fix_rec_buf, &prev_rec_head, XT_REC_EXT_HEADER_SIZE);
	else
		memcpy(rec_info.ri_fix_rec_buf, &prev_rec_head, sizeof(XTTabRecHeadDRec));

	if (!xt_tab_put_log_op_rec_data(ot, XT_LOG_ENT_REC_MODIFIED, 0, rec_id, rec_info.ri_rec_buf_size, (xtWord1 *) rec_info.ri_fix_rec_buf))
		goto failed_1;

	/* Put the index entries back: */
	for (idx_cnt=0, ind=tab->tab_dic.dic_keys; idx_cnt<tab->tab_dic.dic_key_count; idx_cnt++, ind++) {
		if (!xt_idx_insert(ot, *ind, 0, rec_id, before_buf, after_buf, TRUE))
			/* Incomplete restore, there will be a rollback... */
			goto failed_0;
	}

	/* The previous record has now been restored. */
	goto failed_0;

	failed_1:
	/* The old record is overwritten, I must free the previous extended record: */
	if (prev_ext_rec)
		tab_free_ext_record_on_fail(ot, rec_id, &prev_rec_head, TRUE);

	failed_0:
	return FAILED;
}

xtPublic xtBool xt_tab_update_record(XTOpenTablePtr ot, xtWord1 *before_buf, xtWord1 *after_buf)
{
	register XTTableHPtr	tab;
	xtRowID					row_id;
	register XTThreadPtr	self;
	xtRecordID				curr_var_rec_id;
	XTTabRecInfoRec			rec_info;
	u_int					idx_cnt = 0;
	XTIndexPtr				*ind;

	/*
	 * Originally only the flag ot->ot_curr_updated was checked, and if it was on, then
	 * tab_overwrite_record() was called, but this caused crashes in some cases like:
	 *
	 * set @@autocommit = 0;
	 * create table t1 (s1 int primary key); 
	 * create table t2 (s1 int primary key, foreign key (s1) references t1 (s1) on update cascade);
     * insert into t1 values (1);
	 * insert into t2 values (1);
	 * update t1 set s1 = 1;
	 *
	 * the last update lead to a crash on t2 cascade update because before_buf argument is NULL 
	 * in the call below. It is NULL only during cascade update of child table. In that case we 
	 * cannot pass before_buf value from XTDDTableRef::modifyRow as the before_buf is the original 
	 * row for the parent (t1) table and it would be used to update any existing indexes
	 * in the child table which would be wrong of course.
	 *
	 * Alternative solution would be to copy the after_info in the XTDDTableRef::modifyRow():
	 * 
	 * ...
	 * if (!xt_tab_load_record(ot, ot->ot_curr_rec_id, &after_info))
	 *     goto failed_2;
	 * ...
	 *
	 * here the xt_tab_load_record() loads the original row, so we can copy it from there, but in 
	 * that case we'd need to allocate a new (possibly up to 65536 bytes long) buffer, which makes 
	 * the optimization questionable
	 *
	 */
	if (ot->ot_curr_updated && before_buf)
		/* This record has already been updated by this transaction.
		 * Do the update in place!
		 */
		return tab_overwrite_record(ot, before_buf, after_buf);

	tab = ot->ot_table;
	row_id = ot->ot_curr_row_id;
	self = ot->ot_thread;

	if (!myxt_store_row(ot, &rec_info, (char *) after_buf))
		goto failed_0;

	rec_info.ri_fix_rec_buf->tr_stat_id_1 = self->st_update_id;
	XT_SET_DISK_4(rec_info.ri_fix_rec_buf->tr_row_id_4, row_id);
	XT_SET_DISK_4(rec_info.ri_fix_rec_buf->tr_prev_rec_id_4, ot->ot_curr_rec_id);
	XT_SET_DISK_4(rec_info.ri_fix_rec_buf->tr_xact_id_4, self->st_xact_data->xd_start_xn_id);

	/* Create the new record: */
	if (!tab_add_record(ot, &rec_info, XT_LOG_ENT_UPDATE))
		goto failed_0;

	/* Link the new variation into the list: */
	XT_TAB_ROW_WRITE_LOCK(&tab->tab_row_rwlock[row_id % XT_ROW_RWLOCKS], ot->ot_thread);

	if (!xt_tab_get_row(ot, row_id, &curr_var_rec_id))
		goto failed_1;

	if (curr_var_rec_id != ot->ot_curr_rec_id) {
		/* If the transaction does not rollback, I will get an
		 * exception here:
		 */
		if (!tab_wait_for_rollback(ot, row_id, ot->ot_curr_rec_id))
			goto failed_1;
		/* [(4)] This is the situation when we overwrite the
		 * reference to curr_var_rec_id!
		 * When curr_var_rec_id is cleaned up by the sweeper, the
		 * sweeper will notice that the record is no longer in
		 * the row list.
		 */
	}

#ifdef TRACE_VARIATIONS
	xt_ttracef(self, "update: row=%d rec=%d T%d\n", (int) row_id, (int) rec_info.ri_rec_id, (int) self->st_xact_data->xd_start_xn_id);
#endif
	if (!xt_tab_set_row(ot, XT_LOG_ENT_ROW_ADD_REC, row_id, rec_info.ri_rec_id))
		goto failed_1;
	XT_DISABLED_TRACE(("set upd tx=%d row=%d rec=%d\n", (int) self->st_xact_data->xd_start_xn_id, (int) row_id, (int) rec_info.ri_rec_id));

	XT_TAB_ROW_UNLOCK(&tab->tab_row_rwlock[row_id % XT_ROW_RWLOCKS], ot->ot_thread);

	/* Add the index references: */
	for (idx_cnt=0, ind=tab->tab_dic.dic_keys; idx_cnt<tab->tab_dic.dic_key_count; idx_cnt++, ind++) {
		if (!xt_idx_insert(ot, *ind, 0, rec_info.ri_rec_id, after_buf, before_buf, FALSE)) {
			ot->ot_err_index_no = (*ind)->mi_index_no;
			goto failed_2;
		}
	}

	if (ot->ot_table->tab_dic.dic_table->dt_trefs || ot->ot_table->tab_dic.dic_table->dt_fkeys.size() > 0) {
		if (!ot->ot_table->tab_dic.dic_table->updateRow(ot, before_buf, after_buf))
			goto failed_2;
	}

	ot->ot_thread->st_statistics.st_row_update++;
	return OK;

	failed_2:
	tab_overwrite_record_on_fail(ot, &rec_info, before_buf, after_buf, idx_cnt);
	goto failed_0;

	failed_1:
	XT_TAB_ROW_UNLOCK(&tab->tab_row_rwlock[row_id % XT_ROW_RWLOCKS], ot->ot_thread);

	failed_0:
	return FAILED;
}

xtPublic xtBool xt_tab_delete_record(XTOpenTablePtr ot, xtWord1 *rec_buf)
{
	register XTTableHPtr	tab = ot->ot_table;
	xtRowID					row_id = ot->ot_curr_row_id;
	xtRecordID				curr_var_rec_id;
	XTTabRecInfoRec			rec_info;

	/* Setup a delete record: */
	rec_info.ri_fix_rec_buf = (XTTabRecFixDPtr) ot->ot_row_wbuffer;
	rec_info.ri_rec_buf_size = offsetof(XTTabRecFixDRec, rf_data);
	rec_info.ri_ext_rec = NULL;
	rec_info.ri_fix_rec_buf->tr_rec_type_1 = XT_TAB_STATUS_DELETE;
	rec_info.ri_fix_rec_buf->tr_stat_id_1 = 0;
	XT_SET_DISK_4(rec_info.ri_fix_rec_buf->tr_row_id_4, row_id);
	XT_SET_DISK_4(rec_info.ri_fix_rec_buf->tr_prev_rec_id_4, ot->ot_curr_rec_id);
	XT_SET_DISK_4(rec_info.ri_fix_rec_buf->tr_xact_id_4, ot->ot_thread->st_xact_data->xd_start_xn_id);

	if (!tab_add_record(ot, &rec_info, XT_LOG_ENT_DELETE))
		return FAILED;

	XT_TAB_ROW_WRITE_LOCK(&tab->tab_row_rwlock[row_id % XT_ROW_RWLOCKS], ot->ot_thread);

	if (!xt_tab_get_row(ot, row_id, &curr_var_rec_id))
		goto failed_1;

	if (curr_var_rec_id != ot->ot_curr_rec_id) {
		if (!tab_wait_for_rollback(ot, row_id, ot->ot_curr_rec_id))
			goto failed_1;		
	}

#ifdef TRACE_VARIATIONS
	xt_ttracef(ot->ot_thread, "update: row=%d rec=%d T%d\n", (int) row_id, (int) rec_info.ri_rec_id, (int) ot->ot_thread->st_xact_data->xd_start_xn_id);
#endif
	if (!xt_tab_set_row(ot, XT_LOG_ENT_ROW_ADD_REC, row_id, rec_info.ri_rec_id))
		goto failed_1;
	XT_DISABLED_TRACE(("del row tx=%d row=%d rec=%d\n", (int) ot->ot_thread->st_xact_data->xd_start_xn_id, (int) row_id, (int) rec_info.ri_rec_id));

	XT_TAB_ROW_UNLOCK(&tab->tab_row_rwlock[row_id % XT_ROW_RWLOCKS], ot->ot_thread);

	if (ot->ot_table->tab_dic.dic_table->dt_trefs) {
		if (!ot->ot_table->tab_dic.dic_table->deleteRow(ot, rec_buf))
			goto failed_2;
	}

	ot->ot_thread->st_statistics.st_row_delete++;
	return OK;

	failed_2:
	tab_overwrite_record_on_fail(ot, &rec_info, rec_buf, NULL, 0);
	return FAILED;

	failed_1:
	XT_TAB_ROW_UNLOCK(&tab->tab_row_rwlock[row_id % XT_ROW_RWLOCKS], ot->ot_thread);
	return FAILED;
}

xtPublic xtBool xt_tab_restrict_rows(XTBasicListPtr list, XTThreadPtr thread)
{
	u_int				i;
	XTRestrictItemPtr	item;
	XTOpenTablePtr		pot = NULL;
	XTDatabaseHPtr		db = thread->st_database;
	xtBool				ok = TRUE;

	for (i=0; i<list->bl_count; i++) {
		item = (XTRestrictItemPtr) xt_bl_item_at(list, i);
		if (item)
			if (pot) {
				if (pot->ot_table->tab_id == item->ri_tab_id)
					goto check_action;
				xt_db_return_table_to_pool_ns(pot);
				pot = NULL;
			}

			if (!xt_db_open_pool_table_ns(&pot, db, item->ri_tab_id)) {
				/* Should not happen, but just in case, we just don't
				 * remove the lock. We will probably end up with a deadlock
				 * somewhere.
				 */
				xt_log_and_clear_exception_ns();
				goto skip_check_action;
			}
			if (!pot)
				/* Can happen of the table has been dropped: */
				goto skip_check_action;

			check_action:
			if (!pot->ot_table->tab_dic.dic_table->checkNoAction(pot, item->ri_rec_id)) {
				ok = FALSE;
				break;
			}
			skip_check_action:;
	}

	if (pot)
		xt_db_return_table_to_pool_ns(pot);
	xt_bl_free(NULL, list);
	return ok;
}


xtPublic xtBool xt_tab_seq_init(XTOpenTablePtr ot)
{
	register XTTableHPtr tab = ot->ot_table;
	
	ot->ot_seq_page = NULL;
	ot->ot_seq_data = NULL;
	ot->ot_on_page = FALSE;
	ot->ot_seq_offset = 0;

	ot->ot_curr_rec_id = 0;			// 0 is an invalid position!
	ot->ot_curr_row_id = 0;			// 0 is an invalid row ID!
	ot->ot_curr_updated = FALSE;

	/* We note the current EOF before we start a sequential scan.
	 * It is basically possible to update the same record more than
	 * once because an updated record creates a new record which
	 * has a new position which may be in the area that is
	 * still to be scanned.
	 *
	 * By noting the EOF before we start a sequential scan we
	 * reduce the possibility of this.
	 *
	 * However, the possibility still remains, but it should
	 * not be a problem because a record is not modified
	 * if there is nothing to change, which is the case
	 * if the record has already been changed!
	 *
	 * NOTE (2008-01-29) There is no longer a problem with updating a
	 * record twice because records are marked by an update.
	 *
	 * [(10)] I have changed this (see below). I now check the
	 * current EOF of the table.
	 *
	 * The reason is that committed read must be able to see the
	 * changes that occur during table table scan.	 * 
	 */
	ot->ot_seq_eof_id = tab->tab_rec_eof_id;

	if (!ot->ot_thread->st_xact_data) {
		/* MySQL ignores this error, so we
		 * setup the sequential scan so that it will
		 * deliver nothing!
		 */
		ot->ot_seq_rec_id = ot->ot_seq_eof_id;
		xt_register_xterr(XT_REG_CONTEXT, XT_ERR_NO_TRANSACTION);
		return FAILED;
	}

	ot->ot_seq_rec_id = 1;
	ot->ot_thread->st_statistics.st_scan_table++;
	return OK;
}

xtPublic void xt_tab_seq_reset(XTOpenTablePtr ot)
{
	ot->ot_seq_rec_id = 0;
	ot->ot_seq_eof_id = 0;
	ot->ot_seq_page = NULL;
	ot->ot_seq_data = NULL;
	ot->ot_on_page = FALSE;
	ot->ot_seq_offset = 0;
}

xtPublic void xt_tab_seq_exit(XTOpenTablePtr ot)
{
	register XTTableHPtr	tab = ot->ot_table;

	if (ot->ot_seq_page) {
		tab->tab_recs.xt_tc_release_page(ot->ot_rec_file, ot->ot_seq_page, ot->ot_thread);
		ot->ot_seq_page = NULL;
	}
	if (ot->ot_seq_data)
		XT_UNLOCK_MEMORY_PTR(ot->ot_rec_file, ot->ot_seq_data, TRUE, ot->ot_thread);
	ot->ot_on_page = FALSE;
}

#ifdef XT_USE_ROW_REC_MMAP_FILES
#define TAB_SEQ_LOAD_CACHE		FALSE
#else
#ifdef XT_SEQ_SCAN_LOADS_CACHE
#define TAB_SEQ_LOAD_CACHE		TRUE
#else
#define TAB_SEQ_LOAD_CACHE		FALSE
#endif
#endif

xtPublic xtBool xt_tab_seq_next(XTOpenTablePtr ot, xtWord1 *buffer, xtBool *eof)
{
	register XTTableHPtr	tab = ot->ot_table;
	register size_t			rec_size = tab->tab_dic.dic_rec_size;
	xtWord1					*buff_ptr;
	xtRecordID				new_rec_id;
	xtRecordID				invalid_rec = 0;

	next_page:
	if (!ot->ot_on_page) {
		if (!(ot->ot_on_page = tab->tab_recs.xt_tc_get_page(ot->ot_rec_file, ot->ot_seq_rec_id, TAB_SEQ_LOAD_CACHE, &ot->ot_seq_page, &ot->ot_seq_offset, ot->ot_thread)))
			return FAILED;
		if (!ot->ot_seq_page) {
			XT_LOCK_MEMORY_PTR(ot->ot_seq_data, ot->ot_rec_file, xt_rec_id_to_rec_offset(tab, ot->ot_seq_rec_id), tab->tab_rows.tci_page_size, &ot->ot_thread->st_statistics.st_rec, ot->ot_thread);
			if (!ot->ot_seq_data)
				return FAILED;
			ot->ot_on_page = TRUE;
			ot->ot_seq_offset = 0;
		}
	}

	next_record:
	/* [(10)] The current EOF is used: */
	if (ot->ot_seq_rec_id >= ot->ot_seq_eof_id) {
		*eof = TRUE;
		return OK;
	}

	if (ot->ot_seq_offset >= tab->tab_recs.tci_page_size) {
		if (ot->ot_seq_page) {
			tab->tab_recs.xt_tc_release_page(ot->ot_rec_file, ot->ot_seq_page, ot->ot_thread);
			ot->ot_seq_page = NULL;
		}
		if (ot->ot_seq_data)
			/* NULL here means that in the case of non-memory mapped
			 * files we "keep" the lock.
			 */
			XT_UNLOCK_MEMORY_PTR(ot->ot_rec_file, ot->ot_seq_data, FALSE, ot->ot_thread);
		ot->ot_on_page = FALSE;
		goto next_page;
	}

	if (ot->ot_seq_page)
		buff_ptr = ot->ot_seq_page->tcp_data + ot->ot_seq_offset;
	else
		buff_ptr = ot->ot_seq_data + ot->ot_seq_offset;

	/* This is the current record: */
	ot->ot_curr_rec_id = ot->ot_seq_rec_id;
	ot->ot_curr_row_id = 0;

	/* Move to the next record: */
	ot->ot_seq_rec_id++;
	ot->ot_seq_offset += rec_size;

	retry:
	switch (tab_visible(ot, (XTTabRecHeadDPtr) buff_ptr, &new_rec_id)) {
		case FALSE:
			goto next_record;
		case XT_ERR:
			goto failed;
		case XT_NEW:
			buff_ptr = ot->ot_row_rbuffer;
			if (!xt_tab_get_rec_data(ot, new_rec_id, rec_size, ot->ot_row_rbuffer))
				return XT_ERR;
			ot->ot_curr_rec_id = new_rec_id;
			break;
		case XT_RETRY:
			goto retry;
		case XT_REREAD:
			if (invalid_rec != ot->ot_curr_rec_id) {
				/* Don't re-read for the same record twice: */
				invalid_rec = ot->ot_curr_rec_id;

				/* Undo move to next: */
				ot->ot_seq_rec_id--;
				ot->ot_seq_offset -= rec_size;
				
				/* Prepare to reread the page: */
				if (ot->ot_seq_page) {
					tab->tab_recs.xt_tc_release_page(ot->ot_rec_file, ot->ot_seq_page, ot->ot_thread);
					ot->ot_seq_page = NULL;
				}
				ot->ot_on_page = FALSE;
				goto next_page;
			}
#ifdef XT_CRASH_DEBUG
			/* Should not happen! */
			xt_crash_me();
#endif
			/* Continue, and skip the record... */
			invalid_rec = 0;
			goto next_record;
		default:
			break;
	}

	switch (*buff_ptr) {
		case XT_TAB_STATUS_FIXED:
		case XT_TAB_STATUS_FIX_CLEAN:
			memcpy(buffer, buff_ptr + XT_REC_FIX_HEADER_SIZE, rec_size - XT_REC_FIX_HEADER_SIZE);
			break;
		case XT_TAB_STATUS_VARIABLE:
		case XT_TAB_STATUS_VAR_CLEAN:
			if (!myxt_load_row(ot, buff_ptr + XT_REC_FIX_HEADER_SIZE, buffer, ot->ot_cols_req))
				goto failed_1;
			break;
		case XT_TAB_STATUS_EXT_DLOG:
		case XT_TAB_STATUS_EXT_CLEAN: {
			u_int cols_req = ot->ot_cols_req;

			ASSERT_NS(cols_req);
			if (cols_req && cols_req <= tab->tab_dic.dic_fix_col_count) {
				if (!myxt_load_row(ot, buff_ptr + XT_REC_EXT_HEADER_SIZE, buffer, cols_req))
					goto failed_1;
			}
			else {
				if (buff_ptr != ot->ot_row_rbuffer)
					memcpy(ot->ot_row_rbuffer, buff_ptr, rec_size);
				if (!xt_tab_load_ext_data(ot, ot->ot_curr_rec_id, buffer, cols_req))
					goto failed_1;
			}
			break;
		}
	}

	*eof = FALSE;
	return OK;

	failed_1:

	failed:
	return FAILED;
}

/*
 * -----------------------------------------------------------------------
 * REPAIR TABLE
 */

#define REP_FIND		0
#define REP_ADD			1
#define REP_DEL			2

static xtBool tab_exec_repair_pending(XTDatabaseHPtr db, int what, char *table_name)
{
	XTThreadPtr			thread = xt_get_self();
	char				file_path[PATH_MAX];
	XTOpenFilePtr		of = NULL;
	int					len;
	char				*buffer = NULL, *ptr, *name;
	char				ch;
	xtBool				found = FALSE;

	xt_strcpy(PATH_MAX, file_path, db->db_main_path);
	xt_add_pbxt_file(PATH_MAX, file_path, "repair-pending");
	
	if (what == REP_ADD) {
		if (!xt_open_file_ns(&of, file_path, XT_FS_CREATE | XT_FS_MAKE_PATH))
			return FALSE;
	}
	else {
		if (!xt_open_file_ns(&of, file_path, XT_FS_DEFAULT))
			return FALSE;
	}
	if (!of)
		return FALSE;

	len = (int) xt_seek_eof_file(NULL, of);
	
	if (!(buffer = (char *) xt_malloc_ns(len + 1)))
		goto failed;

	if (!xt_pread_file(of, 0, len, len, buffer, NULL, &thread->st_statistics.st_x, thread))
		goto failed;

	buffer[len] = 0;
	ptr = buffer;
	for(;;) {
		name = ptr;
		while (*ptr && *ptr != '\n' && *ptr != '\r')
			ptr++;
		if (ptr > name) {
			ch = *ptr;
			*ptr = 0;
			if (xt_tab_compare_names(name, table_name) == 0) {
				*ptr = ch;
				found = TRUE;
				break;
			}	
			*ptr = ch;
		}
		if (!*ptr)
			break;
		ptr++;
	}

	switch (what) {
		case REP_ADD:
			if (!found) {
				/* Remove any trailing empty lines: */
				while (len > 0) {
					if (buffer[len-1] != '\n' && buffer[len-1] != '\r')
						break;
					len--;
				}
				if (len > 0) {
					if (!xt_pwrite_file(of, len, 1, (void *) "\n", &thread->st_statistics.st_x, thread))
						goto failed;
					len++;
				}
				if (!xt_pwrite_file(of, len, strlen(table_name), table_name, &thread->st_statistics.st_x, thread))
					goto failed;
				len += strlen(table_name);
				if (!xt_set_eof_file(NULL, of, len))
					goto failed;
			}
			break;
		case REP_DEL:
			if (found) {
				if (*ptr != '\0')
					ptr++;
				memmove(name, ptr, len - (ptr - buffer));
				len = len - (ptr - name);

				/* Remove trailing empty lines: */
				while (len > 0) {
					if (buffer[len-1] != '\n' && buffer[len-1] != '\r')
						break;
					len--;
				}

				if (len > 0) {
					if (!xt_pwrite_file(of, 0, len, buffer, &thread->st_statistics.st_x, thread))
						goto failed;
					if (!xt_set_eof_file(NULL, of, len))
						goto failed;
				}
			}
			break;
	}

	xt_close_file_ns(of);
	xt_free_ns(buffer);

	if (len == 0)
		xt_fs_delete(NULL, file_path);
	return found;

	failed:
	if (of)
		xt_close_file_ns(of);
	if (buffer)
		xt_free_ns(buffer);
	xt_log_and_clear_exception(thread);
	return FALSE;
}

xtPublic void tab_make_table_name(XTTableHPtr tab, char *table_name, size_t size)
{
	char	name_buf[XT_IDENTIFIER_NAME_SIZE*3+3];

	xt_2nd_last_name_of_path(sizeof(name_buf), name_buf, tab->tab_name->ps_path);
	myxt_static_convert_file_name(name_buf, table_name, size);
	xt_strcat(size, table_name, ".");
	myxt_static_convert_file_name(xt_last_name_of_path(tab->tab_name->ps_path), name_buf, sizeof(name_buf));
	xt_strcat(size, table_name, name_buf);
}

xtPublic xtBool xt_tab_is_table_repair_pending(XTTableHPtr tab)
{
	char table_name[XT_IDENTIFIER_NAME_SIZE*3+3];

	tab_make_table_name(tab, table_name, sizeof(table_name));
	return tab_exec_repair_pending(tab->tab_db, REP_FIND, table_name);
}

xtPublic void xt_tab_table_repaired(XTTableHPtr tab)
{
	if (tab->tab_repair_pending) {
		char table_name[XT_IDENTIFIER_NAME_SIZE*3+3];

		tab->tab_repair_pending = FALSE;
		tab_make_table_name(tab, table_name, sizeof(table_name));
		tab_exec_repair_pending(tab->tab_db, REP_DEL, table_name);
	}
}

xtPublic void xt_tab_set_table_repair_pending(XTTableHPtr tab)
{
	if (!tab->tab_repair_pending) {
		char table_name[XT_IDENTIFIER_NAME_SIZE*3+3];

		tab->tab_repair_pending = TRUE;
		tab_make_table_name(tab, table_name, sizeof(table_name));
		tab_exec_repair_pending(tab->tab_db, REP_ADD, table_name);
	}
}
