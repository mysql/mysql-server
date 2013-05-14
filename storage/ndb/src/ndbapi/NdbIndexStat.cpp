/* Copyright (c) 2003, 2005-2007 MySQL AB


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA */


#include <ndb_global.h>
#include <AttributeHeader.hpp>
#include <NdbSqlUtil.hpp>
#include <NdbIndexStat.hpp>
#include <NdbTransaction.hpp>
#include "NdbDictionaryImpl.hpp"
#include <NdbInterpretedCode.hpp>
#include <NdbRecord.hpp>
#include <my_sys.h>

NdbIndexStat::NdbIndexStat(const NdbDictionary::Index* index) :
  m_index(index->m_impl),
  m_cache(NULL)
{
}

NdbIndexStat::~NdbIndexStat()
{
  delete [] m_cache;
  m_cache = NULL;
}

int
NdbIndexStat::alloc_cache(Uint32 entries)
{
  delete [] m_cache;
  m_cache = NULL;
  if (entries == 0) {
    return 0;
  }
  Uint32 i;
  Uint32 keysize = 0;
  for (i = 0; i < m_index.m_columns.size(); i++) {
    NdbColumnImpl* c = m_index.m_columns[i];
    keysize += 2;       // counting extra headers
    keysize += (c->m_attrSize * c->m_arraySize + 3 ) / 4;
  }
  Uint32 areasize = entries * (PointerSize + EntrySize + keysize);
  if (areasize > (1 << 16))
    areasize = (1 << 16);
  Uint32 cachesize = 2 * areasize;
  m_cache = new Uint32 [cachesize];
  if (m_cache == NULL) {
    set_error(4000);
    return -1;
  }
  m_areasize = areasize;
  m_seq = 0;
  Uint32 idir;
  for (idir = 0; idir <= 1; idir++) {
    Area& a = m_area[idir];
    a.m_data = &m_cache[idir * areasize];
    a.m_offset = a.m_data - &m_cache[0];
    a.m_free = areasize;
    a.m_entries = 0;
    a.m_idir = idir;
    a.pad1 = 0;
  }
#ifdef VM_TRACE
  memset(&m_cache[0], 0x3f, cachesize << 2);
#endif
  return 0;
}

#ifndef VM_TRACE
#define stat_verify()
#else
void
NdbIndexStat::stat_verify()
{
  Uint32 idir;
  for (idir = 0; idir <= 1; idir++) {
    Uint32 i;
    const Area& a = m_area[idir];
    assert(a.m_offset == idir * m_areasize);
    assert(a.m_data == &m_cache[a.m_offset]);
    Uint32 pointerwords = PointerSize * a.m_entries;
    Uint32 entrywords = 0;
    for (i = 0; i < a.m_entries; i++) {
      const Pointer& p = a.get_pointer(i);
      const Entry& e = a.get_entry(i);
      assert(a.get_pos(e) == p.m_pos);
      entrywords += EntrySize + e.m_keylen;
    }
    assert(a.m_free <= m_areasize);
    assert(pointerwords + a.m_free + entrywords == m_areasize);
    Uint32 off = pointerwords + a.m_free;
    for (i = 0; i < a.m_entries; i++) {
      assert(off < m_areasize);
      const Entry& e = *(const Entry*)&a.m_data[off];
      off += EntrySize + e.m_keylen;
    }
    assert(off == m_areasize);
    for (i = 0; i < a.m_entries; i++) {
      const Entry& e = a.get_entry(i);
      const Uint32* entrykey = (const Uint32*)&e + EntrySize;
      Uint32 n = 0;
      while (n + 2 <= e.m_keylen) {
        Uint32 t = entrykey[n++];
        assert(t == 2 * idir || t == 2 * idir + 1 || t == 4);
        AttributeHeader ah = *(const AttributeHeader*)&entrykey[n++];
        n += ah.getDataSize();
      }
      assert(n == e.m_keylen);
    }
    for (i = 0; i + 1 < a.m_entries; i++) {
      const Entry& e1 = a.get_entry(i);
      const Entry& e2 = a.get_entry(i + 1);
      const Uint32* entrykey1 = (const Uint32*)&e1 + EntrySize;
      const Uint32* entrykey2 = (const Uint32*)&e2 + EntrySize;
      int ret = stat_cmpkey(a, entrykey1, e1.m_keylen, entrykey2, e2.m_keylen);
      assert(ret == -1);
    }
  }
}
#endif

