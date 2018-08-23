/*****************************************************************************

Copyright (c) 2016, 2018, Oracle and/or its affiliates. All Rights Reserved.

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

/** @file handler/p_s.cc
 InnoDB performance_schema tables interface to MySQL.

 *******************************************************/

#include "storage/innobase/handler/p_s.h"

#include <stdlib.h>
#include <sys/types.h>

#include "lock0iter.h"
#include "lock0lock.h"
#include "sql_table.h"
#include "table.h"
#include "trx0i_s.h"
#include "trx0sys.h"

#include "my_io.h"

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
  - Take all necessary locks
  - Resume the scan on innodb internal locks,
  for a given record range
  - Report all the records in the range to the performance schema
  - Release all the locks taken

  This is a compromise, with the following properties:
  - Memory consumption is bounded,
    by the number of records returned in each range.
  - The duration of mutex locks on innodb structures is bounded
    by the number of records in each range
  - The data returned is not consistent,
    but at least it is "consistent by chunks"
  - The overall scan complexity is (N/RANGE)^2, where RANGE is the range size.
  This is still technically O(N^2), but in practice should be reasonable.

  For example with N = 10,000 transactions and RANGE = 256,
  there are 40 batches at the trx list,
  where each batch reports (up to) 256 trx, with the trx locks.
  The total number of operations on the list is 400 thousands.
*/

static const char *g_engine = "INNODB";
static const size_t g_engine_length = 6;

inline trx_t *get_next_trx(const trx_t *trx, bool read_write) {
  if (read_write) {
    return (UT_LIST_GET_NEXT(trx_list, trx));
  } else {
    return (UT_LIST_GET_NEXT(mysql_trx_list, trx));
  }
}

/** Pass of a given scan. */
enum scan_pass {
  INIT_SCANNING,
  /** Scan the RW trx list.
  @sa trx_sys_t::rw_trx_list
  */
  SCANNING_RW_TRX_LIST,
  /** Scan the MySQL trx list.
  @sa trx_t::mysql_trx_list
  */
  SCANNING_MYSQL_TRX_LIST,
  DONE_SCANNING
};

/** State of a given scan.
Scans are restartable, and done in multiple calls.
Overall, the code scans separately:
- the RW trx list
- the MySQL trx list
For each list, the scan is done by ranges of trx_id values.
Saving the current scan state allows to resume where the previous
scan ended.
*/
class Innodb_trx_scan_state {
 public:
  const trx_id_t SCAN_RANGE = 256;

  Innodb_trx_scan_state()
      : m_scan_pass(INIT_SCANNING),
        m_start_trx_id_range(0),
        m_end_trx_id_range(SCAN_RANGE),
        m_next_trx_id_range(TRX_ID_MAX) {}

  ~Innodb_trx_scan_state() {}

  scan_pass get_pass() { return m_scan_pass; }

  /** Prepare the next scan.
  When there are TRX after the current range,
  compute the next range.
  When there are no more TRX for this pass,
  advance to the next pass.
  */
  void prepare_next_scan() {
    if (m_next_trx_id_range != TRX_ID_MAX) {
      m_start_trx_id_range =
          m_next_trx_id_range - (m_next_trx_id_range % SCAN_RANGE);
      m_end_trx_id_range = m_start_trx_id_range + SCAN_RANGE;
      m_next_trx_id_range = TRX_ID_MAX;
    } else {
      switch (m_scan_pass) {
        case INIT_SCANNING:
          m_scan_pass = SCANNING_RW_TRX_LIST;
          m_start_trx_id_range = 0;
          m_end_trx_id_range = SCAN_RANGE;
          m_next_trx_id_range = TRX_ID_MAX;
          break;
        case SCANNING_RW_TRX_LIST:
          m_scan_pass = SCANNING_MYSQL_TRX_LIST;
          m_start_trx_id_range = 0;
          m_end_trx_id_range = SCAN_RANGE;
          m_next_trx_id_range = TRX_ID_MAX;
          break;
        case SCANNING_MYSQL_TRX_LIST:
          m_scan_pass = DONE_SCANNING;
          break;
        case DONE_SCANNING:
        default:
          ut_error;
          break;
      }
    }
  }

