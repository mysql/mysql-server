/*****************************************************************************

Copyright (c) 1996, 2012, Oracle and/or its affiliates. All Rights Reserved.

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
@file include/trx0trx.h
The transaction

Created 3/26/1996 Heikki Tuuri
*******************************************************/

#ifndef trx0trx_h
#define trx0trx_h

#include "univ.i"
#include "trx0types.h"
#include "dict0types.h"
#ifndef UNIV_HOTBACKUP
#include "lock0types.h"
#include "log0log.h"
#include "usr0types.h"
#include "que0types.h"
#include "mem0mem.h"
#include "read0types.h"
#include "trx0xa.h"
#include "ut0vec.h"
#include "fts0fts.h"

#include <set>

/** Dummy session used currently in MySQL interface */
extern sess_t*	trx_dummy_sess;

/********************************************************************//**
Releases the search latch if trx has reserved it. */
UNIV_INLINE
void
trx_search_latch_release_if_reserved(
/*=================================*/
	trx_t*		trx); /*!< in: transaction */
/******************************************************************//**
Set detailed error message for the transaction. */
UNIV_INTERN
void
trx_set_detailed_error(
/*===================*/
	trx_t*		trx,	/*!< in: transaction struct */
	const char*	msg);	/*!< in: detailed error message */
/*************************************************************//**
Set detailed error message for the transaction from a file. Note that the
file is rewinded before reading from it. */
UNIV_INTERN
void
trx_set_detailed_error_from_file(
/*=============================*/
	trx_t*	trx,	/*!< in: transaction struct */
	FILE*	file);	/*!< in: file to read message from */
/****************************************************************//**
Retrieves the error_info field from a trx.
@return	the error info */
UNIV_INLINE
const dict_index_t*
trx_get_error_info(
/*===============*/
	const trx_t*	trx);	/*!< in: trx object */
/********************************************************************//**
Creates a transaction object for MySQL.
@return	own: transaction object */
UNIV_INTERN
trx_t*
trx_allocate_for_mysql(void);
/*========================*/
/********************************************************************//**
Creates a transaction object for background operations by the master thread.
@return	own: transaction object */
UNIV_INTERN
trx_t*
trx_allocate_for_background(void);
/*=============================*/
/********************************************************************//**
Frees a transaction object of a background operation of the master thread. */
UNIV_INTERN
void
trx_free_for_background(
/*====================*/
	trx_t*	trx);	/*!< in, own: trx object */
/********************************************************************//**
At shutdown, frees a transaction object that is in the PREPARED state. */
UNIV_INTERN
void
trx_free_prepared(
/*==============*/
	trx_t*	trx)	/*!< in, own: trx object */
	UNIV_COLD __attribute__((nonnull));
/********************************************************************//**
Frees a transaction object for MySQL. */
UNIV_INTERN
void
trx_free_for_mysql(
/*===============*/
	trx_t*	trx);	/*!< in, own: trx object */
/****************************************************************//**
Creates trx objects for transactions and initializes the trx list of
trx_sys at database start. Rollback segment and undo log lists must
already exist when this function is called, because the lists of
transactions to be rolled back or cleaned up are built based on the
undo log lists. */
UNIV_INTERN
void
trx_lists_init_at_db_start(void);
/*============================*/

#ifdef UNIV_DEBUG
#define trx_start_if_not_started_xa(t)				\
	{							\
	(t)->start_line = __LINE__;				\
	(t)->start_file = __FILE__;				\
	trx_start_if_not_started_xa_low((t));			\
	}
#else
#define trx_start_if_not_started_xa(t)				\
	trx_start_if_not_started_xa_low((t))
#endif /* UNIV_DEBUG */

/*************************************************************//**
Starts the transaction if it is not yet started. */
UNIV_INTERN
void
trx_start_if_not_started_xa_low(
/*============================*/
	trx_t*	trx);	/*!< in: transaction */
/*************************************************************//**
Starts the transaction if it is not yet started. */
UNIV_INTERN
void
trx_start_if_not_started_low(
/*=========================*/
	trx_t*	trx);	/*!< in: transaction */

#ifdef UNIV_DEBUG
#define trx_start_if_not_started(t)				\
	{							\
	(t)->start_line = __LINE__;				\
	(t)->start_file = __FILE__;				\
	trx_start_if_not_started_low((t));			\
	}
#else
#define trx_start_if_not_started(t)				\
	trx_start_if_not_started_low((t))
#endif /* UNIV_DEBUG */

/*************************************************************//**
Starts the transaction for a DDL operation. */
UNIV_INTERN
void
trx_start_for_ddl_low(
/*==================*/
	trx_t*		trx,	/*!< in/out: transaction */
	trx_dict_op_t	op)	/*!< in: dictionary operation type */
	__attribute__((nonnull));

#ifdef UNIV_DEBUG
#define trx_start_for_ddl(t, o)					\
	{							\
	ut_ad((t)->start_file == 0);				\
	(t)->start_line = __LINE__;				\
	(t)->start_file = __FILE__;				\
	trx_start_for_ddl_low((t), (o));			\
	}