// compare keys
int
NdbIndexStat::stat_cmpkey(const Area& a, const Uint32* key1, Uint32 keylen1, const Uint32* key2, Uint32 keylen2)
{
  const Uint32 idir = a.m_idir;
  const int jdir = 1 - 2 * int(idir);
  Uint32 i1 = 0, i2 = 0;
  Uint32 t1 = 4, t2 = 4; //BoundEQ
  int ret = 0;
  Uint32 k = 0;
  while (k < m_index.m_columns.size()) {
    NdbColumnImpl* c = m_index.m_columns[k];
    Uint32 n = c->m_attrSize * c->m_arraySize;
    // absence of keypart is treated specially
    bool havekp1 = (i1 + 2 <= keylen1);
    bool havekp2 = (i2 + 2 <= keylen2);
    AttributeHeader ah1;
    AttributeHeader ah2;
    if (havekp1) {
      t1 = key1[i1++];
      assert(t1 == 2 * idir || t1 == 2 * idir + 1 || t1 == 4);
      ah1 = *(const AttributeHeader*)&key1[i1++];
    }
    if (havekp2) {
      t2 = key2[i2++];
      assert(t2 == 2 * idir || t2 == 2 * idir + 1 || t2 == 4);
      ah2 = *(const AttributeHeader*)&key2[i2++];
    }
    if (havekp1) {
      if (havekp2) {
        if (! ah1.isNULL()) {
          if (! ah2.isNULL()) {
            const NdbSqlUtil::Type& sqlType = NdbSqlUtil::getType(c->m_type);
            ret = (*sqlType.m_cmp)(c->m_cs, &key1[i1], n, &key2[i2], n, true);
            if (ret != 0)
              break;
          } else {
            ret = +1;
            break;
          }
        } else if (! ah2.isNULL()) {
          ret = -1;
          break;
        }
      } else {
        ret = +jdir;
        break;
      }
    } else {
      if (havekp2) {
        ret = -jdir;
        break;
      } else {
        // no more keyparts on either side
        break;
      }
    }
    i1 += ah1.getDataSize();
    i2 += ah2.getDataSize();
    k++;
  }
  if (ret == 0) {
    // strict bound is greater as start key and less as end key
    int s1 = t1 & 1;
    int s2 = t2 & 1;
    ret = (s1 - s2) * jdir;
  }
  return ret;
}

// find first key >= given key
int
NdbIndexStat::stat_search(const Area& a, const Uint32* key, Uint32 keylen, Uint32* idx, bool* match)
{
  // points at minus/plus infinity
  int lo = -1;
  int hi = a.m_entries;
  // loop invariant: key(lo) < key < key(hi)
  while (hi - lo > 1) {
    // observe lo < j < hi
    int j = (hi + lo) / 2;
    Entry& e = a.get_entry(j);
    const Uint32* key2 = (Uint32*)&e + EntrySize;
    Uint32 keylen2 = e.m_keylen;
    int ret = stat_cmpkey(a, key, keylen, key2, keylen2);
    // observe the loop invariant if ret != 0
    if (ret < 0)
      hi = j;
    else if (ret > 0)
      lo = j;
    else {
      *idx = j;
      *match = true;
      return 0;
    }
  }
  // hi - lo == 1 and key(lo) < key < key(hi)
  *idx = hi;
  *match = false;
  return 0;
}

