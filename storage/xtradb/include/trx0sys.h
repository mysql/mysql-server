/*****************************************************************************

Copyright (c) 1996, 2011, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA

*****************************************************************************/

/**************************************************//**
@file include/trx0sys.h
Transaction system

Created 3/26/1996 Heikki Tuuri
*******************************************************/

#ifndef trx0sys_h
#define trx0sys_h

#include "univ.i"

#include "trx0types.h"
#include "fsp0types.h"
#include "fil0fil.h"
#include "buf0buf.h"
#ifndef UNIV_HOTBACKUP
#include "mtr0mtr.h"
#include "ut0byte.h"
#include "mem0mem.h"
#include "sync0sync.h"
#include "ut0lst.h"
#include "ut0bh.h"
#include "read0types.h"
#include "page0types.h"

/** In a MySQL replication slave, in crash recovery we store the master log
file name and position here. */
/* @{ */
/** Master binlog file name */
extern char		trx_sys_mysql_master_log_name[];
/** Master binlog file position.  We have successfully got the updates
up to this position.  -1 means that no crash recovery was needed, or
there was no master log position info inside InnoDB.*/
extern ib_int64_t	trx_sys_mysql_master_log_pos;
/* @} */

extern char		trx_sys_mysql_relay_log_name[];
extern ib_int64_t	trx_sys_mysql_relay_log_pos;

/** If this MySQL server uses binary logging, after InnoDB has been inited
and if it has done a crash recovery, we store the binlog file name and position
here. */
/* @{ */
/** Binlog file name */
extern char		trx_sys_mysql_bin_log_name[];
/** Binlog file position, or -1 if unknown */
extern ib_int64_t	trx_sys_mysql_bin_log_pos;
/* @} */

/** The transaction system */
extern trx_sys_t*	trx_sys;

/** Doublewrite system */
extern trx_doublewrite_t*	trx_doublewrite;
/** The following is set to TRUE when we are upgrading from pre-4.1
format data files to the multiple tablespaces format data files */
extern ibool			trx_doublewrite_must_reset_space_ids;
/** Set to TRUE when the doublewrite buffer is being created */
extern ibool			trx_doublewrite_buf_is_being_created;
/** The following is TRUE when we are using the database in the
post-4.1 format, i.e., we have successfully upgraded, or have created
a new database installation */
extern ibool			trx_sys_multiple_tablespace_format;

/****************************************************************//**
Creates the doublewrite buffer to a new InnoDB installation. The header of the
doublewrite buffer is placed on the trx system header page. */
UNIV_INTERN
void
trx_sys_create_doublewrite_buf(void);
/*================================*/
/****************************************************************//**
At a database startup initializes the doublewrite buffer memory structure if
we already have a doublewrite buffer created in the data files. If we are
upgrading to an InnoDB version which supports multiple tablespaces, then this
function performs the necessary update operations. If we are in a crash
recovery, this function uses a possible doublewrite buffer to restore
half-written pages in the data files. */
UNIV_INTERN
void
trx_sys_doublewrite_init_or_restore_pages(
/*======================================*/
	ibool	restore_corrupt_pages);	/*!< in: TRUE=restore pages */
/****************************************************************//**
Marks the trx sys header when we have successfully upgraded to the >= 4.1.x
multiple tablespace format. */
UNIV_INTERN
void
trx_sys_mark_upgraded_to_multiple_tablespaces(void);
/*===============================================*/
/****************************************************************//**
Determines if a page number is located inside the doublewrite buffer.
@return TRUE if the location is inside the two blocks of the
doublewrite buffer */
UNIV_INTERN
ibool
trx_doublewrite_page_inside(
/*========================*/
	ulint	page_no);	/*!< in: page number */
/***************************************************************//**
Checks if a page address is the trx sys header page.
@return	TRUE if trx sys header page */
UNIV_INLINE
ibool
trx_sys_hdr_page(
/*=============*/
	ulint	space,	/*!< in: space */
	ulint	page_no);/*!< in: page number */
/***************************************************************//**
Checks if a space is the system tablespaces.
@return TRUE if system tablespace */
UNIV_INLINE
ibool
trx_sys_sys_space(
/*==============*/
	ulint	space);	/*!< in: space */
/***************************************************************//**
Checks if a space is the doublewrite tablespace.
@return TRUE if doublewrite tablespace */
UNIV_INLINE
ibool
trx_sys_doublewrite_space(
/*======================*/
	ulint	space);	/*!< in: space */
