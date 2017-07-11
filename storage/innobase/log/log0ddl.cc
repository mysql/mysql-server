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
#include "dict0stats.h"
#include "log0ddl.h"
#include "pars0pars.h"
#include "que0que.h"
#include "row0sel.h"
#include "trx0trx.h"
#include "ha_innodb.h"
#include "btr0sea.h"
#include "row0ins.h"
#include "row0row.h"
#include "lock0lock.h"

LogDDL*	log_ddl = NULL;

/** Whether replaying ddl log
Note: we should not write ddl log when replaying ddl log. */
thread_local bool thread_local_ddl_log_replay = false;

/** Whether in recover(replay) ddl log in startup. */
bool LogDDL::in_recovery = false;

logDDLRecord::logDDLRecord()
{
	m_thread_id = ULINT_UNDEFINED;
	m_space_id = ULINT32_UNDEFINED;
	m_index_id = ULINT_UNDEFINED;
	m_table_id = ULINT_UNDEFINED;
	m_old_file_path = nullptr;
	m_new_file_path = nullptr;
	m_page_no = FIL_NULL;
	m_heap = nullptr;
}


logDDLRecord::~logDDLRecord()
{
	if (m_heap != nullptr) {
		mem_heap_free(m_heap);
	}
}

ulint logDDLRecord::get_id() const
{
	return(m_id);
}

void logDDLRecord::set_id(ulint id)
{
	m_id = id;
}

ulint logDDLRecord::get_type() const
{
	return(m_type);
}

void logDDLRecord::set_type(ulint record_type)
{
	m_type = record_type;
}

ulint logDDLRecord::get_thread_id() const
{
	return(m_thread_id);
}

void logDDLRecord::set_thread_id(ulint thr_id)
{
	m_thread_id = thr_id;
}

space_id_t logDDLRecord::get_space_id() const
{
	return(m_space_id);
}

void logDDLRecord::set_space_id(space_id_t space)
{
	m_space_id = space;
}

page_no_t logDDLRecord::get_page_no() const
{
	return(m_page_no);
}

void logDDLRecord::set_page_no(page_no_t page_no)
{
	m_page_no = page_no;
}

ulint logDDLRecord::get_index_id() const
{
	return(m_index_id);
}

void logDDLRecord::set_index_id(ulint ind_id)
{
	m_index_id = ind_id;
}

table_id_t logDDLRecord::get_table_id() const
{
	return(m_table_id);
}

void logDDLRecord::set_table_id(table_id_t table_id)
{
	m_table_id = table_id;
}

const char* logDDLRecord::get_old_file_path() const
{
	return(m_old_file_path);
}

void logDDLRecord::set_old_file_path(const char* name)
{
	ulint len = strlen(name);

	if (m_heap == nullptr) {
		m_heap = mem_heap_create(FN_REFLEN + 1);
	}

	m_old_file_path = mem_heap_strdupl(m_heap, name, len);
}

void logDDLRecord::set_old_file_path(
	const byte*	data,
	ulint		len)
{
	if (m_heap == nullptr) {
		m_heap = mem_heap_create(FN_REFLEN + 1);
	}

	m_old_file_path = static_cast<char*>(
			mem_heap_dup(m_heap, data, len + 1));
	m_old_file_path[len]='\0';
}

const char* logDDLRecord::get_new_file_path() const
{
	return(m_new_file_path);
}

void logDDLRecord::set_new_file_path(const char* name)
{
	ulint len = strlen(name);

	if (m_heap == nullptr) {
		m_heap = mem_heap_create(1000);
	}

	m_new_file_path = mem_heap_strdupl(m_heap, name, len);
}

void logDDLRecord::set_new_file_path(
	const byte*	data,
	ulint		len)
{
	if (m_heap == nullptr) {
		m_heap = mem_heap_create(1000);
	}

	m_new_file_path = static_cast<char*>(
			mem_heap_dup(m_heap, data, len + 1));
	m_new_file_path[len]='\0';
}

ulint logDDLRecord::fetch_value(
	const byte*	data,
	ulint		offset)
{
	ulint	value = 0;
	switch(offset) {
		case ID_COL_NO:
		case THREAD_ID_COL_NO:
		case INDEX_ID_COL_NO:
		case TABLE_ID_COL_NO:
			value = mach_read_from_8(data);
			return(value);
		case TYPE_COL_NO:
		case SPACE_ID_COL_NO:
		case PAGE_NO_COL_NO:
			value = mach_read_from_4(data);
			return(value);
		case NEW_FILE_PATH_COL_NO:
		case OLD_FILE_PATH_COL_NO:
		default:
			ut_ad(0);
			break;
	}

	return(value);
}

