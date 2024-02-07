/*
   Copyright (c) 2011, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <algorithm>
#include "util/require.h"

#include <NdbEnv.h>
#include <NdbSleep.h>
#include <ndb_global.h>
#include <Bitmask.hpp>
#include <Ndb.hpp>
#include <NdbEventOperation.hpp>
#include <NdbOut.hpp>
#include <NdbRecAttr.hpp>
#include <NdbRecord.hpp>
#include <NdbSqlUtil.hpp>
#include <NdbTransaction.hpp>
#include "NdbIndexStatImpl.hpp"
#include "mysql/strings/m_ctype.h"

static const char *const g_headtable_name = NDB_INDEX_STAT_HEAD_TABLE;
static const char *const g_sampletable_name = NDB_INDEX_STAT_SAMPLE_TABLE;
static const char *const g_sampleindex1_name = NDB_INDEX_STAT_SAMPLE_INDEX1;

static const int ERR_NoSuchObject[] = {709, 723, 4243, 0};
static const int ERR_TupleNotFound[] = {626, 0};

NdbIndexStatImpl::NdbIndexStatImpl(NdbIndexStat &facade)
    : NdbIndexStat(*this),
      m_facade(&facade),
      m_keyData(m_keySpec, false, 2),
      m_valueData(m_valueSpec, false, 2) {
  init();
  m_query_mutex = NdbMutex_Create();
  assert(m_query_mutex != nullptr);
  m_eventOp = nullptr;
  m_mem_handler = &c_mem_default_handler;
}

void NdbIndexStatImpl::init() {
  m_indexSet = false;
  m_indexId = 0;
  m_indexVersion = 0;
  m_tableId = 0;
  m_keyAttrs = 0;
  m_valueAttrs = 0;
  // buffers
  m_keySpecBuf = nullptr;
  m_valueSpecBuf = nullptr;
  m_keyDataBuf = nullptr;
  m_valueDataBuf = nullptr;
  // cache
  m_cacheBuild = nullptr;
  m_cacheQuery = nullptr;
  m_cacheClean = nullptr;
  // head
  init_head(m_facadeHead);
}

NdbIndexStatImpl::~NdbIndexStatImpl() {
  reset_index();
  if (m_query_mutex != nullptr) {
    NdbMutex_Destroy(m_query_mutex);
    m_query_mutex = nullptr;
  }
}

// sys tables meta

NdbIndexStatImpl::Sys::Sys(NdbIndexStatImpl *impl, Ndb *ndb)
    : m_impl(impl), m_ndb(ndb) {
  m_dic = m_ndb->getDictionary();
  m_headtable = nullptr;
  m_sampletable = nullptr;
  m_sampleindex1 = nullptr;
  m_obj_cnt = 0;
}

NdbIndexStatImpl::Sys::~Sys() { m_impl->sys_release(*this); }

void NdbIndexStatImpl::sys_release(Sys &sys) {
  // close schema trans if any exists
  NdbDictionary::Dictionary *const dic = sys.m_dic;
  (void)dic->endSchemaTrans(NdbDictionary::Dictionary::SchemaTransAbort);

  if (sys.m_headtable != nullptr) {
    sys.m_dic->removeTableGlobal(*sys.m_headtable, false);
    sys.m_headtable = nullptr;
  }
  if (sys.m_sampletable != nullptr) {
    sys.m_dic->removeTableGlobal(*sys.m_sampletable, false);
    sys.m_sampletable = nullptr;
  }
  if (sys.m_sampleindex1 != nullptr) {
    sys.m_dic->removeIndexGlobal(*sys.m_sampleindex1, false);
    sys.m_sampleindex1 = nullptr;
  }
}

int NdbIndexStatImpl::make_headtable(NdbDictionary::Table &tab) {
  tab.setName(g_headtable_name);
  tab.setLogging(true);
  // key must be first
  {
    NdbDictionary::Column col("index_id");
    col.setType(NdbDictionary::Column::Unsigned);
    col.setPrimaryKey(true);
    tab.addColumn(col);
  }
  {
    NdbDictionary::Column col("index_version");
    col.setType(NdbDictionary::Column::Unsigned);
    col.setPrimaryKey(true);
    tab.addColumn(col);
  }
  // table
  {
    NdbDictionary::Column col("table_id");
    col.setType(NdbDictionary::Column::Unsigned);
    col.setNullable(false);
    tab.addColumn(col);
  }
  {
    NdbDictionary::Column col("frag_count");
    col.setType(NdbDictionary::Column::Unsigned);
    col.setNullable(false);
    tab.addColumn(col);
  }
  // current sample
  {
    NdbDictionary::Column col("value_format");
    col.setType(NdbDictionary::Column::Unsigned);
    col.setNullable(false);
    tab.addColumn(col);
  }
  {
    NdbDictionary::Column col("sample_version");
    col.setType(NdbDictionary::Column::Unsigned);
    col.setNullable(false);
    tab.addColumn(col);
  }
  {
    NdbDictionary::Column col("load_time");
    col.setType(NdbDictionary::Column::Unsigned);
    col.setNullable(false);
    tab.addColumn(col);
  }
  {
    NdbDictionary::Column col("sample_count");
    col.setType(NdbDictionary::Column::Unsigned);
    col.setNullable(false);
    tab.addColumn(col);
  }
  {
    NdbDictionary::Column col("key_bytes");
    col.setType(NdbDictionary::Column::Unsigned);
    col.setNullable(false);
    tab.addColumn(col);
  }
  NdbError error;
  if (tab.validate(error) == -1) {
    setError(error.code, __LINE__);
    return -1;
  }
  return 0;
}

int NdbIndexStatImpl::make_sampletable(NdbDictionary::Table &tab) {
  tab.setName(g_sampletable_name);
  tab.setLogging(true);
  // key must be first
  {
    NdbDictionary::Column col("index_id");
    col.setType(NdbDictionary::Column::Unsigned);
    col.setPrimaryKey(true);
    tab.addColumn(col);
  }
  {
    NdbDictionary::Column col("index_version");
    col.setType(NdbDictionary::Column::Unsigned);
    col.setPrimaryKey(true);
    tab.addColumn(col);
  }
  {
    NdbDictionary::Column col("sample_version");
    col.setType(NdbDictionary::Column::Unsigned);
    col.setPrimaryKey(true);
    tab.addColumn(col);
  }
  {
    NdbDictionary::Column col("stat_key");
    col.setType(NdbDictionary::Column::Longvarbinary);
    col.setPrimaryKey(true);
    col.setLength(MaxKeyBytes);
    tab.addColumn(col);
  }
  // value
  {
    NdbDictionary::Column col("stat_value");
    col.setType(NdbDictionary::Column::Longvarbinary);
    col.setNullable(false);
    col.setLength(MaxValueCBytes);
    tab.addColumn(col);
  }
  NdbError error;
  if (tab.validate(error) == -1) {
    setError(error.code, __LINE__);
    return -1;
  }
  return 0;
}

int NdbIndexStatImpl::make_sampleindex1(NdbDictionary::Index &ind) {
  ind.setTable(g_sampletable_name);
  ind.setName(g_sampleindex1_name);
  ind.setType(NdbDictionary::Index::OrderedIndex);
  ind.setLogging(false);
  ind.addColumnName("index_id");
  ind.addColumnName("index_version");
  ind.addColumnName("sample_version");
  return 0;
}

int NdbIndexStatImpl::check_table(const NdbDictionary::Table &tab1,
                                  const NdbDictionary::Table &tab2) {
  if (tab1.getNoOfColumns() != tab2.getNoOfColumns()) return -1;
  const uint n = tab1.getNoOfColumns();
  for (uint i = 0; i < n; i++) {
    const NdbDictionary::Column *col1 = tab1.getColumn(i);
    const NdbDictionary::Column *col2 = tab2.getColumn(i);
    require(col1 != nullptr && col2 != nullptr);
    if (!col1->equal(*col2)) return -1;
  }
  return 0;
}

int NdbIndexStatImpl::check_index(const NdbDictionary::Index &ind1,
                                  const NdbDictionary::Index &ind2) {
  if (ind1.getNoOfColumns() != ind2.getNoOfColumns()) return -1;
  const uint n = ind1.getNoOfColumns();
  for (uint i = 0; i < n; i++) {
    const NdbDictionary::Column *col1 = ind1.getColumn(i);
    const NdbDictionary::Column *col2 = ind2.getColumn(i);
    require(col1 != nullptr && col2 != nullptr);
    // getColumnNo() does not work on non-retrieved
    if (!col1->equal(*col2)) return -1;
  }
  return 0;
}

int NdbIndexStatImpl::get_systables(Sys &sys) {
  Ndb *ndb = sys.m_ndb;
  NdbDictionary::Dictionary *const dic = ndb->getDictionary();
  const int NoSuchTable = 723;
  const int NoSuchIndex = 4243;

  sys.m_headtable = dic->getTableGlobal(g_headtable_name);
  if (sys.m_headtable == nullptr) {
    int code = dic->getNdbError().code;
    if (code != NoSuchTable) {
      setError(code, __LINE__);
      return -1;
    }
  } else {
    NdbDictionary::Table tab;
    make_headtable(tab);
    if (check_table(*sys.m_headtable, tab) == -1) {
      setError(BadSysTables, __LINE__);
      return -1;
    }
    sys.m_obj_cnt++;
  }

  sys.m_sampletable = dic->getTableGlobal(g_sampletable_name);
  if (sys.m_sampletable == nullptr) {
    int code = dic->getNdbError().code;
    if (code != NoSuchTable) {
      setError(code, __LINE__);
      return -1;
    }
  } else {
    NdbDictionary::Table tab;
    make_sampletable(tab);
    if (check_table(*sys.m_sampletable, tab) == -1) {
      setError(BadSysTables, __LINE__);
      return -1;
    }
    sys.m_obj_cnt++;
  }

  if (sys.m_sampletable != nullptr) {
    sys.m_sampleindex1 =
        dic->getIndexGlobal(g_sampleindex1_name, *sys.m_sampletable);
    if (sys.m_sampleindex1 == nullptr) {
      int code = dic->getNdbError().code;
      if (code != NoSuchIndex) {
        setError(code, __LINE__);
        return -1;
      }
    } else {
      NdbDictionary::Index ind;
      make_sampleindex1(ind);
      if (check_index(*sys.m_sampleindex1, ind) == -1) {
        setError(BadSysTables, __LINE__);
        return -1;
      }
      sys.m_obj_cnt++;
    }
  }

  return 0;
}

int NdbIndexStatImpl::create_systables(Ndb *ndb) {
  Sys sys(this, ndb);

  NdbDictionary::Dictionary *const dic = sys.m_dic;

  if (dic->beginSchemaTrans() == -1) {
    setError(dic->getNdbError().code, __LINE__);
    return -1;
  }

  if (get_systables(sys) == -1) {
    dic->endSchemaTrans(NdbDictionary::Dictionary::SchemaTransAbort);
    return -1;
  }

  if (sys.m_obj_cnt == Sys::ObjCnt) {
    setError(HaveSysTables, __LINE__);
    dic->endSchemaTrans(NdbDictionary::Dictionary::SchemaTransAbort);
    return -1;
  }

  if (sys.m_obj_cnt != 0) {
    setError(BadSysTables, __LINE__);
    dic->endSchemaTrans(NdbDictionary::Dictionary::SchemaTransAbort);
    return -1;
  }

  {
    NdbDictionary::Table tab;
    if (make_headtable(tab) == -1) {
      dic->endSchemaTrans(NdbDictionary::Dictionary::SchemaTransAbort);
      return -1;
    }
    if (dic->createTable(tab) == -1) {
      setError(dic->getNdbError().code, __LINE__);
      dic->endSchemaTrans(NdbDictionary::Dictionary::SchemaTransAbort);
      return -1;
    }

    sys.m_headtable = dic->getTableGlobal(tab.getName());
    if (sys.m_headtable == nullptr) {
      setError(dic->getNdbError().code, __LINE__);
      dic->endSchemaTrans(NdbDictionary::Dictionary::SchemaTransAbort);
      return -1;
    }
  }

  {
    NdbDictionary::Table tab;
    if (make_sampletable(tab) == -1) {
      dic->endSchemaTrans(NdbDictionary::Dictionary::SchemaTransAbort);
      return -1;
    }

#ifdef VM_TRACE
#ifdef NDB_USE_GET_ENV
    // test of schema trans
    {
      const char *p =
          NdbEnv_GetEnv("NDB_INDEX_STAT_ABORT_SYS_CREATE", (char *)0, 0);
      if (p != 0 && strchr("1Y", p[0]) != 0) {
        setError(9999, __LINE__);
        dic->endSchemaTrans(NdbDictionary::Dictionary::SchemaTransAbort);
        return -1;
      }
    }
#endif
#endif

    if (dic->createTable(tab) == -1) {
      setError(dic->getNdbError().code, __LINE__);
      dic->endSchemaTrans(NdbDictionary::Dictionary::SchemaTransAbort);
      return -1;
    }

    sys.m_sampletable = dic->getTableGlobal(tab.getName());
    if (sys.m_sampletable == nullptr) {
      setError(dic->getNdbError().code, __LINE__);
      dic->endSchemaTrans(NdbDictionary::Dictionary::SchemaTransAbort);
      return -1;
    }
  }

  {
    NdbDictionary::Index ind;
    if (make_sampleindex1(ind) == -1) {
      dic->endSchemaTrans(NdbDictionary::Dictionary::SchemaTransAbort);
      return -1;
    }
    if (dic->createIndex(ind, *sys.m_sampletable) == -1) {
      setError(dic->getNdbError().code, __LINE__);
      dic->endSchemaTrans(NdbDictionary::Dictionary::SchemaTransAbort);
      return -1;
    }

    sys.m_sampleindex1 =
        dic->getIndexGlobal(ind.getName(), sys.m_sampletable->getName());
    if (sys.m_sampleindex1 == nullptr) {
      setError(dic->getNdbError().code, __LINE__);
      dic->endSchemaTrans(NdbDictionary::Dictionary::SchemaTransAbort);
      return -1;
    }
  }

  if (dic->endSchemaTrans() == -1) {
    setError(dic->getNdbError().code, __LINE__);
    return -1;
  }

  return 0;
}

int NdbIndexStatImpl::drop_systables(Ndb *ndb) {
  Sys sys(this, ndb);

  NdbDictionary::Dictionary *const dic = sys.m_dic;

  if (dic->beginSchemaTrans() == -1) {
    setError(dic->getNdbError().code, __LINE__);
    return -1;
  }

  if (get_systables(sys) == -1 && m_error.code != BadSysTables) {
    dic->endSchemaTrans(NdbDictionary::Dictionary::SchemaTransAbort);
    return -1;
  }

  if (sys.m_headtable != nullptr) {
    if (dic->dropTableGlobal(*sys.m_headtable) == -1) {
      setError(dic->getNdbError().code, __LINE__);
      dic->endSchemaTrans(NdbDictionary::Dictionary::SchemaTransAbort);
      return -1;
    }
  }

  if (sys.m_sampletable != nullptr) {
#ifdef VM_TRACE
#ifdef NDB_USE_GET_ENV
    // test of schema trans
    {
      const char *p =
          NdbEnv_GetEnv("NDB_INDEX_STAT_ABORT_SYS_DROP", (char *)0, 0);
      if (p != 0 && strchr("1Y", p[0]) != 0) {
        setError(9999, __LINE__);
        dic->endSchemaTrans(NdbDictionary::Dictionary::SchemaTransAbort);
        return -1;
      }
    }
#endif
#endif

    if (dic->dropTableGlobal(*sys.m_sampletable) == -1) {
      setError(dic->getNdbError().code, __LINE__);
      dic->endSchemaTrans(NdbDictionary::Dictionary::SchemaTransAbort);
      return -1;
    }
  }

  if (dic->endSchemaTrans() == -1) {
    setError(dic->getNdbError().code, __LINE__);
    return -1;
  }

  return 0;
}

int NdbIndexStatImpl::check_systables(Sys &sys) {
  if (get_systables(sys) == -1) return -1;

  if (sys.m_obj_cnt == 0) {
    setError(NoSysTables, __LINE__);
    return -1;
  }

  if (sys.m_obj_cnt != Sys::ObjCnt) {
    setError(BadSysTables, __LINE__);
    return -1;
  }

  return 0;
}

int NdbIndexStatImpl::check_systables(Ndb *ndb) {
  Sys sys(this, ndb);

  if (check_systables(sys) == -1) return -1;

  return 0;
}

// operation context

NdbIndexStatImpl::Con::Con(NdbIndexStatImpl *impl, Head &head, Ndb *ndb)
    : m_impl(impl), m_head(head), m_ndb(ndb), m_start() {
  head.m_indexId = m_impl->m_indexId;
  head.m_indexVersion = m_impl->m_indexVersion;
  m_dic = m_ndb->getDictionary();
  m_headtable = nullptr;
  m_sampletable = nullptr;
  m_sampleindex1 = nullptr;
  m_tx = nullptr;
  m_op = nullptr;
  m_scanop = nullptr;
  m_cacheBuild = nullptr;
  m_cachePos = 0;
  m_cacheKeyOffset = 0;
  m_cacheValueOffset = 0;
}

NdbIndexStatImpl::Con::~Con() {
  if (m_cacheBuild != nullptr) {
    m_impl->free_cache(m_cacheBuild);
    m_cacheBuild = nullptr;
  }
  if (m_tx != nullptr) {
    m_ndb->closeTransaction(m_tx);
    m_tx = nullptr;
  }
  m_impl->sys_release(*this);
}

int NdbIndexStatImpl::Con::startTransaction() {
  assert(m_headtable != nullptr && m_ndb != nullptr && m_tx == nullptr);
  Uint32 key[2] = {m_head.m_indexId, m_head.m_indexVersion};
  m_tx = m_ndb->startTransaction(m_headtable, (const char *)key, sizeof(key));
  if (m_tx == nullptr) return -1;
  return 0;
}

int NdbIndexStatImpl::Con::execute(bool commit) {
  assert(m_tx != nullptr);
  if (commit) {
    if (m_tx->execute(NdbTransaction::Commit) == -1) return -1;
    m_ndb->closeTransaction(m_tx);
    m_tx = nullptr;
  } else {
    if (m_tx->execute(NdbTransaction::NoCommit) == -1) return -1;
  }
  return 0;
}

int NdbIndexStatImpl::Con::getNdbOperation() {
  assert(m_headtable != nullptr);
  assert(m_tx != nullptr && m_op == nullptr);
  m_op = m_tx->getNdbOperation(m_headtable);
  if (m_op == nullptr) return -1;
  return 0;
}

int NdbIndexStatImpl::Con::getNdbIndexScanOperation() {
  assert(m_sampletable != nullptr && m_sampleindex1 != nullptr);
  assert(m_tx != nullptr && m_scanop == nullptr);
  m_scanop = m_tx->getNdbIndexScanOperation(m_sampleindex1, m_sampletable);
  if (m_scanop == nullptr) return -1;
  return 0;
}

void NdbIndexStatImpl::Con::set_time() { m_start = NdbTick_getCurrentTicks(); }

Uint64 NdbIndexStatImpl::Con::get_time() {
  const NDB_TICKS stop = NdbTick_getCurrentTicks();
  Uint64 us = NdbTick_Elapsed(m_start, stop).microSec();
  return us;
}

// index

int NdbIndexStatImpl::set_index(const NdbDictionary::Index &index,
                                const NdbDictionary::Table &table) {
  if (m_indexSet) {
    setError(UsageError, __LINE__);
    return -1;
  }
  m_indexId = index.getObjectId();
  m_indexVersion = index.getObjectVersion();
  m_tableId = table.getObjectId();
  m_keyAttrs = index.getNoOfColumns();
  m_valueAttrs = 1 + m_keyAttrs;
  if (m_keyAttrs == 0) {
    setError(InternalError, __LINE__);
    return -1;
  }
  if (m_keyAttrs > MaxKeyCount) {
    setError(InternalError, __LINE__);
    return -1;
  }

  // spec buffers
  m_keySpecBuf = new NdbPack::Type[m_keyAttrs];
  m_valueSpecBuf = new NdbPack::Type[m_valueAttrs];
  if (m_keySpecBuf == nullptr || m_valueSpecBuf == nullptr) {
    setError(NoMemError, __LINE__);
    return -1;
  }
  m_keySpec.set_buf(m_keySpecBuf, m_keyAttrs);
  m_valueSpec.set_buf(m_valueSpecBuf, m_valueAttrs);

  // index key spec
  {
    for (uint i = 0; i < m_keyAttrs; i++) {
      const NdbDictionary::Column *icol = index.getColumn(i);
      if (icol == nullptr) {
        setError(UsageError, __LINE__);
        return -1;
      }
      NdbPack::Type type(
          icol->getType(), icol->getSizeInBytes(), icol->getNullable(),
          icol->getCharset() != nullptr ? icol->getCharset()->number : 0);
      if (m_keySpec.add(type) == -1) {
        setError(UsageError, __LINE__, m_keySpec.get_error_code());
        return -1;
      }
    }
  }
  // stat values spec
  {
    NdbPack::Type type(NDB_TYPE_UNSIGNED, 4, false, 0);
    // rir + rpk
    if (m_valueSpec.add(type, m_valueAttrs) == -1) {
      setError(InternalError, __LINE__, m_valueSpec.get_error_code());
      return -1;
    }
  }

  // data buffers (rounded to word)
  m_keyDataBuf = new Uint8[m_keyData.get_max_len4()];
  m_valueDataBuf = new Uint8[m_valueData.get_max_len4()];
  if (m_keyDataBuf == nullptr || m_valueDataBuf == nullptr) {
    setError(NoMemError, __LINE__);
    return -1;
  }
  m_keyData.set_buf(m_keyDataBuf, m_keyData.get_max_len());
  m_valueData.set_buf(m_valueDataBuf, m_valueData.get_max_len());

  m_indexSet = true;
  return 0;
}

void NdbIndexStatImpl::reset_index() {
  free_cache();
  m_keySpec.reset();
  m_valueSpec.reset();
  delete[] m_keySpecBuf;
  delete[] m_valueSpecBuf;
  delete[] m_keyDataBuf;
  delete[] m_valueDataBuf;
  init();
}

// head

void NdbIndexStatImpl::init_head(Head &head) {
  head.m_found = -1;
  head.m_eventType = -1;
  head.m_indexId = 0;
  head.m_indexVersion = 0;
  head.m_tableId = 0;
  head.m_fragCount = 0;
  head.m_valueFormat = 0;
  head.m_sampleVersion = 0;
  head.m_loadTime = 0;
  head.m_sampleCount = 0;
  head.m_keyBytes = 0;
}

// sys tables data

int NdbIndexStatImpl::sys_init(Con &con) {
  Ndb *ndb = con.m_ndb;
  NdbDictionary::Dictionary *const dic = ndb->getDictionary();
  sys_release(con);

  con.m_headtable = dic->getTableGlobal(g_headtable_name);
  if (con.m_headtable == nullptr) {
    setError(con, __LINE__);
    mapError(ERR_NoSuchObject, NoSysTables);
    return -1;
  }
  con.m_sampletable = dic->getTableGlobal(g_sampletable_name);
  if (con.m_sampletable == nullptr) {
    setError(con, __LINE__);
    mapError(ERR_NoSuchObject, NoSysTables);
    return -1;
  }
  con.m_sampleindex1 =
      dic->getIndexGlobal(g_sampleindex1_name, *con.m_sampletable);
  if (con.m_sampleindex1 == nullptr) {
    setError(con, __LINE__);
    mapError(ERR_NoSuchObject, NoSysTables);
    return -1;
  }
  return 0;
}

void NdbIndexStatImpl::sys_release(Con &con) {
  if (con.m_headtable != nullptr) {
    con.m_dic->removeTableGlobal(*con.m_headtable, false);
    con.m_headtable = nullptr;
  }
  if (con.m_sampletable != nullptr) {
    con.m_dic->removeTableGlobal(*con.m_sampletable, false);
    con.m_sampletable = nullptr;
  }
  if (con.m_sampleindex1 != nullptr) {
    con.m_dic->removeIndexGlobal(*con.m_sampleindex1, false);
    con.m_sampleindex1 = nullptr;
  }
}

int NdbIndexStatImpl::sys_read_head(Con &con, bool commit) {
  Head &head = con.m_head;
  head.m_sampleVersion = 0;
  head.m_found = false;

  if (con.getNdbOperation() == -1) {
    setError(con, __LINE__);
    return -1;
  }
  if (con.m_op->readTuple(NdbOperation::LM_Read) == -1) {
    setError(con, __LINE__);
    return -1;
  }
  if (sys_head_setkey(con) == -1) return -1;
  if (sys_head_getvalue(con) == -1) return -1;
  if (con.m_op->setAbortOption(NdbOperation::AbortOnError) == -1) {
    setError(con, __LINE__);
    return -1;
  }
  if (con.execute(commit) == -1) {
    setError(con, __LINE__);
    mapError(ERR_TupleNotFound, NoIndexStats);
    return -1;
  }
  head.m_found = true;
  if (head.m_sampleVersion == 0) {
    setError(NoIndexStats, __LINE__);
    return -1;
  }
  return 0;
}

int NdbIndexStatImpl::sys_head_setkey(Con &con) {
  Head &head = con.m_head;
  NdbOperation *op = con.m_op;
  if (op->equal("index_id", (char *)&head.m_indexId) == -1) {
    setError(con, __LINE__);
    return -1;
  }
  if (op->equal("index_version", (char *)&head.m_indexVersion) == -1) {
    setError(con, __LINE__);
    return -1;
  }
  return 0;
}

int NdbIndexStatImpl::sys_head_getvalue(Con &con) {
  Head &head = con.m_head;
  NdbOperation *op = con.m_op;
  if (op->getValue("table_id", (char *)&head.m_tableId) == nullptr) {
    setError(con, __LINE__);
    return -1;
  }
  if (op->getValue("frag_count", (char *)&head.m_fragCount) == nullptr) {
    setError(con, __LINE__);
    return -1;
  }
  if (op->getValue("value_format", (char *)&head.m_valueFormat) == nullptr) {
    setError(con, __LINE__);
    return -1;
  }
  if (op->getValue("sample_version", (char *)&head.m_sampleVersion) ==
      nullptr) {
    setError(con, __LINE__);
    return -1;
  }
  if (op->getValue("load_time", (char *)&head.m_loadTime) == nullptr) {
    setError(con, __LINE__);
    return -1;
  }
  if (op->getValue("sample_count", (char *)&head.m_sampleCount) == nullptr) {
    setError(con, __LINE__);
    return -1;
  }
  if (op->getValue("key_bytes", (char *)&head.m_keyBytes) == nullptr) {
    setError(con, __LINE__);
    return -1;
  }
  return 0;
}

int NdbIndexStatImpl::sys_sample_setkey(Con &con) {
  Head &head = con.m_head;
  NdbIndexScanOperation *op = con.m_scanop;
  if (op->equal("index_id", (char *)&head.m_indexId) == -1) {
    setError(con, __LINE__);
    return -1;
  }
  if (op->equal("index_version", (char *)&head.m_indexVersion) == -1) {
    setError(con, __LINE__);
    return -1;
  }
  if (op->equal("sample_version", (char *)&head.m_sampleVersion) == -1) {
    setError(con, __LINE__);
    return -1;
  }
  if (op->equal("stat_key", (const char *)m_keyData.get_full_buf()) == -1) {
    setError(con, __LINE__);
    return -1;
  }
  return 0;
}

int NdbIndexStatImpl::sys_sample_getvalue(Con &con) {
  NdbIndexScanOperation *op = con.m_scanop;
  if (op->getValue("stat_key", (char *)m_keyData.get_full_buf()) == nullptr) {
    setError(con, __LINE__);
    return -1;
  }
  if (op->getValue("stat_value", (char *)m_valueData.get_full_buf()) ==
      nullptr) {
    setError(con, __LINE__);
    return -1;
  }
  return 0;
}

int NdbIndexStatImpl::sys_sample_setbound(Con &con, int sv_bound) {
  Head &head = con.m_head;
  NdbIndexScanOperation *op = con.m_scanop;
  const NdbIndexScanOperation::BoundType eq_bound =
      NdbIndexScanOperation::BoundEQ;

  if (op->setBound("index_id", eq_bound, &head.m_indexId) == -1) {
    setError(con, __LINE__);
    return -1;
  }
  if (op->setBound("index_version", eq_bound, &head.m_indexVersion) == -1) {
    setError(con, __LINE__);
    return -1;
  }
  if (sv_bound != -1) {
    if (op->setBound("sample_version", sv_bound, &head.m_sampleVersion) == -1) {
      setError(con, __LINE__);
      return -1;
    }
  }
  return 0;
}

// update, delete

int NdbIndexStatImpl::update_stat(Ndb *ndb, Head &head) {
  Con con(this, head, ndb);
  if (con.m_dic->updateIndexStat(m_indexId, m_indexVersion, m_tableId) == -1) {
    setError(con, __LINE__);
    mapError(ERR_NoSuchObject, NoSysTables);
    return -1;
  }
  return 0;
}

int NdbIndexStatImpl::delete_stat(Ndb *ndb, Head &head) {
  Con con(this, head, ndb);
  if (con.m_dic->deleteIndexStat(m_indexId, m_indexVersion, m_tableId) == -1) {
    setError(con, __LINE__);
    mapError(ERR_NoSuchObject, NoSysTables);
    return -1;
  }
  return 0;
}

// read

int NdbIndexStatImpl::read_head(Ndb *ndb, Head &head) {
  Con con(this, head, ndb);
  if (!m_indexSet) {
    setError(UsageError, __LINE__);
    return -1;
  }
  if (sys_init(con) == -1) return -1;
  if (con.startTransaction() == -1) {
    setError(con, __LINE__);
    return -1;
  }
  if (sys_read_head(con, true) == -1) return -1;
  return 0;
}

int NdbIndexStatImpl::read_stat(Ndb *ndb, Head &head) {
  Con con(this, head, ndb);
  con.set_time();

  if (read_start(con) == -1) return -1;
  if (save_start(con) == -1) return -1;
  while (1) {
    int ret = read_next(con);
    if (ret == -1) return -1;
    if (ret != 0) break;
    if (save_next(con) == -1) return -1;
  }
  if (read_commit(con) == -1) return -1;

  Uint64 save_time = con.get_time();
  con.set_time();

  if (save_commit(con) == -1) return -1;
  Uint64 sort_time = con.get_time();

  const Cache &c = *m_cacheBuild;
  c.m_save_time = save_time;
  c.m_sort_time = sort_time;
  return 0;
}

int NdbIndexStatImpl::read_start(Con &con) {
  // UNUSED Head& head = con.m_head;
  if (!m_indexSet) {
    setError(UsageError, __LINE__);
    return -1;
  }
  if (sys_init(con) == -1) return -1;
  if (con.startTransaction() == -1) {
    setError(con, __LINE__);
    return -1;
  }
  if (sys_read_head(con, false) == -1) return -1;
  if (con.getNdbIndexScanOperation() == -1) {
    setError(con, __LINE__);
    return -1;
  }
  if (con.m_scanop->readTuples(NdbOperation::LM_CommittedRead, 0) == -1) {
    setError(con, __LINE__);
    return -1;
  }
  if (sys_sample_setbound(con, NdbIndexScanOperation::BoundEQ) == -1) return -1;
  if (sys_sample_getvalue(con) == -1) return -1;
  if (con.execute(false) == -1) {
    setError(con, __LINE__);
    return -1;
  }
  return 0;
}

int NdbIndexStatImpl::read_next(Con &con) {
  m_keyData.reset();
  m_valueData.reset();
  int ret = con.m_scanop->nextResult();
  if (ret != 0) {
    if (ret == -1) setError(con, __LINE__);
    return ret;
  }

  /*
   * Key and value are raw data and little-endian.  Create the complete
   * NdbPack::Data instance and convert it to native-endian.
   */
  const NdbPack::Endian::Value from_endian = NdbPack::Endian::Little;
  const NdbPack::Endian::Value to_endian = NdbPack::Endian::Native;

  if (m_keyData.desc_all(m_keyAttrs, from_endian) == -1) {
    setError(InternalError, __LINE__, m_keyData.get_error_code());
    return -1;
  }
  if (m_keyData.convert(to_endian) == -1) {
    setError(InternalError, __LINE__, m_keyData.get_error_code());
    return -1;
  }
  if (m_valueData.desc_all(m_valueAttrs, from_endian) == -1) {
    setError(InternalError, __LINE__, m_valueData.get_error_code());
    return -1;
  }
  if (m_valueData.convert(to_endian) == -1) {
    setError(InternalError, __LINE__, m_valueData.get_error_code());
    return -1;
  }
  return 0;
}

