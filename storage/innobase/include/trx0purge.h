/*****************************************************************************

Copyright (c) 1996, 2013, Oracle and/or its affiliates. All Rights Reserved.

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
@file include/trx0purge.h
Purge old versions

Created 3/26/1996 Heikki Tuuri
*******************************************************/

#ifndef trx0purge_h
#define trx0purge_h

#include "univ.i"
#include "trx0types.h"
#include "mtr0mtr.h"
#include "trx0sys.h"
#include "que0types.h"
#include "page0page.h"
#include "usr0sess.h"
#include "fil0fil.h"
#include "read0types.h"

// Forward declaration
struct ib_bh_t;

/** The global data structure coordinating a purge */
extern trx_purge_t*	purge_sys;

/** A dummy undo record used as a return value when we have a whole undo log
which needs no purge */
extern trx_undo_rec_t	trx_purge_dummy_rec;

/********************************************************************//**
Calculates the file address of an undo log header when we have the file
address of its history list node.
@return file address of the log */
UNIV_INLINE
fil_addr_t
trx_purge_get_log_from_hist(
/*========================*/
	fil_addr_t	node_addr);	/*!< in: file address of the history
					list node of the log */
/********************************************************************//**
Creates the global purge system control structure and inits the history
mutex. */

void
trx_purge_sys_create(
/*=================*/
	ulint		n_purge_threads,/*!< in: number of purge threads */
	purge_pq_t*	purge_queue);	/*!< in/own: UNDO log min binary heap*/
/********************************************************************//**
Frees the global purge system control structure. */

void
trx_purge_sys_close(void);
/*======================*/
/************************************************************************
Adds the update undo log as the first log in the history list. Removes the
update undo log segment from the rseg slot if it is too big for reuse. */

void
trx_purge_add_update_undo_to_history(
/*=================================*/
	trx_t*		trx,		/*!< in: transaction */
	trx_undo_ptr_t*	undo_ptr,	/*!< in: update undo log. */
	page_t*		undo_page,	/*!< in: update undo log header page,
					x-latched */
	bool		update_rseg_history_len,
					/*!< in: if true: update rseg history
					len else skip updating it. */
	ulint		n_added_logs,	/*!< in: number of logs added */
	mtr_t*		mtr);		/*!< in: mtr */
/*******************************************************************//**
This function runs a purge batch.
@return number of undo log pages handled in the batch */

ulint
trx_purge(
/*======*/
	ulint	n_purge_threads,	/*!< in: number of purge tasks to
					submit to task queue. */
	ulint	limit,			/*!< in: the maximum number of
					records to purge in one batch */
	bool	truncate);		/*!< in: truncate history if true */
/*******************************************************************//**
Stop purge and wait for it to stop, move to PURGE_STATE_STOP. */

void
trx_purge_stop(void);
/*================*/
/*******************************************************************//**
Resume purge, move to PURGE_STATE_RUN. */

void
trx_purge_run(void);
/*================*/

/** Purge states */
enum purge_state_t {
	PURGE_STATE_INIT,		/*!< Purge instance created */
	PURGE_STATE_RUN,		/*!< Purge should be running */
	PURGE_STATE_STOP,		/*!< Purge should be stopped */
	PURGE_STATE_EXIT,		/*!< Purge has been shutdown */
	PURGE_STATE_DISABLED		/*!< Purge was never started */
};

/*******************************************************************//**
Get the purge state.
@return purge state. */

purge_state_t
trx_purge_state(void);
/*=================*/

// Forward declaration
struct TrxUndoRsegsIterator;

/** This is the purge pointer/iterator. We need both the undo no and the
transaction no up to which purge has parsed and applied the records. */
struct purge_iter_t {
	purge_iter_t()
		:
		trx_no(),
		undo_no(),
		undo_rseg_space(ULINT_UNDEFINED)
	{
		// Do nothing
	}

