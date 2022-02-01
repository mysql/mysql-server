/*
   Copyright (c) 2011, 2021, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_SHARE_H
#define NDB_SHARE_H

#include <string>
#ifndef NDEBUG
#include <unordered_set>
#endif

#include "my_alloc.h"                          // MEM_ROOT
#include "storage/ndb/include/ndbapi/Ndb.hpp"  // Ndb::TupleIdRange
#include "thr_lock.h"                          // THR_LOCK

enum Ndb_binlog_type {
  NBT_DEFAULT = 0,
  NBT_NO_LOGGING = 1,
  NBT_UPDATED_ONLY = 2,
  NBT_FULL = 3,
  NBT_USE_UPDATE = 4,
  NBT_UPDATED_ONLY_USE_UPDATE = 6,
  NBT_FULL_USE_UPDATE = 7,
  NBT_UPDATED_ONLY_MINIMAL = 8,
  NBT_UPDATED_FULL_MINIMAL = 9
};

/*
  Stats that can be retrieved from ndb
*/
struct Ndb_statistics {
  Uint64 row_count;
  ulong row_size;
  Uint64 fragment_memory;
  Uint64 fragment_extent_space;
  Uint64 fragment_extent_free_space;
};

struct NDB_SHARE {
  THR_LOCK lock;
  mutable mysql_mutex_t mutex;
  struct NDB_SHARE_KEY *key;
  char *db;
  char *table_name;

 private:
  /*
    The current TupleIdRange for the table is stored in NDB_SHARE in order
    for the auto_increment value of a table to be consecutive between
    different user connections, i.e subsequent INSERTs by two
    connections should get consecutive values(if that's how the auto
    increment setting of MySQL Server and ndbcluster plugin are currently
    configured). The default of NdbApi would otherwise be to give each
    Ndb object instance their own range.

    NOTE! Protected by NDB_SHARE::mutex, can only be accessed via
    the Tuple_id_range_guard class
  */
  Ndb::TupleIdRange tuple_id_range;

 public:
  // RAII style class for accessing tuple_id_range
  class Tuple_id_range_guard {
    NDB_SHARE *m_share;

   public:
    Ndb::TupleIdRange &range;

    Tuple_id_range_guard(NDB_SHARE *share)
        : m_share(share), range(share->tuple_id_range) {
      mysql_mutex_lock(&m_share->mutex);
    }
    ~Tuple_id_range_guard() { mysql_mutex_unlock(&m_share->mutex); }
  };

  // Reset the tuple_id_range
  void reset_tuple_id_range() {
    Tuple_id_range_guard g(this);
    g.range.reset();
  }

  struct Ndb_statistics stat;
  struct Ndb_index_stat *index_stat_list;

 private:
  enum Ndb_share_flags : uint {
    // Flag describing binlogging ON or OFF
    // - table should not be binlogged
    FLAG_NO_BINLOG = 1UL << 2,

    // Flags describing the binlog mode
    // - table should be binlogged with full rows
    FLAG_BINLOG_MODE_FULL = 1UL << 3,
    // - table update should be binlogged using update log event
    FLAG_BINLOG_MODE_USE_UPDATE = 1UL << 4,
    // - table update should be binlogged using minimal format:
    //    i.e before(primary key(s)):after(changed column(s))
    FLAG_BINLOG_MODE_MINIMAL_UPDATE = 1UL << 5,

    // Flag describing if table have event
    // NOTE! The decision wheter or not a table have event is decided
    // only once by Ndb_binlog_client::table_should_have_event()
    FLAG_TABLE_HAVE_EVENT = 1UL << 6,
  };

  uint flags;

 public:
  bool get_binlog_nologging() const {
    return flags & NDB_SHARE::FLAG_NO_BINLOG;
  }
  bool get_binlog_full() const {
    return flags & NDB_SHARE::FLAG_BINLOG_MODE_FULL;
  }
  bool get_binlog_use_update() const {
    return flags & NDB_SHARE::FLAG_BINLOG_MODE_USE_UPDATE;
  }
  bool get_binlog_update_minimal() const {
    return flags & NDB_SHARE::FLAG_BINLOG_MODE_MINIMAL_UPDATE;
  }

  void set_binlog_flags(Ndb_binlog_type ndb_binlog_type);

  void set_have_event() { flags |= NDB_SHARE::FLAG_TABLE_HAVE_EVENT; }
  bool get_have_event() const {
    return flags & NDB_SHARE::FLAG_TABLE_HAVE_EVENT;
  }

