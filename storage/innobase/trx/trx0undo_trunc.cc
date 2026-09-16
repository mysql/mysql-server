/*****************************************************************************

Copyright (c) 2023, 2026, Oracle and/or its affiliates.

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

/** @file trx/trx0undo_trunc.cc
 Undo truncation handling */

#include "trx0undo_trunc.h"
#include "fil0fil.h"  //fil_*
#include "fil0tablespace_scan.h"
#include "log0helpers.h"
#include "srv0srv.h"    //srv_undo_dir
#include "trx0purge.h"  //purge_sys
#include "univ.i"       //space_id_t
#include "ut0new.h"

namespace undo_truncate {

ib_mutex_t ddl_mutex;

ut::unique_ptr<Undo_num2id_map> num2id_map;

Tablespaces *spaces;

/*===================== Space_id_bank ========================= */
/** A bank of UNDO space ids which is a lock free repository for information
about the space IDs used for undo tablespaces. It is used during creation in
order to assign an unused space number and during truncation in order to
assign the next space_id within that space_number range.

It is initialized with the minimum value in the range so that if a new
space ID is needed in that range the max space ID will be used first.
As truncation occurs, the space_ids are assigned from max down to min. */
class Space_id_bank {
  /** The currently used undo space IDs for an undo space along with a boolean
  showing whether the undo space number is in use. */
  struct Space_id_account {
    space_id_t space_id{SPACE_UNKNOWN};
    bool in_use{false};
  };

 public:
  /* Constructor */
  Space_id_bank() {}

  ~Space_id_bank() {}

  /** Note that the undo space number for a space ID is being used.
  Put that space_id into the space_id_bank.
  @param[in] space_id  undo tablespace number */
  void use_space_id(space_id_t space_id);

  /** Mark an undo number associated with a given space_id as unused and
  available to be reused.  This happens when the fil_space_t is closed
  associated with a drop undo tablespace.
  @param[in] space_id  Undo Tablespace ID */
  void unuse_space_id(space_id_t space_id);

  /** Mark that the given undo space number is being used and
  return the next available space_id for that space number.
  @param[in]  space_num  undo tablespace number
  @return the next tablespace ID to use */
  [[nodiscard]] space_id_t use_next_space_id(space_id_t space_num);

  /** Given a valid undo space_id or SPACE_UNKNOWN, return the next space_id
  for the given space number.
  @param[in]  space_id   undo tablespace ID
  @param[in]  space_num  undo tablespace number
  @return the next tablespace ID to use */
  [[nodiscard]] space_id_t next_space_id(space_id_t space_id,
                                         space_id_t space_num);

  /** Given a valid undo space_id, return the next space_id for that
  space number.
  @param[in]  space_id  undo tablespace ID
  @return the next tablespace ID to use */
  [[nodiscard]] space_id_t next_space_id(space_id_t space_id);

  /** Return the next available undo space ID to be used for a new explicit
  undo tablespaces. The slot will be marked as in-use.
  @return next available undo space number if successful.
  @return SPACE_UNKNOWN if failed */
  [[nodiscard]] space_id_t get_next_available_space_id();

  /** Check if the space_id mapped to space_num is in use
  @param[in]  space_num  undo tablespace number
  @return true if in use */
  [[nodiscard]] bool check_space_id_in_use(space_id_t space_num) {
    return m_accounts[space_num].in_use;
  }

  /** Return the current space id mapped to the space_num
  @param[in]  space_num  undo tablespace number
  @return the tablespace ID mapped to space_num  */
  [[nodiscard]] space_id_t get_current_space_id_for_num(space_id_t space_num) {
    return m_accounts[space_num].space_id;
  }

