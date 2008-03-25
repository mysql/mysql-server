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

%delobject NdbTransaction::close;

class NdbTransaction {
  ~NdbTransaction();
  NdbTransaction(Ndb* aNdb);

public:

  const NdbError & getNdbError() const;

  %ndbexception("NdbApiException") {
    $action
      if (result==NULL) {
        NdbError err = arg1->getNdbError();
        NDB_exception(NdbApiException,err.message);
      }
  }
  Ndb* getNdb();

  NdbOperation* getNdbOperation(const class NdbDictTable* aTable);
  NdbOperation* getNdbOperation(const char* aTableName);

  /* These first two are deprecated */
  NdbIndexScanOperation* getNdbIndexScanOperation(const char* anIndexName,
                                                  const char* aTableName);
  NdbIndexScanOperation* getNdbIndexScanOperation(const NdbDictIndex *anIndex,
                                                  const NdbDictTable *aTable);
  NdbIndexScanOperation* getNdbIndexScanOperation(const NdbDictIndex *anIndex);

  NdbIndexOperation* getNdbIndexOperation(const char*  anIndexName,
                                          const char*  aTableName);
  NdbIndexOperation* getNdbIndexOperation(const NdbDictIndex *anIndex,
                                          const NdbDictTable *aTable);
  NdbIndexOperation* getNdbIndexOperation(const NdbDictIndex *anIndex);


  NdbScanOperation* getNdbScanOperation(const class NdbDictTable* aTable);
  NdbScanOperation* getNdbScanOperation(const char* aTableName);
  NdbOperation*	getNdbErrorOperation();

  %ndbnoexception;

  // TODO: Verify that this can't throw?
  const NdbOperation * getNextCompletedOperation(const NdbOperation * op)const;


  %ndbexception("NdbApiException,NdbApiTemporaryException,"
                "NdbApiPermanentException,"
                "InvalidSchemaObjectVersionException") {
    $action
      if (result < 0) {
        NdbError err = arg1->getNdbError();
        if (err.code == 241) { // Invalid Schema Version - see ndberror.c
          NDB_exception_err(InvalidSchemaObjectVersionException,
                            err.message,err);
        } else {
          switch (err.status) {
          case NdbError::TemporaryError:
            NDB_exception_err(NdbApiTemporaryException,err.message,err);
            break;
          case NdbError::PermanentError:
            // TODO: We should probably at least handle all the various
            // error classifications. mmm, thats going to suck
            // and then we should figure out how to do that all over the place
            NDB_exception_err(NdbApiPermanentException,err.message,err);
            break;
          default:
            NDB_exception_err(NdbApiException,err.message,err);
            break;
          }
        }
      }
  }
  int execute(ExecType execType,
              AbortOption abortOption = AbortOnError,
              bool force = 0 );
  int restart(void);
  int getNdbErrorLine();



};


%extend NdbTransaction {
public:

  %ndbnoexception;

  ~NdbTransaction() {
    if(self!=0)
      self->close();
  }

  void close() {
    if(self!=0)
      self->close();
  }

  bool isClosed() {
    return (self==0);
  }

  void executeAsynchPrepare(ExecType execType, asynch_callback_t * cb,
                            AbortOption abortOption = AbortOnError) {
    cb->create_time=getMicroTime();
    self->executeAsynchPrepare(execType,theCallBack,(void *)cb, abortOption);
  }

  %ndbexception("NdbApiException,NdbApiTemporaryException,"
                "NdbApiPermanentException,") {
    $action
      if (result < 0) {
        NdbError err = arg1->getNdbError();
        switch (err.status) {
        case NdbError::TemporaryError:
          NDB_exception_err(NdbApiTemporaryException,err.message,err);
          break;
        case NdbError::PermanentError:
          NDB_exception_err(NdbApiPermanentException,err.message,err);
          break;
        default:
          NDB_exception_err(NdbApiException,err.message,err);
          break;
        }
      }
  }

  int executeNoCommit(AbortOption abortOption = AbortOnError) {
    return self->execute(NoCommit, abortOption,false);
  }
  int executeCommit(AbortOption abortOption = AbortOnError) {
    return self->execute(Commit, abortOption,false);
  }
  int executeRollback(AbortOption abortOption = AbortOnError) {
    return self->execute(Rollback, abortOption,false);
  }
  %ndbexception("NdbApiException") {
    $action
      if (result == NULL) {
        NdbError err = arg1->getNdbError();
        NDB_exception(NdbApiException,err.message);
      }
  }

  %ndbexception("NdbApiException") {
    $action
      // This should get set before we ever get called.
      static bool selectCountError;
    if ( selectCountError ) {
      NdbError err = arg1->getNdbError();
      NDB_exception(NdbApiException,err.message);
    }
  }

  Uint64 selectCount(const char * tbl) {

    /**
     * This code is taken from the ndb_select_count.cpp utility program,
     * distributed with mysql-src.
     */
    Uint64 OUTPUT = 0;
    static bool selectCountError=false;
    int MAX_RETRIES=3;
    int retryAttempt=0;
    bool finished=false;

    NdbScanOperation *pOp = self->getNdbScanOperation(tbl);
    if (!pOp) {
      selectCountError=true;
      return OUTPUT;
    }

    // we retry the transaction up to MAX_RETRIES if we have a temporary error
    while ((retryAttempt < MAX_RETRIES) && !(finished)) {

      if( pOp->readTuples(NdbScanOperation::LM_Dirty) ) {

        finished=true;
        selectCountError = true; //NDBJ_ERR_SCAN_FAILED;
      }

      int check;
      check = pOp->interpret_exit_last_row();

      if( check == -1 ) {

        finished=true;
        selectCountError = true; //NDBJ_ERR_SCAN_FAILED;

      } else if (selectCountError == false) {

        Uint64 tmp;
        Uint32 row_size;
        pOp->getValue(NdbDictColumn::ROW_COUNT, (char*)&tmp);
        pOp->getValue(NdbDictColumn::ROW_SIZE, (char*)&row_size);
        check = self->execute(NdbTransaction::NoCommit);
        if( check != -1 ) {
          int eof;
          while((eof = pOp->nextResult(true)) == 0){
            OUTPUT += tmp;
          }
          if (eof == -1) {
            const NdbError err = self->getNdbError();
            // retry the transaction if we have a temporary error
            if (err.status == NdbError::TemporaryError){
              // usleep is Linux-specific. Its time unit is microseconds
              // sleep for 50 milliseconds
              usleep(50*1000);
              retryAttempt++;
              continue;
            }
            selectCountError = true; //NDBJ_SELECT_COUNT_ROW_COUNT_FAILED;
            finished=true;
          } else {
            selectCountError=false; //row_count;
            finished=true;
          }
        } else {
          selectCountError=true; //NDBJ_SELECT_COUNT_TRANS_EXEC_FAILED;
          finished=true;
        }
      }
    }
    if (finished==false) {
      selectCountError=true; //NDBJ_SELECT_COUNT_ROW_COUNT_FAILED;
    }

    // The return value 'ret' is the 'length' returned by select_count
    return OUTPUT;

  };
  %ndbnoexception;



};