int NdbIndexStatImpl::read_commit(Con &con) {
  if (con.execute(true) == -1) {
    setError(con, __LINE__);
    return -1;
  }
  return 0;
}

// save

int NdbIndexStatImpl::save_start(Con &con) {
  if (m_cacheBuild != nullptr) {
    free_cache(m_cacheBuild);
    m_cacheBuild = nullptr;
  }
  con.m_cacheBuild = new Cache;
  if (con.m_cacheBuild == nullptr) {
    setError(NoMemError, __LINE__);
    return -1;
  }
  new (con.m_cacheBuild) Cache;
  if (cache_init(con) == -1) return -1;
  return 0;
}

int NdbIndexStatImpl::save_next(Con &con) {
  if (cache_insert(con) == -1) return -1;
  return 0;
}

int NdbIndexStatImpl::save_commit(Con &con) {
  if (cache_commit(con) == -1) return -1;
  m_cacheBuild = con.m_cacheBuild;
  con.m_cacheBuild = nullptr;
  return 0;
}

// cache inline

inline uint NdbIndexStatImpl::Cache::get_keyaddr(uint pos) const {
  assert(pos < m_sampleCount);
  const uint offset = pos * m_addrLen;
  assert(offset + m_addrLen <= m_addrBytes);
  const Uint8 *src = &m_addrArray[offset];
  uint addr = 0;
  switch (m_addrLen) {
    case 4:
      addr += src[3] << 24;
      [[fallthrough]];
    case 3:
      addr += src[2] << 16;
      [[fallthrough]];
    case 2:
      addr += src[1] << 8;
      [[fallthrough]];
    case 1:
      addr += src[0] << 0;
      break;
    default:
      assert(false);
  }
  return addr;
}

