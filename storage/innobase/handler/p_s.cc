/*****************************************************************************

Copyright (c) 2016, 2024, Oracle and/or its affiliates.

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

/** @file handler/p_s.cc
 InnoDB performance_schema tables interface to MySQL.

 *******************************************************/

#include "storage/innobase/handler/p_s.h"

#include <type_traits>

#include "dict0dd.h"
#include "lock0iter.h"
#include "lock0lock.h"
#include "sql_table.h"
#include "trx0i_s.h"

/**
  @page PAGE_INNODB_PFS Innodb data lock instrumentation
  Innodb Performance Schema data lock instrumentation

  @section data_lock_iterators Data lock iterators

  To provide content to the performance_schema.data_locks table,
  innodb implements #Innodb_data_lock_iterator.

  Likewise, table performance_schema.data_wait_locks
  is populated with #Innodb_data_lock_wait_iterator.

  Both these iterators need to return the data present
  in the innodb engine memory,
  which imply to take the proper mutex locks when inspecting it.
  The structure to inspect here is the transaction list (#trx_sys)

  How to implement this scan is critical for performances.

  @subsection no_full_scan No full scan

  Consider this implementation:
  - Take all necessary locks
  - Scan all the innodb internal locks
  - Report all of them to the performance schema
  - Release all the locks taken

  This implementation materializes the entire table.

  The benefits with this approach are:
  - The materialized table is consistent

  The problems with this approach are:
  - The innodb engine is frozen for the entire duration,
  for a time that is unpredictable.
  - Memory consumption spikes, without bounds
  - Materializing all rows upfront is incompatible with supporting an index

  For example with N = 10,000 transactions,
  a single scan reports all 10,000 transaction locks.

  This alternative is rejected.

  @subsection no_single_row_scan No single row scan

  Consider this implementation:
  - Take all necessary locks
  - Resume the scan on innodb internal locks for 1 record
  - Report this record to the performance schema
  - Release all the locks taken

  This implementation returns a row for a single transaction,
  or even a single lock, at a time.

  The benefits with this approach are:
  - Memory consumption is well bounded, and low.

  The problems with this approach are:
  - Data reported can be very inconsistent.
  - Implementing a restartable scan, on a very dynamic structure,
  without holding any lock, is complex.
  - Even assuming how to implement a scan is resolved,
  looping N times to find element i, i+1, i+2 ... in a list
  ends up having a complexity in O(N^2), consuming CPU.

  For example with N = 10,000 transactions,
  the trx_list would be scanned 10,000 times
  to return 1 record each time.
  The total number of operations on the list is 100 Millions.

  This alternative is rejected.

  @subsection restartable_batch_scan Restartable batch scan

  What is implemented is:
  - As lock sys data structures are sharded with each shard having own latch, we
    inspect the shards one by one to avoid latching whole lock system
  - We first process table locks, then record locks
  - Table locks are processed one table at a time
  - Record locks are processed one internal hash table bucket at a time

  This is a compromise, with the following properties:
  - Memory consumption is bounded,
    by the number of locks in each bucket and on each table.
  - The duration of mutex locks on innodb structures is bounded
    by the number of locks in each bucket and on each table.
  - The data returned is not consistent,
    but at least it is "consistent by chunks"

*/

static const char *g_engine = "INNODB";
static const size_t g_engine_length = 6;
namespace {
struct parsed_table_path {
  std::string_view schema;
  std::string_view table;
  std::string_view partition;
  std::string_view subpartition;
};
}  // namespace
/** Searches for the lock_t object which has the specified engine_lock_id (@see
print_lock_id), and if it exists, executes provided f on it, under protection
of shard-specific latch. The type of the lock (LOCK_REC or LOCK_TABLE) is also
provided to f. For LOCK_REC multiple lock requests can share the same lock_t
object, thus f will be fed the specific heap_no. For LOCK_TABLE this third
argument will be always ULINT_UNDEFINED.
@param[in]     engine_lock_id
                   The string used in ENGINE_LOCK_ID column to uniquely
                   identify the lock request.
@param[in]     f
                   A callable, for which f(lock, type, heap_no) will be
                   executed, if lock with specified engine_lock_id exists. */
