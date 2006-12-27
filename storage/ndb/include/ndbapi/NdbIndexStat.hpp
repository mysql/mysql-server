/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef NdbIndexStat_H
#define NdbIndexStat_H

#include <ndb_global.h>
#include <NdbDictionary.hpp>
#include <NdbError.hpp>
class NdbIndexImpl;
class NdbIndexScanOperation;

/*
 * Statistics for an ordered index.
 */
class NdbIndexStat {
public:
  NdbIndexStat(const NdbDictionary::Index* index);
  ~NdbIndexStat();
  /*
   * Allocate memory for cache.  Argument is minimum number of stat
   * entries and applies to lower and upper bounds separately.  More
   * entries may fit (keys have variable size).  If not used, db is
   * contacted always.
   */
  int alloc_cache(Uint32 entries);
  /*
   * Flags for records_in_range.
   */
  enum {
    RR_UseDb = 1,       // contact db
    RR_NoUpdate = 2     // but do not update cache
  };
  /*
   * Estimate how many index records need to be scanned.  The scan
   * operation must be prepared with lock mode LM_CommittedRead and must
   * have the desired bounds set.  The routine may use local cache or
   * may contact db by executing the operation.
   *
   * If returned count is zero then db was contacted and the count is
   * exact.  Otherwise the count is approximate.  If cache is used then
   * caller must provide estimated number of table rows.  It will be
   * multiplied by a percentage obtained from the cache (result zero is
   * returned as 1).
   */
  int records_in_range(const NdbDictionary::Index* index,
                       NdbIndexScanOperation* op,
                       Uint64 table_rows,
                       Uint64* count,
                       int flags);
  /*
   * Get latest error.
   */
  const NdbError& getNdbError() const;

private:
  /*
   * There are 2 areas: start keys and end keys.  An area has pointers
   * at beginning and entries at end.  Pointers are sorted by key.
   *
   * A pointer contains entry offset and also entry timestamp.  An entry
   * contains the key and percentage of rows _not_ satisfying the bound
   * i.e. less than start key or greater than end key.
   *
   * A key is an array of index key bounds.  Each has type (0-4) in
   * first word followed by data with AttributeHeader.
   *
   * Stat update comes as pair of start and end key and associated
   * percentages.  Stat query takes best match of start and end key from
   * each area separately.  Rows in range percentage is then computed by
   * excluding the two i.e. as 100 - (start key pct + end key pct).
   *
   * TODO use more compact key format
   */
  struct Pointer;
  friend struct Pointer;
  struct Entry;
  friend struct Entry;
  struct Area;
  friend struct Area;
  struct Pointer {
    Uint16 m_pos;
    Uint16 m_seq;
  };
  struct Entry {
    float m_pct;
    Uint32 m_keylen;
  };
  STATIC_CONST( EntrySize = sizeof(Entry) >> 2 );
  STATIC_CONST( PointerSize = sizeof(Pointer) >> 2 );
  struct Area {
    Uint32* m_data;
    Uint32 m_offset;
    Uint32 m_free;
    Uint16 m_entries;
    Uint8 m_idir;
    Uint8 pad1;
    Area() {}
    Pointer& get_pointer(unsigned i) const {
      return *(Pointer*)&m_data[i];
    }
    Entry& get_entry(unsigned i) const {
      return *(Entry*)&m_data[get_pointer(i).m_pos];
    }
    Uint32 get_pos(const Entry& e) const {
      return (const Uint32*)&e - m_data;
    }
    unsigned get_firstpos() const {
      return PointerSize * m_entries + m_free;
    }
  };
  const NdbIndexImpl& m_index;
  Uint32 m_areasize;
  Uint16 m_seq;
  Area m_area[2];
  Uint32* m_cache;
  NdbError m_error;
#ifdef VM_TRACE
  void stat_verify();
#endif
  int stat_cmpkey(const Area& a, const Uint32* key1, Uint32 keylen1,
                  const Uint32* key2, Uint32 keylen2);
  int stat_search(const Area& a, const Uint32* key, Uint32 keylen,
                  Uint32* idx, bool* match);
  int stat_oldest(const Area& a);
  int stat_delete(Area& a, Uint32 k);
  int stat_update(const Uint32* key1, Uint32 keylen1,
                  const Uint32* key2, Uint32 keylen2, const float pct[2]);
  int stat_select(const Uint32* key1, Uint32 keylen1,
                  const Uint32* key2, Uint32 keylen2, float pct[2]);
  void set_error(int code);
};

#endif
