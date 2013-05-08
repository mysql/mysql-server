/*****************************************************************************

Copyright (c) 2013, Oracle and/or its affiliates. All Rights Reserved.

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
@file row/row0trunc.cc
TRUNCATE implementation

Created 2013-04-25 Krunal Bauskar
*******************************************************/

#ifndef row0trunc_h
#define row0trunc_h

#include "row0mysql.h"
#include "pars0pars.h"
#include "dict0crea.h"
#include "dict0boot.h"
#include "dict0stats.h"
#include "dict0stats_bg.h"
#include "lock0lock.h"
#include "fts0fts.h"
#include "srv0space.h"
#include "srv0start.h"

/**
Iterator over the the raw records in an index, doesn't support MVCC. */
struct IndexIterator {

	/**
	Iterate over an indexes records
	@param index		index to iterate over */
	IndexIterator(dict_index_t* index)
		:
		m_index(index)
	{
		/* Do nothing */
	}

	/**
	Search for key. Position the cursor on a record GE key.
	@return DB_SUCCESS or error code. */
	dberr_t search(dtuple_t& key, bool turn_off_logging)
	{
		mtr_start(&m_mtr);

		if (turn_off_logging) {
			mtr_set_log_mode(&m_mtr, MTR_LOG_NONE);
		}


		btr_pcur_open_on_user_rec(
			m_index,
			&key,
			PAGE_CUR_GE,
			BTR_MODIFY_LEAF,
			&m_pcur, &m_mtr);

		return(DB_SUCCESS);
	}

	/**
	Iterate over all the records
	@return DB_SUCCESS or error code */
	template <typename Callback>
	dberr_t for_each(Callback& callback)
	{
		dberr_t	err = DB_SUCCESS;

		for (;;) {

			if (!btr_pcur_is_on_user_rec(&m_pcur)
			    || !callback.match(&m_mtr, &m_pcur)) {

				/* The end of of the index has been reached. */
				err = DB_END_OF_INDEX;
				break;
			}

			rec_t*	rec = btr_pcur_get_rec(&m_pcur);

			if (!rec_get_deleted_flag(rec, FALSE)) {

				err = callback(&m_mtr, &m_pcur);

				if (err != DB_SUCCESS) {
					break;
				}
			}

			btr_pcur_move_to_next_user_rec(&m_pcur, &m_mtr);
		}

		btr_pcur_close(&m_pcur);
		mtr_commit(&m_mtr);

		return(err == DB_END_OF_INDEX ? DB_SUCCESS : err);
	}

	mtr_t		m_mtr;
	btr_pcur_t	m_pcur;
	dict_index_t*	m_index;
};

/** SysIndex table iterator, iterate over records for a table. */
struct SysIndexIterator {

	/**
	Iterate over all the records that match the table id.
	@return DB_SUCCESS or error code */
	template <typename Callback>
	dberr_t for_each(Callback& callback) const
	{
		dict_index_t*	sys_index;
		byte		buf[DTUPLE_EST_ALLOC(1)];
		dtuple_t*	tuple =
			dtuple_create_from_mem(buf, sizeof(buf), 1);
		dfield_t*	dfield = dtuple_get_nth_field(tuple, 0);

		dfield_set_data(
			dfield,
			callback.table_id(),
			sizeof(*callback.table_id()));

		sys_index = dict_table_get_first_index(dict_sys->sys_indexes);

		dict_index_copy_types(tuple, sys_index, 1);

		IndexIterator	iterator(sys_index);

		/* Search on the table id and position the cursor
		on GE table_id. */
		iterator.search(*tuple, callback.get_logging_status());

		return(iterator.for_each(callback));
	}
};

/** Generic callback abstract class. */
struct Callback
{
	/**
	Constructor
	@param	table_id		id of the table being operated.
	@param	turn_off_logging	if true turn off logging. */
	Callback(table_id_t table_id, bool turn_off_logging)
		:
		m_id(),
		m_turn_off_logging(turn_off_logging)
	{
		/* Convert to storage byte order. */
		mach_write_to_8(&m_id, table_id);
	}

	/**
	Destructor */
	virtual ~Callback()
	{
		/* Do nothing */
	}

	/**
	@param mtr		mini-transaction covering the iteration
	@param pcur		persistent cursor used for iteration
	@return true if the table id column matches. */
	bool match(mtr_t* mtr, btr_pcur_t* pcur) const
	{
		ulint		len;
		const byte*	field;
		rec_t*		rec = btr_pcur_get_rec(pcur);

		field = rec_get_nth_field_old(
			rec, DICT_FLD__SYS_INDEXES__TABLE_ID, &len);

		ut_ad(len == 8);

		return(memcmp(&m_id, field, len) == 0);
	}