  struct NDB_CONFLICT_FN_SHARE *m_cfn_share;

  // The event operation used for listening to changes in NDB for this
  // table, protected by mutex
  class NdbEventOperation *op;

  // Check if an event operation has been setup for this share
  bool have_event_operation() const {
    mysql_mutex_lock(&mutex);
    const bool have_op = (op != nullptr);
    mysql_mutex_unlock(&mutex);
    return have_op;
  }

  // Raw pointer for passing table definition from schema dist client to
  // participant in the same node to avoid that participant have to access
  // the DD to open the table definition.
  const void *inplace_alter_new_table_def;

  static NDB_SHARE *create(const char *key);
  static void destroy(NDB_SHARE *share);

  // Functions for working with the opaque NDB_SHARE_KEY
  static struct NDB_SHARE_KEY *create_key(const char *new_key);
  static void free_key(struct NDB_SHARE_KEY *);

  static std::string key_get_key(struct NDB_SHARE_KEY *);
  static char *key_get_db_name(struct NDB_SHARE_KEY *);
  static char *key_get_table_name(struct NDB_SHARE_KEY *);

  size_t key_length() const;
  const char *key_string() const;

  /**
   * Note about acquire_reference() / release_reference() functions:
   *
   *  *) All shares are referred from the list of tables
   *     until they are released with NDB_SHARE::mark_share_dropped().
   *  *) All shares are referred by the 'binlog' if its DDL operations
   *     should be replicated with schema events ('share->op != nullptr')
   *     Release 'binlog' reference when event operations are released.
   *  *) All shares are ref counted when they are temporarily referred
   *     inside a function. Release when last share related operation
   *     has been completed.
   *  *) Each ha_ndbcluster instance have a share reference (m_share) which is
   *     acquired in :open() and released in ::close(). Those references are
   *     a little special as it indicates that a user is holding the table
   *     open. Those references can't be controlled in any other way than
   *     trying to flush the table from the MySQL Server open table cache.
   */

  // Acquire NDB_SHARE reference for use by ha_ndbcluster
  static NDB_SHARE *acquire_for_handler(const char *db_name,
                                        const char *table_name,
                                        const class ha_ndbcluster *reference);
  // Create NDB_SHARE reference for use by ha_ndbcluster
  // NOTE! Used only in a few special cases to allow opening table before
  // connection to NDB has been initialized
  static NDB_SHARE *create_for_handler(const char *db_name,
                                       const char *table_name,
                                       const class ha_ndbcluster *);
  // Release NDB_SHARE reference acquired by ha_ndbcluster
  static void release_for_handler(NDB_SHARE *share,
                                  const class ha_ndbcluster *reference);

  // Acquire or create NDB_SHARE
  // - used when table has just been created or discovered/synced/installed
  static NDB_SHARE *acquire_or_create_reference(const char *db_name,
                                                const char *table_name,
                                                const char *reference);
  // Acquire NDB_SHARE reference
  static NDB_SHARE *acquire_reference(const char *db_name,
                                      const char *table_name,
                                      const char *reference);

  // Acquire reference to existing NDB_SHARE
  static NDB_SHARE *acquire_reference_on_existing(NDB_SHARE *share,
                                                  const char *reference);
  // Release NDB_SHARE reference
  static void release_reference(NDB_SHARE *share, const char *reference);

  // Mark NDB_SHARE as dropped and release reference
  static void mark_share_dropped_and_release(NDB_SHARE *share,
                                             const char *reference);

  // Rename share, rename in list of tables
  static int rename_share(NDB_SHARE *share, struct NDB_SHARE_KEY *new_key);

#ifndef NDEBUG
  static void dbg_check_shares_update();
#endif

  static void initialize(CHARSET_INFO *charset);
  static void deinitialize();
  static void release_extra_share_references();

  // Print the list of open shares to stderr
  static void print_remaining_open_shares();

  // Debug print the NDB_SHARE to string
  void debug_print(std::string &out, const char *line_separator = "") const;

 private:
  // Debug print the list of open NDB_SHARE's to string
  static void debug_print_shares(std::string &out);

 private:
  uint m_use_count;
  uint increment_use_count() { return ++m_use_count; }
  uint decrement_use_count();
  uint use_count() const { return m_use_count; }

  enum { NSS_INITIAL = 0, NSS_DROPPED } state;

  const char *share_state_string() const;

  /**
     @brief Permanently free a NDB_SHARE which is no longer referred.
     @note The NDB_SHARE must already be marked as dropped and be in
     the dropped list.

     @param share_ptr Pointer to NDB_SHARE pointer
  */
  static void real_free_share(NDB_SHARE **share_ptr);
  static void free_share(NDB_SHARE **share);

