/*****************************************************************************

Copyright (c) 2017, Oracle and/or its affiliates. All Rights Reserved.

Portions of this file contain modifications contributed and copyrighted by
Google, Inc. Those modifications are gratefully acknowledged and are described
briefly in the InnoDB documentation. The contributions by Google are
incorporated with their permission, and subject to the conditions contained in
the file COPYING.Google.

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
@file log/log0ddl.cc
DDL log

Created 12/1/2016 Shaohua Wang
*******************************************************/

#include "ha_prototypes.h"
#include <debug_sync.h>

//#include <sql_class.h>
//#include "thr_lock.h"
#include <current_thd.h>
//#include <dd/dictionary.h>
#include <sql_thd_internal_api.h>
//#include <mysql/service_thd_alloc.h>
//#include <mysql/service_thd_engine_lock.h>

#include "dict0mem.h"
#include "log0ddl.h"
#include "pars0pars.h"
#include "que0que.h"
#include "row0sel.h"
#include "trx0trx.h"
#include "ha_innodb.h"
#include "btr0sea.h"

LogDDL*	log_ddl = NULL;

/** Whether replaying ddl log
Note: we should not write ddl log when replaying ddl log. */
thread_local bool thread_local_ddl_log_replay = false;

/** Whether in recover(replay) ddl log in startup. */
bool LogDDL::in_recovery = false;

/** Constructor */
LogDDL::LogDDL()
{
	ut_ad(dict_sys->ddl_log != NULL);
	ut_ad(dict_table_has_autoinc_col(dict_sys->ddl_log));
}

/** Get next autoinc counter by increasing 1 for innodb_ddl_log
@return	new next counter */
inline
ib_uint64_t
LogDDL::getNextId()
{
	ib_uint64_t	autoinc;

	dict_table_autoinc_lock(dict_sys->ddl_log);
	autoinc = dict_table_autoinc_read(dict_sys->ddl_log);
	++autoinc;
	dict_table_autoinc_update_if_greater(
		dict_sys->ddl_log, autoinc);
	dict_table_autoinc_unlock(dict_sys->ddl_log);

	return(autoinc);
}

/** Check if we need to skip ddl log for a table.
@param[in]	table	dict table
@param[in]	thd	mysql thread
@return true if should skip, otherwise false */
inline bool
LogDDL::shouldSkip(
	const dict_table_t*	table,
	THD*			thd)
{
	/* Skip ddl log for temp tables and system tables. */
	/* Fixme: 1.skip when upgrade. 2. skip for intrinsic table? */
	if (recv_recovery_on || (table != nullptr && table->is_temporary())
	   || thd_is_bootstrap_thread(thd) || thread_local_ddl_log_replay) {
//	    || strstr(table->name.m_name, "mysql")
//	    || strstr(table->name.m_name, "sys")) {
		return(true);
	}

	return(false);
}

/** Write DDL log for freeing B-tree
@param[in,out]	trx		transaction
@param[in]	index		dict index
@param[in]	is_drop		flag whether dropping index
@return	DB_SUCCESS or error */
dberr_t
LogDDL::writeFreeTreeLog(
	trx_t*			trx,
	const dict_index_t*	index,
	bool			is_drop)
{
	/* Get dd transaction */
	trx = thd_to_trx(current_thd);

	if (shouldSkip(index->table, trx->mysql_thd)) {
		return(DB_SUCCESS);
	}

	if (index->type & DICT_FTS) {
		ut_ad(index->page == FIL_NULL);
		return(DB_SUCCESS);
	}

	ib_uint64_t	id = getNextId();
	ulint		thread_id = thd_get_thread_id(trx->mysql_thd);

	dberr_t	err;

	if (is_drop) {
		err = insertFreeTreeLog(trx, index, id, thread_id);
		ut_ad(err == DB_SUCCESS);
	} else {
		err = insertFreeTreeLog(nullptr, index, id, thread_id);
		ut_ad(err == DB_SUCCESS);

		err = deleteById(trx, id);
		ut_ad(err == DB_SUCCESS);
	}

	return(err);
}