void logDDLRecord::set_field(
	const byte*	data,
	ulint		index_offset,
	ulint		len)
{
	dict_index_t*	index = dict_sys->ddl_log->first_index();
	ulint	col_offset = index->get_col_no(index_offset);

	if (col_offset == NEW_FILE_PATH_COL_NO) {
		set_new_file_path(data, len);
		return;
	}

	if (col_offset == OLD_FILE_PATH_COL_NO) {
		set_old_file_path(data, len);
		return;
	}

	ulint value = fetch_value(data, col_offset);
	switch(col_offset) {
		case ID_COL_NO:
			set_id(value);
			break;
		case THREAD_ID_COL_NO:
			set_thread_id(value);
			break;
		case TYPE_COL_NO:
			set_type(value);
			break;
		case SPACE_ID_COL_NO:
			set_space_id(value);
			break;
		case PAGE_NO_COL_NO:
			set_page_no(value);
			break;
		case INDEX_ID_COL_NO:
			set_index_id(value);
			break;
		case TABLE_ID_COL_NO:
			set_table_id(value);
			break;
		case OLD_FILE_PATH_COL_NO:
		case NEW_FILE_PATH_COL_NO:
		default:
			ut_ad(0);
	}
}

DDLLogTable::DDLLogTable()
{
	m_table = dict_sys->ddl_log;
	m_heap = mem_heap_create(1000);
	m_thr = nullptr;
	m_trx = nullptr;
}

DDLLogTable::DDLLogTable(trx_t*	trx)
{
	m_table = dict_sys->ddl_log;
	m_trx = trx;
	ut_ad(m_trx->ddl_operation);
	m_heap = mem_heap_create(1000);
	start_query_thread();
}

DDLLogTable::~DDLLogTable()
{
	mem_heap_free(m_heap);
}

void
DDLLogTable::start_query_thread()
{
	que_t* graph = static_cast<que_fork_t*>(
		que_node_get_parent(
			pars_complete_graph_for_exec(
				NULL, m_trx, m_heap, NULL)));
	m_thr = que_fork_start_command(graph);
	ut_ad(m_trx->lock.n_active_thrs == 1);
}

void
DDLLogTable::stop_query_thread()
{
	if (m_thr != nullptr) {
		que_thr_stop_for_mysql_no_error(
				m_thr, m_trx);
	}
}

std::vector<ulint>
DDLLogTable::get_list()
{
	return(m_ddl_record_ids);
}

std::vector<logDDLRecord*>
DDLLogTable::get_records_list()
{
	return(m_ddl_records);
}

dict_index_t*
DDLLogTable::getIndex(
	ulint	type)
{
	dict_index_t*	index = m_table->first_index();

	if (type == DDLLogTable::SEARCH_THREAD_ID_OP) {
		index = index->next();
	}

	return(index);
}