	trx_id_t	trx_no;		/*!< Purge has advanced past all
					transactions whose number is less
					than this */
	undo_no_t	undo_no;	/*!< Purge has advanced past all records
					whose undo number is less than this */
	ulint		undo_rseg_space;
					/*!< Last undo record resided in this
					space id. */
};

/** UNDO log truncate logger. Needed to track state of truncate
during crash. A DDL_LOG kind of file will be created that will be removed
on successful truncate but if server crashes before successful truncate
this file will be used to initiate fix-up action during server start. */

class undo_trunc_logger_t {
public:
	undo_trunc_logger_t()
		:
		m_log_file_name()
	{ /* Do nothing. */ }


	/** Create the truncate log file.
	@param[in]	space_id	id of the undo tablespace to truncate.
	@return DB_SUCCESS or error code. */
	dberr_t init(ulint space_id)
	{
		/* Step-1: Create the log file name using the pre-decided
		prefix/suffix and table id of undo tablepsace to truncate. */
		ulint log_file_name_sz = 
			strlen(srv_log_group_home_dir) + 1 + 22 + 1 /* NUL */
			+ strlen(undo_trunc_logger_t::s_log_prefix)
			+ strlen(undo_trunc_logger_t::s_log_ext);

		m_log_file_name = new (std::nothrow) char[log_file_name_sz];
		if (m_log_file_name == 0) {
			return(DB_OUT_OF_MEMORY);
		}

		memset(m_log_file_name, 0, log_file_name_sz);

		strcpy(m_log_file_name, srv_log_group_home_dir);
		ulint	log_file_name_len = strlen(m_log_file_name);

		if (m_log_file_name[log_file_name_len - 1]
				!= OS_PATH_SEPARATOR) {

			m_log_file_name[log_file_name_len]
				= OS_PATH_SEPARATOR;
			log_file_name_len = strlen(m_log_file_name);
		}

		ut_snprintf(m_log_file_name + log_file_name_len,
			    log_file_name_sz - log_file_name_len,
			    "%s_%lu_%s",
			    undo_trunc_logger_t::s_log_prefix,
			    (ulong) space_id, undo_trunc_logger_t::s_log_ext);

		/* Step-2: Create the log file, open it and write magic
		number of 0 to indicate init phase. */
		bool            ret;
		os_file_t	handle = os_file_create(
			innodb_log_file_key, m_log_file_name, OS_FILE_CREATE,
			OS_FILE_NORMAL, OS_LOG_FILE, srv_read_only_mode, &ret);
		if (!ret) {
			return(DB_IO_ERROR);
		}

		byte	buffer[sizeof(undo_trunc_logger_t::s_magic)];
		memset(buffer, 0x0, sizeof(buffer));

                os_file_write(
                        m_log_file_name, handle, buffer, 0, sizeof(buffer));

		os_file_flush(handle);
		os_file_close(handle);

		return(DB_SUCCESS);
	}

	/** Mark completion of undo truncate action by writing magic number to
	the log file and then removing it from the disk.
	If we are going to remove it from disk then why write magic number ?
	This is to safeguard from unlink (file-system) anomalies that will keep
	the link to the file even after unlink action is successfull and
	ref-count = 0. */
	void done()
	{
		bool    ret;
		os_file_t	handle =
			os_file_create_simple_no_error_handling(
				innodb_log_file_key, m_log_file_name,
				OS_FILE_OPEN, OS_FILE_READ_WRITE,
				srv_read_only_mode, &ret);

		if (!ret) {
			os_file_delete(innodb_log_file_key, m_log_file_name);
			delete[] m_log_file_name;
			m_log_file_name = NULL;
			return;
		}

		byte	buffer[sizeof(undo_trunc_logger_t::s_magic)];
		mach_write_to_4(buffer, undo_trunc_logger_t::s_magic);

                os_file_write(
                        m_log_file_name, handle, buffer, 0, sizeof(buffer));

		os_file_flush(handle);
		os_file_close(handle);

		os_file_delete(innodb_log_file_key, m_log_file_name);
		delete[] m_log_file_name;
		m_log_file_name = NULL;
	}

public:
        /** Magic Number to indicate truncate action is complete. */
        const static ib_uint32_t	s_magic;

