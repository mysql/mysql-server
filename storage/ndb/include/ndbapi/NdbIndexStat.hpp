/*
   Copyright (c) 2005, 2023, Oracle and/or its affiliates.

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

#ifndef NdbIndexStat_H
#define NdbIndexStat_H

#include <ndb_types.h>
#include "NdbDictionary.hpp"
#include "NdbError.hpp"
#include "NdbIndexScanOperation.hpp"
class NdbIndexStatImpl;

/*
 * Ordered index stats "v4".  Includes 1) the old records_in_range in
 * simplified form 2) the new scanned and stored stats.  These are
 * completely different.  1) makes a one-round-trip query directly to
 * the index while 2) reads more extensive stats from sys tables where
 * they were stored previously by NDB kernel.
 *
 * Methods in general return 0 on success and -1 on error.  The error
 * details are available via getNdbError().
 */

class NdbIndexStat {
public:
  NdbIndexStat();
  ~NdbIndexStat();

  /*
   * Get latest error.  Can be printed like any NdbError instance and
   * includes some extras.
   */
  struct Error : public NdbError {
    int line;  // source code line number
    int extra; // extra error code
    Error();
  };
  const Error& getNdbError() const;

  /*
   * Estimate how many records exist in given range.  Does a single
   * tree-dive on each index fragment, estimates the count from tree
   * properties, and sums up the results.
   *
   * Caller provides index and scan transaction and range bounds.
   * A scan operation is created and executed.  The result is returned
   * in out-parameter "count".  The result is not transactional.  Value
   * zero is exact (range was empty when checked).
   *
   * This is basically a static method.  The class instance is used only
   * to return errors.
   */
  int records_in_range(const NdbDictionary::Index* index,
                       NdbTransaction* trans,
                       const NdbRecord* key_record,
                       const NdbRecord* result_record,
                       const NdbIndexScanOperation::IndexBound* ib,
                       Uint64 table_rows, // not used
                       Uint64* count,
                       int flags); // not used

  /*
   * Methods for stored stats.
   *
   * There are two distinct users: 1) writer reads samples from sys
   * tables and creates a new query cache 2) readers make concurrent
   * stats queries on current query cache.
   *
   * Writer provides any Ndb object required.  Its database name must be
   * "mysql".  No reference to it is kept.
   *
   * Readers provide structs such as Bound on stack or in TLS.  The
   * structs are opaque.  With source code the structs can be cast to
   * NdbIndexStatImpl structs.
   */

  enum {
    InvalidKeySize = 911, // index has an unsupported key size
    NoSysTables = 4714,   // all sys tables missing
    NoIndexStats = 4715,  // given index has no stored stats
    UsageError = 4716,    // wrong state, invalid input
    NoMemError = 4717,
    InvalidCache = 4718,
    InternalError = 4719,
    BadSysTables = 4720,  // sys tables partly missing or invalid
    HaveSysTables = 4244, // create error if all sys tables exist
    NoSysEvents = 4710,
    BadSysEvents = BadSysTables,
    HaveSysEvents = 746,
    /*
     * Following are for mysqld.  Most are consumed by mysqld itself
     * and should therefore not be seen by clients.
     */
    MyNotAllow = 4721,    // stats thread not open for requests
    MyNotFound = 4722,    // stats entry unexpectedly not found
    MyHasError = 4723,    // request ignored due to recent error
    MyAbortReq = 4724,    // request aborted by stats thread
    AlienUpdate = 4725    // somebody else messed with stats
  };

  /*
   * Methods for sys tables.
   *
   * Create fails if any objects exist.  Specific errors are
   * BadSysTables (drop required) and HaveSysTables.
   *
   * Drop always succeeds and drops any objects that exist.
   *
   * Check succeeds if all correct objects exist.  Specific errors are
   * BadSysTables (drop required) and NoSysTables.
   *
   * Database of the Ndb object is used and must be "mysql" for kernel
   * to see the tables.
   */
  int create_systables(Ndb* ndb);
  int drop_systables(Ndb* ndb);
  int check_systables(Ndb* ndb);

  /*
   * Set index operated on.  Allocates internal structs.  Makes no
   * database access and keeps no references to the objects.
   */
  int set_index(const NdbDictionary::Index& index,
                const NdbDictionary::Table& table);

