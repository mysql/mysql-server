/*****************************************************************************

Copyright (c) 1996, 2017, Oracle and/or its affiliates. All Rights Reserved.

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
#ifdef UNIV_HOTBACKUP
# include "trx0sys.h"
#endif  /* UNIV_HOTBACKUP */

/** The global data structure coordinating a purge */
extern trx_purge_t*	purge_sys;

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
		undo_rseg_space(SPACE_UNKNOWN)
	{
		// Do nothing
	}

	trx_id_t	trx_no;		/*!< Purge has advanced past all
					transactions whose number is less
					than this */
	undo_no_t	undo_no;	/*!< Purge has advanced past all records
					whose undo number is less than this */
	space_id_t	undo_rseg_space;
					/*!< Last undo record resided in this
					space id. */
	trx_id_t	modifier_trx_id;
					/*!< the transaction that created the
					undo log record. Modifier trx id.*/
};

/* Namespace to hold all the related functions and variables needed
to truncate an undo tablespace. */
namespace undo {

	/** Magic Number to indicate truncate action is complete. */
	const ib_uint32_t			s_magic = 76845412;

	/** Truncate Log file Prefix. */
	const char* const			s_log_prefix = "undo_";

	/** Truncate Log file Extension. */
	const char* const			s_log_ext = "trunc.log";

	/** Check if the space_id is an undo space ID in the reserved range.
	@param[in]	space_id	undo tablespace ID
	@return true if it is in the reserved undo space ID range. */
	inline bool is_reserved(space_id_t space_id)
	{
		return(space_id >= dict_sys_t::s_min_undo_space_id
		       && space_id <= dict_sys_t::s_max_undo_space_id);
	}

	/** Convert an undo space number (from 1 to 127) into an undo space_id.
	@param[in]	space_num	undo tablespace number
	@return space_id of the undo tablespace */
	inline space_id_t num2id(space_id_t space_num)
	{
		ut_ad(space_num > 0);
		ut_ad(space_num <= FSP_MAX_UNDO_TABLESPACES);

		return(static_cast<space_id_t>(
				dict_sys_t::s_log_space_first_id - space_num));
	}

	/** Convert an undo space ID into an undo space number.
	NOTE: This may be an undo space_id from a pre-exisiting 5.7
	database which used space_ids from 1 to 128.  If so, the
	space_id is the space_num.
	@param[in]	space_id	undo tablespace ID
	@return space number of the undo tablespace */
	inline space_id_t id2num(space_id_t space_id)
	{
		if (!is_reserved(space_id)) {
			return(space_id);
		}

		return(dict_sys_t::s_log_space_first_id - space_id);
	}

	/** An undo::Tablespace object is used to easily convert between
	undo_space_id and undo_space_num and to create the automatic file_name
	and space name.  In addition, it is used in undo::Tablespaces to track
	the trx_rseg_t objects in an Rsegs vector. So we do not allocate the
	Rsegs vector for each object, only when requested by the constructor. */
	struct Tablespace
	{
		/** Constructor
		@param[in]	id		tablespace id
		@param[in]	use_rsegs	Tue if rsegs will be tracked */
		explicit Tablespace(space_id_t id, bool use_rsegs = false)
			:
			m_id(id),
			m_num(undo::id2num(id)),
			m_space_name(),
			m_file_name(),
			m_log_file_name(),
			m_rsegs()
		{
			/* This object is used to track rollback segments
			only in the global undo::Tablespaces object. */
			if (use_rsegs) {
				ut_ad(id == 0 || is_reserved(id));

				m_rsegs = UT_NEW_NOKEY(Rsegs());
			}
		}

		/** Destructor */
		~Tablespace()
		{
			if (m_space_name != nullptr) {
				ut_free(m_space_name);
				m_space_name = nullptr;
			}

			if (m_file_name != nullptr) {
				ut_free(m_file_name);
				m_file_name = nullptr;
			}

			if (m_log_file_name != nullptr) {
				ut_free(m_log_file_name);
				m_log_file_name = nullptr;
			}

			/* Clear the cached rollback segments.  */
			if (m_rsegs != nullptr) {
				UT_DELETE(m_rsegs);
				m_rsegs = nullptr;
			}
		};

		/** Build a standard undo tablespace name from a space_id.
		@param[in]	space_id	id of the undo tablespace.
		@return tablespace name of the undo tablespace file */
		char* make_space_name(space_id_t space_id);

		/** Get the undo tablespace name. Make it if not yet made.
		NOTE: This is only called from stack objects so there is no
		race condition. If it is ever called from a shared object
		like undo::spaces, then it must be protected by the caller.
		@return tablespace name created from the space_id */
		char* space_name()
		{
			if (m_space_name == nullptr) {
#ifndef UNIV_HOTBACKUP
				m_space_name = make_space_name(m_id);
#endif /* !UNIV_HOTBACKUP */
			}

			return(m_space_name);
		}

