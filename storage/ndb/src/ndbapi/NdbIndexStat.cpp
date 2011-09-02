/*
   Copyright (c) 2005, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <ndb_global.h>
#include <AttributeHeader.hpp>
#include <NdbSqlUtil.hpp>
#include <NdbIndexStat.hpp>
#include <NdbTransaction.hpp>
#include "NdbDictionaryImpl.hpp"
#include <NdbInterpretedCode.hpp>
#include <NdbRecord.hpp>
#include "NdbIndexStatImpl.hpp"

NdbIndexStat::NdbIndexStat() :
  m_impl(*new NdbIndexStatImpl(*this))
{
}

NdbIndexStat::NdbIndexStat(NdbIndexStatImpl& impl) :
  m_impl(impl)
{
}

NdbIndexStat::~NdbIndexStat()
{
  NdbIndexStatImpl* impl = &m_impl;
  if (this != impl)
    delete impl;
}

/** 
 * addKeyPartInfo
 * This method is used to build a standard representation of a 
 * lower or upper index bound in a buffer, which can then
 * be used to identify a range.
 * The buffer format is :
 *   1 word of NdbIndexScanOperation::BoundType
 *   1 word of ATTRINFO header containing the index attrid
 *     and the size in words of the data.
 *   0..N words of data.
 * The data itself is formatted as usual (e.g. 1/2 length
 * bytes for VAR* types)
 * For NULLs, length==0
 */
 
int
NdbIndexStat::addKeyPartInfo(const NdbRecord* record,
                             const char* keyRecordData,
                             Uint32 keyPartNum,
                             const NdbIndexScanOperation::BoundType boundType,
                             Uint32* keyStatData,
                             Uint32& keyLength)
{
  char buf[NdbRecord::Attr::SHRINK_VARCHAR_BUFFSIZE];

  Uint32 key_index= record->key_indexes[ keyPartNum ];
  const NdbRecord::Attr *column= &record->columns[ key_index ];
  
  bool is_null= column->is_null(keyRecordData);
  Uint32 len= 0;
  const void *aValue= keyRecordData + column->offset;

  if (!is_null)
  {
    bool len_ok;
    /* Support for special mysqld varchar format in keys. */
    if (column->flags & NdbRecord::IsMysqldShrinkVarchar)
    {
      len_ok= column->shrink_varchar(keyRecordData, 
                                     len, 
                                     buf);
      aValue= buf;
    }
    else
    {
      len_ok= column->get_var_length(keyRecordData, len);
    }
    if (!len_ok) {
      m_impl.setError(4209, __LINE__);
      return -1;
    }
  }

  /* Insert attribute header. */
  Uint32 tIndexAttrId= column->index_attrId;
  Uint32 sizeInWords= (len + 3) / 4;
  AttributeHeader ah(tIndexAttrId, sizeInWords << 2);
  const Uint32 ahValue= ah.m_value;

  if (keyLength + (2 + len) > NdbIndexStatImpl::BoundBufWords )
  {
    /* Something wrong, key data would be too big */
    /* Key size is limited to 4092 bytes */
    m_impl.setError(4207, __LINE__);
    return -1;
  }

  /* Fill in key data */
  keyStatData[ keyLength++ ]= boundType;
  keyStatData[ keyLength++ ]= ahValue;
  /* Zero last word prior to byte copy, in case we're not aligned */
  keyStatData[ keyLength + sizeInWords - 1] = 0;
  memcpy(&keyStatData[ keyLength ], aValue, len);

  keyLength+= sizeInWords;

  return 0;
}

