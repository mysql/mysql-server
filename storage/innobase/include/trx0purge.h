/*****************************************************************************

Copyright (c) 1996, 2022, Oracle and/or its affiliates.

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

/** @file include/trx0purge.h
 Purge old versions

 Created 3/26/1996 Heikki Tuuri
 *******************************************************/

#ifndef trx0purge_h
#define trx0purge_h

#include <unordered_set>
#include "fil0fil.h"
#include "mtr0mtr.h"
#include "page0page.h"
#include "que0types.h"
#include "read0types.h"
#include "trx0sys.h"
#include "trx0types.h"
#include "univ.i"
#include "usr0sess.h"
#ifdef UNIV_HOTBACKUP
#include "trx0sys.h"
#endif /* UNIV_HOTBACKUP */

/** The global data structure coordinating a purge */
extern trx_purge_t *purge_sys;

/** Calculates the file address of an undo log header when we have the file
 address of its history list node.
 @return file address of the log */
static inline fil_addr_t trx_purge_get_log_from_hist(
    fil_addr_t node_addr); /*!< in: file address of the history
                           list node of the log */

/** Initialize in-memory purge structures */
void trx_purge_sys_mem_create();

/** Creates the global purge system control structure and inits the history
mutex.
@param[in]      n_purge_threads   number of purge threads
@param[in,out]  purge_queue       UNDO log min binary heap */
void trx_purge_sys_initialize(uint32_t n_purge_threads,
                              purge_pq_t *purge_queue);

/** Frees the global purge system control structure. */
void trx_purge_sys_close(void);

/************************************************************************
Adds the update undo log as the first log in the history list. Removes the
update undo log segment from the rseg slot if it is too big for reuse. */
void trx_purge_add_update_undo_to_history(
    trx_t *trx,               /*!< in: transaction */
    trx_undo_ptr_t *undo_ptr, /*!< in: update undo log. */
    page_t *undo_page,        /*!< in: update undo log header page,
                              x-latched */
    bool update_rseg_history_len,
    /*!< in: if true: update rseg history
    len else skip updating it. */
    ulint n_added_logs, /*!< in: number of logs added */
    mtr_t *mtr);        /*!< in: mtr */

/** This function runs a purge batch.
 @return number of undo log pages handled in the batch */
ulint trx_purge(ulint n_purge_threads, /*!< in: number of purge tasks to
                                       submit to task queue. */
                ulint limit,           /*!< in: the maximum number of
                                       records to purge in one batch */
                bool truncate);        /*!< in: truncate history if true */

/** Stop purge and wait for it to stop, move to PURGE_STATE_STOP. */
void trx_purge_stop(void);
/** Resume purge, move to PURGE_STATE_RUN. */
void trx_purge_run(void);

/** Purge states */
enum purge_state_t {
  PURGE_STATE_INIT,    /*!< Purge instance created */
  PURGE_STATE_RUN,     /*!< Purge should be running */
  PURGE_STATE_STOP,    /*!< Purge should be stopped */
  PURGE_STATE_EXIT,    /*!< Purge has been shutdown */
  PURGE_STATE_DISABLED /*!< Purge was never started */
};

/** Get the purge state.
 @return purge state. */
purge_state_t trx_purge_state(void);

// Forward declaration
struct TrxUndoRsegsIterator;

/** This is the purge pointer/iterator. We need both the undo no and the
transaction no up to which purge has parsed and applied the records. */
struct purge_iter_t {
  purge_iter_t() : trx_no(), undo_no(), undo_rseg_space(SPACE_UNKNOWN) {
    // Do nothing
  }

  /** Purge has advanced past all transactions whose number
  is less than this */
  trx_id_t trx_no;

  /** Purge has advanced past all records whose undo number
  is less than this. */
  undo_no_t undo_no;

  /** The last undo record resided in this space id */
  space_id_t undo_rseg_space;

  /** The transaction that created the undo log record,
  the Modifier trx id */
  trx_id_t modifier_trx_id;
};