#else
#define trx_start_for_ddl(t, o)					\
	trx_start_for_ddl_low((t), (o))
#endif /* UNIV_DEBUG */

/****************************************************************//**
Commits a transaction. */
UNIV_INTERN
void
trx_commit(
/*=======*/
	trx_t*	trx);	/*!< in: transaction */
/****************************************************************//**
Cleans up a transaction at database startup. The cleanup is needed if
the transaction already got to the middle of a commit when the database
crashed, and we cannot roll it back. */
UNIV_INTERN
void
trx_cleanup_at_db_startup(
/*======================*/
	trx_t*	trx);	/*!< in: transaction */
/**********************************************************************//**
Does the transaction commit for MySQL.
@return	DB_SUCCESS or error number */
UNIV_INTERN
dberr_t
trx_commit_for_mysql(
/*=================*/
	trx_t*	trx);	/*!< in/out: transaction */
/**********************************************************************//**
Does the transaction prepare for MySQL. */
UNIV_INTERN
void
trx_prepare_for_mysql(
/*==================*/
	trx_t*	trx);	/*!< in/out: trx handle */
/**********************************************************************//**
This function is used to find number of prepared transactions and
their transaction objects for a recovery.
@return	number of prepared transactions */
UNIV_INTERN
int
trx_recover_for_mysql(
/*==================*/
	XID*	xid_list,	/*!< in/out: prepared transactions */
	ulint	len);		/*!< in: number of slots in xid_list */
/*******************************************************************//**
This function is used to find one X/Open XA distributed transaction
which is in the prepared state
@return	trx or NULL; on match, the trx->xid will be invalidated;
note that the trx may have been committed, unless the caller is
holding lock_sys->mutex */
UNIV_INTERN
trx_t *
trx_get_trx_by_xid(
/*===============*/
	const XID*	xid);	/*!< in: X/Open XA transaction identifier */
/**********************************************************************//**
If required, flushes the log to disk if we called trx_commit_for_mysql()
with trx->flush_log_later == TRUE. */
UNIV_INTERN
void
trx_commit_complete_for_mysql(
/*==========================*/
	trx_t*	trx)	/*!< in/out: transaction */
	__attribute__((nonnull));
/**********************************************************************//**
Marks the latest SQL statement ended. */
UNIV_INTERN
void
trx_mark_sql_stat_end(
/*==================*/
	trx_t*	trx);	/*!< in: trx handle */
/********************************************************************//**
Assigns a read view for a consistent read query. All the consistent reads
within the same transaction will get the same read view, which is created
when this function is first called for a new started transaction.
@return	consistent read view */
UNIV_INTERN
read_view_t*
trx_assign_read_view(
/*=================*/
	trx_t*	trx);	/*!< in: active transaction */
/****************************************************************//**
Prepares a transaction for commit/rollback. */
UNIV_INTERN
void
trx_commit_or_rollback_prepare(
/*===========================*/
	trx_t*	trx);	/*!< in/out: transaction */
/*********************************************************************//**
Creates a commit command node struct.
@return	own: commit node struct */
UNIV_INTERN
commit_node_t*
trx_commit_node_create(
/*===================*/
	mem_heap_t*	heap);	/*!< in: mem heap where created */
/***********************************************************//**
Performs an execution step for a commit type node in a query graph.
@return	query thread to run next, or NULL */
UNIV_INTERN
que_thr_t*
trx_commit_step(
/*============*/
	que_thr_t*	thr);	/*!< in: query thread */

/**********************************************************************//**
Prints info about a transaction.
Caller must hold trx_sys->mutex. */
UNIV_INTERN
void
trx_print_low(
/*==========*/
	FILE*		f,
			/*!< in: output stream */
	const trx_t*	trx,
			/*!< in: transaction */
	ulint		max_query_len,
			/*!< in: max query length to print,
			or 0 to use the default max length */
	ulint		n_rec_locks,
			/*!< in: lock_number_of_rows_locked(&trx->lock) */
	ulint		n_trx_locks,
			/*!< in: length of trx->lock.trx_locks */
	ulint		heap_size)
			/*!< in: mem_heap_get_size(trx->lock.lock_heap) */
	__attribute__((nonnull));

/**********************************************************************//**
Prints info about a transaction.
The caller must hold lock_sys->mutex and trx_sys->mutex.
When possible, use trx_print() instead. */
UNIV_INTERN
void
trx_print_latched(
/*==============*/
	FILE*		f,		/*!< in: output stream */
	const trx_t*	trx,		/*!< in: transaction */
	ulint		max_query_len)	/*!< in: max query length to print,
					or 0 to use the default max length */
	__attribute__((nonnull));