int
NdbIndexStat::records_in_range(const NdbDictionary::Index* index, 
                               NdbTransaction* trans,
                               const NdbRecord* key_record,
                               const NdbRecord* result_record,
                               const NdbIndexScanOperation::IndexBound* ib, 
                               Uint64 table_rows, 
                               Uint64* count, 
                               int flags)
{
  DBUG_ENTER("NdbIndexStat::records_in_range");
  Uint64 rows;
  Uint32 key1[NdbIndexStatImpl::BoundBufWords], keylen1;
  Uint32 key2[NdbIndexStatImpl::BoundBufWords], keylen2;

  if (true)
  {
    // get start and end key from NdbIndexBound, using NdbRecord to 
    // get values into a standard format.
    Uint32 maxBoundParts= (ib->low_key_count > ib->high_key_count) ? 
      ib->low_key_count : ib->high_key_count;

    keylen1= keylen2= 0;

    /* Fill in keyX buffers */
    for (Uint32 keyPartNum=0; keyPartNum < maxBoundParts; keyPartNum++)
    {
      if (ib->low_key_count > keyPartNum)
      {
        /* Set bound to LT only if it's not inclusive
         * and this is the last key
         */
        NdbIndexScanOperation::BoundType boundType= 
          NdbIndexScanOperation::BoundLE;
        if ((! ib->low_inclusive) && 
            (keyPartNum == (ib->low_key_count -1 )))
          boundType= NdbIndexScanOperation::BoundLT;

        if (addKeyPartInfo(key_record,
                           ib->low_key,
                           keyPartNum,
                           boundType,
                           key1, 
                           keylen1) != 0)
          DBUG_RETURN(-1);
      }
      if (ib->high_key_count > keyPartNum)
      {
        /* Set bound to GT only if it's not inclusive
         * and this is the last key
         */
        NdbIndexScanOperation::BoundType boundType= 
          NdbIndexScanOperation::BoundGE;
        if ((! ib->high_inclusive) && 
            (keyPartNum == (ib->high_key_count -1)))
          boundType= NdbIndexScanOperation::BoundGT;        

        if (addKeyPartInfo(key_record,
                           ib->high_key,
                           keyPartNum,
                           boundType,
                           key2,
                           keylen2) != 0)
          DBUG_RETURN(-1);
      }
    }
  }

  if (true)
  {
    Uint32 out[4] = { 0, 0, 0, 0 };  // rows, in, before, after
    float tot[4] = { 0, 0, 0, 0 };   // totals of above
    int cnt, ret;
    bool forceSend = true;
    const Uint32 codeWords= 1;
    Uint32 codeSpace[ codeWords ];
    NdbInterpretedCode code(NULL, // No table
                            &codeSpace[0],
                            codeWords);
    if ((code.interpret_exit_last_row() != 0) ||
        (code.finalise() != 0))
    {
      m_impl.setError(code.getNdbError().code, __LINE__);
      DBUG_PRINT("error", ("code: %d", code.getNdbError().code));
      DBUG_RETURN(-1);
    }

    NdbIndexScanOperation* op= NULL;
    NdbScanOperation::ScanOptions options;
    NdbOperation::GetValueSpec extraGet;

    options.optionsPresent= 
      NdbScanOperation::ScanOptions::SO_GETVALUE | 
      NdbScanOperation::ScanOptions::SO_INTERPRETED;

    /* Read RECORDS_IN_RANGE pseudo column */
    extraGet.column= NdbDictionary::Column::RECORDS_IN_RANGE;
    extraGet.appStorage= (void*) out;
    extraGet.recAttr= NULL;

    options.extraGetValues= &extraGet;
    options.numExtraGetValues= 1;

    /* Add interpreted code to return on 1st row */
    options.interpretedCode= &code;

    const Uint32 keyBitmaskWords= (NDB_MAX_NO_OF_ATTRIBUTES_IN_KEY + 31) >> 5;
    Uint32 emptyMask[keyBitmaskWords];
    memset(&emptyMask[0], 0, keyBitmaskWords << 2);

    if (NULL == 
        (op= trans->scanIndex(key_record,
                              result_record,
                              NdbOperation::LM_CommittedRead,
                              (const unsigned char*) &emptyMask[0],
                              ib,
                              &options,
                              sizeof(NdbScanOperation::ScanOptions))))
    {
      m_impl.setError(trans->getNdbError().code, __LINE__);
      DBUG_PRINT("error", ("scanIndex : %d", trans->getNdbError().code));
      DBUG_RETURN(-1);
    }

    if (trans->execute(NdbTransaction::NoCommit,
                       NdbOperation::AbortOnError, forceSend) == -1) {
      m_impl.setError(trans->getNdbError().code, __LINE__);
      DBUG_PRINT("error", ("trans:%d op:%d", trans->getNdbError().code,
                           op->getNdbError().code));
      DBUG_RETURN(-1);
    }
    cnt = 0;
    const char* dummy_out_ptr= NULL;
    while ((ret = op->nextResult(&dummy_out_ptr,
                                 true, forceSend)) == 0) {
      DBUG_PRINT("info", ("frag rows=%u in=%u before=%u after=%u [error=%d]",
                          out[0], out[1], out[2], out[3],
                          (int)(out[1] + out[2] + out[3]) - (int)out[0]));
      unsigned i;
      for (i = 0; i < 4; i++)
        tot[i] += (float)out[i];
      cnt++;
    }
    if (ret == -1) {
      m_impl.setError(op->getNdbError().code, __LINE__);
      DBUG_PRINT("error nextResult ", ("trans:%d op:%d", trans->getNdbError().code,
                           op->getNdbError().code));
      DBUG_RETURN(-1);
    }
    op->close(forceSend);
    rows = (Uint64)tot[1];
  }

  *count = rows;
  DBUG_PRINT("value", ("rows=%u/%u flags=%x",
                       (unsigned)(rows>>32), (unsigned)(rows), flags));
  DBUG_RETURN(0);
}