void
DDLLogTable::create_tuple(logDDLRecord &ddl_record)
{
	const dict_col_t*	col;
	dfield_t*		dfield;
	byte*			buf;

	m_tuple = dtuple_create(m_heap, m_table->get_n_cols());
	dict_table_copy_types(m_tuple, m_table);
	buf = static_cast<byte*>(mem_heap_alloc(m_heap, 8));
	memset(buf, 0xFF, 8);

	col = m_table->get_sys_col(DATA_ROW_ID);
	dfield = dtuple_get_nth_field(m_tuple, dict_col_get_no(col));
	dfield_set_data(dfield, buf, DATA_ROW_ID_LEN);

	col = m_table->get_sys_col(DATA_ROLL_PTR);
	dfield = dtuple_get_nth_field(m_tuple, dict_col_get_no(col));
	dfield_set_data(dfield, buf, DATA_ROLL_PTR_LEN);

	buf = static_cast<byte*>(mem_heap_alloc(m_heap, DATA_TRX_ID_LEN));
	mach_write_to_6(buf, m_trx->id);
	col = m_table->get_sys_col(DATA_TRX_ID);
	dfield = dtuple_get_nth_field(m_tuple, dict_col_get_no(col));
	dfield_set_data(dfield, buf, DATA_TRX_ID_LEN);

	const ulint	rec_id = ddl_record.get_id();

	if (rec_id != ULINT_UNDEFINED) {
		buf = static_cast<byte*>(mem_heap_alloc(
				m_heap, logDDLRecord::ID_COL_LEN));
		mach_write_to_8(buf, rec_id);
		dfield = dtuple_get_nth_field(m_tuple,
					      logDDLRecord::ID_COL_NO);
		dfield_set_data(dfield, buf, logDDLRecord::ID_COL_LEN);
	}

	if (ddl_record.get_thread_id() != ULINT_UNDEFINED) {
		buf = static_cast<byte*>(mem_heap_alloc(
				m_heap, logDDLRecord::THREAD_ID_COL_LEN));
		mach_write_to_8(buf, ddl_record.get_thread_id());
		dfield = dtuple_get_nth_field(m_tuple,
					      logDDLRecord::THREAD_ID_COL_NO);
		dfield_set_data(dfield, buf, logDDLRecord::THREAD_ID_COL_LEN);
	}

	if (ddl_record.get_type() != 0) {
		buf = static_cast<byte*>(mem_heap_alloc(
				m_heap, logDDLRecord::TYPE_COL_LEN));
		mach_write_to_4(buf, ddl_record.get_type());
		dfield = dtuple_get_nth_field(m_tuple,
					      logDDLRecord::TYPE_COL_NO);
		dfield_set_data(dfield, buf, logDDLRecord::TYPE_COL_LEN);
	}

	if (ddl_record.get_space_id() != ULINT32_UNDEFINED) {
		buf = static_cast<byte*>(mem_heap_alloc(
				m_heap, logDDLRecord::SPACE_ID_COL_LEN));
		mach_write_to_4(buf, ddl_record.get_space_id());
		dfield = dtuple_get_nth_field(m_tuple,
					      logDDLRecord::SPACE_ID_COL_NO);
		dfield_set_data(dfield, buf, logDDLRecord::SPACE_ID_COL_LEN);
	}

	if (ddl_record.get_page_no() != FIL_NULL) {
		buf = static_cast<byte*>(mem_heap_alloc(
				m_heap, logDDLRecord::PAGE_NO_COL_LEN));
		mach_write_to_4(buf, ddl_record.get_page_no());
		dfield = dtuple_get_nth_field(m_tuple,
					      logDDLRecord::PAGE_NO_COL_NO);
		dfield_set_data(dfield, buf, logDDLRecord::PAGE_NO_COL_LEN);
	}

	if (ddl_record.get_index_id() != ULINT_UNDEFINED) {
		buf = static_cast<byte*>(mem_heap_alloc(
				m_heap, logDDLRecord::INDEX_ID_COL_LEN));
		mach_write_to_8(buf, ddl_record.get_index_id());
		dfield = dtuple_get_nth_field(m_tuple,
					      logDDLRecord::INDEX_ID_COL_NO);
		dfield_set_data(dfield, buf, logDDLRecord::INDEX_ID_COL_LEN);
	}

	if (ddl_record.get_table_id() != ULINT_UNDEFINED) {
		buf = static_cast<byte*>(mem_heap_alloc(
				m_heap, logDDLRecord::TABLE_ID_COL_LEN));
		mach_write_to_8(buf, ddl_record.get_table_id());
		dfield = dtuple_get_nth_field(m_tuple,
					      logDDLRecord::TABLE_ID_COL_NO);
		dfield_set_data(dfield, buf, logDDLRecord::TABLE_ID_COL_LEN);
	}

	if (ddl_record.get_old_file_path() != nullptr) {
		ulint m_len = strlen(ddl_record.get_old_file_path()) + 1;
		dfield = dtuple_get_nth_field(m_tuple,
					      logDDLRecord::OLD_FILE_PATH_COL_NO);
		dfield_set_data(dfield, ddl_record.get_old_file_path(),
				m_len);
	}

	if (ddl_record.get_new_file_path() != nullptr) {
		ulint m_len = strlen(ddl_record.get_new_file_path()) + 1;
		dfield = dtuple_get_nth_field(m_tuple,
					      logDDLRecord::NEW_FILE_PATH_COL_NO);
		dfield_set_data(dfield, ddl_record.get_new_file_path(),
				m_len);
	}
}

void
DDLLogTable::create_tuple(ulint id, const dict_index_t* index)
{
	ut_ad(id != ULINT_UNDEFINED);

	dfield_t*	dfield;
	ulint		len;
	ulint		table_col_offset;
	ulint		index_col_offset;

	m_tuple = dtuple_create(m_heap, 1);
	dict_index_copy_types(m_tuple, index, 1);

	if (index->is_clustered()) {
		len = logDDLRecord::ID_COL_LEN;
		table_col_offset = logDDLRecord::ID_COL_NO;
	} else {
		len = logDDLRecord::THREAD_ID_COL_LEN;
		table_col_offset = logDDLRecord::THREAD_ID_COL_NO;
	}

	index_col_offset = index->get_col_pos(table_col_offset);
	byte* buf = static_cast<byte*>(mem_heap_alloc(m_heap, len));
	mach_write_to_8(buf, id);
	dfield = dtuple_get_nth_field(m_tuple, index_col_offset);
	dfield_set_data(dfield, buf, len);
}