  /** Check if a transaction belongs to the current range.
  As a side effect, compute the next range.
  @param[in] trx_id	Transaction id to evaluate
  @return True if transaction is within range.
  */
  bool trx_id_in_range(trx_id_t trx_id) {
    ut_ad(trx_id < TRX_ID_MAX);

    if ((m_start_trx_id_range <= trx_id) && (trx_id < m_end_trx_id_range)) {
      return true;
    }

    if ((m_end_trx_id_range <= trx_id) && (trx_id < m_next_trx_id_range)) {
      m_next_trx_id_range = trx_id;
    }

    return false;
  }

 private:
  /** Current scan pass. */
  scan_pass m_scan_pass;
  /** Start of the current range. */
  trx_id_t m_start_trx_id_range;
  /** End of the current range. */
  trx_id_t m_end_trx_id_range;
  /** Next range. */
  trx_id_t m_next_trx_id_range;
};

/** Inspect data locks for the innodb storage engine. */
class Innodb_data_lock_iterator : public PSI_engine_data_lock_iterator {
 public:
  Innodb_data_lock_iterator();
  ~Innodb_data_lock_iterator();

  virtual bool scan(PSI_server_data_lock_container *container,
                    bool with_lock_data);

  virtual bool fetch(PSI_server_data_lock_container *container,
                     const char *engine_lock_id, size_t engine_lock_id_length,
                     bool with_lock_data);

 private:
  /** Scan a trx list.
  @param[in] container		The container to fill
  @param[in] with_lock_data	True if column LOCK_DATA
  needs to be populated.
  @param[in] read_write		True if the trx list is the RW list
  @param[in] trx_list		The trx list to scan
  @returns The number of records found
  */
  size_t scan_trx_list(PSI_server_data_lock_container *container,
                       bool with_lock_data, bool read_write,
                       trx_ut_list_t *trx_list);

  /** Scan a given trx.
  Either scan all the locks for a transaction,
  or scan only records matching a given lock.
  @param[in] container		The container to fill
  @param[in] with_lock_data	True if column LOCK_DATA
  needs to be populated.
  @param[in] trx			The trx to scan
  @param[in] with_filter		True if looking for a specific record
  only.
  @param[in] filter_lock_immutable_id     Immutable id of lock_t we are looking
  for
  @param[in] filter_heap_id	Heap id to look for, when filtering
  @returns The number of records found
  */
  size_t scan_trx(PSI_server_data_lock_container *container,
                  bool with_lock_data, const trx_t *trx, bool with_filter,
                  uint64_t filter_lock_immutable_id = 0,
                  ulint filter_heap_id = 0);

  /** Current scan state. */
  Innodb_trx_scan_state m_scan_state;
};

/** Inspect data lock waits for the innodb storage engine. */
class Innodb_data_lock_wait_iterator
    : public PSI_engine_data_lock_wait_iterator {
 public:
  Innodb_data_lock_wait_iterator();
  ~Innodb_data_lock_wait_iterator();

  virtual bool scan(PSI_server_data_lock_wait_container *container);

  virtual bool fetch(PSI_server_data_lock_wait_container *container,
                     const char *requesting_engine_lock_id,
                     size_t requesting_engine_lock_id_length,
                     const char *blocking_engine_lock_id,
                     size_t blocking_engine_lock_id_length);

 private:
  /** Scan a given transaction list.
  @param[in] container		The container to fill
  @param[in] read_write		True if the transaction list is the RW list.
  @param[in] trx_list		The trx list to scan
  @returns the number of records found.
  */
  size_t scan_trx_list(PSI_server_data_lock_wait_container *container,
                       bool read_write, trx_ut_list_t *trx_list);

  /** Scan a given transaction.
  Either scan all the waits for a transaction,
  or scan only records matching a given wait.
  @param[in] container		          The container to fill
  @param[in] trx		          The trx to scan
  @param[in] with_filter		  True if looking for a given wait only.
  @param[in] filter_requesting_lock_immutable_id  Immutable id of lock_t for
  the requesting lock, when filtering
  @param[in] filter_blocking_lock_immutable_id	  Immutable id of lock_t
  for the blocking lock, when filtering
  @returns the number of records found.
  */
  size_t scan_trx(PSI_server_data_lock_wait_container *container,
                  const trx_t *trx, bool with_filter,
                  uint64_t filter_requesting_lock_immutable_id = 0,
                  uint64_t filter_blocking_lock_immutable_id = 0);

  /** Current scan state. */
  Innodb_trx_scan_state m_scan_state;
};

