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

#ifndef NDB_SCAN_FILTER_HPP
#define NDB_SCAN_FILTER_HPP

#include <ndb_types.h>

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
   * @param op  The NdbOperation that the filter belongs to (is applied to).
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
  
  /** 
   * @name Grouping
   * @{
   */

  /**
   *  Begin of compound.
   *  ®return  0 if successful, -1 otherwize
   */
  int begin(Group group = AND);    

  /**
   *  End of compound.
   *  ®return  0 if successful, -1 otherwize
   */
  int end();

  /** @} *********************************************************************/

  /**
   *  <i>Explanation missing</i>
   */
  int istrue();

  /**
   *  <i>Explanation missing</i>
   */
  int isfalse();
  
  /** 
   * @name Integer Comparators
   * @{
   */
  /** Compare column value with integer for equal   
   *  ®return  0 if successful, -1 otherwize
   */
  int eq(int ColId, Uint32 value);   
  /** Compare column value with integer for not equal.
   *  ®return  0 if successful, -1 otherwize 
   */
  int ne(int ColId, Uint32 value);   
  /** Compare column value with integer for less than.
   *  ®return  0 if successful, -1 otherwize 
   */
  int lt(int ColId, Uint32 value);   
  /** Compare column value with integer for less than or equal. 
   *  ®return  0 if successful, -1 otherwize
   */
  int le(int ColId, Uint32 value);   
  /** Compare column value with integer for greater than. 
   *  ®return  0 if successful, -1 otherwize
   */
  int gt(int ColId, Uint32 value);   
  /** Compare column value with integer for greater than or equal.
   *  ®return  0 if successful, -1 otherwize
   */
  int ge(int ColId, Uint32 value);   

  /** Compare column value with integer for equal. 64-bit.  
   *  ®return  0 if successful, -1 otherwize
   */
  int eq(int ColId, Uint64 value);   
  /** Compare column value with integer for not equal. 64-bit.
   *  ®return  0 if successful, -1 otherwize
   */
  int ne(int ColId, Uint64 value);   
  /** Compare column value with integer for less than. 64-bit.
   *  ®return  0 if successful, -1 otherwize
   */
  int lt(int ColId, Uint64 value);   
  /** Compare column value with integer for less than or equal. 64-bit.
   *  ®return  0 if successful, -1 otherwize
   */
  int le(int ColId, Uint64 value);   
  /** Compare column value with integer for greater than. 64-bit.
   *  ®return  0 if successful, -1 otherwize
   */
  int gt(int ColId, Uint64 value);   
  /** Compare column value with integer for greater than or equal. 64-bit.
   *  ®return  0 if successful, -1 otherwize
   */
  int ge(int ColId, Uint64 value);   
  /** @} *********************************************************************/

  /** Check if column value is NULL */
  int isnull(int ColId);             
  /** Check if column value is non-NULL */
  int isnotnull(int ColId);          

  /** 
   * @name String Comparators
   * @{
   */
  /**
   * Compare string against a Char or Varchar column.
   *
   * By default Char comparison blank-pads both sides to common length.
   * Varchar comparison does not blank-pad.
   *
   * The extra <i>nopad</i> argument can be used to 
   * force non-padded comparison for a Char column.
   *  ®return  0 if successful, -1 otherwize
   */
  int eq(int ColId, const char * val, Uint32 len, bool nopad=false); 
  int ne(int ColId, const char * val, Uint32 len, bool nopad=false); 
  int lt(int ColId, const char * val, Uint32 len, bool nopad=false); 
  int le(int ColId, const char * val, Uint32 len, bool nopad=false); 
  int gt(int ColId, const char * val, Uint32 len, bool nopad=false); 
  int ge(int ColId, const char * val, Uint32 len, bool nopad=false); 

  /**
   *  Like comparison operator.
   *  ®return  0 if successful, -1 otherwize
   */
  int like(int ColId, const char * val, Uint32 len, bool nopad=false);
  /**
   *  Notlike comparison operator.
   *  ®return  0 if successful, -1 otherwize
   */
  int notlike(int ColId, const char * val, Uint32 len, bool nopad=false);
  /** @} *********************************************************************/

private:
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  friend class NdbScanFilterImpl;
#endif
  class NdbScanFilterImpl & m_impl;
  NdbScanFilter& operator=(const NdbScanFilter&); ///< Defined not implemented
};

#endif
