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

class NdbIndexScanOperation : public NdbScanOperation {

  NdbIndexScanOperation(Ndb* aNdb);
  virtual ~NdbIndexScanOperation();

public:

  enum BoundType {
    BoundLE = 0,
    BoundLT = 1,
    BoundGE = 2,
    BoundGT = 3,
    BoundEQ = 4
  };

  %ndbnoexception

  bool getSorted() const;
  bool getDescending();

  %ndbexception("NdbApiException") {
    $action
      if (result == -1) {
        NdbError err = arg1->getNdbError();
        NDB_exception(NdbApiException,err.message);
      }
  }

#if !defined(SWIG_RUBY_AUTORENAME)
  %rename reset_bounds resetBounds;
  %rename end_of_bound endOfBound;
  %rename get_range_no getRangeNo;
#endif

  voidint reset_bounds(bool forceSend = false);
  voidint end_of_bound(Uint32 range_no);
  voidint get_range_no();

  virtual voidint readTuples(NdbOperation::LockMode lock_mode = LM_Read,
                             Uint32 scan_flags = 0,
                             Uint32 parallel = 0,
                             Uint32 batch = 0);

  virtual voidint readTuples(LockMode lock_mode,
                             Uint32 batch,
                             Uint32 parallel,
                             bool order_by,
                             bool order_desc = false,
                             bool read_range_no = false,
                             bool keyinfo = false,
                             bool multi_range = false);

};