		/** Build a standard undo tablespace file name from a space_id.
		@param[in]	space_id	id of the undo tablespace.
		@return file_name of the undo tablespace file */
		char* make_file_name(space_id_t space_id);

		/** Get the undo space filename. Make it if not yet made.
		NOTE: This is only called from stack objects so there is no
		race condition. If it is ever called from a shared object
		like undo::spaces, then it must be protected by the caller.
		@return tablespace filename created from the space_id */
		char* file_name()
		{
			if (m_file_name == nullptr) {
				m_file_name = make_file_name(m_id);
			}

			return(m_file_name);
		}

		/** Build a log file name based on space_id
		@param[in]	space_id	id of the undo tablespace.
		@return DB_SUCCESS or error code */
		char* make_log_file_name(space_id_t space_id);

		/** Get the undo log filename. Make it if not yet made.
		NOTE: This is only called from stack objects so there is no
		race condition. If it is ever called from a shared object
		like undo::spaces, then it must be protected by the caller.
		@return tablespace filename created from the space_id */
		char* log_file_name()
		{
			if (m_log_file_name == nullptr) {
				m_log_file_name = make_log_file_name(m_id);
			}

			return(m_log_file_name);
		}

		/** Get the undo tablespace ID.
		@return tablespace ID */
		space_id_t id()
		{
			return(m_id);
		}

		/** Get the undo tablespace number.  This is the same as m_id
		if m_id is 0 or this is a v5.6-5.7 undo tablespace. v8+ undo
		tablespaces use a space_id from the reserved range.
		@return undo tablespace number */
		space_id_t num()
		{
			ut_ad(m_num < FSP_MAX_ROLLBACK_SEGMENTS);

			return(m_num);
		}

		/** Get a reference to the List of rollback segments within
		this undo tablespace.
		@return a reference to the Rsegs vector. */
		Rsegs* rsegs()
		{
			return(m_rsegs);
		}

	private:
		/** Undo Tablespace ID. */
		space_id_t	m_id;

		/** Undo Tablespace number, from 1 to 127. This is the
		7-bit number that is used in a rollback pointer.
		Use id2num() to get this number from a space_id. */
		space_id_t	m_num;

		/** The tablespace name, auto-generated when needed from
		the space number. */
		char*		m_space_name;

		/** The tablespace file name, auto-generated when needed
		from the space number. */
		char*		m_file_name;

		/** The tablespace log file name, auto-generated when needed
		from the space number. */
		char*		m_log_file_name;

		/** List of rollback segments within this tablespace.
		This is not always used. Must call init_rsegs to use it. */
		Rsegs*		m_rsegs;
	};

	/** List of undo tablespaces, each containing a list of
	rollback segments. */
	class Tablespaces
	{
		using Tablespaces_Vector
			= std::vector<Tablespace*, ut_allocator<Tablespace*>>;

	public:
		Tablespaces()
		{
			init();
		}

		~Tablespaces()
		{
			deinit();
		}

		/** Initialize */
		void init();

		/** De-initialize */
		void deinit();

		/** Clear the contents of the list of Tablespace objects.
		This does not deallocate any memory. */
		void clear() {
			for (auto undo_space : m_spaces) {
				UT_DELETE(undo_space);
			}
			m_spaces.clear();
		}

		/** Get the number of tablespaces tracked by this object. */
		ulint size() {
			return(m_spaces.size());
		}

		/** See if the list of tablespaces is empty. */
		bool empty() {
			return(m_spaces.empty());
		}

		/** Get the Tablespace tracked at a position. */
		Tablespace* at(size_t pos) {
			return(m_spaces.at(pos));
		}

		/** Add a new space_id to the back of the vector.
		The vector has been pre-allocated to 128 so read threads will
		not loose what is pointed to.
		@param[in]	id	tablespace ID */
		void add(space_id_t id);

		/** Check if the given space_is is in the vector.
		@return true if space_id is found, else false */
		bool contains(space_id_t id) {
			return(find(id) != nullptr);
		}

