/*****************************************************************************

Copyright (c) 1996, 2017, Oracle and/or its affiliates. All Rights Reserved.

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
@file trx/trx0sys.cc
Transaction system

Created 3/26/1996 Heikki Tuuri
*******************************************************/

#include <sys/types.h>
#include <new>

#include "current_thd.h"
#include "ha_prototypes.h"
#include "sql_error.h"
#include "trx0sys.h"

#ifndef UNIV_HOTBACKUP
#include "fsp0fsp.h"
#include "fsp0sysspace.h"
#include "log0log.h"
#include "log0recv.h"
#include "mtr0log.h"
#include "mtr0log.h"
#include "os0file.h"
#include "read0read.h"
#include "srv0srv.h"
#include "srv0start.h"
#include "trx0purge.h"
#include "trx0rseg.h"
#include "trx0trx.h"
#include "trx0undo.h"

/** The transaction system */
trx_sys_t*		trx_sys		= NULL;
#endif /* !UNIV_HOTBACKUP */

/** Check whether transaction id is valid.
@param[in]	id              transaction id to check
@param[in]      name            table name */
void
ReadView::check_trx_id_sanity(
	trx_id_t		id,
	const table_name_t&	name)
{
	if (&name == &dict_sys->dynamic_metadata->name) {
		/* The table mysql.innodb_dynamic_metadata uses a
		constant DB_TRX_ID=~0. */
		ut_ad(id == (1ULL << 48) - 1);
		return;
	}

	if (id >= trx_sys->max_trx_id) {

		ib::warn() << "A transaction id"
			   << " in a record of table "
			   << name
			   << " is newer than the"
			   << " system-wide maximum.";
		ut_ad(0);
		THD *thd = current_thd;
		if (thd != NULL) {
			char    table_name[MAX_FULL_NAME_LEN + 1];

			innobase_format_name(
				table_name, sizeof(table_name),
				name.m_name);

			push_warning_printf(thd, Sql_condition::SL_WARNING,
					    ER_SIGNAL_WARN,
					    "InnoDB: Transaction id"
					    " in a record of table"
					    " %s is newer than system-wide"
					    " maximum.", table_name);
		}
	}
}

#ifndef UNIV_HOTBACKUP
# ifdef UNIV_DEBUG
/* Flag to control TRX_RSEG_N_SLOTS behavior debugging. */
uint	trx_rseg_n_slots_debug = 0;
# endif /* UNIV_DEBUG */

/*****************************************************************//**
Writes the value of max_trx_id to the file based trx system header. */
void
trx_sys_flush_max_trx_id(void)
/*==========================*/
{
	mtr_t		mtr;
	trx_sysf_t*	sys_header;

	ut_ad(trx_sys_mutex_own());

	if (!srv_read_only_mode) {
		mtr_start(&mtr);

		sys_header = trx_sysf_get(&mtr);

		mlog_write_ull(
			sys_header + TRX_SYS_TRX_ID_STORE,
			trx_sys->max_trx_id, &mtr);

		mtr_commit(&mtr);
	}
}

/*****************************************************************//**
Updates the offset information about the end of the MySQL binlog entry
which corresponds to the transaction just being committed. In a MySQL
replication slave updates the latest master binlog position up to which
replication has proceeded. */
void
trx_sys_update_mysql_binlog_offset(
/*===============================*/
	const char*	file_name,/*!< in: MySQL log file name */
	int64_t		offset,	/*!< in: position in that log file */
	ulint		field,	/*!< in: offset of the MySQL log info field in
				the trx sys header */
	mtr_t*		mtr)	/*!< in: mtr */
{
	trx_sysf_t*	sys_header;

	if (ut_strlen(file_name) >= TRX_SYS_MYSQL_LOG_NAME_LEN) {

		/* We cannot fit the name to the 512 bytes we have reserved */

		return;
	}

	sys_header = trx_sysf_get(mtr);

	if (mach_read_from_4(sys_header + field
			     + TRX_SYS_MYSQL_LOG_MAGIC_N_FLD)
	    != TRX_SYS_MYSQL_LOG_MAGIC_N) {

		mlog_write_ulint(sys_header + field
				 + TRX_SYS_MYSQL_LOG_MAGIC_N_FLD,
				 TRX_SYS_MYSQL_LOG_MAGIC_N,
				 MLOG_4BYTES, mtr);
	}

	if (0 != strcmp((char*) (sys_header + field + TRX_SYS_MYSQL_LOG_NAME),
			file_name)) {

		mlog_write_string(sys_header + field
				  + TRX_SYS_MYSQL_LOG_NAME,
				  (byte*) file_name, 1 + ut_strlen(file_name),
				  mtr);
	}

	if (mach_read_from_4(sys_header + field
			     + TRX_SYS_MYSQL_LOG_OFFSET_HIGH) > 0
	    || (offset >> 32) > 0) {

		mlog_write_ulint(sys_header + field
				 + TRX_SYS_MYSQL_LOG_OFFSET_HIGH,
				 (ulint)(offset >> 32),
				 MLOG_4BYTES, mtr);
	}

	mlog_write_ulint(sys_header + field
			 + TRX_SYS_MYSQL_LOG_OFFSET_LOW,
			 (ulint)(offset & 0xFFFFFFFFUL),
			 MLOG_4BYTES, mtr);
}