/**********************************************************************//**
Prints info about a transaction.
Acquires and releases lock_sys->mutex and trx_sys->mutex. */
UNIV_INTERN
void
trx_print(
/*======*/
	FILE*		f,		/*!< in: output stream */
	const trx_t*	trx,		/*!< in: transaction */
	ulint		max_query_len)	/*!< in: max query length to print,
					or 0 to use the default max length */
	__attribute__((nonnull));

/**********************************************************************//**
Determine if a transaction is a dictionary operation.
@return	dictionary operation mode */
UNIV_INLINE
enum trx_dict_op_t
trx_get_dict_operation(
/*===================*/
	const trx_t*	trx)	/*!< in: transaction */
	__attribute__((pure));
/**********************************************************************//**
Flag a transaction a dictionary operation. */
UNIV_INLINE
void
trx_set_dict_operation(
/*===================*/
	trx_t*			trx,	/*!< in/out: transaction */
	enum trx_dict_op_t	op);	/*!< in: operation, not
					TRX_DICT_OP_NONE */

#ifndef UNIV_HOTBACKUP
/**********************************************************************//**
Determines if a transaction is in the given state.
The caller must hold trx_sys->mutex, or it must be the thread
that is serving a running transaction.
A running transaction must be in trx_sys->ro_trx_list or trx_sys->rw_trx_list
unless it is a non-locking autocommit read only transaction, which is only
in trx_sys->mysql_trx_list.
@return	TRUE if trx->state == state */
UNIV_INLINE
ibool
trx_state_eq(
/*=========*/
	const trx_t*	trx,	/*!< in: transaction */
	trx_state_t	state)	/*!< in: state;
				if state != TRX_STATE_NOT_STARTED
				asserts that
				trx->state != TRX_STATE_NOT_STARTED */
	__attribute__((nonnull, warn_unused_result));
# ifdef UNIV_DEBUG
/**********************************************************************//**
Asserts that a transaction has been started.
The caller must hold trx_sys->mutex.
@return TRUE if started */
UNIV_INTERN
ibool
trx_assert_started(
/*===============*/
	const trx_t*	trx)	/*!< in: transaction */
	__attribute__((nonnull, warn_unused_result));
# endif /* UNIV_DEBUG */

/**********************************************************************//**
Determines if the currently running transaction has been interrupted.
@return	TRUE if interrupted */
UNIV_INTERN
ibool
trx_is_interrupted(
/*===============*/
	const trx_t*	trx);	/*!< in: transaction */
/**********************************************************************//**
Determines if the currently running transaction is in strict mode.
@return	TRUE if strict */
UNIV_INTERN
ibool
trx_is_strict(
/*==========*/
	trx_t*	trx);	/*!< in: transaction */
#else /* !UNIV_HOTBACKUP */
#define trx_is_interrupted(trx) FALSE
#endif /* !UNIV_HOTBACKUP */

/*******************************************************************//**
Calculates the "weight" of a transaction. The weight of one transaction
is estimated as the number of altered rows + the number of locked rows.
@param t	transaction
@return		transaction weight */
#define TRX_WEIGHT(t)	((t)->undo_no + UT_LIST_GET_LEN((t)->lock.trx_locks))

/*******************************************************************//**
Compares the "weight" (or size) of two transactions. Transactions that
have edited non-transactional tables are considered heavier than ones
that have not.
@return	TRUE if weight(a) >= weight(b) */
UNIV_INTERN
ibool
trx_weight_ge(
/*==========*/
	const trx_t*	a,	/*!< in: the first transaction to be compared */
	const trx_t*	b);	/*!< in: the second transaction to be compared */

/* Maximum length of a string that can be returned by
trx_get_que_state_str(). */
#define TRX_QUE_STATE_STR_MAX_LEN	12 /* "ROLLING BACK" */

/*******************************************************************//**
Retrieves transaction's que state in a human readable string. The string
should not be free()'d or modified.
@return	string in the data segment */
UNIV_INLINE
const char*
trx_get_que_state_str(
/*==================*/
	const trx_t*	trx);	/*!< in: transaction */

/****************************************************************//**
Assign a read-only transaction a rollback-segment, if it is attempting
to write to a TEMPORARY table. */
UNIV_INTERN
void
trx_assign_rseg(
/*============*/
	trx_t*		trx);		/*!< A read-only transaction that
					needs to be assigned a RBS. */
/*******************************************************************//**
Transactions that aren't started by the MySQL server don't set
the trx_t::mysql_thd field. For such transactions we set the lock
wait timeout to 0 instead of the user configured value that comes
from innodb_lock_wait_timeout via trx_t::mysql_thd.
@param trx	transaction
@return		lock wait timeout in seconds */
#define trx_lock_wait_timeout_get(trx)					\
	((trx)->mysql_thd != NULL					\
	 ? thd_lock_wait_timeout((trx)->mysql_thd)			\
	 : 0)

/*******************************************************************//**
Determine if the transaction is a non-locking autocommit select
(implied read-only).
@param t	transaction
@return true	if non-locking autocommit select transaction. */
#define trx_is_autocommit_non_locking(t)				\
((t)->auto_commit && (t)->will_lock == 0)