        /** Truncate Log file Prefix. */
        const static char*		s_log_prefix;

        /** Truncate Log file Extension. */
        const static char*		s_log_ext;

private:
	char*				m_log_file_name;
};

/** Track UNDO tablespace mark for truncate. */
class undo_trunc_t {
public:
	typedef	std::vector<trx_rseg_t*>	rseg_for_trunc_t;

	undo_trunc_t()
		:
		undo_logger(),
		m_undo_for_trunc(ULINT_UNDEFINED),
		m_rseg_for_trunc(),
		m_scan_start(1)
	{
		/* Do Nothing. */
	}

	/** Is tablespace selected for truncate.
	@return true if undo tablespace is marked for truncate */
	bool is_undo_marked_for_trunc()
	{
		return(m_undo_for_trunc == ULINT_UNDEFINED ? false : true);
	}

	/** Mark the tablespace for truncate.
	@param[in]	undo_id		tablespace for truncate. */
	void mark_for_trunc(
		ulint	undo_id)
	{
		m_undo_for_trunc = undo_id;

		m_scan_start = (undo_id + 1) % (srv_undo_tablespaces_open + 1);
		if (m_scan_start == 0) {
			/* Note: UNDO tablespace ids starts from 1. */
			m_scan_start = 1;
		}
	}

	/** Get the tablespace marked for truncate.
	@return tablespace id marked for truncate. */
	ulint get_undo_mark_for_trunc()
	{
		return m_undo_for_trunc;
	}

	/** Add rseg to truncate vector. 
	@param[in,out]	rseg	rseg for truncate */
	void add_rseg_to_trunc(
		trx_rseg_t*	rseg)
	{
		m_rseg_for_trunc.push_back(rseg);
	}

	/** Get number of rsegs registered for truncate.
	@return return number of rseg that belongs to tablespace mark for
	truncate. */
	ulint get_no_of_rsegs()
	{
		return(m_rseg_for_trunc.size());
	}

	/** Get ith registered rseg.
	@return reference to registered rseg. */
	trx_rseg_t* get_ith_rseg(ulint id)
	{
		ut_ad(id < m_rseg_for_trunc.size());
		return(m_rseg_for_trunc.at(id));
	}

	/** Reset for next rseg truncate. */
	void reset()
	{
		m_undo_for_trunc = ULINT_UNDEFINED;
		m_rseg_for_trunc.clear();
	}

	/** Get the tablespace id to start scanning from.
	@return	id of UNDO tablespace to start scanning from. */
	ulint scan_start()
	{
		return(m_scan_start);
	}	

public:
	undo_trunc_logger_t	undo_logger;

private:	
	/** UNDO tablespace is mark for truncate. */
	ulint			m_undo_for_trunc;

	/** rseg that resides in UNDO tablespace marked for truncate. */
	rseg_for_trunc_t	m_rseg_for_trunc;

	/** Start scanning for UNDO tablespace from this space_id.
	This is to avoid bias selection of one tablespace always. */
	ulint			m_scan_start;
};

/** The control structure used in the purge operation */
struct trx_purge_t{
	sess_t*		sess;		/*!< System session running the purge
					query */
	trx_t*		trx;		/*!< System transaction running the
					purge query: this trx is not in the
					trx list of the trx system and it
					never ends */
	rw_lock_t	latch;		/*!< The latch protecting the purge
					view. A purge operation must acquire an
					x-latch here for the instant at which
					it changes the purge view: an undo
					log operation can prevent this by
					obtaining an s-latch here. It also
					protects state and running */
	os_event_t	event;		/*!< State signal event */
	ulint		n_stop;		/*!< Counter to track number stops */
	volatile bool	running;	/*!< true, if purge is active,
					we check this without the latch too */
	volatile purge_state_t	state;	/*!< Purge coordinator thread states,
					we check this in several places
					without holding the latch. */
	que_t*		query;		/*!< The query graph which will do the
					parallelized purge operation */
	ReadView	view;		/*!< The purge will not remove undo logs
					which are >= this view (purge view) */
	bool		view_active;	/*!< true if view is active */
	volatile ulint	n_submitted;	/*!< Count of total tasks submitted
					to the task queue */
	volatile ulint	n_completed;	/*!< Count of total tasks completed */