template <typename F>
static void find_lock_and_execute(const char *engine_lock_id, F &&f) {
  i_s_locks_row_t row;
  const int lock_type = trx_i_s_parse_lock_id(engine_lock_id, &row);
  if (0 == lock_type) {
    return;
  }

  if (lock_type == LOCK_REC) {
    const page_id_t page_id{row.lock_space, row.lock_page};
    locksys::Shard_latch_guard guard{UT_LOCATION_HERE, page_id};

    const lock_t *lock = lock_find_record_lock_by_guid(page_id, row.lock_guid);

    if (lock != nullptr) {
      std::forward<F>(f)(*lock, lock_type, row.lock_rec);
    }

  } else {
    ut_ad(lock_type == LOCK_TABLE);

    dict_table_t *table = dd_table_open_on_id_in_mem(row.lock_table_id, false);

    if (table != nullptr) {
      {
        locksys::Shard_latch_guard guard{UT_LOCATION_HERE, *table};
        const lock_t *lock = lock_find_table_lock_by_guid(table, row.lock_guid);
        if (lock != nullptr) {
          std::forward<F>(f)(*lock, lock_type, ULINT_UNDEFINED);
        }
      }

      dd_table_close(table, nullptr, nullptr, false);
    }
  }
}

/** Inspect data locks for the innodb storage engine. */
class Innodb_data_lock_iterator : public PSI_engine_data_lock_iterator {
 public:
  bool scan(PSI_server_data_lock_container *container,
            bool with_lock_data) override;

  void fetch(PSI_server_data_lock_container *container,
             const char *engine_lock_id, size_t engine_lock_id_length,
             bool with_lock_data) override;

 private:
  All_locks_iterator m_all_locks_iterator;

  /** For a given lock it will inform container about each lock request it
  represents (which can be more than one in case of LOCK_RECORD as there can be
  multiple lock requests differing only by heap_no compressed into single lock
  object), subject to filtering defined for the container, and
  optionally by filter_heap_no if with_filter is true.
  The with_filter is used to fetch just a lock request, the one specified by its
  heap_no. Please note, that there is no guarantee that the lock with this heap
  no is still in the lock sys.
  @param[in,out]  parsed_paths    The cache of table path parsing results
  @param[in,out]  container       The container to fill. Serves as both visitor
                                  and filter
  @param[in]      lock            The lock to inspect and report upon
  @param[in]      with_lock_data  True if column LOCK_DATA is required.
  @param[in]      with_filter     If true then filter_heap_no has
                                  to be passed, and means that only the lock
                                  request for that heap number (if still present
                                  and matches the container's filters) should be
                                  reported to the container. If false then all
                                  lock requests represented for this lock are
                                  considered for reporting.
  @param[in]      filter_heap_no  if with_filter is true it further narrows the
                                  set of reported lock requests to just the one
                                  with given immutable id.
  @return number of locks reported to the container */
  static size_t report(
      std::unordered_map<const char *, parsed_table_path> &parsed_paths,
      PSI_server_data_lock_container &container, const lock_t &lock,
      bool with_lock_data, bool with_filter,
      ulint filter_heap_no = ULINT_UNDEFINED);
};

/** Inspect data lock waits for the innodb storage engine. */
class Innodb_data_lock_wait_iterator
    : public PSI_engine_data_lock_wait_iterator {
 public:
  bool scan(PSI_server_data_lock_wait_container *container) override;

  void fetch(PSI_server_data_lock_wait_container *container,
             const char *requesting_engine_lock_id,
             size_t requesting_engine_lock_id_length,
             const char *blocking_engine_lock_id,
             size_t blocking_engine_lock_id_length) override;

 private:
  All_locks_iterator m_all_locks_iterator;

  /** For a given wait_lock it will inform container about each lock which is
  blocking this wait_lock, subject to filtering defined for the container, and
  optionally by filter_blocking_lock_immutable_id if with_filter is true.
  The with_filter is used to fetch just a single blocking lock, the one
  specified by its immutable id. Please note, that there is no guarantee that
  this blocking lock is still in the lock sys.
  @param[in,out]  container   The container to fill. Serves as both visitor and
                              filter
  @param[in]      wait_lock   The requesting lock which has to wait
  @param[in]      with_filter If true then filter_blocking_lock_immutable_id has
                              to be passed, and means that only the lock with
                              that immutable id (if still present and matches
                              the container's filters) should be reported to the
                              container. If false then all locks blocking the
                              wait_lock are considered for reporting.
  @param[in]      filter_blocking_lock_guid if with_filter is true it
                              further narrows the set of reported locks to just
                              the one with given guid.
  @return number of locks reported to the container */
  static size_t report(
      PSI_server_data_lock_wait_container &container, const lock_t &wait_lock,
      bool with_filter,
      const lock_guid_t &filter_blocking_lock_guid = lock_guid_t());
};