/*******************************************************************//**
Determine if the transaction is a non-locking autocommit select
with an explicit check for the read-only status.
@param t	transaction
@return true	if non-locking autocommit read-only transaction. */
#define trx_is_ac_nl_ro(t)						\
((t)->read_only && trx_is_autocommit_non_locking((t)))

/*******************************************************************//**
Assert that the transaction is in the trx_sys_t::rw_trx_list */
#define assert_trx_in_rw_list(t) do {					\
	ut_ad(!(t)->read_only);						\
	assert_trx_in_list(t);						\
} while (0)

/*******************************************************************//**
Assert that the transaction is either in trx_sys->ro_trx_list or
trx_sys->rw_trx_list but not both and it cannot be an autocommit
non-locking select */
#define assert_trx_in_list(t) do {					\
	ut_ad((t)->in_ro_trx_list == (t)->read_only);			\
	ut_ad((t)->in_rw_trx_list == !(t)->read_only);			\
	ut_ad(!trx_is_autocommit_non_locking((t)));			\
	switch ((t)->state) {						\
	case TRX_STATE_PREPARED:					\
		/* fall through */					\
	case TRX_STATE_ACTIVE:						\
	case TRX_STATE_COMMITTED_IN_MEMORY:				\
		continue;						\
	case TRX_STATE_NOT_STARTED:					\
		break;							\
	}								\
	ut_error;							\
} while (0)

#ifdef UNIV_DEBUG
/*******************************************************************//**
Assert that an autocommit non-locking select cannot be in the
ro_trx_list nor the rw_trx_list and that it is a read-only transaction.
The tranasction must be in the mysql_trx_list. */
# define assert_trx_nonlocking_or_in_list(t)				\
	do {								\
		if (trx_is_autocommit_non_locking(t)) {			\
			trx_state_t	t_state = (t)->state;		\
			ut_ad((t)->read_only);				\
			ut_ad(!(t)->is_recovered);			\
			ut_ad(!(t)->in_ro_trx_list);			\
			ut_ad(!(t)->in_rw_trx_list);			\
			ut_ad((t)->in_mysql_trx_list);			\
			ut_ad(t_state == TRX_STATE_NOT_STARTED		\
			      || t_state == TRX_STATE_ACTIVE);		\
		} else {						\
			assert_trx_in_list(t);				\
		}							\
	} while (0)
#else /* UNIV_DEBUG */
/*******************************************************************//**
Assert that an autocommit non-locking slect cannot be in the
ro_trx_list nor the rw_trx_list and that it is a read-only transaction.
The tranasction must be in the mysql_trx_list. */
# define assert_trx_nonlocking_or_in_list(trx) ((void)0)
#endif /* UNIV_DEBUG */

/*******************************************************************//**
Latching protocol for trx_lock_t::que_state.  trx_lock_t::que_state
captures the state of the query thread during the execution of a query.
This is different from a transaction state. The query state of a transaction
can be updated asynchronously by other threads.  The other threads can be
system threads, like the timeout monitor thread or user threads executing
other queries. Another thing to be mindful of is that there is a delay between
when a query thread is put into LOCK_WAIT state and before it actually starts
waiting.  Between these two events it is possible that the query thread is
granted the lock it was waiting for, which implies that the state can be changed
asynchronously.

All these operations take place within the context of locking. Therefore state
changes within the locking code must acquire both the lock mutex and the
trx->mutex when changing trx->lock.que_state to TRX_QUE_LOCK_WAIT or
trx->lock.wait_lock to non-NULL but when the lock wait ends it is sufficient
to only acquire the trx->mutex.
To query the state either of the mutexes is sufficient within the locking
code and no mutex is required when the query thread is no longer waiting. */

/** The locks and state of an active transaction. Protected by
lock_sys->mutex, trx->mutex or both. */
struct trx_lock_t {
	ulint		n_active_thrs;	/*!< number of active query threads */

	trx_que_t	que_state;	/*!< valid when trx->state
					== TRX_STATE_ACTIVE: TRX_QUE_RUNNING,
					TRX_QUE_LOCK_WAIT, ... */

	lock_t*		wait_lock;	/*!< if trx execution state is
					TRX_QUE_LOCK_WAIT, this points to
					the lock request, otherwise this is
					NULL; set to non-NULL when holding
					both trx->mutex and lock_sys->mutex;
					set to NULL when holding
					lock_sys->mutex; readers should
					hold lock_sys->mutex, except when
					they are holding trx->mutex and
					wait_lock==NULL */
	ib_uint64_t	deadlock_mark;	/*!< A mark field that is initialized
					to and checked against lock_mark_counter
					by lock_deadlock_recursive(). */
	ibool		was_chosen_as_deadlock_victim;
					/*!< when the transaction decides to
					wait for a lock, it sets this to FALSE;
					if another transaction chooses this
					transaction as a victim in deadlock
					resolution, it sets this to TRUE.
					Protected by trx->mutex. */
	time_t		wait_started;	/*!< lock wait started at this time,
					protected only by lock_sys->mutex */

