/******************************************************
Transaction system

(c) 1996 Innobase Oy

Created 3/26/1996 Heikki Tuuri
*******************************************************/

#ifndef trx0sys_h
#define trx0sys_h

#include "univ.i"

#include "trx0types.h"
#include "mtr0mtr.h"
#include "mtr0log.h"
#include "ut0byte.h"
#include "mem0mem.h"
#include "sync0sync.h"
#include "ut0lst.h"
#include "buf0buf.h"
#include "fil0fil.h"
#include "fut0lst.h"
#include "fsp0fsp.h"
#include "read0types.h"

/* In a MySQL replication slave, in crash recovery we store the master log
file name and position here. We have successfully got the updates to InnoDB
up to this position. If .._pos is -1, it means no crash recovery was needed,
or there was no master log position info inside InnoDB. */

extern char 		trx_sys_mysql_master_log_name[];
extern ib_longlong	trx_sys_mysql_master_log_pos;

/* If this MySQL server uses binary logging, after InnoDB has been inited
and if it has done a crash recovery, we store the binlog file name and position
here. If .._pos is -1, it means there was no binlog position info inside
InnoDB. */

extern char 		trx_sys_mysql_bin_log_name[];
extern ib_longlong	trx_sys_mysql_bin_log_pos;

/* The transaction system */
extern trx_sys_t*	trx_sys;

/* Doublewrite system */
extern trx_doublewrite_t*	trx_doublewrite;
extern ibool			trx_doublewrite_must_reset_space_ids;
extern ibool			trx_sys_multiple_tablespace_format;

/********************************************************************
Creates the doublewrite buffer to a new InnoDB installation. The header of the
doublewrite buffer is placed on the trx system header page. */

void
trx_sys_create_doublewrite_buf(void);
/*================================*/
/********************************************************************
At a database startup initializes the doublewrite buffer memory structure if
we already have a doublewrite buffer created in the data files. If we are
upgrading to an InnoDB version which supports multiple tablespaces, then this
function performs the necessary update operations. If we are in a crash
recovery, this function uses a possible doublewrite buffer to restore
half-written pages in the data files. */

void
trx_sys_doublewrite_init_or_restore_pages(
/*======================================*/
	ibool	restore_corrupt_pages);
/********************************************************************
Marks the trx sys header when we have successfully upgraded to the >= 4.1.x
multiple tablespace format. */

void
trx_sys_mark_upgraded_to_multiple_tablespaces(void);
/*===============================================*/
/********************************************************************
Determines if a page number is located inside the doublewrite buffer. */

ibool
trx_doublewrite_page_inside(
/*========================*/
				/* out: TRUE if the location is inside
				the two blocks of the doublewrite buffer */
	ulint	page_no);	/* in: page number */
/*******************************************************************
Checks if a page address is the trx sys header page. */
UNIV_INLINE
ibool
trx_sys_hdr_page(
/*=============*/
			/* out: TRUE if trx sys header page */
	ulint	space,	/* in: space */
	ulint	page_no);/* in: page number */
/*********************************************************************
Creates and initializes the central memory structures for the transaction
system. This is called when the database is started. */

void
trx_sys_init_at_db_start(void);
/*==========================*/
/*********************************************************************
Creates and initializes the transaction system at the database creation. */

void
trx_sys_create(void);
/*================*/
/********************************************************************
Looks for a free slot for a rollback segment in the trx system file copy. */

ulint
trx_sysf_rseg_find_free(
/*====================*/
					/* out: slot index or ULINT_UNDEFINED
					if not found */
	mtr_t*		mtr);		/* in: mtr */
/*******************************************************************
Gets the pointer in the nth slot of the rseg array. */
UNIV_INLINE
trx_rseg_t*
trx_sys_get_nth_rseg(
/*=================*/
				/* out: pointer to rseg object, NULL if slot
				not in use */
	trx_sys_t*	sys,	/* in: trx system */
	ulint		n);	/* in: index of slot */
/*******************************************************************
Sets the pointer in the nth slot of the rseg array. */
UNIV_INLINE
void
trx_sys_set_nth_rseg(
/*=================*/
	trx_sys_t*	sys,	/* in: trx system */
	ulint		n,	/* in: index of slot */
	trx_rseg_t*	rseg);	/* in: pointer to rseg object, NULL if slot
				not in use */
