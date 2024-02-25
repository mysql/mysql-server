/* Copyright (c) 2016, 2023, Oracle and/or its affiliates.

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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file storage/perfschema/pfs_data_lock.cc
  The performance schema implementation for data locks.
*/

#include "storage/perfschema/pfs_data_lock.h"

#include <assert.h>
#include <stddef.h>

/* clang-format off */
/**
  @page PAGE_PFS_DATA_LOCKS Performance schema data locks

  @section SERVER_ENGINE_INTERFACE Server / Storage engine interface

  @subsection SE_INTERFACE_REGISTRATION Registration

  @startuml

  title Registration

  participant server as "MySQL server"
  participant pfs as "Performance schema"
  participant se as "Storage Engine"
  participant se_inspector as "Storage Engine\nData Lock Inspector"

  == plugin init ==
  server -> se : plugin_init()
  se -> pfs : register_data_lock()

  == SELECT * FROM performance_schema.data_locks ==
  server -> pfs : table_data_locks::rnd_next()
  pfs -> se_inspector : (multiple calls)

  == plugin deinit ==
  server -> se : plugin_deinit()
  se -> pfs : unregister_data_lock()

  @enduml

  To expose DATA_LOCKS to the performance schema,
  a storage engine needs to:
  - implement a sub class of #PSI_engine_data_lock_inspector
  - register it with the performance schema on init
  - unregister it with the performance schema on deinit

  While the storage engine is in use (between init and deinit),
  the performance schema keeps a reference to the data lock inspector given,
  and use it to inspect the storage engine data locks.

  @subsection SE_INTERFACE_SCAN_1 Iteration for each storage engine

  @startuml

  title Iteration for each storage engine

  participant server as "MySQL server"
  participant pfs as "Performance schema\nTable data_locks"
  participant pfs_container as "Performance schema\nData Lock container"
  participant se_inspector as "Storage Engine\nData Lock Inspector"
  participant se_iterator as "Storage Engine\nData Lock Iterator"

  == SELECT init ==
  server -> pfs : rnd_init()
  activate pfs_container
  pfs -> pfs_container : create()

  == For each storage engine ==
  pfs -> se_inspector : create_iterator()
  activate se_iterator
  se_inspector -> se_iterator : create()

  pfs -> se_iterator : (multiple calls)

  pfs -> se_iterator : destroy()
  deactivate se_iterator

  == SELECT end ==
  server -> pfs : rnd_end()
  pfs -> pfs_container : destroy()
  deactivate pfs_container

  @enduml

  When the server performs a SELECT * from performance_schema.data_locks,
  the performance schema creates a #PSI_server_data_lock_container for
  the duration of the table scan.

  Then, the scan loops for each storage engine capable of exposing data locks
  (that is, engines that registered a data lock inspector).

  For each engine, the inspector is called to create an iterator,
  dedicated for this SELECT scan.

  @subsection SE_INTERFACE_SCAN_2 Iteration inside a storage engine

  @startuml

  title Iteration inside a storage engine

  participant server as "MySQL server"
  participant pfs as "Performance schema\nTable data_locks"
  participant pfs_container as "Performance schema\nData Lock container"
  participant se_iterator as "Storage Engine\nData Lock Iterator"

  loop until the storage engine iterator is done

    == SELECT scan ==

    server -> pfs : rnd_next()

    == First scan, fetch N rows at once from the storage engine ==

    pfs -> se_iterator : scan()
    se_iterator -> pfs_container : add_row() // 1
    se_iterator -> pfs_container : add_row() // 2
    se_iterator -> pfs_container : ...
    se_iterator -> pfs_container : add_row() // N
    pfs -> pfs_container : get_row(1)
    pfs -> server : result set row 1

    == Subsequent scans, return the rows collected ==

    server -> pfs : rnd_next()
    pfs -> pfs_container : get_row(2)
    pfs -> server : result set row 2

    server -> pfs : rnd_next()
    pfs -> pfs_container : get_row(...)
    pfs -> server : result set row ...

    server -> pfs : rnd_next()
    pfs -> pfs_container : get_row(N)
    pfs -> server : result set row N

  end

  @enduml

  When table_data_locks::rnd_next() is first called,
  the performance schema calls the storage engine iterator,
  which adds N rows in the data container.

  Upon subsequent calls to table_data_locks::rnd_next(),
  data present in the container is returned.
  This process loops until the storage engine iterator finally reports
  that it reached the end of the scan.

  Note that the storage engine iterator has freedom to implement:
  - either a full table scan, returning all rows in a single call,
  - or a restartable scan, returning only a few rows in each call.

  The major benefit of this interface is that the engine iterator
  can stop and restart a scan at natural boundaries within the
  storage engine (say, return all the locks for one transaction per call),
  which simplifies a lot the storage engine implementation.
*/
/* clang-format on */

