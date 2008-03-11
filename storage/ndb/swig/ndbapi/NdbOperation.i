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

class NdbOperation {

  NdbOperation(Ndb* aNdb, Type aType = PrimaryKeyAccess);
  virtual ~NdbOperation();

public:

  enum Type {
    PrimaryKeyAccess = 0,
    UniqueIndexAccess = 1,
    TableScan = 2,
    OrderedIndexScan = 3,
  };

  enum LockMode {
    LM_Read = 0,
    LM_Exclusive = 1,
    LM_CommittedRead = 2,
    LM_Dirty = 2
  };

  %ndbnoexception;

  // These are not expected to fail
  const NdbError & getNdbError() const;
  int getNdbErrorLine();
  const char* getTableName() const;

  void setPartitionId(Uint32 id);
  Uint32 getPartitionId() const;

  const NdbDictTable * getTable() const;

  %ndbexception("NdbApiException") {
    $action
      if (result == NULL) {
        NdbError err = arg1->getNdbError();
        NDB_exception_err(NdbApiException,err.message,err);
      }
  }

  NdbTransaction* getNdbTransaction();

  virtual NdbBlob* getBlobHandle(const char* anAttrName);
  virtual NdbBlob* getBlobHandle(Uint32 anAttrId);

  %ndbexception("NdbApiException") {
    $action
      if (result==-1) {
        NdbError err = arg1->getNdbError();
        NDB_exception_err(NdbApiException,err.message,err);
      }
  }


  %rename(equalInt) equal(const char *, Int32);
  %rename(equalInt) equal(Uint32, Int32);
  voidint equal(const char* anAttrName, Int32 aValue);
  voidint equal(Uint32 anAttrId, Int32 aValue);

  %rename(equalUint) equal(const char *, Uint32);
  %rename(equalUint) equal(Uint32, Uint32);
  voidint equal(const char* anAttrName, Uint32 aValue);
  voidint equal(Uint32 anAttrId, Uint32 aValue);

  %rename(equalLong) equal(const char *, Int64);
  %rename(equalLong) equal(Uint32, Int64);
  voidint equal(const char* anAttrName, Int64 aValue);
  voidint equal(Uint32 anAttrId, Int64 aValue);

  %rename(equalUlong) equal(const char *, Uint64);
  %rename(equalUlong) equal(Uint32, Uint64);

  voidint equal(const char* anAttrName, Uint64 aValue);
  voidint equal(Uint32 anAttrId, Uint64 aValue);


  virtual voidint readTuple(LockMode);
  virtual voidint insertTuple();
  virtual voidint writeTuple();
  virtual voidint updateTuple();
  virtual voidint deleteTuple();


/* Interpreted Program Support */

#if !defined(SWIG_RUBY_AUTORENAME)
  %rename(incValueLong) incValue(const char*, Uint32);
  %rename(incValueLong) incValue(Uint32, Uint32);
  %rename(incValueUlong) incValue(const char*, Uint64);
  %rename(incValueUlong) incValue(Uint32, Uint64);

  %rename(subValueLong) subValue(const char*, Uint32);
  %rename(subValueLong) subValue(Uint32, Uint32);
  %rename(subValueUlong) subValue(const char*, Uint64);
  %rename(subValueUlong) subValue(Uint32, Uint64);
#endif

  %contract incValue(const char* anAttrName, Uint32 aValue) {
require:
    aValue > 0;
  }
  voidint incValue(const char* anAttrName, Uint32 aValue);

  %contract incValue(Uint32 anAttrId, Uint32 aValue) {
require:
    aValue > 0;
  }
  voidint incValue(Uint32 anAttrId, Uint32 aValue);

  %contract incValue(const char* anAttrName, Uint64 aValue) {
require:
    aValue > 0;
  }
  voidint incValue(const char* anAttrName, Uint64 aValue);

  %contract incValue(Uint32 anAttrId, Uint64 aValue ) {
require:
    aValue > 0;
  }
  voidint incValue(Uint32 anAttrId, Uint64 aValue);