/*****************************************************************//**
Creates and initializes the central memory structures for the transaction
system. This is called when the database is started. */
UNIV_INTERN
void
trx_sys_init_at_db_start(void);
/*==========================*/
/*****************************************************************//**
Creates and initializes the transaction system at the database creation. */
UNIV_INTERN
void
trx_sys_create(void);
/*================*/
/*****************************************************************//**
Creates and initializes the dummy transaction system page for tablespace. */
UNIV_INTERN
void
trx_sys_dummy_create(
/*=================*/
	ulint	space);
/****************************************************************//**
Looks for a free slot for a rollback segment in the trx system file copy.
@return	slot index or ULINT_UNDEFINED if not found */
UNIV_INTERN
ulint
trx_sysf_rseg_find_free(
/*====================*/
	mtr_t*		mtr);		/*!< in: mtr */
/***************************************************************//**
Gets the pointer in the nth slot of the rseg array.
@return	pointer to rseg object, NULL if slot not in use */
UNIV_INLINE
trx_rseg_t*
trx_sys_get_nth_rseg(
/*=================*/
	trx_sys_t*	sys,	/*!< in: trx system */
	ulint		n);	/*!< in: index of slot */
/***************************************************************//**
Sets the pointer in the nth slot of the rseg array. */
UNIV_INLINE
void
trx_sys_set_nth_rseg(
/*=================*/
	trx_sys_t*	sys,	/*!< in: trx system */
	ulint		n,	/*!< in: index of slot */
	trx_rseg_t*	rseg);	/*!< in: pointer to rseg object, NULL if slot
				not in use */
/**********************************************************************//**
Gets a pointer to the transaction system file copy and x-locks its page.
@return	pointer to system file copy, page x-locked */
UNIV_INLINE
trx_sysf_t*
trx_sysf_get(
/*=========*/
	mtr_t*	mtr);	/*!< in: mtr */
/*****************************************************************//**
Gets the space of the nth rollback segment slot in the trx system
file copy.
@return	space id */
UNIV_INLINE
ulint
trx_sysf_rseg_get_space(
/*====================*/
	trx_sysf_t*	sys_header,	/*!< in: trx sys file copy */
	ulint		i,		/*!< in: slot index == rseg id */
	mtr_t*		mtr);		/*!< in: mtr */
/*****************************************************************//**
Gets the page number of the nth rollback segment slot in the trx system
file copy.
@return	page number, FIL_NULL if slot unused */
UNIV_INLINE
ulint
trx_sysf_rseg_get_page_no(
/*======================*/
	trx_sysf_t*	sys_header,	/*!< in: trx sys file copy */
	ulint		i,		/*!< in: slot index == rseg id */
	mtr_t*		mtr);		/*!< in: mtr */
/*****************************************************************//**
Sets the space id of the nth rollback segment slot in the trx system
file copy. */
UNIV_INLINE
void
trx_sysf_rseg_set_space(
/*====================*/
	trx_sysf_t*	sys_header,	/*!< in: trx sys file copy */
	ulint		i,		/*!< in: slot index == rseg id */
	ulint		space,		/*!< in: space id */
	mtr_t*		mtr);		/*!< in: mtr */
/*****************************************************************//**
Sets the page number of the nth rollback segment slot in the trx system
file copy. */
UNIV_INLINE
void
trx_sysf_rseg_set_page_no(
/*======================*/
	trx_sysf_t*	sys_header,	/*!< in: trx sys file copy */
	ulint		i,		/*!< in: slot index == rseg id */
	ulint		page_no,	/*!< in: page number, FIL_NULL if
					the slot is reset to unused */
	mtr_t*		mtr);		/*!< in: mtr */
/*****************************************************************//**
Allocates a new transaction id.
@return	new, allocated trx id */
UNIV_INLINE
trx_id_t
trx_sys_get_new_trx_id(void);
/*========================*/

#ifdef UNIV_DEBUG
/* Flag to control TRX_RSEG_N_SLOTS behavior debugging. */
extern uint			trx_rseg_n_slots_debug;
#endif

/*****************************************************************//**
Writes a trx id to an index page. In case that the id size changes in
some future version, this function should be used instead of
mach_write_... */
UNIV_INLINE
void
trx_write_trx_id(
/*=============*/
	byte*		ptr,	/*!< in: pointer to memory where written */
	trx_id_t	id);	/*!< in: id */
