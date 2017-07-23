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

#include "ha_prototypes.h"

#include "mysqld.h"
#include "trx0sys.h"
#include "sql_error.h"
#ifdef UNIV_NONINL
#include "trx0sys.ic"
#endif

#ifdef UNIV_HOTBACKUP
#include "fsp0types.h"

#else	/* !UNIV_HOTBACKUP */
#include "fsp0fsp.h"
#include "mtr0log.h"
#include "mtr0log.h"
#include "trx0trx.h"
#include "trx0rseg.h"
#include "trx0undo.h"
#include "srv0srv.h"
#include "srv0start.h"
#include "trx0purge.h"
#include "log0log.h"
#include "log0recv.h"
#include "os0file.h"
#include "read0read.h"
#include "fsp0sysspace.h"

/** The file format tag structure with id and name. */
struct file_format_t {
	ulint		id;		/*!< id of the file format */
	const char*	name;		/*!< text representation of the
					file format */
	ib_mutex_t		mutex;		/*!< covers changes to the above
					fields */
};

/** The transaction system */
trx_sys_t*		trx_sys		= NULL;
#endif /* !UNIV_HOTBACKUP */

/** List of animal names representing file format. */
static const char*	file_format_name_map[] = {
	"Antelope",
	"Barracuda",
	"Cheetah",
	"Dragon",
	"Elk",
	"Fox",
	"Gazelle",
	"Hornet",
	"Impala",
	"Jaguar",
	"Kangaroo",
	"Leopard",
	"Moose",
	"Nautilus",
	"Ocelot",
	"Porpoise",
	"Quail",
	"Rabbit",
	"Shark",
	"Tiger",
	"Urchin",
	"Viper",
	"Whale",
	"Xenops",
	"Yak",
	"Zebra"
};

/** The number of elements in the file format name array. */
static const ulint	FILE_FORMAT_NAME_N
	= sizeof(file_format_name_map) / sizeof(file_format_name_map[0]);