 private:
  /* Space ids accounts */
  Space_id_account m_accounts[FSP_MAX_UNDO_TABLESPACES];
};
static Space_id_bank space_id_bank;

void Space_id_bank::use_space_id(space_id_t space_id) {
  size_t slot = id2num(space_id) - 1;

  ut_a(!m_accounts[slot].in_use);
  m_accounts[slot].space_id = space_id;
  m_accounts[slot].in_use = true;
}

void Space_id_bank::unuse_space_id(space_id_t space_id) {
  ut_ad(fsp_is_undo_tablespace(space_id));

  space_id_t space_num = id2num(space_id);
  size_t slot = space_num - 1;

  m_accounts[slot].in_use = false;
}

space_id_t Space_id_bank::next_space_id(space_id_t space_id,
                                        space_id_t space_num) {
  ut_ad(space_id == SPACE_UNKNOWN || fsp_is_undo_tablespace(space_id));
  ut_ad(space_id != SPACE_UNKNOWN ||
        (space_num > 0 && space_num <= FSP_MAX_UNDO_TABLESPACES));

  space_id_t first_id = dict_sys_t::s_max_undo_space_id + 1 - space_num;
  space_id_t last_id = first_id - (FSP_MAX_UNDO_TABLESPACES *
                                   (dict_sys_t::s_undo_space_id_range - 1));
  return (space_id == SPACE_UNKNOWN || space_id == last_id
              ? first_id
              : space_id - FSP_MAX_UNDO_TABLESPACES);
}

space_id_t Space_id_bank::next_space_id(space_id_t space_id) {
  ut_ad(space_id != SPACE_UNKNOWN);
  ut_ad(fsp_is_undo_tablespace(space_id));

  space_id_t space_num = id2num(space_id);

  return (next_space_id(space_id, space_num));
}

space_id_t Space_id_bank::use_next_space_id(space_id_t space_num) {
  const size_t slot = space_num - 1;

  ut_ad(!m_accounts[slot].in_use);

  const auto cur_id = m_accounts[slot].space_id;
  const auto next_id = next_space_id(cur_id, space_num);

  m_accounts[slot].space_id = next_id;
  m_accounts[slot].in_use = true;

  return next_id;
}

space_id_t Space_id_bank::get_next_available_space_id() {
  for (space_id_t slot = FSP_IMPLICIT_UNDO_TABLESPACES;
       slot < FSP_MAX_UNDO_TABLESPACES; ++slot) {
    space_id_t space_num = slot + 1;

    if (!m_accounts[slot].in_use) {
      return use_next_space_id(space_num);
    }
    /* Slot is in use.  Try the next slot. */
  }

  return SPACE_UNKNOWN;
}

void use_space_id(space_id_t space_id) { space_id_bank.use_space_id(space_id); }

space_id_t get_next_available_space_id() {
  return space_id_bank.get_next_available_space_id();
}

void unuse_space_id(space_id_t space_id) {
  space_id_bank.unuse_space_id(space_id);
}

space_id_t next_space_id(space_id_t space_id) {
  return space_id_bank.next_space_id(space_id);
}

space_id_t use_next_space_id(space_id_t space_num) {
  return space_id_bank.use_next_space_id(space_num);
}

/*===================== Tablespace ========================= */
bool Tablespace::needs_truncation() {
  /* If it is already inactive, even implicitly, then proceed. */
  m_rsegs->s_lock();
  if (m_rsegs->is_inactive_implicit() || m_rsegs->is_inactive_explicit()) {
    m_rsegs->s_unlock();
    return true;
  }

  /* If implicit undo truncation is turned off, or if the rsegs don't exist
  yet, don't bother checking the size. */
  if (!srv_undo_log_truncate || m_rsegs == nullptr || m_rsegs->is_empty() ||
      m_rsegs->is_init()) {
    m_rsegs->s_unlock();
    return false;
  }
  ut_ad(m_rsegs->is_active());
  m_rsegs->s_unlock();

  /* Check if undo truncation is happening so often that too many pages
  from old space IDs are still in memory. Since undo spaces are deleted
  with BUF_REMOVE_NONE, the actual space is not deleted for that old
  space ID until all pages have been passively removed from the buffer
  pool. */
  auto count = fil_count_undo_deleted(id2num(m_id));
  if (count > CONCURRENT_UNDO_TRUNCATE_LIMIT) {
    ib::warn(ER_IB_MSG_UNDO_TRUNCATE_TOO_OFTEN);
    return false;
  }

  ut_ad(fil_space_get_undo_initial_size(m_id) != 0);

  page_no_t trunc_size = std::max(
      static_cast<page_no_t>(srv_max_undo_tablespace_size / srv_page_size),
      fil_space_get_undo_initial_size(m_id));

  if (fil_space_get_size(m_id) > trunc_size) {
    return true;
  }

  return false;
}

void Tablespace::set_space_id(space_id_t space_id) {
  ut_ad_eq(num(), id2num(space_id));
  m_id = space_id;
}

void Tablespace::set_space_name(const char *new_space_name) {
  if (m_space_name != nullptr) {
    ut::free(m_space_name);
    m_space_name = nullptr;
  }

  size_t size = strlen(new_space_name) + 1;
  m_space_name =
      static_cast<char *>(ut::malloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, size));

  strncpy(m_space_name, new_space_name, size);
}

