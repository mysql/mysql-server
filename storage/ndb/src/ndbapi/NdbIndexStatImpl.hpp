/* Copyright (c) 2011, 2023, Oracle and/or its affiliates.

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

#ifndef NDB_INDEX_STAT_IMPL_HPP
#define NDB_INDEX_STAT_IMPL_HPP

#include <ndb_global.h>
#include <ndb_limits.h>
#include <NdbDictionary.hpp>
#include <NdbIndexStat.hpp>
#include <util/NdbPack.hpp>
#include <NdbError.hpp>
#include <NdbMutex.h>
#include <NdbTick.h>
class Ndb;
class NdbTransaction;
class NdbIndexScanOperation;
class NdbRecAttr;
class NdbOperation;
class NdbEventOperation;

class NdbIndexStatImpl : public NdbIndexStat {
public:
  friend class NdbIndexStat;
  struct Con;
  struct Cache;

  enum { MaxKeyCount = MAX_INDEX_STAT_KEY_COUNT };
  enum { MaxKeyBytes = MAX_INDEX_STAT_KEY_SIZE * 4 };
  enum { MaxValueBytes = MAX_INDEX_STAT_VALUE_SIZE * 4 };
  enum { MaxValueCBytes = MAX_INDEX_STAT_VALUE_CSIZE * 4 };

  NdbIndexStatImpl(NdbIndexStat& facade);
  ~NdbIndexStatImpl();
  void init();

  NdbIndexStat* const m_facade;
  Head m_facadeHead; // owned by facade
  bool m_indexSet;
  Uint32 m_indexId;
  Uint32 m_indexVersion;
  Uint32 m_tableId;
  uint m_keyAttrs;
  uint m_valueAttrs;
  NdbPack::Spec m_keySpec;
  NdbPack::Spec m_valueSpec;
  NdbPack::Type* m_keySpecBuf;
  NdbPack::Type* m_valueSpecBuf;
  NdbPack::Data m_keyData;
  NdbPack::Data m_valueData;
  Uint8* m_keyDataBuf;
  Uint8* m_valueDataBuf;
  Cache* m_cacheBuild;
  Cache* m_cacheQuery;
  Cache* m_cacheClean;
  // mutex for reference count
  NdbMutex* m_query_mutex;
  NdbEventOperation* m_eventOp;
  Mem* m_mem_handler;
  // Allow update error from const methods
  mutable NdbIndexStat::Error m_error;

  // sys tables meta
  struct Sys {
    NdbIndexStatImpl* const m_impl;
    Ndb* const m_ndb;
    NdbDictionary::Dictionary* m_dic;
    const NdbDictionary::Table* m_headtable;
    const NdbDictionary::Table* m_sampletable;
    const NdbDictionary::Index* m_sampleindex1;
    int m_obj_cnt;
    enum { ObjCnt = 3 };
    Sys(NdbIndexStatImpl* impl, Ndb* ndb);
    ~Sys();
  };
  void sys_release(Sys& sys);
  int make_headtable(NdbDictionary::Table& tab);
  int make_sampletable(NdbDictionary::Table& tab);
  int make_sampleindex1(NdbDictionary::Index& ind);
  int check_table(const NdbDictionary::Table& tab1,
                  const NdbDictionary::Table& tab2);
  int check_index(const NdbDictionary::Index& ind1,
                  const NdbDictionary::Index& ind2);
  int get_systables(Sys& sys);
  int create_systables(Ndb* ndb);
  int drop_systables(Ndb* ndb);
  int check_systables(Sys& sys);
  int check_systables(Ndb* ndb);

  // operation context
  struct Con {
    NdbIndexStatImpl* const m_impl;
    Head& m_head;
    Ndb* const m_ndb;
    NdbDictionary::Dictionary* m_dic;
    const NdbDictionary::Table* m_headtable;
    const NdbDictionary::Table* m_sampletable;
    const NdbDictionary::Index* m_sampleindex1;
    NdbTransaction* m_tx;
    NdbOperation* m_op;
    NdbIndexScanOperation* m_scanop;
    Cache* m_cacheBuild;
    uint m_cachePos;
    uint m_cacheKeyOffset;   // in bytes
    uint m_cacheValueOffset; // in bytes
    NDB_TICKS m_start;
    Con(NdbIndexStatImpl* impl, Head& head, Ndb* ndb);
    ~Con();
    int startTransaction();
    int execute(bool commit);
    int getNdbOperation();
    int getNdbIndexScanOperation();
    void set_time();
    Uint64 get_time(); //Elapsed time(us) since set_time
  };

  // index
  int set_index(const NdbDictionary::Index& index,
                const NdbDictionary::Table& table);
  void reset_index();

  // init m_facadeHead here (keep API struct a POD)
  void init_head(Head& head);

  // sys tables data
  int sys_init(Con& con);
  void sys_release(Con& con);
  int sys_read_head(Con& con, bool commit);
  int sys_head_setkey(Con& con);
  int sys_head_getvalue(Con& con);
  int sys_sample_setkey(Con& con);
  int sys_sample_getvalue(Con& con);
  int sys_sample_setbound(Con& con, int sv_bound);

  // update, delete (head may record elapsed time)
  int update_stat(Ndb* ndb, Head& head);
  int delete_stat(Ndb* ndb, Head& head);

  // read
  int read_head(Ndb* ndb, Head& head);
  int read_stat(Ndb* ndb, Head& head);
  int read_start(Con& con);
  int read_next(Con& con);
  int read_commit(Con& con);
  int save_start(Con& con);
  int save_next(Con& con);
  int save_commit(Con& con);
  int cache_init(Con& con);
  int cache_insert(Con& con);
  int cache_commit(Con& con);

  // cache
  struct Cache {
    bool m_valid;
    uint m_keyAttrs;      // number of attrs in index key
    uint m_valueAttrs;    // number of values
    uint m_fragCount;     // index fragments
    uint m_sampleVersion;
    uint m_sampleCount;   // sample count from head record
    uint m_keyBytes;      // total key bytes from head record
    uint m_valueLen;      // value bytes per entry i.e. valueAttrs * ValueSize
    uint m_valueBytes;    // total value bytes i.e. sampleCount * valuelen
    uint m_addrLen;       // 1-4 based on keyBytes
    uint m_addrBytes;     // total address bytes
    Uint8* m_addrArray;
    Uint8* m_keyArray;
    Uint8* m_valueArray;
    Cache* m_nextClean;
    // performance
    mutable Uint64 m_save_time;
    mutable Uint64 m_sort_time;
    // in use by query_stat
    mutable uint m_ref_count;
    Cache();
    // pos is index < sampleCount, addr is offset in keyArray
    uint get_keyaddr(uint pos) const;
    void set_keyaddr(uint pos, uint addr);
    // get pointers to key and value arrays at pos
    const Uint8* get_keyptr(uint addr) const;
    Uint8* get_keyptr(uint addr);
    const Uint8* get_valueptr(uint pos) const;
    Uint8* get_valueptr(uint pos);
    // for sort
    void swap_entry(uint pos1, uint pos2);
    // get stats values primitives
    double get_rir1(uint pos) const;
    double get_rir1(uint pos1, uint pos2) const;
    double get_rir(uint pos) const;
    double get_rir(uint pos1, uint pos2) const;
    double get_unq1(uint pos, uint k) const;
    double get_unq1(uint pos1, uint pos2, uint k) const;
    double get_unq(uint pos, uint k, double *factor) const;
    double get_unq(uint pos1, uint pos2, uint k, double *factor) const;
    double get_rpk(uint pos, uint k, double *factor) const;
    double get_rpk(uint pos1, uint pos2, uint k, double *factor) const;
  };
  int cache_cmpaddr(const Cache& c, uint addr1, uint addr2) const;
  int cache_cmppos(const Cache& c, uint pos1, uint pos2) const;
  int cache_sort(Cache& c);
  void cache_isort(Cache& c);
  void cache_hsort(Cache& c);
  void cache_hsort_sift(Cache& c, int i, int count);
#ifdef ndb_index_stat_hsort_verify
  void cache_hsort_verify(Cache& c, int count);
#endif
  int cache_verify(const Cache& c);
  void move_cache();
  void clean_cache();
  void free_cache(Cache* c);
  void free_cache();

  // query cache dump (not available via facade)
  struct CacheIter {
    Uint32 m_keyCount;
    Uint32 m_sampleCount;
    Uint32 m_sampleIndex;
    NdbPack::DataC m_keyData;
    NdbPack::DataC m_valueData;
    CacheIter(const NdbIndexStatImpl& impl);
  };
  int dump_cache_start(CacheIter& iter);
  bool dump_cache_next(CacheIter& iter);

  // bound
  struct Bound {
    NdbPack::Data m_data;
    NdbPack::Bound m_bound;
    int m_type;     // 0-lower 1-upper
    int m_strict;
    Bound(const NdbPack::Spec& spec);
  };
  int finalize_bound(Bound&);

  // range
  struct Range {
    Range(Bound& bound1, Bound& bound2);
    Bound& m_bound1;
    Bound& m_bound2;
  };
  int finalize_range(Range& range);
  int convert_range(Range& range,
                    const NdbRecord* key_record,
                    const NdbIndexScanOperation::IndexBound* ib);

  // computed stats values
  struct StatValue {
    Uint32 m_num_fragments;
    Uint32 m_num_rows;
    bool m_empty;
    double m_rir;
    double m_unq_factor[MaxKeyCount];
    double m_unq[MaxKeyCount];
    StatValue();
  };

  // query
  struct StatBound {
    uint m_pos;       // non-empty bound is between pos-1,pos
    uint m_numEqL;    // components matching key at pos-1
    uint m_numEqH;    // components matching key at pos
    StatValue m_value;
    const char* m_rule;
    StatBound();
  };
  struct Stat {
    StatBound m_stat1;
    StatBound m_stat2;
    StatValue m_value;
    const char* m_rule[3];
    Stat();
  };
  void query_normalize(const Cache&, StatValue&);
  void query_unq2rpk(const Cache&, StatValue&);
  int query_stat(const Range&, Stat&);
  void query_interpolate(const Cache&, const Range&, Stat&);
  void query_interpolate(const Cache&, const Bound&, StatBound&);
  void query_search(const Cache&, const Bound&, StatBound&);
  int query_keycmp(const Cache&, const Bound&, uint pos, Uint32& numEq);

  // events and polling
  int create_sysevents(Ndb* ndb);
  int drop_sysevents(Ndb* ndb);
  int check_sysevents(Ndb* ndb);
  //
  int create_listener(Ndb* ndb);
  int execute_listener(Ndb* ndb);
  int poll_listener(Ndb* ndb, int max_wait_ms);
  int next_listener(Ndb* ndb);
  int drop_listener(Ndb* ndb);

  // default memory allocator
  struct MemDefault : public Mem {
    void* mem_alloc(UintPtr bytes) override;
    void mem_free(void* ptr) override;
    MemDefault();
    ~MemDefault() override;
  };
  MemDefault c_mem_default_handler;

  // error
  const NdbIndexStat::Error& getNdbError() const;
  void setError(int code, int line, int extra = 0);
  void setError(const Con& con, int line);
  void mapError(const int* map, int code);

// moved from NdbIndexStat.hpp by jonas

  /* Need 2 words per column in a bound plus space for the
   * bound data.
   * Worst case is 32 cols in key and max key size used.
   */
  static constexpr Uint32 BoundBufWords =
    (2 * NDB_MAX_NO_OF_ATTRIBUTES_IN_KEY) + NDB_MAX_KEYSIZE_IN_WORDS;
};

inline
NdbIndexStatImpl::Bound::Bound(const NdbPack::Spec& spec) :
  m_data(spec, true, 0),
  m_bound(m_data)
{
  m_type = -1;
  m_strict = -1;
}

inline
NdbIndexStatImpl::Range::Range(Bound& bound1, Bound& bound2) :
  m_bound1(bound1),
  m_bound2(bound2)
{
  bound1.m_type = 0;
  bound2.m_type = 1;
}

inline int
NdbIndexStatImpl::finalize_range(Range& range)
{
  if (finalize_bound(range.m_bound1) == -1)
    return -1;
  if (finalize_bound(range.m_bound2) == -1)
    return -1;
  return 0;
}

inline
NdbIndexStatImpl::StatValue::StatValue()
  : m_num_fragments(0), m_num_rows(0), m_empty(false)
{}

inline
NdbIndexStatImpl::StatBound::StatBound()
{
  m_pos = 0;
  m_numEqL = 0;
  m_numEqH = 0;
}

inline
NdbIndexStatImpl::Stat::Stat()
{
  m_rule[0] = nullptr;
  m_rule[1] = nullptr;
  m_rule[2] = nullptr;
}
 
#endif