/* Namespace to hold all the related functions and variables needed
to truncate an undo tablespace. */
namespace undo {

/** Magic Number to indicate truncate action is complete. */
const uint32_t s_magic = 76845412;

/** Truncate Log file Prefix. */
const char *const s_log_prefix = "undo_";

/** Truncate Log file Extension. */
const char *const s_log_ext = "trunc.log";

/** The currently used undo space IDs for an undo space number
along with a boolean showing whether the undo space number is in use. */
struct space_id_account {
  space_id_t space_id;
  bool in_use;
};

/** List of currently used undo space IDs for each undo space number
along with a boolean showing whether the undo space number is in use. */
extern struct space_id_account *space_id_bank;

/** Check if the space_id is an undo space ID in the reserved range.
@param[in]      space_id        undo tablespace ID
@return true if it is in the reserved undo space ID range. */
inline bool is_reserved(space_id_t space_id) {
  return (space_id >= dict_sys_t::s_min_undo_space_id &&
          space_id <= dict_sys_t::s_max_undo_space_id);
}

/** Convert an undo space number (from 1 to 127) into the undo space_id,
given an index indicating which space_id from the pool assigned to that
undo number.
@param[in]  space_num  undo tablespace number
@param[in]  ndx        index of the space_id within that undo number
@return space_id of the undo tablespace */
inline space_id_t num2id(space_id_t space_num, size_t ndx) {
  ut_ad(space_num > 0);
  ut_ad(space_num <= FSP_MAX_UNDO_TABLESPACES);
  ut_ad(ndx < dict_sys_t::s_undo_space_id_range);

  space_id_t space_id = dict_sys_t::s_max_undo_space_id + 1 - space_num -
                        static_cast<space_id_t>(ndx * FSP_MAX_UNDO_TABLESPACES);

  return (space_id);
}

/** Convert an undo space number (from 1 to 127) into an undo space_id.
Use the undo::space_id_bank to return the current space_id assigned to
that undo number.
@param[in]  space_num   undo tablespace number
@return space_id of the undo tablespace */
inline space_id_t num2id(space_id_t space_num) {
  ut_ad(space_num > 0);
  ut_ad(space_num <= FSP_MAX_UNDO_TABLESPACES);

  size_t slot = space_num - 1;

  /* The space_id_back is normally protected by undo::spaces::m_latch.
  But this can only be called on a specific slot when truncation is not
  happening on that slot, i.e. the undo tablespace is in use. */
  ut_ad(undo::space_id_bank[slot].in_use);

  return (undo::space_id_bank[slot].space_id);
}

/* clang-format off */
/** Convert an undo space ID into an undo space number.
NOTE: This may be an undo space_id from a pre-exisiting 5.7
database which used space_ids from 1 to 127.  If so, the
space_id is the space_num.
The space_ids are assigned to number ranges in reverse from high to low.
In addition, the first space IDs for each undo number occur sequentially
and descending before the second space_id.

Since s_max_undo_space_id = 0xFFFFFFEF, FSP_MAX_UNDO_TABLESPACES = 127
and s_undo_space_id_range = 400,000:
  Space ID   Space Num    Space ID   Space Num   ...  Space ID   Space Num
  0xFFFFFFEF      1       0xFFFFFFEe       2     ...  0xFFFFFF71    127
  0xFFFFFF70      1       0xFFFFFF6F       2     ...  0xFFFFFEF2    127
  0xFFFFFEF1      1       0xFFFFFEF0       2     ...  0xFFFFFE73    127
...

This is done to maintain backward compatibility to when there was only one
space_id per undo space number.
@param[in]      space_id        undo tablespace ID
@return space number of the undo tablespace */
/* clang-format on */
inline space_id_t id2num(space_id_t space_id) {
  if (!is_reserved(space_id)) {
    return (space_id);
  }

  return (((dict_sys_t::s_max_undo_space_id - space_id) %
           FSP_MAX_UNDO_TABLESPACES) +
          1);
}

/* Given a reserved undo space_id, return the next space_id for the associated
undo space number. */
inline space_id_t id2next_id(space_id_t space_id) {
  ut_ad(is_reserved(space_id));

  space_id_t space_num = id2num(space_id);
  space_id_t first_id = dict_sys_t::s_max_undo_space_id + 1 - space_num;
  space_id_t last_id = first_id - (FSP_MAX_UNDO_TABLESPACES *
                                   (dict_sys_t::s_undo_space_id_range - 1));

  return (space_id == SPACE_UNKNOWN || space_id == last_id
              ? first_id
              : space_id - FSP_MAX_UNDO_TABLESPACES);
}

/** Initialize the undo tablespace space_id bank which is a lock free
repository for information about the space IDs used for undo tablespaces.
It is used during creation in order to assign an unused space number and
during truncation in order to assign the next space_id within that
space_number range. */
void init_space_id_bank();

/** Note that the undo space number for a space ID is being used.
Put that space_id into the space_id_bank.
@param[in] space_id  undo tablespace number */
void use_space_id(space_id_t space_id);

/** Mark that the given undo space number is being used and
return the next available space_id for that space number.
@param[in]  space_num  undo tablespace number
@return the next tablespace ID to use */
space_id_t use_next_space_id(space_id_t space_num);

/** Mark an undo number associated with a given space_id as unused and
available to be reused.  This happens when the fil_space_t is closed
associated with a drop undo tablespace.
@param[in] space_id  Undo Tablespace ID */
void unuse_space_id(space_id_t space_id);

/** Given a valid undo space_id or SPACE_UNKNOWN, return the next space_id
for the given space number.
@param[in]  space_id   undo tablespace ID
@param[in]  space_num  undo tablespace number
@return the next tablespace ID to use */
space_id_t next_space_id(space_id_t space_id, space_id_t space_num);

/** Given a valid undo space_id, return the next space_id for that
space number.
@param[in]  space_id  undo tablespace ID
@return the next tablespace ID to use */
space_id_t next_space_id(space_id_t space_id);

/** Return the next available undo space ID to be used for a new explicit
undo tablespaces. The slot will be marked as in-use.
@return next available undo space number if successful.
@return SPACE_UNKNOWN if failed */
space_id_t get_next_available_space_num();

/** Build a standard undo tablespace name from a space_id.
@param[in]      space_id        id of the undo tablespace.
@return tablespace name of the undo tablespace file */
char *make_space_name(space_id_t space_id);

/** Build a standard undo tablespace file name from a space_id.
This will create a name like 'undo_001' if the space_id is in the
reserved range, else it will be like 'undo001'.
@param[in]      space_id        id of the undo tablespace.
@return file_name of the undo tablespace file */
char *make_file_name(space_id_t space_id);

/** An undo::Tablespace object is used to easily convert between
undo_space_id and undo_space_num and to create the automatic file_name
and space name.  In addition, it is used in undo::Tablespaces to track
the trx_rseg_t objects in an Rsegs vector. So we do not allocate the
Rsegs vector for each object, only when requested by the constructor. */
struct Tablespace {
  /** Constructor
  @param[in]  id    tablespace id */
  explicit Tablespace(space_id_t id)
      : m_id(id),
        m_num(undo::id2num(id)),
        m_implicit(true),
        m_new(false),
        m_space_name(),
        m_file_name(),
        m_log_file_name(),
        m_log_file_name_old(),
        m_rsegs() {}