/*****************************************************************//**
Reads a trx id from an index page. In case that the id size changes in
some future version, this function should be used instead of
mach_read_...
@return	id */
UNIV_INLINE
trx_id_t
trx_read_trx_id(
/*============*/
	const byte*	ptr);	/*!< in: pointer to memory from where to read */
/****************************************************************//**
Looks for the trx handle with the given id in trx_list.
@return	the trx handle or NULL if not found */
UNIV_INLINE
trx_t*
trx_get_on_id(
/*==========*/
	trx_id_t	trx_id);/*!< in: trx id to search for */
/****************************************************************//**
Returns the minumum trx id in trx list. This is the smallest id for which
the trx can possibly be active. (But, you must look at the trx->conc_state to
find out if the minimum trx id transaction itself is active, or already
committed.)
@return	the minimum trx id, or trx_sys->max_trx_id if the trx list is empty */
UNIV_INLINE
trx_id_t
trx_list_get_min_trx_id(void);
/*=========================*/
/****************************************************************//**
Checks if a transaction with the given id is active.
@return	TRUE if active */
UNIV_INLINE
ibool
trx_is_active(
/*==========*/
	trx_id_t	trx_id);/*!< in: trx id of the transaction */
/****************************************************************//**
Checks that trx is in the trx list.
@return	TRUE if is in */
UNIV_INTERN
ibool
trx_in_trx_list(
/*============*/
	trx_t*	in_trx);/*!< in: trx */
#if defined UNIV_DEBUG || defined UNIV_BLOB_LIGHT_DEBUG
/***********************************************************//**
Assert that a transaction has been recovered.
@return TRUE */
UNIV_INLINE
ibool
trx_assert_recovered(
/*=================*/
	trx_id_t	trx_id)		/*!< in: transaction identifier */
	__attribute__((warn_unused_result));
#endif /* UNIV_DEBUG || UNIV_BLOB_LIGHT_DEBUG */
/*****************************************************************//**
Updates the offset information about the end of the MySQL binlog entry
which corresponds to the transaction just being committed. In a MySQL
replication slave updates the latest master binlog position up to which
replication has proceeded. */
UNIV_INTERN
void
trx_sys_update_mysql_binlog_offset(
/*===============================*/
	trx_sysf_t*	sys_header,
	const char*	file_name_in,/*!< in: MySQL log file name */
	ib_int64_t	offset,	/*!< in: position in that log file */
	ulint		field,	/*!< in: offset of the MySQL log info field in
				the trx sys header */
	mtr_t*		mtr);	/*!< in: mtr */
/*****************************************************************//**
Prints to stderr the MySQL binlog offset info in the trx system header if
the magic number shows it valid. */
UNIV_INTERN
void
trx_sys_print_mysql_binlog_offset(void);
/*===================================*/
/*****************************************************************//**
Prints to stderr the MySQL master log offset info in the trx system header
COMMIT set of fields if the magic number shows it valid and stores it
in global variables. */
UNIV_INTERN
void
trx_sys_print_committed_mysql_master_log_pos(void);
/*==============================================*/
/*****************************************************************//**
Prints to stderr the MySQL master log offset info in the trx system header if
the magic number shows it valid. */
UNIV_INTERN
void
trx_sys_print_mysql_master_log_pos(void);
/*====================================*/
/*****************************************************************//**
Initializes the tablespace tag system. */
UNIV_INTERN
void
trx_sys_file_format_init(void);
/*==========================*/
/*****************************************************************//**
Closes the tablespace tag system. */
UNIV_INTERN
void
trx_sys_file_format_close(void);
/*===========================*/
/********************************************************************//**
Tags the system table space with minimum format id if it has not been
tagged yet.
WARNING: This function is only called during the startup and AFTER the
redo log application during recovery has finished. */
UNIV_INTERN
void
trx_sys_file_format_tag_init(void);
/*==============================*/
#ifndef UNIV_HOTBACKUP
/*****************************************************************//**
Shutdown/Close the transaction system. */
UNIV_INTERN
void
trx_sys_close(void);
/*===============*/
#endif /* !UNIV_HOTBACKUP */
/*****************************************************************//**
Get the name representation of the file format from its id.
@return	pointer to the name */
UNIV_INTERN
const char*
trx_sys_file_format_id_to_name(
/*===========================*/
	const ulint	id);		/*!< in: id of the file format */
/*****************************************************************//**
Set the file format id unconditionally except if it's already the
same value.
@return	TRUE if value updated */
UNIV_INTERN
ibool
trx_sys_file_format_max_set(
/*========================*/
	ulint		format_id,	/*!< in: file format id */
	const char**	name);		/*!< out: max file format name or
					NULL if not needed. */
