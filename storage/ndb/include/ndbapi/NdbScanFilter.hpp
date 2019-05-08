/*
   Copyright (c) 2003, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NDB_SCAN_FILTER_HPP
#define NDB_SCAN_FILTER_HPP

#include <ndb_types.h>
#include "ndbapi_limits.h"

#include "NdbInterpretedCode.hpp"

/**
 * @class NdbScanFilter
 * @brief A simple way to specify filters for scan operations
 *
 * @note  This filter interface is under development and may change in 
 *        the future! 
 * 
 */
class NdbScanFilter {
public:
  /**
   * Constructor
   * Using this constructor, a ScanFilter is created which will
   * build and finalise a scan filter program using the 
   * NdbInterpretedCode object passed.
   * Once defined, the generated NdbInterpretedCode object can 
   * be used to specify a scan filter for one or more NdbRecord defined 
   * ScanOperations on the supplied table.
   * The NdbInterpretedCode object is passed to the ScanTable() 
   * or ScanIndex() call via the ScanOptions structure.
   *
   * @param code Pointer to the NdbInterpretedCode object to build 
   * the ScanFilter in.
   */
  NdbScanFilter(NdbInterpretedCode* code);

  /**
   * Constructor
   * This constructor is used to create an ScanFilter object
   * for use with a non-NdbRecord defined ScanOperation.
   * 
   * As part of the filter definition, it is automatically added 
   * to the supplied operation.
   * ScanFilters defined this way can only be used with the passed
   * Scan operation.
   *
   * @param op  The NdbOperation that the filter is applied to.
   *            Note that this MUST be an NdbScanOperation or
   *            NdbIndexScanOperation object created using the
   *            NdbTransaction->getNdbScanOperation() or
   *            NdbTransaciton->getNdbIndexScanOperation() 
   *            methods
   */
  NdbScanFilter(class NdbOperation * op);

  ~NdbScanFilter();
  
  /**
   *  Group operators
   */
  enum Group {
    AND  = 1,    ///< (x1 AND x2 AND x3)
    OR   = 2,    ///< (x1 OR x2 OR X3)
    NAND = 3,    ///< NOT (x1 AND x2 AND x3)
    NOR  = 4     ///< NOT (x1 OR x2 OR x3)
  };

  enum BinaryCondition 
  {
    COND_LE = 0,           ///< lower bound
    COND_LT = 1,           ///< lower bound, strict
    COND_GE = 2,           ///< upper bound
    COND_GT = 3,           ///< upper bound, strict
    COND_EQ = 4,           ///< equality
    COND_NE = 5,           ///< not equal
    COND_LIKE = 6,         ///< like
    COND_NOT_LIKE = 7,     ///< not like
    COND_AND_EQ_MASK = 8,  ///< (bit & mask) == mask
    COND_AND_NE_MASK = 9,  ///< (bit & mask) != mask (incl. NULL)
    COND_AND_EQ_ZERO = 10, ///< (bit & mask) == 0
    COND_AND_NE_ZERO = 11  ///< (bit & mask) != 0 (incl. NULL)
  };

  /** 
   * @name Grouping
   * @{
   */

  /**
   *  Begin of compound.
   *  If no group type is passed, defaults to AND.
   *  ®return  0 if successful, -1 otherwise
   */
  int begin(Group group = AND);    

  /**
   *  End of compound.
   *  ®return  0 if successful, -1 otherwise
   */
  int end();

  /**
   *  Reset the ScanFilter object, discarding any previous
   *  filter definition and error state.
   */
  void reset();

  /** @} *********************************************************************/

  /**
   *  Define one term of the current group as TRUE
   *  ®return  0 if successful, -1 otherwise
   */
  int istrue();

  /**
   *  Define one term of the current group as FALSE
   *  ®return  0 if successful, -1 otherwise
   */
  int isfalse();

  /**
   * Compare column <b>ColId</b> with <b>val</b>
   *
   * For all BinaryConditions except LIKE and NOT_LIKE, the value pointed 
   * to by val should be in normal column format as described in the 
   * documentation for NdbOperation::equal().
   * For BinaryConditions LIKE and NOT_LIKE, the value pointed to by val
   * should NOT include initial length bytes.
   * For LIKE and NOT_LIKE, the % and ? wildcards are supported.
   * For bitmask operations, see the bitmask format information against
   * the branch_col_and_mask_eq_mask instruction in NdbInterpretedCode.hpp
   *
   *  ®return  0 if successful, -1 otherwise
   */
  int cmp(BinaryCondition cond, int ColId, const void *val, Uint32 len = 0); 