  /** Copy Constructor
  @param[in]  other    undo tablespace to copy */
  Tablespace(Tablespace &other)
      : m_id(other.id()),
        m_num(undo::id2num(other.id())),
        m_implicit(other.is_implicit()),
        m_new(other.is_new()),
        m_space_name(),
        m_file_name(),
        m_log_file_name(),
        m_log_file_name_old(),
        m_rsegs() {
    ut_ad(m_id == 0 || is_reserved(m_id));

    set_space_name(other.space_name());
    set_file_name(other.file_name());

    /* When the copy constructor is used, add an Rsegs
    vector. This constructor is only used in the global
    undo::Tablespaces object where rollback segments are
    tracked. */
    m_rsegs = ut::new_withkey<Rsegs>(UT_NEW_THIS_FILE_PSI_KEY);
  }

  /** Destructor */
  ~Tablespace() {
    if (m_space_name != nullptr) {
      ut::free(m_space_name);
      m_space_name = nullptr;
    }

    if (m_file_name != nullptr) {
      ut::free(m_file_name);
      m_file_name = nullptr;
    }

    if (m_log_file_name != nullptr) {
      ut::free(m_log_file_name);
      m_log_file_name = nullptr;
    }

    if (m_log_file_name_old != nullptr) {
      ut::free(m_log_file_name_old);
      m_log_file_name_old = nullptr;
    }

    /* Clear the cached rollback segments.  */
    if (m_rsegs != nullptr) {
      ut::delete_(m_rsegs);
      m_rsegs = nullptr;
    }
  }

  /* Determine if this undo space needs to be truncated.
  @return true if it should be truncated, false if not. */
  bool needs_truncation();

  /** Change the space_id from its current value.
  @param[in]  space_id  The new undo tablespace ID */
  void set_space_id(space_id_t space_id);

  /** Replace the standard undo space name if it exists with a copy
  of the undo tablespace name provided.
  @param[in]  new_space_name  non-standard undo space name */
  void set_space_name(const char *new_space_name);

  /** Get the undo tablespace name. Make it if not yet made.
  NOTE: This is only called from stack objects so there is no
  race condition. If it is ever called from a shared object
  like undo::spaces, then it must be protected by the caller.
  @return tablespace name created from the space_id */
  char *space_name() {
    if (m_space_name == nullptr) {
#ifndef UNIV_HOTBACKUP
      m_space_name = make_space_name(m_id);
#endif /* !UNIV_HOTBACKUP */
    }

    return (m_space_name);
  }