/** Insert a FREE log record
@param[in,out]	trx		transaction
@param[in]	index		dict index
@param[in]	id		log id
@param[in]	thread_id	thread id
@return DB_SUCCESS or error */
dberr_t
LogDDL::insertFreeTreeLog(
	trx_t*			trx,
	const dict_index_t*	index,
	ib_uint64_t		id,
	ulint			thread_id)
{
	ut_ad(index->page != FIL_NULL);

	bool	has_dd_trx = (trx != nullptr);
	if (!has_dd_trx) {
		trx = trx_allocate_for_background();
	}

	pars_info_t*	info = pars_info_create();

	pars_info_add_ull_literal(info, "id", id);

	pars_info_add_ull_literal(info, "thread_id", thread_id);

	pars_info_add_int4_literal(info, "type", LogType::FREE_LOG);

	pars_info_add_int4_literal(info, "space_id", index->space);

	pars_info_add_int4_literal(info, "page_no", index->page);

	pars_info_add_ull_literal(info, "index_id", index->id);

	ut_ad(mutex_own(&dict_sys->mutex));

	mutex_exit(&dict_sys->mutex);

	dberr_t error = que_eval_sql(
			info,
			"PROCEDURE P () IS\n"
			"BEGIN\n"
			"INSERT INTO mysql/innodb_ddl_log VALUES"
			"(:id, :thread_id, :type, :space_id, :page_no,"
			":index_id, NULL, NULL, NULL);\n"
			"END;\n",
			TRUE, trx);

	mutex_enter(&dict_sys->mutex);

	if (!has_dd_trx) {
		trx_commit_for_mysql(trx);
		trx_free_for_background(trx);
	}

	ut_ad(error == DB_SUCCESS);

	ib::info() << "ddl log insert : " << "FREE "
		<< "id " << id << ", thread id " << thread_id
		<< ", space id " << index->space
		<< ", page_no " << index->page;

	return(error);
}

/** Write DDL log for deleting tablespace file
@param[in,out]	trx		transaction
@param[in]	table		dict table
@param[in]	space_id	tablespace id
@param[in]	file_path	file path
@param[in]	is_drop		flag whether dropping tablespace
@param[in]	dict_locked	true if dict_sys mutex is held
@return	DB_SUCCESS or error */
dberr_t
LogDDL::writeDeleteSpaceLog(
	trx_t*			trx,
	const dict_table_t*	table,
	space_id_t		space_id,
	const char*		file_path,
	bool			is_drop,
	bool			dict_locked)
{
	/* Get dd transaction */
	trx = thd_to_trx(current_thd);

	if (shouldSkip(table, trx->mysql_thd)) {
		return(DB_SUCCESS);
	}

#ifdef UNIV_DEBUG
	if (table != nullptr) {
		ut_ad(dict_table_is_file_per_table(table));
	}
#endif /* UNIV_DEBUG */

	ib_uint64_t	id = getNextId();
	ulint		thread_id = thd_get_thread_id(trx->mysql_thd);

	dberr_t	err;

	if (is_drop) {
		err = insertDeleteSpaceLog(
			trx, id, thread_id, space_id, file_path, dict_locked);
		ut_ad(err == DB_SUCCESS);
	} else {
		err = insertDeleteSpaceLog(
			nullptr, id, thread_id, space_id,
			file_path, dict_locked);
		ut_ad(err == DB_SUCCESS);

		err = deleteById(trx, id);
		ut_ad(err == DB_SUCCESS);
	}

	return(err);
}

/** Insert a DELETE log record
@param[in,out]	trx		transaction
@param[in]	id		log id
@param[in]	thread_id	thread id
@param[in]	space_id	tablespace id
@param[in]	file_path	file path
@param[in]	dict_locked	true if dict_sys mutex is held
@return DB_SUCCESS or error */
dberr_t
LogDDL::insertDeleteSpaceLog(
	trx_t*			trx,
	ib_uint64_t		id,
	ulint			thread_id,
	space_id_t		space_id,
	const char*		file_path,
	bool			dict_locked)
{
	bool	has_dd_trx = (trx != nullptr);
	if (!has_dd_trx) {
		trx = trx_allocate_for_background();
	}

	pars_info_t*	info = pars_info_create();

	pars_info_add_ull_literal(info, "id", id);

	pars_info_add_ull_literal(info, "thread_id", thread_id);

	pars_info_add_int4_literal(info, "type", LogType::DELETE_LOG);

	pars_info_add_int4_literal(info, "space_id", space_id);

	pars_info_add_str_literal(info, "old_file_path", file_path);

	if (dict_locked) {
		mutex_exit(&dict_sys->mutex);
	}

	dberr_t error = que_eval_sql(
			info,
			"PROCEDURE P () IS\n"
			"BEGIN\n"
			"INSERT INTO mysql/innodb_ddl_log VALUES"
			"(:id, :thread_id, :type, :space_id, NULL,"
			"NULL, NULL, :old_file_path, NULL);\n"
			"END;\n",
			TRUE, trx);

	if (dict_locked) {
		mutex_enter(&dict_sys->mutex);
	}

	ut_ad(error == DB_SUCCESS);

	if (!has_dd_trx) {
		trx_commit_for_mysql(trx);
		trx_free_for_background(trx);
	}

	ib::info() << "ddl log insert : " << "DELETE "
		<< "id " << id << ", thread id " << thread_id
		<< ", space id " << space_id << ", file path " << file_path;

	return(error);
}