/*****************************************************************//**
Stores the MySQL binlog offset info in the trx system header if
the magic number shows it valid, and print the info to stderr */
void
trx_sys_print_mysql_binlog_offset(void)
/*===================================*/
{
	trx_sysf_t*	sys_header;
	mtr_t		mtr;
	ulint		trx_sys_mysql_bin_log_pos_high;
	ulint		trx_sys_mysql_bin_log_pos_low;

	mtr_start(&mtr);

	sys_header = trx_sysf_get(&mtr);

	if (mach_read_from_4(sys_header + TRX_SYS_MYSQL_LOG_INFO
			     + TRX_SYS_MYSQL_LOG_MAGIC_N_FLD)
	    != TRX_SYS_MYSQL_LOG_MAGIC_N) {

		mtr_commit(&mtr);

		return;
	}

	trx_sys_mysql_bin_log_pos_high = mach_read_from_4(
		sys_header + TRX_SYS_MYSQL_LOG_INFO
		+ TRX_SYS_MYSQL_LOG_OFFSET_HIGH);
	trx_sys_mysql_bin_log_pos_low = mach_read_from_4(
		sys_header + TRX_SYS_MYSQL_LOG_INFO
		+ TRX_SYS_MYSQL_LOG_OFFSET_LOW);

	ib::info() << "Last MySQL binlog file position "
		<< trx_sys_mysql_bin_log_pos_high << " "
		<< trx_sys_mysql_bin_log_pos_low << ", file name "
		<< sys_header + TRX_SYS_MYSQL_LOG_INFO
		+ TRX_SYS_MYSQL_LOG_NAME;

	mtr_commit(&mtr);
}

/** Look for a free slot for a rollback segment in the trx system file copy.
@param[in,out]	mtr		mtr
@return slot index or ULINT_UNDEFINED if not found */
ulint
trx_sysf_rseg_find_free(mtr_t*	mtr)
{
	trx_sysf_t*	sys_header = trx_sysf_get(mtr);

	for (ulint slot_no = 0; slot_no < TRX_SYS_N_RSEGS; slot_no++) {
		page_no_t	page_no
			= trx_sysf_rseg_get_page_no(sys_header, slot_no, mtr);

		if (page_no == FIL_NULL) {
			return(slot_no);
		}
	}

	return(ULINT_UNDEFINED);
}

/** Look for used slots for redo rollback segment.
We assume there may be gaps in the array.
@param[in,out]	mtr		mtr
@return number of used slots */
static
ulint
trx_sysf_used_slots_for_redo_rseg(
	mtr_t*	mtr)
{
	trx_sysf_t*	sys_header;
	ulint		n_used = 0;

	sys_header = trx_sysf_get(mtr);

	for (ulint i = 0; i < TRX_SYS_N_RSEGS; i++) {

		ulint	page_no;

		page_no = trx_sysf_rseg_get_page_no(sys_header, i, mtr);

		if (page_no != FIL_NULL) {
			++n_used;
		}
	}

	return(n_used);
}

