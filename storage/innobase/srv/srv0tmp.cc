/*****************************************************************************

Copyright (c) 2018, Oracle and/or its affiliates. All Rights Reserved.

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

#include "srv0tmp.h"
#include <algorithm>
#include "dict0dict.h"
#include "ib0mutex.h"
#include "srv0srv.h"
#include "srv0start.h"

namespace ibt {

/** The initial size of temporary tablespace pool */
const uint32_t INIT_SIZE = 10;

/** The number of tablespaces added to the pool every time the pool is expanded
 */
const uint32_t POOL_EXPAND_SIZE = 10;

/** Thread id for the replication thread */
const uint32_t SLAVE_THREAD_ID = UINT32_MAX;

/** Directory to store the session temporary tablespaces.
Used when user doesn't provide a temporary tablespace dir */
static const char DIR_NAME[] = "#innodb_temp";

/** Filename prefix to identify the session temporary tablespaces.
They are of pattern temp_*.ibt */
static const char PREFIX_NAME[] = "temp_";

/** Directory name where session temporary tablespaces are stored.
This location is decided after consulting srv_temp_dir */
static std::string temp_tbsp_dir;

/** Tablespace to be used by the replication thread */
static Tablespace *rpl_slave_tblsp = nullptr;

Tablespace_pool *tbsp_pool = nullptr;

/* Directory to store session temporary tablespaces, provided by user */
char *srv_temp_dir = nullptr;

/** Sesssion Temporary tablespace */
Tablespace::Tablespace()
    : m_space_id(++m_last_used_space_id), m_inited(), m_thread_id() {
  ut_ad(m_space_id <= dict_sys_t::s_max_temp_space_id);
  m_purpose = TBSP_NONE;
}

Tablespace::~Tablespace() {
  if (!m_inited) {
    return;
  }

  close();

  ut_ad(srv_shutdown_state == SRV_SHUTDOWN_EXIT_THREADS);
  bool file_pre_exists = false;
  bool success = os_file_delete_if_exists(innodb_temp_file_key, path().c_str(),
                                          &file_pre_exists);

  if (file_pre_exists && !success) {
    ib::error() << "Failed to delete file " << path();
    os_file_get_last_error(true);
    ut_ad(0);
  }
}

dberr_t Tablespace::create() {
  ut_ad(m_space_id > dict_sys_t::s_min_temp_space_id);

  /* Create the filespace flags */
  ulint fsp_flags =
      fsp_flags_init(univ_page_size, /* page sizes and a flag if compressed */
                     false,          /* needed only for compressed tables */
                     false,          /* has DATA_DIR */
                     true,           /* is shared */
                     true);          /* is temporary */

  dberr_t err = fil_ibt_create(m_space_id, file_name().c_str(), path().c_str(),
                               fsp_flags, FIL_IBT_FILE_INITIAL_SIZE);

  if (err != DB_SUCCESS) {
    return (err);
  }

  m_inited = true;

  mtr_t mtr;
  mtr_start(&mtr);

  mtr_set_log_mode(&mtr, MTR_LOG_NO_REDO);

  bool ret =
      fsp_header_init(m_space_id, FIL_IBT_FILE_INITIAL_SIZE, &mtr, false);
  mtr_commit(&mtr);

  if (!ret) {
    return (DB_ERROR);
  }
  buf_LRU_flush_or_remove_pages(m_space_id, BUF_REMOVE_FLUSH_WRITE, NULL);
  return (DB_SUCCESS);
}

bool Tablespace::close() const {
  if (!m_inited) {
    return (false);
  }

  fil_space_close(m_space_id);
  return (true);
}

bool Tablespace::truncate() {
  if (!m_inited) {
    return (false);
  }

  bool success = fil_truncate_tablespace(m_space_id, FIL_IBT_FILE_INITIAL_SIZE);
  if (!success) {
    return (success);
  }
  mtr_t mtr;
  mtr_start(&mtr);
  mtr_set_log_mode(&mtr, MTR_LOG_NO_REDO);
  fsp_header_init(m_space_id, FIL_IBT_FILE_INITIAL_SIZE, &mtr, false);
  mtr_commit(&mtr);
  return (true);
}