/**************************************************************************
Gets a pointer to the transaction system file copy and x-locks its page. */
UNIV_INLINE
trx_sysf_t*
trx_sysf_get(
/*=========*/
			/* out: pointer to system file copy, page x-locked */
	mtr_t*	mtr);	/* in: mtr */
/*********************************************************************
Gets the space of the nth rollback segment slot in the trx system
file copy. */
UNIV_INLINE
ulint
trx_sysf_rseg_get_space(
/*====================*/
					/* out: space id */
	trx_sysf_t*	sys_header,	/* in: trx sys file copy */
	ulint		i,		/* in: slot index == rseg id */
	mtr_t*		mtr);		/* in: mtr */
/*********************************************************************
Gets the page number of the nth rollback segment slot in the trx system
file copy. */
UNIV_INLINE
ulint
trx_sysf_rseg_get_page_no(
/*======================*/
					/* out: page number, FIL_NULL
					if slot unused */
	trx_sysf_t*	sys_header,	/* in: trx sys file copy */
	ulint		i,		/* in: slot index == rseg id */
	mtr_t*		mtr);		/* in: mtr */
/*********************************************************************
Sets the space id of the nth rollback segment slot in the trx system
file copy. */
UNIV_INLINE
void
trx_sysf_rseg_set_space(
/*====================*/
	trx_sysf_t*	sys_header,	/* in: trx sys file copy */
	ulint		i,		/* in: slot index == rseg id */
	ulint		space,		/* in: space id */
	mtr_t*		mtr);		/* in: mtr */
/*********************************************************************
Sets the page number of the nth rollback segment slot in the trx system
file copy. */
UNIV_INLINE
void
trx_sysf_rseg_set_page_no(
/*======================*/
	trx_sysf_t*	sys_header,	/* in: trx sys file copy */
	ulint		i,		/* in: slot index == rseg id */
	ulint		page_no,	/* in: page number, FIL_NULL if
					the slot is reset to unused */
	mtr_t*		mtr);		/* in: mtr */
/*********************************************************************
Allocates a new transaction id. */
UNIV_INLINE
dulint
trx_sys_get_new_trx_id(void);
/*========================*/
			/* out: new, allocated trx id */
/*********************************************************************
Allocates a new transaction number. */
UNIV_INLINE
dulint
trx_sys_get_new_trx_no(void);
/*========================*/
			/* out: new, allocated trx number */
/*********************************************************************
Writes a trx id to an index page. In case that the id size changes in
some future version, this function should be used instead of
mach_write_... */
UNIV_INLINE
void
trx_write_trx_id(
/*=============*/
	byte*	ptr,	/* in: pointer to memory where written */
	dulint	id);	/* in: id */
/*********************************************************************
Reads a trx id from an index page. In case that the id size changes in
some future version, this function should be used instead of
mach_read_... */
UNIV_INLINE
dulint
trx_read_trx_id(
/*============*/
			/* out: id */
	byte*	ptr);	/* in: pointer to memory from where to read */
/********************************************************************
Looks for the trx handle with the given id in trx_list. */
UNIV_INLINE
trx_t*
trx_get_on_id(
/*==========*/
			/* out: the trx handle or NULL if not found */
	dulint	trx_id);	/* in: trx id to search for */
/********************************************************************
Returns the minumum trx id in trx list. This is the smallest id for which
the trx can possibly be active. (But, you must look at the trx->conc_state to
find out if the minimum trx id transaction itself is active, or already
committed.) */
UNIV_INLINE
dulint
trx_list_get_min_trx_id(void);
/*=========================*/
			/* out: the minimum trx id, or trx_sys->max_trx_id
			if the trx list is empty */
/********************************************************************
Checks if a transaction with the given id is active. */
UNIV_INLINE
ibool
trx_is_active(
/*==========*/
			/* out: TRUE if active */
	dulint	trx_id);/* in: trx id of the transaction */
/********************************************************************
Checks that trx is in the trx list. */