/*****************************************************************//**
Creates the file page for the transaction system. This function is called only
at the database creation, before trx_sys_init. */
static
void
trx_sysf_create(
/*============*/
	mtr_t*	mtr)	/*!< in: mtr */
{
	trx_sysf_t*	sys_header;
	ulint		slot_no;
	buf_block_t*	block;
	page_t*		page;
	ulint		page_no;
	byte*		ptr;
	ulint		len;

	ut_ad(mtr);

	/* Note that below we first reserve the file space x-latch, and
	then enter the kernel: we must do it in this order to conform
	to the latching order rules. */

	mtr_x_lock_space(fil_space_get_sys_space(), mtr);

	/* Create the trx sys file block in a new allocated file segment */
	block = fseg_create(TRX_SYS_SPACE, 0, TRX_SYS + TRX_SYS_FSEG_HEADER,
			    mtr);
	buf_block_dbg_add_level(block, SYNC_TRX_SYS_HEADER);

	ut_a(block->page.id.page_no() == TRX_SYS_PAGE_NO);

	page = buf_block_get_frame(block);

	mlog_write_ulint(page + FIL_PAGE_TYPE, FIL_PAGE_TYPE_TRX_SYS,
			 MLOG_2BYTES, mtr);

	/* Reset the doublewrite buffer magic number to zero so that we
	know that the doublewrite buffer has not yet been created (this
	suppresses a Valgrind warning) */

	mlog_write_ulint(page + TRX_SYS_DOUBLEWRITE
			 + TRX_SYS_DOUBLEWRITE_MAGIC, 0, MLOG_4BYTES, mtr);

	sys_header = trx_sysf_get(mtr);

	/* Start counting transaction ids from number 1 up */
	mach_write_to_8(sys_header + TRX_SYS_TRX_ID_STORE, 1);

	/* Reset the rollback segment slots.  Old versions of InnoDB
	define TRX_SYS_N_RSEGS as 256 (TRX_SYS_OLD_N_RSEGS) and expect
	that the whole array is initialized. */
	ptr = TRX_SYS_RSEGS + sys_header;
	len = ut_max(TRX_SYS_OLD_N_RSEGS, TRX_SYS_N_RSEGS)
		* TRX_SYS_RSEG_SLOT_SIZE;
	memset(ptr, 0xff, len);
	ptr += len;
	ut_a(ptr <= page + (UNIV_PAGE_SIZE - FIL_PAGE_DATA_END));

	/* Initialize all of the page.  This part used to be uninitialized. */
	memset(ptr, 0, UNIV_PAGE_SIZE - FIL_PAGE_DATA_END + page - ptr);

	mlog_log_string(sys_header, UNIV_PAGE_SIZE - FIL_PAGE_DATA_END
			+ page - sys_header, mtr);

	/* Create the first rollback segment in the SYSTEM tablespace */
	slot_no = trx_sysf_rseg_find_free(mtr);
	page_no = trx_rseg_header_create(TRX_SYS_SPACE, univ_page_size,
					 PAGE_NO_MAX, slot_no, mtr);

	ut_a(slot_no == TRX_SYS_SYSTEM_RSEG_ID);
	ut_a(page_no == FSP_FIRST_RSEG_PAGE_NO);
}