dberr_t
DDLLogTable::insert(
	logDDLRecord& ddl_record)
{
	dberr_t		error;
	dict_index_t*	index = m_table->first_index();
	dtuple_t*	entry;
	ulint		flags = BTR_NO_LOCKING_FLAG;
	mem_heap_t*	offsets_heap = mem_heap_create(1000);

	create_tuple(ddl_record);
	entry = row_build_index_entry(m_tuple, NULL,
				      index, m_heap);

	error = row_ins_clust_index_entry_low(
			flags, BTR_MODIFY_LEAF, index,
			index->n_uniq,
			entry, 0, m_thr, false);

	if (error == DB_FAIL) {
		error = row_ins_clust_index_entry_low(
				flags, BTR_MODIFY_TREE, index,
				index->n_uniq, entry,
				0, m_thr, false);
		ut_ad(error == DB_SUCCESS);
	}

	index = index->next();

	entry = row_build_index_entry(m_tuple, NULL, index, m_heap);

	error = row_ins_sec_index_entry_low(
			flags, BTR_MODIFY_LEAF, index, offsets_heap, m_heap,
			entry, m_trx->id, m_thr, false);

	if (error == DB_FAIL) {
		error = row_ins_sec_index_entry_low(
				flags, BTR_MODIFY_TREE, index, offsets_heap,
				m_heap, entry, m_trx->id, m_thr, false);
	}

	mem_heap_free(offsets_heap);
	ut_ad(error == DB_SUCCESS);
	return(error);
}

void
DDLLogTable::convert_to_ddl_record(
	rec_t*		clust_rec,
	ulint*		clust_offsets,
	logDDLRecord&	ddl_record)
{
	for (ulint i = 0; i < rec_offs_n_fields(clust_offsets); i++) {
		const byte*	data;
		ulint		len;
		data = rec_get_nth_field(clust_rec, clust_offsets,
					 i, &len);

		if (i == DATA_ROLL_PTR
		    || i == DATA_TRX_ID) {
			continue;
		}

		if (len != UNIV_SQL_NULL) {
			ddl_record.set_field(data, i, len);
		}
	}
}

ulint
DDLLogTable::fetch_id_from_sec_rec_index(
	rec_t*		rec,
	ulint*		offsets)
{
	ulint		len;
	dict_index_t*	index = m_table->first_index()->next();
	ulint		index_offset = index->get_col_pos(
					logDDLRecord::ID_COL_NO);

	byte* data = rec_get_nth_field(rec, offsets,
				       index_offset, &len);

	ut_ad(len == logDDLRecord::ID_COL_LEN);
	ulint value = mach_read_from_8(data);
	return(value);
}

dberr_t
DDLLogTable::search()
{
	mtr_t				mtr;
	btr_pcur_t			pcur;
	rec_t*				rec;
	bool				move = true;
	ulint*				offsets;
	dict_index_t*			index = m_table->first_index();
	dberr_t				error = DB_SUCCESS;

	mtr_start(&mtr);

	/** Scan the index in decreasing order. */
	btr_pcur_open_at_index_side(
		false, index, BTR_SEARCH_LEAF, &pcur, true,
		0, &mtr);

	for (;move == true; move = btr_pcur_move_to_prev(&pcur, &mtr)) {

		rec = btr_pcur_get_rec(&pcur);

		if (page_rec_is_infimum(rec)
		    || page_rec_is_supremum(rec)) {
			continue;
		}

		offsets = rec_get_offsets(rec, index, NULL,
				ULINT_UNDEFINED, &m_heap);

		if (rec_get_deleted_flag(
				rec, dict_table_is_comp(m_table))) {
			continue;
		}

		/** Replay the ddl record operation. */
		logDDLRecord*	ddl_record = new logDDLRecord();
		convert_to_ddl_record(
			rec, offsets, *ddl_record);
		m_ddl_records.push_back(ddl_record);

		/** Store the ids in case of ScanALL.
		It can be stored to clear the table. */
		const ulint id = ddl_record->get_id();
		m_ddl_record_ids.push_back(id);
	}

	btr_pcur_close(&pcur);
	mtr_commit(&mtr);
	return(error);
}

dberr_t
DDLLogTable::search(
	ulint	type)
{
	/** In case of search by thread id, InnoDB have to scan the
	clustered index with the id present in the list and replay
	the ddl record operation. */
	ut_ad(type == DDLLogTable::SEARCH_THREAD_ID_OP);
	dberr_t	error = DB_SUCCESS;

	for(auto it = m_ddl_record_ids.rbegin(); it != m_ddl_record_ids.rend();
	    it++) {
		error = search(*it, DDLLogTable::SEARCH_ID_OP);
		ut_ad(error == DB_SUCCESS);
	}

	return(error);
}

