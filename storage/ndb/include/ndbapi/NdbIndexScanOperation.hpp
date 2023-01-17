/*
   Copyright (c) 2004, 2023, Oracle and/or its affiliates.

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

#ifndef NdbIndexScanOperation_H
#define NdbIndexScanOperation_H

#include "NdbScanOperation.hpp"

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
   * This method is used to specify details for an old Api Index Scan
   * operation.
   * 
   * @param lock_mode Lock mode
   * @param scan_flags see @ref ScanFlag
   * @param parallel No of fragments to scan in parallel (0=max)
   */ 
  int readTuples(LockMode lock_mode = LM_Read, 
                         Uint32 scan_flags = 0, 
			 Uint32 parallel = 0,
			 Uint32 batch = 0) override;

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

  /* Maximum number of ranges that can be supplied to a single 
   * NdbIndexScanOperation 
   */
  enum {
    MaxRangeNo= 0xfff
  };

  /**
   * Define bound on index key in range scan - old Api.
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
   * @note See the two parameter setBound variant for use with NdbRecord
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
   * This method is called to separate sets of bounds (ranges) when 
   * defining an Index Scan with multiple ranges
   * It can only be used with scans defined using the SF_MultiRange
   * scan flag.
   * For NdbRecord, ranges are specified using the IndexBound structure
   * and the setBound() API.
   * If an index scan has more than one range then end_of_bound must be 
   * called after every range, including the last.
   * If the SF_ReadRangeNo flag is set then the range_no supplied when
   * the range is defined will be associated with each row returned from
   * that range.  This can be obtained by calling get_range_no().
   * If SF_ReadRangeNo and SF_OrderBy flags are provided then range_no
   * values must be strictly increasing (i.e. starting at zero and 
   * getting larger by 1 for each range specified).  This is to ensure 
   * that rows are returned in order.
   */
  int end_of_bound(Uint32 range_no= 0);

  /**
   * Return range number for current row, as defined in the IndexBound
   * structure used when the scan was defined.
   * Only available if the SF_ReadRangeNo and SF_MultiRange flags were
   * set in the ScanOptions::scan_flags structure passed to scanIndex().
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
   * Add a range to an NdbRecord defined Index scan
   * 
   * This method is called to add a range to an IndexScan operation
   * which has been defined with a call to NdbTransaction::scanIndex().
   * To add more than one range, the index scan operation must have been
   * defined with the the SF_MultiRange flag set.
   *
   * Where multiple numbered ranges are defined with multiple calls to 
   * setBound, and the scan is ordered, the range number for each range 
   * must be larger than the range number for the previously defined range.
   *
   * When the application knows that rows in-range will only be found in
   * a particular partition, a PartitionSpecification can be supplied.
   * This may be used to limit the scan to a single partition, improving
   * system efficiency
   * The sizeOfPartInfo parameter should be set to the
   * sizeof(PartitionSpec) to enable backwards compatibility.
   *
   * @param key_record NdbRecord structure for the key the index is
   *        defined on
   * @param bound The bound to add
   * @param partInfo Extra information to enable a reduced set of
   *        partitions to be scanned.
   * @param sizeOfPartInfo  should be set to the
   *        sizeof(PartitionSpec) to enable backwards compatibility.
   *
   * @return 0 for Success, other for Failure.
   */
  int setBound(const NdbRecord *key_record,
               const IndexBound& bound,
               const Ndb::PartitionSpec* partInfo,
               Uint32 sizeOfPartInfo= 0);
  int setBound(const NdbRecord *key_record,
               const IndexBound& bound);

  /**
   * Return size of data, in 32-bit words, that will be send to data nodes for
   * all bounds added so far with setBound().
   *
   * This method is only available for NdbRecord index scans.
   */
  int getCurrentKeySize();

  /**
   * Is current scan sorted?
   */
  bool getSorted() const { return m_ordered; }

  /**
   * Is current scan sorted descending?
   */
  bool getDescending() const { return m_descending; }

