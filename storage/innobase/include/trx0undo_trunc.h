/*****************************************************************************

Copyright (c) 2025, 2026, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/trx0undo_trunc.h
 Undo truncation handling */

/**
  @page PAGE_INNODB_UNDO_TRUNCATE Innodb UNDO Tablespace Truncate
  Module to handle UNDO tablespace truncation.

  @section undo_num_space_id Undo number vs undo space id

  @subsection undo_num Undo number
  There could be maximum of 127 UNDO tablespaces on a MySQL instance. Each undo
  tablespace is assigned an UNDO number in range from 1-127.

  @subsection undo_space_id Undo space id
  Undo tablespaces have reserved range of 400,000 (s_undo_space_id_range) space
  ids with Maximum space id as 0xFFFFFFEF (s_max_undo_space_id). Undo space ids
  are assigned in reverse from high to low. In other words, first Undo space
  gets max_id assigned and further Undo spaces get id assigned descending from
  this max.

  So
  - FSP_MAX_UNDO_TABLESPACES = 127
  - s_undo_space_id_range    = 400,000
  - s_max_undo_space_id      = 0xFFFFFFEF

  So the assignment of UNDO space id to UNDO num is as follows:
  <pre>
  Space ID   Space Num    Space ID   Space Num   ...  Space ID   Space Num
  0xFFFFFFEF      1       0xFFFFFFEe       2     ...  0xFFFFFF71    127
  0xFFFFFF70      1       0xFFFFFF6F       2     ...  0xFFFFFEF2    127
  0xFFFFFEF1      1       0xFFFFFEF0       2     ...  0xFFFFFE73    127
  </pre>
*/

#pragma once

#include "dict0dict.h"  //dict_sys_t::*
#include "fsp0fsp.h"    //fsp_*
#include "trx0sys.h"    //Space_ids

/* Namespace to hold all the related functions and variables needed
to truncate an undo tablespace. */
namespace undo_truncate {

// Forward declaration.
struct Tablespace;

/** Truncate Log file Prefix. */
static constexpr char s_log_prefix[] = "undo_";

/** Truncate Log file Extension. */
static constexpr char s_log_ext[] = "trunc.log";

/** Check if TRUNCATE_DDL_LOG file exist.
@param[in]  space_num  undo tablespace number
@return true if exist else false. */
[[nodiscard]] bool is_active_truncate_log_present(space_id_t space_num);

/** Remove TRUNCATE_DDL_LOG file if it exists
@param[in]  space_num  undo tablespace number */
void remove_truncate_log_file(space_id_t space_num);

/** Mutex for serializing undo tablespace related DDL.  These have to do with
creating and dropping undo tablespaces. */
extern ib_mutex_t ddl_mutex;

/** Build a standard undo tablespace name from a space_id.
This caller is responsible for freeing the returned value using ut::free
@param[in]      space_id        id of the undo tablespace.
@return tablespace name of the undo tablespace file */
[[nodiscard]] char *make_space_name(space_id_t space_id);

/** Build a standard undo tablespace file name from a space_id.
This will create a name like 'undo_001' if the space_id is in the
reserved range, else it will be like 'undo001'.
This caller is responsible for freeing the returned value using ut::free
@param[in]      space_id        id of the undo tablespace.
@return file_name of the undo tablespace file */
[[nodiscard]] char *make_file_name(space_id_t space_id);

/** Check if the space_id is an undo space ID in the reserved range.
@param[in]      space_id        undo tablespace ID
@return true if it is in the reserved undo space ID range. */
[[nodiscard]] inline bool is_reserved(space_id_t space_id) {
  return (space_id >= dict_sys_t::s_min_undo_space_id &&
          space_id <= dict_sys_t::s_max_undo_space_id);
}

/** Convert an undo space number (from 1 to 127) into the undo space_id,
given an index indicating which space_id from the pool assigned to that
undo number.
@param[in]  space_num  undo tablespace number
@param[in]  ndx        index of the space_id within that undo number
@return space_id of the undo tablespace */
[[nodiscard]] space_id_t num2id(space_id_t space_num, size_t ndx);

/** Convert an undo space number (from 1 to 127) into an undo space_id.
Use the undo_truncate::space_id_bank to return the current space_id assigned to
that undo number.
@param[in]  space_num   undo tablespace number
@return space_id of the undo tablespace */
[[nodiscard]] space_id_t num2id(space_id_t space_num);

/** Get the corresponding UNDO space number for a given UNDO space id
@param[in]      space_id        undo tablespace ID
@return space number of the undo tablespace */
[[nodiscard]] space_id_t id2num(space_id_t space_id);

/* Given a reserved undo space_id, return the next space_id for the associated
undo space number. */
[[nodiscard]] space_id_t id2next_id(space_id_t space_id);
/** Map from undo tablespace number to space id to allow a reserved undo space
ID to be found quickly. */
class Undo_num2id_map {
 public:
  /** Constructor which will create the mapping from UNDO space number to UNDO
  space id from the given space_id list.
  @param[in] space_ids list of space ids */
  Undo_num2id_map(const std::vector<space_id_t> &space_ids) {
    for (auto &id : space_ids) {
      if (fsp_is_undo_tablespace(id)) {
        const auto num = id2num(id);
        ut_a(m_num2id.count(num) == 0);
        m_num2id[num] = id;
      }
    }
  }

