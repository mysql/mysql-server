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

class NdbBlob {
public:
  enum State {
    Idle = 0,
    Prepared = 1,
    Active = 2,
    Closed = 3,
    Invalid = 9
  };
  State getState();
/*  typedef int ActiveHook(NdbBlob* me, char* arg);*/

  %ndbexception("NdbApiException") {
    $action
      if (result==-1) {
        NdbError err = arg1->getNdbError();
        NDB_exception(NdbApiException, err.message);
      }
  }
  int getValue(char* BYTE, Uint32 bytes);

#ifdef SWIGPYTHON
  %rename(setVal) setValue(const char* data, Uint32 bytes);
#endif

  int setValue(const char* BYTE, size_t len);
  /* TODO: build the structure for this callback
     int setActiveHook(ActiveHook* activeHook, char* arg);
  */
  int setNull();
  int truncate(Uint64 length = 0);
  int setPos(Uint64 pos);
  int writeData(const char* data, Uint32 bytes);

  %ndbexception("NdbErrorNotAvailable") {
    $action
      if (result==NULL) {
        NDB_exception(NdbApiException,"Problem creating NdbError object");
      }
  }

  const NdbError& getNdbError() const;

  %ndbexception("NdbApiException") {
    $action
      if (result==0) {
        NdbError err = arg1->getNdbError();
        NDB_exception(NdbApiException, err.message);
      }
  }

  const NdbDictColumn* getColumn();
  NdbBlob* blobsFirstBlob();
  NdbBlob* blobsNextBlob();
  %exception;
  static int getBlobTableName(char* btname, Ndb* anNdb, const char* tableName, const char* columnName);
private:
  NdbBlob();
};

%extend NdbBlob {
public:
  // Making this an Int64 instead of a Uint32 so that we can return
  // a -1 on error to wrap this properly in exceptions

  %ndbexception("NdbApiException") {
    $action
      if (result==-1) {
        NdbError err = arg1->getNdbError();
        NDB_exception(NdbApiException, err.message);
      }
  }

  Int64 readData(char * BYTE, size_t len) {

    Uint32 b = len;
    int ret=self->readData((void *)BYTE, b);
    if (ret==-1) {
      return -1;
    }
    return (Int64)b;
  }

  %ndbexception("NdbApiException") {
    static int ret = 0;
    $action
      if (ret==-1) {
        NdbError err = arg1->getNdbError();
        NDB_exception(NdbApiException, err.message);
      }
  }

  bool getNull() {
    static int ret;
    self->getNull(ret);

    return (ret==0);

  }

  Uint64 getLength() {

    Uint64 OUTPUT=0;
    static int ret=0;
    ret = self->getLength(OUTPUT);
    return OUTPUT;
  }

  Uint64 getPos() {
    Uint64 OUTPUT=0;
    static int ret=0;
    ret = self->getPos(OUTPUT);
    return OUTPUT;
  }

  %ndbnoexception;

};