// stored stats

int
NdbIndexStat::create_systables(Ndb* ndb)
{
  DBUG_ENTER("NdbIndexStat::create_systables");
  if (m_impl.create_systables(ndb) == -1)
    DBUG_RETURN(-1);
  DBUG_RETURN(0);
}

int
NdbIndexStat::drop_systables(Ndb* ndb)
{
  DBUG_ENTER("NdbIndexStat::drop_systables");
  if (m_impl.drop_systables(ndb) == -1)
    DBUG_RETURN(-1);
  DBUG_RETURN(0);
}

int
NdbIndexStat::check_systables(Ndb* ndb)
{
  DBUG_ENTER("NdbIndexStat::check_systables");
  if (m_impl.check_systables(ndb) == -1)
    DBUG_RETURN(-1);
  DBUG_RETURN(0);
}

int
NdbIndexStat::set_index(const NdbDictionary::Index& index,
                        const NdbDictionary::Table& table)
{
  DBUG_ENTER("NdbIndexStat::set_index");
  if (m_impl.set_index(index, table) == -1)
    DBUG_RETURN(-1);
  m_impl.m_facadeHead.m_indexId = index.getObjectId();
  m_impl.m_facadeHead.m_indexVersion = index.getObjectVersion();
  m_impl.m_facadeHead.m_tableId = table.getObjectId();
  DBUG_RETURN(0);
}

void
NdbIndexStat::reset_index()
{
  DBUG_ENTER("NdbIndexStat::reset_index");
  m_impl.reset_index();
  DBUG_VOID_RETURN;
}

int
NdbIndexStat::update_stat(Ndb* ndb)
{
  DBUG_ENTER("NdbIndexStat::update_stat");
  if (m_impl.update_stat(ndb, m_impl.m_facadeHead) == -1)
    DBUG_RETURN(-1);
  DBUG_RETURN(0);
}

int
NdbIndexStat::delete_stat(Ndb* ndb)
{
  DBUG_ENTER("NdbIndexStat::delete_stat");
  if (m_impl.delete_stat(ndb, m_impl.m_facadeHead) == -1)
    DBUG_RETURN(-1);
  DBUG_RETURN(0);
}

// cache

void
NdbIndexStat::move_cache()
{
  DBUG_ENTER("NdbIndexStat::move_cache");
  m_impl.move_cache();
  DBUG_VOID_RETURN;
}

void
NdbIndexStat::clean_cache()
{
  DBUG_ENTER("NdbIndexStat::clean_cache");
  m_impl.clean_cache();
  DBUG_VOID_RETURN;
}

void
NdbIndexStat::get_cache_info(CacheInfo& info, CacheType type) const
{
  const NdbIndexStatImpl::Cache* c = 0;
  switch (type) {
  case CacheBuild:
    c = m_impl.m_cacheBuild;
    break;
  case CacheQuery:
    c = m_impl.m_cacheQuery;
    break;
  case CacheClean:
    c = m_impl.m_cacheClean;
    break;
  }
  info.m_count = 0;
  info.m_valid = 0;
  info.m_sampleCount = 0;
  info.m_totalBytes = 0;
  info.m_save_time = 0;
  info.m_sort_time = 0;
  while (c != 0)
  {
    info.m_count += 1;
    info.m_valid += c->m_valid;
    info.m_sampleCount += c->m_sampleCount;
    info.m_totalBytes += c->m_keyBytes + c->m_valueBytes + c->m_addrBytes;
    info.m_save_time += c->m_save_time;
    info.m_sort_time += c->m_sort_time;
    c = c->m_nextClean;
  }
  // build and query cache have at most one instance
  require(type == CacheClean || info.m_count <= 1);
}

