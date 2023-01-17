/*****************************************************************************

Copyright (c) 1996, 2023, Oracle and/or its affiliates.

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

/** @file include/que0que.h
 Query graph

 Created 5/27/1996 Heikki Tuuri
 *******************************************************/

#ifndef que0que_h
#define que0que_h

#include "data0data.h"
#include "dict0types.h"
#include "pars0types.h"
#include "que0types.h"
#include "row0types.h"
#include "srv0srv.h"
#include "trx0roll.h"
#include "trx0trx.h"
#include "univ.i"
#include "usr0types.h"

/** Mutex protecting the query threads. */
extern ib_mutex_t que_thr_mutex;

/** Creates a query graph fork node.
 @return own: fork node */
que_fork_t *que_fork_create(
    que_t *graph,       /*!< in: graph, if NULL then this
                        fork node is assumed to be the
                        graph root */
    que_node_t *parent, /*!< in: parent node */
    ulint fork_type,    /*!< in: fork type */
    mem_heap_t *heap);  /*!< in: memory heap where created */
/** Gets the first thr in a fork. */
static inline que_thr_t *que_fork_get_first_thr(
    que_fork_t *fork); /*!< in: query fork */
/** Gets the child node of the first thr in a fork. */
static inline que_node_t *que_fork_get_child(
    que_fork_t *fork); /*!< in: query fork */

/** Sets the parent of a graph node.
@param[in]      node    graph node
@param[in]      parent  parent */
static inline void que_node_set_parent(que_node_t *node, que_node_t *parent);

/** Creates a query graph thread node.
@param[in]      parent          parent node, i.e., a fork node
@param[in]      heap            memory heap where created
@param[in]      prebuilt        row prebuilt structure
@return own: query thread node */
que_thr_t *que_thr_create(que_fork_t *parent, mem_heap_t *heap,
                          row_prebuilt_t *prebuilt);
/** Frees a query graph, but not the heap where it was created. Does not free
 explicit cursor declarations, they are freed in que_graph_free. */
void que_graph_free_recursive(que_node_t *node); /*!< in: query graph node */
/** Frees a query graph. */
void que_graph_free(que_t *graph); /*!< in: query graph; we assume that the
                                   memory heap where this graph was created is
                                   private to this graph: if not, then use
                                   que_graph_free_recursive and free the heap
                                   afterwards! */
/** Stops a query thread if graph or trx is in a state requiring it. The
 conditions are tested in the order (1) graph, (2) trx.
 Caller must hold the trx mutex.
 @param[in,out]   thr   query thread
 @return true if stopped */
bool que_thr_stop(que_thr_t *thr);

/** Moves a thread from another state to the QUE_THR_RUNNING state. Increments
 the n_active_thrs counters of the query graph and transaction. */
void que_thr_move_to_run_state_for_mysql(
    que_thr_t *thr, /*!< in: an query thread */
    trx_t *trx);    /*!< in: transaction */

/** A patch for MySQL used to 'stop' a dummy query thread used in MySQL
 select, when there is no error or lock wait.
@param[in] thr Query thread
@param[in] trx Transaction */
void que_thr_stop_for_mysql_no_error(que_thr_t *thr, trx_t *trx);

/** A patch for MySQL used to 'stop' a dummy query thread used in MySQL. The
 query thread is stopped and made inactive, except in the case where
 it was put to the lock wait state in lock0lock.cc, but the lock has already
 been granted or the transaction chosen as a victim in deadlock resolution. */
void que_thr_stop_for_mysql(que_thr_t *thr); /*!< in: query thread */
/** Run a query thread. Handles lock waits. */
void que_run_threads(que_thr_t *thr); /*!< in: query thread */
/** Moves a suspended query thread to the QUE_THR_RUNNING state and release
 a worker thread to execute it. This function should be used to end
 the wait state of a query thread waiting for a lock or a stored procedure
 completion.
 @return query thread instance of thread to wakeup or NULL */
que_thr_t *que_thr_end_lock_wait(trx_t *trx); /*!< in: transaction in the
                                              QUE_THR_LOCK_WAIT state */
/** Starts execution of a command in a query fork. Picks a query thread which
 is not in the QUE_THR_RUNNING state and moves it to that state. If none
 can be chosen, a situation which may arise in parallelized fetches, NULL
 is returned.
 @return a query thread of the graph moved to QUE_THR_RUNNING state, or
 NULL; the query thread should be executed by que_run_threads by the
 caller */
