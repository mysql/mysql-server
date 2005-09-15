/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <ndb_global.h>
#include <AttributeHeader.hpp>
#include <NdbSqlUtil.hpp>
#include <NdbIndexStat.hpp>
#include <NdbTransaction.hpp>
#include <NdbIndexScanOperation.hpp>
#include "NdbDictionaryImpl.hpp"
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
  Uint32 i, k, m;
  bool found = false;
  for (i = 0; i < a.m_entries; i++) {
    Pointer& p = a.get_pointer(i);
    Entry& e = a.get_entry(i);
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

int
NdbIndexStat::records_in_range(NdbDictionary::Index* index, NdbIndexScanOperation* op, Uint64 table_rows, Uint64* count, int flags)
{
  DBUG_ENTER("NdbIndexStat::records_in_range");
  Uint64 rows;
  Uint32 key1[1000], keylen1;
  Uint32 key2[1000], keylen2;

  if (m_cache == NULL)
    flags |= RR_UseDb | RR_NoUpdate;
  else if (m_area[0].m_entries == 0 || m_area[1].m_entries == 0)
    flags |= RR_UseDb;

  if ((flags & (RR_UseDb | RR_NoUpdate)) != RR_UseDb | RR_NoUpdate) {
    // get start and end key - assume bound is ordered, wellformed
    Uint32 bound[1000];
    Uint32 boundlen = op->getKeyFromSCANTABREQ(bound, 1000);

    keylen1 = keylen2 = 0;
    Uint32 n = 0;
    while (n < boundlen) {
      Uint32 t = bound[n];
      AttributeHeader ah(bound[n + 1]);
      Uint32 sz = 2 + ah.getDataSize();
      t &= 0xFFFF;      // may contain length
      assert(t <= 4);
      bound[n] = t;
      if (t == 0 || t == 1 || t == 4) {
        memcpy(&key1[keylen1], &bound[n], sz << 2);
        keylen1 += sz;
      }
      if (t == 2 || t == 3 || t == 4) {
        memcpy(&key2[keylen2], &bound[n], sz << 2);
        keylen2 += sz;
      }
      n += sz;
    }
  }

  if (flags & RR_UseDb) {
    Uint32 out[4] = { 0, 0, 0, 0 };  // rows, in, before, after
    float tot[4] = { 0, 0, 0, 0 };   // totals of above
    int cnt, ret;
    bool forceSend = true;
    NdbTransaction* trans = op->m_transConnection;
    if (op->interpret_exit_last_row() == -1 ||
        op->getValue(NdbDictionary::Column::RECORDS_IN_RANGE, (char*)out) == 0) {
      DBUG_PRINT("error", ("op:%d", op->getNdbError().code));
      DBUG_RETURN(-1);
    }
    if (trans->execute(NdbTransaction::NoCommit,
                       NdbTransaction::AbortOnError, forceSend) == -1) {
      DBUG_PRINT("error", ("trans:%d op:%d", trans->getNdbError().code,
                           op->getNdbError().code));
      DBUG_RETURN(-1);
    }
    cnt = 0;
    while ((ret = op->nextResult(true, forceSend)) == 0) {
      DBUG_PRINT("info", ("frag rows=%u in=%u before=%u after=%u [error=%d]",
                          out[0], out[1], out[2], out[3],
                          (int)(out[1] + out[2] + out[3]) - (int)out[0]));
      unsigned i;
      for (i = 0; i < 4; i++)
        tot[i] += (float)out[i];
      cnt++;
    }
    if (ret == -1) {
      DBUG_PRINT("error", ("trans:%d op:%d", trans->getNdbError().code,
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