// read

void
NdbIndexStat::get_head(Head& head) const
{
  head = m_impl.m_facadeHead;
}

int
NdbIndexStat::read_head(Ndb* ndb)
{
  DBUG_ENTER("NdbIndexStat::read_head");
  if (m_impl.read_head(ndb, m_impl.m_facadeHead) == -1)
    DBUG_RETURN(-1);
  DBUG_RETURN(0);
}

int
NdbIndexStat::read_stat(Ndb* ndb)
{
  DBUG_ENTER("NdbIndexStat::read_stat");
  if (m_impl.read_stat(ndb, m_impl.m_facadeHead) == -1)
    DBUG_RETURN(-1);
  DBUG_RETURN(0);
}

// bound

NdbIndexStat::Bound::Bound(const NdbIndexStat* is, void* buffer)
{
  DBUG_ENTER("NdbIndexStat::Bound::Bound");
  require(is != 0 && is->m_impl.m_indexSet);
  require(buffer != 0);
  Uint8* buf = (Uint8*)buffer;
  // bound impl
  Uint8* buf1 = buf;
  UintPtr ubuf1 = (UintPtr)buf1;
  if (ubuf1 % 8 != 0)
    buf1 += (8 - ubuf1 % 8);
  new (buf1) NdbIndexStatImpl::Bound(is->m_impl.m_keySpec);
  m_impl = (void*)buf1;
  NdbIndexStatImpl::Bound& bound = *(NdbIndexStatImpl::Bound*)m_impl;
  // bound data
  Uint8* buf2 = buf1 + sizeof(NdbIndexStatImpl::Bound);
  uint used = (uint)(buf2 - buf);
  uint bytes = BoundBufferBytes - used;
  bound.m_data.set_buf(buf2, bytes);
  DBUG_VOID_RETURN;
}

int
NdbIndexStat::add_bound(Bound& bound_f, const void* value)
{
  DBUG_ENTER("NdbIndexStat::add_bound");
  NdbIndexStatImpl::Bound& bound =
    *(NdbIndexStatImpl::Bound*)bound_f.m_impl;
  Uint32 len_out;
  if (value == 0)
  {
    m_impl.setError(UsageError, __LINE__);
    DBUG_RETURN(-1);
  }
  if (bound.m_data.add(value, &len_out) == -1)
  {
    m_impl.setError(UsageError, __LINE__);
    DBUG_RETURN(-1);
  }
  DBUG_RETURN(0);
}

int
NdbIndexStat::add_bound_null(Bound& bound_f)
{
  DBUG_ENTER("NdbIndexStat::add_bound_null");
  NdbIndexStatImpl::Bound& bound =
    *(NdbIndexStatImpl::Bound*)bound_f.m_impl;
  Uint32 len_out;
  if (bound.m_data.add_null(&len_out) == -1)
  {
    m_impl.setError(UsageError, __LINE__);
    DBUG_RETURN(-1);
  }
  DBUG_RETURN(0);
}

void
NdbIndexStat::set_bound_strict(Bound& bound_f, int strict)
{
  DBUG_ENTER("NdbIndexStat::set_bound_strict");
  NdbIndexStatImpl::Bound& bound =
    *(NdbIndexStatImpl::Bound*)bound_f.m_impl;
  bound.m_strict = strict;
  DBUG_VOID_RETURN;
}

void
NdbIndexStat::reset_bound(Bound& bound_f)
{
  DBUG_ENTER("NdbIndexStat::reset_bound");
  NdbIndexStatImpl::Bound& bound =
    *(NdbIndexStatImpl::Bound*)bound_f.m_impl;
  bound.m_bound.reset();
  bound.m_type = -1;
  bound.m_strict = -1;
  DBUG_VOID_RETURN;
}

// range

NdbIndexStat::Range::Range(Bound& bound1, Bound& bound2) :
  m_bound1(bound1),
  m_bound2(bound2)
{
  DBUG_ENTER("NdbIndexStat::Range::Range");
  DBUG_VOID_RETURN;
}