void Tablespace::set_file_name(const char *file_name) {
  /* Make a copy of the filename and normalize it. */
  char norm_fn[FN_REFLEN];
  strncpy(norm_fn, file_name, FN_REFLEN - 1);
  Fil_path::normalize(norm_fn);
  std::string tmp_fn{norm_fn};

  /* This name can come in three forms: absolute path, relative path,
  and basename. ADD DATAFILE for undo tablespaces does not accept a
  relative path. If a relative path comes in here, it was the scanned
  name and is relative to the datadir. So only prepend the undo_dir if
  this is just a basename. */
  std::string final_fn;
  if (tmp_fn.find_first_of(":/\\") == std::string::npos) {
    /* Prepend the undo directory. */
    bool is_circ = MySQL_undo_path.is_circular();
    final_fn += (is_circ ? MySQL_undo_path.abs_path() : MySQL_undo_path.path());
    char back = (is_circ ? MySQL_undo_path.abs_path().back()
                         : MySQL_undo_path.path().back());
    final_fn += (back == OS_PATH_SEPARATOR ? "" : OS_PATH_SEPARATOR_STR);
  }
  final_fn += tmp_fn;

  /* We are going to replace any existing m_file_name. */
  if (m_file_name != nullptr) {
    ut::free(m_file_name);
  }

  size_t len = final_fn.size();
  m_file_name = static_cast<char *>(
      ut::malloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, len + 1));
  memcpy(m_file_name, final_fn.c_str(), len);
  m_file_name[len] = '\0';
}

char *Tablespace::make_log_file_name(space_id_t space_id,
                                     const char *location) {
  size_t size = strlen(location) + 22 + 1 /* NUL */
                + strlen(s_log_prefix) + strlen(s_log_ext);

  char *name =
      static_cast<char *>(ut::malloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, size));

  memset(name, 0, size);

  strcpy(name, location);
  ulint len = strlen(name);

  if (name[len - 1] != OS_PATH_SEPARATOR) {
    name[len] = OS_PATH_SEPARATOR;
    len = strlen(name);
  }

  snprintf(name + len, size - len, "%s%lu_%s", s_log_prefix,
           (ulong)id2num(space_id), s_log_ext);

  return name;
}

void Tablespace::alter_active() {
  m_rsegs->x_lock();
  ut_d(ib::info(ER_IB_MSG_UNDO_ALTERED_ACTIVE, file_name()));
  if (m_rsegs->is_empty()) {
    m_rsegs->set_active();
  } else if (m_rsegs->is_inactive_explicit()) {
    if (purge_sys->undo_trunc.is_marked() &&
        purge_sys->undo_trunc.get_marked_space_num() == num()) {
      m_rsegs->set_inactive_implicit();
    } else {
      m_rsegs->set_active();
    }
  }
  m_rsegs->x_unlock();
}

/*======== Truncate operations helpers functions ========== */

/* These functions are still kept to support upgrade from a version which used
the truncate log functionality in case it crashed during truncate or CREATE
UNDO TABLESPACE */

bool is_active_truncate_log_present(space_id_t space_num) {
  /* Get the first space id for thus space_num. That is good enough since we
  only need the log_file_name. */
  Tablespace undo_space(num2id(space_num, 0));
  return os_file_exists(undo_space.log_file_name());
}

void remove_truncate_log_file(space_id_t space_num) {
  ut_a(tablespace_scanning);

  Tablespace undo_space(num2id(space_num, 0));

  /* The truncation log file location changed to a new default location.
  Check if it exists in either location. */
  char *log_file_name = undo_space.log_file_name();
  if (log_file_name && os_file_exists(log_file_name)) {
    os_file_delete_if_exists(innodb_log_file_key, log_file_name, nullptr);
  }
}

/*===================== Global Functions ========================= */

void set_active(space_id_t space_id) {
  ut_ad(spaces != nullptr);
  ut_ad(fsp_is_undo_tablespace(space_id));

  spaces->s_lock(UT_LOCATION_HERE);
  Tablespace *undo_space = spaces->find(id2num(space_id));

  if (undo_space != nullptr) {
    undo_space->set_active();
  }
  spaces->s_unlock();
}

char *make_space_name(space_id_t space_id) {
  /* 8.0 undo tablespace names have an extra '_' */
  bool old = (id2num(space_id) == space_id);

  size_t size = sizeof(undo_space_name) + 3 + (old ? 0 : 1);

  char *name =
      static_cast<char *>(ut::malloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, size));

  snprintf(name, size, (old ? "%s%03" SPACE_ID_PFS : "%s_%03" SPACE_ID_PFS),
           undo_space_name, static_cast<unsigned>(id2num(space_id)));

  return name;
}