/** Write a RENAME log record
@param[in]	id		log id
@param[in]	space_id	tablespace id
@param[in]	old_file_path	file path after rename
@param[in]	new_file_path	file path before rename
@return DB_SUCCESS or error */
dberr_t
LogDDL::writeRenameSpaceLog(
	trx_t*		trx,
	space_id_t	space_id,
	const char*	old_file_path,
	const char*	new_file_path)
{
	trx = thd_to_trx(current_thd);

	if (shouldSkip(NULL, trx->mysql_thd)) {
		return(DB_SUCCESS);
	}

	ib_uint64_t	id = getNextId();
	ulint		thread_id = thd_get_thread_id(trx->mysql_thd);

	dberr_t	err = insertRenameLog(id, thread_id, space_id,
				      old_file_path, new_file_path);
	ut_ad(err == DB_SUCCESS);

	err = deleteById(trx, id);
	ut_ad(err == DB_SUCCESS);

	return(err);
}

/** Insert a RENAME log record
@param[in]	id		log id
@param[in]	thread_id	thread id
@param[in]	space_id	tablespace id
@param[in]	old_file_path	file path after rename
@param[in]	new_file_path	file path before rename
@return DB_SUCCESS or error */
dberr_t
LogDDL::insertRenameLog(
	ib_uint64_t		id,
	ulint			thread_id,
	space_id_t		space_id,
	const char*		old_file_path,
	const char*		new_file_path)
{
	trx_t*	trx = trx_allocate_for_background();

	pars_info_t*	info = pars_info_create();

	pars_info_add_ull_literal(info, "id", id);

	pars_info_add_ull_literal(info, "thread_id", thread_id);

	pars_info_add_int4_literal(info, "type", LogType::RENAME_LOG);

	pars_info_add_int4_literal(info, "space_id", space_id);

	pars_info_add_str_literal(info, "old_file_path", old_file_path);

	pars_info_add_str_literal(info, "new_file_path", new_file_path);

	ut_ad(mutex_own(&dict_sys->mutex));

	mutex_exit(&dict_sys->mutex);

	dberr_t error = que_eval_sql(
			info,
			"PROCEDURE P () IS\n"
			"BEGIN\n"
			"INSERT INTO mysql/innodb_ddl_log VALUES"
			"(:id, :thread_id, :type, :space_id, NULL,"
			"NULL, NULL, :old_file_path, :new_file_path);\n"
			"END;\n",
			TRUE, trx);

	mutex_enter(&dict_sys->mutex);

	ut_ad(error == DB_SUCCESS);

	trx_commit_for_mysql(trx);
	trx_free_for_background(trx);

	ib::info() << "ddl log insert : " << "RENAME "
		<< "id " << id << ", thread id " << thread_id
		<< ", space id " << space_id << ", old_file path "
		<< old_file_path << ", new_file_path " << new_file_path;

	return(error);
}

/** Write a DROP log to indicate the entry in innodb_table_metadata
should be removed for specified table
@param[in,out]	trx		transaction
@param[in]	table_id	table ID
@return DB_SUCCESS or error */
dberr_t
LogDDL::writeDropLog(
	trx_t*			trx,
	const table_id_t	table_id)
{
	if (shouldSkip(NULL, trx->mysql_thd)) {
		return(DB_SUCCESS);
	}

	ib_uint64_t	id = getNextId();
	ulint		thread_id = thd_get_thread_id(trx->mysql_thd);

	dberr_t	err;
	err = insertDropLog(trx, id, thread_id, table_id);
	ut_ad(err == DB_SUCCESS);

	return(err);
}

/** Insert a DROP log record
@param[in,out]	trx		transaction
@param[in]	id		log id
@param[in]	thread_id	thread id
@param[in]	table_id	table id
@return DB_SUCCESS or error */
dberr_t
LogDDL::insertDropLog(
	trx_t*			trx,
	ib_uint64_t		id,
	ulint			thread_id,
	const table_id_t	table_id)
{
	pars_info_t*	info = pars_info_create();

	pars_info_add_ull_literal(info, "id", id);

	pars_info_add_ull_literal(info, "thread_id", thread_id);

	pars_info_add_int4_literal(info, "type", LogType::DROP_LOG);

	pars_info_add_ull_literal(info, "table_id", table_id);

	ut_ad(mutex_own(&dict_sys->mutex));

	mutex_exit(&dict_sys->mutex);

	dberr_t	error = que_eval_sql(
		info,
		"PROCEDURE P() IS\n"
		"BEGIN\n"
		"INSERT INTO mysql/innodb_ddl_log VALUES"
		"(:id, :thread_id, :type, NULL, NULL,"
		"NULL, :table_id, NULL, NULL);\n"
		"END;\n",
		TRUE, trx);

	mutex_enter(&dict_sys->mutex);

	ut_ad(error == DB_SUCCESS);

	ib::info() << "ddl log drop : " << "DROP "
		<< "id " << id << ", thread id " << thread_id
		<< ", table id " << table_id;

	return(error);
}

