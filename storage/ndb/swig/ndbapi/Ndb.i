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

%delobject Ndb::close;
%delobject Ndb::closeTransaction;

class Ndb {

public:
  Ndb(Ndb_cluster_connection *ndb_cluster_connection,
      const char* aCatalogName = "", const char* aSchemaName = "def");
  ~Ndb();

  %ndbnoexception;

  // These are not expected to fail
  const NdbError & getNdbError() const;
  const NdbError & getNdbError(int errorCode);

  const char * getDatabaseName() const;
  void setDatabaseName(const char * aDatabaseName);
  const char * getDatabaseSchemaName() const;
  void setDatabaseSchemaName(const char * aDatabaseSchemaName);

  void closeTransaction(NdbTransaction*);

  NdbEventOperation *nextEvent();

  void sendPreparedTransactions(int forceSend = 0);

  %ndbexception("NdbApiException") {
    $action
      if (result==-1) {
        NdbError err = arg1->getNdbError();
        NDB_exception(NdbApiException,err.message);
      }
  }
  int dropEventOperation(NdbEventOperation* eventOp);
  int sendPollNdb(int aMillisecondNumber = WAITFOR_RESPONSE_TIMEOUT,
                  int minNoOfEventsToWakeup = 1,
                  int forceSend = 0);
  int  pollNdb(int aMillisecondNumber = WAITFOR_RESPONSE_TIMEOUT,
               int minNoOfEventsToWakeup = 1);

  %ndbexception("NdbApiException") {
    $action
      if (result==NULL) {
        NdbError err = arg1->getNdbError();
        NDB_exception(NdbApiException,err.message);
      }
  }

  class NdbDictDictionary* getDictionary();
  NdbEventOperation* createEventOperation(const char* eventName);

};

%extend Ndb {
public:


  %ndbexception("NdbApiException") init {
    $action
      if (result) {
        NDB_exception(NdbApiException,"Cluster not ready");
      }
  }
  int init(int maxNoOfTransactions = 4) {
    /* This is becoming a no-op, since we're forcing it */
    return 0;
  }

  %ndbexception("NdbApiException") {
    $action
      if (result==(Uint64)-1) {
        NdbError err = arg1->getNdbError();
        NDB_exception_err(NdbApiException,err.message,err);
// TODO: Really? There isn't a better way to do this?
#if defined(SWIGPERL)
        SWIG_croak_null();
#else
#if defined(SWIGPHP)
        return;
#else
        return (Uint64)0;
#endif //defined(SWIGPHP)
#endif //defined(SWIGPERL)
      }
  }
  Uint64 getAutoIncrementValue(const char* aTableName,
                               Uint32 cacheSize) {

    Uint64 id = 0;
    int ret = self->getAutoIncrementValue(aTableName,id,cacheSize);
    if (ret == -1) {
      return (Uint64)-1;
    }
    return id;
  };
  Uint64 getAutoIncrementValue(NdbDictTable * myTable,
                               Uint32 cacheSize) {

    Uint64 id = 0;
    int ret = self->getAutoIncrementValue(myTable,id,cacheSize);
    if (ret == -1) {
      return (Uint64)-1;
    }
    return id;
  };
  %ndbexception("NdbApiException") {
    $action
      if (result < 0) {
        NdbError err = arg1->getNdbError();
        NDB_exception_err(NdbApiException,err.message,err);
      }
  }
  int pollEvents(int aMillisecondNumber, Uint64 latestGCI=0) {
    return self->pollEvents(aMillisecondNumber,&latestGCI);

  }
  %ndbexception("NdbApiException") {
    $action
      if (result == NULL) {
        NdbError err = arg1->getNdbError();
        NDB_exception(NdbApiException,err.message);
      }
  }
  const NdbEventOperation*
    getGCIEventOperations(Uint32 iter, Uint32 event_types) {
    return self->getGCIEventOperations(&iter,&event_types);
  }

  NdbTransaction* startTransaction(const char* aTableName,
                                   const char * anInputString, size_t len) {
    const NdbDictDictionary *myDict = self->getDictionary();
    const NdbDictTable *myTable = myDict->getTable(aTableName);

    Ndb::Key_part_ptr keys[2];
    keys[0].ptr=(const void*)anInputString;
    keys[0].len=len;
    keys[1].ptr=NULL;
    keys[1].len=0;
    return self->startTransaction(myTable,keys);
  }
  NdbTransaction* startTransaction(const char* aTableName,
                                   short keyData) {
    const NdbDictDictionary *myDict = self->getDictionary();
    const NdbDictTable *myTable = myDict->getTable(aTableName);

    Ndb::Key_part_ptr keys[2];
    keys[0].ptr=(const void*)&keyData;
    keys[0].len=sizeof(short);
    keys[1].ptr=NULL;
    keys[1].len=0;

    return self->startTransaction(myTable,keys);
  }
  NdbTransaction* startTransaction(const char* aTableName,
                                   Int32 keyData) {
    const NdbDictDictionary *myDict = self->getDictionary();
    const NdbDictTable *myTable = myDict->getTable(aTableName);

    Ndb::Key_part_ptr keys[2];
    keys[0].ptr=(const void*)&keyData;
    keys[0].len=sizeof(Int32);
    keys[1].ptr=NULL;
    keys[1].len=0;

    return self->startTransaction(myTable,keys);
  }
  NdbTransaction* startTransaction(const char* aTableName,
                                   Uint32 keyData) {
    const NdbDictDictionary *myDict = self->getDictionary();
    const NdbDictTable *myTable = myDict->getTable(aTableName);

    Ndb::Key_part_ptr keys[2];
    keys[0].ptr=(const void*)&keyData;
    keys[0].len=sizeof(Uint32);
    keys[1].ptr=NULL;
    keys[1].len=0;

    return self->startTransaction(myTable,keys);
  }
  NdbTransaction* startTransaction(const NdbDictTable* table,
                                   const char * anInputString, size_t len) {

    Ndb::Key_part_ptr keys[2];
    keys[0].ptr=(const void*)anInputString;
    keys[0].len=len;
    keys[1].ptr=NULL;
    keys[1].len=0;
    return self->startTransaction(table,keys);
  }
  NdbTransaction* startTransaction(const NdbDictTable* table,
                                   short keyData) {

    Ndb::Key_part_ptr keys[2];
    keys[0].ptr=(const void*)&keyData;
    keys[0].len=sizeof(short);
    keys[1].ptr=NULL;
    keys[1].len=0;

    return self->startTransaction(table,keys);
  }
  NdbTransaction* startTransaction(const NdbDictTable* table,
                                   Int32 keyData) {

    Ndb::Key_part_ptr keys[2];
    keys[0].ptr=(const void*)&keyData;
    keys[0].len=sizeof(Int32);
    keys[1].ptr=NULL;
    keys[1].len=0;

    return self->startTransaction(table,keys);
  }
  NdbTransaction* startTransaction(const NdbDictTable* table,
                                   Uint32 keyData) {

    Ndb::Key_part_ptr keys[2];
    keys[0].ptr=(const void*)&keyData;
    keys[0].len=sizeof(Uint32);
    keys[1].ptr=NULL;
    keys[1].len=0;

    return self->startTransaction(table,keys);
  }

  NdbTransaction* startTransaction() {
    return self->startTransaction();
  }

  %ndbnoexception;

  void close() {
    delete self;
  }

};

