/*****************************************************************************

Copyright (c) 2005, 2013, Oracle and/or its affiliates. All Rights Reserved.

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
@file include/handler0alter.h
Smart ALTER TABLE
*******************************************************/

/*************************************************************//**
Copies an InnoDB record to table->record[0]. */
UNIV_INTERN
void
innobase_rec_to_mysql(
/*==================*/
	struct TABLE*		table,	/*!< in/out: MySQL table */
	const rec_t*		rec,	/*!< in: record */
	const dict_index_t*	index,	/*!< in: index */
	const ulint*		offsets)/*!< in: rec_get_offsets(
					rec, index, ...) */
	__attribute__((nonnull));

/*************************************************************//**
Copies an InnoDB index entry to table->record[0]. */
UNIV_INTERN
void
innobase_fields_to_mysql(
/*=====================*/
	struct TABLE*		table,	/*!< in/out: MySQL table */
	const dict_index_t*	index,	/*!< in: InnoDB index */
	const dfield_t*		fields)	/*!< in: InnoDB index fields */
	__attribute__((nonnull));

/*************************************************************//**
Copies an InnoDB row to table->record[0]. */
UNIV_INTERN
void
innobase_row_to_mysql(
/*==================*/
	struct TABLE*		table,	/*!< in/out: MySQL table */
	const dict_table_t*	itab,	/*!< in: InnoDB table */
	const dtuple_t*		row)	/*!< in: InnoDB row */
	__attribute__((nonnull));

/*************************************************************//**
Resets table->record[0]. */
UNIV_INTERN
void
innobase_rec_reset(
/*===============*/
	struct TABLE*		table)		/*!< in/out: MySQL table */
	__attribute__((nonnull));

/** Generate the next autoinc based on a snapshot of the session
auto_increment_increment and auto_increment_offset variables. */
struct ib_sequence_t {

	/**
	@param thd - the session
	@param start_value - the lower bound
	@param max_value - the upper bound (inclusive) */
	ib_sequence_t(THD* thd, ulonglong start_value, ulonglong max_value);

	/**
	Postfix increment
	@return the value to insert */
	ulonglong operator++(int) UNIV_NOTHROW;

	/** Check if the autoinc "sequence" is exhausted.
	@return true if the sequence is exhausted */
	bool eof() const UNIV_NOTHROW
	{
		return(m_eof);
	}

	/**
	@return the next value in the sequence */
	ulonglong last() const UNIV_NOTHROW
	{
		ut_ad(m_next_value > 0);

		return(m_next_value);
	}

	/** Maximum calumn value if adding an AUTOINC column else 0. Once
	we reach the end of the sequence it will be set to ~0. */
	const ulonglong	m_max_value;

	/** Value of auto_increment_increment */
	ulong		m_increment;

	/** Value of auto_increment_offset */
	ulong		m_offset;

	/** Next value in the sequence */
	ulonglong	m_next_value;

	/** true if no more values left in the sequence */
	bool		m_eof;
};