/*****************************************************************//**
Get the name representation of the file format from its id.
@return	pointer to the max format name */
UNIV_INTERN
const char*
trx_sys_file_format_max_get(void);
/*=============================*/
/*****************************************************************//**
Check for the max file format tag stored on disk.
@return	DB_SUCCESS or error code */
UNIV_INTERN
ulint
trx_sys_file_format_max_check(
/*==========================*/
	ulint		max_format_id);	/*!< in: the max format id to check */
/********************************************************************//**
Update the file format tag in the system tablespace only if the given
format id is greater than the known max id.
@return	TRUE if format_id was bigger than the known max id */
UNIV_INTERN
ibool
trx_sys_file_format_max_upgrade(
/*============================*/
	const char**	name,		/*!< out: max file format name */
	ulint		format_id);	/*!< in: file format identifier */
#else /* !UNIV_HOTBACKUP */
/*****************************************************************//**
Prints to stderr the MySQL binlog info in the system header if the
magic number shows it valid. */
UNIV_INTERN
void
trx_sys_print_mysql_binlog_offset_from_page(
/*========================================*/
	const byte*	page);	/*!< in: buffer containing the trx
				system header page, i.e., page number
				TRX_SYS_PAGE_NO in the tablespace */
/*****************************************************************//**
Reads the file format id from the first system table space file.
Even if the call succeeds and returns TRUE, the returned format id
may be ULINT_UNDEFINED signalling that the format id was not present
in the data file.
@return TRUE if call succeeds */
UNIV_INTERN
ibool
trx_sys_read_file_format_id(
/*========================*/
	const char *pathname,	/*!< in: pathname of the first system
				table space file */
	ulint *format_id);	/*!< out: file format of the system table
				space */
/*****************************************************************//**
Reads the file format id from the given per-table data file.
@return TRUE if call succeeds */
UNIV_INTERN
ibool
trx_sys_read_pertable_file_format_id(
/*=================================*/
	const char *pathname,	/*!< in: pathname of a per-table
				datafile */
	ulint *format_id);	/*!< out: file format of the per-table
				data file */
/*****************************************************************//**
Get the name representation of the file format from its id.
@return	pointer to the name */
UNIV_INTERN
const char*
trx_sys_file_format_id_to_name(
/*===========================*/
	const ulint	id);	/*!< in: id of the file format */

#endif /* !UNIV_HOTBACKUP */
/*********************************************************************
Creates the rollback segments */
UNIV_INTERN
void
trx_sys_create_rsegs(
/*=================*/
	ulint	n_rsegs);	/*!< number of rollback segments to create */

/* The automatically created system rollback segment has this id */
#define TRX_SYS_SYSTEM_RSEG_ID	0

/* Space id and page no where the trx system file copy resides */
#define	TRX_SYS_SPACE	0	/* the SYSTEM tablespace */
#define	TRX_DOUBLEWRITE_SPACE	0xFFFFFFE0UL	/* the doublewrite buffer tablespace if used */
#define	TRX_SYS_SPACE_MAX	9	/* reserved max space id for system tablespaces */
#include "fsp0fsp.h"
#define	TRX_SYS_PAGE_NO	FSP_TRX_SYS_PAGE_NO

/* The offset of the transaction system header on the page */
#define	TRX_SYS		FSEG_PAGE_DATA

/** Transaction system header */
/*------------------------------------------------------------- @{ */
#define	TRX_SYS_TRX_ID_STORE	0	/*!< the maximum trx id or trx
					number modulo
					TRX_SYS_TRX_ID_UPDATE_MARGIN
					written to a file page by any
					transaction; the assignment of
					transaction ids continues from
					this number rounded up by
					TRX_SYS_TRX_ID_UPDATE_MARGIN
					plus
					TRX_SYS_TRX_ID_UPDATE_MARGIN
					when the database is
					started */
#define TRX_SYS_FSEG_HEADER	8	/*!< segment header for the
					tablespace segment the trx
					system is created into */
#define	TRX_SYS_RSEGS		(8 + FSEG_HEADER_SIZE)
					/*!< the start of the array of
					rollback segment specification
					slots */
/*------------------------------------------------------------- @} */