PSI_engine_data_lock_iterator *
Innodb_data_lock_inspector::create_data_lock_iterator() {
  return new Innodb_data_lock_iterator();
}

PSI_engine_data_lock_wait_iterator *
Innodb_data_lock_inspector::create_data_lock_wait_iterator() {
  return new Innodb_data_lock_wait_iterator();
}

void Innodb_data_lock_inspector::destroy_data_lock_iterator(
    PSI_engine_data_lock_iterator *it) {
  delete it;
}

void Innodb_data_lock_inspector::destroy_data_lock_wait_iterator(
    PSI_engine_data_lock_wait_iterator *it) {
  delete it;
}

namespace {
/** Allocate identifier in performance schema container. The string is allocated
in the cache of the container and is owned by that cache. Therefore the
returned std::string_view is only valid for as long as the cache is not cleared
by a call to PFS_data_cache::clear() which occurs in:
  - PFS_data_lock_wait_container::clear()
  - PFS_data_lock_wait_container::shrink()
  - PFS_data_lock_container::clear()
  - PFS_data_lock_wait_container::shrink()
@param[in]  container        The container to fill
@param[in]  kind             The identifier kind
@param[in]  id_str           The identifier string
@return view of the cached string */
std::string_view alloc_identifier(PSI_server_data_lock_container *container,
                                  PSI_identifier kind,
                                  const std::string &id_str) {
  if (id_str.length() > 0) {
    const char *cached_id;
    size_t cached_id_length;
    container->cache_identifier(kind, id_str.c_str(), id_str.length(),
                                &cached_id, &cached_id_length);
    return {cached_id, cached_id_length};
  } else {
    return {};
  }
}

/** Parse a table path string.
Isolate the table schema, name, partition and sub partition
from a table path string.
Convert these strings and store them in the performance schema container.
@note String returned are not zero terminated.
@param[in] container                    The container to fill
@param[in] table_path                   The table path string
@return parsed fragments of the table name pointing to cached strings
*/
parsed_table_path parse_table_path(PSI_server_data_lock_container *container,
                                   const char *table_path) {
  std::string dict_table(table_path);

  /* Get schema and table name in system cs. */
  std::string schema;
  std::string table;
  std::string partition;
  bool is_tmp;
  dict_name::get_table(dict_table, true, schema, table, partition, is_tmp);

  std::string part;
  std::string sub_part;
  if (!partition.empty()) {
    ut_ad(dict_name::is_partition(dict_table));
    /* Get schema partition and sub-partition name in system cs. */
    dict_name::get_partition(partition, true, part, sub_part);
  }

  return {
      .schema = alloc_identifier(container, PSI_IDENTIFIER_SCHEMA, schema),
      .table = alloc_identifier(container, PSI_IDENTIFIER_TABLE, table),
      .partition = alloc_identifier(container, PSI_IDENTIFIER_PARTITION, part),
      .subpartition =
          alloc_identifier(container, PSI_IDENTIFIER_SUBPARTITION, sub_part),
  };
}
}  // namespace