/** Write a RENAME TABLE log record
@param[in]	trx		transaction
@param[in]	table		dict table
@param[in]	old_name	table name after rename
@param[in]	new_name	table name before rename
@return DB_SUCCESS or error */
dberr_t
LogDDL::writeRenameTableLog(
	trx_t*		trx,
	dict_table_t*	table,
	const char*	old_name,
	const char*	new_name)
{
	trx = thd_to_trx(current_thd);

	if (shouldSkip(table, trx->mysql_thd)) {
		return(DB_SUCCESS);
	}

	ib_uint64_t	id = getNextId();
	ulint		thread_id = thd_get_thread_id(trx->mysql_thd);

	dberr_t	err = insertRenameTableLog(id, thread_id, table->id,
				      old_name, new_name);
	ut_ad(err == DB_SUCCESS);

	err = deleteById(trx, id);
	ut_ad(err == DB_SUCCESS);

	return(err);
}

/** Insert a RENAME TABLE log record
@param[in]	id		log id
@param[in]	thread_id	thread id
@param[in]	table_id	table id
@param[in]	old_name	table name after rename
@param[in]	new_name	table name before rename
@return DB_SUCCESS or error */
dberr_t
LogDDL::insertRenameTableLog(
	ib_uint64_t		id,
	ulint			thread_id,
	table_id_t		table_id,
	const char*		old_name,
	const char*		new_name)
{
	trx_t*	trx = trx_allocate_for_background();

	pars_info_t*	info = pars_info_create();

	pars_info_add_ull_literal(info, "id", id);

	pars_info_add_ull_literal(info, "thread_id", thread_id);

	pars_info_add_int4_literal(info, "type", LogType::RENAME_TABLE_LOG);

	pars_info_add_ull_literal(info, "table_id", table_id);

	pars_info_add_str_literal(info, "old_file_path", old_name);

	pars_info_add_str_literal(info, "new_file_path", new_name);

	ut_ad(mutex_own(&dict_sys->mutex));

	mutex_exit(&dict_sys->mutex);

	dberr_t error = que_eval_sql(
			info,
			"PROCEDURE P () IS\n"
			"BEGIN\n"
			"INSERT INTO mysql/innodb_ddl_log VALUES"
			"(:id, :thread_id, :type, NULL, NULL,"
			"NULL, :table_id, :old_file_path, :new_file_path);\n"
			"END;\n",
			TRUE, trx);

	mutex_enter(&dict_sys->mutex);

	ut_ad(error == DB_SUCCESS);

	trx_commit_for_mysql(trx);
	trx_free_for_background(trx);

	ib::info() << "ddl log insert : " << "RENAME TABLE "
		<< "id " << id << ", thread id " << thread_id
		<< ", table id " << table_id << ", old name "
		<< old_name << ", new_name " << new_name;

	return(error);
}

/** Write a REMOVE cache log record
@param[in]	trx		transaction
@param[in]	table		dict table
@return DB_SUCCESS or error */
dberr_t
LogDDL::writeRemoveCacheLog(
	trx_t*		trx,
	dict_table_t*	table)
{
	trx = thd_to_trx(current_thd);

	if (shouldSkip(table, trx->mysql_thd)) {
		return(DB_SUCCESS);
	}

	ib_uint64_t	id = getNextId();
	ulint		thread_id = thd_get_thread_id(trx->mysql_thd);

	dberr_t	err = insertRemoveCacheLog(id, thread_id, table->id,
				      table->name.m_name);
	ut_ad(err == DB_SUCCESS);

	err = deleteById(trx, id);
	ut_ad(err == DB_SUCCESS);

	return(err);
}

/** Insert a REMOVE cache log record
@param[in]	id		log id
@param[in]	thread_id	thread id
@param[in]	table_id	table id
@param[in]	table_name	table name
@return DB_SUCCESS or error */
dberr_t
LogDDL::insertRemoveCacheLog(
	ib_uint64_t		id,
	ulint			thread_id,
	table_id_t		table_id,
	const char*		table_name)
{
	trx_t*	trx = trx_allocate_for_background();

	pars_info_t*	info = pars_info_create();

	pars_info_add_ull_literal(info, "id", id);

	pars_info_add_ull_literal(info, "thread_id", thread_id);

	pars_info_add_int4_literal(info, "type", LogType::REMOVE_LOG);

	pars_info_add_ull_literal(info, "table_id", table_id);

	pars_info_add_str_literal(info, "new_file_path", table_name);

	ut_ad(mutex_own(&dict_sys->mutex));

	mutex_exit(&dict_sys->mutex);

	dberr_t error = que_eval_sql(
			info,
			"PROCEDURE P () IS\n"
			"BEGIN\n"
			"INSERT INTO mysql/innodb_ddl_log VALUES"
			"(:id, :thread_id, :type, NULL, NULL,"
			"NULL, :table_id, NULL, :new_file_path);\n"
			"END;\n",
			TRUE, trx);

	mutex_enter(&dict_sys->mutex);

	ut_ad(error == DB_SUCCESS);

	trx_commit_for_mysql(trx);
	trx_free_for_background(trx);

	ib::info() << "ddl log insert : " << "REMOVE "
		<< "id " << id << ", thread id " << thread_id
		<< ", table id " << table_id << ", table name "
		<< table_name;

	return(error);
}