  /** Replace the standard undo file name if it exists with a copy
  of the file name provided. This name can come in three forms:
  absolute path, relative path, and basename.  Undo ADD DATAFILE
  does not accept a relative path.  So if that comes in here, it
  was the scanned name and is relative to the datadir.
  If this is just a basename, add it to srv_undo_dir.
  @param[in]  file_name  explicit undo file name */
  void set_file_name(const char *file_name);

  /** Get the undo space filename. Make it if not yet made.
  NOTE: This is only called from stack objects so there is no
  race condition. If it is ever called from a shared object
  like undo::spaces, then it must be protected by the caller.
  @return tablespace filename created from the space_id */
  char *file_name() {
    if (m_file_name == nullptr) {
      m_file_name = make_file_name(m_id);
    }

    return (m_file_name);
  }

  /** Build a log file name based on space_id
  @param[in]  space_id  id of the undo tablespace.
  @param[in]  location  directory location of the file.
  @return DB_SUCCESS or error code */
  char *make_log_file_name(space_id_t space_id, const char *location);

  /** Get the undo log filename. Make it if not yet made.
  NOTE: This is only called from stack objects so there is no
  race condition. If it is ever called from a shared object
  like undo::spaces, then it must be protected by the caller.
  @return tablespace filename created from the space_id */
  char *log_file_name() {
    if (m_log_file_name == nullptr) {
      m_log_file_name = make_log_file_name(m_id, srv_undo_dir);
    }

    return (m_log_file_name);
  }

  /** Get the old undo log filename from the srv_log_group_home_dir.
  Make it if not yet made. */
  char *log_file_name_old() {
    if (m_log_file_name_old == nullptr) {
      m_log_file_name_old = make_log_file_name(m_id, srv_log_group_home_dir);
    }

    return (m_log_file_name_old);
  }

  /** Get the undo tablespace ID.
  @return tablespace ID */
  space_id_t id() { return (m_id); }

  /** Get the undo tablespace number.  This is the same as m_id
  if m_id is 0 or this is a v5.6-5.7 undo tablespace. v8+ undo
  tablespaces use a space_id from the reserved range.
  @return undo tablespace number */
  space_id_t num() {
    ut_ad(m_num < FSP_MAX_ROLLBACK_SEGMENTS);

    return (m_num);
  }

  /** Get a reference to the List of rollback segments within
  this undo tablespace.
  @return a reference to the Rsegs vector. */
  Rsegs *rsegs() { return (m_rsegs); }

  /** Report whether this undo tablespace was explicitly created
  by an SQL statement.
  @return true if the tablespace was created explicitly. */
  bool is_explicit() { return (!m_implicit); }

  /** Report whether this undo tablespace was implicitly created.
  @return true if the tablespace was created implicitly. */
  bool is_implicit() { return (m_implicit); }

  /** Report whether this undo tablespace was created at startup.
  @retval true if created at startup.
  @retval false if pre-existed at startup. */
  bool is_new() { return (m_new); }

  /** Note that this undo tablespace is being created. */
  void set_new() { m_new = true; }

  /** Return whether the undo tablespace is active.
  @return true if active */
  bool is_active() {
    if (m_rsegs == nullptr) {
      return (false);
    }
    m_rsegs->s_lock();
    bool ret = m_rsegs->is_active();
    m_rsegs->s_unlock();
    return (ret);
  }

  /** Return whether the undo tablespace is active. For optimization purposes,
  do not take a latch.
  @return true if active */
  bool is_active_no_latch() {
    if (m_rsegs == nullptr) {
      return (false);
    }
    return (m_rsegs->is_active());
  }

  /** Return the rseg at the requested rseg slot if the undo space is active.
  @param[in] slot   The slot of the rseg.  1 to 127
  @return Rseg pointer of nullptr if the space is not active. */
  trx_rseg_t *get_active(ulint slot) {
    m_rsegs->s_lock();
    if (!m_rsegs->is_active()) {
      m_rsegs->s_unlock();
      return (nullptr);
    }

    /* Mark the chosen rseg so that it will not be selected
    for UNDO truncation. */
    trx_rseg_t *rseg = m_rsegs->at(slot);
    rseg->trx_ref_count++;

    m_rsegs->s_unlock();

    return (rseg);
  }

  /** Return whether the undo tablespace is inactive due to
  implicit selection by the purge thread.
  @return true if marked for truncation by the purge thread */
  bool is_inactive_implicit() {
    if (m_rsegs == nullptr) {
      return (false);
    }
    m_rsegs->s_lock();
    bool ret = m_rsegs->is_inactive_implicit();
    m_rsegs->s_unlock();
    return (ret);
  }