  /** Find the UNDO space_id for a given UNDO space number from the mapping.
  @param[in]  num  Undo space number
  @return UNDO space_id */
  [[nodiscard]] space_id_t get(space_id_t num) {
    ut_a(m_num2id.count(num) != 0);

    return m_num2id[num];
  }

  /** check if mapping contains an entry for given UNDO space number.
  @param[in]  num  UNDO space number
  @return True if entry is there, false otherwise */
  [[nodiscard]] bool contains(space_id_t num) {
    return (m_num2id.count(num) != 0);
  }

 private:
  /** Map from undo space_num to undo space_id */
  std::unordered_map<space_id_t, space_id_t> m_num2id{};
};

/* A pointer to an instance of Undo_num2id_map. It is initialized after we get
the space_ids from tablespace directory scan and is released after bootstrap.
It is used only during bootstrap when UNDO tablespaces are being opened post
REDO recovery. */
extern ut::unique_ptr<Undo_num2id_map> num2id_map;

/** An undo_truncate::Tablespace object is used to easily convert between
undo_space_id and undo_space_num and to create the automatic file_name
and space name.  In addition, it is used in undo_truncate::Tablespaces to track
the trx_rseg_t objects in an Rsegs vector. So we do not allocate the
Rsegs vector for each object, only when requested by the constructor. */
struct Tablespace {
  /** Constructor
  @param[in]  id    tablespace id */
  explicit Tablespace(space_id_t id)
      : m_id(id),
        m_new(false),
        m_space_name(),
        m_file_name(),
        m_log_file_name(),
        m_rsegs(),
        truncate_in_progress(false) {}

  /** Copy Constructor
  @param[in]  other    undo tablespace to copy */
  Tablespace(Tablespace &other)
      : m_id(other.id()),
        m_new(other.is_new()),
        m_space_name(),
        m_file_name(),
        m_log_file_name(),
        m_rsegs() {
    ut_ad(m_id == 0 || is_reserved(m_id));

    set_space_name(other.space_name());
    set_file_name(other.file_name());

    /* When the copy constructor is used, add an Rsegs
    vector. This constructor is only used in the global
    undo_truncate::Tablespaces object where rollback segments are
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

    /* Clear the cached rollback segments.  */
    if (m_rsegs != nullptr) {
      ut::delete_(m_rsegs);
      m_rsegs = nullptr;
    }
  }