/* Max number of rollback segments: the number of segment specification slots
in the transaction system array; rollback segment id must fit in one (signed)
byte, therefore 128; each slot is currently 8 bytes in size. If you want
to raise the level to 256 then you will need to fix some assertions that
impose the 7 bit restriction. e.g., mach_write_to_3() */
#define	TRX_SYS_N_RSEGS			128
/* Originally, InnoDB defined TRX_SYS_N_RSEGS as 256 but created only one
rollback segment.  It initialized some arrays with this number of entries.
We must remember this limit in order to keep file compatibility. */
#define TRX_SYS_OLD_N_RSEGS		256

/** Maximum length of MySQL binlog file name, in bytes.
@see trx_sys_mysql_master_log_name
@see trx_sys_mysql_bin_log_name */
#define TRX_SYS_MYSQL_LOG_NAME_LEN	512
#define TRX_SYS_MYSQL_MASTER_LOG_NAME_LEN	480	/* (500 - 12) is dead line. */
/** Contents of TRX_SYS_MYSQL_LOG_MAGIC_N_FLD */
#define TRX_SYS_MYSQL_LOG_MAGIC_N	873422344

//#if UNIV_PAGE_SIZE < 4096
//# error "UNIV_PAGE_SIZE < 4096"
//#endif
/** The offset of the MySQL replication info in the trx system header;
this contains the same fields as TRX_SYS_MYSQL_LOG_INFO below.  These are
written at prepare time and are the main copy. */
#define TRX_SYS_MYSQL_MASTER_LOG_INFO	(UNIV_PAGE_SIZE - 2000)
#define TRX_SYS_MYSQL_RELAY_LOG_INFO	(UNIV_PAGE_SIZE - 1500)

/** The copy of the above which is made at transaction COMMIT time. If binlog
crash recovery rollbacks a PREPAREd transaction, they are copied back. */
#define TRX_SYS_COMMIT_MASTER_LOG_INFO	(UNIV_PAGE_SIZE - 3000)
#define TRX_SYS_COMMIT_RELAY_LOG_INFO	(UNIV_PAGE_SIZE - 2500)

/** The offset of the MySQL binlog offset info in the trx system header */
#define TRX_SYS_MYSQL_LOG_INFO		(UNIV_PAGE_SIZE - 1000)
#define	TRX_SYS_MYSQL_LOG_MAGIC_N_FLD	0	/*!< magic number which is
						TRX_SYS_MYSQL_LOG_MAGIC_N
						if we have valid data in the
						MySQL binlog info */
#define TRX_SYS_MYSQL_LOG_OFFSET_HIGH	4	/*!< high 4 bytes of the offset
						within that file */
#define TRX_SYS_MYSQL_LOG_OFFSET_LOW	8	/*!< low 4 bytes of the offset
						within that file */
#define TRX_SYS_MYSQL_LOG_NAME		12	/*!< MySQL log file name */

/** Doublewrite buffer */
/* @{ */
/** The offset of the doublewrite buffer header on the trx system header page */
#define TRX_SYS_DOUBLEWRITE		(UNIV_PAGE_SIZE - 200)
/*-------------------------------------------------------------*/
#define TRX_SYS_DOUBLEWRITE_FSEG	0	/*!< fseg header of the fseg
						containing the doublewrite
						buffer */
#define TRX_SYS_DOUBLEWRITE_MAGIC	FSEG_HEADER_SIZE
						/*!< 4-byte magic number which
						shows if we already have
						created the doublewrite
						buffer */
#define TRX_SYS_DOUBLEWRITE_BLOCK1	(4 + FSEG_HEADER_SIZE)
						/*!< page number of the
						first page in the first
						sequence of 64
						(= FSP_EXTENT_SIZE) consecutive
						pages in the doublewrite
						buffer */
#define TRX_SYS_DOUBLEWRITE_BLOCK2	(8 + FSEG_HEADER_SIZE)
						/*!< page number of the
						first page in the second
						sequence of 64 consecutive
						pages in the doublewrite
						buffer */
#define TRX_SYS_DOUBLEWRITE_REPEAT	12	/*!< we repeat
						TRX_SYS_DOUBLEWRITE_MAGIC,
						TRX_SYS_DOUBLEWRITE_BLOCK1,
						TRX_SYS_DOUBLEWRITE_BLOCK2
						so that if the trx sys
						header is half-written
						to disk, we still may
						be able to recover the
						information */