int
NdbIndexStat::finalize_range(Range& range_f)
{
  DBUG_ENTER("NdbIndexStat::finalize_range");
  Bound& bound1_f = range_f.m_bound1;
  Bound& bound2_f = range_f.m_bound2;
  NdbIndexStatImpl::Bound& bound1 =
    *(NdbIndexStatImpl::Bound*)bound1_f.m_impl;
  NdbIndexStatImpl::Bound& bound2 =
    *(NdbIndexStatImpl::Bound*)bound2_f.m_impl;
  NdbIndexStatImpl::Range range(bound1, bound2);
  if (m_impl.finalize_range(range) == -1)
    DBUG_RETURN(-1);
  DBUG_RETURN(0);
}

void
NdbIndexStat::reset_range(Range& range)
{
  DBUG_ENTER("NdbIndexStat::reset_range");
  reset_bound(range.m_bound1);
  reset_bound(range.m_bound2);
  DBUG_VOID_RETURN;
}

int
NdbIndexStat::convert_range(Range& range_f,
                            const NdbRecord* key_record,
                            const NdbIndexScanOperation::IndexBound* ib)
{
  DBUG_ENTER("NdbIndexStat::convert_range");
  Bound& bound1_f = range_f.m_bound1;
  Bound& bound2_f = range_f.m_bound2;
  NdbIndexStatImpl::Bound& bound1 =
    *(NdbIndexStatImpl::Bound*)bound1_f.m_impl;
  NdbIndexStatImpl::Bound& bound2 =
    *(NdbIndexStatImpl::Bound*)bound2_f.m_impl;
  NdbIndexStatImpl::Range range(bound1, bound2);
  if (m_impl.convert_range(range, key_record, ib) == -1)
    DBUG_RETURN(-1);
  DBUG_RETURN(0);
}

// stat

NdbIndexStat::Stat::Stat(void* buffer)
{
  DBUG_ENTER("NdbIndexStat::Stat::Stat");
  require(buffer != 0);
  Uint8* buf = (Uint8*)buffer;
  // stat impl
  Uint8* buf1 = buf;
  UintPtr ubuf1 = (UintPtr)buf1;
  if (ubuf1 % 8 != 0)
    buf1 += (8 - ubuf1 % 8);
  new (buf1) NdbIndexStatImpl::Stat;
  m_impl = (void*)buf1;
  DBUG_VOID_RETURN;
}

int
NdbIndexStat::query_stat(const Range& range_f, Stat& stat_f)
{
  DBUG_ENTER("NdbIndexStat::query_stat");
  Bound& bound1_f = range_f.m_bound1;
  Bound& bound2_f = range_f.m_bound2;
  NdbIndexStatImpl::Bound& bound1 =
    *(NdbIndexStatImpl::Bound*)bound1_f.m_impl;
  NdbIndexStatImpl::Bound& bound2 =
    *(NdbIndexStatImpl::Bound*)bound2_f.m_impl;
  NdbIndexStatImpl::Range range(bound1, bound2);
#ifndef DBUG_OFF
  const uint sz = 8000;
  char buf[sz];
  DBUG_PRINT("index_stat", ("lo: %s", bound1.m_bound.print(buf, sz)));
  DBUG_PRINT("index_stat", ("hi: %s", bound2.m_bound.print(buf, sz)));
#endif
  NdbIndexStatImpl::Stat& stat =
    *(NdbIndexStatImpl::Stat*)stat_f.m_impl;
  if (m_impl.query_stat(range, stat) == -1)
    DBUG_RETURN(-1);
  DBUG_RETURN(0);
}

void
NdbIndexStat::get_empty(const Stat& stat_f, bool* empty)
{
  DBUG_ENTER("NdbIndexStat::get_empty");
  const NdbIndexStatImpl::Stat& stat =
    *(const NdbIndexStatImpl::Stat*)stat_f.m_impl;
  require(empty != 0);
  *empty = stat.m_value.m_empty;
  DBUG_PRINT("index_stat", ("empty:%d", *empty));
  DBUG_VOID_RETURN;
}

void
NdbIndexStat::get_rir(const Stat& stat_f, double* rir)
{
  DBUG_ENTER("NdbIndexStat::get_rir");
  const NdbIndexStatImpl::Stat& stat =
    *(const NdbIndexStatImpl::Stat*)stat_f.m_impl;
  double x = stat.m_value.m_rir;
  if (x < 1.0)
    x = 1.0;
  require(rir != 0);
  *rir = x;
#ifndef DBUG_OFF
  char buf[100];
  sprintf(buf, "%.2f", *rir);
#endif
  DBUG_PRINT("index_stat", ("rir:%s", buf));
  DBUG_VOID_RETURN;
}

