/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=4:tabstop=4:smarttab:
 *
 *  Copyright (c) 2010, 2024, Oracle and/or its affiliates.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License, version 2.0,
 *  as published by the Free Software Foundation.
 *
 *  This program is designed to work with certain software (including
 *  but not limited to OpenSSL) that is licensed under separate terms,
 *  as designated in a particular file or component or in included license
 *  documentation.  The authors of MySQL hereby grant you an additional
 *  permission to link the program and your derivative works with the
 *  separately licensed software that they have either included with
 *  the program or referenced in the documentation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License, version 2.0, for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifndef TwsDriver_hpp
#define TwsDriver_hpp

#include "Driver.hpp"

class TwsDriver : public Driver {
 protected:
  // benchmark settings
  enum LockMode { READ_COMMITTED, SHARED, EXCLUSIVE };
  static const char *toStr(LockMode mode);
  enum XMode { SINGLE, BULK, BATCH };
  static const char *toStr(XMode mode);
  bool renewConnection;
  bool doInsert;
  bool doLookup;
  bool doUpdate;
  bool doDelete;
  bool doSingle;
  bool doBulk;
  bool doBatch;
  bool doVerify;
  LockMode lockMode;
  int nRows;
  int nRuns;

  // benchmark initializers/finalizers
  virtual void init();
  virtual void close();
  virtual void initProperties();
  virtual void printProperties();

  // benchmark operations
  virtual void runTests();
  virtual void runLoads();
  virtual void runSeries();
  virtual void runOperations();
  virtual void runLoadOperations() = 0;
  void verify(int exp, int act);
  void verify(long exp, long act);
  void verify(long long exp, long long act);
  void verify(const char *exp, const char *act);

  // datastore operations
  virtual void initConnection() = 0;
  virtual void closeConnection() = 0;
  // virtual void clearPersistenceContext() = 0; // not used
  // virtual void clearData() = 0; // not used
};

#endif  // TwsDriver_hpp