	que_thr_t*	wait_thr;	/*!< query thread belonging to this
					trx that is in QUE_THR_LOCK_WAIT
					state. For threads suspended in a
					lock wait, this is protected by
					lock_sys->mutex. Otherwise, this may
					only be modified by the thread that is
					serving the running transaction. */

	mem_heap_t*	lock_heap;	/*!< memory heap for trx_locks;
					protected by lock_sys->mutex */

	UT_LIST_BASE_NODE_T(lock_t)
			trx_locks;	/*!< locks requested
					by the transaction;
					insertions are protected by trx->mutex
					and lock_sys->mutex; removals are
					protected by lock_sys->mutex */

	ib_vector_t*	table_locks;	/*!< All table locks requested by this
					transaction, including AUTOINC locks */

	ibool		cancel;		/*!< TRUE if the transaction is being
					rolled back either via deadlock
					detection or due to lock timeout. The
					caller has to acquire the trx_t::mutex
					in order to cancel the locks. In
					lock_trx_table_locks_remove() we
					check for this cancel of a transaction's
					locks and avoid reacquiring the trx
					mutex to prevent recursive deadlocks.
					Protected by both the lock sys mutex
					and the trx_t::mutex. */
};

#define TRX_MAGIC_N	91118598

/** Type used to store the list of tables that are modified by a given
transaction. We store pointers to the table objects in memory because
we know that a table object will not be destroyed until a transaction
that modified it is running. */
typedef std::set<dict_table_t*>	trx_mod_tables_t;

/** The transaction handle

Normally, there is a 1:1 relationship between a transaction handle
(trx) and a session (client connection). One session is associated
with exactly one user transaction. There are some exceptions to this:

* For DDL operations, a subtransaction is allocated that modifies the
data dictionary tables. Lock waits and deadlocks are prevented by
acquiring the dict_operation_lock before starting the subtransaction
and releasing it after committing the subtransaction.

* The purge system uses a special transaction that is not associated
with any session.

* If the system crashed or it was quickly shut down while there were
transactions in the ACTIVE or PREPARED state, these transactions would
no longer be associated with a session when the server is restarted.

A session may be served by at most one thread at a time. The serving
thread of a session might change in some MySQL implementations.
Therefore we do not have os_thread_get_curr_id() assertions in the code.

Normally, only the thread that is currently associated with a running
transaction may access (read and modify) the trx object, and it may do
so without holding any mutex. The following are exceptions to this:

* trx_rollback_resurrected() may access resurrected (connectionless)
transactions while the system is already processing new user
transactions. The trx_sys->mutex prevents a race condition between it
and lock_trx_release_locks() [invoked by trx_commit()].

* trx_print_low() may access transactions not associated with the current
thread. The caller must be holding trx_sys->mutex and lock_sys->mutex.

* When a transaction handle is in the trx_sys->mysql_trx_list or
trx_sys->trx_list, some of its fields must not be modified without
holding trx_sys->mutex exclusively.

* The locking code (in particular, lock_deadlock_recursive() and
lock_rec_convert_impl_to_expl()) will access transactions associated
to other connections. The locks of transactions are protected by
lock_sys->mutex and sometimes by trx->mutex. */

struct trx_t{
	ulint		magic_n;

	ib_mutex_t		mutex;		/*!< Mutex protecting the fields
					state and lock
					(except some fields of lock, which
					are protected by lock_sys->mutex) */

	/** State of the trx from the point of view of concurrency control
	and the valid state transitions.

	Possible states:

	TRX_STATE_NOT_STARTED
	TRX_STATE_ACTIVE
	TRX_STATE_PREPARED
	TRX_STATE_COMMITTED_IN_MEMORY (alias below COMMITTED)

	Valid state transitions are:

	Regular transactions:
	* NOT_STARTED -> ACTIVE -> COMMITTED -> NOT_STARTED

	Auto-commit non-locking read-only:
	* NOT_STARTED -> ACTIVE -> NOT_STARTED

	XA (2PC):
	* NOT_STARTED -> ACTIVE -> PREPARED -> COMMITTED -> NOT_STARTED

	Recovered XA:
	* NOT_STARTED -> PREPARED -> COMMITTED -> (freed)

	XA (2PC) (shutdown before ROLLBACK or COMMIT):
	* NOT_STARTED -> PREPARED -> (freed)

	Latching and various transaction lists membership rules:

	XA (2PC) transactions are always treated as non-autocommit.

	Transitions to ACTIVE or NOT_STARTED occur when
	!in_rw_trx_list and !in_ro_trx_list (no trx_sys->mutex needed).

	Autocommit non-locking read-only transactions move between states
	without holding any mutex. They are !in_rw_trx_list, !in_ro_trx_list.

	When a transaction is NOT_STARTED, it can be in_mysql_trx_list if
	it is a user transaction. It cannot be in ro_trx_list or rw_trx_list.

	ACTIVE->PREPARED->COMMITTED is only possible when trx->in_rw_trx_list.
	The transition ACTIVE->PREPARED is protected by trx_sys->mutex.

	ACTIVE->COMMITTED is possible when the transaction is in
	ro_trx_list or rw_trx_list.

	Transitions to COMMITTED are protected by both lock_sys->mutex
	and trx->mutex.

	NOTE: Some of these state change constraints are an overkill,
	currently only required for a consistent view for printing stats.
	This unnecessarily adds a huge cost for the general case.

	NOTE: In the future we should add read only transactions to the
	ro_trx_list the first time they try to acquire a lock ie. by default
	we treat all read-only transactions as non-locking.  */
	trx_state_t	state;