PFS_data_cache::PFS_data_cache() = default;

PFS_data_cache::~PFS_data_cache() = default;

const char *PFS_data_cache::cache_data(const char *ptr, size_t length) {
  /*
    std::string is just a sequence of bytes,
    which actually can contain a 0 byte ...
    Never use strlen() on the binary data.
  */
  const std::string key(ptr, length);
  std::pair<set_type::iterator, bool> ret;

  ret = m_set.insert(key);
  return (*ret.first).data();
}

void PFS_data_cache::clear() { m_set.clear(); }

PFS_data_lock_container::PFS_data_lock_container()
    : m_logical_row_index(0), m_filter(nullptr) {}

PFS_data_lock_container::~PFS_data_lock_container() = default;

const char *PFS_data_lock_container::cache_string(const char *string) {
  return m_cache.cache_data(string, strlen(string));
}

const char *PFS_data_lock_container::cache_data(const char *ptr,
                                                size_t length) {
  return m_cache.cache_data(ptr, length);
}

bool PFS_data_lock_container::accept_engine(const char *engine,
                                            size_t engine_length) {
  if (m_filter != nullptr) {
    return m_filter->match_engine(engine, engine_length);
  }
  return true;
}

bool PFS_data_lock_container::accept_lock_id(const char *engine_lock_id,
                                             size_t engine_lock_id_length) {
  if (m_filter != nullptr) {
    return m_filter->match_lock_id(engine_lock_id, engine_lock_id_length);
  }
  return true;
}

bool PFS_data_lock_container::accept_transaction_id(ulonglong transaction_id) {
  if (m_filter != nullptr) {
    return m_filter->match_transaction_id(transaction_id);
  }
  return true;
}

bool PFS_data_lock_container::accept_thread_id_event_id(ulonglong thread_id,
                                                        ulonglong event_id) {
  if (m_filter != nullptr) {
    return m_filter->match_thread_id_event_id(thread_id, event_id);
  }
  return true;
}

bool PFS_data_lock_container::accept_object(
    const char *table_schema, size_t table_schema_length,
    const char *table_name, size_t table_name_length,
    const char *partition_name, size_t partition_name_length,
    const char *sub_partition_name, size_t sub_partition_name_length) {
  if (m_filter != nullptr) {
    return m_filter->match_object(table_schema, table_schema_length, table_name,
                                  table_name_length, partition_name,
                                  partition_name_length, sub_partition_name,
                                  sub_partition_name_length);
  }
  return true;
}