  static NDB_SHARE *acquire_reference_impl(const char *key);

  /**
    @brief Mark NDB_SHARE as dropped, remove from list of open shares and
    put in list of dropped shares if the NDB_SHARE is still referenced.

    @param share_ptr Pointer to the NDB_SHARE pointer
  */
  static void mark_share_dropped_impl(NDB_SHARE **share_ptr);

#ifndef NDEBUG
  // Lists of the different "users" who have acquired a reference to
  // this NDB_SHARE, used for checking the reference counter "m_use_count"
  // in a programmatic way.
  // Protected by "shares_mutex" in the same way as "m_use_count".
  struct Ndb_share_references {
    std::unordered_set<const class ha_ndbcluster *> handlers;
    std::unordered_set<std::string> strings;

    size_t size() const { return handlers.size() + strings.size(); }

    bool exists(const class ha_ndbcluster *ref) const {
      return handlers.find(ref) != handlers.end();
    }

    bool insert(const class ha_ndbcluster *ref) {
      // The reference should not already exist
      assert(!exists(ref));

      // Insert the new handler reference in the list
      const auto result = handlers.insert(ref);
      assert(result.second);

      return result.second;
    }

    bool erase(const class ha_ndbcluster *ref) {
      // The reference must already exist
      assert(exists(ref));

      // Remove the handler reference from the list
      const size_t erased = handlers.erase(ref);
      assert(erased == 1);

      return erased == 1;
    }

    bool exists(const char *ref) { return strings.find(ref) != strings.end(); }

    bool insert(const char *ref) {
      // The reference should not already exist
      assert(!exists(ref));

      // Insert the new string reference in the list
      const auto result = strings.insert(ref);
      assert(result.second);

      return result.second;
    }

    bool erase(const char *ref) {
      // The reference must already exist
      assert(exists(ref));

      // Remove the string reference from the list
      const size_t erased = strings.erase(ref);
      assert(erased == 1);

      return erased == 1;
    }

    bool check_empty() const;

    // Debug print the reference lists to string
    void debug_print(std::string &out, const char *line_separator = "") const;
  };
  Ndb_share_references *refs;
#endif
  void refs_insert(const char *reference MY_ATTRIBUTE((unused))) {
    assert(refs->insert(reference));
  }
  void refs_insert(
      const class ha_ndbcluster *reference MY_ATTRIBUTE((unused))) {
    assert(refs->insert(reference));
  }
  void refs_erase(const char *reference MY_ATTRIBUTE((unused))) {
    assert(refs->erase(reference));
  }
  void refs_erase(const class ha_ndbcluster *reference MY_ATTRIBUTE((unused))) {
    assert(refs->erase(reference));
  }

 public:
  bool refs_exists(const char *reference MY_ATTRIBUTE((unused))) const {
#ifndef NDEBUG
    return refs->exists(reference);
#else
    return true;
#endif
  }
};

/**
   @brief Utility class for working with a temporary
          NDB_SHARE* references RAII style

          The class will automatically "get" a NDB_SHARE*
          reference and release it when going out of scope.
 */
class Ndb_share_temp_ref {
  NDB_SHARE *m_share;
  const std::string m_reference;

  Ndb_share_temp_ref(const Ndb_share_temp_ref &) = delete;
  Ndb_share_temp_ref &operator=(const Ndb_share_temp_ref &) = delete;

 public:
  Ndb_share_temp_ref(const char *db_name, const char *table_name,
                     const char *reference)
      : m_reference(reference) {
    m_share =
        NDB_SHARE::acquire_reference(db_name, table_name, m_reference.c_str());
    // Should always exist
    assert(m_share);
  }

  Ndb_share_temp_ref(NDB_SHARE *share, const char *reference)
      : m_reference(reference) {
    // The share and a reference should exist
    assert(share);
    assert(share->refs_exists(reference));
    m_share = share;
  }

  ~Ndb_share_temp_ref() {
    assert(m_share);
    /* release the temporary reference */
    NDB_SHARE::release_reference(m_share, m_reference.c_str());
  }

  // Return the NDB_SHARE* by type conversion operator
  operator NDB_SHARE *() const {
    assert(m_share);
    return m_share;
  }

  // Return the NDB_SHARE* when using pointer operator
  const NDB_SHARE *operator->() const {
    assert(m_share);
    return m_share;
  }
};

#endif