que_thr_t *que_fork_start_command(que_fork_t *fork); /*!< in: a query fork */
/** Gets the trx of a query thread. */
static inline trx_t *thr_get_trx(que_thr_t *thr); /*!< in: query thread */
/** Determines if this thread is rolling back an incomplete transaction
 in crash recovery.
 @return true if thr is rolling back an incomplete transaction in crash
 recovery */
static inline bool thr_is_recv(const que_thr_t *thr); /*!< in: query thread */
/** Gets the type of a graph node. */
static inline ulint que_node_get_type(
    const que_node_t *node); /*!< in: graph node */
/** Gets pointer to the value data type field of a graph node. */
static inline dtype_t *que_node_get_data_type(
    que_node_t *node); /*!< in: graph node */
/** Gets pointer to the value dfield of a graph node. */
static inline dfield_t *que_node_get_val(
    que_node_t *node); /*!< in: graph node */
/** Gets the value buffer size of a graph node.
 @return val buffer size, not defined if val.data == NULL in node */
static inline ulint que_node_get_val_buf_size(
    que_node_t *node); /*!< in: graph node */

/** Sets the value buffer size of a graph node.
@param[in]      node    graph node
@param[in]      size    size */
static inline void que_node_set_val_buf_size(que_node_t *node, ulint size);

/** Gets the next list node in a list of query graph nodes. */
static inline que_node_t *que_node_get_next(
    que_node_t *node); /*!< in: node in a list */
/** Gets the parent node of a query graph node.
 @return parent node or NULL */
static inline que_node_t *que_node_get_parent(
    que_node_t *node); /*!< in: node */
/** Get the first containing loop node (e.g. while_node_t or for_node_t) for the
 given node, or NULL if the node is not within a loop.
 @return containing loop node, or NULL. */
que_node_t *que_node_get_containing_loop_node(
    que_node_t *node); /*!< in: node */

/** Catenates a query graph node to a list of them, possible empty list.
@param[in]      node_list       node list, or NULL
@param[in]      node            node
@return one-way list of nodes */
static inline que_node_t *que_node_list_add_last(que_node_t *node_list,
                                                 que_node_t *node);

/*************************************************************************
Get the last node from the list.*/
static inline que_node_t *que_node_list_get_last(
    /* out: node last node from list.*/
    que_node_t *node_list); /* in: node list, or NULL */
/** Gets a query graph node list length.
 @return length, for NULL list 0 */
static inline ulint que_node_list_get_len(
    que_node_t *node_list); /*!< in: node list, or NULL */
/** Checks if graph, trx, or session is in a state where the query thread should
 be stopped.
 @return true if should be stopped; NOTE that if the peek is made
 without reserving the trx_t::mutex, then another peek with the mutex
 reserved is necessary before deciding the actual stopping */
static inline bool que_thr_peek_stop(que_thr_t *thr); /*!< in: query thread */
/** Returns true if the query graph is for a SELECT statement.
 @return true if a select */
static inline bool que_graph_is_select(que_t *graph); /*!< in: graph */
/** Prints info of an SQL query graph node. */
void que_node_print_info(que_node_t *node); /*!< in: query graph node */
/** Evaluate the given SQL
@param[in] info Info struct, or nullptr.
@param[in] sql SQL string.
@param[in] trx Transaction.
@return error code or DB_SUCCESS */
dberr_t que_eval_sql(pars_info_t *info, const char *sql, trx_t *trx);

/** Round robin scheduler.
 @return a query thread of the graph moved to QUE_THR_RUNNING state, or
 NULL; the query thread should be executed by que_run_threads by the
 caller */
que_thr_t *que_fork_scheduler_round_robin(
    que_fork_t *fork, /*!< in: a query fork */
    que_thr_t *thr);  /*!< in: current pos */

/** Initialise the query sub-system. */
void que_init(void);

/** Close the query sub-system. */
void que_close(void);

/** Query thread states */
enum que_thr_state_t {
  QUE_THR_RUNNING,
  QUE_THR_PROCEDURE_WAIT,
  /** in selects this means that the thread is at the end of its
  result set (or start, in case of a scroll cursor); in other
  statements, this means the thread has done its task */
  QUE_THR_COMPLETED,
  QUE_THR_COMMAND_WAIT,
  QUE_THR_LOCK_WAIT,
  QUE_THR_SUSPENDED
};