/** Print a table lock id.
@param[in]      lock            The lock to print
@param[in,out]  lock_id         Printing buffer
@param[in]      lock_id_size    Printing buffer length
*/
static void print_table_lock_id(const lock_t &lock, char *lock_id,
                                size_t lock_id_size) {
  /* We try to be backward compatible with INFORMATION_SCHEMA so that one can
  join with INFORMATION_SCHEMA.innodb_trx.trx_requested_lock_id column */
  i_s_locks_row_t row;
  fill_locks_row(&row, &lock, ULINT_UNDEFINED);
  trx_i_s_create_lock_id(row, lock_id, lock_id_size);
}

/** Print a record lock id.
@param[in]      lock            The lock to print
@param[in]      heap_no         Lock heap number
@param[in,out]  lock_id         Printing buffer
@param[in]      lock_id_size    Printing buffer length
*/
static void print_record_lock_id(const lock_t &lock, ulint heap_no,
                                 char *lock_id, size_t lock_id_size) {
  /* We try to be backward compatible with INFORMATION_SCHEMA so that one can
  join with INFORMATION_SCHEMA.innodb_trx.trx_requested_lock_id column */
  i_s_locks_row_t row;
  fill_locks_row(&row, &lock, heap_no);
  trx_i_s_create_lock_id(row, lock_id, lock_id_size);
}

/** Print a lock id.
@param[in]      lock            The lock to print
@param[in]      heap_no         Lock heap number if lock's type is LOCK_REC,
                                ignored otherwise.
@param[in,out]  lock_id         Printing buffer
@param[in]      lock_id_size    Printing buffer length
*/
static void print_lock_id(const lock_t &lock, ulint heap_no, char *lock_id,
                          size_t lock_id_size) {
  switch (lock_get_type(&lock)) {
    case LOCK_TABLE:
      print_table_lock_id(lock, lock_id, lock_id_size);
      break;
    case LOCK_REC:
      print_record_lock_id(lock, heap_no, lock_id, lock_id_size);
      break;
    default:
      ut_error;
  }
}

bool Innodb_data_lock_iterator::scan(PSI_server_data_lock_container *container,
                                     bool with_lock_data) {
  if (!container->accept_engine(g_engine, g_engine_length)) {
    return true;
  }
  std::unordered_map<const char *, parsed_table_path> parsed_paths;
  for (size_t found = 0; 0 == found;) {
    if (m_all_locks_iterator.iterate_over_next_batch([&](const lock_t &lock) {
          found +=
              report(parsed_paths, *container, lock, with_lock_data, false);
        })) {
      return true;
    }
  }
  return false;
}

void Innodb_data_lock_iterator::fetch(PSI_server_data_lock_container *container,
                                      const char *engine_lock_id,
                                      size_t engine_lock_id_length,
                                      bool with_lock_data) {
  ut_ad(strnlen(engine_lock_id, engine_lock_id_length + 1) ==
        engine_lock_id_length);

  if (!container->accept_engine(g_engine, g_engine_length)) {
    return;
  }
  std::unordered_map<const char *, parsed_table_path> parsed_paths;
  find_lock_and_execute(engine_lock_id,
                        [&](const lock_t &lock, int lock_type, ulint heap_no) {
                          report(parsed_paths, *container, lock, with_lock_data,
                                 lock_type == LOCK_REC, heap_no);
                        });
}

