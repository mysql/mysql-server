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

#ifndef NdbIndexScanOperation_H
#define NdbIndexScanOperation_H

#include <NdbScanOperation.hpp>

/**
 * @class NdbIndexScanOperation
 * @brief Class of scan operations for use to scan ordered index
 */
class NdbIndexScanOperation : public NdbScanOperation {
  friend class Ndb;
  friend class NdbConnection;
  friend class NdbResultSet;
  friend class NdbOperation;
  friend class NdbScanOperation;
public:
  /**
   * readTuples returns a NdbResultSet where tuples are stored.
   * Tuples are not stored in NdbResultSet until execute(NoCommit) 
   * has been executed and nextResult has been called.
   * 
   * @param parallel  Scan parallelism
   * @param batch     No of rows to fetch from each fragment at a time
   * @param LockMode  Scan lock handling   
   * @param order_by  Order result set in index order
   * @returns NdbResultSet.
   * @see NdbScanOperation::readTuples
   */ 
  NdbResultSet* readTuples(LockMode = LM_Read,
			   Uint32 batch = 0, 
			   Uint32 parallel = 0,
			   bool order_by = false);
  
  inline NdbResultSet* readTuples(int parallell){
    return readTuples(LM_Read, 0, parallell, false);
  }
  
  inline NdbResultSet* readTuplesExclusive(int parallell = 0){
    return readTuples(LM_Exclusive, 0, parallell, false);
  }

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

  /**
   * Define bound on index key in range scan.
   *
   * Each index key can have lower and/or upper bound.  Setting the key
   * equal to a value defines both upper and lower bounds.  The bounds
   * can be defined in any order.  Conflicting definitions is an error.
   *
   * For equality, it is better to use BoundEQ instead of the equivalent
   * pair of BoundLE and BoundGE.  This is especially true when table
   * distribution key is an initial part of the index key.
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
   * @param attrName    Attribute name, alternatively:
   * @param anAttrId    Index column id (starting from 0)
   * @param type        Type of bound
   * @param value       Pointer to bound value, 0 for NULL
   * @param len         Value length in bytes.
   *                    Fixed per datatype and can be omitted
   * @return            0 if successful otherwise -1
   */
  int setBound(const char* attr, int type, const void* aValue, Uint32 len = 0);

  /**
   * Define bound on index key in range scan using index column id.
   * See the other setBound() method for details.
   */
  int setBound(Uint32 anAttrId, int type, const void* aValue, Uint32 len = 0);

  /**
   * Reset bounds and put operation in list that will be
   *   sent on next execute
   */
  int reset_bounds();
  
  bool getSorted() const { return m_ordered; }
private:
  NdbIndexScanOperation(Ndb* aNdb);
  virtual ~NdbIndexScanOperation();

  int setBound(const NdbColumnImpl*, int type, const void* aValue, Uint32 len);
  int insertBOUNDS(Uint32 * data, Uint32 sz);

  virtual int equal_impl(const NdbColumnImpl*, const char*, Uint32);
  virtual NdbRecAttr* getValue_impl(const NdbColumnImpl*, char*);

  void fix_get_values();
  int next_result_ordered(bool fetchAllowed);
  int send_next_scan_ordered(Uint32 idx);
  int compare(Uint32 key, Uint32 cols, const NdbReceiver*, const NdbReceiver*);

  Uint32 m_sort_columns;
};

#endif