dberr_t
DDLLogTable::search(
	ulint	id,
	ulint	type)
{
	ut_ad(type == DDLLogTable::SEARCH_THREAD_ID_OP
	      || type == DDLLogTable::SEARCH_ID_OP);

	mtr_t				mtr;
	btr_pcur_t			pcur;
	rec_t*				rec;
	bool				move = true;
	ulint*				offsets;
	dict_index_t*			index = getIndex(type);
	dberr_t				error = DB_SUCCESS;

	mtr_start(&mtr);

	/** Search the tuple in the index. */
	create_tuple(id, index);
	btr_pcur_open_with_no_init(index, m_tuple, PAGE_CUR_GE,
				   BTR_SEARCH_LEAF, &pcur, 0, &mtr);

	for (; move == true; move = btr_pcur_move_to_next(&pcur, &mtr)) {

		rec = btr_pcur_get_rec(&pcur);

		if (page_rec_is_infimum(rec)
		    || page_rec_is_supremum(rec)) {
			continue;
		}

		offsets = rec_get_offsets(rec, index, NULL,
				ULINT_UNDEFINED, &m_heap);

		if (0 != cmp_dtuple_rec(
			m_tuple, rec, index, offsets)) {
			break;
		}

		if (rec_get_deleted_flag(
				rec, dict_table_is_comp(m_table))) {
			continue;
		}

		if (type == DDLLogTable::SEARCH_ID_OP) {

			/** Replay the ddl record operation. */
			logDDLRecord*	ddl_record = new logDDLRecord();
			convert_to_ddl_record(
					rec, offsets, *ddl_record);
			m_ddl_records.push_back(ddl_record);

		} else {
			/** Fetch the record id from secondary index record.
			Store it and it can be used to replay the operation,
			remove the entry from innodb_ddl_log table. */
			const ulint id =
				fetch_id_from_sec_rec_index(rec, offsets);
			m_ddl_record_ids.push_back(id);
		}
	}

	mtr_commit(&mtr);

	if (type == DDLLogTable::SEARCH_ID_OP) {
		return(error);
	}

	error = search(DDLLogTable::SEARCH_THREAD_ID_OP);
	return(error);
}

dberr_t
DDLLogTable::remove(
	ulint	id,
	ulint	type)
{
	ut_ad(type == DDLLogTable::DELETE_ID_OP);

	mtr_t			mtr;
	dict_index_t*		clust_index = m_table->first_index();
	btr_pcur_t		pcur;
	ulint*			offsets;
	rec_t*			rec;
	dict_index_t*		index;
	dtuple_t*		row;
	btr_cur_t*		btr_cur;
	dtuple_t*		entry;
	dberr_t			error = DB_SUCCESS;
	enum row_search_result	search_result;
	ulint			flags = BTR_NO_LOCKING_FLAG;

	create_tuple(id, clust_index);

	mtr_start(&mtr);

	btr_pcur_open(clust_index, m_tuple, PAGE_CUR_LE,
		      BTR_MODIFY_TREE | BTR_LATCH_FOR_DELETE,
		      &pcur, &mtr);

	btr_cur = btr_pcur_get_btr_cur(&pcur);

	if (page_rec_is_infimum(btr_pcur_get_rec(&pcur))
	    || btr_pcur_get_low_match(&pcur) < clust_index->n_uniq) {
		btr_pcur_close(&pcur);
		mtr_commit(&mtr);
		return(DB_SUCCESS);
	}

	offsets = rec_get_offsets(btr_pcur_get_rec(&pcur), clust_index, NULL,
				  ULINT_UNDEFINED, &m_heap);

	row = row_build(ROW_COPY_DATA, clust_index, btr_pcur_get_rec(&pcur),
			offsets, NULL, NULL, NULL, NULL, m_heap);

	rec = btr_cur_get_rec(btr_cur);

	if (!rec_get_deleted_flag(rec, dict_table_is_comp(m_table))) {
		error = btr_cur_del_mark_set_clust_rec(
				flags, btr_cur_get_block(btr_cur),
				rec, clust_index, offsets,
				m_thr, m_tuple, &mtr);
	}

	btr_pcur_close(&pcur);
	mtr_commit(&mtr);

	if (error != DB_SUCCESS) {
		return(error);
	}

	mtr_start(&mtr);
	index = clust_index->next();
	entry = row_build_index_entry(row, NULL, index, m_heap);
	search_result = row_search_index_entry(index, entry,
					       BTR_MODIFY_LEAF | BTR_DELETE_MARK,
					       &pcur, &mtr);
	btr_cur = btr_pcur_get_btr_cur(&pcur);

	if (search_result == ROW_NOT_FOUND) {
		btr_pcur_close(&pcur);
		mtr_commit(&mtr);
		ut_ad(0);
		return(DB_CORRUPTION);
	}

	rec = btr_cur_get_rec(btr_cur);

	if (!rec_get_deleted_flag(rec, dict_table_is_comp(m_table))) {
		error = btr_cur_del_mark_set_sec_rec(
				flags, btr_cur, TRUE, m_thr, &mtr);
	}

	btr_pcur_close(&pcur);
	mtr_commit(&mtr);

	return(error);
}