		/** Find the given space_id in the vector.
		@return pointer to an undo::Tablespace struct */
		Tablespace* find(space_id_t id) {
			if (m_spaces.empty()) {
				return(nullptr);
			}

			/* The sort method above puts this vector in order by
			Tablespace::num. If there are no gaps, then we should
			be able to find it quickly. */
			space_id_t	slot = id2num(id) - 1;
			if (slot < m_spaces.size()) {
				auto undo_space = m_spaces.at(slot);
				if (undo_space->id() == id) {
					return(undo_space);
				}
			}

			/* If there are gaps in the numbering, do a search. */
			for (auto undo_space : m_spaces) {
				if (undo_space->id() == id) {
					return(undo_space);
				}
			}

			return(nullptr);
		}

#ifdef UNIV_DEBUG
		/** Determine if this thread owns a lock on m_latch. */
		bool own_latch()
		{
			return(rw_lock_own(m_latch, RW_LOCK_X)
			       || rw_lock_own(m_latch, RW_LOCK_S));
		}
#endif /* UNIV_DEBUG */

		/** Get a shared lock on m_spaces. */
		void s_lock()
		{
			rw_lock_s_lock(m_latch);
		}

		/** Release a shared lock on m_spaces. */
		void s_unlock()
		{
			rw_lock_s_unlock(m_latch);
		}

		/** Get an exclusive lock on m_spaces. */
		void x_lock()
		{
			rw_lock_x_lock(m_latch);
		}

		/** Release an exclusive lock on m_spaces. */
		void x_unlock()
		{
			rw_lock_x_unlock(m_latch);
		}

		Tablespaces_Vector	m_spaces;

	private:
		/** RW lock to protect m_spaces.
		x for adding elements, s for scanning, size() etc. */
		rw_lock_t*	m_latch;
	};

	/** A global object that contains a vector of undo::Tablespace structs. */
	extern Tablespaces*	spaces;

	/** Create the truncate log file.
	@param[in]	space_id	id of the undo tablespace to truncate.
	@return DB_SUCCESS or error code. */
	dberr_t start_logging(space_id_t space_id);

	/** Mark completion of undo truncate action by writing magic number
	to the log file and then removing it from the disk.
	If we are going to remove it from disk then why write magic number?
	This is to safeguard from unlink (file-system) anomalies that will
	keep the link to the file even after unlink action is successfull
	and ref-count = 0.
	@param[in]	space_id	ID of the undo tablespace to truncate.*/
	void done_logging(space_id_t space_id);

	/** Check if TRUNCATE_DDL_LOG file exist.
	@param[in]	space_id	ID of the undo tablespace.
	@return true if exist else false. */
	bool is_active_truncate_log_present(space_id_t space_id);

	/** list of undo tablespaces that need header pages and rollback
	segments written to them at startup.  This can be because they are
	newly initialized, were being truncated and the system crashed, or
	they were an old format at startup and were replaced when they were
	opened. Old format undo tablespaces do not have space_ids between
	dict_sys_t::s_min_undo_space_id and dict_sys_t::s_max_undo_space_id
	and they do not contain an RSEG_ARRAY page. */
	extern Space_Ids	s_under_construction;

	/** Add undo tablespace to s_under_construction vector.
	@param[in]	space_id	space id of tablespace to
	truncate */
	void add_space_to_construction_list(space_id_t space_id);

	/** Clear the s_under_construction vector. */
	void clear_construction_list();

	/** Is an undo tablespace under constuction at the moment.
	@param[in]	space_id	space id to check
	@return true if marked for truncate, else false. */
	bool is_under_construction(space_id_t space_id);

	/* Return whether the undo tablespace is active.
	@return true if active */
	bool is_active(space_id_t space_id);

	/* Return whether the undo tablespace is inactive.
	@return true if inactive */
	inline bool is_inactive(space_id_t space_id) {
		return(!is_active(space_id));
	}

	/** Track an UNDO tablespace marked for truncate. */
	class Truncate {
	public:

		Truncate()
			:
			m_space_id_marked(SPACE_UNKNOWN),
			m_tablespace_marked(),
			m_purge_rseg_truncate_frequency(
				static_cast<ulint>(
				srv_purge_rseg_truncate_frequency))
		{
			/* Do Nothing. */
		}

		/** Is tablespace selected for truncate.
		@return true if undo tablespace is marked for truncate */
		bool is_marked() const
		{
			return(m_space_id_marked != SPACE_UNKNOWN);
		}

		/** Mark the tablespace for truncate.
		@param[in]	undo_space_id	tablespace for truncate. */
		void mark(space_id_t undo_space_id)
		{
			undo::Tablespace* space_to_mark
				= undo::spaces->find(undo_space_id);
			ut_ad(space_to_mark != nullptr);

			/* Set this undo space inactive so that its rsegs
			will not be allocated to any new transaction. */
			space_to_mark->rsegs()->x_lock();
			space_to_mark->rsegs()->set_inactive();
			space_to_mark->rsegs()->x_unlock();

			/* We found an UNDO-tablespace to truncate so set the
			local purge rseg truncate frequency to 1. This will help
			accelerate the purge action and in turn truncate. */
			m_purge_rseg_truncate_frequency = 1;

			m_space_id_marked = undo_space_id;
			m_tablespace_marked = space_to_mark;
		}