  /*
   * Release index.  Required only if re-used for another index.
   */
  void reset_index();

  /*
   * Trivial invocation of NdbDictionary::Dictionary::updateIndexStat.
   */
  int update_stat(Ndb* ndb);

  /*
   * Trivial invocation of NdbDictionary::Dictionary::deleteIndexStat.
   */
  int delete_stat(Ndb* ndb);

  /*
   * Cache types.
   */
  enum CacheType {
    CacheBuild = 1,     // new cache under construction
    CacheQuery = 2,     // cache used to answer queries
    CacheClean = 3      // old caches waiting to be deleted
  };

  /*
   * Move CacheQuery (if any) to CacheClean and CacheBuild (if any) to
   * CacheQuery.  The CacheQuery switch is atomic.
   */
  void move_cache();

  /*
   * Delete all CacheClean instances.  This can be safely done after old
   * cache queries have finished.  Cache queries are fast since they do
   * binary searches in memory.
   */
  void clean_cache();

  /*
   * Cache info.  CacheClean may have several instances and the values
   * for them are summed up.
   */
  struct CacheInfo {
    Uint32 m_count;       // number of instances
    Uint32 m_valid;       // should be except for incomplete CacheBuild
    Uint32 m_sampleCount; // number of samples
    Uint32 m_totalBytes;  // total bytes memory used
    Uint64 m_save_time;   // microseconds to read stats into cache
    Uint64 m_sort_time;   // microseconds to sort the cache
    Uint32 m_ref_count;   // in use by query_stat
    // end v4 fields
  };

  /*
   * Get info about a cache type.
   */
  void get_cache_info(CacheInfo& info, CacheType type) const;

  /*
   * Saved head record retrieved with get_head().  The database fields
   * are updated by any method which reads stats tables.  Stats exist if
   * sampleVersion is not zero.
   */
  struct Head {
    Int32 m_found;        // -1 no read done, 0 = no record, 1 = exists
    Int32 m_eventType;    // if polling, NdbDictionary::Event::TE_INSERT etc
    Uint32 m_indexId;
    Uint32 m_indexVersion;
    Uint32 m_tableId;
    Uint32 m_fragCount;
    Uint32 m_valueFormat;
    Uint32 m_sampleVersion;
    Uint32 m_loadTime;
    Uint32 m_sampleCount;
    Uint32 m_keyBytes;
    // end v4 fields
  };

  /*
   * Get latest saved head record.  Makes no database access.
   */
  void get_head(Head& head) const;

  /*
   * Read stats head record for the index.  Returns error and sets code
   * to NoIndexStats if head record does not exist or sample version is
   * zero.  Use get_head() to retrieve the results.
   */
  int read_head(Ndb* ndb);

  /*
   * Read current version of stats into CacheBuild.  A move_cache() is
   * required before it is available for queries.
   */
  int read_stat(Ndb* ndb);

  /*
   * Reader provides bounds for cache query.  The struct must be
   * initialized from a thread-local byte buffer of the given size.
   * NdbIndexStat instance is used and must have index set.  Note that
   * a bound becomes low or high only as part of Range.
   */
  enum { BoundBufferBytes = 8192 };
  struct Bound {
    Bound(const NdbIndexStat* is, void* buffer);
    void* m_impl;
  };

  /*
   * Add non-NULL attribute value to the bound.  May return error for
   * invalid data.
   */
  int add_bound(Bound& bound, const void* value);

  /*
   * Add NULL attribute value to the bound.
   */
  int add_bound_null(Bound& bound);

  /*
   * A non-empty bound must be set strict (true) or non-strict (false).
   * For empty bound this must remain unset (-1).
   */
  void set_bound_strict(Bound& bound, int strict);

  /*
   * To re-use same bound instance, a reset is required.
   */
  void reset_bound(Bound& bound);

  /*
   * Queries take a range consisting of low and high bound (start key
   * and end key in mysql).
   */
  struct Range {
    Range(Bound& bound1, Bound& bound2);
    Bound& m_bound1;
    Bound& m_bound2;
  };

  /*
   * After defining bounds, the range must be finalized.  This updates
   * internal info.  Usage error is possible.
   */
  int finalize_range(Range& range);

  /*
   * Reset the bounds.
   */
  void reset_range(Range& range);