	/*------------------------------*/
	/* The following two fields form the 'purge pointer' which advances
	during a purge, and which is used in history list truncation */

	purge_iter_t	iter;		/* Limit up to which we have read and
					parsed the UNDO log records.  Not
					necessarily purged from the indexes.
					Note that this can never be less than
					the limit below, we check for this
					invariant in trx0purge.cc */
	purge_iter_t	limit;		/* The 'purge pointer' which advances
					during a purge, and which is used in
					history list truncation */
#ifdef UNIV_DEBUG
	purge_iter_t	done;		/* Indicate 'purge pointer' which have
					purged already accurately. */
#endif /* UNIV_DEBUG */
	/*-----------------------------*/
	ibool		next_stored;	/*!< TRUE if the info of the next record
					to purge is stored below: if yes, then
					the transaction number and the undo
					number of the record are stored in
					purge_trx_no and purge_undo_no above */
	trx_rseg_t*	rseg;		/*!< Rollback segment for the next undo
					record to purge */
	ulint		page_no;	/*!< Page number for the next undo
					record to purge, page number of the
					log header, if dummy record */
	ulint		offset;		/*!< Page offset for the next undo
					record to purge, 0 if the dummy
					record */
	ulint		hdr_page_no;	/*!< Header page of the undo log where
					the next record to purge belongs */
	ulint		hdr_offset;	/*!< Header byte offset on the page */


	TrxUndoRsegsIterator*
			rseg_iter;	/*!< Iterator to get the next rseg
					to process */

	/*-----------------------------*/
	ib_bh_t*	ib_bh;		/*!< Binary min-heap, ordered on
					rseg_queue_t::trx_no. It is protected
					by the bh_mutex */
	purge_pq_t*	purge_queue;	/*!< Binary min-heap, ordered on
					TrxUndoRsegs::trx_no. It is protected
					by the pq_mutex */
	PQMutex		pq_mutex;	/*!< Mutex protecting purge_queue */

	undo_trunc_t    undo_trunc;	/*!< Track UNDO tablespace marked
					for truncate. */
};

/** Info required to purge a record */
struct trx_purge_rec_t {
	trx_undo_rec_t*	undo_rec;	/*!< Record to purge */
	roll_ptr_t	roll_ptr;	/*!< File pointr to UNDO record */
};

/**
Chooses the rollback segment with the smallest trx_no. */
struct TrxUndoRsegsIterator {

	/** Constructor */
	TrxUndoRsegsIterator(trx_purge_t* purge_sys);

	/** Sets the next rseg to purge in m_purge_sys.
	@return page size of the table for which the log is.
	NOTE: if rseg is NULL when this function returns this means that
	there are no rollback segments to purge and then the returned page
	size object should not be used. */
	const page_size_t set_next();

private:
	// Disable copying
	TrxUndoRsegsIterator(const TrxUndoRsegsIterator&);
	TrxUndoRsegsIterator& operator=(const TrxUndoRsegsIterator&);

	/** The purge system pointer */
	trx_purge_t*			m_purge_sys;

	/** The current element to process */
	TrxUndoRsegs			m_trx_undo_rsegs;

	/** Track the current element in m_trx_undo_rseg */
	TrxUndoRsegs::iterator		m_iter;

	/** Sentinel value */
	static const TrxUndoRsegs	NullElement;
};

#ifndef UNIV_NONINL
#include "trx0purge.ic"
#endif /* UNIV_NOINL */

#endif /* trx0purge_h */