	trx_lock_t	lock;		/*!< Information about the transaction
					locks and state. Protected by
					trx->mutex or lock_sys->mutex
					or both */
	ulint		is_recovered;	/*!< 0=normal transaction,
					1=recovered, must be rolled back,
					protected by trx_sys->mutex when
					trx->in_rw_trx_list holds */

	/* These fields are not protected by any mutex. */
	const char*	op_info;	/*!< English text describing the
					current operation, or an empty
					string */
	ulint		isolation_level;/*!< TRX_ISO_REPEATABLE_READ, ... */
	ulint		check_foreigns;	/*!< normally TRUE, but if the user
					wants to suppress foreign key checks,
					(in table imports, for example) we
					set this FALSE */
	/*------------------------------*/
	/* MySQL has a transaction coordinator to coordinate two phase
	commit between multiple storage engines and the binary log. When
	an engine participates in a transaction, it's responsible for
	registering itself using the trans_register_ha() API. */
	unsigned	is_registered:1;/* This flag is set to 1 after the
					transaction has been registered with
					the coordinator using the XA API, and
					is set to 0 after commit or rollback. */
	unsigned	owns_prepare_mutex:1;/* 1 if owns prepare mutex, if
					this is set to 1 then registered should
					also be set to 1. This is used in the
					XA code */
	/*------------------------------*/
	ulint		check_unique_secondary;
					/*!< normally TRUE, but if the user
					wants to speed up inserts by
					suppressing unique key checks
					for secondary indexes when we decide
					if we can use the insert buffer for
					them, we set this FALSE */
	ulint		support_xa;	/*!< normally we do the XA two-phase
					commit steps, but by setting this to
					FALSE, one can save CPU time and about
					150 bytes in the undo log size as then
					we skip XA steps */
	ulint		flush_log_later;/* In 2PC, we hold the
					prepare_commit mutex across
					both phases. In that case, we
					defer flush of the logs to disk
					until after we release the
					mutex. */
	ulint		must_flush_log_later;/*!< this flag is set to TRUE in
					trx_commit() if flush_log_later was
					TRUE, and there were modifications by
					the transaction; in that case we must
					flush the log in
					trx_commit_complete_for_mysql() */
	ulint		duplicates;	/*!< TRX_DUP_IGNORE | TRX_DUP_REPLACE */
	ulint		has_search_latch;
					/*!< TRUE if this trx has latched the
					search system latch in S-mode */
	ulint		search_latch_timeout;
					/*!< If we notice that someone is
					waiting for our S-lock on the search
					latch to be released, we wait in
					row0sel.cc for BTR_SEA_TIMEOUT new
					searches until we try to keep
					the search latch again over
					calls from MySQL; this is intended
					to reduce contention on the search
					latch */
	trx_dict_op_t	dict_operation;	/**< @see enum trx_dict_op */

	/* Fields protected by the srv_conc_mutex. */
	ulint		declared_to_be_inside_innodb;
					/*!< this is TRUE if we have declared
					this transaction in
					srv_conc_enter_innodb to be inside the
					InnoDB engine */
	ulint		n_tickets_to_enter_innodb;
					/*!< this can be > 0 only when
					declared_to_... is TRUE; when we come
					to srv_conc_innodb_enter, if the value
					here is > 0, we decrement this by 1 */
	ulint		dict_operation_lock_mode;
					/*!< 0, RW_S_LATCH, or RW_X_LATCH:
					the latch mode trx currently holds
					on dict_operation_lock. Protected
					by dict_operation_lock. */

	trx_id_t	no;		/*!< transaction serialization number:
					max trx id shortly before the
					transaction is moved to
					COMMITTED_IN_MEMORY state.
					Protected by trx_sys_t::mutex
					when trx->in_rw_trx_list. Initially
					set to IB_ULONGLONG_MAX. */