void
NdbIndexStat::get_rpk(const Stat& stat_f, Uint32 k, double* rpk)
{
  DBUG_ENTER("NdbIndexStat::get_rpk");
  const NdbIndexStatImpl::Stat& stat =
    *(const NdbIndexStatImpl::Stat*)stat_f.m_impl;
  double x = stat.m_value.m_rir / stat.m_value.m_unq[k];
  if (x < 1.0)
    x = 1.0;
  require(rpk != 0);
  *rpk = x;
#ifndef DBUG_OFF
  char buf[100];
  sprintf(buf, "%.2f", *rpk);
#endif
  DBUG_PRINT("index_stat", ("rpk[%u]:%s", k, buf));
  DBUG_VOID_RETURN;
}

void
NdbIndexStat::get_rule(const Stat& stat_f, char* buffer)
{
  DBUG_ENTER("NdbIndexStat::get_rule");
  const NdbIndexStatImpl::Stat& stat =
    *(const NdbIndexStatImpl::Stat*)stat_f.m_impl;
  require(buffer != 0);
  BaseString::snprintf(buffer, RuleBufferBytes, "%s/%s/%s",
                       stat.m_rule[0], stat.m_rule[1], stat.m_rule[2]);
  DBUG_VOID_RETURN;
}

// events and polling

int
NdbIndexStat::create_sysevents(Ndb* ndb)
{
  DBUG_ENTER("NdbIndexStat::create_sysevents");
  if (m_impl.create_sysevents(ndb) == -1)
    DBUG_RETURN(-1);
  DBUG_RETURN(0);
}

int
NdbIndexStat::drop_sysevents(Ndb* ndb)
{
  DBUG_ENTER("NdbIndexStat::drop_sysevents");
  if (m_impl.drop_sysevents(ndb) == -1)
    DBUG_RETURN(-1);
  DBUG_RETURN(0);
}

int
NdbIndexStat::check_sysevents(Ndb* ndb)
{
  DBUG_ENTER("NdbIndexStat::check_sysevents");
  if (m_impl.check_sysevents(ndb) == -1)
    DBUG_RETURN(-1);
  DBUG_RETURN(0);
}

int
NdbIndexStat::create_listener(Ndb* ndb)
{
  DBUG_ENTER("NdbIndexStat::create_listener");
  if (m_impl.create_listener(ndb) == -1)
    DBUG_RETURN(-1);
  DBUG_RETURN(0);
}

int
NdbIndexStat::execute_listener(Ndb* ndb)
{
  DBUG_ENTER("NdbIndexStat::execute_listener");
  if (m_impl.execute_listener(ndb) == -1)
    DBUG_RETURN(-1);
  DBUG_RETURN(0);
}

int
NdbIndexStat::poll_listener(Ndb* ndb, int max_wait_ms)
{
  DBUG_ENTER("NdbIndexStat::poll_listener");
  int ret = m_impl.poll_listener(ndb, max_wait_ms);
  if (ret == -1)
    DBUG_RETURN(-1);
  DBUG_RETURN(ret);
}

int
NdbIndexStat::next_listener(Ndb* ndb)
{
  DBUG_ENTER("NdbIndexStat::next_listener");
  int ret = m_impl.next_listener(ndb);
  if (ret == -1)
    DBUG_RETURN(-1);
  DBUG_RETURN(ret);
}

int
NdbIndexStat::drop_listener(Ndb* ndb)
{
  DBUG_ENTER("NdbIndexStat::drop_listener");
  if (m_impl.drop_listener(ndb) == -1)
    DBUG_RETURN(-1);
  DBUG_RETURN(0);
}

// mem

NdbIndexStat::Mem::Mem()
{
}

NdbIndexStat::Mem::~Mem()
{
}

void
NdbIndexStat::set_mem_handler(Mem* mem)
{
  m_impl.m_mem_handler = mem;
}

// get impl

NdbIndexStatImpl&
NdbIndexStat::getImpl()
{
  return m_impl;
}

// error

NdbIndexStat::Error::Error()
{
  line = 0;
  extra = 0;
}

const NdbIndexStat::Error&
NdbIndexStat::getNdbError() const
{
  return m_impl.getNdbError();
}

class NdbOut&
operator<<(class NdbOut& out, const NdbIndexStat::Error& error)
{
  out << static_cast<const NdbError&>(error);
  out << " (line " << error.line << ", extra " << error.extra << ")";
  return out;
}
