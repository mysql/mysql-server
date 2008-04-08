/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 MySQL, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

%{

  Uint32 cmpGetColumnLength(const NdbDictionary::Column * theColumn) {

    Uint32 cmpLength = 0;
    switch(theColumn->getType()) {
    case NDB_TYPE_VARCHAR:
    case NDB_TYPE_VARBINARY:
    {
      cmpLength=theColumn->getLength()+1;
      break;
    }
    case NDB_TYPE_LONGVARCHAR:
    case NDB_TYPE_LONGVARBINARY:
    {
      cmpLength=theColumn->getLength()+2;
      break;
    }
    case NDB_TYPE_CHAR:
    case NDB_TYPE_BINARY:
    {
      cmpLength=theColumn->getLength();
      break;
    }
    default:
      return 0;
    }
    return cmpLength;
  }

  %}

class NdbScanFilter {

  class NdbScanFilterImpl & m_impl;
  NdbScanFilter& operator=(const NdbScanFilter&); ///< Defined not implemented
public:

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
    COND_LE = 0,        ///< lower bound
    COND_LT = 1,        ///< lower bound, strict
    COND_GE = 2,        ///< upper bound
    COND_GT = 3,        ///< upper bound, strict
    COND_EQ = 4,        ///< equality
    COND_NE = 5,        ///< not equal
    COND_LIKE = 6,      ///< like
    COND_NOT_LIKE = 7   ///< not like
  };


  %ndbexception("NdbApiException") {
    $action
      if (result==NULL) {
        NDB_exception(NdbApiException,"ScanFilter error" );
      }
  }
  NdbOperation * getNdbOperation();

  %ndbexception("NdbApiException") {
    $action
      if (result==-1) {
        NDB_exception(NdbApiException,"ScanFilter error" );
      }
  }
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
#if defined(SWIG_RUBY_AUTORENAME)
  %rename("is_true?") istrue;
  %rename("is_false?") isfalse;
  %rename("is_null?") isnull;
  %rename("is_not_null?") isnotnull;
#else
  %rename istrue isTrue;
  %rename isfalse isFalse;
  %rename isnull isNull;
  %rename isnotnull isNotNull;
#endif

  /**
   *  <i>Explanation missing</i>
   */
  int istrue();

  /**
   *  <i>Explanation missing</i>
   */
  int isfalse();

  /**
   * Compare column <b>ColId</b> with <b>val</b>
   */
//  int cmp(BinaryCondition cond, int ColId, const void *val, Uint32 len = 0);

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

  %ndbnoexception
};