	time_t		start_time;	/*!< time the trx object was created
					or the state last time became
					TRX_STATE_ACTIVE */
	trx_id_t	id;		/*!< transaction id */
	XID		xid;		/*!< X/Open XA transaction
					identification to identify a
					transaction branch */
	lsn_t		commit_lsn;	/*!< lsn at the time of the commit */
	table_id_t	table_id;	/*!< Table to drop iff dict_operation
					== TRX_DICT_OP_TABLE, or 0. */
	/*------------------------------*/
	THD*		mysql_thd;	/*!< MySQL thread handle corresponding
					to this trx, or NULL */
	const char*	mysql_log_file_name;
					/*!< if MySQL binlog is used, this field
					contains a pointer to the latest file
					name; this is NULL if binlog is not
					used */
	ib_int64_t	mysql_log_offset;
					/*!< if MySQL binlog is used, this
					field contains the end offset of the
					binlog entry */
	/*------------------------------*/
	ulint		n_mysql_tables_in_use; /*!< number of Innobase tables
					used in the processing of the current
					SQL statement in MySQL */
	ulint		mysql_n_tables_locked;
					/*!< how many tables the current SQL
					statement uses, except those
					in consistent read */
	/*------------------------------*/
	UT_LIST_NODE_T(trx_t)
			trx_list;	/*!< list of transactions;
					protected by trx_sys->mutex.
					The same node is used for both
					trx_sys_t::ro_trx_list and
					trx_sys_t::rw_trx_list */
#ifdef UNIV_DEBUG
	/** The following two fields are mutually exclusive. */
	/* @{ */

	ibool		in_ro_trx_list;	/*!< TRUE if in trx_sys->ro_trx_list */
	ibool		in_rw_trx_list;	/*!< TRUE if in trx_sys->rw_trx_list */
	/* @} */
#endif /* UNIV_DEBUG */
	UT_LIST_NODE_T(trx_t)
			mysql_trx_list;	/*!< list of transactions created for
					MySQL; protected by trx_sys->mutex */
#ifdef UNIV_DEBUG
	ibool		in_mysql_trx_list;
					/*!< TRUE if in
					trx_sys->mysql_trx_list */
#endif /* UNIV_DEBUG */
	/*------------------------------*/
	dberr_t		error_state;	/*!< 0 if no error, otherwise error
					number; NOTE That ONLY the thread
					doing the transaction is allowed to
					set this field: this is NOT protected
					by any mutex */
	const dict_index_t*error_info;	/*!< if the error number indicates a
					duplicate key error, a pointer to
					the problematic index is stored here */
	ulint		error_key_num;	/*!< if the index creation fails to a
					duplicate key error, a mysql key
					number of that index is stored here */
	sess_t*		sess;		/*!< session of the trx, NULL if none */
	que_t*		graph;		/*!< query currently run in the session,
					or NULL if none; NOTE that the query
					belongs to the session, and it can
					survive over a transaction commit, if
					it is a stored procedure with a COMMIT
					WORK statement, for instance */
	mem_heap_t*	global_read_view_heap;
					/*!< memory heap for the global read
					view */
	read_view_t*	global_read_view;
					/*!< consistent read view associated
					to a transaction or NULL */
	read_view_t*	read_view;	/*!< consistent read view used in the
					transaction or NULL, this read view
					if defined can be normal read view
					associated to a transaction (i.e.
					same as global_read_view) or read view
					associated to a cursor */
	/*------------------------------*/
	UT_LIST_BASE_NODE_T(trx_named_savept_t)
			trx_savepoints;	/*!< savepoints set with SAVEPOINT ...,
					oldest first */
	/*------------------------------*/
	ib_mutex_t		undo_mutex;	/*!< mutex protecting the fields in this
					section (down to undo_no_arr), EXCEPT
					last_sql_stat_start, which can be
					accessed only when we know that there
					cannot be any activity in the undo
					logs! */
	undo_no_t	undo_no;	/*!< next undo log record number to
					assign; since the undo log is
					private for a transaction, this
					is a simple ascending sequence
					with no gaps; thus it represents
					the number of modified/inserted
					rows in a transaction */
	trx_savept_t	last_sql_stat_start;
					/*!< undo_no when the last sql statement
					was started: in case of an error, trx
					is rolled back down to this undo
					number; see note at undo_mutex! */
	trx_rseg_t*	rseg;		/*!< rollback segment assigned to the
					transaction, or NULL if not assigned
					yet */
	trx_undo_t*	insert_undo;	/*!< pointer to the insert undo log, or
					NULL if no inserts performed yet */
	trx_undo_t*	update_undo;	/*!< pointer to the update undo log, or
					NULL if no update performed yet */
	undo_no_t	roll_limit;	/*!< least undo number to undo during
					a rollback */
	ulint		pages_undone;	/*!< number of undo log pages undone
					since the last undo log truncation */
	trx_undo_arr_t*	undo_no_arr;	/*!< array of undo numbers of undo log
					records which are currently processed
					by a rollback operation */
	/*------------------------------*/
	ulint		n_autoinc_rows;	/*!< no. of AUTO-INC rows required for
					an SQL statement. This is useful for
					multi-row INSERTs */
	ib_vector_t*    autoinc_locks;  /* AUTOINC locks held by this
					transaction. Note that these are
					also in the lock list trx_locks. This
					vector needs to be freed explicitly
					when the trx instance is destroyed.
					Protected by lock_sys->mutex. */
	/*------------------------------*/
	ibool		read_only;	/*!< TRUE if transaction is flagged
					as a READ-ONLY transaction.
					if !auto_commit || will_lock > 0
					then it will added to the list
					trx_sys_t::ro_trx_list. A read only
					transaction will not be assigned an
					UNDO log. Non-locking auto-commit
					read-only transaction will not be on
					either list. */
	ibool		auto_commit;	/*!< TRUE if it is an autocommit */
	ulint		will_lock;	/*!< Will acquire some locks. Increment
					each time we determine that a lock will
					be acquired by the MySQL layer. */
	bool		ddl;		/*!< true if it is a transaction that
					is being started for a DDL operation */
	/*------------------------------*/
	fts_trx_t*	fts_trx;	/*!< FTS information, or NULL if
					transaction hasn't modified tables
					with FTS indexes (yet). */
	doc_id_t	fts_next_doc_id;/* The document id used for updates */
	/*------------------------------*/
	ulint		flush_tables;	/*!< if "covering" the FLUSH TABLES",
					count of tables being flushed. */