/** Delete log record by id
@param[in]	trx	transaction instance
@param[in]	id	log id
@return DB_SUCCESS or error */
dberr_t
LogDDL::deleteById(
	trx_t*		trx,
	ib_uint64_t	id)
{
	ulint	org_isolation_level = trx->isolation_level;

	/* If read repeatable, we always gap-lock next record
	in row_sel(), which will block followup insertion. */
	trx->isolation_level = TRX_ISO_READ_COMMITTED;

	pars_info_t*	info = pars_info_create();

	pars_info_add_ull_literal(info, "id", id);

	ut_ad(mutex_own(&dict_sys->mutex));

	mutex_exit(&dict_sys->mutex);

	dberr_t error = que_eval_sql(
			info,
			"PROCEDURE P () IS\n"
			"BEGIN\n"
			"DELETE FROM mysql/innodb_ddl_log WHERE id=:id;\n"
			"END;\n",
			TRUE, trx);

	mutex_enter(&dict_sys->mutex);

	ut_ad(error == DB_SUCCESS);

	trx->isolation_level = org_isolation_level;

	ib::info() << "ddl log delete : " << "by id " << id;

	return(error);
}

/** Delete log records by thread id
@param[in]	trx		transaction instance
@param[in]	thread_id	thread id
@return DB_SUCCESS or error */
dberr_t
LogDDL::deleteByThreadId(
	trx_t*		trx,
	ulint		thread_id)
{
	pars_info_t*	info = pars_info_create();

	pars_info_add_ull_literal(info, "thread_id", thread_id);

	dberr_t error = que_eval_sql(
			info,
			"PROCEDURE P () IS\n"
			"BEGIN\n"
			"DELETE FROM mysql/innodb_ddl_log WHERE thread_id=:thread_id;\n"
			"END;\n",
			TRUE, trx);

	ut_ad(error == DB_SUCCESS || error == DB_LOCK_WAIT_TIMEOUT);

	ib::info() << "ddl log delete : " << "by thread id " << thread_id;

	return(error);
}

/** Delete all log records
@param[in]	trx	transaction instance
@return DB_SUCCESS or error */
dberr_t
LogDDL::deleteAll(
	trx_t*		trx)
{
	/* TODO: use truncate? */
	pars_info_t*    info = pars_info_create();

	dberr_t error = que_eval_sql(
			info,
			"PROCEDURE P () IS\n"
			"BEGIN\n"
			"DELETE FROM mysql/innodb_ddl_log;\n"
			"END;\n",
			TRUE, trx);

	ut_ad(error == DB_SUCCESS);

	return(error);
}

/** Scan and replay all log records
@param[in]	trx		transaction instance
@return DB_SUCCESS or error */
dberr_t
LogDDL::scanAll(
	trx_t*		trx)
{
	pars_info_t*	info = pars_info_create();

	pars_info_bind_function(info, "my_func", readAndReplay, NULL);

	dberr_t error = que_eval_sql(
			info,
			"PROCEDURE P() IS\n"
			"DECLARE FUNCTION my_func;\n"
			"DECLARE CURSOR c IS"
			" SELECT id, type, thread_id, space_id, page_no,"
			" index_id, table_id, old_file_path, new_file_path\n"
			" FROM mysql/innodb_ddl_log\n"
			" ORDER BY id;\n"
			"BEGIN\n"
			"\n"
			"OPEN c;\n"
			"WHILE 1 = 1 LOOP\n"
			"  FETCH c INTO my_func();\n"
			"  IF c % NOTFOUND THEN\n"
			"    EXIT;\n"
			"  END IF;\n"
			"END LOOP;\n"
			"CLOSE c;\n"
			"END;\n",
			TRUE, trx);

	ut_ad(error == DB_SUCCESS);

	return(DB_SUCCESS);
}

/** Scan and print all log records
@param[in]	trx		transaction instance
@return DB_SUCCESS or error */
dberr_t
LogDDL::printAll()
{
	trx_t*		trx = trx_allocate_for_background();

	trx->isolation_level = TRX_ISO_READ_UNCOMMITTED;

	pars_info_t*	info = pars_info_create();

	pars_info_bind_function(info, "my_func", printRecord, NULL);

	dberr_t error = que_eval_sql(
			info,
			"PROCEDURE P() IS\n"
			"DECLARE FUNCTION my_func;\n"
			"DECLARE CURSOR c IS"
			" SELECT id, type, thread_id, space_id, page_no,"
			" index_id, table_id, old_file_path, new_file_path\n"
			" FROM mysql/innodb_ddl_log\n"
			" ORDER BY id;\n"
			"BEGIN\n"
			"\n"
			"OPEN c;\n"
			"WHILE 1 = 1 LOOP\n"
			"  FETCH c INTO my_func();\n"
			"  IF c % NOTFOUND THEN\n"
			"    EXIT;\n"
			"  END IF;\n"
			"END LOOP;\n"
			"CLOSE c;\n"
			"END;\n",
			TRUE, trx);

	ut_ad(error == DB_SUCCESS);

	trx_commit_for_mysql(trx);

        trx_free_for_background(trx);

	return(DB_SUCCESS);
}