  /* Determine if this undo space needs to be truncated.
  @return true if it should be truncated, false if not. */
  [[nodiscard]] bool needs_truncation();

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
  like undo_truncate::spaces, then it must be protected by the caller.
  @return tablespace name created from the space_id */
  [[nodiscard]] char *space_name() {
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
  like undo_truncate::spaces, then it must be protected by the caller.
  @return tablespace filename created from the space_id */
  [[nodiscard]] char *file_name() {
    if (m_file_name == nullptr) {
      m_file_name = make_file_name(m_id);
    }

    return (m_file_name);
  }

  /** Build a log file name based on space_id
  @param[in]  space_id  id of the undo tablespace.
  @param[in]  location  directory location of the file.
  @return DB_SUCCESS or error code */
  [[nodiscard]] char *make_log_file_name(space_id_t space_id,
                                         const char *location);

  /** Get the undo log filename. Make it if not yet made.
  NOTE: This is only called from stack objects so there is no
  race condition. If it is ever called from a shared object
  like undo_truncate::spaces, then it must be protected by the caller.
  @return tablespace filename created from the space_id */
  [[nodiscard]] char *log_file_name() {
    if (m_log_file_name == nullptr) {
      m_log_file_name = make_log_file_name(m_id, srv_undo_dir);
    }

    return (m_log_file_name);
  }

  /** Get the undo tablespace ID.
  @return tablespace ID */
  [[nodiscard]] space_id_t id() { return (m_id); }

  /** Get the undo tablespace number.
  This is the same as m_id if m_id is 0.
  @return undo tablespace number */
  [[nodiscard]] space_id_t num() {
    const auto n = id2num(m_id);
    ut_ad(n <= FSP_MAX_UNDO_TABLESPACES);
    return n;
  }

  /** Get a reference to the List of rollback segments within
  this undo tablespace.
  @return a reference to the Rsegs vector. */
  [[nodiscard]] Rsegs *rsegs() { return (m_rsegs); }

  /** Report whether this undo tablespace was explicitly created
  by an SQL statement.
  @return true if the tablespace was created explicitly. */
  [[nodiscard]] bool is_explicit() {
    return num() > FSP_IMPLICIT_UNDO_TABLESPACES;
  }

  /** Report whether this undo tablespace was created at startup.
  @retval true if created at startup.
  @retval false if pre-existed at startup. */
  [[nodiscard]] bool is_new() { return (m_new); }

  /** Note that this undo tablespace is being created. */
  void set_new() { m_new = true; }

  /** Return whether the undo tablespace is active.
  @return true if active */
  [[nodiscard]] bool is_active() {
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
  [[nodiscard]] bool is_active_no_latch() {
    if (m_rsegs == nullptr) {
      return (false);
    }
    return (m_rsegs->is_active());
  }

  /** Return the rseg at the requested rseg slot if the undo space is active.
  @param[in] slot   The slot of the rseg.  1 to 127
  @return Rseg pointer of nullptr if the space is not active. */
  [[nodiscard]] trx_rseg_t *get_active(ulint slot) {
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
  [[nodiscard]] bool is_inactive_implicit() {
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
  [[nodiscard]] bool is_inactive_explicit() {
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
  [[nodiscard]] bool is_empty() {
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

  /** List of rollback segments within this tablespace.
  This is not always used. Must call init_rsegs to use it. */
  Rsegs *m_rsegs;

 public:
  /** True if truncation of this undo tablespace is in progress. */
  bool truncate_in_progress;
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

  /** Add a new undo_truncate::Tablespace to the back of the vector.
  The vector has been pre-allocated to 128 so read threads will
  not lose what is pointed to. If tablespace_name and file_name
  are standard names, they are optional.
  @param[in]    ref_undo_space  undo tablespace */
  void add(Tablespace &ref_undo_space);

  /** Drop an existing explicit undo_truncate::Tablespace.
  @param[in]    undo_space      pointer to undo space */
  void drop(Tablespace *undo_space);

  /** Drop an existing explicit undo_truncate::Tablespace.
  @param[in]    ref_undo_space  reference to undo space */
  void drop(Tablespace &ref_undo_space);

  /** Check if the given space_id is in the vector.
  @param[in]  num  undo tablespace number
  @return true if space_id is found, else false */
  bool contains(space_id_t num) { return (find(num) != nullptr); }

  /** Find the given space_num in the vector.
  @param[in]  num  undo tablespace number
  @return pointer to an undo_truncate::Tablespace struct */
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
  @return pointer to an undo_truncate::Tablespace struct */
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
  void s_lock(ut::Location location) { rw_lock_s_lock(m_latch, location); }

  /** Release a shared lock on m_spaces. */
  void s_unlock() { rw_lock_s_unlock(m_latch); }

  /** Get an exclusive lock on m_spaces. */
  void x_lock(ut::Location location) { rw_lock_x_lock(m_latch, location); }

  /** Release an exclusive lock on m_spaces. */
  void x_unlock() { rw_lock_x_unlock(m_latch); }

  Tablespaces_Vector m_spaces;

 private:
  /** RW lock to protect m_spaces.
  x for adding elements, s for scanning, size() etc. */
  rw_lock_t *m_latch;
};

/** A global object that contains a vector of undo_truncate::Tablespace structs.
 */
extern Tablespaces *spaces;

/** Set an undo tablespace active. */
void set_active(space_id_t space_id);

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
  [[nodiscard]] bool is_marked() const {
    return (m_space_id_marked != SPACE_UNKNOWN);
  }

  /** Mark the undo tablespace selected for truncate as empty
  so that it will be truncated next. */
  void set_marked_space_empty() { m_marked_space_is_empty = true; }

  /** Is the tablespace selected for truncate empty of undo logs yet?
  @return true if the marked undo tablespace has no more undo logs */
  [[nodiscard]] bool is_marked_space_empty() const {
    return (m_marked_space_is_empty);
  }

  /** Mark the tablespace for truncate.
  @param[in]  undo_space  undo tablespace to truncate. */
  void mark(Tablespace *undo_space);

  /** Get the number of the tablespace marked for truncate.
  @return tablespace number marked for truncate. */
  [[nodiscard]] space_id_t get_marked_space_num() const {
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
  [[nodiscard]] space_id_t get_scan_space_num() const {
    s_scan_pos = s_scan_pos % spaces->size();

    Tablespace *undo_space = spaces->at(s_scan_pos);

    return (undo_space->num());
  }

  /** Increment the scanning position in a round-robin fashion.
  @return undo space_num at incremented scanning position. */
  [[nodiscard]] space_id_t increment_scan() const {
    /** Round-robin way of selecting an undo tablespace for the truncate
    operation. Once we reach the end of the list of known undo tablespace
    IDs, move back to the first undo tablespace ID. This will scan active
    as well as inactive undo tablespaces. */
    s_scan_pos = (s_scan_pos + 1) % undo_truncate::spaces->size();

    return (get_scan_space_num());
  }

  /** Check if the given space id is equal to the space ID that is marked for
  truncation.
  @return true if they are equal, false otherwise. */
  [[nodiscard]] bool is_equal(space_id_t space_id) const {
    return (m_space_id_marked == space_id);
  }

  /** @return the number of milliseconds since last reset. */
  [[nodiscard]] int64_t check_timer() const { return (m_timer.elapsed()); }

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

/* Exposed functions */

/** Note that the undo space number for a space ID is being used.
Put that space_id into the space_id_bank.
@param[in] space_id  undo tablespace number */
void use_space_id(space_id_t space_id);

/** Mark that the given undo space number is being used and
return the next available space_id for that space number.
@param[in]  space_num  undo tablespace number
@return the next tablespace ID to use */
[[nodiscard]] space_id_t use_next_space_id(space_id_t space_num);

/** Given a valid undo space_id, return the next space_id for that
space number.
@param[in]  space_id  undo tablespace ID
@return the next tablespace ID to use */
[[nodiscard]] space_id_t next_space_id(space_id_t space_id);

/** Given a valid undo space_id or SPACE_UNKNOWN, return the next space_id
for the given space number.
@param[in]  space_id   undo tablespace ID
@param[in]  space_num  undo tablespace number
@return the next tablespace ID to use */
[[nodiscard]] space_id_t next_space_id(space_id_t space_id,
                                       space_id_t space_num);

/** Mark an undo number associated with a given space_id as unused and
available to be reused.  This happens when the fil_space_t is closed
associated with a drop undo tablespace.
@param[in] space_id  Undo Tablespace ID */
void unuse_space_id(space_id_t space_id);

/** Return the next available undo space ID to be used for a new explicit
undo tablespaces. The slot will be marked as in-use.
@return next available undo space number if successful.
@return SPACE_UNKNOWN if failed */
[[nodiscard]] space_id_t get_next_available_space_id();

/** Set the FSP_FLAGS_MASK_UNDO_UNUSABLE flag in the undo tablespace
header to indicate that the undo file is not usable. This is mainly
done during the truncate operation of undo tablespace.
@param[in]  space_id  undo tablespace id
@param[in]  mtr       Mini-transaction */
void mark_undo_tablespace_unusable(space_id_t space_id, mtr_t *mtr);

/** Unset the FSP_FLAGS_MASK_UNDO_UNUSABLE in the undo tablespace
header to indicate that the undo file is now usable.
@param[in]  space_id   undo tablespace id
@param[in]  mtr        Mini-transaction */
void mark_undo_tablespace_usable(space_id_t space_id, mtr_t *mtr);

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
  explicit Inject_failure_once(const char *inject_name)
      : m_already_failed{false}, m_inject_name{inject_name} {}

  /**  If a certain SET GLOBAL DEBUG has been set and this is the first time
  this has been called for that injection point, write an entry to the
  error log and return true so that the caller can cause the failure.
  @return true iff compiled with debug and the debug point has been set
          and this it the first call for this debug point. */
  [[nodiscard]] bool should_fail();
};
#endif /* UNIV_DEBUG */

}  // namespace undo_truncate
