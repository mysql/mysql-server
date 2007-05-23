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

#ifndef NdbIndexScanOperation_H
#define NdbIndexScanOperation_H

#include <NdbScanOperation.hpp>

/**
 * @class NdbIndexScanOperation
 * @brief Class of scan operations for use to scan ordered index
 */
class NdbIndexScanOperation : public NdbScanOperation {
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  friend class Ndb;
  friend class NdbTransaction;
  friend class NdbResultSet;
  friend class NdbOperation;
  friend class NdbScanOperation;
  friend class NdbIndexStat;
#endif

public:
  /**
   * readTuples using ordered index
   * 
   * @param lock_mode Lock mode
   * @param scan_flags see @ref ScanFlag
   * @param parallel No of fragments to scan in parallel (0=max)
   */ 
  virtual int readTuples(LockMode lock_mode = LM_Read, 
                         Uint32 scan_flags = 0, 
			 Uint32 parallel = 0,
			 Uint32 batch = 0);

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  /**
   * readTuples using ordered index
   * 
   * @param lock_mode Lock mode
   * @param batch     No of rows to fetch from each fragment at a time
   * @param parallel  No of fragments to scan in parallel
   * @param order_by  Order result set in index order
   * @param order_desc Order descending, ignored unless order_by
   * @param read_range_no Enable reading of range no using @ref get_range_no
   * @returns 0 for success and -1 for failure
   * @see NdbScanOperation::readTuples
   */ 
  inline int readTuples(LockMode lock_mode,
                        Uint32 batch, 
                        Uint32 parallel,
                        bool order_by,
                        bool order_desc = false,
                        bool read_range_no = false,
			bool keyinfo = false,
			bool multi_range = false) {
    Uint32 scan_flags =
      (SF_OrderBy & -(Int32)order_by) |
      (SF_Descending & -(Int32)order_desc) |
      (SF_ReadRangeNo & -(Int32)read_range_no) | 
      (SF_KeyInfo & -(Int32)keyinfo) |
      (SF_MultiRange & -(Int32)multi_range);
    
    return readTuples(lock_mode, scan_flags, parallel, batch);
  }
#endif

  /**
   * Type of ordered index key bound.  The values (0-4) will not change
   * and can be used explicitly (e.g. they could be computed).
   */
  enum BoundType {
    BoundLE = 0,        ///< lower bound
    BoundLT = 1,        ///< lower bound, strict
    BoundGE = 2,        ///< upper bound
    BoundGT = 3,        ///< upper bound, strict
    BoundEQ = 4         ///< equality
  };

  enum {
    MaxRangeNo= 0xfff
  };

  /**
   * Define bound on index key in range scan.
   *
   * Each index key can have lower and/or upper bound.  Setting the key
   * equal to a value defines both upper and lower bounds.  The bounds
   * can be defined in any order.  Conflicting definitions is an error.
   *
   * For equality, it is better to use BoundEQ instead of the equivalent
   * pair of BoundLE and BoundGE.  This is especially true when table
   * partition key is an initial part of the index key.
   *
   * The sets of lower and upper bounds must be on initial sequences of
   * index keys.  All but possibly the last bound must be non-strict.
   * So "a >= 2 and b > 3" is ok but "a > 2 and b >= 3" is not.
   *
   * The scan may currently return tuples for which the bounds are not
   * satisfied.  For example, "a <= 2 and b <= 3" scans the index up to
   * (a=2, b=3) but also returns any (a=1, b=4).
   *
   * NULL is treated like a normal value which is less than any not-NULL
   * value and equal to another NULL value.  To compare against NULL use
   * setBound with null pointer (0).
   *
   * An index stores also all-NULL keys.  Doing index scan with empty
   * bound set returns all table tuples.
   *
   * @param attr        Attribute name, alternatively:
   * @param type        Type of bound
   * @param value       Pointer to bound value, 0 for NULL
   * @return            0 if successful otherwise -1
   *
   * @note See comment under equal() about data format and length.
   */
#ifndef DOXYGEN_SHOULD_SKIP_DEPRECATED
  int setBound(const char* attr, int type, const void* value, Uint32 len);
#endif
  int setBound(const char* attr, int type, const void* value);