void PFS_data_lock_container::add_lock_row(
    const char *engine, size_t engine_length [[maybe_unused]],
    const char *engine_lock_id, size_t engine_lock_id_length,
    ulonglong transaction_id, ulonglong thread_id, ulonglong event_id,
    const char *table_schema, size_t table_schema_length,
    const char *table_name, size_t table_name_length,
    const char *partition_name, size_t partition_name_length,
    const char *sub_partition_name, size_t sub_partition_name_length,
    const char *index_name, size_t index_name_length, const void *identity,
    const char *lock_mode, const char *lock_type, const char *lock_status,
    const char *lock_data) {
  row_data_lock row;

  row.m_engine = engine;

  if (engine_lock_id != nullptr) {
    size_t len = engine_lock_id_length;
    if (len > sizeof(row.m_hidden_pk.m_engine_lock_id)) {
      assert(false);
      len = sizeof(row.m_hidden_pk.m_engine_lock_id);
    }
    if (len > 0) {
      memcpy(row.m_hidden_pk.m_engine_lock_id, engine_lock_id, len);
    }
    row.m_hidden_pk.m_engine_lock_id_length = len;
  } else {
    row.m_hidden_pk.m_engine_lock_id_length = 0;
  }

  row.m_transaction_id = transaction_id;
  row.m_thread_id = thread_id;
  row.m_event_id = event_id;

  row.m_index_row.m_object_row.m_object_type = OBJECT_TYPE_TABLE;

  row.m_index_row.m_object_row.m_schema_name.set(table_schema,
                                                 table_schema_length);

  row.m_index_row.m_object_row.m_object_name.set_as_table(table_name,
                                                          table_name_length);

  row.m_partition_name = partition_name;
  row.m_partition_name_length = partition_name_length;

  row.m_sub_partition_name = sub_partition_name;
  row.m_sub_partition_name_length = sub_partition_name_length;

  if (index_name_length > 0) {
    memcpy(row.m_index_row.m_index_name, index_name, index_name_length);
  }
  row.m_index_row.m_index_name_length = index_name_length;

  row.m_identity = identity;
  row.m_lock_mode = lock_mode;
  row.m_lock_type = lock_type;
  row.m_lock_status = lock_status;
  row.m_lock_data = lock_data;

  m_rows.push_back(row);
}

void PFS_data_lock_container::clear() {
  m_logical_row_index = 0;
  m_rows.clear();
  m_cache.clear();
}

void PFS_data_lock_container::shrink() {
  /* Keep rows numbering. */
  m_logical_row_index += m_rows.size();
  /* Discard existing data. */
  m_rows.clear();
  m_cache.clear();
}

row_data_lock *PFS_data_lock_container::get_row(size_t index) {
  if (index < m_logical_row_index) {
    /*
      This row existed, before a call to ::shrink().
      The caller should not ask for it again.
    */
    assert(false);
    return nullptr;
  }

  const size_t physical_index = index - m_logical_row_index;

  if (physical_index < m_rows.size()) {
    return &m_rows[physical_index];
  }

  return nullptr;
}

PFS_data_lock_wait_container::PFS_data_lock_wait_container()
    : m_logical_row_index(0), m_filter(nullptr) {}

PFS_data_lock_wait_container::~PFS_data_lock_wait_container() = default;

const char *PFS_data_lock_wait_container::cache_string(const char *string) {
  return m_cache.cache_data(string, strlen(string));
}

const char *PFS_data_lock_wait_container::cache_data(const char *ptr,
                                                     size_t length) {
  return m_cache.cache_data(ptr, length);
}

bool PFS_data_lock_wait_container::accept_engine(const char *engine,
                                                 size_t engine_length) {
  if (m_filter != nullptr) {
    return m_filter->match_engine(engine, engine_length);
  }
  return true;
}

bool PFS_data_lock_wait_container::accept_requesting_lock_id(
    const char *engine_lock_id, size_t engine_lock_id_length) {
  if (m_filter != nullptr) {
    return m_filter->match_requesting_lock_id(engine_lock_id,
                                              engine_lock_id_length);
  }
  return true;
}

bool PFS_data_lock_wait_container::accept_blocking_lock_id(
    const char *engine_lock_id, size_t engine_lock_id_length) {
  if (m_filter != nullptr) {
    return m_filter->match_blocking_lock_id(engine_lock_id,
                                            engine_lock_id_length);
  }
  return true;
}

bool PFS_data_lock_wait_container::accept_requesting_transaction_id(
    ulonglong transaction_id) {
  if (m_filter != nullptr) {
    return m_filter->match_requesting_transaction_id(transaction_id);
  }
  return true;
}