size_t Innodb_data_lock_iterator::report(
    std::unordered_map<const char *, parsed_table_path> &parsed_paths,
    PSI_server_data_lock_container &container, const lock_t &lock,
    bool with_lock_data, bool with_filter, ulint filter_heap_no) {
  size_t found = 0;
  ulonglong thread_id;
  ulonglong event_id;
  const char *index_name;
  size_t index_name_length;
  const void *identity;
  const char *lock_mode_str;
  const char *lock_type_str;
  const char *lock_status_str;
  const char *lock_data_str;
  char engine_lock_id[TRX_I_S_LOCK_ID_MAX_LEN + 1];
  size_t engine_lock_id_length;
  ulint heap_no;
  ut_ad(locksys::owns_lock_shard(&lock));
  const auto trx_id = lock_get_trx_id(&lock);

  if (!container.accept_transaction_id(trx_id)) {
    return found;
  }

  lock_get_psi_event(&lock, &thread_id, &event_id);

  if (!container.accept_thread_id_event_id(thread_id, event_id)) {
    return found;
  }
  const char *table_path = lock_get_table_name(&lock).m_name;
  auto parsed_it = parsed_paths.find(table_path);
  if (parsed_it == parsed_paths.end()) {
    parsed_it =
        parsed_paths
            .emplace(table_path, parse_table_path(&container, table_path))
            .first;
  }
  const auto parsed = parsed_it->second;

  if (!container.accept_object(
          parsed.schema.data(), parsed.schema.size(), parsed.table.data(),
          parsed.table.size(), parsed.partition.data(), parsed.partition.size(),
          parsed.subpartition.data(), parsed.subpartition.size())) {
    return found;
  }

  identity = &lock;
  lock_mode_str = lock_get_mode_str(&lock);
  lock_type_str = lock_get_type_str(&lock);
  lock_status_str = lock_is_waiting(lock) ? "WAITING" : "GRANTED";

  switch (lock_get_type(&lock)) {
    case LOCK_TABLE:
      print_table_lock_id(lock, engine_lock_id, sizeof(engine_lock_id));
      engine_lock_id_length = strlen(engine_lock_id);

      if (container.accept_lock_id(engine_lock_id, engine_lock_id_length)) {
        container.add_lock_row(
            g_engine, g_engine_length, engine_lock_id, engine_lock_id_length,
            trx_id, thread_id, event_id, parsed.schema.data(),
            parsed.schema.size(), parsed.table.data(), parsed.table.size(),
            parsed.partition.data(), parsed.partition.size(),
            parsed.subpartition.data(), parsed.subpartition.size(), nullptr, 0,
            identity, lock_mode_str, lock_type_str, lock_status_str, nullptr);
        found++;
      }
      break;
    case LOCK_REC:
      index_name = lock_rec_get_index_name(&lock);
      index_name_length = strlen(index_name);

      heap_no = lock_rec_find_set_bit(&lock);

      while (heap_no != ULINT_UNDEFINED) {
        if (!with_filter || (heap_no == filter_heap_no)) {
          print_record_lock_id(lock, heap_no, engine_lock_id,
                               sizeof(engine_lock_id));
          engine_lock_id_length = strlen(engine_lock_id);

          if (container.accept_lock_id(engine_lock_id, engine_lock_id_length)) {
            if (with_lock_data) {
              p_s_fill_lock_data(&lock_data_str, &lock, heap_no, &container);
            } else {
              lock_data_str = nullptr;
            }

            container.add_lock_row(
                g_engine, g_engine_length, engine_lock_id,
                engine_lock_id_length, trx_id, thread_id, event_id,
                parsed.schema.data(), parsed.schema.size(), parsed.table.data(),
                parsed.table.size(), parsed.partition.data(),
                parsed.partition.size(), parsed.subpartition.data(),
                parsed.subpartition.size(), index_name, index_name_length,
                identity, lock_mode_str, lock_type_str, lock_status_str,
                lock_data_str);
            found++;
          }
        }

        heap_no = lock_rec_find_next_set_bit(&lock, heap_no);
      }
      break;
    default:
      ut_error;
  }

  return found;
}

bool Innodb_data_lock_wait_iterator::scan(
    PSI_server_data_lock_wait_container *container) {
  if (!container->accept_engine(g_engine, g_engine_length)) {
    return true;
  }

  for (size_t found = 0; 0 == found;) {
    if (m_all_locks_iterator.iterate_over_next_batch([&](const lock_t &lock) {
          found += report(*container, lock, false);
        })) {
      return true;
    }
  }
  return false;
}

void Innodb_data_lock_wait_iterator::fetch(
    PSI_server_data_lock_wait_container *container,
    const char *requesting_engine_lock_id,
    size_t requesting_engine_lock_id_length,
    const char *blocking_engine_lock_id,
    size_t blocking_engine_lock_id_length) {
  ut_ad(strnlen(requesting_engine_lock_id,
                requesting_engine_lock_id_length + 1) ==
        requesting_engine_lock_id_length);
  ut_ad(strnlen(blocking_engine_lock_id, blocking_engine_lock_id_length + 1) ==
        blocking_engine_lock_id_length);

  if (!container->accept_engine(g_engine, g_engine_length)) {
    return;
  }

  i_s_locks_row_t blocking_lock_row;
  const int blocking_lock_type =
      trx_i_s_parse_lock_id(blocking_engine_lock_id, &blocking_lock_row);
  if (0 == blocking_lock_type) {
    return;
  }

  find_lock_and_execute(
      requesting_engine_lock_id,
      [&](const lock_t &lock, int lock_type, ulint heap_no) {
        ut_ad(lock_type != LOCK_REC || heap_no == lock_rec_find_set_bit(&lock));
        report(*container, lock, true, blocking_lock_row.lock_guid);
      });
  return;
}