  /** Return whether the undo tablespace was made inactive by
  ALTER TABLESPACE.
  @return true if altered inactive */
  bool is_inactive_explicit() {
    if (m_rsegs == nullptr) {
      return (false);
    }
    m_rsegs->s_lock();
    bool ret = m_rsegs->is_inactive_explicit();
    m_rsegs->s_unlock();
    return (ret);
  }

  /** Return whether the undo tablespace is empty and ready
  to be dropped.
  @return true if empty */
  bool is_empty() {
    if (m_rsegs == nullptr) {
      return (true);
    }
    m_rsegs->s_lock();
    bool ret = m_rsegs->is_empty();
    m_rsegs->s_unlock();
    return (ret);
  }

  /** Set the undo tablespace active for use by transactions. */
  void set_active() {
    m_rsegs->x_lock();
    m_rsegs->set_active();
    m_rsegs->x_unlock();
  }

  /** Set the state of the rollback segments in this undo tablespace to
  inactive_implicit if currently active.  If the state is inactive_explicit,
  leave as is. Then put the space_id into the callers marked_space_id.
  This is done when marking a space for truncate.  It will not be used
  for new transactions until it becomes active again. */
  void set_inactive_implicit(space_id_t *marked_space_id) {
    m_rsegs->x_lock();
    if (m_rsegs->is_active()) {
      m_rsegs->set_inactive_implicit();
    }
    *marked_space_id = m_id;

    m_rsegs->x_unlock();
  }

  /** Make the undo tablespace inactive so that it will not be
  used for new transactions.  The purge thread will clear out
  all the undo logs, truncate it, and then mark it empty. */
  void set_inactive_explicit() {
    m_rsegs->x_lock();
    m_rsegs->set_inactive_explicit();
    m_rsegs->x_unlock();
  }

  /** Make the undo tablespace active again so that it will
  be used for new transactions.
  If current State is ___ then do:
  empty:            Set active.
  active_implicit:  Ignore.  It was not altered inactive. When it is done
                    being truncated it will go back to active.
  active_explicit:  Depends if it is marked for truncation.
    marked:         Set to inactive_implicit. the next state will be active.
    not yet:        Set to active so that it does not get truncated.  */
  void alter_active();

  /** Set the state of the undo tablespace to empty so that it
  can be dropped. */
  void set_empty() {
    m_rsegs->x_lock();
    m_rsegs->set_empty();
    m_rsegs->x_unlock();
  }

 private:
  /** Undo Tablespace ID. */
  space_id_t m_id;

  /** Undo Tablespace number, from 1 to 127. This is the
  7-bit number that is used in a rollback pointer.
  Use id2num() to get this number from a space_id. */
  space_id_t m_num;

  /** True if this is an implicit undo tablespace */
  bool m_implicit;

  /** True if this undo tablespace was implicitly created when
  this instance started up. False if it pre-existed. */
  bool m_new;

  /** The tablespace name, auto-generated when needed from
  the space number. */
  char *m_space_name;

  /** The tablespace file name, auto-generated when needed
  from the space number. */
  char *m_file_name;

  /** The truncation log file name, auto-generated when needed
  from the space number and the srv_undo_dir. */
  char *m_log_file_name;

  /** The old truncation log file name, auto-generated when needed
  from the space number and the srv_log_group_home_dir. */
  char *m_log_file_name_old;

  /** List of rollback segments within this tablespace.
  This is not always used. Must call init_rsegs to use it. */
  Rsegs *m_rsegs;
};

/** List of undo tablespaces, each containing a list of
rollback segments. */
class Tablespaces {
  using Tablespaces_Vector =
      std::vector<Tablespace *, ut::allocator<Tablespace *>>;

 public:
  Tablespaces() { init(); }

  ~Tablespaces() { deinit(); }

  /** Initialize */
  void init();

  /** De-initialize */
  void deinit();

  /** Clear the contents of the list of Tablespace objects.
  This does not deallocate any memory. */
  void clear() {
    for (auto undo_space : m_spaces) {
      ut::delete_(undo_space);
    }
    m_spaces.clear();
  }

  /** Get the number of tablespaces tracked by this object. */
  ulint size() { return (m_spaces.size()); }

  /** See if the list of tablespaces is empty. */
  bool empty() { return (m_spaces.empty()); }

  /** Get the Tablespace tracked at a position. */
  Tablespace *at(size_t pos) { return (m_spaces.at(pos)); }

  /** Add a new undo::Tablespace to the back of the vector.
  The vector has been pre-allocated to 128 so read threads will
  not loose what is pointed to. If tablespace_name and file_name
  are standard names, they are optional.
  @param[in]    ref_undo_space  undo tablespace */
  void add(Tablespace &ref_undo_space);