	/*------------------------------*/
#ifdef UNIV_DEBUG
	ulint		start_line;	/*!< Track where it was started from */
	const char*	start_file;	/*!< Filename where it was started */
#endif /* UNIV_DEBUG */

	trx_mod_tables_t mod_tables;	/*!< List of tables that were modified
					by this transaction */

	/*------------------------------*/
	char detailed_error[256];	/*!< detailed error message for last
					error, or empty. */
};

/* Transaction isolation levels (trx->isolation_level) */
#define TRX_ISO_READ_UNCOMMITTED	0	/* dirty read: non-locking
						SELECTs are performed so that
						we do not look at a possible
						earlier version of a record;
						thus they are not 'consistent'
						reads under this isolation
						level; otherwise like level
						2 */

#define TRX_ISO_READ_COMMITTED		1	/* somewhat Oracle-like
						isolation, except that in
						range UPDATE and DELETE we
						must block phantom rows
						with next-key locks;
						SELECT ... FOR UPDATE and ...
						LOCK IN SHARE MODE only lock
						the index records, NOT the
						gaps before them, and thus
						allow free inserting;
						each consistent read reads its
						own snapshot */

#define TRX_ISO_REPEATABLE_READ		2	/* this is the default;
						all consistent reads in the
						same trx read the same
						snapshot;
						full next-key locking used
						in locking reads to block
						insertions into gaps */

#define TRX_ISO_SERIALIZABLE		3	/* all plain SELECTs are
						converted to LOCK IN SHARE
						MODE reads */

/* Treatment of duplicate values (trx->duplicates; for example, in inserts).
Multiple flags can be combined with bitwise OR. */
#define TRX_DUP_IGNORE	1	/* duplicate rows are to be updated */
#define TRX_DUP_REPLACE	2	/* duplicate rows are to be replaced */


/* Types of a trx signal */
#define TRX_SIG_NO_SIGNAL		0
#define TRX_SIG_TOTAL_ROLLBACK		1
#define TRX_SIG_ROLLBACK_TO_SAVEPT	2
#define TRX_SIG_COMMIT			3
#define TRX_SIG_BREAK_EXECUTION		5

/* Sender types of a signal */
#define TRX_SIG_SELF		0	/* sent by the session itself, or
					by an error occurring within this
					session */
#define TRX_SIG_OTHER_SESS	1	/* sent by another session (which
					must hold rights to this) */

/** Commit node states */
enum commit_node_state {
	COMMIT_NODE_SEND = 1,	/*!< about to send a commit signal to
				the transaction */
	COMMIT_NODE_WAIT	/*!< commit signal sent to the transaction,
				waiting for completion */
};

/** Commit command node in a query graph */
struct commit_node_t{
	que_common_t	common;	/*!< node type: QUE_NODE_COMMIT */
	enum commit_node_state
			state;	/*!< node execution state */
};


/** Test if trx->mutex is owned. */
#define trx_mutex_own(t) mutex_own(&t->mutex)

/** Acquire the trx->mutex. */
#define trx_mutex_enter(t) do {			\
	mutex_enter(&t->mutex);			\
} while (0)

/** Release the trx->mutex. */
#define trx_mutex_exit(t) do {			\
	mutex_exit(&t->mutex);			\
} while (0)

/** @brief The latch protecting the adaptive search system

This latch protects the
(1) hash index;
(2) columns of a record to which we have a pointer in the hash index;

but does NOT protect:

(3) next record offset field in a record;
(4) next or previous records on the same page.

Bear in mind (3) and (4) when using the hash index.
*/
extern rw_lock_t*	btr_search_latch_temp;

/** The latch protecting the adaptive search system */
#define btr_search_latch	(*btr_search_latch_temp)

#ifndef UNIV_NONINL
#include "trx0trx.ic"
#endif
#endif /* !UNIV_HOTBACKUP */

#endif
