/*****************************************************************************

Copyright (c) 2014, 2014, Oracle and/or its affiliates. All Rights Reserved.

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
@file ut/ut0stage.h
Supplementary code to performance schema stage instrumentation.

Created Nov 12, 2014 Vasil Dimov
*******************************************************/

#ifndef ut0stage_h
#define ut0stage_h

#include <algorithm>

#include "my_global.h" /* needed for headers from mysql/psi/ */

#include "mysql/psi/mysql_stage.h" /* mysql_stage_inc_work_completed */
#include "mysql/psi/psi.h" /* HAVE_PSI_STAGE_INTERFACE, PSI_stage_progress */

#include "univ.i"

#include "dict0mem.h" /* dict_index_t */
#include "row0log.h" /* row_log_estimate_work() */
#include "srv0srv.h" /* ut_stage_alter_t */

#ifdef HAVE_PSI_STAGE_INTERFACE

/** Change the current stage to a new one and keep the "work completed"
and "work estimated" numbers.
@param[in,out]	progress	progress whose stage to change
@param[in]	new_stage	new stage to be set
*/
inline
void
ut_stage_change(
	PSI_stage_progress**	progress,
	const PSI_stage_info*	new_stage)
{
	if (*progress == NULL) {
		return;
	}

	const ulonglong	completed = mysql_stage_get_work_completed(*progress);
	const ulonglong	estimated = mysql_stage_get_work_estimated(*progress);

	*progress = mysql_set_stage(new_stage->m_key);

	if (*progress == NULL) {
		return;
	}

	mysql_stage_set_work_completed(*progress, completed);
	mysql_stage_set_work_estimated(*progress, estimated);
}

class ut_stage_alter_t {
public:
	explicit
	ut_stage_alter_t(
		const dict_index_t*	pk)
		:
		m_pk(pk),
		m_n_pk_recs(0),
		m_n_pk_pages(0),
		m_n_recs_processed(0),
		m_last_estimate_of_log(0),
		m_n_flush_pages(0),
		m_cur_phase(NOT_STARTED)
	{
	}

	~ut_stage_alter_t()
	{
		/* Set completed = estimated before we quit. */
		mysql_stage_set_work_completed(
			m_progress,
			mysql_stage_get_work_estimated(m_progress));

		mysql_end_stage();
	}

	/** Flag an ALTER TABLE start.
	@param[in]	n_sort_indexes	number of indexes that will be sorted
	during ALTER TABLE, used for estimating the total work to be done
	*/
	void
	begin(
		ulint	n_sort_indexes)
	{
		m_n_sort_indexes = n_sort_indexes;

		m_cur_phase = READ_PK;

		m_progress = mysql_set_stage(
			srv_stage_alter_table_read_pk.m_key);

		mysql_stage_set_work_completed(m_progress, 0);

		update_estimate();
	}

	/** Increment the number of records in PK (table) with 1. */
	void
	n_pk_recs_inc()
	{
		m_n_pk_recs++;
	}

	void
	set_n_flush_pages(
		ulint	n_flush_pages)
	{
		m_n_flush_pages = n_flush_pages;

		update_estimate();
	}

	void
	read_pk_completed()
	{
		update_estimate();

		if (m_n_pk_pages == 0) {
			m_n_recs_per_page = 1;
		} else {
			m_n_recs_per_page = std::max(
				1UL, m_n_pk_recs / m_n_pk_pages);
		}
	}

	void
	one_page_was_processed(
		ulint	inc_val = 1)
	{
		if (m_progress == NULL) {
			return;
		}

		if (m_cur_phase == READ_PK) {
			m_n_pk_pages++;
		}

		update_estimate();

		mysql_stage_inc_work_completed(m_progress, inc_val);
	}

	void
	one_rec_was_processed()
	{
		if (m_progress == NULL) {
			return;
		}

		ulint	multi_factor;

		switch (m_cur_phase) {
		case READ_PK:
			m_n_pk_recs++;
			return;
		case SORT:
			multi_factor = m_sort_multi_factor;
			break;
		default:
			multi_factor = 1;
		}

		ulint	inc_every_nth_rec = m_n_recs_per_page * multi_factor;

		if (m_n_recs_processed++ % inc_every_nth_rec == 0) {
			one_page_was_processed();
		}
	}