/** Check if a transaction should be discarded.
Transactions present in any TRX LIST that have not started yet
are discarded, when inspecting data locks.
Transactions present in the MySQL TRX LIST,
that are writing data and have an id, are also discarded.
@param[in] trx Transaction to evaluate
@param[in] read_write True if trx is in the RW TRX list
@returns True if the trx should be discarded
*/
bool discard_trx(const trx_t *trx, bool read_write) {
  if (!trx_is_started(trx)) {
    return true;
  }

  if ((!read_write && trx->id != 0 && !trx->read_only)) {
    return true;
  }

  return false;
}

/** Find a transaction in a TRX LIST.
@param[in] filter_trx_immutable_id  The transaction immutable id
@param[in] read_write	            True for the RW TRX LIST
@param[in] trx_list	            The transaction list
@returns The transaction when found, or NULL
*/
static const trx_t *fetch_trx_in_trx_list(uint64_t filter_trx_immutable_id,
                                          bool read_write,
                                          trx_ut_list_t *trx_list) {
  const trx_t *trx;

  ut_ad(lock_mutex_own());
  ut_ad(trx_sys_mutex_own());

  for (trx = UT_LIST_GET_FIRST(*trx_list); trx != NULL;
       trx = get_next_trx(trx, read_write)) {
    if (discard_trx(trx, read_write)) {
      continue;
    }

    if (filter_trx_immutable_id == trx_immutable_id(trx)) {
      return trx;
    }
  }

  return NULL;
}

Innodb_data_lock_inspector::Innodb_data_lock_inspector() {}

Innodb_data_lock_inspector::~Innodb_data_lock_inspector() {}

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

/** Convert an identifier.
Convert identifiers stored in innodb to the proper
character set, and allocate memory for them in the
performance schema container.
@param[in] container		The container to fill
@param[in] str			The identifier string
@param[in] length		The identifier string length
@param[out] converted_length	The length of the converted string
@returns A string in UTF8, allocated in the performance schema container.
*/
const char *convert_identifier(PSI_server_data_lock_container *container,
                               const char *str, size_t length,
                               size_t *converted_length) {
  if (str == NULL) {
    *converted_length = 0;
    return NULL;
  }

  const char *result_string;
  size_t result_length;
  char buffer[FN_REFLEN];
  uint err_cs = 0;

  result_length = my_convert(buffer, sizeof(buffer), system_charset_info, str,
                             length, &my_charset_filename, &err_cs);

  ut_ad(err_cs == 0);

  result_string = container->cache_data(buffer, result_length);
  *converted_length = result_length;
  return result_string;
}

/** Parse a table path string.
Isolate the table schema, name, partition and sub partition
from a table path string.
Convert these strings and store them in the performance schema container.
@note String returned are not zero terminated.
@param[in] container			The container to fill
@param[in] table_path			The table path string
@param[in] table_path_length		The table path string length
@param[out] table_schema		The table schema
@param[out] table_schema_length		The table schema length
@param[out] table_name			The table name
@param[out] table_name_length		The table name length
@param[out] partition_name		Partition name
@param[out] partition_name_length	Partition name length
@param[out] subpartition_name		Sub partition name
@param[out] subpartition_name_length	Sub partition name length
*/
void parse_table_path(PSI_server_data_lock_container *container,
                      const char *table_path, size_t table_path_length,
                      const char **table_schema, size_t *table_schema_length,
                      const char **table_name, size_t *table_name_length,
                      const char **partition_name,
                      size_t *partition_name_length,
                      const char **subpartition_name,
                      size_t *subpartition_name_length) {
  const char *p1;
  size_t s1;
  const char *p2;
  size_t s2;
  const char *p3;
  size_t s3;
  const char *p4;
  size_t s4;

  parse_filename(table_path, table_path_length, &p1, &s1, &p2, &s2, &p3, &s3,
                 &p4, &s4);

  *table_schema = convert_identifier(container, p1, s1, table_schema_length);

  *table_name = convert_identifier(container, p2, s2, table_name_length);

  *partition_name =
      convert_identifier(container, p3, s3, partition_name_length);

  *subpartition_name =
      convert_identifier(container, p4, s4, subpartition_name_length);
}