inline void NdbIndexStatImpl::Cache::set_keyaddr(uint pos, uint addr) {
  assert(pos < m_sampleCount);
  const uint offset = pos * m_addrLen;
  assert(offset + m_addrLen <= m_addrBytes);
  Uint8 *dst = &m_addrArray[offset];
  switch (m_addrLen) {
    case 4:
      dst[3] = (addr >> 24) & 0xFF;
      [[fallthrough]];
    case 3:
      dst[2] = (addr >> 16) & 0xFF;
      [[fallthrough]];
    case 2:
      dst[1] = (addr >> 8) & 0xFF;
      [[fallthrough]];
    case 1:
      dst[0] = (addr >> 0) & 0xFF;
      break;
    default:
      assert(false);
  }
  assert(get_keyaddr(pos) == addr);
}

inline const Uint8 *NdbIndexStatImpl::Cache::get_keyptr(uint addr) const {
  assert(addr < m_keyBytes);
  return &m_keyArray[addr];
}

inline Uint8 *NdbIndexStatImpl::Cache::get_keyptr(uint addr) {
  assert(addr < m_keyBytes);
  return &m_keyArray[addr];
}

inline const Uint8 *NdbIndexStatImpl::Cache::get_valueptr(uint pos) const {
  assert(pos < m_sampleCount);
  return &m_valueArray[pos * m_valueLen];
}