/** Check whether transaction id is valid.
@param[in]	id              transaction id to check
@param[in]      name            table name */
void
ReadView::check_trx_id_sanity(
	trx_id_t		id,
	const table_name_t&	name)
{
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
#ifdef UNIV_DEBUG
/* Flag to control TRX_RSEG_N_SLOTS behavior debugging. */
uint	trx_rseg_n_slots_debug = 0;
#endif

/** This is used to track the maximum file format id known to InnoDB. It's
updated via SET GLOBAL innodb_file_format_max = 'x' or when we open
or create a table. */
static	file_format_t	file_format_max;

#ifdef UNIV_DEBUG
/****************************************************************//**
Checks whether a trx is in one of rw_trx_list
@return true if is in */
bool
trx_in_rw_trx_list(
/*============*/
	const trx_t*	in_trx)	/*!< in: transaction */
{
	const trx_t*	trx;

	/* Non-locking autocommits should not hold any locks. */
	check_trx_state(in_trx);

	ut_ad(trx_sys_mutex_own());

	ut_ad(trx_assert_started(in_trx));

	for (trx = UT_LIST_GET_FIRST(trx_sys->rw_trx_list);
	     trx != NULL && trx != in_trx;
	     trx = UT_LIST_GET_NEXT(trx_list, trx)) {

		check_trx_state(trx);

		ut_ad(trx->rsegs.m_redo.rseg != NULL && !trx->read_only);
	}

	return(trx != 0);
}
#endif /* UNIV_DEBUG */

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

/****************************************************************//**
Looks for a free slot for a rollback segment in the trx system file copy.
@return slot index or ULINT_UNDEFINED if not found */
ulint
trx_sysf_rseg_find_free(
/*====================*/
	mtr_t*	mtr,			/*!< in/out: mtr */
	bool	include_tmp_slots,	/*!< in: if true, report slots reserved
					for temp-tablespace as free slots. */
	ulint	nth_free_slots)		/*!< in: allocate nth free slot.
					0 means next free slot. */
{
	ulint		i;
	trx_sysf_t*	sys_header;

	sys_header = trx_sysf_get(mtr);

	ulint	found_free_slots = 0;
	for (i = 0; i < TRX_SYS_N_RSEGS; i++) {
		ulint	page_no;

		if (!include_tmp_slots && trx_sys_is_noredo_rseg_slot(i)) {
			continue;
		}

		page_no = trx_sysf_rseg_get_page_no(sys_header, i, mtr);

		if (page_no == FIL_NULL
		    || (include_tmp_slots
			&& trx_sys_is_noredo_rseg_slot(i))) {

			if (found_free_slots++ >= nth_free_slots) {
				return(i);
			}
		}
	}

	return(ULINT_UNDEFINED);
}

/****************************************************************//**
Looks for used slots for redo rollback segment.
@return number of used slots */
static
ulint
trx_sysf_used_slots_for_redo_rseg(
/*==============================*/
	mtr_t*	mtr)			/*!< in: mtr */
{
	trx_sysf_t*	sys_header;
	ulint		n_used = 0;

	sys_header = trx_sysf_get(mtr);

	for (ulint i = 0; i < TRX_SYS_N_RSEGS; i++) {

		if (trx_sys_is_noredo_rseg_slot(i)) {
			continue;
		}

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

	mtr_x_lock_space(TRX_SYS_SPACE, mtr);

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
	slot_no = trx_sysf_rseg_find_free(mtr, false, 0);
	page_no = trx_rseg_header_create(TRX_SYS_SPACE, univ_page_size,
					 ULINT_MAX, slot_no, mtr);

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
		trx_rseg_array_init(purge_queue);
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

/*****************************************************************//**
Update the file format tag.
@return always TRUE */
static
ibool
trx_sys_file_format_max_write(
/*==========================*/
	ulint		format_id,	/*!< in: file format id */
	const char**	name)		/*!< out: max file format name, can
					be NULL */
{
	mtr_t		mtr;
	byte*		ptr;
	buf_block_t*	block;
	ib_uint64_t	tag_value;

	mtr_start(&mtr);

	block = buf_page_get(
		page_id_t(TRX_SYS_SPACE, TRX_SYS_PAGE_NO), univ_page_size,
		RW_X_LATCH, &mtr);

	file_format_max.id = format_id;
	file_format_max.name = trx_sys_file_format_id_to_name(format_id);

	ptr = buf_block_get_frame(block) + TRX_SYS_FILE_FORMAT_TAG;
	tag_value = format_id + TRX_SYS_FILE_FORMAT_TAG_MAGIC_N;

	if (name) {
		*name = file_format_max.name;
	}

	mlog_write_ull(ptr, tag_value, &mtr);

	mtr_commit(&mtr);

	return(TRUE);
}

/*****************************************************************//**
Read the file format tag.
@return the file format or ULINT_UNDEFINED if not set. */
static
ulint
trx_sys_file_format_max_read(void)
/*==============================*/
{
	mtr_t			mtr;
	const byte*		ptr;
	const buf_block_t*	block;
	ib_id_t			file_format_id;

	/* Since this is called during the startup phase it's safe to
	read the value without a covering mutex. */
	mtr_start(&mtr);

	block = buf_page_get(
		page_id_t(TRX_SYS_SPACE, TRX_SYS_PAGE_NO), univ_page_size,
		RW_X_LATCH, &mtr);

	ptr = buf_block_get_frame(block) + TRX_SYS_FILE_FORMAT_TAG;
	file_format_id = mach_read_from_8(ptr);

	mtr_commit(&mtr);

	file_format_id -= TRX_SYS_FILE_FORMAT_TAG_MAGIC_N;

	if (file_format_id >= FILE_FORMAT_NAME_N) {

		/* Either it has never been tagged, or garbage in it. */
		return(ULINT_UNDEFINED);
	}

	return((ulint) file_format_id);
}

/*****************************************************************//**
Get the name representation of the file format from its id.
@return pointer to the name */
const char*
trx_sys_file_format_id_to_name(
/*===========================*/
	const ulint	id)	/*!< in: id of the file format */
{
	ut_a(id < FILE_FORMAT_NAME_N);

	return(file_format_name_map[id]);
}

/*****************************************************************//**
Check for the max file format tag stored on disk. Note: If max_format_id
is == UNIV_FORMAT_MAX + 1 then we only print a warning.
@return DB_SUCCESS or error code */
dberr_t
trx_sys_file_format_max_check(
/*==========================*/
	ulint	max_format_id)	/*!< in: max format id to check */
{
	ulint	format_id;

	/* Check the file format in the tablespace. Do not try to
	recover if the file format is not supported by the engine
	unless forced by the user. */
	format_id = trx_sys_file_format_max_read();
	if (format_id == ULINT_UNDEFINED) {
		/* Format ID was not set. Set it to minimum possible
		value. */
		format_id = UNIV_FORMAT_MIN;
	}

	ib::info() << "Highest supported file format is "
		<< trx_sys_file_format_id_to_name(UNIV_FORMAT_MAX) << ".";

	if (format_id > UNIV_FORMAT_MAX) {

		ut_a(format_id < FILE_FORMAT_NAME_N);

		const std::string	msg = std::string("The system"
			" tablespace is in a file format that this version"
			" doesn't support - ")
			+ trx_sys_file_format_id_to_name(format_id)
			+ ".";

		if (max_format_id <= UNIV_FORMAT_MAX) {
			ib::error() << msg;
		} else {
			ib::warn() << msg;
		}

		if (max_format_id <= UNIV_FORMAT_MAX) {
			return(DB_ERROR);
		}
	}

	format_id = (format_id > max_format_id) ? format_id : max_format_id;

	/* We don't need a mutex here, as this function should only
	be called once at start up. */
	file_format_max.id = format_id;
	file_format_max.name = trx_sys_file_format_id_to_name(format_id);

	return(DB_SUCCESS);
}

/*****************************************************************//**
Set the file format id unconditionally except if it's already the
same value.
@return TRUE if value updated */
ibool
trx_sys_file_format_max_set(
/*========================*/
	ulint		format_id,	/*!< in: file format id */
	const char**	name)		/*!< out: max file format name or
					NULL if not needed. */
{
	ibool		ret = FALSE;

	ut_a(format_id <= UNIV_FORMAT_MAX);

	mutex_enter(&file_format_max.mutex);

	/* Only update if not already same value. */
	if (format_id != file_format_max.id) {

		ret = trx_sys_file_format_max_write(format_id, name);
	}

	mutex_exit(&file_format_max.mutex);

	return(ret);
}

/********************************************************************//**
Tags the system table space with minimum format id if it has not been
tagged yet.
WARNING: This function is only called during the startup and AFTER the
redo log application during recovery has finished. */
void
trx_sys_file_format_tag_init(void)
/*==============================*/
{
	ulint	format_id;

	format_id = trx_sys_file_format_max_read();

	/* If format_id is not set then set it to the minimum. */
	if (format_id == ULINT_UNDEFINED) {
		trx_sys_file_format_max_set(UNIV_FORMAT_MIN, NULL);
	}
}

/********************************************************************//**
Update the file format tag in the system tablespace only if the given
format id is greater than the known max id.
@return TRUE if format_id was bigger than the known max id */
ibool
trx_sys_file_format_max_upgrade(
/*============================*/
	const char**	name,		/*!< out: max file format name */
	ulint		format_id)	/*!< in: file format identifier */
{
	ibool		ret = FALSE;

	ut_a(name);
	ut_a(file_format_max.name != NULL);
	ut_a(format_id <= UNIV_FORMAT_MAX);

	mutex_enter(&file_format_max.mutex);

	if (format_id > file_format_max.id) {

		ret = trx_sys_file_format_max_write(format_id, name);
	}

	mutex_exit(&file_format_max.mutex);

	return(ret);
}

/*****************************************************************//**
Get the name representation of the file format from its id.
@return pointer to the max format name */
const char*
trx_sys_file_format_max_get(void)
/*=============================*/
{
	return(file_format_max.name);
}

/*****************************************************************//**
Initializes the tablespace tag system. */
void
trx_sys_file_format_init(void)
/*==========================*/
{
	mutex_create(LATCH_ID_FILE_FORMAT_MAX, &file_format_max.mutex);

	/* We don't need a mutex here, as this function should only
	be called once at start up. */
	file_format_max.id = UNIV_FORMAT_MIN;

	file_format_max.name = trx_sys_file_format_id_to_name(
		file_format_max.id);
}

/*****************************************************************//**
Closes the tablespace tag system. */
void
trx_sys_file_format_close(void)
/*===========================*/
{
	mutex_free(&file_format_max.mutex);
}

/*********************************************************************
Creates non-redo rollback segments.
@return number of non-redo rollback segments created. */
static
ulint
trx_sys_create_noredo_rsegs(
/*========================*/
	ulint	n_nonredo_rseg)	/*!< number of non-redo rollback segment
				to create. */
{
	ulint n_created = 0;

	/* Create non-redo rollback segments residing in temp-tablespace.
	non-redo rollback segments don't perform redo logging and so
	are used for undo logging of objects/table that don't need to be
	recover on crash.
	(Non-Redo rollback segments are created on every server startup).
	Slot-0: reserved for system-tablespace.
	Slot-1....Slot-N: reserved for temp-tablespace.
	Slot-N+1....Slot-127: reserved for system/undo-tablespace. */
	for (ulint i = 0; i < n_nonredo_rseg; i++) {
		ulint space = srv_tmp_space.space_id();
		if (trx_rseg_create(space, i) == NULL) {
			break;
		}
		++n_created;
	}

	return(n_created);
}

/*********************************************************************
Creates the rollback segments.
@return number of rollback segments that are active. */
ulint
trx_sys_create_rsegs(
/*=================*/
	ulint	n_spaces,	/*!< number of tablespaces for UNDO logs */
	ulint	n_rsegs,	/*!< number of rollback segments to create */
	ulint	n_tmp_rsegs)	/*!< number of rollback segments reserved for
				temp-tables. */
{
	mtr_t	mtr;
	ulint	n_used;
	ulint	n_noredo_created;

	ut_a(n_spaces < TRX_SYS_N_RSEGS);
	ut_a(n_rsegs <= TRX_SYS_N_RSEGS);
	ut_a(n_tmp_rsegs > 0 && n_tmp_rsegs < TRX_SYS_N_RSEGS);

	if (srv_read_only_mode) {
		return(ULINT_UNDEFINED);
	}

	/* Create non-redo rollback segments. */
	n_noredo_created = trx_sys_create_noredo_rsegs(n_tmp_rsegs);

	/* This is executed in single-threaded mode therefore it is not
	necessary to use the same mtr in trx_rseg_create(). n_used cannot
	change while the function is executing. */
	mtr_start(&mtr);
	n_used = trx_sysf_used_slots_for_redo_rseg(&mtr) + n_noredo_created;
	mtr_commit(&mtr);

	ut_ad(n_used <= TRX_SYS_N_RSEGS);

	/* By default 1 redo rseg is always active that is hosted in
	system tablespace. */
	ulint	n_redo_active;
	if (n_rsegs <= n_tmp_rsegs) {
		n_redo_active = 1;
	} else if (n_rsegs > n_used) {
		n_redo_active = n_used - n_tmp_rsegs;
	} else {
		n_redo_active = n_rsegs - n_tmp_rsegs;
	}

	/* Do not create additional rollback segments if innodb_force_recovery
	has been set and the database was not shutdown cleanly. */
	if (!srv_force_recovery && !recv_needed_recovery && n_used < n_rsegs) {
		ulint	i;
		ulint	new_rsegs = n_rsegs - n_used;

		for (i = 0; i < new_rsegs; ++i) {
			ulint	space_id;
			space_id = (n_spaces == 0) ? 0
				: (srv_undo_space_id_start + i % n_spaces);

			ut_ad(n_spaces == 0
			      || srv_is_undo_tablespace(space_id));

			if (trx_rseg_create(space_id, 0) != NULL) {
				++n_used;
				++n_redo_active;

				ulint	last_undo_space =
					srv_undo_space_id_start
					+ (srv_undo_tablespaces_active - 1);

				/* Increase the number of active undo
				tablespace in case new rollback segment
				assigned to new undo tablespace. */
				if (space_id > last_undo_space) {
					srv_undo_tablespaces_active++;

					ut_ad(space_id == last_undo_space + 1);
				}
			} else {
				break;
			}
		}
	}

	ib::info() << n_used - srv_tmp_undo_logs
		<< " redo rollback segment(s) found. "
		<< n_redo_active
		<< " redo rollback segment(s) are active.";

	ib::info() << n_noredo_created << " non-redo rollback segment(s) are"
		" active.";

	return(n_used);
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

/*****************************************************************//**
Reads the file format id from the first system table space file.
Even if the call succeeds and returns TRUE, the returned format id
may be ULINT_UNDEFINED signalling that the format id was not present
in the data file.
@return TRUE if call succeeds */
ibool
trx_sys_read_file_format_id(
/*========================*/
	const char *pathname,  /*!< in: pathname of the first system
				        table space file */
	ulint *format_id)      /*!< out: file format of the system table
				         space */
{
	os_file_t	file;
	bool		success;
	byte		buf[UNIV_PAGE_SIZE * 2];
	page_t*		page = ut_align(buf, UNIV_PAGE_SIZE);
	const byte*	ptr;
	ib_id_t		file_format_id;

	*format_id = ULINT_UNDEFINED;

	file = os_file_create_simple_no_error_handling(
		innodb_data_file_key,
		pathname,
		OS_FILE_OPEN,
		OS_FILE_READ_ONLY,
		srv_read_only_mode,
		&success
	);
	if (!success) {
		/* The following call prints an error message */
		os_file_get_last_error(true);

		ib::error() << "mysqlbackup: Error: trying to read system"
			" tablespace file format, but could not open the"
			" tablespace file " << pathname << "!";
		return(FALSE);
	}

	/* Read the page on which file format is stored */

	IORequest	read_req(IORequest::READ)

	dberr_t	err = os_file_read_no_error_handling(
		read_req, file, page, TRX_SYS_PAGE_NO * UNIV_PAGE_SIZE,
		UNIV_PAGE_SIZE, NULL);

	if (err != DB_SUCCESS) {
		/* The following call prints an error message */
		os_file_get_last_error(true);

		ib::error() << "mysqlbackup: Error: trying to read system"
			" tablespace file format, but failed to read the"
			" tablespace file " << pathname << "!";

		os_file_close(file);
		return(FALSE);
	}
	os_file_close(file);

	/* get the file format from the page */
	ptr = page + TRX_SYS_FILE_FORMAT_TAG;
	file_format_id = mach_read_from_8(ptr);
	file_format_id -= TRX_SYS_FILE_FORMAT_TAG_MAGIC_N;

	if (file_format_id >= FILE_FORMAT_NAME_N) {

		/* Either it has never been tagged, or garbage in it. */
		return(TRUE);
	}

	*format_id = (ulint) file_format_id;

	return(TRUE);
}

/*****************************************************************//**
Reads the file format id from the given per-table data file.
@return TRUE if call succeeds */
ibool
trx_sys_read_pertable_file_format_id(
/*=================================*/
	const char *pathname,  /*!< in: pathname of a per-table
				        datafile */
	ulint *format_id)      /*!< out: file format of the per-table
				         data file */
{
	os_file_t	file;
	bool		success;
	byte		buf[UNIV_PAGE_SIZE * 2];
	page_t*		page = ut_align(buf, UNIV_PAGE_SIZE);
	const byte*	ptr;
	ib_uint32_t	flags;

	*format_id = ULINT_UNDEFINED;

	file = os_file_create_simple_no_error_handling(
		innodb_data_file_key,
		pathname,
		OS_FILE_OPEN,
		OS_FILE_READ_ONLY,
		srv_read_only_mode,
		&success
	);
	if (!success) {
		/* The following call prints an error message */
		os_file_get_last_error(true);

		ib::error() << "mysqlbackup: Error: trying to read per-table"
			" tablespace format, but could not open the tablespace"
			" file " << pathname << "!";

		return(FALSE);
	}

	IORequest	read_req(IORequest::READ);

	/* Read the first page of the per-table datafile */

	dberr_t	err = os_file_read_no_error_handling(
		read_req, file, page, 0, UNIV_PAGE_SIZE, NULL);

	if (err != DB_SUCCESS) {
		/* The following call prints an error message */
		os_file_get_last_error(true);

		ib::error() << "mysqlbackup: Error: trying to per-table data"
			" file format, but failed to read the tablespace file "
			<< pathname << "!";

		os_file_close(file);
		return(FALSE);
	}
	os_file_close(file);

	/* get the file format from the page */
	ptr = page + 54;
	flags = mach_read_from_4(ptr);

	if (!fsp_flags_is_valid(flags) {
		/* bad tablespace flags */
		return(FALSE);
	}

	*format_id = FSP_FLAGS_GET_POST_ANTELOPE(flags);

	return(TRUE);
}


/*****************************************************************//**
Get the name representation of the file format from its id.
@return pointer to the name */
const char*
trx_sys_file_format_id_to_name(
/*===========================*/
	const ulint	id)	/*!< in: id of the file format */
{
	if (!(id < FILE_FORMAT_NAME_N)) {
		/* unknown id */
		return("Unknown");
	}

	return(file_format_name_map[id]);
}

#endif /* !UNIV_HOTBACKUP */

#ifndef UNIV_HOTBACKUP
/*********************************************************************
Shutdown/Close the transaction system. */
void
trx_sys_close(void)
/*===============*/
{
	ut_ad(trx_sys != NULL);
	ut_ad(srv_shutdown_state == SRV_SHUTDOWN_EXIT_THREADS);

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
	trx_rseg_t** rseg_array = static_cast<trx_rseg_t**>(
		trx_sys->rseg_array);

	for (ulint i = 0; i < TRX_SYS_N_RSEGS; ++i) {
		trx_rseg_t*	rseg;

		rseg = trx_sys->rseg_array[i];

		if (rseg != NULL) {
			trx_rseg_mem_free(rseg, rseg_array);
		}
	}

	rseg_array = ((trx_rseg_t**) trx_sys->pending_purge_rseg_array);

	for (ulint i = 0; i < TRX_SYS_N_RSEGS; ++i) {
		trx_rseg_t*	rseg;

		rseg = trx_sys->pending_purge_rseg_array[i];

		if (rseg != NULL) {
			trx_rseg_mem_free(rseg, rseg_array);
		}
	}

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
				trx_sys->n_prepared_recovered_trx++;
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
#endif /* !UNIV_HOTBACKUP */