bool PFS_data_lock_wait_container::accept_blocking_transaction_id(
    ulonglong transaction_id) {
  if (m_filter != nullptr) {
    return m_filter->match_blocking_transaction_id(transaction_id);
  }
  return true;
}

bool PFS_data_lock_wait_container::accept_requesting_thread_id_event_id(
    ulonglong thread_id, ulonglong event_id) {
  if (m_filter != nullptr) {
    return m_filter->match_requesting_thread_id_event_id(thread_id, event_id);
  }
  return true;
}

bool PFS_data_lock_wait_container::accept_blocking_thread_id_event_id(
    ulonglong thread_id, ulonglong event_id) {
  if (m_filter != nullptr) {
    return m_filter->match_blocking_thread_id_event_id(thread_id, event_id);
  }
  return true;
}

void PFS_data_lock_wait_container::add_lock_wait_row(
    const char *engine, size_t engine_length [[maybe_unused]],
    const char *requesting_engine_lock_id,
    size_t requesting_engine_lock_id_length,
    ulonglong requesting_transaction_id, ulonglong requesting_thread_id,
    ulonglong requesting_event_id, const void *requesting_identity,
    const char *blocking_engine_lock_id, size_t blocking_engine_lock_id_length,
    ulonglong blocking_transaction_id, ulonglong blocking_thread_id,
    ulonglong blocking_event_id, const void *blocking_identity) {
  row_data_lock_wait row;

  row.m_engine = engine;

  if (requesting_engine_lock_id != nullptr) {
    size_t len = requesting_engine_lock_id_length;
    if (len > sizeof(row.m_hidden_pk.m_requesting_engine_lock_id)) {
      assert(false);
      len = sizeof(row.m_hidden_pk.m_requesting_engine_lock_id);
    }
    if (len > 0) {
      memcpy(row.m_hidden_pk.m_requesting_engine_lock_id,
             requesting_engine_lock_id, len);
    }
    row.m_hidden_pk.m_requesting_engine_lock_id_length = len;
  } else {
    row.m_hidden_pk.m_requesting_engine_lock_id_length = 0;
  }

  row.m_requesting_transaction_id = requesting_transaction_id;
  row.m_requesting_thread_id = requesting_thread_id;
  row.m_requesting_event_id = requesting_event_id;
  row.m_requesting_identity = requesting_identity;

  if (blocking_engine_lock_id != nullptr) {
    size_t len = blocking_engine_lock_id_length;
    if (len > sizeof(row.m_hidden_pk.m_blocking_engine_lock_id)) {
      assert(false);
      len = sizeof(row.m_hidden_pk.m_blocking_engine_lock_id);
    }
    if (len > 0) {
      memcpy(row.m_hidden_pk.m_blocking_engine_lock_id, blocking_engine_lock_id,
             len);
    }
    row.m_hidden_pk.m_blocking_engine_lock_id_length = len;
  } else {
    row.m_hidden_pk.m_blocking_engine_lock_id_length = 0;
  }

  row.m_blocking_transaction_id = blocking_transaction_id;
  row.m_blocking_thread_id = blocking_thread_id;
  row.m_blocking_event_id = blocking_event_id;
  row.m_blocking_identity = blocking_identity;

  m_rows.push_back(row);
}

void PFS_data_lock_wait_container::clear() {
  m_logical_row_index = 0;
  m_rows.clear();
  m_cache.clear();
}

void PFS_data_lock_wait_container::shrink() {
  /* Keep rows numbering. */
  m_logical_row_index += m_rows.size();
  /* Discard existing data. */
  m_rows.clear();
  m_cache.clear();
}

row_data_lock_wait *PFS_data_lock_wait_container::get_row(size_t index) {
  if (index < m_logical_row_index) {
    /*
      This row existed, before a call to ::shrink().
      The caller should not ask for it again.
    */
    assert(false);
    return nullptr;
  }

  const size_t physical_index = index - m_logical_row_index;

  if (physical_index < m_rows.size()) {
    return &m_rows[physical_index];
  }

  return nullptr;
}