inline Uint8 *NdbIndexStatImpl::Cache::get_valueptr(uint pos) {
  assert(pos < m_sampleCount);
  return &m_valueArray[pos * m_valueLen];
}

inline void NdbIndexStatImpl::Cache::swap_entry(uint pos1, uint pos2) {
  uint hold_addr;
  Uint8 hold_value[MaxValueBytes];

  hold_addr = get_keyaddr(pos1);
  memcpy(hold_value, get_valueptr(pos1), m_valueLen);
  set_keyaddr(pos1, get_keyaddr(pos2));
  memcpy(get_valueptr(pos1), get_valueptr(pos2), m_valueLen);
  set_keyaddr(pos2, hold_addr);
  memcpy(get_valueptr(pos2), hold_value, m_valueLen);
}

inline double NdbIndexStatImpl::Cache::get_rir1(uint pos) const {
  const Uint8 *ptr = get_valueptr(pos);
  Uint32 n;
  memcpy(&n, &ptr[0], 4);
  double x = (double)n;
  return x;
}

inline double NdbIndexStatImpl::Cache::get_rir1(uint pos1, uint pos2) const {
  assert(pos2 > pos1);
  return get_rir1(pos2) - get_rir1(pos1);
}

inline double NdbIndexStatImpl::Cache::get_rir(uint pos) const {
  double x = (double)m_fragCount * get_rir1(pos);
  return x;
}

inline double NdbIndexStatImpl::Cache::get_rir(uint pos1, uint pos2) const {
  assert(pos2 > pos1);
  return get_rir(pos2) - get_rir(pos1);
}

inline double NdbIndexStatImpl::Cache::get_unq1(uint pos, uint k) const {
  assert(k < m_keyAttrs);
  const Uint8 *ptr = get_valueptr(pos);
  Uint32 n;
  memcpy(&n, &ptr[4 + k * 4], 4);
  double x = (double)n;
  return x;
}

inline double NdbIndexStatImpl::Cache::get_unq1(uint pos1, uint pos2,
                                                uint k) const {
  assert(pos2 > pos1);
  return get_unq1(pos2, k) - get_unq1(pos1, k);
}

static inline double get_unqfactor(uint fragments, double rows,
                                   double uniques_found);

inline double NdbIndexStatImpl::Cache::get_unq(uint pos, uint k,
                                               double *factor) const {
  uint p = m_fragCount;
  double r = get_rir1(pos);
  double u = get_unq1(pos, k);
  double f = get_unqfactor(p, r, u);
  *factor = f;
  double x = f * u;
  return x;
}

inline double NdbIndexStatImpl::Cache::get_unq(uint pos1, uint pos2, uint k,
                                               double *factor) const {
  uint p = m_fragCount;
  double r = get_rir1(pos1, pos2);
  double u = get_unq1(pos1, pos2, k);
  double f = get_unqfactor(p, r, u);
  *factor = f;
  double x = f * u;
  return x;
}

inline double NdbIndexStatImpl::Cache::get_rpk(uint pos, uint k,
                                               double *factor) const {
  return get_rir(pos) / get_unq(pos, k, factor);
}

inline double NdbIndexStatImpl::Cache::get_rpk(uint pos1, uint pos2, uint k,
                                               double *factor) const {
  assert(pos2 > pos1);
  return get_rir(pos1, pos2) / get_unq(pos1, pos2, k, factor);
}

// cache

NdbIndexStatImpl::Cache::Cache() {
  m_valid = false;
  m_keyAttrs = 0;
  m_valueAttrs = 0;
  m_fragCount = 0;
  m_sampleVersion = 0;
  m_sampleCount = 0;
  m_keyBytes = 0;
  m_valueLen = 0;
  m_valueBytes = 0;
  m_addrLen = 0;
  m_addrBytes = 0;
  m_addrArray = nullptr;
  m_keyArray = nullptr;
  m_valueArray = nullptr;
  m_nextClean = nullptr;
  // performance
  m_save_time = 0;
  m_sort_time = 0;
  // in use by query_stat
  m_ref_count = 0;
}

/**
  Implementing the get_unqfactor function
  ---------------------------------------
  One problem to solve is how to calculate the records per key based on
  scanning one fragment. The problem to solve here is a complex problem.
  In the code previously an estimate was attempted that uses the function
  1 + (num_fragments - 1) * (uniques_found /num_rows)^(num_fragments - 1)

  Where this function comes from is unknown, there is no documentation of
  this function other than as a quick fix. It is not extremely bad,
  but also not very accurate.

  Let us consider the problem from the following angle:
  We have a number of rows. Assume that we have a number of records that
  have equal keys. Assume that for a key the number of rows with equal key
  is always the same.

  Now what is the probability that no row with a specific key is not
  placed in the fragment we are scanning. The probability that one row
  is placed in another fragment is (1 - 1/num_fragments). Selection of
  fragment for different rows is assumed to be independent. This is not
  true in the case of that the key contains all parts of the partition
  key, but otherwise this assumption should be ok. It would be very hard
  to create any dependencies here since the selection of fragment is based
  on a complex hash algorithm that is even better at distributing data
  than randomness. So to create dependencies that do not take into account
  the partition key would be extremely hard.

  Thus to place X number keys in other fragments than the one we are scanning
  is (1 - 1/num_fragments)^X. X is the records per key we are searching for.
  We will call this RPK (Records Per Key) for now.

  Assume that the number of unique keys is UNQ. Assume that number of rows
  is ROWS. Now UNQ * RPK = ROWS or RPK = ROWS / UNQ.

  While searching the fragment we derived the number of rows in the fragment
  and the number of unique keys in the fragment. We assume that placement of
  rows is independent and thus ROWS = found_rows_in_fragment * num_fragments.
  The reasoning is the same as above, but here we also assume that the user
  selected a partition key that is sensible.

  Now the following holds:
  UNQ * (1 - P(unique key not in the fragment)) = uniques_found_in_fragment.
  P(unique key not in fragment) we showed above that it is equal to:
  (1 - 1/num_fragments)^RPK.

  From this we derive the following equation:
  ROWS/RPK * (1 - (1 - 1/num_fragments)^RPK)) = uniques_found_in_fragment.

  ROWS = rows_found_in_fragment * num_fragments

  Thus we have an equation with one variable, the RPK variable. This equation
  has no well-known simple solution that can be expressed as a single function.

  But solving it numerically works fine.
  We can e.g. use the original function as a first estimate of RPK and then
  iterate until we have found a solution that is sufficiently accurate.

  This iterative solution solves the problem with reasonable accuracy when
  the number of rows in a fragment exceeds 100. The smaller the number of
  rows per fragment, the more variance gets into the solution since the
  different fragments will have high variance among themselves.

  The proposed solution for small tables is to perform a full table scan
  to derive the result needed. So this solution is only intended for
  tables that are slightly bigger.

  As an example we tried the solution on a cluster with 16 fragments and
  a table with 60M rows. This was performed using a simulation program
  available as test_distinct.c in the source code. It is not built as
  part of MySQL Cluster, to build it simply do e.g. gcc test_distinct.c
  and execute the produced binary with 3 parameters rows, fragments and
  records per key simulated.

  We tested with different values on records per key. We present the results
  below with old and new results.

  rpk 2: old: 1.6  new: 2.0
  rpk 3: old: 2.5  new: 3.0
  rpk 4: old: 3.78 new: 4.0
  rpk 5: old: 5.5  new: 5.0
  rpk 6: old: 7.6  new: 6.0
  rpk 8: old: 12.4 new: 7.99
  rpk 10:old: 16.8 new: 10.01
  rpk 15:old: 23.5 new: 14.99
  rpk 25:old: 31.2 new: 24.99
  rpk 40:old: 43.28new: 40.02
  rpk 80:old: 80.5 new: 80.01
  rpk:200 old: 200 new: 200
  rpk:1000 old: 1000 new: 1000.02

  What happens here is that when rpk is larger than 10 * num_fragments, the
  old algorithm predicts correctly that the number of distinct values in
  the table is equal to the number of distinct values in the searched
  fragment. The reason is that the factor in this case is simply 1.

  Thus there is no reason to perform the iteration in this case since it
  will not improve the already existing solution, actually in this situation
  we can simply set number of unique keys in the table to the value found
  in the searched fragment.

  Now with a table with 10000 rows and 16 fragments we get the following
  results:

  rpk 2: old: 1.6+-0.3 new: 2+-0.3
  rpk 3: old 2.6+-0.4  new: 3.1+-0.3

  We see here that the variance is about the same in the new and old
  solution, but the mean value of the different runs is closer to the
  real mean value. With such little data per fragment it will always
  be difficult to estimate the number of records per key correctly
  using only one fragment. Thus a table like this is at the limit of
  where it makes sense to use scans on only one fragment. It is reasonable
  to use this algorithm down to around 25 rows per fragment. After this
  the variance is too big and it makes better sense to scan the full
  table and build a sorted result. A simple solution to handle those
  tables would be to simply execute the following SQL queries to
  get the data (assuming we have an index on key1, key2).
  SELECT count(*) from (SELECT DISTINCT key1 from t);
  SELECT count(*) from (SELECT DISTINCT key1, key2 from t);
  This will provide the necessary rpk estimates for smaller tables.

  The old algorithm really breaks down when there are very many
  fragments and lots of data (that is a large cluster with lots of
  data). The old algorithm is the function estimator below that is
  now used as a first estimate to start the iterative loop.

  In this case we get the following:
  Rows: 100M
  Fragments: 1000

  Similar numbers to above for rpk < 6.
  rpk 6: old: 11+-1 new: 6+-0.5
  rpk:10: old: 70+-20 new: 9.8+-0.4
  rpk: 20 old: 920+-20 new: 20+-0.5
  rpk: 25 old: 1000+-50 new: 25+-0.2

  After this the old algorithm delivers results around
  1000-1200 even going to rpk = 400 whereas the new
  algorithm continues to be fairly accurate.

  At around rpk = 4000 the accuracy of the old algorithm is
  ok again. But at its worst it is 40x off the grid. The old
  algorithm breaks down especially with very high number of
  fragments whereas the new algorithm delivers good results
  even with many thousands of fragments.

  We also have a special case as mentioned previously. This
  case is when we have a partition key on the column a.

  Now an index on b,a will use the same algorithm for records
  per key on b, but on b,a the estimate from the single
  fragment will be multiplied by the number of fragments since
  we assume that the user has ensured that rows are evenly
  spread among the fragments in the cluster.

  So this means there are 4 cases.

  1) The partition key(s) is part of the key part. =>
     Set UNQ = unique_rows_in_fragment * num_fragments
  2) If number of rows per fragment is smaller than 25, scan
     all fragments instead.
  3) Estimate from old function provides rpk > 10 * num_fragments
     Use old function
  4) Estimate from old function fed into iterative algorithm that
     refines the result to be much more accurate.

  1) is resolved in the set_records_per_key function by multiplying
  the unique values by num_fragments / stored_factor.

  stored_factor is the factor calculated by iterative_solution.
  Thus we need to store this value in the cache to enable
  records per key to be set correctly also for indexes that use
  the partition keys as part of their index columns.
*/
#define NDB_DOUBLE long double
static NDB_DOUBLE ONE = (NDB_DOUBLE)1.0;