  /** 
   * @name Integer Comparators
   * @{
   */
  /** Compare column value with integer for equal   
   *  ®return  0 if successful, -1 otherwise
   */
  int eq(int ColId, Uint32 value) { return cmp(COND_EQ, ColId, &value, 4);}

  /** Compare column value with integer for not equal.
   *  ®return  0 if successful, -1 otherwise 
   */
  int ne(int ColId, Uint32 value) { return cmp(COND_NE, ColId, &value, 4);}  
  /** Compare column value with integer for less than.
   *  ®return  0 if successful, -1 otherwise 
   */
  int lt(int ColId, Uint32 value) { return cmp(COND_LT, ColId, &value, 4);}
  /** Compare column value with integer for less than or equal. 
   *  ®return  0 if successful, -1 otherwise
   */
  int le(int ColId, Uint32 value) { return cmp(COND_LE, ColId, &value, 4);}
  /** Compare column value with integer for greater than. 
   *  ®return  0 if successful, -1 otherwise
   */
  int gt(int ColId, Uint32 value) { return cmp(COND_GT, ColId, &value, 4);} 
  /** Compare column value with integer for greater than or equal.
   *  ®return  0 if successful, -1 otherwise
   */
  int ge(int ColId, Uint32 value) { return cmp(COND_GE, ColId, &value, 4);}

  /** Compare column value with integer for equal. 64-bit.  
   *  ®return  0 if successful, -1 otherwise
   */
  int eq(int ColId, Uint64 value) { return cmp(COND_EQ, ColId, &value, 8);}
  /** Compare column value with integer for not equal. 64-bit.
   *  ®return  0 if successful, -1 otherwise
   */
  int ne(int ColId, Uint64 value) { return cmp(COND_NE, ColId, &value, 8);}
  /** Compare column value with integer for less than. 64-bit.
   *  ®return  0 if successful, -1 otherwise
   */
  int lt(int ColId, Uint64 value) { return cmp(COND_LT, ColId, &value, 8);}  
  /** Compare column value with integer for less than or equal. 64-bit.
   *  ®return  0 if successful, -1 otherwise
   */
  int le(int ColId, Uint64 value) { return cmp(COND_LE, ColId, &value, 8);}
  /** Compare column value with integer for greater than. 64-bit.
   *  ®return  0 if successful, -1 otherwise
   */
  int gt(int ColId, Uint64 value) { return cmp(COND_GT, ColId, &value, 8);}
  /** Compare column value with integer for greater than or equal. 64-bit.
   *  ®return  0 if successful, -1 otherwise
   */
  int ge(int ColId, Uint64 value) { return cmp(COND_GE, ColId, &value, 8);}
  /** @} *********************************************************************/

  /** Check if column value is NULL 
   *  ®return  0 if successful, -1 otherwise
   */
  int isnull(int ColId);             
  /** Check if column value is non-NULL 
   *  ®return  0 if successful, -1 otherwise
   */
  int isnotnull(int ColId);          
  
  enum Error {
    FilterTooLarge = 4294
  };

  /**
   * Get filter level error.
   *
   * Errors encountered when building a ScanFilter do not propagate
   * to any involved NdbOperation object.  This method gives access
   * to error information.
   */
  const struct NdbError & getNdbError() const;

  /**
   * Get filter's associated InterpretedCode object.  For
   * ScanFilters associated with a non-NdbRecord scan operation,
   * this method always returns NULL.
   */
  const NdbInterpretedCode* getInterpretedCode() const;

  /**
   * Get NdbScanFilter's associated NdbScanOperation
   * 
   * Where the NdbScanFilter was constructed with an NdbOperation
   * this method can be used to obtain a pointer to the NdbOperation
   * object.
   * For other NdbScanFilter objects it will return NULL
   */
  NdbOperation * getNdbOperation() const;
private:
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  friend class NdbScanFilterImpl;
#endif
  class NdbScanFilterImpl & m_impl;
  NdbScanFilter& operator=(const NdbScanFilter&); ///< Defined not implemented
};

#endif