uint32_t Tablespace::file_id() const {
  return (m_space_id - dict_sys_t::s_min_temp_space_id);
}

std::string Tablespace::file_name() const {
  std::string str(PREFIX_NAME);
  str += std::to_string(file_id());
  return (str);
}

std::string Tablespace::path() const {
  std::string str(temp_tbsp_dir);
  str.append(file_name());
  str.append(DOT_IBT);
  return (str);
}

/** Space_ids for Session temporary tablespace. The available range is
from dict_sys_t::s_min_temp_space_id to dict_sys_t::s_max_temp_space_id.
Total 400K space_ids are reserved for session temporary tablespaces. */
space_id_t Tablespace::m_last_used_space_id = dict_sys_t::s_min_temp_space_id;

Tablespace_pool::Tablespace_pool(size_t init_size)
    : m_pool_initialized(),
      m_init_size(init_size),
      m_free(nullptr),
      m_active(nullptr) {
  mutex_create(LATCH_ID_TEMP_POOL_MANAGER, &m_mutex);
}

Tablespace_pool::~Tablespace_pool() {
  mutex_destroy(&m_mutex);
  for (Tablespace *ts : *m_active) {
    UT_DELETE(ts);
  }

  for (Tablespace *ts : *m_free) {
    UT_DELETE(ts);
  }
  UT_DELETE(m_active);
  UT_DELETE(m_free);
}

Tablespace *Tablespace_pool::get(my_thread_id id, enum tbsp_purpose purpose) {
  DBUG_EXECUTE_IF("ibt_pool_exhausted", return (nullptr););

  Tablespace *ts = nullptr;
  acquire();
  if (m_free->size() == 0) {
    /* Free pool is empty. Add more tablespaces by expanding it */
    dberr_t err = expand(POOL_EXPAND_SIZE);
    if (err != DB_SUCCESS) {
      /* Failure to expand the pool means that there is no disk space
      available to create .IBT files */
      release();
      ib::error() << "Unable to expand the temporary tablespace pool";
      return (nullptr);
    }
  }

  ts = m_free->back();
  m_free->pop_back();
  m_active->push_back(ts);
  ts->set_thread_id_and_purpose(id, purpose);

  release();
  return (ts);
}

void Tablespace_pool::free_ts(Tablespace *ts) {
  space_id_t space_id = ts->space_id();
  fil_space_t *space = fil_space_get(space_id);
  ut_ad(space != nullptr);

  if (space->size != FIL_IBT_FILE_INITIAL_SIZE) {
    ts->truncate();
  }

  acquire();

  Pool::iterator it = std::find(m_active->begin(), m_active->end(), ts);
  if (it != m_active->end()) {
    m_active->erase(it);
  } else {
    ut_ad(0);
  }

  m_free->push_back(ts);

  release();
}

dberr_t Tablespace_pool::initialize(bool create_new_db) {
  if (m_pool_initialized) {
    return (DB_SUCCESS);
  }

  ut_ad(m_active == nullptr && m_free == nullptr);

  m_active = UT_NEW_NOKEY(Pool());
  if (m_active == nullptr) {
    return (DB_OUT_OF_MEMORY);
  }

  m_free = UT_NEW_NOKEY(Pool());
  if (m_free == nullptr) {
    return (DB_OUT_OF_MEMORY);
  }

  delete_old_pool(create_new_db);
  dberr_t err = expand(m_init_size);
  if (err != DB_SUCCESS) {
    return (err);
  }

  m_pool_initialized = true;
  return (DB_SUCCESS);
}