  voidint subValue(const char* anAttrName, Uint32 aValue);
  voidint subValue(const char* anAttrName, Uint64 aValue);
  voidint subValue(Uint32 anAttrId, Uint32 aValue);
  voidint subValue(Uint32 anAttrId, Uint64 aValue);

#if !defined(SWIG_RUBY_AUTORENAME)
  %rename(defLabel) def_label(int labelNumber);
  %rename(addReg) add_reg(Uint32 RegSource1, Uint32 RegSource2, Uint32 RegDest);
  %rename(subReg) sub_reg(Uint32 RegSource1, Uint32 RegSource2, Uint32 RegDest);
  %rename(loadConstU32) load_const_u32(Uint32 RegDest, Uint32 Constant);
  %rename(loadConstU64) load_const_u64(Uint32 RegDest, Uint64 Constant);
  %rename(loadConstNull) load_const_null(Uint32 RegDest);
  %rename(readAttr) read_attr(const char* anAttrName, Uint32 RegDest);
  %rename(writeAttr) write_attr(const char* anAttrName, Uint32 RegSource);
  %rename(readAttr) read_attr(Uint32 anAttrId, Uint32 RegDest);
  %rename(writeAttr) write_attr(Uint32 anAttrId, Uint32 RegSource);
  %rename(branchGe) branch_ge(Uint32 RegLvalue, Uint32 RegRvalue, Uint32 Label);
  %rename(branchGt) branch_gt(Uint32 RegLvalue, Uint32 RegRvalue, Uint32 Label);
  %rename(branchLe) branch_le(Uint32 RegLvalue, Uint32 RegRvalue, Uint32 Label);
  %rename(branchLt) branch_lt(Uint32 RegLvalue, Uint32 RegRvalue, Uint32 Label);
  %rename(branchEq) branch_eq(Uint32 RegLvalue, Uint32 RegRvalue, Uint32 Label);
  %rename(branchNe) branch_ne(Uint32 RegLvalue, Uint32 RegRvalue, Uint32 Label);
  %rename(branchNeNull) branch_ne_null(Uint32 RegLvalue, Uint32 Label);
  %rename(branchEqNull) branch_eq_null(Uint32 RegLvalue, Uint32 Label);
  %rename(branchLabel) branch_label(Uint32 Label);
  %rename(branchColEqNull) branch_col_eq_null(Uint32 ColId, Uint32 Label);
  %rename(branchColNeNull) branch_col_ne_null(Uint32 ColId, Uint32 Label);
#endif

  voidint def_label(int labelNumber);
  voidint add_reg(Uint32 RegSource1, Uint32 RegSource2, Uint32 RegDest);
  voidint sub_reg(Uint32 RegSource1, Uint32 RegSource2, Uint32 RegDest);
  voidint load_const_u32(Uint32 RegDest, Uint32 Constant);
  voidint load_const_u64(Uint32 RegDest, Uint64 Constant);
  voidint load_const_null(Uint32 RegDest);
  voidint read_attr(const char* anAttrName, Uint32 RegDest);
  voidint write_attr(const char* anAttrName, Uint32 RegSource);
  voidint read_attr(Uint32 anAttrId, Uint32 RegDest);
  voidint write_attr(Uint32 anAttrId, Uint32 RegSource);
  voidint branch_ge(Uint32 RegLvalue, Uint32 RegRvalue, Uint32 Label);
  voidint branch_gt(Uint32 RegLvalue, Uint32 RegRvalue, Uint32 Label);
  voidint branch_le(Uint32 RegLvalue, Uint32 RegRvalue, Uint32 Label);
  voidint branch_lt(Uint32 RegLvalue, Uint32 RegRvalue, Uint32 Label);
  voidint branch_eq(Uint32 RegLvalue, Uint32 RegRvalue, Uint32 Label);
  voidint branch_ne(Uint32 RegLvalue, Uint32 RegRvalue, Uint32 Label);
  voidint branch_ne_null(Uint32 RegLvalue, Uint32 Label);
  voidint branch_eq_null(Uint32 RegLvalue, Uint32 Label);
  voidint branch_label(Uint32 Label);
  voidint branch_col_eq_null(Uint32 ColId, Uint32 Label);
  voidint branch_col_ne_null(Uint32 ColId, Uint32 Label);

#if !defined(SWIG_RUBY_AUTORENAME)
  %rename(interpretExitOk) interpret_exit_ok();
  %rename(interpretExitNok) interpret_exit_nok(Uint32);
  %rename(interpretExitNok) interpret_exit_nok();
  %rename(interpretExitLastRow) interpret_exit_last_row();
  %rename(defSubroutine) def_subroutine(int);
  %rename(callSub) call_sub(Uint32);
  %rename(retSub) ret_sub();
#endif