/** Print a table lock id.
@param[in]	lock		The lock to print
@param[in,out]	lock_id		Printing buffer
@param[in]	lock_id_size	Printing buffer length
*/
void print_table_lock_id(const lock_t *lock, char *lock_id,
                         size_t lock_id_size) {
  /* We try to be backward compatible with INFORMATION_SCHEMA so that one can
  join with INFORMATION_SCHEMA.innodb_trx.trx_requested_lock_id column */
  i_s_locks_row_t row;
  fill_locks_row(&row, lock, ULINT_UNDEFINED);
  trx_i_s_create_lock_id(&row, lock_id, lock_id_size);
}

/** Print a record lock id.
@param[in]	lock		The lock to print
@param[in]	heap_no		Lock heap number
@param[in,out]	lock_id		Printing buffer
@param[in]	lock_id_size	Printing buffer length
*/
void print_record_lock_id(const lock_t *lock, ulint heap_no, char *lock_id,
                          size_t lock_id_size) {
  /* We try to be backward compatible with INFORMATION_SCHEMA so that one can
  join with INFORMATION_SCHEMA.innodb_trx.trx_requested_lock_id column */
  i_s_locks_row_t row;
  fill_locks_row(&row, lock, heap_no);
  trx_i_s_create_lock_id(&row, lock_id, lock_id_size);
}