  /** Drop an existing explicit undo::Tablespace.
  @param[in]    undo_space      pointer to undo space */
  void drop(Tablespace *undo_space);

  /** Drop an existing explicit undo::Tablespace.
  @param[in]    ref_undo_space  reference to undo space */
  void drop(Tablespace &ref_undo_space);

  /** Check if the given space_id is in the vector.
  @param[in]  num  undo tablespace number
  @return true if space_id is found, else false */
  bool contains(space_id_t num) { return (find(num) != nullptr); }

  /** Find the given space_num in the vector.
  @param[in]  num  undo tablespace number
  @return pointer to an undo::Tablespace struct */
  Tablespace *find(space_id_t num) {
    if (m_spaces.empty()) {
      return (nullptr);
    }

    /* The sort method above puts this vector in order by
    Tablespace::num. If there are no gaps, then we should
    be able to find it quickly. */
    space_id_t slot = num - 1;
    if (slot < m_spaces.size()) {
      auto undo_space = m_spaces.at(slot);
      if (undo_space->num() == num) {
        return (undo_space);
      }
    }

    /* If there are gaps in the numbering, do a search. */
    for (auto undo_space : m_spaces) {
      if (undo_space->num() == num) {
        return (undo_space);
      }
    }

    return (nullptr);
  }

  /** Find the first undo space that is marked inactive explicitly.
  @param[in,out]  num_active  If there are no inactive_explicit spaces
                              found, this will contain the number of
                              active spaces found.
  @return pointer to an undo::Tablespace struct */
  Tablespace *find_first_inactive_explicit(size_t *num_active) {
    ut_ad(own_latch());

    if (m_spaces.empty()) {
      return (nullptr);
    }

    for (auto undo_space : m_spaces) {
      if (undo_space->is_inactive_explicit()) {
        return (undo_space);
      }

      if (num_active != nullptr && undo_space->is_active()) {
        (*num_active)++;
      }
    }

    return (nullptr);
  }

#ifdef UNIV_DEBUG
  /** Determine if this thread owns a lock on m_latch. */
  bool own_latch() {
    return (rw_lock_own(m_latch, RW_LOCK_X) || rw_lock_own(m_latch, RW_LOCK_S));
  }
#endif /* UNIV_DEBUG */

  /** Get a shared lock on m_spaces. */
  void s_lock() { rw_lock_s_lock(m_latch, UT_LOCATION_HERE); }

  /** Release a shared lock on m_spaces. */
  void s_unlock() { rw_lock_s_unlock(m_latch); }

  /** Get an exclusive lock on m_spaces. */
  void x_lock() { rw_lock_x_lock(m_latch, UT_LOCATION_HERE); }

  /** Release an exclusive lock on m_spaces. */
  void x_unlock() { rw_lock_x_unlock(m_latch); }

  Tablespaces_Vector m_spaces;

 private:
  /** RW lock to protect m_spaces.
  x for adding elements, s for scanning, size() etc. */
  rw_lock_t *m_latch;
};

/** Mutex for serializing undo tablespace related DDL.  These have to do with
creating and dropping undo tablespaces. */
extern ib_mutex_t ddl_mutex;

/** A global object that contains a vector of undo::Tablespace structs. */
extern Tablespaces *spaces;

#ifdef UNIV_DEBUG
/**  Inject a crash if a certain SET GLOBAL DEBUG has been set.
Before DBUG_SUICIDE(), write an entry about this crash to the error log
and flush the redo log. */
void inject_crash(const char *injection_point_name);

/** Inject a failure in the undo truncation debug compiled code at various
places so that it fails the first time it hits and succeeds after that. */
class Inject_failure_once {
  bool m_already_failed;
  const char *m_inject_name;

 public:
  Inject_failure_once(const char *inject_name)
      : m_already_failed{false}, m_inject_name{inject_name} {}

