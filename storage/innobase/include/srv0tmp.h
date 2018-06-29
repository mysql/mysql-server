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

#ifndef srv0tmp_h
#define srv0tmp_h
#include "srv0srv.h"
namespace ibt {

/** Purpose for using tablespace */
enum tbsp_purpose {
  TBSP_NONE = 0,  /*!< Tablespace is not being used for any
                 temporary table */
  TBSP_USER,      /*!< Tablespace is used for user temporary
                 tables */
  TBSP_INTRINSIC, /*!< Tablespace is used for intrinsic
                  tables */
  TBSP_SLAVE      /*!< Tablespace is used by the slave node
                  in a replication setup */
};
/** Create the session temporary tablespaces on startup
@param[in] create_new_db	true if bootstrapping
@return DB_SUCCESS on success, else DB_ERROR on failure */
dberr_t open_or_create(bool create_new_db);

/** Sesssion Temporary tablespace */
class Tablespace {
 public:
  Tablespace();

  ~Tablespace();

  /** Create the .IBT file with pattern "temp_*.ibt"
  @return DB_SUCCESS on success */
  dberr_t create();

  /** Close the .ibt file. Doesn't delete the tablespace
  @return true on success, false on failure */
  bool close() const;

  /** Truncate the tablespace
  @return true on success, false on failure */
  bool truncate();

  /** comparator for two tablespace objects
  @return true if space_ids are same, else false */
  bool operator==(const Tablespace &other) {
    return (this->m_space_id == other.m_space_id);
  }

  /** @return Return the space_id of the tablespace */
  space_id_t space_id() const { return (m_space_id); }

  /** Set the thread id of the thread and the purpose of using the tablespace
  @param[in]        thread_id     thread id of the thread requesting the
                                  tablespace
  @param[in]        purpose       purpose for requesting the tablespace */
  void set_thread_id_and_purpose(my_thread_id thread_id,
                                 enum tbsp_purpose purpose) {
    ut_ad(m_thread_id == 0);
    m_thread_id = thread_id;
    m_purpose = purpose;
  }

  /** Reset the thread id while returning the tablespace to the pool */
  void reset_thread_id_and_purpose() {
    ut_ad(m_thread_id != 0);
    m_thread_id = 0;
    m_purpose = TBSP_NONE;
  }

  /** @return thread id of the thread using the tablespace */
  my_thread_id thread_id() const { return (m_thread_id); }

  /** @return purpose for which the tablespace is being used */
  enum tbsp_purpose purpose() const { return (m_purpose); }

  /** @return complete path including filename */
  std::string path() const;

 private:
  /** The id used for name on disk temp_1.ibt, temp_2.ibt, etc
  @return the offset based on s_m_temp_space_id. The minimum offset is 1 */
  uint32_t file_id() const;

  /** @return the file_name only excluding the path */
  std::string file_name() const;

  /** space_id of the current tablespace */
  const space_id_t m_space_id;

  /** Next available space_id for tablespace. These are
  hardcoded space_ids at higher range */
  static space_id_t m_last_used_space_id;

  /** True only after .ibt file is created */
  bool m_inited;

  /** Tablespace belongs to this Session id  */
  my_thread_id m_thread_id;

  /** Purpose for this tablespace */
  enum tbsp_purpose m_purpose;
};

/** Pool of session temporary tablespaces. Each session gets at max two
tablespaces. For a session, we allocate one tablespace on the creation of
first intrinsic table and another on the creation of first user temporary
table (CREATE TEMPORARY TABLE t1). These tablespaces are private to session.
No other session can use them when a tablespace is in-use by the session.

Once a session disconnects, the tablespaces are truncated and released
to the pool. */
class Tablespace_pool {
 public:
  using Pool = std::list<Tablespace *, ut_allocator<Tablespace *>>;

  /** Tablespace_pool constructor
  @param[in]    init_size    Initial size of the tablespace pool */
  Tablespace_pool(size_t init_size);

  /** Destructor */
  ~Tablespace_pool();

  /** Return a session temporary tablespace. If
  pool is exhausted, expand and return one
  @param[in]    id      session id
  @param[in]    purpose purpose of using the tablespace
  @return Handle to the tablespace */
  Tablespace *get(my_thread_id id, enum tbsp_purpose purpose);

  /** Truncate and release the tablespace back to the pool
  @param[in]    ts      tablespace that need to be released back to the pool */
  void free_ts(Tablespace *ts);

  /** Initialize the pool on startup. Also delete
  old tablespaces if found
  @param[in]    create_new_db    true if the database is being created,
                                 false otherwise
  @return DB_SUCCESS on success, else DB_ERROR on failure */
  dberr_t initialize(bool create_new_db);

  /** Iterate through the list of tablespaces and perform specified operation
  on the tablespace on every iteration.
  @param[in]    f                Function pointer for the function to be
  executed on every iteration */
  template <typename F>
  void iterate_tbsp(F &&f) {
    acquire();

    std::for_each(begin(*m_active), end(*m_active), f);
    std::for_each(begin(*m_free), end(*m_free), f);

    release();
  }

 private:
  /** Acquire the mutex. It is used for all
  operations on the pool */
  void acquire() { mutex_enter(&m_mutex); }

  /** Release the mutex */
  void release() { mutex_exit(&m_mutex); }

  /** Expand the pool to the requested size
  @param[in] size	Number of tablespaces to be created
  @return DB_SUCCESS on success, else DB_ERROR on error */
  dberr_t expand(size_t size);

  /** Delete old session temporary tablespaces found
  on startup. This can happen if server is killed and
  started again
  @param[in]	create_new_db	true if we are bootstrapping */
  void delete_old_pool(bool create_new_db);

 private:
  /** True after the pool has been initialized */
  bool m_pool_initialized;
  /** Initial size of pool */
  size_t m_init_size;
  /** Vector of tablespaces that are unused */
  Pool *m_free;
  /** Vector of tablespaces that are being used */
  Pool *m_active;
  /** Mutex to protect concurrent operations on the pool */
  ib_mutex_t m_mutex;
};

/** Pool of temporary tablespace */
extern class Tablespace_pool *tbsp_pool;

/** Server temp tablespaces directory, can be absolute path. */
extern char *srv_temp_dir;

/** Release a tablespace back to the pool. The tablespace
will be truncated before adding back to pool */
void free_tmp(Tablespace *ts);

/** Delete the pool manager. This should be called only on
shutdown */
void delete_pool_manager();

/** Close all files in the pool */
void close_files();

/** @return a session tablespace dedicated for replication
slave threads. Note this slave session tablespace could
be used from many slave worker threads */
Tablespace *get_rpl_slave_tblsp();
}  // namespace ibt
#endif