/*****************************************************************//**
Creates and initializes the central memory structures for the transaction
system. This is called when the database is started.
@return min binary heap of rsegs to purge */
purge_pq_t*
trx_sys_init_at_db_start(void)
/*==========================*/
{
	purge_pq_t*	purge_queue;
	trx_sysf_t*	sys_header;
	ib_uint64_t	rows_to_undo	= 0;
	const char*	unit		= "";

	/* We create the min binary heap here and pass ownership to
	purge when we init the purge sub-system. Purge is responsible
	for freeing the binary heap. */
	purge_queue = UT_NEW_NOKEY(purge_pq_t());
	ut_a(purge_queue != NULL);

	if (srv_force_recovery < SRV_FORCE_NO_UNDO_LOG_SCAN) {
		/* Create the memory objects for all the rollback segments
		referred to in the TRX_SYS page. */
		trx_sys_rsegs_init(purge_queue);
	}

	/* VERY important: after the database is started, max_trx_id value is
	divisible by TRX_SYS_TRX_ID_WRITE_MARGIN, and the 'if' in
	trx_sys_get_new_trx_id will evaluate to TRUE when the function
	is first time called, and the value for trx id will be written
	to the disk-based header! Thus trx id values will not overlap when
	the database is repeatedly started! */

	mtr_t	mtr;
	mtr.start();

	sys_header = trx_sysf_get(&mtr);

	trx_sys->max_trx_id = 2 * TRX_SYS_TRX_ID_WRITE_MARGIN
		+ ut_uint64_align_up(mach_read_from_8(sys_header
						   + TRX_SYS_TRX_ID_STORE),
				     TRX_SYS_TRX_ID_WRITE_MARGIN);

	mtr.commit();
	ut_d(trx_sys->rw_max_trx_id = trx_sys->max_trx_id);

	trx_dummy_sess = sess_open();

	trx_lists_init_at_db_start();

	/* This mutex is not strictly required, it is here only to satisfy
	the debug code (assertions). We are still running in single threaded
	bootstrap mode. */

	trx_sys_mutex_enter();

	if (UT_LIST_GET_LEN(trx_sys->rw_trx_list) > 0) {
		const trx_t*	trx;

		for (trx = UT_LIST_GET_FIRST(trx_sys->rw_trx_list);
		     trx != NULL;
		     trx = UT_LIST_GET_NEXT(trx_list, trx)) {

			ut_ad(trx->is_recovered);
			assert_trx_in_rw_list(trx);

			if (trx_state_eq(trx, TRX_STATE_ACTIVE)) {
				rows_to_undo += trx->undo_no;
			}
		}

		if (rows_to_undo > 1000000000) {
			unit = "M";
			rows_to_undo = rows_to_undo / 1000000;
		}

		ib::info() << UT_LIST_GET_LEN(trx_sys->rw_trx_list)
			<< " transaction(s) which must be rolled back or"
			" cleaned up in total " << rows_to_undo << unit
			<< " row operations to undo";

		ib::info() << "Trx id counter is " << trx_sys->max_trx_id;
	}

	trx_sys_mutex_exit();

	return(purge_queue);
}

/*****************************************************************//**
Creates the trx_sys instance and initializes purge_queue and mutex. */
void
trx_sys_create(void)
/*================*/
{
	ut_ad(trx_sys == NULL);

	trx_sys = static_cast<trx_sys_t*>(ut_zalloc_nokey(sizeof(*trx_sys)));

	mutex_create(LATCH_ID_TRX_SYS, &trx_sys->mutex);

	UT_LIST_INIT(trx_sys->serialisation_list, &trx_t::no_list);
	UT_LIST_INIT(trx_sys->rw_trx_list, &trx_t::trx_list);
	UT_LIST_INIT(trx_sys->mysql_trx_list, &trx_t::mysql_trx_list);

	trx_sys->mvcc = UT_NEW_NOKEY(MVCC(1024));

	new(&trx_sys->rw_trx_ids) trx_ids_t(ut_allocator<trx_id_t>(
			mem_key_trx_sys_t_rw_trx_ids));

	new(&trx_sys->rw_trx_set) TrxIdSet();

	new(&trx_sys->rsegs) Rsegs(ut_allocator<trx_rseg_t*>(
			mem_key_trx_sys_t_rsegs));

	new(&trx_sys->tmp_rsegs) Rsegs(ut_allocator<trx_rseg_t*>(
			mem_key_trx_sys_t_rsegs));

}

/*****************************************************************//**
Creates and initializes the transaction system at the database creation. */
void
trx_sys_create_sys_pages(void)
/*==========================*/
{
	mtr_t	mtr;

	mtr_start(&mtr);

	trx_sysf_create(&mtr);

	mtr_commit(&mtr);
}

/** Create non-redo rollback segments residing in the temp-tablespace.
Non-redo rollback segments don't perform redo logging and so they are
used for undo logging of objects/tables that don't need to be recovered
after a crash. Non-Redo rollback segments are created on every server
startup.
@return number of non-redo rollback segments created. */
ulint
trx_rsegs_create_in_temp_space()
{
	ulint n_created = 0;

	for (ulint rseg_id = 0; rseg_id < srv_tmp_rollback_segments; rseg_id++) {
		if (trx_rseg_create_in_temp_space(rseg_id) == NULL) {
			break;
		}
		++n_created;
	}

	ib::info() << n_created
		<< " rollback segment(s) are active in the temporary tablespace.";

	return(n_created);
}