  /**  If a certain SET GLOBAL DEBUG has been set and this is the first time
  this has been called for that injection point, write an entry to the
  error log and return true so that the caller can cause the failure.
  @return true iff compiled with debug and the debug point has been set
          and this it the first call for this debug point. */
  bool should_fail();
};

#endif /* UNIV_DEBUG */

/** Create the truncate log file. Needed to track the state of truncate during
a crash. An auxiliary redo log file undo_<space_id>_trunc.log will be created
while the truncate of the UNDO is in progress. This file is required during
recovery to complete the truncate.
@param[in]  undo_space  undo tablespace to truncate.
@return DB_SUCCESS or error code.*/
dberr_t start_logging(Tablespace *undo_space);

/** Mark completion of undo truncate action by writing magic number
to the log file and then removing it from the disk.
If we are going to remove it from disk then why write magic number?
This is to safeguard from unlink (file-system) anomalies that will
keep the link to the file even after unlink action is successful
and ref-count = 0.
@param[in]  space_num  number of the undo tablespace to truncate. */
void done_logging(space_id_t space_num);

/** Check if TRUNCATE_DDL_LOG file exist.
@param[in]  space_num  undo tablespace number
@return true if exist else false. */
bool is_active_truncate_log_present(space_id_t space_num);

/** list of undo tablespaces that need header pages and rollback
segments written to them at startup.  This can be because they are
newly initialized, were being truncated and the system crashed, or
they were an old format at startup and were replaced when they were
opened. Old format undo tablespaces do not have space_ids between
dict_sys_t::s_min_undo_space_id and dict_sys_t::s_max_undo_space_id
and they do not contain an RSEG_ARRAY page. */
extern Space_Ids s_under_construction;

/** Add undo tablespace to s_under_construction vector.
@param[in]      space_id        space id of tablespace to
truncate */
void add_space_to_construction_list(space_id_t space_id);

/** Clear the s_under_construction vector. */
void clear_construction_list();

/** Is an undo tablespace under construction at the moment.
@param[in]      space_id        space id to check
@return true if marked for truncate, else false. */
bool is_under_construction(space_id_t space_id);

/** Set an undo tablespace active. */
void set_active(space_id_t space_id);

/* Return whether the undo tablespace is active.  If this is a
non-undo tablespace, then it will not be found in spaces and it
will not be under construction, so this function will return true.
@param[in]  space_id   Undo Tablespace ID
@param[in]  get_latch  Specifies whether the rsegs->s_lock() is needed.
@return true if active (non-undo spaces are always active) */
bool is_active(space_id_t space_id, bool get_latch = true);

constexpr ulint TRUNCATE_FREQUENCY = 128;

/** Track an UNDO tablespace marked for truncate. */
class Truncate {
 public:
  /** Constructor. */
  Truncate() : m_space_id_marked(SPACE_UNKNOWN), m_timer() {}

  /** Destructor. */
  ~Truncate() = default;

  /** Is tablespace selected for truncate.
  @return true if undo tablespace is marked for truncate */
  bool is_marked() const { return (m_space_id_marked != SPACE_UNKNOWN); }

  /** Mark the undo tablespace selected for truncate as empty
  so that it will be truncated next. */
  void set_marked_space_empty() { m_marked_space_is_empty = true; }

  /** Is the tablespace selected for truncate empty of undo logs yet?
  @return true if the marked undo tablespace has no more undo logs */
  bool is_marked_space_empty() const { return (m_marked_space_is_empty); }

  /** Mark the tablespace for truncate.
  @param[in]  undo_space  undo tablespace to truncate. */
  void mark(Tablespace *undo_space);

  /** Get the ID of the tablespace marked for truncate.
  @return tablespace ID marked for truncate. */
  space_id_t get_marked_space_num() const {
    return (id2num(m_space_id_marked));
  }

  /** Reset for next rseg truncate. */
  void reset() {
    reset_timer();
    m_marked_space_is_empty = false;
    m_space_id_marked = SPACE_UNKNOWN;
  }

  /** Get the undo tablespace number to start a scan.
  Re-adjust in case the spaces::size() went down.
  @return undo space_num to start scanning. */
  space_id_t get_scan_space_num() const {
    s_scan_pos = s_scan_pos % undo::spaces->size();

    Tablespace *undo_space = undo::spaces->at(s_scan_pos);

    return (undo_space->num());
  }

  /** Increment the scanning position in a round-robin fashion.
  @return undo space_num at incremented scanning position. */
  space_id_t increment_scan() const {
    /** Round-robin way of selecting an undo tablespace for the truncate
    operation. Once we reach the end of the list of known undo tablespace
    IDs, move back to the first undo tablespace ID. This will scan active
    as well as inactive undo tablespaces. */
    s_scan_pos = (s_scan_pos + 1) % undo::spaces->size();

    return (get_scan_space_num());
  }

  /** Check if the given space id is equal to the space ID that is marked for
  truncation.
  @return true if they are equal, false otherwise. */
  bool is_equal(space_id_t space_id) const {
    return (m_space_id_marked == space_id);
  }

  /** @return the number of milliseconds since last reset. */
  int64_t check_timer() const { return (m_timer.elapsed()); }

  /** Reset the timer. */
  void reset_timer() { m_timer.reset(); }

 private:
  /** UNDO space ID that is marked for truncate. */
  space_id_t m_space_id_marked;

  /** This is true if the marked space is empty of undo logs and ready
  to truncate.  We leave the rsegs object 'inactive' until after it is
  truncated and rebuilt.  This allow the code to do the check for undo
  logs only once. */
  bool m_marked_space_is_empty;