dberr_t
DDLLogTable::remove(
	std::vector<ulint>&	ddl_record_ids,
	ulint			type)
{
	ut_ad(type == DDLLogTable::DELETE_LIST_OP);

	dberr_t	error = DB_SUCCESS;

	for(auto it = ddl_record_ids.begin();
	    it != ddl_record_ids.end(); it++) {
		error = remove(*it, DDLLogTable::DELETE_ID_OP);
		ut_ad(error == DB_SUCCESS);
	}

	return(error);
}

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

	trx->ddl_operation = true;

	ib_uint64_t	id = getNextId();
	ulint		thread_id = thd_get_thread_id(trx->mysql_thd);

	dberr_t	err;

	if (is_drop) {
		/* Drop index case, if committed, will be redo only */
		err = insertFreeTreeLog(trx, index, id, thread_id);
		ut_ad(err == DB_SUCCESS);
	} else {
		/* This is the case of building index during create table
		scenario. The index will be dropped if ddl is rolled back */
		err = insertFreeTreeLog(nullptr, index, id, thread_id);
		ut_ad(err == DB_SUCCESS);
		trx->ddl_operation = true;

		/* Delete this operation is the create trx is committed */
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
		trx_start_internal(trx);
	} else {
		trx_start_if_not_started(trx, true);
	}

	trx->ddl_operation = true;

	ut_ad(mutex_own(&dict_sys->mutex));

	mutex_exit(&dict_sys->mutex);

	dberr_t	error = lock_table_for_trx(dict_sys->ddl_log,
					   trx, LOCK_IX);
	ut_ad(error == DB_SUCCESS);

	logDDLRecord ddl_record;
	ddl_record.set_id(id);
	ddl_record.set_thread_id(thread_id);
	ddl_record.set_type(LogType::FREE_TREE_LOG);
	ddl_record.set_space_id(index->space);
	ddl_record.set_page_no(index->page);
	ddl_record.set_index_id(index->id);

	DDLLogTable insert_op(trx);
	error = insert_op.insert(ddl_record);
	insert_op.stop_query_thread();
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

	trx->ddl_operation = true;

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
		trx->ddl_operation = true;

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
		trx_start_internal(trx);
	} else {
		trx_start_if_not_started(trx, true);
	}

	trx->ddl_operation = true;

	if (dict_locked) {
		mutex_exit(&dict_sys->mutex);
	}

	dberr_t	error = lock_table_for_trx(dict_sys->ddl_log, trx, LOCK_IX);
	ut_ad(error == DB_SUCCESS);

	logDDLRecord	ddl_record;
	ddl_record.set_id(id);
	ddl_record.set_thread_id(thread_id);
	ddl_record.set_type(LogType::DELETE_SPACE_LOG);
	ddl_record.set_space_id(space_id);
	ddl_record.set_old_file_path(file_path);

	DDLLogTable	insert_op(trx);
	error = insert_op.insert(ddl_record);
	insert_op.stop_query_thread();

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
	/* Missing current_thd, it happens during crash recovery */
	if (!current_thd) {
		return(DB_SUCCESS);
	}

	trx = thd_to_trx(current_thd);

	/* This is special case for fil_rename_tablespace during recovery */
	if (trx == nullptr) {
		return(DB_SUCCESS);
	}

	if (shouldSkip(NULL, trx->mysql_thd)) {
		return(DB_SUCCESS);
	}

	trx->ddl_operation = true;

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
	trx_start_internal(trx);
	trx->ddl_operation = true;

	ut_ad(mutex_own(&dict_sys->mutex));
	mutex_exit(&dict_sys->mutex);

	dberr_t	error = lock_table_for_trx(dict_sys->ddl_log,
					   trx, LOCK_IX);
	ut_ad(error == DB_SUCCESS);

	logDDLRecord ddl_record;
	ddl_record.set_id(id);
	ddl_record.set_thread_id(thread_id);
	ddl_record.set_type(LogType::RENAME_LOG);
	ddl_record.set_space_id(space_id);
	ddl_record.set_old_file_path(old_file_path);
	ddl_record.set_new_file_path(new_file_path);

	DDLLogTable	insert_op(trx);
	error = insert_op.insert(ddl_record);
	insert_op.stop_query_thread();
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

	trx->ddl_operation = true;

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
	trx_start_if_not_started(trx, true);
	ut_ad(mutex_own(&dict_sys->mutex));

	mutex_exit(&dict_sys->mutex);

	dberr_t	error = lock_table_for_trx(dict_sys->ddl_log, trx,
					   LOCK_IX);
	ut_ad(error == DB_SUCCESS);

	logDDLRecord ddl_record;
	ddl_record.set_id(id);
	ddl_record.set_thread_id(thread_id);
	ddl_record.set_type(LogType::DROP_LOG);
	ddl_record.set_table_id(table_id);

	DDLLogTable	insert_op(trx);
	error = insert_op.insert(ddl_record);
	insert_op.stop_query_thread();
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

	trx->ddl_operation = true;

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
	trx_start_internal(trx);
	trx->ddl_operation = true;

	ut_ad(mutex_own(&dict_sys->mutex));
	mutex_exit(&dict_sys->mutex);

	dberr_t	error = lock_table_for_trx(dict_sys->ddl_log, trx,
					   LOCK_IX);
	ut_ad(error == DB_SUCCESS);

	logDDLRecord ddl_record;
	ddl_record.set_id(id);
	ddl_record.set_thread_id(thread_id);
	ddl_record.set_type(LogType::RENAME_TABLE_LOG);
	ddl_record.set_table_id(table_id);
	ddl_record.set_old_file_path(old_name);
	ddl_record.set_new_file_path(new_name);

	DDLLogTable	insert_op(trx);
	error = insert_op.insert(ddl_record);
	insert_op.stop_query_thread();

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
	trx->ddl_operation = true;

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
	trx_start_internal(trx);
	trx->ddl_operation = true;

	ut_ad(mutex_own(&dict_sys->mutex));

	mutex_exit(&dict_sys->mutex);

	dberr_t	error = lock_table_for_trx(dict_sys->ddl_log, trx,
					   LOCK_IX);
	ut_ad(error == DB_SUCCESS);

	logDDLRecord ddl_record;
	ddl_record.set_id(id);
	ddl_record.set_thread_id(thread_id);
	ddl_record.set_type(LogType::REMOVE_CACHE_LOG);
	ddl_record.set_table_id(table_id);
	ddl_record.set_new_file_path(table_name);

	DDLLogTable	insert_op(trx);
	error = insert_op.insert(ddl_record);
	insert_op.stop_query_thread();

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
	trx_start_if_not_started(trx, true);

	ut_ad(mutex_own(&dict_sys->mutex));

	mutex_exit(&dict_sys->mutex);

	dberr_t error = lock_table_for_trx(dict_sys->ddl_log,
					   trx, LOCK_IX);

	DDLLogTable	delete_op(trx);
	error = delete_op.remove(id, DDLLogTable::DELETE_ID_OP);
	delete_op.stop_query_thread();

	mutex_enter(&dict_sys->mutex);

	ut_ad(error == DB_SUCCESS);

	trx->isolation_level = org_isolation_level;

	ib::info() << "ddl log delete : " << "by id " << id;

	return(error);
}