static inline double estimator(NDB_DOUBLE fragments, NDB_DOUBLE uniques_found,
                               NDB_DOUBLE rows) {
  NDB_DOUBLE f =
      ONE + (fragments - ONE) * ::powl(uniques_found / rows, fragments - ONE);
  return f;
}

static inline NDB_DOUBLE convert_rpk_to_estimate(NDB_DOUBLE rows,
                                                 NDB_DOUBLE rpk,
                                                 NDB_DOUBLE uniques_found) {
  return rows / (rpk * uniques_found);
}

static inline NDB_DOUBLE prob_key_in_fragment(NDB_DOUBLE fragments,
                                              NDB_DOUBLE rpk) {
  NDB_DOUBLE p_key_not_in_fragment = ONE - (ONE / fragments);
  NDB_DOUBLE p_no_key_in_fragment = powl(p_key_not_in_fragment, rpk);
  NDB_DOUBLE p_key_in_fragment = ONE - p_no_key_in_fragment;
  return p_key_in_fragment;
}

static NDB_DOUBLE iterative_solution(NDB_DOUBLE fragments, NDB_DOUBLE rows,
                                     NDB_DOUBLE uniques_found) {
  const NDB_DOUBLE estimate = estimator(fragments, uniques_found, rows);
  DBUG_PRINT("index_stat", ("iterative_solution: rows: %.2Lf, uniques_found:"
                            " %.2Lf, fragments: %.2Lf, estimate: %.2Lf",
                            rows, uniques_found, fragments, estimate));
  if (uniques_found < (NDB_DOUBLE)0.1) {
    /**
     * The number of unique values are so small that all values are found
     * in all fragments with an extremely high probability.
     * Note that initial 'estimate' calculated above will converge against
     * 'ONE' as 'uniques_found -> 0'. Consider 'estimate' good enough.
     * Note that this early return also avoid a division by zero in
     * calculating est_rpk if 'uniques_found = 0'.
     */
    return estimate;  // Will be close to 1.0
  }
  if (rows / fragments < (NDB_DOUBLE)25.0)  // 2) above
  {
    /**
     * The table is so small that the variance from selecting a small subset
     * of rows is so high that it doesn't really improve the solution to
     * perform any iteration.
     */
    return estimate;
  }

  NDB_DOUBLE est_rpk = rows / (estimate * uniques_found);
  if (est_rpk > fragments * (NDB_DOUBLE)10.0)  // 3) above
  {
    /**
     * This is condition 3) above, there are so many 'rows pr keys'
     * that all values are found in all fragments with an extremely high
     * probability. It doesn't really improve the solution to perform
     * any iteration.
     */
    return estimate;  // Will be close to 1.0
  }
  /**
   * We decide to use 2) above, the iterative approach.
   * In this case we calculate the estimated uniques found based on the
   * estimated RPK.
   *
   * The definition of the end of the iterative loop is defined as when this
   * iteration does estimates the uniques_found within 1.0 of the previous
   * loop iteration. In this case there is very little to gain from continuing
   * the search for an optimum.
   *
   * It is important to remember here that the optimum is based on a random
   * observation from one fragment, thus our predicted RPK can never be better
   * than this observation. Thus all the error in the observation is
   * transported along in this calculation. So there is no need to overdo the
   * accuracy of the calculation.
   *
   * The initial prediction can be way off. To ensure that we quickly come
   * close to the optimum we will move with 50% at the time at first. If we
   * started with a prediction that was too high we will eventually come
   * to a prediction that is smaller. When this happens we will half the
   * increments by which we move the new RPK estimate. In the end we will
   * move in very small steps and eventually the iteration should bring us
   * to the end condition. As a safeguard we ensure that we will never
   * iterat more than 100 times.
   *
   * When returning we will convert the estimated RPK to a factor instead.
   */
  NDB_DOUBLE percent_change = (NDB_DOUBLE)0.5;
  NDB_DOUBLE prev_est_uniques_found = (NDB_DOUBLE)0.0;
  bool prev_decreased = true;
  bool decreased;
  unsigned i = 0;
  bool first = true;
  do {
    NDB_DOUBLE p_key_in_fragment = prob_key_in_fragment(fragments, est_rpk);
    NDB_DOUBLE est_uniques_found = p_key_in_fragment * rows / est_rpk;
    if (!first) {
      if (est_uniques_found < prev_est_uniques_found) {
        if (est_uniques_found + ONE > prev_est_uniques_found) break;
      } else {
        if (est_uniques_found - ONE < prev_est_uniques_found) break;
      }
    }
    first = false;
    if (est_uniques_found < uniques_found) {
      decreased = true;
      est_rpk *= (ONE - percent_change);
    } else {
      decreased = false;
      est_rpk *= (ONE + percent_change);
    }
    if (prev_decreased != decreased) {
      percent_change /= (NDB_DOUBLE)2;
    }
    prev_decreased = decreased;
    prev_est_uniques_found = est_uniques_found;
  } while (++i < 100);
  return convert_rpk_to_estimate(rows, est_rpk, uniques_found);
}

static inline double get_unqfactor(uint fragments, double rows,
                                   double uniques_found) {
  return (double)iterative_solution((NDB_DOUBLE)fragments,
                                    (NDB_DOUBLE)(rows * fragments),
                                    (NDB_DOUBLE)uniques_found);
}

int NdbIndexStatImpl::cache_init(Con &con) {
  Cache &c = *con.m_cacheBuild;
  Head &head = con.m_head;
  Mem *mem = m_mem_handler;

  if (m_keyAttrs == 0) {
    setError(InternalError, __LINE__);
    return -1;
  }
  c.m_keyAttrs = m_keyAttrs;
  c.m_valueAttrs = m_valueAttrs;
  c.m_fragCount = head.m_fragCount;
  c.m_sampleCount = head.m_sampleCount;
  c.m_keyBytes = head.m_keyBytes;
  c.m_valueLen = 4 + c.m_keyAttrs * 4;
  c.m_valueBytes = c.m_sampleCount * c.m_valueLen;
  c.m_addrLen =
      c.m_keyBytes < (1 << 8)
          ? 1
          : c.m_keyBytes < (1 << 16) ? 2 : c.m_keyBytes < (1 << 24) ? 3 : 4;
  c.m_addrBytes = c.m_sampleCount * c.m_addrLen;

  // wl4124_todo omit addrArray if keys have fixed size
  c.m_addrArray = (Uint8 *)mem->mem_alloc(c.m_addrBytes);
  if (c.m_addrArray == nullptr) {
    setError(NoMemError, __LINE__);
    return -1;
  }
  c.m_keyArray = (Uint8 *)mem->mem_alloc(c.m_keyBytes);
  if (c.m_keyArray == nullptr) {
    setError(NoMemError, __LINE__);
    return -1;
  }
  c.m_valueArray = (Uint8 *)mem->mem_alloc(c.m_valueBytes);
  if (c.m_valueArray == nullptr) {
    setError(NoMemError, __LINE__);
    return -1;
  }
  return 0;
}

int NdbIndexStatImpl::cache_insert(Con &con) {
  Cache &c = *con.m_cacheBuild;

  const uint nextPos = con.m_cachePos + 1;
  if (nextPos > c.m_sampleCount) {
    setError(InternalError, __LINE__);
    return -1;
  }
  assert(m_keyData.is_full());
  const uint keyLen = m_keyData.get_data_len();
  const uint nextKeyOffset = con.m_cacheKeyOffset + keyLen;
  if (nextKeyOffset > c.m_keyBytes) {
    setError(InternalError, __LINE__);
    return -1;
  }
  if (m_valueData.get_data_len() != c.m_valueLen) {
    setError(InternalError, __LINE__);
    return -1;
  }
  const uint nextValueOffset = con.m_cacheValueOffset + c.m_valueLen;
  if (nextValueOffset > c.m_valueBytes) {
    setError(InternalError, __LINE__);
    return -1;
  }

  c.set_keyaddr(con.m_cachePos, con.m_cacheKeyOffset);
  con.m_cachePos = nextPos;

  Uint8 *cacheKeyPtr = &c.m_keyArray[con.m_cacheKeyOffset];
  const Uint8 *keyPtr = (const Uint8 *)m_keyData.get_data_buf();
  memcpy(cacheKeyPtr, keyPtr, keyLen);
  con.m_cacheKeyOffset = nextKeyOffset;

  Uint8 *cacheValuePtr = &c.m_valueArray[con.m_cacheValueOffset];
  const Uint8 *valuePtr = (const Uint8 *)m_valueData.get_data_buf();
  memcpy(cacheValuePtr, valuePtr, c.m_valueLen);
  con.m_cacheValueOffset = nextValueOffset;

  // verify sanity
  {
    const Uint8 *rir_ptr = &cacheValuePtr[0];
    Uint32 rir;
    memcpy(&rir, rir_ptr, 4);
    if (!(rir != 0)) {
      setError(InvalidCache, __LINE__);
      return -1;
    }
    Uint32 unq_prev = 0;
    for (uint k = 0; k < c.m_keyAttrs; k++) {
      Uint8 *unq_ptr = &cacheValuePtr[4 + k * 4];
      Uint32 unq;
      memcpy(&unq, unq_ptr, 4);
      if (!(unq != 0)) {
        setError(InvalidCache, __LINE__);
        return -1;
      }
      if (!(rir >= unq)) {
        setError(InvalidCache, __LINE__);
        return -1;
      }
      if (!(unq >= unq_prev)) {
        setError(InvalidCache, __LINE__);
        return -1;
      }
      unq_prev = unq;
    }
  }
  return 0;
}