char *make_file_name(space_id_t space_id) {
  /* 8.0 undo tablespace names have an extra '_' */
  size_t len = strlen(srv_undo_dir);
  bool with_sep = (srv_undo_dir[len - 1] == OS_PATH_SEPARATOR);
  bool old = (id2num(space_id) == space_id);

  size_t size = strlen(srv_undo_dir) + (with_sep ? 0 : 1) + sizeof("undo000") +
                (old ? 0 : 1);

  char *name =
      static_cast<char *>(ut::malloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, size));

  memcpy(name, srv_undo_dir, len);

  if (!with_sep) {
    name[len++] = OS_PATH_SEPARATOR;
  }

  memcpy(&name[len], "undo", 4);
  len += 4;

  if (!old) {
    name[len++] = '_';
  }

  snprintf(&name[len], size - len, "%03" SPACE_ID_PFS,
           static_cast<unsigned>(id2num(space_id)));

  return name;
}

space_id_t num2id(space_id_t space_num, size_t ndx) {
  ut_ad(space_num > 0);
  ut_ad(space_num <= FSP_MAX_UNDO_TABLESPACES);
  ut_ad(ndx < dict_sys_t::s_undo_space_id_range);

  space_id_t space_id = dict_sys_t::s_max_undo_space_id + 1 - space_num -
                        static_cast<space_id_t>(ndx * FSP_MAX_UNDO_TABLESPACES);

  return space_id;
}

space_id_t num2id(space_id_t space_num) {
  ut_ad(space_num > 0);
  ut_ad(space_num <= FSP_MAX_UNDO_TABLESPACES);

  size_t slot = space_num - 1;

  /* The space_id_back is normally protected by spaces::m_latch.
  But this can only be called on a specific slot when truncation is not
  happening on that slot, i.e. the undo tablespace is in use. */
  ut_ad(space_id_bank.check_space_id_in_use(slot));

  return space_id_bank.get_current_space_id_for_num(slot);
}

space_id_t id2num(space_id_t space_id) {
  if (!is_reserved(space_id)) {
    ut_ad_eq(space_id, 0);
    return space_id;
  }

  return (((dict_sys_t::s_max_undo_space_id - space_id) %
           FSP_MAX_UNDO_TABLESPACES) +
          1);
}

space_id_t id2next_id(space_id_t space_id) {
  ut_ad(is_reserved(space_id));

  space_id_t space_num = id2num(space_id);
  space_id_t first_id = dict_sys_t::s_max_undo_space_id + 1 - space_num;
  space_id_t last_id = first_id - (FSP_MAX_UNDO_TABLESPACES *
                                   (dict_sys_t::s_undo_space_id_range - 1));

  return (space_id == SPACE_UNKNOWN || space_id == last_id
              ? first_id
              : space_id - FSP_MAX_UNDO_TABLESPACES);
}

void mark_undo_tablespace_unusable(space_id_t space_id, mtr_t *mtr) {
  /* Set the flag in UNDO tablespace header to indicate truncate or create is
     in progress. */

  ut_ad(fsp_is_undo_tablespace(space_id));
  const auto space = fil_space_get(space_id);
  ut_a(space != nullptr);

  fsp_flags_set_undo_unusable(space->flags);
  fsp_header_store_flags(space_id, space->flags, mtr);
}

void mark_undo_tablespace_usable(space_id_t space_id, mtr_t *mtr) {
  const auto space = fil_space_get(space_id);
  ut_a(space != nullptr);

  fsp_flags_unset_undo_unusable(space->flags);
  fsp_header_store_flags(space_id, space->flags, mtr);
}

/*===================== Crash Injection ========================= */

#ifdef UNIV_DEBUG
void inject_crash(const char *injection_point_name) {
  DBUG_EXECUTE_IF(injection_point_name,
                  ib::info(ER_IB_MSG_INJECT_CRASH, injection_point_name);
                  ib::redo::must_persist_all(UT_LOCATION_HERE);
                  DBUG_SUICIDE(););
}

bool Inject_failure_once::should_fail() {
  DBUG_EXECUTE_IF(m_inject_name, {
    if (!m_already_failed) {
      m_already_failed = true;
      ib::info(ER_IB_MSG_INJECT_FAILURE, m_inject_name);
      return true;
    }
  });
  return false;
}

#endif /* UNIV_DEBUG */

}  // namespace undo_truncate
