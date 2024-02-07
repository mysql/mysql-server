/*
  Copyright (c) 2010, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef TwsDriver_hpp
#define TwsDriver_hpp

#include "Driver.hpp"

class TwsDriver : public Driver {
 protected:
  // benchmark settings
  enum LockMode { READ_COMMITTED, SHARED, EXCLUSIVE };
  static const char *toStr(LockMode mode);
  enum XMode { BULK, EACH, INDY };
  static const char *toStr(XMode mode);
  bool renewConnection;
  bool renewOperations;
  bool logSumOfOps;
  LockMode lockMode;
  int nOpsStart;
  int nOpsEnd;
  int nOpsScale;
  bool doInsert;
  bool doLookup;
  bool doUpdate;
  bool doDelete;
  bool doBulk;
  bool doEach;
  bool doIndy;
  bool doVerify;

  // benchmark initializers/finalizers
  virtual void initProperties();
  virtual void printProperties();

  // benchmark operations
  virtual void initOperations() = 0;
  virtual void closeOperations() = 0;
  virtual void runTests();
  virtual void runLoads(int nOps);
  virtual void runOperations(int nOps);
  virtual void runInserts(XMode mode, int nOps) = 0;
  virtual void runLookups(XMode mode, int nOps) = 0;
  virtual void runUpdates(XMode mode, int nOps) = 0;
  virtual void runDeletes(XMode mode, int nOps) = 0;
  void verify(int exp, int act);
  void verify(long exp, long act);
  void verify(long long exp, long long act);
  void verify(const char *exp, const char *act);

  // datastore operations
  virtual void initConnection() = 0;
  virtual void closeConnection() = 0;
  virtual void clearData() = 0;

  // XXX temp compile fixes
  virtual bool createLoad(const std::string &) {
    assert(0);
    return false;
  }
  virtual void runLoad(Load &) { assert(0); }
};

#endif  // TwsDriver_hpp