int NdbIndexStatImpl::cache_commit(Con &con) {
  Cache &c = *con.m_cacheBuild;
  Head &head = con.m_head;
  if (con.m_cachePos != c.m_sampleCount) {
    setError(InternalError, __LINE__);
    return -1;
  }
  if (con.m_cacheKeyOffset != c.m_keyBytes) {
    setError(InternalError, __LINE__);
    return -1;
  }
  if (con.m_cacheValueOffset != c.m_valueBytes) {
    setError(InternalError, __LINE__);
    return -1;
  }
  c.m_sampleVersion = head.m_sampleVersion;
  if (cache_sort(c) == -1) return -1;
  if (cache_verify(c) == -1) return -1;
  c.m_valid = true;
  return 0;
}

int NdbIndexStatImpl::cache_cmpaddr(const Cache &c, uint addr1,
                                    uint addr2) const {
  const Uint8 *key1 = c.get_keyptr(addr1);
  const Uint8 *key2 = c.get_keyptr(addr2);

  NdbPack::DataC keyData1(m_keySpec, false);
  NdbPack::DataC keyData2(m_keySpec, false);
  keyData1.set_buf(key1, c.m_keyBytes - addr1, c.m_keyAttrs);
  keyData2.set_buf(key2, c.m_keyBytes - addr2, c.m_keyAttrs);

  Uint32 num_eq;
  int res = keyData1.cmp(keyData2, c.m_keyAttrs, num_eq);
  assert(addr1 == addr2 || res != 0);
  return res;
}

int NdbIndexStatImpl::cache_cmppos(const Cache &c, uint pos1, uint pos2) const {
  uint addr1 = c.get_keyaddr(pos1);
  uint addr2 = c.get_keyaddr(pos2);
  return cache_cmpaddr(c, addr1, addr2);
}

/*
 * Sort addr and value arrays via key values.  The samples were inserted
 * in key order and were read back via index scan so they may be nearly
 * ordered at first.  This is quicksort worst case so we do not use it.
 */
int NdbIndexStatImpl::cache_sort(Cache &c) {
  if (c.m_sampleCount > 1) cache_hsort(c);
  return 0;
}

// insertion sort - expensive
void NdbIndexStatImpl::cache_isort(Cache &c) {
  int n = c.m_sampleCount;
  for (int i = 1; i < n; i++) {
    for (int j = i - 1; j >= 0; j--) {
      int res = cache_cmppos(c, j, j + 1);
      if (res < 0) break;
      c.swap_entry(j, j + 1);
    }
  }
}

// heapsort
void NdbIndexStatImpl::cache_hsort(Cache &c) {
  int count = c.m_sampleCount;
  int i;

  // highest entry which can have children
  i = count / 2;

  // make into heap (binary tree where child < parent)
  while (i >= 0) {
    cache_hsort_sift(c, i, count);
    i--;
  }

  // verify is too expensive to enable under VM_TRACE

#ifdef ndb_index_stat_hsort_verify
  cache_hsort_verify(c, count);
#endif

  // sort
  i = count - 1;
  while (i > 0) {
    // move current max to proper position
    c.swap_entry(0, i);

    // restore heap property for the rest
    cache_hsort_sift(c, 0, i);
#ifdef ndb_index_stat_hsort_verify
    cache_hsort_verify(c, i);
#endif
    i--;
  }
}

void NdbIndexStatImpl::cache_hsort_sift(Cache &c, int i, int count) {
  int parent = i;

  while (1) {
    // left child if any
    int child = parent * 2 + 1;
    if (!(child < count)) break;

    // replace by right child if bigger
    if (child + 1 < count && cache_cmppos(c, child, child + 1) < 0)
      child = child + 1;

    // done if both children are less than parent
    if (cache_cmppos(c, child, parent) < 0) break;

    c.swap_entry(parent, child);
    parent = child;
  }
}

#ifdef ndb_index_stat_hsort_verify
// verify heap property
void NdbIndexStatImpl::cache_hsort_verify(Cache &c, int count) {
  for (int i = 0; i < count; i++) {
    int parent = i;
    int child1 = 2 * i + 1;
    int child2 = 2 * i + 2;
    if (child1 < count) {
      assert(cache_cmppos(c, child1, parent) < 0);
    }
    if (child2 < count) {
      assert(cache_cmppos(c, child2, parent) < 0);
    }
  }
}
#endif

int NdbIndexStatImpl::cache_verify(const Cache &c) {
  for (uint pos1 = 0; pos1 < c.m_sampleCount; pos1++) {
    const uint addr1 = c.get_keyaddr(pos1);
    const Uint8 *key1 = c.get_keyptr(addr1);
    NdbPack::DataC keyData1(m_keySpec, false);
    keyData1.set_buf(key1, c.m_keyBytes - addr1, c.m_keyAttrs);
    uint pos2 = pos1 + 1;
    if (pos2 < c.m_sampleCount) {
      const uint addr2 = c.get_keyaddr(pos2);
      const Uint8 *key2 = c.get_keyptr(addr2);
      NdbPack::DataC keyData2(m_keySpec, false);
      keyData2.set_buf(key2, c.m_keyBytes - addr2, c.m_keyAttrs);
      Uint32 num_eq;
      int res = keyData1.cmp(keyData2, c.m_keyAttrs, num_eq);
      if (!(res < 0)) {
        setError(InvalidCache, __LINE__);
        return -1;
      }
      const Uint8 *ptr1 = c.get_valueptr(pos1);
      const Uint8 *ptr2 = c.get_valueptr(pos2);
      Uint32 rir1;
      Uint32 rir2;
      memcpy(&rir1, &ptr1[0], 4);
      memcpy(&rir2, &ptr2[0], 4);
      if (!(rir1 < rir2)) {
        setError(InvalidCache, __LINE__);
        return -1;
      }
      for (uint k = 0; k < c.m_keyAttrs; k++) {
        Uint32 unq1;
        Uint32 unq2;
        memcpy(&unq1, &ptr1[4 + k * 4], 4);
        memcpy(&unq2, &ptr2[4 + k * 4], 4);
        if (!(unq1 <= unq2)) {
          setError(InvalidCache, __LINE__);
          return -1;
        }
        if (k == c.m_keyAttrs - 1 && !(unq1 < unq2)) {
          setError(InvalidCache, __LINE__);
          return -1;
        }
      }
    }
  }
  return 0;
}

void NdbIndexStatImpl::move_cache() {
  Cache *cacheTmp = m_cacheQuery;

  NdbMutex_Lock(m_query_mutex);
  m_cacheQuery = m_cacheBuild;
  NdbMutex_Unlock(m_query_mutex);
  m_cacheBuild = nullptr;

  if (cacheTmp != nullptr) {
    cacheTmp->m_nextClean = m_cacheClean;
    m_cacheClean = cacheTmp;
  }
}

void NdbIndexStatImpl::clean_cache() {
  while (m_cacheClean != nullptr) {
    NdbIndexStatImpl::Cache *tmp = m_cacheClean;
    m_cacheClean = tmp->m_nextClean;
    free_cache(tmp);
  }
}

void NdbIndexStatImpl::free_cache(Cache *c) {
  Mem *mem = m_mem_handler;
  mem->mem_free(c->m_addrArray);
  mem->mem_free(c->m_keyArray);
  mem->mem_free(c->m_valueArray);
  delete c;
}

void NdbIndexStatImpl::free_cache() {
  // twice to move all to clean list
  move_cache();
  move_cache();
  clean_cache();
}

// cache dump

NdbIndexStatImpl::CacheIter::CacheIter(const NdbIndexStatImpl &impl)
    : m_keyData(impl.m_keySpec, false), m_valueData(impl.m_valueSpec, false) {
  m_keyCount = impl.m_keyAttrs;
  m_sampleCount = 0;
  m_sampleIndex = 0;
}

int NdbIndexStatImpl::dump_cache_start(CacheIter &iter) {
  if (m_cacheQuery == nullptr) {
    setError(UsageError, __LINE__);
    return -1;
  }
  const Cache &c = *m_cacheQuery;
  new (&iter) CacheIter(*this);
  iter.m_sampleCount = c.m_sampleCount;
  iter.m_sampleIndex = ~(Uint32)0;
  return 0;
}

bool NdbIndexStatImpl::dump_cache_next(CacheIter &iter) {
  if (iter.m_sampleIndex == ~(Uint32)0)
    iter.m_sampleIndex = 0;
  else
    iter.m_sampleIndex++;
  if (iter.m_sampleIndex >= iter.m_sampleCount) return false;
  const Cache &c = *m_cacheQuery;
  const uint pos = iter.m_sampleIndex;
  const uint addr = c.get_keyaddr(pos);
  const Uint8 *key = c.get_keyptr(addr);
  const Uint8 *value = c.get_valueptr(pos);
  iter.m_keyData.set_buf(key, c.m_keyBytes - addr, c.m_keyAttrs);
  iter.m_valueData.set_buf(value, c.m_valueLen, c.m_valueAttrs);
  return true;
}

// bound

int NdbIndexStatImpl::finalize_bound(Bound &bound) {
  assert(bound.m_type == 0 || bound.m_type == 1);
  int side = 0;
  if (bound.m_data.get_cnt() == 0) {
    if (bound.m_strict != -1) {
      setError(UsageError, __LINE__);
      return -1;
    }
  } else {
    if (bound.m_strict == -1) {
      setError(UsageError, __LINE__);
      return -1;
    }
    if (bound.m_type == 0)
      side = bound.m_strict ? +1 : -1;
    else
      side = bound.m_strict ? -1 : +1;
  }
  if (bound.m_bound.finalize(side) == -1) {
    setError(UsageError, __LINE__);
    return -1;
  }
  return 0;
}

// range

int NdbIndexStatImpl::convert_range(
    Range &range, const NdbRecord *key_record,
    const NdbIndexScanOperation::IndexBound *ib) {
  if (ib == nullptr) return 0;
  if (ib->low_key_count == 0 && ib->high_key_count == 0) return 0;
  for (uint j = 0; j <= 1; j++) {
    Bound &bound = j == 0 ? range.m_bound1 : range.m_bound2;
    bound.m_bound.reset();
    const char *key = j == 0 ? ib->low_key : ib->high_key;
    const uint key_count = j == 0 ? ib->low_key_count : ib->high_key_count;
    const bool inclusive = j == 0 ? ib->low_inclusive : ib->high_inclusive;
    Uint32 len_out;
    for (uint i = 0; i < key_count; i++) {
      const uint i2 = key_record->key_indexes[i];
      require(i2 < key_record->noOfColumns);
      const NdbRecord::Attr &attr = key_record->columns[i2];
      if (!attr.is_null(key)) {
        const char *data = key + attr.offset;
        char buf[256];
        if (attr.flags & NdbRecord::IsMysqldShrinkVarchar) {
          Uint32 len;
          if (!attr.shrink_varchar(key, len, buf)) {
            setError(InternalError, __LINE__);
            return -1;
          }
          data = buf;
        }
        if (bound.m_data.add(data, &len_out) == -1) {
          setError(InternalError, __LINE__, bound.m_data.get_error_code());
          return -1;
        }
      } else {
        if (bound.m_data.add_null(&len_out) == -1) {
          setError(InternalError, __LINE__, bound.m_data.get_error_code());
          return -1;
        }
      }
    }
    if (key_count > 0) bound.m_strict = !inclusive;
    if (finalize_bound(bound) == -1) {
      setError(InternalError, __LINE__);
      return -1;
    }
  }

#ifdef VM_TRACE
#ifdef NDB_USE_GET_ENV
  {
    const char *p = NdbEnv_GetEnv("NDB_INDEX_STAT_RANGE_ERROR", (char *)0, 0);
    if (p != 0 && strchr("1Y", p[0]) != 0) {
      if (rand() % 10 == 0) {
        setError(InternalError, __LINE__, NdbIndexStat::InternalError);
        return -1;
      }
    }
  }
#endif
#endif

  return 0;
}