	/**
	@return pointer to table id storage format buffer */
	const table_id_t* table_id() const
	{
		return(&m_id);
	}

	/**
	@return	return if logging needs to be turned off. */
	bool get_logging_status() const
	{
		return(m_turn_off_logging);
	}

protected:
	/** Table id in storage format */
	table_id_t		m_id;

	/** Turn off logging. */
	bool			m_turn_off_logging;
};

/**
Creates a TRUNCATE log record with space id, table name, data directory path,
tablespace flags, table format, index ids, index types, number of index fields
and index field information of the table. */
struct Logger : public Callback {

	/**
	Constructor

	@param table	Table to truncate
	@param flags	tablespace falgs */
	Logger(dict_table_t* table, ulint flags, table_id_t new_table_id)
		:
		Callback(table->id, false),
		m_table(table),
		m_flags(flags),
		m_truncate()
	{
		if (m_table->data_dir_path != NULL) {
			m_truncate.m_dir_path = strdup(m_table->data_dir_path);
		}

		m_truncate.m_old_table_id = table->id;

		m_truncate.m_new_table_id = new_table_id;
	}

	/**
	@param mtr	mini-transaction covering the read
	@param pcur	persistent cursor used for reading
	@return DB_SUCCESS or error code */
	dberr_t operator()(mtr_t* mtr, btr_pcur_t* pcur);

	/** Called after iteratoring over the records.
	@return true if invariant satisfied. */
	bool debug() const
	{
		/* We must find all the index entries on disk. */
		return(UT_LIST_GET_LEN(m_table->indexes)
		       == m_truncate.m_indexes.size());
	}

	/**
	Write the TRUNCATE redo log */
	void log() const
	{
		m_truncate.write(
			m_table->space, m_table->name, m_flags,
			m_table->flags);
	}

private:
	/** Lookup the index using the index id.
	@return index instance if found else NULL */
	const dict_index_t* find(index_id_t id) const
	{
		for (const dict_index_t* index = UT_LIST_GET_FIRST(
				m_table->indexes);
		     index != NULL;
		     index = UT_LIST_GET_NEXT(indexes, index)) {

			if (index->id == id) {
				return(index);
			}
		}

		return(NULL);
	}

private:
	/** Table to be truncated */
	dict_table_t*		m_table;

	/** Tablespace flags */
	ulint			m_flags;

	/** Collect the truncate REDO information */
	truncate_t		m_truncate;
};

/** Callback to drop indexes during TRUNCATE */
struct DropIndex : public Callback {
	/**
	Constructor

	@param table	Table to truncate */
	DropIndex(dict_table_t* table)
		:
		Callback(table->id, false),
		m_table(table)
	{
		/* No op */
	}

	/**
	@param mtr	mini-transaction covering the read
	@param pcur	persistent cursor used for reading
	@return DB_SUCCESS or error code */
	dberr_t operator()(mtr_t* mtr, btr_pcur_t* pcur) const;

private:
	/** Table to be truncated */
	dict_table_t*		m_table;
};

/** Callback to create the indexes during TRUNCATE */
struct CreateIndex : public Callback {

	/**
	Constructor

	@param table	Table to truncate */
	CreateIndex(dict_table_t* table)
		:
		Callback(table->id, false),
		m_table(table)
	{
		/* No op */
	}

	/**
	Create the new index and update the root page number in the
	SysIndex table.

	@param mtr	mini-transaction covering the read
	@param pcur	persistent cursor used for reading
	@return DB_SUCCESS or error code */
	dberr_t operator()(mtr_t* mtr, btr_pcur_t* pcur) const;

private:
	/** Table to be truncated */
	dict_table_t*		m_table;
};

/**
Truncates a table for MySQL.
@param table		table being truncated
@param trx		transaction covering the truncate
@return	error code or DB_SUCCESS */
UNIV_INTERN
dberr_t
row_truncate_table_for_mysql(dict_table_t* table, trx_t* trx);

/**
Fix the table truncate by applying information cached while REDO log
scan phase. Fix-up includes re-creating table (drop and re-create
indexes) and for single-tablespace re-creating tablespace.
@return	error code or DB_SUCCESS */
UNIV_INTERN
dberr_t
row_fixup_truncate_of_tables();

#endif