/** Create any more rollback segments above what was previously built
in the system tablespace. During create_new_db, only one rollback segment
was created initially so that the system tablespace would be backward
compatible in its file segment ordering.
When opening an existing db, the setting for srv_rollback_segments
may have changed. If the TRX_SYS page does not track enough rollback
segments, we will create the rest here.
@return true if successful or not done, false for failure */
bool
trx_sys_create_additional_rsegs(bool needed_recovery)
{
	mtr_t		mtr;
	ulint		initial_rsegs;
	ulint		new_rsegs = 0;
	ulint		rseg_id;
	trx_rseg_t*	rseg;

	ut_ad(srv_rollback_segments <= TRX_SYS_N_RSEGS);

	if (trx_sys->rsegs.size() >= srv_rollback_segments) {
		srv_available_rollback_segments = trx_sys->rsegs.size();
		return(true);
	}

	/* This is executed in single-threaded mode therefore it is not
	necessary to use the same mtr in trx_rseg_create(). */
	mtr.start();
	initial_rsegs = trx_sysf_used_slots_for_redo_rseg(&mtr);
	mtr.commit();

	ib::info() << initial_rsegs << " durable rollback segment(s)"
		<< " found in the system tablespace.";

	if (srv_read_only_mode || srv_force_recovery || needed_recovery) {
		/* Set this global variable so it can be monitored. */
		srv_available_rollback_segments = trx_sys->rsegs.size();

		if (srv_available_rollback_segments < srv_rollback_segments) {
			bool and2 = srv_read_only_mode && srv_force_recovery;
			bool and3 = (srv_read_only_mode || srv_force_recovery)
				    && needed_recovery;

			ib::info() << "Did not create all "
				<< srv_rollback_segments
				<< " rollback segments because"
				<< (srv_read_only_mode ?
					" read-only mode is set" : "")
				<< (and2 ? " and" : "")
				<< (srv_force_recovery > 0 ?
					" innodb_force_recovery is set": "")
				<< (and3 ? " and" : "")
				<< (needed_recovery ?
					" the server needed recovery" : "")
				<< ". Only "
				<< srv_available_rollback_segments
				<< " are available.";
		}

		/* We do not proceed to add any more rsegs under these
		three conditions.  Just use what we have. */
		return(true);
	}

	/* These initial rsegs are in the system space and should
	have already been loaded. */
	ut_ad(initial_rsegs == trx_sys->rsegs.size());
	ut_ad(initial_rsegs < srv_rollback_segments);

	new_rsegs = srv_rollback_segments - initial_rsegs;

	ib::info() << " Adding " << new_rsegs
		<< " more rollback segments.";

	for (ulint i = 0; i < new_rsegs; i++) {

		mtr.start();
		rseg_id = trx_sysf_rseg_find_free(&mtr);
		mtr.commit();

		/* Besides the initial rollback segment created in the
		system tablespace, the rest will be created in the
		external undo tablespaces in a round-robin fashion,
		if they exist.  If not, they will be created in the
		system tablespace. */
		space_id_t	space_id = 0;
		if (srv_undo_tablespaces > 0) {
			space_id = trx_sys_undo_spaces->at(
				i % srv_undo_tablespaces);
		}

		rseg = trx_rseg_create(space_id, rseg_id);
		if (rseg != nullptr) {
			trx_sys->rsegs.push_back(rseg);
		}
	}

	/* Set this global variable so it can be monitored. */
	srv_available_rollback_segments = trx_sys->rsegs.size();

	if (srv_available_rollback_segments < srv_rollback_segments) {
		ib::error() << "Could not create all "
			<< srv_rollback_segments
			<< " rollback segments. Only "
			<< srv_available_rollback_segments
			<< " are available."
			" The disk may be running out of space";
		return(false);
	}

	return(true);
}