/** Query thread lock states */
enum que_thr_lock_t {
  QUE_THR_LOCK_NOLOCK,
  QUE_THR_LOCK_ROW,
  QUE_THR_LOCK_TABLE
};

/** From where the cursor position is counted */
enum que_cur_t { QUE_CUR_NOT_DEFINED, QUE_CUR_START, QUE_CUR_END };

/* Query graph query thread node: the fields are protected by the
trx_t::mutex with the exceptions named below */

struct que_thr_t {
  que_common_t common;   /*!< type: QUE_NODE_THR */
  ulint magic_n;         /*!< magic number to catch memory
                         corruption */
  que_node_t *child;     /*!< graph child node */
  que_t *graph;          /*!< graph where this node belongs */
  que_thr_state_t state; /*!< state of the query thread */
  bool is_active;        /*!< true if the thread has been set
                          to the run state in
                          que_thr_move_to_run_state, but not
                          deactivated in
                          que_thr_dec_reference_count */
  /*------------------------------*/
  /* The following fields are private to the OS thread executing the
  query thread, and are not protected by any mutex: */

  que_node_t *run_node;  /*!< pointer to the node where the
                         subgraph down from this node is
                         currently executed */
  que_node_t *prev_node; /*!< pointer to the node from which
                         the control came */
  ulint resource;        /*!< resource usage of the query thread
                         thus far */
  ulint lock_state;      /*!< lock state of thread (table or
                         row) */

  /** The thread slot in the lock_sys->waiting_threads array protected by
  lock_sys->wait_mutex when writing to it, and also by trx->mutex when changing
  from null to non-null.
  While reading, one can either hold the lock_sys->wait_mutex, or hold the
  trx->mutex and a proof that no one has woken the trx yet,
  so the slot is either still null (if trx hadn't yet started the sleep), or
  already non-null (if it already started sleep), but definitely not
  changing from null to non-null (as it requires trx->mutex) nor
  changing from non-null to null (as it happens after wake up). */
  struct srv_slot_t *slot;
  /*------------------------------*/
  /* The following fields are links for the various lists that
  this type can be on. */
  UT_LIST_NODE_T(que_thr_t)
  thrs; /*!< list of thread nodes of the fork
        node */
  UT_LIST_NODE_T(que_thr_t)
  trx_thrs; /*!< lists of threads in wait list of
            the trx */
  UT_LIST_NODE_T(que_thr_t)
  queue;                    /*!< list of runnable thread nodes in
                            the server task queue */
  ulint fk_cascade_depth;   /*!< maximum cascading call depth
                          supported for foreign key constraint
                          related delete/updates */
  row_prebuilt_t *prebuilt; /*!< prebuilt structure processed by
                            the query thread */
};

constexpr uint32_t QUE_THR_MAGIC_N = 8476583;
constexpr uint32_t QUE_THR_MAGIC_FREED = 123461526;

/* Query graph fork node: its fields are protected by the query thread mutex */
struct que_fork_t {
  que_common_t common; /*!< type: QUE_NODE_FORK */
  que_t *graph;        /*!< query graph of this node */
  ulint fork_type;     /*!< fork type */
  ulint n_active_thrs; /*!< if this is the root of a graph, the
                       number query threads that have been
                       started in que_thr_move_to_run_state
                       but for which que_thr_dec_refer_count
                       has not yet been called */
  trx_t *trx;          /*!< transaction: this is set only in
                       the root node */
  ulint state;         /*!< state of the fork node */
  que_thr_t *caller;   /*!< pointer to a possible calling query
                       thread */
  UT_LIST_BASE_NODE_T(que_thr_t, thrs)
  thrs; /*!< list of query threads */
  /*------------------------------*/
  /* The fields in this section are defined only in the root node */
  sym_tab_t *sym_tab; /*!< symbol table of the query,
                      generated by the parser, or NULL
                      if the graph was created 'by hand' */
  pars_info_t *info;  /*!< info struct, or NULL */
  /* The following cur_... fields are relevant only in a select graph */