/** Scan and replay log records by thread id
@param[in]	trx		transaction instance
@param[in]	thread_id	thread id
@return DB_SUCCESS or error */
dberr_t
LogDDL::scanByThreadId(
	trx_t*		trx,
	ulint		thread_id)
{
	pars_info_t*	info = pars_info_create();

	pars_info_add_ull_literal(info, "thread_id", thread_id);
	pars_info_bind_function(info, "my_func", readAndReplay, NULL);

	dberr_t error = que_eval_sql(
			info,
			"PROCEDURE P() IS\n"
			"DECLARE FUNCTION my_func;\n"
			"DECLARE CURSOR c IS"
			" SELECT id, type, thread_id, space_id, page_no,"
			" index_id, table_id, old_file_path, new_file_path\n"
			" FROM mysql/innodb_ddl_log\n"
			" WHERE thread_id = :thread_id\n"
			" ORDER BY id DESC;\n"
			"BEGIN\n"
			"\n"
			"OPEN c;\n"
			"WHILE 1 = 1 LOOP\n"
			"  FETCH c INTO my_func();\n"
			"  IF c % NOTFOUND THEN\n"
			"    EXIT;\n"
			"  END IF;\n"
			"END LOOP;\n"
			"CLOSE c;\n"
			"END;\n",
			TRUE, trx);

	ut_ad(error == DB_SUCCESS);

	return(DB_SUCCESS);
}

/** Read and replay DDL log record
@param[in]	row	DDL log row
@param[in]	arg	argument passed down
@return TRUE on success, FALSE on failure */
ibool
LogDDL::readAndReplay(
	void*		row,
	void*		arg)
{
	sel_node_t*	sel_node = static_cast<sel_node_t*>(row);
	que_node_t*	exp = sel_node->select_list;
	mem_heap_t*	heap = mem_heap_create(512);
	Record		record;

	read(exp, record, heap);

	ib::info() << "ddl log read : id " << record.id
		<< ", type " << record.type
		<< ", thread_id " << record.thread_id
		<< ", space_id " << record.space_id
		<<", page_no " << record.page_no << ", index_id "
		<< record.index_id << ", table_id " << record.table_id
		<< ", old_file_path "
		<< (record.old_file_path == nullptr ? "(null)"
		    : record.old_file_path)
		<< ", new_file_path "
		<< (record.new_file_path == nullptr ? "(null)"
		    : record.new_file_path);

	replay(record);

	mem_heap_free(heap);

	return(TRUE);
}

/** Read and replay DDL log record
@param[in]	row	DDL log row
@param[in]	arg	argument passed down
@return TRUE on success, FALSE on failure */
ibool
LogDDL::printRecord(
	void*		row,
	void*		arg)
{
	sel_node_t*	sel_node = static_cast<sel_node_t*>(row);
	que_node_t*	exp = sel_node->select_list;
	mem_heap_t*	heap = mem_heap_create(512);
	Record		record;

	read(exp, record, heap);

	ib::info() << "ddl log read : id " << record.id
		<< ", type " << record.type
		<< ", thread_id " << record.thread_id
		<< ", space_id " << record.space_id
		<<", page_no " << record.page_no << ", index_id "
		<< record.index_id << ", table_id " << record.table_id
		<< ", old_file_path "
		<< (record.old_file_path == nullptr ? "(null)"
		    : record.old_file_path)
		<< ", new_file_path "
		<< (record.new_file_path == nullptr ? "(null)"
		    : record.new_file_path);

	mem_heap_free(heap);

	return(TRUE);
}