  /**
   * Define bound on index key in range scan using index column id.
   * See the other setBound() method for details.
   */
#ifndef DOXYGEN_SHOULD_SKIP_DEPRECATED
  int setBound(Uint32 anAttrId, int type, const void* aValue, Uint32 len);
#endif
  int setBound(Uint32 anAttrId, int type, const void* aValue);

  /**
   * Reset bounds and put operation in list that will be
   *   sent on next execute
   */
  int reset_bounds(bool forceSend = false);

  /**
   * Marks end of a bound, 
   *  used when batching index reads (multiple ranges)
   *
   * With this call, it is possible to do multiple range scans (on the same
   * table/index) in a single readTuples() operation and roundtrip. Each range
   * bound (min and/or max) is defined with setBound(), and end_of_bound() is
   * called when all setBound() calls for one range bound have been done.
   *
   * The RANGE_NO argument is an application-selected value (in the range
   * 0-MaxRangeNo) that can be used to identify which range scan a particular row
   * belong to; the value can be obtained from get_range_no() when rows are
   * fetched with nextResult().
   */
  int end_of_bound(Uint32 range_no);
  
  /**
   * Return range no for current row, as defined in end_of_bound().
   * Only available if the SF_ReadRangeNo is passed to readTuples().
   */
  int get_range_no();
  
  /* Structure used to describe index scan bounds, for NdbRecord scans. */
  struct IndexBound {
    /* Row containing lower bound, or NULL for scan from the start. */
    const char *low_key;
    /* Number of columns in lower bound, for bounding by partial prefix. */
    Uint32 low_key_count;
    /* True for less-than-or-equal, false for strictly less-than. */
    bool low_inclusive;
    /* Row containing upper bound, or NULL for scan to the end. */
    const char * high_key;
    /* Number of columns in upper bound, for bounding by partial prefix. */
    Uint32 high_key_count;
    /* True for greater-than-or-equal, false for strictly greater-than. */
    bool high_inclusive;
    /*
      Value to identify this bound, may be read with get_range_no().
      Must be <= MaxRangeNo (set to zero if not using range_no).
      Note that for ordered scans, the range_no must be strictly increasing
      for each range, or the result set will not be sorted correctly.
    */
    Uint32 range_no;
  };

  /**
   * Is current scan sorted
   */
  bool getSorted() const { return m_ordered; }

  /**
   * Is current scan sorted descending
   */
  bool getDescending() const { return m_descending; }

private:
  NdbIndexScanOperation(Ndb* aNdb);
  virtual ~NdbIndexScanOperation();

  int setBound(const NdbColumnImpl*, int type, const void* aValue);
  int insertBOUNDS(Uint32 * data, Uint32 sz);
  int ndbrecord_insert_bound(const NdbRecord *key_record,
                             Uint32 column_index,
                             const char *row,
                             Uint32 bound_type);
  Uint32 getKeyFromSCANTABREQ(Uint32* data, Uint32 size);

  virtual int equal_impl(const NdbColumnImpl*, const char*);
  virtual NdbRecAttr* getValue_impl(const NdbColumnImpl*, char*);

  void fix_get_values();
  int next_result_ordered(bool fetchAllowed, bool forceSend = false);
  int next_result_ordered_ndbrecord(const char * & out_row,
                                    bool fetchAllowed,
                                    bool forceSend);
  void ordered_insert_receiver(Uint32 start, NdbReceiver *receiver);
  int ordered_send_scan_wait_for_all(bool forceSend);
  int send_next_scan_ordered(Uint32 idx);
  int compare(Uint32 key, Uint32 cols, const NdbReceiver*, const NdbReceiver*);
  int compare_ndbrecord(const NdbReceiver *r1,
                        const NdbReceiver *r2) const;
  Uint32 m_sort_columns;
  Uint32 m_this_bound_start;
  Uint32 * m_first_bound_word;

  friend struct Ndb_free_list_t<NdbIndexScanOperation>;
};

inline
int
NdbIndexScanOperation::setBound(const char* attr, int type, const void* value,
                                Uint32 len)
{
  return setBound(attr, type, value);
}

inline
int
NdbIndexScanOperation::setBound(Uint32 anAttrId, int type, const void* value,
                                Uint32 len)
{
  return setBound(anAttrId, type, value);
}

#endif