dberr_t Tablespace_pool::expand(size_t size) {
  ut_ad(!m_pool_initialized || mutex_own(&m_mutex));
  for (size_t i = 0; i < size; i++) {
    Tablespace *ts = UT_NEW_NOKEY(Tablespace());

    if (ts == nullptr) {
      return (DB_OUT_OF_MEMORY);
    }

    dberr_t err = ts->create();

    if (err == DB_SUCCESS) {
      m_free->push_back(ts);
    } else {
      UT_DELETE(ts);
      return (err);
    }
  }
  return (DB_SUCCESS);
}

void Tablespace_pool::delete_old_pool(bool create_new_db) {
  if (create_new_db) {
    return;
  }

  ib::info() << "Scanning temp tablespace dir:'" << temp_tbsp_dir << "'";

  os_file_type_t type;
  bool exists = false;

  os_file_status(temp_tbsp_dir.c_str(), &exists, &type);

  if (!exists) {
    return;
  } else {
    ut_ad(type == OS_FILE_TYPE_DIR);
  }

  /* Walk the sub-tree of dir. */
  Dir_Walker::walk(temp_tbsp_dir.c_str(), false, [&](const std::string &path) {
    /* If it is a file and the suffix matches ".ibt", then delete it */

    if (!Dir_Walker::is_directory(path) && path.size() >= 4 &&
        (path.compare(path.length() - 4, 4, DOT_IBT) == 0)) {
      os_file_delete_if_exists(innodb_temp_file_key, path.c_str(), nullptr);
    }
  });
}

/** Create the directory holding the temporary pool tablespaces.
@return DB_SUCCESS in case of success and error if unable to create
the directory */
static dberr_t create_temp_dir() {
  temp_tbsp_dir = srv_temp_dir;

  /* Append PATH separator */
  if ((temp_tbsp_dir.length() != 0) &&
      (*temp_tbsp_dir.rbegin() != OS_PATH_SEPARATOR)) {
    temp_tbsp_dir += OS_PATH_SEPARATOR;
  }

  Fil_path temp_dir_path(srv_temp_dir);

  if (temp_dir_path.is_same_as(MySQL_datadir_path())) {
    /* User didn't pass explicit temp tablespace dir,
    create directory for temp tablespaces under this */
    temp_tbsp_dir.append(DIR_NAME);
    bool ret = os_file_create_directory(temp_tbsp_dir.c_str(), false);
    if (!ret) {
      ib::error() << "Cannot create directory: " << temp_tbsp_dir.c_str();
      return (DB_CANNOT_OPEN_FILE);
    }
    temp_tbsp_dir += OS_PATH_SEPARATOR;
    srv_temp_dir = const_cast<char *>(temp_tbsp_dir.c_str());
  } else {
    /* Explicit directory passed by user. Assume it exists as the parameter
    innodb_temp_tablespaces_dir has already been validated in
    innodb_init_params, returns appropriate error if the directory could not be
    found */
    return (DB_SUCCESS);
  }

  return (DB_SUCCESS);
}

dberr_t open_or_create(bool create_new_db) {
  dberr_t err = create_temp_dir();
  if (err != DB_SUCCESS) {
    return (err);
  }

  tbsp_pool = UT_NEW_NOKEY(Tablespace_pool(INIT_SIZE));
  if (tbsp_pool == nullptr) {
    return (DB_OUT_OF_MEMORY);
  }

  err = tbsp_pool->initialize(create_new_db);

  return (err);
}

void free_tmp(Tablespace *ts) {
  ts->reset_thread_id_and_purpose();
  tbsp_pool->free_ts(ts);
}

void delete_pool_manager() { UT_DELETE(tbsp_pool); }

void close_files() {
  auto close = [&](const ibt::Tablespace *ts) { ts->close(); };

  ibt::tbsp_pool->iterate_tbsp(close);
}

Tablespace *get_rpl_slave_tblsp() {
  if (rpl_slave_tblsp == nullptr) {
    rpl_slave_tblsp = tbsp_pool->get(SLAVE_THREAD_ID, TBSP_SLAVE);
  }
  return (rpl_slave_tblsp);
}

}  // namespace ibt