private:
  NdbIndexScanOperation(Ndb* aNdb);
  ~NdbIndexScanOperation() override;
  
  int processIndexScanDefs(LockMode lm,
                           Uint32 scan_flags,
                           Uint32 parallel,
                           Uint32 batch);
  int scanIndexImpl(const NdbRecord *key_record,
                    const NdbRecord *result_record,
                    NdbOperation::LockMode lock_mode,
                    const unsigned char *result_mask,
                    const NdbIndexScanOperation::IndexBound *bound,
                    const NdbScanOperation::ScanOptions *options,
                    Uint32 sizeOfOptions);

  /* Structure used to collect information about an IndexBound
   * as it is provided by the old Api setBound() calls
   */
public:
  struct OldApiBoundInfo
  {
    Uint32 highestKey;
    bool highestSoFarIsStrict;
    Uint32 keysPresentBitmap;
    char* key;
  };

private:
  struct OldApiScanRangeDefinition
  {
    /* OldApiBoundInfo used during definition
     * IndexBound used once bound defined
     * Todo : For heavy old Api use, consider splitting
     * to allow NdbRecAttr use without malloc
     */
    union {
      struct {
        OldApiBoundInfo lowBound;
        OldApiBoundInfo highBound;
      } oldBound;

      IndexBound ib;
    };
    /* Space for key bounds 
     *   Low bound from offset 0
     *   High bound from offset key_record->m_row_size
     */
    char space[1];
  };

  int setBoundHelperOldApi(OldApiBoundInfo& boundInfo,
                           Uint32 maxKeyRecordBytes,
                           Uint32 index_attrId,
                           Uint32 valueLen,
                           bool inclusive,
                           Uint32 byteOffset,
                           Uint32 nullbit_byte_offset,
                           Uint32 nullbit_bit_in_byte,
                           const void *aValue);

  int setBound(const NdbColumnImpl*, int type, const void* aValue);
  int buildIndexBoundOldApi(int range_no);
  const IndexBound* getIndexBoundFromRecAttr(NdbRecAttr* recAttr);
  void releaseIndexBoundsOldApi();
  int ndbrecord_insert_bound(const NdbRecord *key_record,
                             Uint32 column_index,
                             const char *row,
                             Uint32 bound_type,
                             Uint32*& firstWordOfBound);
  int insert_open_bound(Uint32*& firstWordOfBound);

  int equal_impl(const NdbColumnImpl*, const char*) override;
  NdbRecAttr* getValue_impl(const NdbColumnImpl*, char*) override;

  int getDistKeyFromRange(const NdbRecord* key_record,
                          const NdbRecord* result_record,
                          const char* row,
                          Uint32* distKey);
  void fix_get_values();
  int next_result_ordered(bool fetchAllowed, bool forceSend = false);
  int next_result_ordered_ndbrecord(const char * & out_row,
                                    bool fetchAllowed,
                                    bool forceSend);
  void ordered_insert_receiver(Uint32 start, NdbReceiver *receiver);
  int ordered_send_scan_wait_for_all(bool forceSend);
  int send_next_scan_ordered(Uint32 idx);
  int compare(Uint32 key, Uint32 cols, const NdbReceiver*, const NdbReceiver*);
  Uint32 m_sort_columns;

  /* Number of IndexBounds for this scan (NdbRecord only) */
  Uint32 m_num_bounds;
  /* Most recently added IndexBound's range number */
  Uint32 m_previous_range_num;
  
  /* Old Scan API range information 
   * List of RecAttr structures containing OldApiScanRangeDefinition
   * structures
   * currentRangeOldApi is range currently being defined (if any)
   * Once defined (end_of_bound() / execute()) they are added to 
   * the list between first/lastRangeOldApi
   */
  NdbRecAttr* firstRangeOldApi;
  NdbRecAttr* lastRangeOldApi;
  NdbRecAttr* currentRangeOldApi;

  friend struct Ndb_free_list_t<NdbIndexScanOperation>;

private:
  NdbIndexScanOperation(const NdbIndexScanOperation&); // Not impl.
  NdbIndexScanOperation&operator=(const NdbIndexScanOperation&);
};

inline
int
NdbIndexScanOperation::setBound(const char* attr, int type, const void* value,
                                Uint32 len)
{
  (void)len;  // unused
  return setBound(attr, type, value);
}

inline
int
NdbIndexScanOperation::setBound(Uint32 anAttrId, int type, const void* value,
                                Uint32 len)
{
  (void)len;  // unused
  return setBound(anAttrId, type, value);
}

#endif