/** If this is not yet set to TRX_SYS_DOUBLEWRITE_SPACE_ID_STORED_N,
we must reset the doublewrite buffer, because starting from 4.1.x the
space id of a data page is stored into
FIL_PAGE_ARCH_LOG_NO_OR_SPACE_NO. */
#define TRX_SYS_DOUBLEWRITE_SPACE_ID_STORED (24 + FSEG_HEADER_SIZE)

/*-------------------------------------------------------------*/
/** Contents of TRX_SYS_DOUBLEWRITE_MAGIC */
#define TRX_SYS_DOUBLEWRITE_MAGIC_N	536853855
/** Contents of TRX_SYS_DOUBLEWRITE_SPACE_ID_STORED */
#define TRX_SYS_DOUBLEWRITE_SPACE_ID_STORED_N 1783657386

/** Size of the doublewrite block in pages */
#define TRX_SYS_DOUBLEWRITE_BLOCK_SIZE	FSP_EXTENT_SIZE
/* @} */

/** File format tag */
/* @{ */
/** The offset of the file format tag on the trx system header page
(TRX_SYS_PAGE_NO of TRX_SYS_SPACE) */
#define TRX_SYS_FILE_FORMAT_TAG		(UNIV_PAGE_SIZE - 16)

/** Contents of TRX_SYS_FILE_FORMAT_TAG when valid. The file format
identifier is added to this constant. */
#define TRX_SYS_FILE_FORMAT_TAG_MAGIC_N_LOW	3645922177UL
/** Contents of TRX_SYS_FILE_FORMAT_TAG+4 when valid */
#define TRX_SYS_FILE_FORMAT_TAG_MAGIC_N_HIGH	2745987765UL
/** Contents of TRX_SYS_FILE_FORMAT_TAG when valid. The file format
identifier is added to this 64-bit constant. */
#define TRX_SYS_FILE_FORMAT_TAG_MAGIC_N					\
	((ib_uint64_t) TRX_SYS_FILE_FORMAT_TAG_MAGIC_N_HIGH << 32	\
	 | TRX_SYS_FILE_FORMAT_TAG_MAGIC_N_LOW)
/* @} */

#ifndef UNIV_HOTBACKUP
/** Doublewrite control struct */
struct trx_doublewrite_struct{
	mutex_t	mutex;		/*!< mutex protecting the first_free field and
				write_buf */
	ulint	block1;		/*!< the page number of the first
				doublewrite block (64 pages) */
	ulint	block2;		/*!< page number of the second block */
	ulint	first_free;	/*!< first free position in write_buf measured
				in units of UNIV_PAGE_SIZE */
	byte*	write_buf;	/*!< write buffer used in writing to the
				doublewrite buffer, aligned to an
				address divisible by UNIV_PAGE_SIZE
				(which is required by Windows aio) */
	byte*	write_buf_unaligned;
				/*!< pointer to write_buf, but unaligned */
	buf_page_t**
		buf_block_arr;	/*!< array to store pointers to the buffer
				blocks which have been cached to write_buf */
};

/** The transaction system central memory data structure; protected by the
kernel mutex */
struct trx_sys_struct{
	trx_id_t	max_trx_id;	/*!< The smallest number not yet
					assigned as a transaction id or
					transaction number */
	UT_LIST_BASE_NODE_T(trx_t) trx_list;
					/*!< List of active and committed in
					memory transactions, sorted on trx id,
					biggest first */
	UT_LIST_BASE_NODE_T(trx_t) mysql_trx_list;
					/*!< List of transactions created
					for MySQL */
	UT_LIST_BASE_NODE_T(trx_rseg_t) rseg_list;
					/*!< List of rollback segment
					objects */
	trx_rseg_t*	latest_rseg;	/*!< Latest rollback segment in the
					round-robin assignment of rollback
					segments to transactions */
	trx_rseg_t*	rseg_array[TRX_SYS_N_RSEGS];
					/*!< Pointer array to rollback
					segments; NULL if slot not in use */
	ulint		rseg_history_len;/*!< Length of the TRX_RSEG_HISTORY
					list (update undo logs for committed
					transactions), protected by
					rseg->mutex */
	UT_LIST_BASE_NODE_T(read_view_t) view_list;
					/*!< List of read views sorted
					on trx no, biggest first */
};

/** When a trx id which is zero modulo this number (which must be a power of
two) is assigned, the field TRX_SYS_TRX_ID_STORE on the transaction system
page is updated */
#define TRX_SYS_TRX_ID_WRITE_MARGIN	256
#endif /* !UNIV_HOTBACKUP */

#ifndef UNIV_NONINL
#include "trx0sys.ic"
#endif

#endif