/** Print a lock id.
@param[in]	lock		The lock to print
@param[in]	heap_no		Lock heap number
@param[in,out]	lock_id		Printing buffer
@param[in]	lock_id_size	Printing buffer length
*/
void print_lock_id(const lock_t *lock, ulint heap_no, char *lock_id,
                   size_t lock_id_size) {
  switch (lock_get_type(lock)) {
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

/** Scan a lock id string and extract information necessary to find a row
by primary key.
@param[in] 	lock_id		        The lock id string to parse
@param[out]	trx_immutable_id	The immutable id of lock->trx
@param[out]	lock_immutable_id       The immutable id of lock
@param[out]	heap_id		        The heap number found, for record locks
@returns The type of lock found.
@retval LOCK_TABLE	Table lock
@retval LOCK_REC	Record lock
@retval 0		Format error
*/
int scan_lock_id(const char *lock_id, ulint *trx_immutable_id,
                 uint64_t *lock_immutable_id, ulint *heap_id) {
  i_s_locks_row_t row;
  int lock_type = trx_i_s_parse_lock_id(lock_id, &row);
  if (!lock_type) {
    return 0;
  }
  *trx_immutable_id = row.lock_trx_immutable_id;
  *lock_immutable_id = row.lock_immutable_id;
  if (lock_type == LOCK_REC) {
    *heap_id = row.lock_rec;
  }
  return lock_type;
}

Innodb_data_lock_iterator::Innodb_data_lock_iterator() {}

Innodb_data_lock_iterator::~Innodb_data_lock_iterator() {}

bool Innodb_data_lock_iterator::scan(PSI_server_data_lock_container *container,
                                     bool with_lock_data) {
  if (m_scan_state.get_pass() == INIT_SCANNING) {
    if (!container->accept_engine(g_engine, g_engine_length)) {
      return true;
    }

    m_scan_state.prepare_next_scan();
  }

  if (m_scan_state.get_pass() == DONE_SCANNING) {
    return true;
  }

  lock_mutex_enter();

  trx_sys_mutex_enter();

  size_t found = 0;

  while ((m_scan_state.get_pass() == SCANNING_RW_TRX_LIST) && (found == 0)) {
    found =
        scan_trx_list(container, with_lock_data, true, &trx_sys->rw_trx_list);
    m_scan_state.prepare_next_scan();
  }

  while ((m_scan_state.get_pass() == SCANNING_MYSQL_TRX_LIST) && (found == 0)) {
    found = scan_trx_list(container, with_lock_data, false,
                          &trx_sys->mysql_trx_list);
    m_scan_state.prepare_next_scan();
  }

  trx_sys_mutex_exit();

  lock_mutex_exit();

  return false;
}

bool Innodb_data_lock_iterator::fetch(PSI_server_data_lock_container *container,
                                      const char *engine_lock_id,
                                      size_t engine_lock_id_length,
                                      bool with_lock_data) {
  int record_type;
  uint64_t trx_immutable_id;
  ulint heap_id;
  uint64_t lock_immutable_id;
  const trx_t *trx;

  if (!container->accept_engine(g_engine, g_engine_length)) {
    return true;
  }

  record_type = scan_lock_id(engine_lock_id, &trx_immutable_id,
                             &lock_immutable_id, &heap_id);

  if (record_type == 0) {
    return true;
  }

  lock_mutex_enter();

  trx_sys_mutex_enter();

  trx = fetch_trx_in_trx_list(trx_immutable_id, true, &trx_sys->rw_trx_list);

  if (trx == NULL) {
    trx = fetch_trx_in_trx_list(trx_immutable_id, false,
                                &trx_sys->mysql_trx_list);
  }

  if (trx != NULL) {
    scan_trx(container, with_lock_data, trx, true, lock_immutable_id, heap_id);
  }

  trx_sys_mutex_exit();

  lock_mutex_exit();

  return true;
}

/** Scan a trx list.
@param[in] container		The container to fill
@param[in] with_lock_data	True if column LOCK_DATA
needs to be populated.
@param[in] read_write		True if the trx list is the RW list
@param[in] trx_list		The trx list to scan
@returns The number of records found
*/
size_t Innodb_data_lock_iterator::scan_trx_list(
    PSI_server_data_lock_container *container, bool with_lock_data,
    bool read_write, trx_ut_list_t *trx_list) {
  const trx_t *trx;
  trx_id_t trx_id;
  size_t found = 0;

  ut_ad(lock_mutex_own());
  ut_ad(trx_sys_mutex_own());

  for (trx = UT_LIST_GET_FIRST(*trx_list); trx != NULL;
       trx = get_next_trx(trx, read_write)) {
    if (discard_trx(trx, read_write)) {
      continue;
    }

    trx_id = trx_get_id_for_print(trx);

    if (!m_scan_state.trx_id_in_range(trx_id)) {
      continue;
    }

    found += scan_trx(container, with_lock_data, trx, false);
  }

  return found;
}

/** Scan a given trx.
Either scan all the locks for a transaction,
or scan only records matching a given lock.
@param[in] container		      The container to fill
@param[in] with_lock_data	      True if column LOCK_DATA
needs to be populated.
@param[in] trx			      The trx to scan
@param[in] with_filter		      True if looking for a specific
record only.
@param[in] filter_lock_immutable_id   Immutable id of lock_t we are looking for
@param[in] filter_heap_id	      Heap id to look for, when filtering
@returns The number of records found
*/
size_t Innodb_data_lock_iterator::scan_trx(
    PSI_server_data_lock_container *container, bool with_lock_data,
    const trx_t *trx, bool with_filter, uint64_t filter_lock_immutable_id,
    ulint filter_heap_id) {
  assert_trx_nonlocking_or_in_list(trx);

  size_t found = 0;
  const lock_t *lock;
  ulonglong trx_id;
  ulonglong thread_id;
  ulonglong event_id;
  const char *table_path;
  const char *table_schema;
  size_t table_schema_length;
  const char *table_name;
  size_t table_name_length;
  const char *partition_name;
  size_t partition_name_length;
  const char *subpartition_name;
  size_t subpartition_name_length;
  const char *index_name;
  size_t index_name_length;
  const void *identity;
  const char *lock_mode_str;
  const char *lock_type_str;
  const char *lock_status_str = "GRANTED";
  const char *lock_data_str;
  char engine_lock_id[TRX_I_S_LOCK_ID_MAX_LEN + 1];
  size_t engine_lock_id_length;
  ulint heap_no;
  int record_type;
  lock_t *wait_lock;

  wait_lock = trx->lock.wait_lock;

  trx_id = trx_get_id_for_print(trx);

  if (!container->accept_transaction_id(trx_id)) {
    return 0;
  }

  for (lock = lock_get_first_trx_locks(&trx->lock); lock != NULL;
       lock = lock_get_next_trx_locks(lock)) {
    record_type = lock_get_type(lock);

    if (with_filter &&
        filter_lock_immutable_id != lock_get_immutable_id(lock)) {
      continue;
    }

    lock_get_psi_event(lock, &thread_id, &event_id);

    if (!container->accept_thread_id_event_id(thread_id, event_id)) {
      continue;
    }

    table_path = lock_get_table_name(lock).m_name;
    parse_table_path(container, table_path, strlen(table_path), &table_schema,
                     &table_schema_length, &table_name, &table_name_length,
                     &partition_name, &partition_name_length,
                     &subpartition_name, &subpartition_name_length);

    if (!container->accept_object(table_schema, table_schema_length, table_name,
                                  table_name_length, partition_name,
                                  partition_name_length, subpartition_name,
                                  subpartition_name_length)) {
      continue;
    }

    identity = lock;
    lock_mode_str = lock_get_mode_str(lock);
    lock_type_str = lock_get_type_str(lock);

    if (lock == wait_lock) {
      lock_status_str = "WAITING";
    }

    switch (record_type) {
      case LOCK_TABLE:
        print_table_lock_id(lock, engine_lock_id, sizeof(engine_lock_id));
        engine_lock_id_length = strlen(engine_lock_id);

        if (container->accept_lock_id(engine_lock_id, engine_lock_id_length)) {
          container->add_lock_row(
              g_engine, g_engine_length, engine_lock_id, engine_lock_id_length,
              trx_id, thread_id, event_id, table_schema, table_schema_length,
              table_name, table_name_length, partition_name,
              partition_name_length, subpartition_name,
              subpartition_name_length, NULL, 0, identity, lock_mode_str,
              lock_type_str, lock_status_str, NULL);
          found++;
        }
        break;
      case LOCK_REC:
        index_name = lock_rec_get_index_name(lock);
        index_name_length = strlen(index_name);

        heap_no = lock_rec_find_set_bit(lock);

        while (heap_no != ULINT_UNDEFINED) {
          if (!with_filter || (heap_no == filter_heap_id)) {
            print_record_lock_id(lock, heap_no, engine_lock_id,
                                 sizeof(engine_lock_id));
            engine_lock_id_length = strlen(engine_lock_id);

            if (container->accept_lock_id(engine_lock_id,
                                          engine_lock_id_length)) {
              if (with_lock_data) {
                p_s_fill_lock_data(&lock_data_str, lock, heap_no, container);
              } else {
                lock_data_str = NULL;
              }

              container->add_lock_row(
                  g_engine, g_engine_length, engine_lock_id,
                  engine_lock_id_length, trx_id, thread_id, event_id,
                  table_schema, table_schema_length, table_name,
                  table_name_length, partition_name, partition_name_length,
                  subpartition_name, subpartition_name_length, index_name,
                  index_name_length, identity, lock_mode_str, lock_type_str,
                  lock_status_str, lock_data_str);
              found++;
            }
          }

          heap_no = lock_rec_find_next_set_bit(lock, heap_no);
        }
        break;
      default:
        ut_error;
    }
  }

  return found;
}

Innodb_data_lock_wait_iterator::Innodb_data_lock_wait_iterator()

{}

Innodb_data_lock_wait_iterator::~Innodb_data_lock_wait_iterator() {}

bool Innodb_data_lock_wait_iterator::scan(
    PSI_server_data_lock_wait_container *container) {
  if (m_scan_state.get_pass() == INIT_SCANNING) {
    if (!container->accept_engine(g_engine, g_engine_length)) {
      return true;
    }

    m_scan_state.prepare_next_scan();
  }

  if (m_scan_state.get_pass() == DONE_SCANNING) {
    return true;
  }

  lock_mutex_enter();

  trx_sys_mutex_enter();

  size_t found = 0;

  while ((m_scan_state.get_pass() == SCANNING_RW_TRX_LIST) && (found == 0)) {
    found = scan_trx_list(container, true, &trx_sys->rw_trx_list);
    m_scan_state.prepare_next_scan();
  }

  while ((m_scan_state.get_pass() == SCANNING_MYSQL_TRX_LIST) && (found == 0)) {
    found = scan_trx_list(container, false, &trx_sys->mysql_trx_list);
    m_scan_state.prepare_next_scan();
  }

  trx_sys_mutex_exit();

  lock_mutex_exit();

  return false;
}

bool Innodb_data_lock_wait_iterator::fetch(
    PSI_server_data_lock_wait_container *container,
    const char *requesting_engine_lock_id,
    size_t requesting_engine_lock_id_length,
    const char *blocking_engine_lock_id,
    size_t blocking_engine_lock_id_length) {
  int requesting_record_type;
  uint64_t requesting_trx_immutable_id;
  ulint requesting_heap_id;
  uint64_t requesting_lock_immutable_id;
  int blocking_record_type;
  uint64_t blocking_trx_immutable_id;
  ulint blocking_heap_id;
  uint64_t blocking_lock_immutable_id;
  const trx_t *trx;

  if (!container->accept_engine(g_engine, g_engine_length)) {
    return true;
  }

  requesting_record_type =
      scan_lock_id(requesting_engine_lock_id, &requesting_trx_immutable_id,
                   &requesting_lock_immutable_id, &requesting_heap_id);

  if (requesting_record_type == 0) {
    return true;
  }

  blocking_record_type =
      scan_lock_id(blocking_engine_lock_id, &blocking_trx_immutable_id,
                   &blocking_lock_immutable_id, &blocking_heap_id);

  if (blocking_record_type == 0) {
    return true;
  }

  lock_mutex_enter();

  trx_sys_mutex_enter();

  trx = fetch_trx_in_trx_list(requesting_trx_immutable_id, true,
                              &trx_sys->rw_trx_list);

  if (trx == NULL) {
    trx = fetch_trx_in_trx_list(requesting_trx_immutable_id, false,
                                &trx_sys->mysql_trx_list);
  }

  if (trx != NULL) {
    scan_trx(container, trx, true, requesting_lock_immutable_id,
             blocking_lock_immutable_id);
  }

  trx_sys_mutex_exit();

  lock_mutex_exit();

  return true;
}

/** Scan a given transaction list.
@param[in] container		The container to fill
@param[in] read_write		True if the transaction list is the RW list.
@param[in] trx_list		The trx list to scan
@returns the number of records found.
*/
size_t Innodb_data_lock_wait_iterator::scan_trx_list(
    PSI_server_data_lock_wait_container *container, bool read_write,
    trx_ut_list_t *trx_list) {
  const trx_t *trx;
  trx_id_t trx_id;
  size_t found = 0;

  ut_ad(lock_mutex_own());
  ut_ad(trx_sys_mutex_own());

  for (trx = UT_LIST_GET_FIRST(*trx_list); trx != NULL;
       trx = get_next_trx(trx, read_write)) {
    if (discard_trx(trx, read_write)) {
      continue;
    }

    trx_id = trx_get_id_for_print(trx);

    if (!m_scan_state.trx_id_in_range(trx_id)) {
      continue;
    }

    found += scan_trx(container, trx, false);
  }

  return found;
}

/** Scan a given transaction.
Either scan all the waits for a transaction,
or scan only records matching a given wait.
@param[in] container		The container to fill
@param[in] trx			The trx to scan
@param[in] with_filter		True if looking for a given wait only.
@param[in] filter_requesting_lock_immutable_id		Immutable id of
lock_t for the requesting lock, when filtering
@param[in] filter_blocking_lock_immutable_id		Immutable idof
lock_t for the blocking lock, when filtering
@returns the number of records found.
*/
size_t Innodb_data_lock_wait_iterator::scan_trx(
    PSI_server_data_lock_wait_container *container, const trx_t *trx,
    bool with_filter, uint64_t filter_requesting_lock_immutable_id,
    uint64_t filter_blocking_lock_immutable_id) {
  assert_trx_nonlocking_or_in_list(trx);

  if (trx->lock.que_state != TRX_QUE_LOCK_WAIT) {
    return 0;
  }

  ulonglong requesting_trx_id;
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
  lock_t *wait_lock = trx->lock.wait_lock;
  const lock_t *curr_lock;
  int requesting_record_type;
  size_t found = 0;
  lock_queue_iterator_t iter;

  ut_a(wait_lock != NULL);

  requesting_record_type = lock_get_type(wait_lock);

  if (with_filter &&
      lock_get_immutable_id(wait_lock) != filter_requesting_lock_immutable_id) {
    return 0;
  }

  requesting_trx_id = trx_get_id_for_print(trx);
  if (!container->accept_requesting_transaction_id(requesting_trx_id)) {
    return 0;
  }

  lock_get_psi_event(wait_lock, &requesting_thread_id, &requesting_event_id);
  if (!container->accept_requesting_thread_id_event_id(requesting_thread_id,
                                                       requesting_event_id)) {
    return 0;
  }

  ulint heap_no = 0;
  if (requesting_record_type == LOCK_REC) {
    heap_no = lock_rec_find_set_bit(wait_lock);
  }

  print_lock_id(wait_lock, heap_no, requesting_engine_lock_id,
                sizeof(requesting_engine_lock_id));
  requesting_engine_lock_id_length = strlen(requesting_engine_lock_id);
  if (!container->accept_requesting_lock_id(requesting_engine_lock_id,
                                            requesting_engine_lock_id_length)) {
    return 0;
  }

  requesting_identity = wait_lock;
  lock_queue_iterator_reset(&iter, wait_lock, ULINT_UNDEFINED);

  for (curr_lock = lock_queue_iterator_get_prev(&iter); curr_lock != NULL;
       curr_lock = lock_queue_iterator_get_prev(&iter)) {
    if (with_filter &&
        lock_get_immutable_id(curr_lock) != filter_blocking_lock_immutable_id) {
      continue;
    }

    if (lock_has_to_wait(wait_lock, curr_lock)) {
      blocking_trx_id = lock_get_trx_id(curr_lock);
      if (!container->accept_blocking_transaction_id(blocking_trx_id)) {
        continue;
      }

      lock_get_psi_event(curr_lock, &blocking_thread_id, &blocking_event_id);
      if (!container->accept_blocking_thread_id_event_id(blocking_thread_id,
                                                         blocking_event_id)) {
        continue;
      }

      blocking_identity = curr_lock;
      print_lock_id(curr_lock, heap_no, blocking_engine_lock_id,
                    sizeof(blocking_engine_lock_id));
      blocking_engine_lock_id_length = strlen(blocking_engine_lock_id);
      if (!container->accept_blocking_lock_id(blocking_engine_lock_id,
                                              blocking_engine_lock_id_length)) {
        continue;
      }

      container->add_lock_wait_row(
          g_engine, g_engine_length, requesting_engine_lock_id,
          requesting_engine_lock_id_length, requesting_trx_id,
          requesting_thread_id, requesting_event_id, requesting_identity,
          blocking_engine_lock_id, blocking_engine_lock_id_length,
          blocking_trx_id, blocking_thread_id, blocking_event_id,
          blocking_identity);
      found++;
    }
  }

  return found;
}