  /*
   * Convert NdbRecord index bound to Range.  Invokes reset and finalize
   * and cannot be mixed with the other methods.
   */
  int convert_range(Range& range,
                    const NdbRecord* key_record,
                    const NdbIndexScanOperation::IndexBound* ib);

  /*
   * Reader provides storage for stats values.  The struct must be
   * initialized from a thread-local byte buffer of the given size.
   */
  enum { StatBufferBytes = 2048 };
  struct Stat {
    Stat(void* buffer);
    void* m_impl;
  };

  /*
   * Compute Stat for a Range from the query cache.  Returns error
   * if there is no valid query cache.  The Stat is used to get
   * stats values without further reference to the Range.
   */
  int query_stat(const Range& range, Stat& stat);

  /*
   * Check if range is empty i.e. bound1 >= bound2 (for bounds this
   * means empty) or the query cache is empty.  The RIR and RPK return
   * 1.0 if range is empty.
   */
  static void get_empty(const Stat& stat, bool* empty);

  /*
   * Get number of rows the statistcs is sampled over.
   * Could be used as a metric for the quality of the statistic.
   */
  static void get_numrows(const Stat& stat, Uint32* rows);

  /*
   * Get estimated RIR (records in range).  Value is always >= 1.0 since
   * no exact 0 rows can be returned.
   */
  static void get_rir(const Stat& stat, double* rir);

  /*
   * Get estimated RPK (records per key) at given level k (from 0 to
   * NK-1 where NK = number of index keys).  Value is >= 1.0.
   */
  static void get_rpk(const Stat& stat,
                      Uint32 k,
                      double* rpk);
  /*
   * Similar as above, with the range being 'pruned' to a single
   * fragment due the entire partitioned key being specified.
   */
  static void get_rpk_pruned(const Stat& stat,
                             Uint32 k,
                             double* rpk);

  /*
   * Get a short string summarizing the rules used.
   */
  enum { RuleBufferBytes = 80 };
  static void get_rule(const Stat& stat, char* buffer);

  /*
   * Events (there is 1) for polling.  These are dictionary objects.
   * Correct sys tables must exist.  Drop ignores non-existing events.
   */
  int create_sysevents(Ndb* ndb);
  int drop_sysevents(Ndb* ndb);
  int check_sysevents(Ndb* ndb);

  /*
   * Create listener for stats updates.  Only 1 is allowed.
   */
  int create_listener(Ndb* ndb);

  /*
   * Check if the listener has been created.
   */
  bool has_listener() const;

  /*
   * Start listening for events (call NdbEventOperation::execute).
   */
  int execute_listener(Ndb* ndb);

  /*
   * Poll the listener (call Ndb::pollEvents).  Returns 1 if there are
   * events available and 0 otherwise, or -1 on failure as usual.
   */
  int poll_listener(Ndb* ndb, int max_wait_ms);

  /*
   * Get next available event.  Returns 1 if a new event was returned
   * and 0 otherwise, or -1 on failure as usual.  Use get_heed() to
   * retrieve event type and data.
   */
  int next_listener(Ndb* ndb);

  /*
   * Drop the listener if it exists.  Always succeeds.
   */
  int drop_listener(Ndb* ndb);

  /*
   * Memory allocator for stats cache data (key and value byte arrays).
   * Implementation default uses malloc/free.  The memory in use is the
   * sum of CacheInfo::m_totalBytes from all cache types.
   */
  struct Mem {
    Mem();
    virtual ~Mem();
    virtual void* mem_alloc(UintPtr size) = 0;
    virtual void mem_free(void* ptr) = 0;
  };

  /*
   * Set a non-default memory allocator.
   */
  void set_mem_handler(Mem* mem);

  // get impl class for use in NDB API programs
  NdbIndexStatImpl& getImpl();

private:
  int addKeyPartInfo(const NdbRecord* record,
                     const char* keyRecordData,
                     Uint32 keyPartNum,
                     const NdbIndexScanOperation::BoundType boundType,
                     Uint32* keyStatData,
                     Uint32& keyLength);

  // stored stats

  friend class NdbIndexStatImpl;
  NdbIndexStat(NdbIndexStatImpl& impl);
  NdbIndexStatImpl& m_impl;
};

class NdbOut&
operator<<(class NdbOut& out, const NdbIndexStat::Error&);

#endif