ibool
trx_in_trx_list(
/*============*/
			/* out: TRUE if is in */
	trx_t*	in_trx);/* in: trx */
/*********************************************************************
Updates the offset information about the end of the MySQL binlog entry
which corresponds to the transaction just being committed. In a MySQL
replication slave updates the latest master binlog position up to which
replication has proceeded. */

void
trx_sys_update_mysql_binlog_offset(
/*===============================*/
	const char*	file_name,/* in: MySQL log file name */
	ib_longlong	offset,	/* in: position in that log file */
	ulint		field,	/* in: offset of the MySQL log info field in
				the trx sys header */
	mtr_t*		mtr);	/* in: mtr */
/*********************************************************************
Prints to stderr the MySQL binlog offset info in the trx system header if
the magic number shows it valid. */

void
trx_sys_print_mysql_binlog_offset(void);
/*===================================*/
#ifdef UNIV_HOTBACKUP
/*********************************************************************
Prints to stderr the MySQL binlog info in the system header if the
magic number shows it valid. */

void
trx_sys_print_mysql_binlog_offset_from_page(
/*========================================*/
	byte*	page);	/* in: buffer containing the trx system header page,
			i.e., page number TRX_SYS_PAGE_NO in the tablespace */
#endif /* UNIV_HOTBACKUP */
/*********************************************************************
Prints to stderr the MySQL master log offset info in the trx system header if
the magic number shows it valid. */

void
trx_sys_print_mysql_master_log_pos(void);
/*====================================*/

/* The automatically created system rollback segment has this id */
#define TRX_SYS_SYSTEM_RSEG_ID	0

/* Space id and page no where the trx system file copy resides */
#define	TRX_SYS_SPACE	0	/* the SYSTEM tablespace */
#define	TRX_SYS_PAGE_NO	FSP_TRX_SYS_PAGE_NO

/* The offset of the transaction system header on the page */
#define	TRX_SYS		FSEG_PAGE_DATA

/* Transaction system header */
/*-------------------------------------------------------------*/
#define	TRX_SYS_TRX_ID_STORE	0	/* the maximum trx id or trx number
					modulo TRX_SYS_TRX_ID_UPDATE_MARGIN
					written to a file page by any
					transaction; the assignment of
					transaction ids continues from this
					number rounded up by .._MARGIN plus
					.._MARGIN when the database is
					started */
#define TRX_SYS_FSEG_HEADER	8	/* segment header for the tablespace
					segment the trx system is created
					into */
#define	TRX_SYS_RSEGS		(8 + FSEG_HEADER_SIZE)	
					/* the start of the array of rollback
					segment specification slots */
/*-------------------------------------------------------------*/

/* Max number of rollback segments: the number of segment specification slots
in the transaction system array; rollback segment id must fit in one byte,
therefore 256; each slot is currently 8 bytes in size */
#define	TRX_SYS_N_RSEGS		256

#define TRX_SYS_MYSQL_LOG_NAME_LEN	512
#define TRX_SYS_MYSQL_LOG_MAGIC_N	873422344

/* The offset of the MySQL replication info in the trx system header;
this contains the same fields as TRX_SYS_MYSQL_LOG_INFO below */
#define TRX_SYS_MYSQL_MASTER_LOG_INFO	(UNIV_PAGE_SIZE - 2000)

/* The offset of the MySQL binlog offset info in the trx system header */
#define TRX_SYS_MYSQL_LOG_INFO		(UNIV_PAGE_SIZE - 1000)
#define	TRX_SYS_MYSQL_LOG_MAGIC_N_FLD	0	/* magic number which shows
						if we have valid data in the
						MySQL binlog info; the value
						is ..._MAGIC_N if yes */
#define TRX_SYS_MYSQL_LOG_OFFSET_HIGH	4	/* high 4 bytes of the offset
						within that file */
#define TRX_SYS_MYSQL_LOG_OFFSET_LOW	8	/* low 4 bytes of the offset
						within that file */
#define TRX_SYS_MYSQL_LOG_NAME		12	/* MySQL log file name */

