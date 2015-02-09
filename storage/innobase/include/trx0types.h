/*****************************************************************************

Copyright (c) 1996, 2014, Oracle and/or its affiliates. All Rights Reserved.

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
@file include/trx0types.h
Transaction system global type definitions

Created 3/26/1996 Heikki Tuuri
*******************************************************/

#ifndef trx0types_h
#define trx0types_h

#include "ut0byte.h"
#include "ut0mutex.h"
#include "ut0new.h"

#include <set>
#include <queue>
#include <vector>

//#include <unordered_set>

/** printf(3) format used for printing DB_TRX_ID and other system fields */
#define TRX_ID_FMT		IB_ID_FMT

/** maximum length that a formatted trx_t::id could take, not including
the terminating NUL character. */
static const ulint TRX_ID_MAX_LEN = 17;

/** Space id of the transaction system page (the system tablespace) */
static const ulint TRX_SYS_SPACE = 0;

/** Page number of the transaction system page */
#define TRX_SYS_PAGE_NO		FSP_TRX_SYS_PAGE_NO

/** Random value to check for corruption of trx_t */
static const ulint TRX_MAGIC_N = 91118598;

/** If this flag is set then the transaction cannot be rolled back
asynchronously. */
static const ib_uint32_t TRX_FORCE_ROLLBACK_DISABLE = 1 << 29;

/** Was the transaction rolled back asynchronously or by the
owning thread. This flag is relevant only if TRX_FORCE_ROLLBACK
is set.  */
static const ib_uint32_t TRX_FORCE_ROLLBACK_ASYNC = 1 << 30;

/** Mark the transaction for forced rollback */
static const ib_uint32_t TRX_FORCE_ROLLBACK = 1 << 31;

/** For masking out the above four flags */
static const ib_uint32_t TRX_FORCE_ROLLBACK_MASK = 0x1FFFFFFF;

/** Transaction execution states when trx->state == TRX_STATE_ACTIVE */
enum trx_que_t {
	TRX_QUE_RUNNING,		/*!< transaction is running */
	TRX_QUE_LOCK_WAIT,		/*!< transaction is waiting for
					a lock */
	TRX_QUE_ROLLING_BACK,		/*!< transaction is rolling back */
	TRX_QUE_COMMITTING		/*!< transaction is committing */
};

/** Transaction states (trx_t::state) */
enum trx_state_t {

	TRX_STATE_NOT_STARTED,

	/** Same as not started but with additional semantics that it
	was rolled back asynchronously the last time it was active. */
	TRX_STATE_FORCED_ROLLBACK,

	TRX_STATE_ACTIVE,

	/** Support for 2PC/XA */
	TRX_STATE_PREPARED,

	TRX_STATE_COMMITTED_IN_MEMORY
};

/** Type of data dictionary operation */
enum trx_dict_op_t {
	/** The transaction is not modifying the data dictionary. */
	TRX_DICT_OP_NONE = 0,
	/** The transaction is creating a table or an index, or
	dropping a table.  The table must be dropped in crash
	recovery.  This and TRX_DICT_OP_NONE are the only possible
	operation modes in crash recovery. */
	TRX_DICT_OP_TABLE = 1,
	/** The transaction is creating or dropping an index in an
	existing table.  In crash recovery, the data dictionary
	must be locked, but the table must not be dropped. */
	TRX_DICT_OP_INDEX = 2
};

/** Memory objects */
/* @{ */
/** Transaction */
struct trx_t;
/** The locks and state of an active transaction */
struct trx_lock_t;
/** Transaction system */
struct trx_sys_t;
/** Signal */
struct trx_sig_t;
/** Rollback segment */
struct trx_rseg_t;
/** Transaction undo log */
struct trx_undo_t;
/** The control structure used in the purge operation */
struct trx_purge_t;
/** Rollback command node in a query graph */
struct roll_node_t;
/** Commit command node in a query graph */
struct commit_node_t;
/** SAVEPOINT command node in a query graph */
struct trx_named_savept_t;
/* @} */

/** Row identifier (DB_ROW_ID, DATA_ROW_ID) */
typedef ib_id_t	row_id_t;
/** Transaction identifier (DB_TRX_ID, DATA_TRX_ID) */
typedef ib_id_t	trx_id_t;
/** Rollback pointer (DB_ROLL_PTR, DATA_ROLL_PTR) */
typedef ib_id_t	roll_ptr_t;
/** Undo number */
typedef ib_id_t	undo_no_t;

/** Maximum transaction identifier */
#define TRX_ID_MAX	IB_ID_MAX

/** Transaction savepoint */
struct trx_savept_t{
	undo_no_t	least_undo_no;	/*!< least undo number to undo */
};