/** Read DDL log record
@param[in]	exp	query node expression
@param[in,out]	record	DDL log record
@param[in,out]	heap	mem heap
@return DB_SUCCESS or error */
dberr_t
LogDDL::read(
	que_node_t*		exp,
	Record&			record,
	mem_heap_t*		heap)
{
	/* Initialize */
	record.id = ULINT_UNDEFINED;
	record.thread_id = ULINT_UNDEFINED;
	record.space_id = ULINT32_UNDEFINED;
	record.page_no = FIL_NULL;
	record.index_id = ULINT_UNDEFINED;
	record.table_id = ULINT_UNDEFINED;
	record.old_file_path = nullptr;
	record.new_file_path = nullptr;

	/* Read values */
	ulint	i;
	for (i = 0; exp != nullptr; exp = que_node_get_next(exp), ++i) {

		dfield_t*	dfield = que_node_get_val(exp);
		byte*		data = static_cast<byte*>(
					dfield_get_data(dfield));
		ulint		len = dfield_get_len(dfield);

		/* Note: The column numbers below must match the SELECT. */
		switch (i) {
		case 0: /* ID */
			ut_a(len != UNIV_SQL_NULL);
			record.id = mach_read_from_8(data);
			break;

		case 1: /* TYPE */
			ut_a(len != UNIV_SQL_NULL);
			record.type = mach_read_from_4(data);
			break;

		case 2: /* THREAD_ID */
			ut_a(len != UNIV_SQL_NULL);
			record.thread_id = mach_read_from_8(data);
			break;

		case 3: /* TABLESPACE_ID */
			if (len != UNIV_SQL_NULL) {
				record.space_id = mach_read_from_4(data);
			}
			break;

		case 4: /* PAGE_NO */
			if (len != UNIV_SQL_NULL) {
				record.page_no = mach_read_from_4(data);
			}
			break;
		case 5: /* INDEX_ID */
			if (len != UNIV_SQL_NULL) {
				record.index_id = mach_read_from_8(data);
			}
			break;
		case 6: /* TABLE_ID */
			if (len != UNIV_SQL_NULL) {
				record.table_id = mach_read_from_8(data);
			}
			break;
		case 7: /* OLD_FILE_PATH */
			if (len != UNIV_SQL_NULL && len != 0) {
				record.old_file_path = static_cast<char*>(
					mem_heap_alloc(heap, len + 1));
				memcpy(record.old_file_path, data, len);
				record.old_file_path[len] = '\0';
			}
			break;
		case 8: /* NEW_FILE_PATH */
			if (len != UNIV_SQL_NULL && len != 0) {
				record.new_file_path = static_cast<char*>(
					mem_heap_alloc(heap, len + 1));
				memcpy(record.new_file_path, data, len);
				record.new_file_path[len] = '\0';
			}
			break;
		default:
			ut_error;
		}
	}

	ut_a(i == 9);

	return(DB_SUCCESS);
}

/** Replay DDL log record
@param[in,out]	record	DDL log record
return DB_SUCCESS or error */
dberr_t
LogDDL::replay(
//	trx_t*		trx,
	Record&		record)
{
	dberr_t		err = DB_SUCCESS;

	switch(record.type) {
	case	LogType::FREE_LOG:
		replayFreeLog(
			record.space_id,
			record.page_no,
			record.index_id);
		break;

	case	LogType::DELETE_LOG:
		replayDeleteLog(
			record.space_id,
			record.old_file_path);
		break;

	case	LogType::RENAME_LOG:
		replayRenameLog(
			record.space_id,
			record.old_file_path,
			record.new_file_path);
		break;

	case	LogType::DROP_LOG:
		replayDropLog(record.table_id);
		break;

	case	LogType::RENAME_TABLE_LOG:
		replayRenameTableLog(
			record.table_id,
			record.old_file_path,
			record.new_file_path);
		break;

	case	LogType::REMOVE_LOG:
		replayRemoveLog(
			record.table_id,
			record.new_file_path);
		break;

	default:
		ut_error;
	}

	return(err);
}

/** Replay FREE log(free B-tree if exist)
@param[in]	space_id	tablespace id
@param[in]	page_no		root page no
@param[in]	index_id	index id */
void
LogDDL::replayFreeLog(
	space_id_t	space_id,
	page_no_t	page_no,
	ulint		index_id)
{
	ut_ad(space_id != ULINT32_UNDEFINED);
	ut_ad(page_no != FIL_NULL);

	bool			found;
	const page_size_t	page_size(fil_space_get_page_size(space_id,
								  &found));

	if (!found) {
		/* It is a single table tablespace and the .ibd file is
		missing: do nothing */

		ib::info() << "ddl log replay : FREE tablespace " << space_id
			<< " is missing.";
		return;
	}

	/* This is required by dropping hash index afterwards. */
	mutex_enter(&dict_sys->mutex);

	mtr_t	mtr;
	mtr_start(&mtr);

	btr_free_if_exists(page_id_t(space_id, page_no),
			   page_size, index_id, &mtr);

	ib::info() << "ddl log replay : FREE space_id " << space_id
		<< ", page_no " << page_no << ", index_id " << index_id;

	mtr_commit(&mtr);

	mutex_exit(&dict_sys->mutex);

	return;	
}

/** Replay DELETE log(delete file if exist)
@param[in]	space_id	tablespace id
@param[in]	file_path	file path */
void
LogDDL::replayDeleteLog(
	space_id_t	space_id,
	const char*	file_path)
{
	row_drop_single_table_tablespace(space_id, NULL, file_path);

	ib::info() << "ddl log replay : DELETE space_id " << space_id
		<< ", file_path " << file_path;
}