// query

// normalize values to >= 1.0
void NdbIndexStatImpl::query_normalize(const Cache &c, StatValue &value) {
  if (!value.m_empty) {
    if (value.m_rir < 1.0) value.m_rir = 1.0;
    for (uint k = 0; k < c.m_keyAttrs; k++) {
      if (value.m_unq[k] < 1.0) value.m_unq[k] = 1.0;
    }
  } else {
    value.m_rir = 1.0;
    for (uint k = 0; k < c.m_keyAttrs; k++) {
      value.m_unq[k] = 1.0;
      value.m_unq_factor[k] = 1.0;
    }
  }
}

int NdbIndexStatImpl::query_stat(const Range &range, Stat &stat) {
  NdbMutex_Lock(m_query_mutex);
  if (unlikely(m_cacheQuery == nullptr)) {
    NdbMutex_Unlock(m_query_mutex);
    setError(UsageError, __LINE__);
    return -1;
  }
  const Cache &c = *m_cacheQuery;
  if (unlikely(!c.m_valid)) {
    NdbMutex_Unlock(m_query_mutex);
    setError(InvalidCache, __LINE__);
    return -1;
  }
  c.m_ref_count++;
  NdbMutex_Unlock(m_query_mutex);

#ifdef VM_TRACE
#ifdef NDB_USE_GET_ENV
  {
    const char *p = NdbEnv_GetEnv("NDB_INDEX_STAT_SLOW_QUERY", (char *)0, 0);
    if (p != 0 && strchr("1Y", p[0]) != 0) {
      int ms = 1 + rand() % 20;
      NdbSleep_MilliSleep(ms);
    }
  }
#endif
#endif

  // clients run these in parallel
  query_interpolate(c, range, stat);
  query_normalize(c, stat.m_value);

  NdbMutex_Lock(m_query_mutex);
  assert(c.m_ref_count != 0);
  c.m_ref_count--;
  NdbMutex_Unlock(m_query_mutex);
  return 0;
}

void NdbIndexStatImpl::query_interpolate(const Cache &c, const Range &range,
                                         Stat &stat) {
  const uint keyAttrs = c.m_keyAttrs;
  StatValue &value = stat.m_value;
  value.m_empty = false;
  stat.m_rule[0] = "-";
  stat.m_rule[1] = "-";
  stat.m_rule[2] = "-";
  value.m_num_fragments = c.m_fragCount;

  if (c.m_sampleCount == 0) {
    stat.m_rule[0] = "r1.1";
    value.m_num_rows = 0;
    value.m_empty = true;
    return;
  }
  const uint posMIN = 0;
  const uint posMAX = c.m_sampleCount - 1;
  value.m_num_rows = c.get_rir1(posMAX);
  DBUG_PRINT("index_stat",
             ("rows: %u, frags: %u, samples: %u", value.m_num_rows,
              value.m_num_fragments, c.m_sampleCount));

  const Bound &bound1 = range.m_bound1;
  const Bound &bound2 = range.m_bound2;
  if (bound1.m_data.is_empty() && bound2.m_data.is_empty()) {
    stat.m_rule[0] = "r1.2";
    value.m_rir = c.get_rir(posMAX);
    DBUG_PRINT("index_stat", ("m_rir: %.2f", value.m_rir));
    for (uint k = 0; k < keyAttrs; k++) {
      double factor;
      value.m_unq[k] = c.get_unq(posMAX, k, &factor);
      value.m_unq_factor[k] = factor;
      DBUG_PRINT("index_stat:1.2", ("m_unq[%u]: %.2f, factor: %.2f", k,
                                    value.m_unq[k], value.m_unq_factor[k]));
    }
    return;
  }

  StatBound &stat1 = stat.m_stat1;
  StatBound &stat2 = stat.m_stat2;
  if (!bound1.m_data.is_empty()) {
    query_interpolate(c, bound1, stat1);
    query_normalize(c, stat1.m_value);
    stat.m_rule[1] = stat1.m_rule;
  }
  if (!bound2.m_data.is_empty()) {
    query_interpolate(c, bound2, stat2);
    query_normalize(c, stat2.m_value);
    stat.m_rule[2] = stat2.m_rule;
  }

  const StatValue &value1 = stat1.m_value;
  const StatValue &value2 = stat2.m_value;
  const uint posL1 = stat1.m_pos - 1;  // invalid if posH1 == posMIN
  const uint posH1 = stat1.m_pos;
  const uint posL2 = stat2.m_pos - 1;  // invalid if posH2 == posMIN
  const uint posH2 = stat2.m_pos;
  const uint cnt1 = bound1.m_data.get_cnt();
  const uint cnt2 = bound2.m_data.get_cnt();
  const uint mincnt = std::min(cnt1, cnt2);
  Uint32 numEq = 0;  // of bound1,bound2

  if (bound1.m_data.is_empty()) {
    stat.m_rule[0] = "r1.3";
    value.m_rir = value2.m_rir;
    for (uint k = 0; k < keyAttrs; k++) {
      value.m_unq[k] = value2.m_unq[k];
      value.m_unq_factor[k] = value2.m_unq_factor[k];
    }
    return;
  }
  if (bound2.m_data.is_empty()) {
    stat.m_rule[0] = "r1.4";
    value.m_rir = c.get_rir(posMAX) - value1.m_rir;
    for (uint k = 0; k < keyAttrs; k++) {
      double factor;
      value.m_unq[k] = c.get_unq(posMAX, k, &factor) - value1.m_unq[k];
      value.m_unq_factor[k] = factor;
    }
    return;
  }
  if (posH1 > posH2) {
    stat.m_rule[0] = "r1.5";
    value.m_empty = true;
    return;
  }
  // also returns number of equal initial components
  if (bound1.m_bound.cmp(bound2.m_bound, mincnt, numEq) >= 0) {
    stat.m_rule[0] = "r1.6";
    value.m_empty = true;
    return;
  }
  if (posH1 == posMIN) {
    stat.m_rule[0] = "r1.7";
    value.m_rir = value2.m_rir - value1.m_rir;
    for (uint k = 0; k < keyAttrs; k++) {
      value.m_unq[k] = value2.m_unq[k] - value1.m_unq[k];
      value.m_unq_factor[k] = value2.m_unq_factor[k];
    }
    return;
  }
  if (posH2 == posMAX + 1) {
    stat.m_rule[0] = "r1.8";
    value.m_rir = value2.m_rir - value1.m_rir;
    for (uint k = 0; k < keyAttrs; k++) {
      value.m_unq[k] = value2.m_unq[k] - value1.m_unq[k];
      value.m_unq_factor[k] = value2.m_unq_factor[k];
    }
    return;
  }
  if (posL1 == posL2) {
    assert(posH1 == posH2);
    if (cnt1 == keyAttrs && cnt2 == keyAttrs && numEq == keyAttrs) {
      stat.m_rule[0] = "r2.1";
      assert(bound1.m_bound.get_side() == -1 &&
             bound2.m_bound.get_side() == +1);
      assert(stat1.m_numEqL < keyAttrs && stat2.m_numEqH < keyAttrs);
      {
        double factor;
        value.m_rir = c.get_rpk(posL1, posH1, keyAttrs - 1, &factor);
      }
      for (uint k = 0; k < keyAttrs; k++) {
        double factor;
        value.m_unq[k] = value.m_rir / c.get_rpk(posL1, posH1, k, &factor);
        value.m_unq_factor[k] = factor;
      }
      return;
    }
    if (numEq != 0) {
      stat.m_rule[0] = "r2.2";
      // skip for now
    }
    if (true) {
      stat.m_rule[0] = "r2.3";
      const double w = 0.5;
      value.m_rir = w * c.get_rir(posL1, posH1);
      for (uint k = 0; k < keyAttrs; k++) {
        double factor;
        value.m_unq[k] = w * c.get_unq(posL1, posH1, k, &factor);
        value.m_unq_factor[k] = factor;
      }
      return;
    }
  }
  if (posH1 == posL2) {
    if (cnt1 == keyAttrs && cnt2 == keyAttrs && numEq == keyAttrs) {
      stat.m_rule[0] = "r3.1";
      assert(bound1.m_bound.get_side() == -1 &&
             bound2.m_bound.get_side() == +1);
      assert(stat1.m_numEqH == keyAttrs && stat2.m_numEqL == keyAttrs);
      value.m_rir = value2.m_rir - value1.m_rir;
      for (uint k = 0; k < keyAttrs; k++) {
        value.m_unq[k] = value2.m_unq[k] - value1.m_unq[k];
        value.m_unq_factor[k] = value2.m_unq_factor[k];
      }
      return;
    }
    if (numEq != 0) {
      stat.m_rule[0] = "r3.2";
      // skip for now
    }
    if (true) {
      stat.m_rule[0] = "r3.3";
      const double w = 0.5;
      value.m_rir = w * c.get_rir(posL1, posH1);
      for (uint k = 0; k < keyAttrs; k++) {
        double factor;
        value.m_unq[k] = w * c.get_unq(posL1, posH1, k, &factor);
        value.m_unq_factor[k] = factor;
      }
      return;
    }
  }
  if (true) {
    stat.m_rule[0] = "r4";
    value.m_rir = value2.m_rir - value1.m_rir;
    for (uint k = 0; k < keyAttrs; k++) {
      value.m_unq[k] = value2.m_unq[k] - value1.m_unq[k];
      value.m_unq_factor[k] = value2.m_unq_factor[k];
    }
    return;
  }
}