  voidint interpret_exit_ok();
  voidint interpret_exit_nok(Uint32 ErrorCode);
  voidint interpret_exit_nok();
  voidint interpret_exit_last_row();
  voidint def_subroutine(int SubroutineNumber);
  voidint call_sub(Uint32 Subroutine);
  voidint ret_sub();

  virtual voidint interpretedUpdateTuple();
  virtual voidint interpretedDeleteTuple();


  %ndbnoexception;

};


%extend NdbOperation {


public:

  %ndbexception("NdbApiException") {
    $action
      if (result==NULL) {
        NdbError err = arg1->getNdbError();
        NDB_exception_err(NdbApiException,err.message,err);
      }
  }

  NdbRecAttr* getValue(const char* anAttrName) {
    return self->getValue(anAttrName,NULL);
  }
  NdbRecAttr* getValue(Uint32 anAttrId) {
    return self->getValue(anAttrId,NULL);
  }
  NdbRecAttr* getValue(const NdbDictColumn* col) {
    return self->getValue(col,NULL);
  }


  const char * getColumnCharsetName(const char* columnName) {
    const NdbDictColumn * theColumn = self->getTable()->getColumn(columnName);
    const CHARSET_INFO * csinfo = theColumn->getCharset();
    if (csinfo == NULL) {
      return "latin1";
    }
    return csinfo->csname;
  }

  %ndbexception("NdbApiException") {
    $action
      if (result==-1) {
        NdbError err = arg1->getNdbError();
        NDB_exception_err(NdbApiException,err.message,err);
      }
  }

  voidint equalNull(const NdbDictColumn * theColumn) {
    return self->equal(theColumn->getName(), (char*)0);
  }
  voidint equalNull(const char* anAttrName) {
    return self->equal(anAttrName, (char*)0);
  }
  voidint equalNull(Uint32 anAttrId) {
    return self->equal(anAttrId, (char*)0);
  }

  voidint equalInt(const NdbDictColumn * theColumn, Int32 theValue) {
    return self->equal(theColumn->getName(), theValue);
  }
  voidint equalUint(const NdbDictColumn * theColumn, Uint32 theValue) {
    return self->equal(theColumn->getName(), theValue);
  }
  voidint equalLong(const NdbDictColumn * theColumn, Int64 theValue) {
    return self->equal(theColumn->getName(), theValue);
  }
  voidint equalUlong(const NdbDictColumn * theColumn, Uint64 theValue) {
    return self->equal(theColumn->getName(), theValue);
  }
  voidint equalShort(const NdbDictColumn * theColumn, short theValue) {
    return self->equal(theColumn->getName(), theValue);
  }
  voidint equalShort(const char* anAttrName, short theValue) {
    return self->equal(anAttrName, (Int32)theValue);
  }
  voidint equalShort(Uint32 anAttrId, short theValue) {
    return self->equal(anAttrId, (Int32)theValue);
  }

  voidint setBytes(const char* anAttrName, const char* BYTE, size_t len) {
    const NdbDictColumn * theColumn = self->getTable()->getColumn(anAttrName);
    char * stringVal = ndbFormatString(theColumn,BYTE,len);
    if (stringVal == NULL)
      return -1;
    int retval = self->setValue(anAttrName,stringVal);
    free(stringVal);
    return retval;
  }
  voidint setBytes(Uint32 anAttrId, const char* BYTE, size_t len) {
    const NdbDictColumn * theColumn = self->getTable()->getColumn(anAttrId);
    char * stringVal = ndbFormatString(theColumn,BYTE,len);
    if (stringVal == NULL)
      return -1;
    int retval = self->setValue(anAttrId,stringVal);
    free(stringVal);
    return retval;
  }

  voidint setString(const char* anAttrName,
                    const char* anInputString, size_t len) {
    const NdbDictColumn * theColumn = self->getTable()->getColumn(anAttrName);
    char * stringVal = ndbFormatString(theColumn,anInputString,len);
    if (stringVal == NULL)
      return -1;
    int retval = self->setValue(anAttrName,stringVal);
    free(stringVal);
    return retval;
  }
  voidint setString(Uint32 anAttrId,
                    const char* anInputString, size_t len) {
    const NdbDictColumn * theColumn = self->getTable()->getColumn(anAttrId);
    char * stringVal = ndbFormatString(theColumn,anInputString,len);
    if (stringVal == NULL)
      return -1;
    int retval = self->setValue(anAttrId,stringVal);
    free(stringVal);
    return retval;
  }

  voidint equalString(const char* anAttrName,
                      const char* anInputString, size_t len) {
    const NdbDictColumn * theColumn = self->getTable()->getColumn(anAttrName);
    char * stringVal = ndbFormatString(theColumn,anInputString,len);
    if (stringVal == NULL)
      return -1;
    int retval = self->equal(anAttrName,stringVal);
    free(stringVal);
    return retval;
  }
  voidint equalString(Uint32 anAttrId,
                      const char* anInputString, size_t len) {
    const NdbDictColumn * theColumn = self->getTable()->getColumn(anAttrId);
    char * stringVal = ndbFormatString(theColumn,anInputString,len);
    if (stringVal == NULL)
      return -1;
    int retval = self->equal(anAttrId,stringVal);
    free(stringVal);
    return retval;
  }

  voidint setShort(const char* anAttrName, short intVal) {
    return self->setValue(anAttrName,(Int32)intVal);
  }
  voidint setShort(Uint32 anAttrId, short intVal) {
    return self->setValue(anAttrId,(Int32)intVal);
  }

  voidint setInt(const char* anAttrName, Int32 intVal) {
    return self->setValue(anAttrName,intVal);
  }
  voidint setInt(Uint32 anAttrId, Int32 intVal) {
    return self->setValue(anAttrId,intVal);
  }

  voidint setLong(const char* anAttrName, Int64 intVal) {
    return self->setValue(anAttrName,intVal);
  }
  voidint setLong(Uint32 anAttrId, Int64 intVal) {
    return self->setValue(anAttrId,intVal);
  }

  voidint setDouble(const char* anAttrName, double intVal) {
    return self->setValue(anAttrName,intVal);
  }
  voidint setDouble(Uint32 anAttrId, double intVal) {
    return self->setValue(anAttrId,intVal);
  }

  voidint setFloat(const char* anAttrName, float intVal) {
    return self->setValue(anAttrName,intVal);
  }
  voidint setFloat(Uint32 anAttrId, float intVal) {
    return self->setValue(anAttrId,intVal);
  }

  voidint setDecimal(Uint32 anAttrId, decimal_t * dec) {

    int theScale = dec->frac;
    int thePrecision = (dec->intg)+theScale;

    char * theValue = (char *) malloc(decimal_bin_size(thePrecision,
                                                       theScale));
    decimal2bin(dec, theValue, thePrecision, theScale);
    int ret = self->setValue(anAttrId,theValue);
    free(theValue);
    return ret;
  }
  voidint setDecimal(const char* anAttrName, decimal_t * dec) {

    int theScale = dec->frac;
    int thePrecision = (dec->intg)+theScale;

    char * theValue = (char *) malloc(decimal_bin_size(thePrecision,
                                                       theScale));
    decimal2bin(dec, theValue, thePrecision, theScale);
    int ret = self->setValue(anAttrName,theValue);
    free(theValue);
    return ret;
  }

  voidint setDatetime(const char* anAttrName, NdbDateTime * anInputDateTime) {

    const NdbDictColumn * theColumn = self->getTable()->getColumn(anAttrName);

    Uint64 dtval = ndbFormatDateTime(theColumn,anInputDateTime);
    if (dtval == 1) {
      return dtval;
    }
    return self->setValue(anAttrName,dtval);

  }

  voidint setDatetime(Uint32 anAttrId, NdbDateTime * anInputDateTime) {

    const NdbDictColumn * theColumn = self->getTable()->getColumn(anAttrId);

    Uint64 dtval = ndbFormatDateTime(theColumn,anInputDateTime);
    if (dtval == 1) {
      return dtval;
    }
    return  self->setValue(anAttrId,dtval);
  }

  voidint setTimestamp(const char* anAttrName, NdbTimestamp anInputTimestamp) {
    return self->setValue(anAttrName,anInputTimestamp);
  }
  voidint setTimestamp(Uint32 anAttrId, NdbTimestamp anInputTimestamp) {
    return  self->setValue(anAttrId,anInputTimestamp);
  }


  voidint setNull(const char * anAttrName) {
    return self->setValue(anAttrName,(char *)0);
  }
  voidint setNull(Uint32 anAttrId) {
    return self->setValue(anAttrId,(char *)0);
  }


  voidint equalBytes(const NdbDictColumn * theColumn,
                     const char* BYTE, size_t len) {
    char * stringVal = ndbFormatString(theColumn,BYTE,len);
    if (stringVal == NULL)
      return -1;
    int retval = self->equal(theColumn->getName(),stringVal);
    free(stringVal);
    return retval;
  }
  voidint equalBytes(const char* anAttrName,
                     const char* BYTE, size_t len) {
    const NdbDictColumn * theColumn = self->getTable()->getColumn(anAttrName);
    char * stringVal = ndbFormatString(theColumn,BYTE,len);
    if (stringVal == NULL)
      return -1;
    int retval = self->equal(anAttrName,stringVal);
    free(stringVal);
    return retval;
  }
  voidint equalBytes(Uint32 anAttrId,
                     const char* BYTE, size_t len) {
    const NdbDictColumn * theColumn = self->getTable()->getColumn(anAttrId);
    char * stringVal = ndbFormatString(theColumn,BYTE,len);
    if (stringVal == NULL)
      return -1;
    int retval = self->equal(anAttrId,stringVal);
    free(stringVal);
    return retval;
  }

  voidint equalDatetime(const NdbDictColumn * theColumn,
                        NdbDateTime * anInputDateTime) {

    Uint64 dtval = ndbFormatDateTime(theColumn,anInputDateTime);
    if (dtval == 1)
      return dtval;
    return self->equal(theColumn->getName(),dtval);
  }
  voidint equalDatetime(const char* anAttrName,
                        NdbDateTime * anInputDateTime) {

    const NdbDictColumn * theColumn = self->getTable()->getColumn(anAttrName);

    Uint64 dtval = ndbFormatDateTime(theColumn,anInputDateTime);
    if (dtval == 1)
      return dtval;
    return self->equal(anAttrName,dtval);
  }
  voidint equalDatetime(Uint32 anAttrId, NdbDateTime * anInputDateTime) {

    const NdbDictColumn * theColumn = self->getTable()->getColumn(anAttrId);

    Uint64 dtval = ndbFormatDateTime(theColumn,anInputDateTime);
    if (dtval == 1)
      return dtval;
    return  self->equal(anAttrId,dtval);
  }

  voidint equalTimestamp(const NdbDictColumn * theColumn,
                         NdbTimestamp anInputTimestamp) {
    return self->equal(theColumn->getName(),anInputTimestamp);
  }
  voidint equalTimestamp(const char* anAttrName,
                         NdbTimestamp anInputTimestamp) {
    return self->equal(anAttrName,anInputTimestamp);
  }
  voidint equalTimestamp(Uint32 anAttrId,
                         NdbTimestamp anInputTimestamp) {
    return  self->equal(anAttrId,anInputTimestamp);
  }

  voidint equalDecimal(const NdbDictColumn * theColumn, decimal_t * decVal) {

    const int prec = theColumn->getPrecision();
    const int scale = theColumn->getScale();

    char * theValue = (char *) malloc(decimal_bin_size(prec, scale));
    decimal2bin(decVal, theValue, prec, scale);
    int ret = self->equal(theColumn->getName(),theValue);
    free(theValue);
    return ret;
  }
  voidint equalDecimal(Uint32 anAttrId, decimal_t * decVal) {
    const NdbDictColumn * theColumn = self->getTable()->getColumn(anAttrId);

    const int prec = theColumn->getPrecision();
    const int scale = theColumn->getScale();

    char * theValue = (char *) malloc(decimal_bin_size(prec, scale));
    decimal2bin(decVal, theValue, prec, scale);
    int ret = self->equal(anAttrId,theValue);
    free(theValue);
    return ret;
  }
  voidint equalDecimal(const char * anAttrName, decimal_t * decVal) {
    const NdbDictColumn * theColumn = self->getTable()->getColumn(anAttrName);

    const int prec = theColumn->getPrecision();
    const int scale = theColumn->getScale();

    char * theValue = (char *) malloc(decimal_bin_size(prec, scale));
    decimal2bin(decVal, theValue, prec, scale);
    int ret = self->equal(anAttrName,theValue);
    free(theValue);
    return ret;
  }

  voidint getColumnId(const char* columnName) {
    return getColumnId(self,columnName);
  }


  voidint branchColEq(Uint32 ColId, Int64 val,  Uint32 len,
                      bool nopad, Uint32 Label) {
    return self->branch_col_eq(ColId, (void *) &val, len, nopad, Label);
  }
  voidint branchColNe(Uint32 ColId, Int64 val,  Uint32 len,
                      bool nopad, Uint32 Label) {
    return self->branch_col_ne(ColId, (void *) &val, len, nopad, Label);
  }
  voidint branchColLt(Uint32 ColId, Int64 val,  Uint32 len,
                      bool nopad, Uint32 Label) {
    return self->branch_col_lt(ColId, (void *) &val, len, nopad, Label);
  }
  voidint branchColLe(Uint32 ColId, Int64 val,  Uint32 len,
                      bool nopad, Uint32 Label) {
    return self->branch_col_le(ColId, (void *) &val, len, nopad, Label);
  }
  voidint branchColGt(Uint32 ColId, Int64 val,  Uint32 len,
                      bool nopad, Uint32 Label) {
    return self->branch_col_gt(ColId, (void *) &val, len, nopad, Label);
  }
  voidint branchColGe(Uint32 ColId, Int64 val, Uint32 len,
                      bool nopad, Uint32 Label) {
    return self->branch_col_ge(ColId, (void *) &val, len, nopad, Label);
  }
  voidint branchColLikeString(Uint32 ColId,
                              const char* anInputString, size_t len,
                              bool nopad, Uint32 Label) {
    return self->branch_col_like(ColId, (void *)anInputString, (Uint32)len,
                                 nopad, Label);
  };
  voidint branchColNotLikeString(Uint32 ColId,
                                 const char* anInputString, size_t len,
                                 bool nopad, Uint32 Label) {
    return self->branch_col_notlike(ColId, (void *)anInputString, (Uint32)len,
                                    nopad, Label);
  };
  voidint branchColLikeBytes(Uint32 ColId, const char* BYTE, size_t len,
                             bool nopad, Uint32 Label) {
    return self->branch_col_like(ColId, (void *)BYTE, (Uint32)len,
                                 nopad, Label);
  };
  voidint branchColNotLikeBytes(Uint32 ColId, const char* BYTE, size_t len,
                                bool nopad, Uint32 Label) {
    return self->branch_col_notlike(ColId, (void *)BYTE, (Uint32)len,
                                    nopad, Label);
  };
  %ndbnoexception;


};