  ulint cur_end;             /*!< QUE_CUR_NOT_DEFINED, QUE_CUR_START,
                             QUE_CUR_END */
  ulint cur_pos;             /*!< if there are n rows in the result
                             set, values 0 and n + 1 mean before
                             first row, or after last row, depending
                             on cur_end; values 1...n mean a row
                             index */
  bool cur_on_row;           /*!< true if cursor is on a row, i.e.,
                              it is not before the first row or
                              after the last row */
  sel_node_t *last_sel_node; /*!< last executed select node, or NULL
                             if none */
  UT_LIST_NODE_T(que_fork_t)
  graphs; /*!< list of query graphs of a session
          or a stored procedure */
  /*------------------------------*/
  mem_heap_t *heap; /*!< memory heap where the fork was
                    created */
};

/* Query fork (or graph) types */
constexpr uint32_t QUE_FORK_SELECT_NON_SCROLL = 1; /* forward-only cursor */
constexpr uint32_t QUE_FORK_SELECT_SCROLL = 2;     /* scrollable cursor */
constexpr uint32_t QUE_FORK_INSERT = 3;
constexpr uint32_t QUE_FORK_UPDATE = 4;
constexpr uint32_t QUE_FORK_ROLLBACK = 5;
/* This is really the undo graph used in rollback,
no signal-sending roll_node in this graph */
constexpr uint32_t QUE_FORK_PURGE = 6;
constexpr uint32_t QUE_FORK_EXECUTE = 7;
constexpr uint32_t QUE_FORK_PROCEDURE = 8;
constexpr uint32_t QUE_FORK_PROCEDURE_CALL = 9;
constexpr uint32_t QUE_FORK_MYSQL_INTERFACE = 10;
constexpr uint32_t QUE_FORK_RECOVERY = 11;

/* Query fork (or graph) states */
constexpr uint32_t QUE_FORK_ACTIVE = 1;
constexpr uint32_t QUE_FORK_COMMAND_WAIT = 2;
constexpr uint32_t QUE_FORK_INVALID = 3;
constexpr uint32_t QUE_FORK_BEING_FREED = 4;

/* Flag which is ORed to control structure statement node types */
constexpr uint32_t QUE_NODE_CONTROL_STAT = 1024;

/* Query graph node types */
constexpr uint32_t QUE_NODE_LOCK = 1;
constexpr uint32_t QUE_NODE_INSERT = 2;
constexpr uint32_t QUE_NODE_UPDATE = 4;
constexpr uint32_t QUE_NODE_CURSOR = 5;
constexpr uint32_t QUE_NODE_SELECT = 6;
constexpr uint32_t QUE_NODE_AGGREGATE = 7;
constexpr uint32_t QUE_NODE_FORK = 8;
constexpr uint32_t QUE_NODE_THR = 9;
constexpr uint32_t QUE_NODE_UNDO = 10;
constexpr uint32_t QUE_NODE_COMMIT = 11;
constexpr uint32_t QUE_NODE_ROLLBACK = 12;
constexpr uint32_t QUE_NODE_PURGE = 13;
constexpr uint32_t QUE_NODE_SYMBOL = 14;
constexpr uint32_t QUE_NODE_RES_WORD = 15;
constexpr uint32_t QUE_NODE_FUNC = 16;
constexpr uint32_t QUE_NODE_ORDER = 17;
constexpr uint32_t QUE_NODE_PROC = 18 + QUE_NODE_CONTROL_STAT;
constexpr uint32_t QUE_NODE_IF = 19 + QUE_NODE_CONTROL_STAT;
constexpr uint32_t QUE_NODE_WHILE = 20 + QUE_NODE_CONTROL_STAT;
constexpr uint32_t QUE_NODE_ASSIGNMENT = 21;
constexpr uint32_t QUE_NODE_FETCH = 22;
constexpr uint32_t QUE_NODE_OPEN = 23;
constexpr uint32_t QUE_NODE_COL_ASSIGNMENT = 24;
constexpr uint32_t QUE_NODE_FOR = 25 + QUE_NODE_CONTROL_STAT;
constexpr uint32_t QUE_NODE_RETURN = 26;
constexpr uint32_t QUE_NODE_ELSIF = 27;
constexpr uint32_t QUE_NODE_CALL = 28;
constexpr uint32_t QUE_NODE_EXIT = 29;

#include "que0que.ic"

#endif