void NdbIndexStatImpl::query_interpolate(const Cache &c, const Bound &bound,
                                         StatBound &stat) {
  const uint keyAttrs = c.m_keyAttrs;
  StatValue &value = stat.m_value;
  value.m_empty = false;
  stat.m_rule = "-";

  query_search(c, bound, stat);

  const uint posMIN = 0;
  const uint posMAX = c.m_sampleCount - 1;
  const uint posL = stat.m_pos - 1;  // invalid if posH == posMIN
  const uint posH = stat.m_pos;
  const uint cnt = bound.m_data.get_cnt();
  const int side = bound.m_bound.get_side();

  if (posH == posMIN) {
    if (cnt == keyAttrs && cnt == stat.m_numEqH) {
      stat.m_rule = "b1.1";
      assert(side == -1);
      {
        double factor;
        value.m_rir =
            c.get_rir(posMIN) - c.get_rpk(posMIN, keyAttrs - 1, &factor);
      }
      for (uint k = 0; k < keyAttrs; k++) {
        double factor;
        value.m_unq[k] = c.get_unq(posMIN, k, &factor) - 1;
        value.m_unq_factor[k] = factor;
      }
      return;
    }
    if (true) {
      stat.m_rule = "b1.2";
      value.m_empty = true;
      return;
    }
  }
  if (posH == posMAX + 1) {
    stat.m_rule = "b2";
    value.m_rir = c.get_rir(posMAX);
    for (uint k = 0; k < keyAttrs; k++) {
      double factor;
      value.m_unq[k] = c.get_unq(posMAX, k, &factor);
      value.m_unq_factor[k] = factor;
    }
    return;
  }
  if (cnt == keyAttrs && cnt == stat.m_numEqL) {
    stat.m_rule = "b3.1";
    assert(side == +1);
    value.m_rir = c.get_rir(posL);
    for (uint k = 0; k < keyAttrs; k++) {
      double factor;
      value.m_unq[k] = c.get_unq(posL, k, &factor);
      value.m_unq_factor[k] = factor;
    }
    return;
  }
  if (cnt == keyAttrs && cnt == stat.m_numEqH && side == +1) {
    stat.m_rule = "b3.2";
    value.m_rir = c.get_rir(posH);
    for (uint k = 0; k < keyAttrs; k++) {
      double factor;
      value.m_unq[k] = c.get_unq(posH, k, &factor);
      value.m_unq_factor[k] = factor;
    }
    return;
  }
  if (cnt == keyAttrs && cnt == stat.m_numEqH && side == -1) {
    stat.m_rule = "b3.3";
    double u;
    {
      double factor;
      u = c.get_unq(posL, posH, keyAttrs - 1, &factor);
    }
    const double wL = 1.0 / u;
    const double wH = 1.0 - wL;
    value.m_rir = wL * c.get_rir(posL) + wH * c.get_rir(posH);
    for (uint k = 0; k < keyAttrs; k++) {
      double factor;
      value.m_unq[k] =
          wL * c.get_unq(posL, k, &factor) + wH * c.get_unq(posH, k, &factor);
      value.m_unq_factor[k] = factor;
    }
    return;
  }
  if (true) {
    stat.m_rule = "b4";
    const double wL = 0.5;
    const double wH = 0.5;
    value.m_rir = wL * c.get_rir(posL) + wH * c.get_rir(posH);
    for (uint k = 0; k < keyAttrs; k++) {
      double factor;
      value.m_unq[k] =
          wL * c.get_unq(posL, k, &factor) + wH * c.get_unq(posH, k, &factor);
      value.m_unq_factor[k] = factor;
    }
    return;
  }
}

void NdbIndexStatImpl::query_search(const Cache &c, const Bound &bound,
                                    StatBound &stat) {
  assert(c.m_sampleCount > 0);
  assert(!bound.m_data.is_empty());
  Uint32 numEq;

  int lo = -1;
  int hi = c.m_sampleCount;
  while (hi - lo > 1) {
    int j = (hi + lo) / 2;
    assert(lo < j && j < hi);
    int res = query_keycmp(c, bound, j, numEq);
    if (res < 0)
      lo = j;
    else if (res > 0)
      hi = j;
    else {
      assert(false);
      return;
    }
  }
  assert(hi - lo == 1);
  stat.m_pos = hi;

  if (stat.m_pos > 0) {
    (void)query_keycmp(c, bound, stat.m_pos - 1, stat.m_numEqL);
  }
  if (stat.m_pos < c.m_sampleCount) {
    (void)query_keycmp(c, bound, stat.m_pos, stat.m_numEqH);
  }
}

// return <0/>0 for key before/after bound
int NdbIndexStatImpl::query_keycmp(const Cache &c, const Bound &bound, uint pos,
                                   Uint32 &numEq) {
  const uint addr = c.get_keyaddr(pos);
  const Uint8 *key = c.get_keyptr(addr);
  NdbPack::DataC keyData(m_keySpec, false);
  keyData.set_buf(key, c.m_keyBytes - addr, c.m_keyAttrs);
  // reverse result for key vs bound
  Uint32 cnt = bound.m_bound.get_data().get_cnt();
  int res = (-1) * bound.m_bound.cmp(keyData, cnt, numEq);
  return res;
}

// events and polling

int NdbIndexStatImpl::create_sysevents(Ndb *ndb) {
  Sys sys(this, ndb);
  NdbDictionary::Dictionary *const dic = ndb->getDictionary();

  if (check_systables(sys) == -1) return -1;
  const NdbDictionary::Table *tab = sys.m_headtable;
  require(tab != nullptr);

  const char *const evname = NDB_INDEX_STAT_HEAD_EVENT;
  NdbDictionary::Event ev(evname, *tab);
  ev.addTableEvent(NdbDictionary::Event::TE_INSERT);
  ev.addTableEvent(NdbDictionary::Event::TE_DELETE);
  ev.addTableEvent(NdbDictionary::Event::TE_UPDATE);
  for (int i = 0; i < tab->getNoOfColumns(); i++) ev.addEventColumn(i);
  ev.setReport(NdbDictionary::Event::ER_UPDATED);

  if (dic->createEvent(ev) == -1) {
    setError(dic->getNdbError().code, __LINE__);
    return -1;
  }
  return 0;
}

int NdbIndexStatImpl::drop_sysevents(Ndb *ndb) {
  Sys sys(this, ndb);
  NdbDictionary::Dictionary *const dic = ndb->getDictionary();

  if (check_systables(sys) == -1) return -1;

  const char *const evname = NDB_INDEX_STAT_HEAD_EVENT;
  if (dic->dropEvent(evname) == -1) {
    int code = dic->getNdbError().code;
    if (code != 4710) {
      setError(dic->getNdbError().code, __LINE__);
      return -1;
    }
  }
  return 0;
}

int NdbIndexStatImpl::check_sysevents(Ndb *ndb) {
  Sys sys(this, ndb);

  if (check_systables(sys) == -1) return -1;

  NdbDictionary::Event_ptr ev(
      ndb->getDictionary()->getEvent(NDB_INDEX_STAT_HEAD_EVENT));
  if (ev == nullptr) {
    setError(ndb->getDictionary()->getNdbError().code, __LINE__);
    return -1;
  }
  return 0;
}

int NdbIndexStatImpl::create_listener(Ndb *ndb) {
  if (m_eventOp != nullptr) {
    setError(UsageError, __LINE__);
    return -1;
  }
  const char *const evname = NDB_INDEX_STAT_HEAD_EVENT;
  m_eventOp = ndb->createEventOperation(evname);
  if (m_eventOp == nullptr) {
    setError(ndb->getNdbError().code, __LINE__);
    return -1;
  }

  // all columns are non-nullable
  Head &head = m_facadeHead;
  if (m_eventOp->getValue("index_id", (char *)&head.m_indexId) == nullptr ||
      m_eventOp->getValue("index_version", (char *)&head.m_indexVersion) ==
          nullptr ||
      m_eventOp->getValue("table_id", (char *)&head.m_tableId) == nullptr ||
      m_eventOp->getValue("frag_count", (char *)&head.m_fragCount) == nullptr ||
      m_eventOp->getValue("value_format", (char *)&head.m_valueFormat) ==
          nullptr ||
      m_eventOp->getValue("sample_version", (char *)&head.m_sampleVersion) ==
          nullptr ||
      m_eventOp->getValue("load_time", (char *)&head.m_loadTime) == nullptr ||
      m_eventOp->getValue("sample_count", (char *)&head.m_sampleCount) ==
          nullptr ||
      m_eventOp->getValue("key_bytes", (char *)&head.m_keyBytes) == nullptr) {
    setError(m_eventOp->getNdbError().code, __LINE__);
    return -1;
  }
  // wl4124_todo why this
  static Head xxx;
  if (m_eventOp->getPreValue("index_id", (char *)&xxx.m_indexId) == nullptr ||
      m_eventOp->getPreValue("index_version", (char *)&xxx.m_indexVersion) ==
          nullptr ||
      m_eventOp->getPreValue("table_id", (char *)&xxx.m_tableId) == nullptr ||
      m_eventOp->getPreValue("frag_count", (char *)&xxx.m_fragCount) ==
          nullptr ||
      m_eventOp->getPreValue("value_format", (char *)&xxx.m_valueFormat) ==
          nullptr ||
      m_eventOp->getPreValue("sample_version", (char *)&xxx.m_sampleVersion) ==
          nullptr ||
      m_eventOp->getPreValue("load_time", (char *)&xxx.m_loadTime) == nullptr ||
      m_eventOp->getPreValue("sample_count", (char *)&xxx.m_sampleCount) ==
          nullptr ||
      m_eventOp->getPreValue("key_bytes", (char *)&xxx.m_keyBytes) == nullptr) {
    setError(m_eventOp->getNdbError().code, __LINE__);
    return -1;
  }
  return 0;
}

int NdbIndexStatImpl::execute_listener(Ndb * /*ndb*/) {
  if (m_eventOp == nullptr) {
    setError(UsageError, __LINE__);
    return -1;
  }
  if (m_eventOp->execute() == -1) {
    setError(m_eventOp->getNdbError().code, __LINE__);
    return -1;
  }
  return 0;
}

int NdbIndexStatImpl::poll_listener(Ndb *ndb, int max_wait_ms) {
  int ret;
  if ((ret = ndb->pollEvents(max_wait_ms)) < 0) {
    setError(ndb->getNdbError().code, __LINE__);
    return -1;
  }
  return (ret == 0 ? 0 : 1);
}

int NdbIndexStatImpl::next_listener(Ndb *ndb) {
  NdbEventOperation *op = ndb->nextEvent();
  if (op == nullptr) return 0;

  Head &head = m_facadeHead;
  head.m_eventType = (int)op->getEventType();
  return 1;
}

int NdbIndexStatImpl::drop_listener(Ndb *ndb) {
  if (m_eventOp != nullptr) {
    // NOTE! dropEventoperation always return 0
    int ret;
    (void)ret;  // USED
    ret = ndb->dropEventOperation(m_eventOp);
    assert(ret == 0);
    m_eventOp = nullptr;
  }
  return 0;
}

// mem alloc - default impl

NdbIndexStatImpl::MemDefault::MemDefault() {}

NdbIndexStatImpl::MemDefault::~MemDefault() {}

void *NdbIndexStatImpl::MemDefault::mem_alloc(UintPtr size) {
  void *ptr = malloc(size);
  return ptr;
}

void NdbIndexStatImpl::MemDefault::mem_free(void *ptr) {
  if (ptr != nullptr) free(ptr);
}

// error

void NdbIndexStatImpl::setError(int code, int line, int extra) {
  if (code == 0) code = InternalError;
  m_error.code = code;
  m_error.line = line;
  m_error.extra = extra;
#ifdef VM_TRACE
#ifdef NDB_USE_GET_ENV
  const char *p = NdbEnv_GetEnv("NDB_INDEX_STAT_ABORT_ON_ERROR", (char *)0, 0);
  if (p != 0 && strchr("1Y", p[0]) != 0) abort();
#endif
#endif
}

void NdbIndexStatImpl::setError(const Con &con, int line) {
  int code = 0;
  if (code == 0 && con.m_op != nullptr) {
    code = con.m_op->getNdbError().code;
  }
  if (code == 0 && con.m_scanop != nullptr) {
    code = con.m_scanop->getNdbError().code;
  }
  if (code == 0 && con.m_tx != nullptr) {
    code = con.m_tx->getNdbError().code;
  }
  if (code == 0 && con.m_dic != nullptr) {
    code = con.m_dic->getNdbError().code;
  }
  if (code == 0 && con.m_ndb != nullptr) {
    code = con.m_ndb->getNdbError().code;
  }
  setError(code, line);
}

void NdbIndexStatImpl::mapError(const int *map, int code) {
  while (*map != 0) {
    if (m_error.code == *map) {
      m_error.code = code;
      break;
    }
    map++;
  }
}