/** File objects */
/* @{ */
/** Transaction system header */
typedef byte	trx_sysf_t;
/** Rollback segment header */
typedef byte	trx_rsegf_t;
/** Undo segment header */
typedef byte	trx_usegf_t;
/** Undo log header */
typedef byte	trx_ulogf_t;
/** Undo log page header */
typedef byte	trx_upagef_t;

/** Undo log record */
typedef	byte	trx_undo_rec_t;

/* @} */

typedef ib_mutex_t RsegMutex;
typedef ib_mutex_t TrxMutex;
typedef ib_mutex_t UndoMutex;
typedef ib_mutex_t PQMutex;
typedef ib_mutex_t TrxSysMutex;

/** Rollback segements from a given transaction with trx-no
scheduled for purge. */
class TrxUndoRsegs {
private:
	typedef std::vector<trx_rseg_t*, ut_allocator<trx_rseg_t*> >
		trx_rsegs_t;
public:
	typedef trx_rsegs_t::iterator iterator;

	/** Default constructor */
	TrxUndoRsegs() : m_trx_no() { }

	explicit TrxUndoRsegs(trx_id_t trx_no)
		:
		m_trx_no(trx_no)
	{
		// Do nothing
	}

	/** Get transaction number
	@return trx_id_t - get transaction number. */
	trx_id_t get_trx_no() const
	{
		return(m_trx_no);
	}

	/** Add rollback segment.
	@param rseg rollback segment to add. */
	void push_back(trx_rseg_t* rseg)
	{
		m_rsegs.push_back(rseg);
	}

	/** Erase the element pointed by given iterator.
	@param[in]	iterator	iterator */
	void erase(iterator& it)
	{
		m_rsegs.erase(it);
	}

	/** Number of registered rsegs.
	@return size of rseg list. */
	ulint size() const
	{
		return(m_rsegs.size());
	}

	/**
	@return an iterator to the first element */
	iterator begin()
	{
		return(m_rsegs.begin());
	}

	/**
	@return an iterator to the end */
	iterator end()
	{
		return(m_rsegs.end());
	}

	/** Append rollback segments from referred instance to current
	instance. */
	void append(const TrxUndoRsegs& append_from)
	{
		ut_ad(get_trx_no() == append_from.get_trx_no());

		m_rsegs.insert(m_rsegs.end(),
			       append_from.m_rsegs.begin(),
			       append_from.m_rsegs.end());
	}

	/** Compare two TrxUndoRsegs based on trx_no.
	@param elem1 first element to compare
	@param elem2 second element to compare
	@return true if elem1 > elem2 else false.*/
	bool operator()(const TrxUndoRsegs& lhs, const TrxUndoRsegs& rhs)
	{
		return(lhs.m_trx_no > rhs.m_trx_no);
	}

	/** Compiler defined copy-constructor/assignment operator
	should be fine given that there is no reference to a memory
	object outside scope of class object.*/

private:
	/** The rollback segments transaction number. */
	trx_id_t		m_trx_no;

	/** Rollback segments of a transaction, scheduled for purge. */
	trx_rsegs_t		m_rsegs;
};

typedef std::priority_queue<
	TrxUndoRsegs,
	std::vector<TrxUndoRsegs, ut_allocator<TrxUndoRsegs> >,
	TrxUndoRsegs>	purge_pq_t;

typedef std::vector<trx_id_t, ut_allocator<trx_id_t> >	trx_ids_t;

/** Mapping read-write transactions from id to transaction instance, for
creating read views and during trx id lookup for MVCC and locking. */
struct TrxTrack {
	explicit TrxTrack(trx_id_t id, trx_t* trx = NULL)
		:
		m_id(id),
		m_trx(trx)
	{
		// Do nothing
	}

	trx_id_t	m_id;
	trx_t*		m_trx;
};

struct TrxTrackHash {
	size_t operator()(const TrxTrack& key) const
	{
		return(size_t(key.m_id));
	}
};

/**
Comparator for TrxMap */
struct TrxTrackHashCmp {

	bool operator() (const TrxTrack& lhs, const TrxTrack& rhs) const
	{
		return(lhs.m_id == rhs.m_id);
	}
};

/**
Comparator for TrxMap */
struct TrxTrackCmp {

	bool operator() (const TrxTrack& lhs, const TrxTrack& rhs) const
	{
		return(lhs.m_id < rhs.m_id);
	}
};

//typedef std::unordered_set<TrxTrack, TrxTrackHash, TrxTrackHashCmp> TrxIdSet;
typedef std::set<TrxTrack, TrxTrackCmp, ut_allocator<TrxTrack> >
	TrxIdSet;

#endif /* trx0types_h */