  /** Elapsed time since last truncate check. */
  ib::Timer m_timer;

  /** Start scanning for UNDO tablespace from this vector position. This is
  to avoid bias selection of one tablespace always. */
  static size_t s_scan_pos;

}; /* class Truncate */

} /* namespace undo */

/** The control structure used in the purge operation */
struct trx_purge_t {
  /** System session running the purge query */
  sess_t *sess;

  /** System transaction running the purge query: this trx is not in the trx
  list of the trx system and it never ends */
  trx_t *trx;
#ifndef UNIV_HOTBACKUP
  /** The latch protecting the purge view. A purge operation must acquire an
  x-latch here for the instant at which it changes the purge view: an undo
  log operation can prevent this by obtaining an s-latch here. It also
  protects state and running */
  rw_lock_t latch;
#endif /* !UNIV_HOTBACKUP */

  /** State signal event */
  os_event_t event;

  /** Counter to track number stops */
  ulint n_stop;

  /** true, if purge is active, we check this without the latch too */
  volatile bool running;

  /** Purge coordinator thread states, we check this in several places without
  holding the latch. */
  volatile purge_state_t state;

  /** The query graph which will do the parallelized purge operation */
  que_t *query;

  /** The purge will not remove undo logs which are >= this view (purge view) */
  ReadView view;

  /** true if view is active */
  bool view_active;

  /** Count of total tasks submitted to the task queue */
  volatile ulint n_submitted;

  /** Count of total tasks completed */
  std::atomic<ulint> n_completed;

  /* The following two fields form the 'purge pointer' which advances
  during a purge, and which is used in history list truncation */

  /** Limit up to which we have read and parsed the UNDO log records.  Not
  necessarily purged from the indexes.  Note that this can never be less than
  the limit below, we check for this invariant in trx0purge.cc */
  purge_iter_t iter;

  /** The 'purge pointer' which advances during a purge, and which is used in
  history list truncation */
  purge_iter_t limit;
#ifdef UNIV_DEBUG
  /** Indicate 'purge pointer' which have purged already accurately. */
  purge_iter_t done;
#endif /* UNIV_DEBUG */

  /** true if the info of the next record to purge is stored below: if yes, then
  the transaction number and the undo number of the record are stored in
  purge_trx_no and purge_undo_no above */
  bool next_stored;

  /** Rollback segment for the next undo record to purge */
  trx_rseg_t *rseg;

  /** Page number for the next undo record to purge, page number of the log
  header, if dummy record */
  page_no_t page_no;

  /** Page offset for the next undo record to purge, 0 if the dummy record */
  ulint offset;

  /** Header page of the undo log where the next record to purge belongs */
  page_no_t hdr_page_no;

  /** Header byte offset on the page */
  ulint hdr_offset;

  /** Iterator to get the next rseg to process */
  TrxUndoRsegsIterator *rseg_iter;

  /** Binary min-heap, ordered on TrxUndoRsegs::trx_no. It is protected
  by the pq_mutex */
  purge_pq_t *purge_queue;

  /** Mutex protecting purge_queue */
  PQMutex pq_mutex;

  /** Track UNDO tablespace marked for truncate. */
  undo::Truncate undo_trunc;

  /** Heap for reading the undo log records */
  mem_heap_t *heap;

  /** Set of all THDs allocated by the purge system. */
  ut::unordered_set<THD *> thds;

  /** Set of all rseg queue. */
  std::vector<trx_rseg_t *> rsegs_queue;
};

/** Choose the rollback segment with the smallest trx_no. */
struct TrxUndoRsegsIterator {
  /** Constructor */
  TrxUndoRsegsIterator(trx_purge_t *purge_sys);

  /** Sets the next rseg to purge in m_purge_sys.
  @return page size of the table for which the log is.
  NOTE: if rseg is NULL when this function returns this means that
  there are no rollback segments to purge and then the returned page
  size object should not be used. */
  const page_size_t set_next();

 private:
  // Disable copying
  TrxUndoRsegsIterator(const TrxUndoRsegsIterator &);
  TrxUndoRsegsIterator &operator=(const TrxUndoRsegsIterator &);

  /** The purge system pointer */
  trx_purge_t *m_purge_sys;

  /** The current element to process */
  TrxUndoRsegs m_trx_undo_rsegs;

  /** Track the current element in m_trx_undo_rseg */
  typename Rsegs_array<2>::iterator m_iter;

  /** Sentinel value */
  static const TrxUndoRsegs NullElement;
};

#include "trx0purge.ic"

#endif /* trx0purge_h */