/* The offset of the doublewrite buffer header on the trx system header page */
#define TRX_SYS_DOUBLEWRITE		(UNIV_PAGE_SIZE - 200)
/*-------------------------------------------------------------*/
#define TRX_SYS_DOUBLEWRITE_FSEG 	0	/* fseg header of the fseg
						containing the doublewrite
						buffer */
#define TRX_SYS_DOUBLEWRITE_MAGIC 	FSEG_HEADER_SIZE
						/* 4-byte magic number which
						shows if we already have
						created the doublewrite
						buffer */
#define TRX_SYS_DOUBLEWRITE_BLOCK1	(4 + FSEG_HEADER_SIZE)
						/* page number of the
						first page in the first
						sequence of 64
						(= FSP_EXTENT_SIZE) consecutive
						pages in the doublewrite
						buffer */
#define TRX_SYS_DOUBLEWRITE_BLOCK2	(8 + FSEG_HEADER_SIZE)
						/* page number of the
						first page in the second
						sequence of 64 consecutive
						pages in the doublewrite
						buffer */
#define TRX_SYS_DOUBLEWRITE_REPEAT	12	/* we repeat the above 3
						numbers so that if the trx
						sys header is half-written
						to disk, we still may be able
						to recover the information */
#define TRX_SYS_DOUBLEWRITE_SPACE_ID_STORED (24 + FSEG_HEADER_SIZE)
						/* If this is not yet set to
						.._N, we must reset the
						doublewrite buffer, because
						starting from 4.1.x the space
						id of a data page is stored to
					FIL_PAGE_ARCH_LOG_NO_OR_SPACE_NO */
/*-------------------------------------------------------------*/
#define TRX_SYS_DOUBLEWRITE_MAGIC_N	536853855
#define TRX_SYS_DOUBLEWRITE_SPACE_ID_STORED_N 1783657386


#define TRX_SYS_DOUBLEWRITE_BLOCK_SIZE	FSP_EXTENT_SIZE	

/* Doublewrite control struct */
struct trx_doublewrite_struct{
	mutex_t	mutex;		/* mutex protecting the first_free field and
				write_buf */
	ulint	block1;		/* the page number of the first
				doublewrite block (64 pages) */
	ulint	block2;		/* page number of the second block */
	ulint	first_free;	/* first free position in write_buf measured
				in units of UNIV_PAGE_SIZE */
	byte*	write_buf; 	/* write buffer used in writing to the
				doublewrite buffer, aligned to an
				address divisible by UNIV_PAGE_SIZE
				(which is required by Windows aio) */
	byte*	write_buf_unaligned; /* pointer to write_buf, but unaligned */
	buf_block_t**
		buf_block_arr;	/* array to store pointers to the buffer
				blocks which have been cached to write_buf */
};

/* The transaction system central memory data structure; protected by the
kernel mutex */
struct trx_sys_struct{
	dulint		max_trx_id;	/* The smallest number not yet
					assigned as a transaction id or
					transaction number */
	UT_LIST_BASE_NODE_T(trx_t) trx_list;
					/* List of active and committed in
					memory transactions, sorted on trx id,
					biggest first */
	UT_LIST_BASE_NODE_T(trx_t) mysql_trx_list;
					/* List of transactions created
					for MySQL */
	UT_LIST_BASE_NODE_T(trx_rseg_t) rseg_list;
					/* List of rollback segment objects */
	trx_rseg_t*	latest_rseg;	/* Latest rollback segment in the
					round-robin assignment of rollback
					segments to transactions */
	trx_rseg_t*	rseg_array[TRX_SYS_N_RSEGS];
					/* Pointer array to rollback segments;
					NULL if slot not in use */
	ulint		rseg_history_len;/* Length of the TRX_RSEG_HISTORY
					list (update undo logs for committed
					transactions), protected by
					rseg->mutex */
	UT_LIST_BASE_NODE_T(read_view_t) view_list;
					/* List of read views sorted on trx no,
					biggest first */
};

/* When a trx id which is zero modulo this number (which must be a power of
two) is assigned, the field TRX_SYS_TRX_ID_STORE on the transaction system
page is updated */
#define TRX_SYS_TRX_ID_WRITE_MARGIN	256

#ifndef UNIV_NONINL
#include "trx0sys.ic"
#endif

#endif 