%extend NdbScanFilter {

  %ndbexception("NdbApiException") {
    $action
      if (result==-1) {
        NDB_exception(NdbApiException,"ScanFilter error" );
      }
  }


  int cmp(BinaryCondition cond, int ColId) {
    return self->cmp(cond,ColId,(void *)0);
  };
  int cmp(BinaryCondition cond, int ColId, const Int32 val) {
    Int32 value = (Int32)val;
    return self->cmp(cond,ColId,(void *) &value);
  };
  int cmp(BinaryCondition cond, int ColId, const Int64 val) {
    Int64 value = (Int64)val;
    return self->cmp(cond,ColId,(void *) &value);
  };
  int cmp(BinaryCondition cond, int ColId, const Uint64 val) {
    Uint64 value = (Uint64)val;
    return self->cmp(cond,ColId,(void *) &value);
  };
  int cmp(BinaryCondition cond, int ColId, const double val) {
    double value = (double)val;
    return self->cmp(cond,ColId,(void *) &value);
  };
  int cmp(BinaryCondition cond, int ColId, const float val) {
    float value = (float)val;
    return self->cmp(cond,ColId,(void *) &value);
  };
  int cmpTimestamp(BinaryCondition cond, int ColId,
                   NdbTimestamp anInputTimestamp) {
    Uint32 value = (Uint32)anInputTimestamp;
    return self->cmp(cond,ColId,(void *) &value);
  };
  int cmp(BinaryCondition cond, int ColId, NdbDateTime *anInputDateTime) {
    NdbOperation * op = self->getNdbOperation();
    if (op == NULL)
      return -1;
    const NdbDictionary::Column * theColumn = op->getTable()->getColumn(ColId);
    Uint64 dtval = ndbFormatDateTime(theColumn,anInputDateTime);
    if (dtval == 1)
      return -1;
    return self->cmp(cond,ColId,(void *) &dtval);
  };

  int cmp(BinaryCondition cond, int ColId, const char * BYTE, size_t len) {
    NdbOperation * op = self->getNdbOperation();
    if (op == NULL)
      return -1;
    const NdbDictionary::Column * theColumn = op->getTable()->getColumn(ColId);
    char * stringVal = ndbFormatString(theColumn,BYTE,len);
    if (stringVal == NULL)
      return -1;

    Uint32 cmpLength = 0;
    if (cond == NdbScanFilter::COND_EQ) {
      cmpLength = cmpGetColumnLength(theColumn);
      if (cmpLength == 0)
        return -1;
    } else if ((cond == NdbScanFilter::COND_LIKE)
               || (cond == NdbScanFilter::COND_NOT_LIKE)) {
      cmpLength = (Uint32)len;
    }

    int retval = self->cmp(cond,ColId,(void *)stringVal,cmpLength);
    free(stringVal);
    return retval;
  };
  int cmpString(BinaryCondition cond, int ColId,
                const char * anInputString, size_t len ) {
    NdbOperation * op = self->getNdbOperation();
    if (op == NULL)
      return -1;
    const NdbDictionary::Column * theColumn = op->getTable()->getColumn(ColId);
    char * stringVal = ndbFormatString(theColumn,anInputString,len);
    if (stringVal == NULL)
      return -1;

    Uint32 cmpLength = 0;
    if (cond == NdbScanFilter::COND_EQ) {
      cmpLength = cmpGetColumnLength(theColumn);
      if (cmpLength == 0)
        return -1;
    } else if ((cond == NdbScanFilter::COND_LIKE)
               || (cond == NdbScanFilter::COND_NOT_LIKE)) {
      cmpLength = (Uint32)len;
    }

    int retval = self->cmp(cond,ColId,(void *)stringVal,cmpLength);
    free(stringVal);
    return retval;
  };



  int cmp(BinaryCondition cond, const char * ColName) {
    NdbOperation * op = self->getNdbOperation();
    if (op == NULL)
      return -1;
    int ColId = getColumnId(op,ColName);
    if (ColId == -1) {
      return ColId;
    }
    return self->cmp(cond,ColId,(void *)0);
  };
  int cmp(BinaryCondition cond, const char * ColName, const Int32 val) {
    NdbOperation * op = self->getNdbOperation();
    if (op == NULL)
      return -1;
    int ColId = getColumnId(op,ColName);
    if (ColId == -1) {
      return ColId;
    }
    Int32 value = (Int32)val;
    return self->cmp(cond,ColId,(void *) &value);
  };
  int cmp(BinaryCondition cond, const char * ColName, const Int64 val) {
    NdbOperation * op = self->getNdbOperation();
    if (op == NULL)
      return -1;
    int ColId = getColumnId(op,ColName);
    if (ColId == -1) {
      return ColId;
    }
    Int64 value = (Int64)val;
    return self->cmp(cond,ColId,(void *) &value);
  };
  int cmp(BinaryCondition cond, const char * ColName, const Uint64 val) {
    NdbOperation * op = self->getNdbOperation();
    if (op == NULL)
      return -1;
    int ColId = getColumnId(op,ColName);
    if (ColId == -1) {
      return ColId;
    }
    Uint64 value = (Uint64)val;
    return self->cmp(cond,ColId,(void *) &value);
  };
  int cmp(BinaryCondition cond, const char * ColName, const double val) {
    NdbOperation * op = self->getNdbOperation();
    if (op == NULL)
      return -1;
    int ColId = getColumnId(op,ColName);
    if (ColId == -1) {
      return ColId;
    }
    double value = (double)val;
    return self->cmp(cond,ColId,(void *) &value);
  };
  int cmp(BinaryCondition cond, const char * ColName, const float val) {
    NdbOperation * op = self->getNdbOperation();
    if (op == NULL)
      return -1;
    int ColId = getColumnId(op,ColName);
    if (ColId == -1) {
      return ColId;
    }
    float value = (float)val;
    return self->cmp(cond,ColId,(void *) &value);
  };
  int cmpTimestamp(BinaryCondition cond, const char * ColName,
                   NdbTimestamp anInputTimestamp) {
    NdbOperation * op = self->getNdbOperation();
    if (op == NULL)
      return -1;
    int ColId = getColumnId(op,ColName);
    if (ColId == -1) {
      return ColId;
    }
    Uint32 value = (Uint32)anInputTimestamp;
    return self->cmp(cond,ColId,(void *) &value);
  };
  int cmp(BinaryCondition cond, const char * ColName,
          NdbDateTime *anInputDateTime) {
    NdbOperation * op = self->getNdbOperation();
    if (op == NULL)
      return -1;
    int ColId = getColumnId(op,ColName);
    if (ColId == -1) {
      return ColId;
    }
    const NdbDictionary::Column * theColumn = op->getTable()->getColumn(ColId);
    Uint64 dtval = ndbFormatDateTime(theColumn,anInputDateTime);
    if (dtval == 1)
      return -1;
    return self->cmp(cond,ColId,(void *) &dtval);
  };

  int cmp(BinaryCondition cond, const char * ColName,
          const char * BYTE, size_t len) {
    NdbOperation * op = self->getNdbOperation();
    if (op == NULL)
      return -1;
    int ColId = getColumnId(op,ColName);
    if (ColId == -1) {
      return ColId;
    }
    const NdbDictionary::Column * theColumn = op->getTable()->getColumn(ColId);
    char * stringVal = ndbFormatString(theColumn,BYTE,len);
    if (stringVal == NULL)
      return -1;

    Uint32 cmpLength = 0;
    if (cond == NdbScanFilter::COND_EQ) {
      cmpLength = cmpGetColumnLength(theColumn);
      if (cmpLength == 0)
        return -1;
    } else if ((cond == NdbScanFilter::COND_LIKE)
               || (cond == NdbScanFilter::COND_NOT_LIKE)) {
      cmpLength = (Uint32)len;
    }

    int retval = self->cmp(cond,ColId,(void *)stringVal,cmpLength);
    free(stringVal);
    return retval;
  };
  int cmpString(BinaryCondition cond, const char * ColName,
                const char * anInputString, size_t len ) {
    NdbOperation * op = self->getNdbOperation();
    if (op == NULL)
      return -1;
    int ColId = getColumnId(op,ColName);
    if (ColId == -1) {
      return ColId;
    }
    const NdbDictionary::Column * theColumn = op->getTable()->getColumn(ColId);
    char * stringVal = ndbFormatString(theColumn,anInputString,len);
    if (stringVal == NULL)
      return -1;
    Uint32 cmpLength = 0;
    if (cond == NdbScanFilter::COND_EQ) {
      cmpLength = cmpGetColumnLength(theColumn);
      if (cmpLength == 0)
        return -1;
    } else if ((cond == NdbScanFilter::COND_LIKE) || (cond == NdbScanFilter::COND_NOT_LIKE)) {
      cmpLength = (Uint32)len;
    }
    int retval = self->cmp(cond,ColId,(void *)stringVal,cmpLength);
    free(stringVal);
    return retval;
  };

  /** Compare column value with integer for equal
   *  ®return  0 if successful, -1 otherwize
   */
  int eq(const char * ColName, Uint32 value) {
    NdbOperation * op = self->getNdbOperation();
    if (op == NULL)
      return -1;
    int ColId = getColumnId(op,ColName);
    if (ColId == -1) {
      return ColId;
    }
    return self->eq(ColId,value);
  }

  /** Compare column value with integer for not equal.
   *  ®return  0 if successful, -1 otherwize
   */
  int ne(const char * ColName, Uint32 value) {
    NdbOperation * op = self->getNdbOperation();
    if (op == NULL)
      return -1;
    int ColId = getColumnId(op,ColName);
    if (ColId == -1) {
      return ColId;
    }
    return self->ne(ColId,value);
  }

  /** Compare column value with integer for less than.
   *  ®return  0 if successful, -1 otherwize
   */
  int lt(const char * ColName, Uint32 value) {
    NdbOperation * op = self->getNdbOperation();
    if (op == NULL)
      return -1;
    int ColId = getColumnId(op,ColName);
    if (ColId == -1) {
      return ColId;
    }
    return self->lt(ColId,value);
  }

  /** Compare column value with integer for less than or equal.
   *  ®return  0 if successful, -1 otherwize
   */
  int le(const char * ColName, Uint32 value) {
    NdbOperation * op = self->getNdbOperation();
    if (op == NULL)
      return -1;
    int ColId = getColumnId(op,ColName);
    if (ColId == -1) {
      return ColId;
    }
    return self->le(ColId,value);
  }

  /** Compare column value with integer for greater than.
   *  ®return  0 if successful, -1 otherwize
   */
  int gt(const char * ColName, Uint32 value) {
    NdbOperation * op = self->getNdbOperation();
    if (op == NULL)
      return -1;
    int ColId = getColumnId(op,ColName);
    if (ColId == -1) {
      return ColId;
    }
    return self->gt(ColId,value);
  }

  /** Compare column value with integer for greater than or equal.
   *  ®return  0 if successful, -1 otherwize
   */
  int ge(const char * ColName, Uint32 value)  {
    NdbOperation * op = self->getNdbOperation();
    if (op == NULL)
      return -1;
    int ColId = getColumnId(op,ColName);
    if (ColId == -1) {
      return ColId;
    }
    return self->ge(ColId,value);
  }


  /** Compare column value with integer for equal. 64-bit.
   *  ®return  0 if successful, -1 otherwize
   */
  int eq(const char * ColName, Uint64 value)  {
    NdbOperation * op = self->getNdbOperation();
    if (op == NULL)
      return -1;
    int ColId = getColumnId(op,ColName);
    if (ColId == -1) {
      return ColId;
    }
    return self->eq(ColId,value);
  }
  int eq(const char * ColName, const char * anInputString, size_t len ) {
    NdbOperation * op = self->getNdbOperation();
    if (op == NULL)
      return -1;
    int ColId = getColumnId(op,ColName);
    if (ColId == -1) {
      return ColId;
    }

    const NdbDictionary::Column * theColumn = op->getTable()->getColumn(ColId);
    char * stringVal = ndbFormatString(theColumn,anInputString,len);
    if (stringVal == NULL)
      return -1;
    Uint32 cmpLength = cmpGetColumnLength(theColumn);
    if (cmpLength == 0)
      return -1;
    int retval = self->cmp(NdbScanFilter::COND_EQ,ColId,
                           (void *)stringVal,cmpLength);
    free(stringVal);
    return retval;
  };

  int eq(int ColId, const char * anInputString, size_t len ) {
    NdbOperation * op = self->getNdbOperation();
    if (op == NULL)
      return -1;

    const NdbDictionary::Column * theColumn = op->getTable()->getColumn(ColId);
    char * stringVal = ndbFormatString(theColumn,anInputString,len);
    if (stringVal == NULL)
      return -1;
    Uint32 cmpLength = cmpGetColumnLength(theColumn);
    if (cmpLength == 0)
      return -1;
    int retval = self->cmp(NdbScanFilter::COND_EQ,ColId,
                           (void *)stringVal,cmpLength);
    free(stringVal);
    return retval;
  };

  /** Compare column value with integer for not equal. 64-bit.
   *  ®return  0 if successful, -1 otherwize
   */
  int ne(const char * ColName, Uint64 value) {
    NdbOperation * op = self->getNdbOperation();
    if (op == NULL)
      return -1;
    int ColId = getColumnId(op,ColName);
    if (ColId == -1) {
      return ColId;
    }
    return self->ne(ColId,value);
  }

  /** Compare column value with integer for less than. 64-bit.
   *  ®return  0 if successful, -1 otherwize
   */
  int lt(const char * ColName, Uint64 value) {
    NdbOperation * op = self->getNdbOperation();
    if (op == NULL)
      return -1;
    int ColId = getColumnId(op,ColName);
    if (ColId == -1) {
      return ColId;
    }
    return self->lt(ColId,value);
  }

  /** Compare column value with integer for less than or equal. 64-bit.
   *  ®return  0 if successful, -1 otherwize
   */
  int le(const char * ColName, Uint64 value) {
    NdbOperation * op = self->getNdbOperation();
    if (op == NULL)
      return -1;
    int ColId = getColumnId(op,ColName);
    if (ColId == -1) {
      return ColId;
    }
    return self->le(ColId,value);
  }

  /** Compare column value with integer for greater than. 64-bit.
   *  ®return  0 if successful, -1 otherwize
   */
  int gt(const char * ColName, Uint64 value)  {
    NdbOperation * op = self->getNdbOperation();
    if (op == NULL)
      return -1;
    int ColId = getColumnId(op,ColName);
    if (ColId == -1) {
      return ColId;
    }
    return self->gt(ColId,value);
  }

  /** Compare column value with integer for greater than or equal. 64-bit.
   *  ®return  0 if successful, -1 otherwize
   */
  int ge(const char * ColName, Uint64 value) {
    NdbOperation * op = self->getNdbOperation();
    if (op == NULL)
      return -1;
    int ColId = getColumnId(op,ColName);
    if (ColId == -1) {
      return ColId;
    }
    return self->ge(ColId,value);
  }

  /** Check if column value is NULL */
  int isNull(const char * ColName)  {
    NdbOperation * op = self->getNdbOperation();
    if (op == NULL)
      return -1;
    int ColId = getColumnId(op,ColName);
    if (ColId == -1) {
      return ColId;
    }
    return self->isnull(ColId);
  }

  /** Check if column value is non-NULL */
  int isNotNull(const char * ColName) {
    NdbOperation * op = self->getNdbOperation();
    if (op == NULL)
      return -1;
    int ColId = getColumnId(op,ColName);
    if (ColId == -1) {
      return ColId;
    }
    return self->isnotnull(ColId);
  }




  %ndbnoexception

     }