#else /* !UNIV_HOTBACKUP */
/*****************************************************************//**
Prints to stderr the MySQL binlog info in the system header if the
magic number shows it valid. */
void
trx_sys_print_mysql_binlog_offset_from_page(
/*========================================*/
	const byte*	page)	/*!< in: buffer containing the trx
				system header page, i.e., page number
				TRX_SYS_PAGE_NO in the tablespace */
{
	const trx_sysf_t*	sys_header;

	sys_header = page + TRX_SYS;

	if (mach_read_from_4(sys_header + TRX_SYS_MYSQL_LOG_INFO
			     + TRX_SYS_MYSQL_LOG_MAGIC_N_FLD)
	    == TRX_SYS_MYSQL_LOG_MAGIC_N) {

		ib::info() << "mysqlbackup: Last MySQL binlog file position "
			<< mach_read_from_4(
				sys_header + TRX_SYS_MYSQL_LOG_INFO
				+ TRX_SYS_MYSQL_LOG_OFFSET_HIGH) << " "
			<< mach_read_from_4(
				sys_header + TRX_SYS_MYSQL_LOG_INFO
				+ TRX_SYS_MYSQL_LOG_OFFSET_LOW)
			<< ", file name " << sys_header
			+ TRX_SYS_MYSQL_LOG_INFO + TRX_SYS_MYSQL_LOG_NAME;
	}
}
#endif /* !UNIV_HOTBACKUP */

#ifndef UNIV_HOTBACKUP
/*********************************************************************
Shutdown/Close the transaction system. */
void
trx_sys_close(void)
/*===============*/
{
	ut_ad(srv_shutdown_state == SRV_SHUTDOWN_EXIT_THREADS);

	if (trx_sys == NULL) {
		return;
	}

	ulint	size = trx_sys->mvcc->size();

	if (size > 0) {
		ib::error() << "All read views were not closed before"
			" shutdown: " << size << " read views open";
	}

	sess_close(trx_dummy_sess);
	trx_dummy_sess = NULL;

	trx_purge_sys_close();

	/* Free the double write data structures. */
	buf_dblwr_free();

	/* Only prepared transactions may be left in the system. Free them. */
	ut_a(UT_LIST_GET_LEN(trx_sys->rw_trx_list) == trx_sys->n_prepared_trx);

	for (trx_t* trx = UT_LIST_GET_FIRST(trx_sys->rw_trx_list);
	     trx != NULL;
	     trx = UT_LIST_GET_FIRST(trx_sys->rw_trx_list)) {

		trx_free_prepared(trx);

		UT_LIST_REMOVE(trx_sys->rw_trx_list, trx);
	}

	/* There can't be any active transactions. */
	Rseg_Iterator it;
	for (it = trx_sys->rsegs.begin();
	     it != trx_sys->rsegs.end(); ++it) {
		ut_ad(*it != NULL);
		trx_rseg_mem_free(*it);
	}
	trx_sys->rsegs.clear();
	trx_sys->rsegs.~Rsegs();

	for (it = trx_sys->tmp_rsegs.begin();
	     it != trx_sys->tmp_rsegs.end(); ++it) {
		ut_ad(*it != NULL);
		trx_rseg_mem_free(*it);
	}
	trx_sys->tmp_rsegs.clear();
	trx_sys->tmp_rsegs.~Rsegs();

	UT_DELETE(trx_sys->mvcc);

	ut_a(UT_LIST_GET_LEN(trx_sys->rw_trx_list) == 0);
	ut_a(UT_LIST_GET_LEN(trx_sys->mysql_trx_list) == 0);
	ut_a(UT_LIST_GET_LEN(trx_sys->serialisation_list) == 0);

	/* We used placement new to create this mutex. Call the destructor. */
	mutex_free(&trx_sys->mutex);

	trx_sys->rw_trx_ids.~trx_ids_t();

	trx_sys->rw_trx_set.~TrxIdSet();

	ut_free(trx_sys);

	trx_sys = NULL;
}

/** @brief Convert an undo log to TRX_UNDO_PREPARED state on shutdown.

If any prepared ACTIVE transactions exist, and their rollback was
prevented by innodb_force_recovery, we convert these transactions to
XA PREPARE state in the main-memory data structures, so that shutdown
will proceed normally. These transactions will again recover as ACTIVE
on the next restart, and they will be rolled back unless
innodb_force_recovery prevents it again.

@param[in]	trx	transaction
@param[in,out]	undo	undo log to convert to TRX_UNDO_PREPARED */
static
void
trx_undo_fake_prepared(
	const trx_t*	trx,
	trx_undo_t*	undo)
{
	ut_ad(srv_force_recovery >= SRV_FORCE_NO_TRX_UNDO);
	ut_ad(trx_state_eq(trx, TRX_STATE_ACTIVE));
	ut_ad(trx->is_recovered);

	if (undo != NULL) {
		ut_ad(undo->state == TRX_UNDO_ACTIVE);
		undo->state = TRX_UNDO_PREPARED;
	}
}

