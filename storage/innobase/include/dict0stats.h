/*****************************************************************************

Copyright (c) 2009, 2017, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/**************************************************//**
@file include/dict0stats.h
Code used for calculating and manipulating table statistics.

Created Jan 06, 2010 Vasil Dimov
*******************************************************/

#ifndef dict0stats_h
#define dict0stats_h

#include "univ.i"

#include "dict0types.h"
#include "trx0types.h"
#include "mem0mem.h"

enum dict_stats_upd_option_t {
	DICT_STATS_RECALC_PERSISTENT,/* (re) calculate the
				statistics using a precise and slow
				algo and save them to the persistent
				storage, if the persistent storage is
				not present then emit a warning and
				fall back to transient stats */
	DICT_STATS_RECALC_TRANSIENT,/* (re) calculate the statistics
				using an imprecise quick algo
				without saving the results
				persistently */
	DICT_STATS_EMPTY_TABLE,	/* Write all zeros (or 1 where it makes sense)
				into a table and its indexes' statistics
				members. The resulting stats correspond to an
				empty table. If the table is using persistent
				statistics, then they are saved on disk. */
	DICT_STATS_FETCH_ONLY_IF_NOT_IN_MEMORY /* fetch the stats
				from the persistent storage if the in-memory
				structures have not been initialized yet,
				otherwise do nothing */
};

/** Set the persistent statistics flag for a given table. This is set only in
the in-memory table object and is not saved on disk. It will be read from the
.frm file upon first open from MySQL after a server restart.
@param[in,out]	table	table
@param[in]	ps_on	persistent stats explicitly enabled
@param[in]	ps_off	persistent stats explicitly disabled */
UNIV_INLINE
void
dict_stats_set_persistent(
	dict_table_t*	table,
	ibool		ps_on,
	ibool		ps_off);

/*********************************************************************//**
Check whether persistent statistics is enabled for a given table.
@return TRUE if enabled, FALSE otherwise */
UNIV_INLINE
ibool
dict_stats_is_persistent_enabled(
/*=============================*/
	const dict_table_t*	table)	/*!< in: table */
	MY_ATTRIBUTE((warn_unused_result));

/** Set the auto recalc flag for a given table (only honored for a persistent
stats enabled table). The flag is set only in the in-memory table object and is
not saved in InnoDB files. It will be read from the .frm file upon first open
from MySQL after a server restart.
@param[in,out]	table		table
@param[in]	auto_recalc_on	explicitly enabled
@param[in]	auto_recalc_off	explicitly disabled */
UNIV_INLINE
void
dict_stats_auto_recalc_set(
	dict_table_t*	table,
	ibool		auto_recalc_on,
	ibool		auto_recalc_off);

/*********************************************************************//**
Check whether auto recalc is enabled for a given table.
@return TRUE if enabled, FALSE otherwise */
UNIV_INLINE
ibool
dict_stats_auto_recalc_is_enabled(
/*==============================*/
	const dict_table_t*	table);	/*!< in: table */

/*********************************************************************//**
Initialize table's stats for the first time when opening a table. */
UNIV_INLINE
void
dict_stats_init(
/*============*/
	dict_table_t*	table);	/*!< in/out: table */

/*********************************************************************//**
Deinitialize table's stats after the last close of the table. This is
used to detect "FLUSH TABLE" and refresh the stats upon next open. */
UNIV_INLINE
void
dict_stats_deinit(
/*==============*/
	dict_table_t*	table);	/*!< in/out: table */

/*********************************************************************//**
Calculates new estimates for table and index statistics. The statistics
are used in query optimization.
@return DB_* error code or DB_SUCCESS */
dberr_t
dict_stats_update(
/*==============*/
	dict_table_t*		table,	/*!< in/out: table */
	dict_stats_upd_option_t	stats_upd_option);
					/*!< in: whether to (re) calc
					the stats or to fetch them from
					the persistent storage */

/*********************************************************************//**
Removes the information for a particular index's stats from the persistent
storage if it exists and if there is data stored for this index.
This function creates its own trx and commits it.
@return DB_SUCCESS or error code */
dberr_t
dict_stats_drop_index(
/*==================*/
	const char*	tname,	/*!< in: table name */
	const char*	iname,	/*!< in: index name */
	char*		errstr, /*!< out: error message if != DB_SUCCESS
				is returned */
	ulint		errstr_sz);/*!< in: size of the errstr buffer */

/*********************************************************************//**
Removes the statistics for a table and all of its indexes from the
persistent storage if it exists and if there is data stored for the table.
This function creates its own transaction and commits it.
@return DB_SUCCESS or error code */
dberr_t
dict_stats_drop_table(
/*==================*/
	const char*	table_name,	/*!< in: table name */
	char*		errstr,		/*!< out: error message
					if != DB_SUCCESS is returned */
	ulint		errstr_sz);	/*!< in: size of errstr buffer */