// find oldest entry
int
NdbIndexStat::stat_oldest(const Area& a)
{
  Uint32 i, k= 0, m;
  bool found = false;
  m = ~(Uint32)0;     // shut up incorrect CC warning
  for (i = 0; i < a.m_entries; i++) {
    Pointer& p = a.get_pointer(i);
    Uint32 m2 = m_seq >= p.m_seq ? m_seq - p.m_seq : p.m_seq - m_seq;
    if (! found || m < m2) {
      m = m2;
      k = i;
      found = true;
    }
  }
  assert(found);
  return k;
}

// delete entry
int
NdbIndexStat::stat_delete(Area& a, Uint32 k)
{
  Uint32 i;
  NdbIndexStat::Entry& e = a.get_entry(k);
  Uint32 entrylen = EntrySize + e.m_keylen;
  Uint32 pos = a.get_pos(e);
  // adjust pointers to entries after
  for (i = 0; i < a.m_entries; i++) {
    Pointer& p = a.get_pointer(i);
    if (p.m_pos < pos) {
      p.m_pos += entrylen;
    }
  }
  // compact entry area
  unsigned firstpos = a.get_firstpos();
  for (i = pos; i > firstpos; i--) {
    a.m_data[i + entrylen - 1] = a.m_data[i - 1];
  }
  // compact pointer area
  for (i = k; i + 1 < a.m_entries; i++) {
    NdbIndexStat::Pointer& p = a.get_pointer(i);
    NdbIndexStat::Pointer& q = a.get_pointer(i + 1);
    p = q;
  }
  a.m_free += PointerSize + entrylen;
  a.m_entries--;
  stat_verify();
  return 0;
}

// update or insert stat values
int
NdbIndexStat::stat_update(const Uint32* key1, Uint32 keylen1, const Uint32* key2, Uint32 keylen2, const float pct[2])
{
  const Uint32* const key[2] = { key1, key2 };
  const Uint32 keylen[2] = { keylen1, keylen2 };
  Uint32 idir;
  for (idir = 0; idir <= 1; idir++) {
    Area& a = m_area[idir];
    Uint32 k;
    bool match;
    stat_search(a, key[idir], keylen[idir], &k, &match);
    Uint16 seq = m_seq++;
    if (match) {
      // update old entry
      NdbIndexStat::Pointer& p = a.get_pointer(k);
      NdbIndexStat::Entry& e = a.get_entry(k);
      e.m_pct = pct[idir];
      p.m_seq = seq;
    } else {
      Uint32 entrylen = NdbIndexStat::EntrySize + keylen[idir];
      Uint32 need = NdbIndexStat::PointerSize + entrylen;
      while (need > a.m_free) {
        Uint32 j = stat_oldest(a);
        if (j < k)
          k--;
        stat_delete(a, j);
      }
      // insert pointer
      Uint32 i;
      for (i = a.m_entries; i > k; i--) {
        NdbIndexStat::Pointer& p1 = a.get_pointer(i);
        NdbIndexStat::Pointer& p2 = a.get_pointer(i - 1);
        p1 = p2;
      }
      NdbIndexStat::Pointer& p = a.get_pointer(k);
      // insert entry
      Uint32 firstpos = a.get_firstpos();
      p.m_pos = firstpos - entrylen;
      NdbIndexStat::Entry& e = a.get_entry(k);
      e.m_pct = pct[idir];
      e.m_keylen = keylen[idir];
      Uint32* entrykey = (Uint32*)&e + EntrySize;
      for (i = 0; i < keylen[idir]; i++) {
        entrykey[i] = key[idir][i];
      }
      p.m_seq = seq;
      // total
      a.m_free -= PointerSize + entrylen;
      a.m_entries++;
    }
  }
  stat_verify();
  return 0;
}