dberr_t
LogDDL::replayAll(
	std::vector<ulint>&	ids_list)
{
	std::vector<logDDLRecord*>	ddl_records;
	DDLLogTable			search_op;

	dberr_t error = search_op.search();
	ut_ad(error == DB_SUCCESS);
	ids_list = search_op.get_list();
	ddl_records = search_op.get_records_list();

	for (auto record : ddl_records) {
		log_ddl->replay(*record);
		delete(record);
	}

	return(error);
}

dberr_t
LogDDL::replayByThreadID(
	trx_t*			trx,
	ulint			thread_id,
	std::vector<ulint>&	ids_list)
{
	std::vector<logDDLRecord*>	ddl_records;
	DDLLogTable			search_op;

	dberr_t error = search_op.search(
			thread_id, DDLLogTable::SEARCH_THREAD_ID_OP);
	ut_ad(error == DB_SUCCESS);

	ids_list = search_op.get_list();
	ddl_records = search_op.get_records_list();

	for (auto record : ddl_records) {
		log_ddl->replay(*record);
		delete(record);
	}

	return(error);
}

dberr_t
LogDDL::deleteByList(
	trx_t*			trx,
	std::vector<ulint>&	ids_list)
{
	if (ids_list.size() == 0) {
		return(DB_SUCCESS);
	}

	trx_start_if_not_started(trx, true);
	dberr_t	error = lock_table_for_trx(dict_sys->ddl_log,
					   trx, LOCK_IX);
	ut_ad(error == DB_SUCCESS);

	DDLLogTable	delete_op(trx);
	error = delete_op.remove(ids_list, DDLLogTable::DELETE_LIST_OP);
	delete_op.stop_query_thread();
	ut_ad(error == DB_SUCCESS);

	return(error);
}