/*********************************************************************//**
Fetches or calculates new estimates for index statistics. */
void
dict_stats_update_for_index(
/*========================*/
	dict_index_t*	index);	/*!< in/out: index */

/*********************************************************************//**
Renames a table in InnoDB persistent stats storage.
This function creates its own transaction and commits it.
@return DB_SUCCESS or error code */
dberr_t
dict_stats_rename_table(
/*====================*/
	const char*	old_name,	/*!< in: old table name */
	const char*	new_name,	/*!< in: new table name */
	char*		errstr,		/*!< out: error string if != DB_SUCCESS
					is returned */
	size_t		errstr_sz);	/*!< in: errstr size */

/*********************************************************************//**
Renames an index in InnoDB persistent stats storage.
This function creates its own transaction and commits it.
@return DB_SUCCESS or error code. DB_STATS_DO_NOT_EXIST will be returned
if the persistent stats do not exist. */
dberr_t
dict_stats_rename_index(
/*====================*/
	const dict_table_t*	table,		/*!< in: table whose index
						is renamed */
	const char*		old_index_name,	/*!< in: old index name */
	const char*		new_index_name)	/*!< in: new index name */
	MY_ATTRIBUTE((warn_unused_result));

/** Evict the stats tables if they loaded in tablespace cache and also
close the stats .ibd files. We have to close stats tables because
8.0 stats tables will use the same name. We load the stats from 5.7
with a suffix "_backup57" and migrate the statistics. */
void
dict_stats_evict_tablespaces();

/** Represent the record of innodb_table_stats table. */
class TableStatsRecord {

public:
	/** Constructor. */
	TableStatsRecord();

	/** Destructor. */
	~TableStatsRecord();

	/** Set the data for the innodb_table_stats record.
	@param[in]	data		data to be set in the record
	@param[in]	col_offset	column offset
	@param[in]	len		length of the data. */
	void set_data(const byte* data, ulint col_offset, ulint len);

	/** Get the table name from innodb_table_stats record.
	@retval table name of the table_stats record. */
	char* get_tbl_name() const;

	/** Set the table name for the innodb_table_stats record.
	@param[in]	data	data to be set in the record
	@param[in]	len	length of the data. */
	void set_tbl_name(const byte* data, ulint len);

	/** Get the db name from the innodb_table_stats record.
	@retval db name of the table stats record. */
	char* get_db_name() const;

	/** Set the db name for the innodb_table_stats record.
	@param[in]	data	data to be set
	@param[in]	len	length of the data. */
	void set_db_name(const byte* data, ulint len);

	/** Get the n_rows from the innodb_table_stats record.
	@retval n_rows from the record. */
	ib_uint64_t get_n_rows() const;

	/** Set the n_rows for the innodb_table_stats record.
	@param[in]	no_of_rows	number of rows. */
	void set_n_rows(ib_uint64_t no_of_rows);

	/** Get the clustered index size from
	innodb_table_stats record.
	@retval size of the clustered index. */
	ulint get_clustered_index_size() const;

	/** Set the clustered index size for the
	innodb_table_stats record.
	@param[in]	clust_size	clustered index size. */
	void set_clustered_index_size(ulint clust_size);

	/** Get the sum of other index size.
	@retval sum of secondary index size. */
	ulint get_sum_of_other_index_size() const;

	/** Set the sum of sec index size.
	@param[in]	sum_of_other_index_size	sum of secondary index size. */
	void set_sum_of_other_index_size(ulint sum_of_other_index_size);

	/** Column number of innodb_table_stats.database_name. */
	static constexpr unsigned	DB_NAME_COL_NO = 0;
	/** Column number of innodb_table_stats.table_name. */
	static constexpr unsigned	TABLE_NAME_COL_NO = 1;
	/** Column number of innodb_table_stats.n_rows. */
	static constexpr unsigned	N_ROWS_COL_NO = 3;
	/** Column number of innodb_table_stats.clustered_index_size. */
	static constexpr unsigned	CLUST_INDEX_SIZE_COL_NO = 4;
	/** Column number of innodb_table_stats.sum_of_other_index_sizes. */
	static constexpr unsigned	SUM_OF_OTHER_INDEX_SIZE_COL_NO = 5;

private:
	/** Database name. */
	char*		m_db_name;
	/** Table name. */
	char*		m_tbl_name;
	/** Number of rows. */
	ib_uint64_t	m_n_rows;
	/** Clustered index size. */
	ulint		m_clustered_index_size;
	/** Sum of other index size. */
	ulint		m_sum_of_other_index_sizes;
	/** Heap to store db_name, tbl_name for the record. */
	mem_heap_t*	m_heap;
};

#include "dict0stats.ic"

#ifdef UNIV_ENABLE_UNIT_TEST_DICT_STATS
void test_dict_stats_all();
#endif /* UNIV_ENABLE_UNIT_TEST_DICT_STATS */

#endif /* dict0stats_h */