/** Relay RENAME log
@param[in]	space_id	tablespace id
@param[in]	old_file_path	old file path
@param[in]	new_file_path	new file path */
void
LogDDL::replayRenameLog(
	space_id_t	space_id,
	const char*	old_file_path,
	const char*	new_file_path)
{
	bool		ret;
	page_id_t	page_id(space_id, 0);

	ret = fil_op_replay_rename(page_id, old_file_path, new_file_path);
	if (!ret) {
		ib::info() << "ddl log replay : RENAME failed";
	}

	ib::info() << "ddl log replay : RENAME space_id " << space_id
		<< ", old_file_path " << old_file_path << ", new_file_path "
		<< new_file_path;
}

/** Replay DROP log
@param[in]	table_id	table id */
void
LogDDL::replayDropLog(
	const table_id_t	table_id)
{
	mutex_enter(&dict_persist->mutex);
	ut_d(dberr_t	error =)
	dict_persist->table_buffer->remove(table_id);
	ut_ad(error == DB_SUCCESS);
	mutex_exit(&dict_persist->mutex);
}

/** Relay RENAME TABLE log
@param[in]	table_id	table id
@param[in]	old_name	old name
@param[in]	new_name	new name */
void
LogDDL::replayRenameTableLog(
	table_id_t	table_id,
	const char*	old_name,
	const char*	new_name)
{
	if (in_recovery) {
		return;
	}

	trx_t*	trx;
	trx = trx_allocate_for_background();
	trx->mysql_thd = current_thd;
	trx_start_if_not_started(trx, true);

	row_mysql_lock_data_dictionary(trx);
	trx_set_dict_operation(trx, TRX_DICT_OP_TABLE);

	dberr_t	err;
	err = row_rename_table_for_mysql(old_name, new_name, NULL, trx, false);

	row_mysql_unlock_data_dictionary(trx);

	trx_commit_for_mysql(trx);
	trx_free_for_background(trx);

	if (err != DB_SUCCESS) {
		ib::info() << "ddl log replay rename table in cache from "
			<< old_name << " to " << new_name;
	}
	
	ib::info() << "ddl log replay : RENAME TABLE table_id " << table_id
		<< ", old name " << old_name << ", new name " << new_name;
}

/** Relay REMOVE cache log
@param[in]	table_id	table id
@param[in]	table_name	table name */
void
LogDDL::replayRemoveLog(
	table_id_t	table_id,
	const char*	table_name)
{
	if (in_recovery) {
		return;
	}

	dict_table_t*	table;

	table = dd_table_open_on_id_in_mem(table_id, false);

	if (table != nullptr) {
		ut_ad(strcmp(table->name.m_name, table_name) == 0);

		mutex_enter(&dict_sys->mutex);
		dd_table_close(table, nullptr, nullptr, true);
		btr_drop_ahi_for_table(table);
		dict_table_remove_from_cache(table);
		mutex_exit(&dict_sys->mutex);
	}

	ib::info() << "ddl log replay : REMOVE table from cache table_id "
		<< table_id << ", new name " << table_name;
}

/** Replay and clean DDL logs after DDL transaction
commints or rollbacks.
@param[in]	thd	mysql thread
@return	DB_SUCCESS or error */
dberr_t
LogDDL::postDDL(THD*	thd)
{
	if (shouldSkip(nullptr, thd)) {
		return(DB_SUCCESS);
	}


	if (srv_read_only_mode || srv_force_recovery > 0) {
		return(DB_SUCCESS);
	}

	ulint	thread_id = thd_get_thread_id(thd);

	ib::info() << "innodb ddl log : post ddl begin, thread id : "
		<< thread_id;

	trx_t*	trx;
	trx = trx_allocate_for_background();
	trx->isolation_level = TRX_ISO_READ_COMMITTED;
	/* In order to get correct value of lock_wait_timeout */
	trx->mysql_thd = thd;

	thread_local_ddl_log_replay = true;

	scanByThreadId(trx, thread_id);

	thread_local_ddl_log_replay = false;

	deleteByThreadId(trx, thread_id);

	trx_commit_for_mysql(trx);
	trx_free_for_background(trx);

	ib::info() << "innodb ddl log : post ddl end, thread id : "
		<< thread_id;

	return(DB_SUCCESS);
}

/** Recover in server startup.
Scan innodb_ddl_log table, and replay all log entries.
Note: redo log should be applied, and dict transactions
should be recovered before calling this function.
@return	DB_SUCCESS or error */
dberr_t
LogDDL::recover()
{
	trx_t*	trx;
	trx = trx_allocate_for_background();

	thread_local_ddl_log_replay = true;
	in_recovery = true;

	scanAll(trx);

	thread_local_ddl_log_replay = false;
	in_recovery = false;

	deleteAll(trx);

	trx_commit_for_mysql(trx);
	trx_free_for_background(trx);

	return(DB_SUCCESS);
}