		/** Get the tablespace marked for truncate.
		@return tablespace ID marked for truncate. */
		space_id_t get_marked_space_id() const
		{
			return(m_space_id_marked);
		}

		/** Get a pointer to the list of rollback segments within
		this undo tablespace that is marked
		for truncate.
		@return pointer to the Rsegs_Vector. */
		Rsegs* rsegs()
		{
			ut_ad(m_tablespace_marked != nullptr);
			return(m_tablespace_marked->rsegs());
		}

		/** Reset for next rseg truncate. */
		void reset()
		{
			m_space_id_marked = SPACE_UNKNOWN;
			m_tablespace_marked = nullptr;

			/* Sync with global value as we are done with
			truncate now. */
			m_purge_rseg_truncate_frequency = static_cast<ulint>(
				srv_purge_rseg_truncate_frequency);
		}

		/** Get the tablespace ID to start a scan.
		@return	UNDO space_id to start scanning. */
		space_id_t get_scan_space_id() const
		{
			Tablespace* undo_space = undo::spaces->at(s_scan_pos);

			return(undo_space->id());
		}

		/** Increment the scanning position in a round-robin fashion.
		@return	UNDO space_id at incremented scanning position. */
		space_id_t increment_scan() const
		{
			/** Round-robin way of selecting an undo tablespace
			for the truncate operation. Once we reach the end of
			the list of known undo tablespace IDs, move back to
			the first undo tablespace ID. This will scan active
			(undo_space_num <= srv_undo_tablespaces) as well as
			inactive (undo_space_num > srv_undo_tablespaces)
			undo tablespaces. */
			s_scan_pos = (s_scan_pos + 1) % undo::spaces->size();

			return(get_scan_space_id());
		}

		/** Get local rseg purge truncate frequency
		@return rseg purge truncate frequency. */
		ulint get_rseg_truncate_frequency() const
		{
			return(m_purge_rseg_truncate_frequency);
		}

	private:
		/** UNDO space ID that is marked for truncate. */
		space_id_t		m_space_id_marked;

		/** UNDO tablespace that is marked for truncate. */
		undo::Tablespace*	m_tablespace_marked;

		/** Rollback segment(s) purge frequency. This is local
		value maintained along with global value. It is set to global
		value on start but when tablespace is marked for truncate it
		is updated to 1 and then minimum value among 2 is used by
		purge action. */
		ulint			m_purge_rseg_truncate_frequency;

		/** Start scanning for UNDO tablespace from this
		vector position. This is to avoid bias selection
		of one tablespace always. */
		static size_t		s_scan_pos;

	};	/* class Truncate */

}	/* namespace undo */

/** The control structure used in the purge operation */
struct trx_purge_t{
	sess_t*		sess;		/*!< System session running the purge
					query */
	trx_t*		trx;		/*!< System transaction running the
					purge query: this trx is not in the
					trx list of the trx system and it
					never ends */
#ifndef UNIV_HOTBACKUP
	rw_lock_t	latch;		/*!< The latch protecting the purge
					view. A purge operation must acquire an
					x-latch here for the instant at which
					it changes the purge view: an undo
					log operation can prevent this by
					obtaining an s-latch here. It also
					protects state and running */
#endif  /* !UNIV_HOTBACKUP */
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
	page_no_t	page_no;	/*!< Page number for the next undo
					record to purge, page number of the
					log header, if dummy record */
	ulint		offset;		/*!< Page offset for the next undo
					record to purge, 0 if the dummy
					record */
	page_no_t	hdr_page_no;	/*!< Header page of the undo log where
					the next record to purge belongs */
	ulint		hdr_offset;	/*!< Header byte offset on the page */


	TrxUndoRsegsIterator*
			rseg_iter;	/*!< Iterator to get the next rseg
					to process */

	purge_pq_t*	purge_queue;	/*!< Binary min-heap, ordered on
					TrxUndoRsegs::trx_no. It is protected
					by the pq_mutex */
	PQMutex		pq_mutex;	/*!< Mutex protecting purge_queue */

	undo::Truncate	undo_trunc;	/*!< Track UNDO tablespace marked
					for truncate. */

	mem_heap_t*	heap;		/*!< Heap for reading the undo log
					records */
};

/** Choose the rollback segment with the smallest trx_no. */
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
	Rseg_Iterator			m_iter;

	/** Sentinel value */
	static const TrxUndoRsegs	NullElement;
};

#include "trx0purge.ic"

#endif /* trx0purge_h */