int
NdbIndexStat::stat_select(const Uint32* key1, Uint32 keylen1, const Uint32* key2, Uint32 keylen2, float pct[2])
{
  const Uint32* const key[2] = { key1, key2 };
  const Uint32 keylen[2] = { keylen1, keylen2 };
  Uint32 idir;
  for (idir = 0; idir <= 1; idir++) {
    Area& a = m_area[idir];
    Uint32 k;
    bool match;
    stat_search(a, key[idir], keylen[idir], &k, &match);
    if (match) {
      NdbIndexStat::Entry& e = a.get_entry(k);
      pct[idir] = e.m_pct;
    } else if (k == 0) {
      NdbIndexStat::Entry& e = a.get_entry(k);
      if (idir == 0)
        pct[idir] = e.m_pct / 2;
      else
        pct[idir] = e.m_pct + (1 - e.m_pct) / 2;
    } else if (k == a.m_entries) {
      NdbIndexStat::Entry& e = a.get_entry(k - 1);
      if (idir == 0)
        pct[idir] = e.m_pct + (1 - e.m_pct) / 2;
      else
        pct[idir] = e.m_pct / 2;
    } else {
      NdbIndexStat::Entry& e1 = a.get_entry(k - 1);
      NdbIndexStat::Entry& e2 = a.get_entry(k);
      pct[idir] = (e1.m_pct + e2.m_pct) / 2;
    }
  }
  return 0;
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
      set_error(4209);
      return -1;
    }
  }

  /* Insert attribute header. */
  Uint32 tIndexAttrId= column->index_attrId;
  Uint32 sizeInWords= (len + 3) / 4;
  AttributeHeader ah(tIndexAttrId, sizeInWords << 2);
  const Uint32 ahValue= ah.m_value;

  if (keyLength + (2 + len) > BoundBufWords )
  {
    /* Something wrong, key data would be too big */
    /* Key size is limited to 4092 bytes */
    set_error(4207);
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
  Uint32 key1[BoundBufWords], keylen1;
  Uint32 key2[BoundBufWords], keylen2;

  if (m_cache == NULL)
    flags |= RR_UseDb | RR_NoUpdate;
  else if (m_area[0].m_entries == 0 || m_area[1].m_entries == 0)
    flags |= RR_UseDb;

  if ((flags & (RR_UseDb | RR_NoUpdate)) != (RR_UseDb | RR_NoUpdate)) {
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

  if (flags & RR_UseDb) {
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
      m_error= code.getNdbError();
      DBUG_PRINT("error", ("code: %d", m_error.code));
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
      m_error= trans->getNdbError();
      DBUG_PRINT("error", ("scanIndex : %d", m_error.code));
      DBUG_RETURN(-1);
    }

    if (trans->execute(NdbTransaction::NoCommit,
                       NdbOperation::AbortOnError, forceSend) == -1) {
      m_error = trans->getNdbError();
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
      m_error = op->getNdbError();
      DBUG_PRINT("error nextResult ", ("trans:%d op:%d", trans->getNdbError().code,
                           op->getNdbError().code));
      DBUG_RETURN(-1);
    }
    op->close(forceSend);
    rows = (Uint64)tot[1];
    if (cnt != 0 && ! (flags & RR_NoUpdate)) {
      float pct[2];
      pct[0] = 100 * tot[2] / tot[0];
      pct[1] = 100 * tot[3] / tot[0];
      DBUG_PRINT("info", ("update stat pct"
                          " before=%.2f after=%.2f",
                          pct[0], pct[1]));
      stat_update(key1, keylen1, key2, keylen2, pct);
    }
  } else {
      float pct[2];
      stat_select(key1, keylen1, key2, keylen2, pct);
      float diff = 100.0 - (pct[0] + pct[1]);
      float trows = (float)table_rows;
      DBUG_PRINT("info", ("select stat pct"
                          " before=%.2f after=%.2f in=%.2f table_rows=%.2f",
                          pct[0], pct[1], diff, trows));
      rows = 0;
      if (diff >= 0)
        rows = (Uint64)(diff * trows / 100);
      if (rows == 0)
        rows = 1;
  }

  *count = rows;
  DBUG_PRINT("value", ("rows=%llu flags=%o", rows, flags));
  DBUG_RETURN(0);
}

void
NdbIndexStat::set_error(int code)
{
  m_error.code = code;
}

const NdbError&
NdbIndexStat::getNdbError() const
{
  return m_error;
}