size_t Innodb_data_lock_wait_iterator::report(
    PSI_server_data_lock_wait_container &container, const lock_t &wait_lock,
    bool with_filter, const lock_guid_t &filter_blocking_lock_guid) {
  ut_ad(locksys::owns_lock_shard(&wait_lock));
  if (!lock_is_waiting(wait_lock)) {
    return 0;
  }

  ulonglong requesting_thread_id;
  ulonglong requesting_event_id;
  const void *requesting_identity;
  char requesting_engine_lock_id[TRX_I_S_LOCK_ID_MAX_LEN + 1];
  size_t requesting_engine_lock_id_length;
  ulonglong blocking_trx_id;
  ulonglong blocking_thread_id;
  ulonglong blocking_event_id;
  const void *blocking_identity;
  char blocking_engine_lock_id[TRX_I_S_LOCK_ID_MAX_LEN + 1];
  size_t blocking_engine_lock_id_length;

  size_t found = 0;

  const auto requesting_record_type = lock_get_type(&wait_lock);
  const auto requesting_trx_id = lock_get_trx_id(&wait_lock);
  if (!container.accept_requesting_transaction_id(requesting_trx_id)) {
    return found;
  }

  lock_get_psi_event(&wait_lock, &requesting_thread_id, &requesting_event_id);
  if (!container.accept_requesting_thread_id_event_id(requesting_thread_id,
                                                      requesting_event_id)) {
    return found;
  }

  const ulint heap_no = requesting_record_type == LOCK_REC
                            ? lock_rec_find_set_bit(&wait_lock)
                            : ULINT_UNDEFINED;

  print_lock_id(wait_lock, heap_no, requesting_engine_lock_id,
                sizeof(requesting_engine_lock_id));
  requesting_engine_lock_id_length = strlen(requesting_engine_lock_id);
  if (!container.accept_requesting_lock_id(requesting_engine_lock_id,
                                           requesting_engine_lock_id_length)) {
    return found;
  }

  requesting_identity = &wait_lock;
  locksys::find_blockers(wait_lock, [&](const lock_t &curr_lock) {
    ut_ad(locksys::owns_lock_shard(&curr_lock));
    if (with_filter && lock_guid_t(curr_lock) != filter_blocking_lock_guid) {
      return false;
    }

    blocking_trx_id = lock_get_trx_id(&curr_lock);
    if (!container.accept_blocking_transaction_id(blocking_trx_id)) {
      return false;
    }

    lock_get_psi_event(&curr_lock, &blocking_thread_id, &blocking_event_id);
    if (!container.accept_blocking_thread_id_event_id(blocking_thread_id,
                                                      blocking_event_id)) {
      return false;
    }

    blocking_identity = &curr_lock;
    print_lock_id(curr_lock, heap_no, blocking_engine_lock_id,
                  sizeof(blocking_engine_lock_id));
    blocking_engine_lock_id_length = strlen(blocking_engine_lock_id);
    if (!container.accept_blocking_lock_id(blocking_engine_lock_id,
                                           blocking_engine_lock_id_length)) {
      return false;
    }

    container.add_lock_wait_row(
        g_engine, g_engine_length, requesting_engine_lock_id,
        requesting_engine_lock_id_length, requesting_trx_id,
        requesting_thread_id, requesting_event_id, requesting_identity,
        blocking_engine_lock_id, blocking_engine_lock_id_length,
        blocking_trx_id, blocking_thread_id, blocking_event_id,
        blocking_identity);
    found++;
    return false;
  });

  return found;
}