	void
	begin_phase_sort(
		ulint	sort_multi_factor)
	{
		m_sort_multi_factor = std::max(1UL, sort_multi_factor);

		change(&srv_stage_alter_table_sort);
	}

	void
	begin_phase_insert()
	{
		change(&srv_stage_alter_table_insert);
	}

	void
	begin_phase_flush()
	{
		change(&srv_stage_alter_table_flush);
	}

	void
	begin_phase_log()
	{
		change(&srv_stage_alter_table_log);
	}

	void
	begin_phase_end()
	{
		change(&srv_stage_alter_table_end);
	}

private:

	void
	update_estimate()
	{
		if (m_progress == NULL) {
			return;
		}

		if (m_cur_phase == LOG) {
			mysql_stage_set_work_estimated(
				m_progress,
				mysql_stage_get_work_completed(m_progress)
				+ row_log_estimate_work(m_pk));
			return;
		}

		const ulint	n_pk_pages
			= m_n_pk_pages != 0
			? m_n_pk_pages
			: m_pk->stat_n_leaf_pages;

		if (m_n_flush_pages == 0) {
			m_n_flush_pages = n_pk_pages;
		}

		const ulonglong	estimate
			= n_pk_pages
			* (1 /* read PK */
			   /* sort & insert per created index */
			   + m_n_sort_indexes * 2)
			+ m_n_flush_pages
			+ row_log_estimate_work(m_pk);

		mysql_stage_set_work_estimated(m_progress, estimate);
	}

	void
	change(
		const PSI_stage_info*	new_stage)
	{
		if (new_stage == &srv_stage_alter_table_read_pk) {
			m_cur_phase = READ_PK;
		} else if (new_stage == &srv_stage_alter_table_sort) {
			m_cur_phase = SORT;
		} else if (new_stage == &srv_stage_alter_table_insert) {
			m_cur_phase = INSERT;
		} else if (new_stage == &srv_stage_alter_table_flush) {
			m_cur_phase = FLUSH;
		} else if (new_stage == &srv_stage_alter_table_log) {
			m_cur_phase = LOG;
		} else {
			m_cur_phase = OTHER;
		}

		ut_stage_change(&m_progress, new_stage);
	}

	PSI_stage_progress*	m_progress;

	const dict_index_t*	m_pk;

	/** Number of records in the primary key (table). */
	ulint			m_n_pk_recs;

	/** Number of leaf pages in the primary key. */
	ulint			m_n_pk_pages;

	ulint			m_n_recs_per_page;

	ulint			m_n_sort_indexes;

	ulint			m_sort_multi_factor;

	ulint			m_n_recs_processed;

	ulint			m_last_estimate_of_log;

	ulint			m_n_flush_pages;

	enum {
		NOT_STARTED = 0,
		READ_PK = 1,
		SORT = 2,
		INSERT = 3,
		FLUSH = 4,
		LOG = 5,
		OTHER = 6,
	} 			m_cur_phase;
};

#else /* HAVE_PSI_STAGE_INTERFACE */

class ut_stage_alter_t {
public:
	explicit
	ut_stage_alter_t(
		const dict_index_t*	pk)
	{
	}

	~ut_stage_alter_t()
	{
	}

	void
	begin(
		ulint	n_sort_indexes)
	{
	}

	void
	n_pk_recs_inc()
	{
	}

	void
	set_n_flush_pages(
		ulint	n_flush_pages)
	{
	}

	void
	read_pk_completed()
	{
	}

	void
	one_page_was_processed(
		ulint	inc_val = 1)
	{
	}

	void
	one_rec_was_processed()
	{
	}

	void
	begin_phase_sort(
		ulint	sort_multi_factor)
	{
	}

	void
	begin_phase_insert()
	{
	}

	void
	begin_phase_flush()
	{
	}

	void
	begin_phase_log()
	{
	}

	void
	begin_phase_end()
	{
	}
};

#endif /* HAVE_PSI_STAGE_INTERFACE */

#endif /* ut0stage_h */