/** Replay DDL log record
@param[in,out]	record	DDL log record
return DB_SUCCESS or error */
dberr_t
LogDDL::replay(
	logDDLRecord&		record)
{
	dberr_t		err = DB_SUCCESS;

	switch(record.get_type()) {
	case	LogType::FREE_TREE_LOG:
		replayFreeLog(
			record.get_space_id(),
			record.get_page_no(),
			record.get_index_id());
		break;

	case	LogType::DELETE_SPACE_LOG:
		replayDeleteLog(
			record.get_space_id(),
			record.get_old_file_path());
		break;

	case	LogType::RENAME_LOG:
		replayRenameLog(
			record.get_space_id(),
			record.get_old_file_path(),
			record.get_new_file_path());
		break;

	case	LogType::DROP_LOG:
		replayDropLog(record.get_table_id());
		break;

	case	LogType::RENAME_TABLE_LOG:
		replayRenameTableLog(
			record.get_table_id(),
			record.get_old_file_path(),
			record.get_new_file_path());
		break;

	case	LogType::REMOVE_CACHE_LOG:
		replayRemoveLog(
			record.get_table_id(),
			record.get_new_file_path());
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

extern ib_mutex_t	master_key_id_mutex;

/** Replay DELETE log(delete file if exist)
@param[in]	space_id	tablespace id
@param[in]	file_path	file path */
void
LogDDL::replayDeleteLog(
	space_id_t	space_id,
	const char*	file_path)
{
	/* Require the mutex to block key rotation. Please note that
	here we don't know if this tablespace is encrypted or not,
	so just acquire the mutex unconditionally. */
	mutex_enter(&master_key_id_mutex);

	row_drop_single_table_tablespace(space_id, NULL, file_path);

	mutex_exit(&master_key_id_mutex);

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

	ret = fil_op_replay_rename_for_ddl(page_id, old_file_path, new_file_path);
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

	dict_table_t*	table;
	table = dd_table_open_on_name_in_mem(new_name, true);
	if (table != nullptr) {
		dict_table_ddl_release(table);
		dd_table_close(table, nullptr, nullptr, true);
	}

	row_mysql_unlock_data_dictionary(trx);

	trx_commit_for_mysql(trx);
	trx_free_for_background(trx);

	if (err != DB_SUCCESS) {
		ib::info() << "ddl log replay rename table in cache from "
			<< old_name << " to " << new_name;
	} else {
		/* TODO: Once we get rid of dict_operation_lock,
		we may consider to do this in row_rename_table_for_mysql,
		so no need to worry this rename here */
		char	errstr[512];

		dict_stats_rename_table(old_name, new_name,
					errstr, sizeof(errstr));
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

	if (srv_read_only_mode
	    || srv_force_recovery >= SRV_FORCE_NO_UNDO_LOG_SCAN) {
		return(DB_SUCCESS);
	}

	if (srv_force_recovery > 0) {
		/* In this mode, DROP TABLE is allowed, so here only
		DELETE and DROP log can be replayed. */
	}

	ulint	thread_id = thd_get_thread_id(thd);
	std::vector<ulint>	ids_list;

	ib::info() << "innodb ddl log : post ddl begin, thread id : "
		<< thread_id;

	trx_t*	trx;
	trx = trx_allocate_for_background();
	trx->isolation_level = TRX_ISO_READ_COMMITTED;
	trx->ddl_operation = true;
	/* In order to get correct value of lock_wait_timeout */
	trx->mysql_thd = thd;

	thread_local_ddl_log_replay = true;

	replayByThreadID(trx, thread_id, ids_list);

	thread_local_ddl_log_replay = false;

	deleteByList(trx, ids_list);

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
	if (srv_read_only_mode
	    || srv_force_recovery > 0) {
		return(DB_SUCCESS);
	}

	trx_t*	trx;
	trx = trx_allocate_for_background();
	trx->ddl_operation = true;
	std::vector<ulint>	ids_list;

	thread_local_ddl_log_replay = true;
	in_recovery = true;

	replayAll(ids_list);

	thread_local_ddl_log_replay = false;
	in_recovery = false;

	deleteByList(trx, ids_list);

	trx_commit_for_mysql(trx);
	trx_free_for_background(trx);

	return(DB_SUCCESS);
}