%extend NdbIndexScanOperation {

public:
  %ndbexception("NdbApiException") {
    $action
      if (result == -1) {
        NdbError err = arg1->getNdbError();
        NDB_exception(NdbApiException,err.message);
      }
  }

  voidint setBoundNull(const char* anAttrName, BoundType type) {
    return self->setBound(anAttrName,type,(void *)0);
  };
  voidint setBoundNull(Uint32 anAttrId, BoundType type) {
    return self->setBound(anAttrId,type,(void *)0);
  };

  voidint setBoundInt(const char* anAttrName, BoundType type,
                      const Int32 val) {
    Int32 value = (Int32)val;
    return self->setBound(anAttrName,type,(void *) &value);
  };
  voidint setBoundInt(Uint32 anAttrId, BoundType type,
                      const Int32 val) {
    Int32 value = (Int32)val;
    return self->setBound(anAttrId,type,(void *) &value);
  };

  voidint setBoundLong(const char* anAttrName, BoundType type,
                       const Int64 val) {
    Int64 value = (Int64)val;
    return self->setBound(anAttrName,type,(void *) &value);
  };
  voidint setBoundLong(Uint32 anAttrId, BoundType type,
                       const Int64 val) {
    Int64 value = (Int64)val;
    return self->setBound(anAttrId,type,(void *) &value);
  };

  voidint setBoundUlong(const char* anAttrName, BoundType type,
                        const Uint64 val) {
    Uint64 value = (Uint64)val;
    return self->setBound(anAttrName,type,(void *) &value);
  };
  voidint setBoundUlong(Uint32 anAttrId, BoundType type,
                        const Uint64 val) {
    Uint64 value = (Uint64)val;
    return self->setBound(anAttrId,type,(void *) &value);
  };

  voidint setBoundDouble(const char* anAttrName, BoundType type,
                         const double val) {
    double value = (double)val;
    return self->setBound(anAttrName,type,(void *) &value);
  };
  voidint setBoundDouble(Uint32 anAttrId, BoundType type,
                         const double val) {
    double value = (double)val;
    return self->setBound(anAttrId,type,(void *) &value);
  };

  voidint setBoundFloat(const char* anAttrName, BoundType type,
                        const float val) {
    float value = (float)val;
    return self->setBound(anAttrName,type,(void *) &value);
  };
  voidint setBoundFloat(Uint32 anAttrId, BoundType type,
                        const float val) {
    float value = (float)val;
    return self->setBound(anAttrId,type,(void *) &value);
  };

  voidint setBoundDecimal(const char* anAttrName, BoundType type,
                          decimal_t * val) {

    int ret = -1;
    char * theValue = decimal2bytes(val);
    if (theValue != NULL) {
      ret = self->setBound(anAttrName, type, (void *) theValue);
      free(theValue);
    }
    return ret;
  };
  voidint setBoundDecimal(Uint32 anAttrId, BoundType type,
                          decimal_t * val) {

    int ret = -1;
    char * theValue = decimal2bytes(val);
    if (theValue != NULL) {
      ret = self->setBound(anAttrId, type, (void *) theValue);
      free(theValue);
    }
    return ret;
  };

  voidint setBoundTimestamp(const char* anAttrName, BoundType type,
                            NdbTimestamp anInputTimestamp) {
    Uint32 value = (Uint32)anInputTimestamp;
    return self->setBound(anAttrName,type,(void *) &value);
  };
  voidint setBoundTimestamp(Uint32 anAttrId, BoundType type,
                            NdbTimestamp anInputTimestamp) {
    Uint32 value = (Uint32)anInputTimestamp;
    return self->setBound(anAttrId,type,(void *) &value);
  };

  voidint setBoundDatetime(const char* anAttrName, BoundType type,
                           NdbDateTime * anInputDateTime) {
    const NdbDictionary::Column * theColumn =
      self->getTable()->getColumn(anAttrName);

    Uint64 dtval = ndbFormatDateTime(theColumn,anInputDateTime);
    if (dtval == 1){
      return -1;
    }
    return self->setBound(anAttrName,type,(void *) &dtval);
  };
  voidint setBoundDatetime(Uint32 anAttrId, BoundType type,
                           NdbDateTime * anInputDateTime) {
    const NdbDictionary::Column * theColumn =
      self->getTable()->getColumn(anAttrId);
    Uint64 dtval = ndbFormatDateTime(theColumn,anInputDateTime);
    if (dtval == 1){
      return -1;
    }
    return self->setBound(anAttrId,type,(void *) &dtval);
  };

  voidint setBoundBytes(const char* anAttrName, BoundType type,
                        const char * BYTE, size_t len) {
    const NdbDictionary::Column * theColumn =
      self->getTable()->getColumn(anAttrName);
    char * stringVal = ndbFormatString(theColumn,BYTE,len);
    if (stringVal == NULL) {
      return -1;
    }
    int retval = self->setBound(anAttrName,type,(void *)stringVal);
    free(stringVal);
    return retval;
  };
  voidint setBoundBytes(Uint32 anAttrId, BoundType type,
                        const char * BYTE, size_t len) {
    const NdbDictionary::Column * theColumn =
      self->getTable()->getColumn(anAttrId);
    char * stringVal = ndbFormatString(theColumn,BYTE,len);
    if (stringVal == NULL) {
      return -1;
    }
    int retval = self->setBound(anAttrId,type,(void *)stringVal);
    free(stringVal);
    return retval;
  };

  voidint setBoundString(const char* anAttrName, BoundType type,
                         const char * anInputString, size_t len) {
    const NdbDictionary::Column * theColumn =
      self->getTable()->getColumn(anAttrName);
    char * stringVal = ndbFormatString(theColumn,anInputString,len);
    if (stringVal == NULL) {
      return -1;
    }
    int retval = self->setBound(anAttrName,type,(void *)stringVal);
    free(stringVal);
    return retval;
  };
  voidint setBoundString(Uint32 anAttrId, BoundType type,
                         const char * anInputString, size_t len ) {
    const NdbDictionary::Column * theColumn =
      self->getTable()->getColumn(anAttrId);
    char * stringVal = ndbFormatString(theColumn,anInputString,len);
    if (stringVal == NULL) {
      return -1;
    }
    int retval = self->setBound(anAttrId,type,(void *)stringVal);
    free(stringVal);
    return retval;
  };

  voidint whereGreaterThan(const char * columnName, Int32 val) {
    Int32 value = (Int32)val;
    return self->setBound(columnName,NdbIndexScanOperation::BoundLT,
                          (void *) &value);
  }
  voidint whereGreaterThan(Uint32 columnId, Int32 val) {
    Int32 value = (Int32)val;
    return self->setBound(columnId,NdbIndexScanOperation::BoundLT,
                          (void *) &value);
  }
  voidint whereGreaterThan(const char * columnName, Int64 val) {
    Int64 value = (Int64)val;
    return self->setBound(columnName,NdbIndexScanOperation::BoundLT,
                          (void *) &value);
  }
  voidint whereGreaterThan(Uint32 columnId, Int64 val) {
    Int64 value = (Int64)val;
    return self->setBound(columnId,NdbIndexScanOperation::BoundLT,
                          (void *) &value);
  }
  voidint whereGreaterThan(const char * columnName, Uint64 val) {
    Uint64 value = (Uint64)val;
    return self->setBound(columnName,NdbIndexScanOperation::BoundLT,
                          (void *) &value);
  }
  voidint whereGreaterThan(Uint32 columnId, Uint64 val) {
    Uint64 value = (Uint64)val;
    return self->setBound(columnId,NdbIndexScanOperation::BoundLT,
                          (void *) &value);
  }
  voidint whereGreaterThan(const char * columnName, double val) {
    double value = (double)val;
    return self->setBound(columnName,NdbIndexScanOperation::BoundLT,
                          (void *) &value);
  }
  voidint whereGreaterThan(Uint32 columnId, double val) {
    double value = (double)val;
    return self->setBound(columnId,NdbIndexScanOperation::BoundLT,
                          (void *) &value);
  }
  voidint whereGreaterThan(const char * columnName, float val) {
    float value = (float)val;
    return self->setBound(columnName,NdbIndexScanOperation::BoundLT,
                          (void *) &value);
  }
  voidint whereGreaterThan(Uint32 columnId, float val) {
    float value = (float)val;
    return self->setBound(columnId,NdbIndexScanOperation::BoundLT,
                          (void *) &value);
  }
  voidint whereGreaterThan(const char * columnName, decimal_t * anInputValue) {

    int ret = -1;
    char * theValue = decimal2bytes(anInputValue);
    if (theValue != NULL) {
      ret = self->setBound(columnName, NdbIndexScanOperation::BoundLT,
                           (void *) theValue);
      free(theValue);
    }
    return ret;
  }
  voidint whereGreaterThan(Uint32 columnId, decimal_t * anInputValue) {

    int ret = -1;
    char * theValue = decimal2bytes(anInputValue);
    if (theValue != NULL) {
      ret = self->setBound(columnId, NdbIndexScanOperation::BoundLT,
                           (void *) theValue);
      free(theValue);
    }
    return ret;
  }

  voidint whereGreaterThan(const char* anAttrName,
                           const char * anInputString, size_t len) {
    const NdbDictionary::Column * theColumn =
      self->getTable()->getColumn(anAttrName);
    char * stringVal = ndbFormatString(theColumn,anInputString,len);
    if (stringVal == NULL) {
      return -1;
    }
    int retval = self->setBound(anAttrName,NdbIndexScanOperation::BoundLT,
                                (void *)stringVal);
    free(stringVal);
    return retval;
  };
  voidint whereGreaterThan(Uint32 anAttrId,
                           const char * anInputString, size_t len ) {
    const NdbDictionary::Column * theColumn =
      self->getTable()->getColumn(anAttrId);
    char * stringVal = ndbFormatString(theColumn,anInputString,len);
    if (stringVal == NULL) {
      return -1;
    }
    int retval = self->setBound(anAttrId,NdbIndexScanOperation::BoundLT,
                                (void *)stringVal);
    free(stringVal);
    return retval;
  };


  voidint whereGreaterThanEqualTo(const char * columnName, Int32 val) {
    Int32 value = (Int32)val;
    return self->setBound(columnName,NdbIndexScanOperation::BoundLE,
                          (void *) &value);
  }
  voidint whereGreaterThanEqualTo(Uint32 columnId, Int32 val) {
    Int32 value = (Int32)val;
    return self->setBound(columnId,NdbIndexScanOperation::BoundLE,
                          (void *) &value);
  }

  voidint whereGreaterThanEqualTo(const char * columnName, Int64 val) {
    Int64 value = (Int64)val;
    return self->setBound(columnName,NdbIndexScanOperation::BoundLE,
                          (void *) &value);
  }
  voidint whereGreaterThanEqualTo(Uint32 columnId, Int64 val) {
    Int64 value = (Int64)val;
    return self->setBound(columnId,NdbIndexScanOperation::BoundLE,
                          (void *) &value);
  }

  voidint whereGreaterThanEqualTo(const char * columnName, Uint64 val) {
    Uint64 value = (Uint64)val;
    return self->setBound(columnName,NdbIndexScanOperation::BoundLE,
                          (void *) &value);
  }
  voidint whereGreaterThanEqualTo(Uint32 columnId, Uint64 val) {
    Uint64 value = (Uint64)val;
    return self->setBound(columnId,NdbIndexScanOperation::BoundLE,
                          (void *) &value);
  }

  voidint whereGreaterThanEqualTo(const char * columnName, double val) {
    double value = (double)val;
    return self->setBound(columnName,NdbIndexScanOperation::BoundLE,
                          (void *) &value);
  }
  voidint whereGreaterThanEqualTo(Uint32 columnId, double val) {
    double value = (double)val;
    return self->setBound(columnId,NdbIndexScanOperation::BoundLE,
                          (void *) &value);
  }

  voidint whereGreaterThanEqualTo(const char * columnName, float val) {
    float value = (float)val;
    return self->setBound(columnName,NdbIndexScanOperation::BoundLE,
                          (void *) &value);
  }
  voidint whereGreaterThanEqualTo(Uint32 columnId, float val) {
    float value = (float)val;
    return self->setBound(columnId,NdbIndexScanOperation::BoundLE,
                          (void *) &value);
  }

  voidint whereGreaterThanEqualTo(const char * columnName,
                                  decimal_t * anInputDecimal) {
    int ret = -1;
    char * theValue = decimal2bytes(anInputDecimal);
    if (theValue != NULL) {
      ret = self->setBound(columnName, NdbIndexScanOperation::BoundLE,
                           (void *) theValue);
      free(theValue);
    }
    return ret;
  }
  voidint whereGreaterThanEqualTo(Uint32 columnId, Int64 anInputDecimal) {
    int ret = -1;
    char * theValue = decimal2bytes(val);
    if (theValue != NULL) {
      ret = self->setBound(columnId, NdbIndexScanOperation::BoundLE,
                           (void *) theValue);
      free(theValue);
    }
    return ret;
    Int64 value = (Int64)val;
  }

  voidint whereGreaterThanEqualTo(const char* anAttrName,
                                  const char * anInputString, size_t len) {
    const NdbDictionary::Column * theColumn =
      self->getTable()->getColumn(anAttrName);
    char * stringVal = ndbFormatString(theColumn,anInputString,len);
    if (stringVal == NULL) {
      return -1;
    }
    int retval = self->setBound(anAttrName,NdbIndexScanOperation::BoundLE,
                                (void *)stringVal);
    free(stringVal);
    return retval;
  };
  voidint whereGreaterThanEqualTo(Uint32 anAttrId,
                                  const char * anInputString, size_t len ) {
    const NdbDictionary::Column * theColumn =
      self->getTable()->getColumn(anAttrId);
    char * stringVal = ndbFormatString(theColumn,anInputString,len);
    if (stringVal == NULL) {
      return -1;
    }
    int retval = self->setBound(anAttrId,NdbIndexScanOperation::BoundLE,
                                (void *)stringVal);
    free(stringVal);
    return retval;
  };

  voidint whereLessThan(const char * columnName, Int32 val) {
    Int32 value = (Int32)val;
    return self->setBound(columnName,NdbIndexScanOperation::BoundGT,
                          (void *) &value);
  }
  voidint whereLessThan(Uint32 columnId, Int32 val) {
    Int32 value = (Int32)val;
    return self->setBound(columnId,NdbIndexScanOperation::BoundGT,
                          (void *) &value);
  }

  voidint whereLessThan(const char * columnName,
                        Int64 val) {
    Int64 value = (Int64)val;
    return self->setBound(columnName,NdbIndexScanOperation::BoundGT,
                          (void *) &value);
  }
  voidint whereLessThan(Uint32 columnId, Int64 val) {
    Int64 value = (Int64)val;
    return self->setBound(columnId,NdbIndexScanOperation::BoundGT,
                          (void *) &value);
  }

  voidint whereLessThan(const char * columnName, Uint64 val) {
    Uint64 value = (Uint64)val;
    return self->setBound(columnName,NdbIndexScanOperation::BoundGT,
                          (void *) &value);
  }
  voidint whereLessThan(Uint32 columnId, Uint64 val) {
    Uint64 value = (Uint64)val;
    return self->setBound(columnId,NdbIndexScanOperation::BoundGT,
                          (void *) &value);
  }

  voidint whereLessThan(const char * columnName, double val) {
    double value = (double)val;
    return self->setBound(columnName,NdbIndexScanOperation::BoundGT,
                          (void *) &value);
  }
  voidint whereLessThan(Uint32 columnId, double val) {
    double value = (double)val;
    return self->setBound(columnId,NdbIndexScanOperation::BoundGT,(void *) &value);
  }

  voidint whereLessThan(const char * columnName, float val) {
    float value = (float)val;
    return self->setBound(columnName,NdbIndexScanOperation::BoundGT,
                          (void *) &value);
  }
  voidint whereLessThan(Uint32 columnId, float val) {
    float value = (float)val;
    return self->setBound(columnId,NdbIndexScanOperation::BoundGT,
                          (void *) &value);
  }

  voidint whereLessThan(const char * columnName, Int64 anInputDecimal) {
    int ret = -1;
    char * theValue = decimal2bytes(anInputDecimal);
    if (theValue != NULL) {
      ret = self->setBound(columnName, NdbIndexScanOperation::BoundGT,
                           (void *) theValue);
      free(theValue);
    }
    return ret;
  }
  voidint whereLessThan(Uint32 columnId, Int64 anInputDecimal) {
    int ret = -1;
    char * theValue = decimal2bytes(anInputDecimal);
    if (theValue != NULL) {
      ret = self->setBound(columnId, NdbIndexScanOperation::BoundGT,
                           (void *) theValue);
      free(theValue);
    }
    return ret;
  }

  voidint whereLessThan(const char* anAttrName,
                        const char * anInputString, size_t len) {
    const NdbDictionary::Column * theColumn =
      self->getTable()->getColumn(anAttrName);
    char * stringVal = ndbFormatString(theColumn,anInputString,len);
    if (stringVal == NULL) {
      return -1;
    }
    int retval = self->setBound(anAttrName,NdbIndexScanOperation::BoundGT,
                                (void *)stringVal);
    free(stringVal);
    return retval;
  };
  voidint whereLessThan(Uint32 anAttrId,
                        const char * anInputString, size_t len ) {
    const NdbDictionary::Column * theColumn =
      self->getTable()->getColumn(anAttrId);
    char * stringVal = ndbFormatString(theColumn,anInputString,len);
    if (stringVal == NULL) {
      return -1;
    }
    int retval = self->setBound(anAttrId,NdbIndexScanOperation::BoundGT,
                                (void *)stringVal);
    free(stringVal);
    return retval;
  };

  voidint whereLessThanEqualTo(const char * columnName, Int32 val) {
    Int32 value = (Int32)val;
    return self->setBound(columnName,NdbIndexScanOperation::BoundGE,
                          (void *) &value);
  }
  voidint whereLessThanEqualTo(Uint32 columnId, Int32 val) {
    Int32 value = (Int32)val;
    return self->setBound(columnId,NdbIndexScanOperation::BoundGE,
                          (void *) &value);
  }

  voidint whereLessThanEqualTo(const char * columnName, Int64 val) {
    Int64 value = (Int64)val;
    return self->setBound(columnName,NdbIndexScanOperation::BoundGE,
                          (void *) &value);
  }
  voidint whereLessThanEqualTo(Uint32 columnId, Int64 val) {
    Int64 value = (Int64)val;
    return self->setBound(columnId,NdbIndexScanOperation::BoundGE,
                          (void *) &value);
  }

  voidint whereLessThanEqualTo(const char * columnName, Uint64 val) {
    Uint64 value = (Uint64)val;
    return self->setBound(columnName,NdbIndexScanOperation::BoundGE,
                          (void *) &value);
  }
  voidint whereLessThanEqualTo(Uint32 columnId, Uint64 val) {
    Uint64 value = (Uint64)val;
    return self->setBound(columnId,NdbIndexScanOperation::BoundGE,
                          (void *) &value);
  }

  voidint whereLessThanEqualTo(const char * columnName, double val) {
    double value = (double)val;
    return self->setBound(columnName,NdbIndexScanOperation::BoundGE,
                          (void *) &value);
  }
  voidint whereLessThanEqualTo(Uint32 columnId, double val) {
    double value = (double)val;
    return self->setBound(columnId,NdbIndexScanOperation::BoundGE,
                          (void *) &value);
  }

  voidint whereLessThanEqualTo(const char * columnName, float val) {
    float value = (float)val;
    return self->setBound(columnName,NdbIndexScanOperation::BoundGE,
                          (void *) &value);
  }
  voidint whereLessThanEqualTo(Uint32 columnId, float val) {
    float value = (float)val;
    return self->setBound(columnId,NdbIndexScanOperation::BoundGE,
                          (void *) &value);
  }

  voidint whereLessThanEqualTo(const char * columnName, Int64 anInputDecimal) {
    int ret = -1;
    char * theValue = decimal2bytes(anInputDecimal);
    if (theValue != NULL) {
      ret = self->setBound(columnName, NdbIndexScanOperation::BoundGE,
                           (void *) theValue);
      free(theValue);
    }
    return ret;
  }
  voidint whereLessThanEqualTo(Uint32 columnId, Int64 anInputDecimal) {
    int ret = -1;
    char * theValue = decimal2bytes(anInputDecimal);
    if (theValue != NULL) {
      ret = self->setBound(columnId, NdbIndexScanOperation::BoundGE,
                           (void *) theValue);
      free(theValue);
    }
    return ret;
  }

  voidint whereLessThanEqualTo(const char* anAttrName,
                               const char * anInputString, size_t len) {
    const NdbDictionary::Column * theColumn =
      self->getTable()->getColumn(anAttrName);
    char * stringVal = ndbFormatString(theColumn,anInputString,len);
    if (stringVal == NULL) {
      return -1;
    }
    int retval = self->setBound(anAttrName,NdbIndexScanOperation::BoundGE,
                                (void *)stringVal);
    free(stringVal);
    return retval;
  };
  voidint whereLessThanEqualTo(Uint32 anAttrId,
                               const char * anInputString, size_t len ) {
    const NdbDictionary::Column * theColumn =
      self->getTable()->getColumn(anAttrId);
    char * stringVal = ndbFormatString(theColumn,anInputString,len);
    if (stringVal == NULL) {
      return -1;
    }
    int retval = self->setBound(anAttrId,NdbIndexScanOperation::BoundGE,
                                (void *)stringVal);
    free(stringVal);
    return retval;
  };

}