/*********************************************************************
Check if there are any active (non-prepared) transactions.
@return total number of active transactions or 0 if none */
ulint
trx_sys_any_active_transactions(void)
/*=================================*/
{
	trx_sys_mutex_enter();

	ulint	total_trx = UT_LIST_GET_LEN(trx_sys->mysql_trx_list);

	if (total_trx == 0) {
		total_trx = UT_LIST_GET_LEN(trx_sys->rw_trx_list);
		ut_a(total_trx >= trx_sys->n_prepared_trx);

		if (total_trx > trx_sys->n_prepared_trx
		    && srv_force_recovery >= SRV_FORCE_NO_TRX_UNDO) {
			for (trx_t* trx = UT_LIST_GET_FIRST(
				     trx_sys->rw_trx_list);
			     trx != NULL;
			     trx = UT_LIST_GET_NEXT(trx_list, trx)) {
				if (!trx_state_eq(trx, TRX_STATE_ACTIVE)
				    || !trx->is_recovered) {
					continue;
				}
				/* This was a recovered transaction
				whose rollback was disabled by
				the innodb_force_recovery setting.
				Pretend that it is in XA PREPARE
				state so that shutdown will work. */
				trx_undo_fake_prepared(
					trx, trx->rsegs.m_redo.insert_undo);
				trx_undo_fake_prepared(
					trx, trx->rsegs.m_redo.update_undo);
				trx_undo_fake_prepared(
					trx, trx->rsegs.m_noredo.insert_undo);
				trx_undo_fake_prepared(
					trx, trx->rsegs.m_noredo.update_undo);
				trx->state = TRX_STATE_PREPARED;
				trx_sys->n_prepared_trx++;
			}
		}

		ut_a(total_trx >= trx_sys->n_prepared_trx);
		total_trx -= trx_sys->n_prepared_trx;
	}

	trx_sys_mutex_exit();

	return(total_trx);
}

#ifdef UNIV_DEBUG
/*************************************************************//**
Validate the trx_ut_list_t.
@return true if valid. */
static
bool
trx_sys_validate_trx_list_low(
/*===========================*/
	trx_ut_list_t*	trx_list)	/*!< in: &trx_sys->rw_trx_list */
{
	const trx_t*	trx;
	const trx_t*	prev_trx = NULL;

	ut_ad(trx_sys_mutex_own());

	ut_ad(trx_list == &trx_sys->rw_trx_list);

	for (trx = UT_LIST_GET_FIRST(*trx_list);
	     trx != NULL;
	     prev_trx = trx, trx = UT_LIST_GET_NEXT(trx_list, prev_trx)) {

		check_trx_state(trx);
		ut_a(prev_trx == NULL || prev_trx->id > trx->id);
	}

	return(true);
}

/*************************************************************//**
Validate the trx_sys_t::rw_trx_list.
@return true if the list is valid. */
bool
trx_sys_validate_trx_list()
/*=======================*/
{
	ut_ad(trx_sys_mutex_own());

	ut_a(trx_sys_validate_trx_list_low(&trx_sys->rw_trx_list));

	return(true);
}
#endif /* UNIV_DEBUG */

/** A list of undo tablespace IDs found in the TRX_SYS page.
This cannot be part of the trx_sys_t object because it is initialized
before that object is created. */
Space_Ids*	trx_sys_undo_spaces;

/** Initialize trx_sys_undo_spaces, called once during srv_start(). */
void
trx_sys_undo_spaces_init()
{
	trx_sys_undo_spaces = UT_NEW(Space_Ids(), mem_key_trx_sys_t_rsegs);

	trx_sys_undo_spaces->reserve(TRX_SYS_N_RSEGS);
}

/** Free the resources occupied by trx_sys_undo_spaces,
called once during thread de-initialization. */
void
trx_sys_undo_spaces_deinit()
{
	trx_sys_undo_spaces->clear();

	UT_DELETE(trx_sys_undo_spaces);

	trx_sys_undo_spaces = NULL;
}
#endif /* !UNIV_HOTBACKUP */
