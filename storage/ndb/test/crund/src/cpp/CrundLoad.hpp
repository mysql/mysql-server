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

#ifndef CrundLoad_hpp
#define CrundLoad_hpp

#include <sstream>
#include <string>
#include <vector>

#include "CrundDriver.hpp"
#include "Load.hpp"
#include "string_helpers.hpp"

using std::cout;
using std::endl;
using std::ostringstream;
using std::string;
using std::vector;

class CrundLoad : public Load {
 public:
  // usage
  CrundLoad(const string &name, CrundDriver &driver)
      : Load(name), driver(driver), sdata(NULL), bdata(NULL) {
    driver.addLoad(*this);
  }
  virtual ~CrundLoad() {}

  // initializers/finalizers
  virtual void init();
  virtual void close();

  // datastore operations
  virtual void initConnection() = 0;
  virtual void closeConnection() = 0;
  virtual void clearData() = 0;

 protected:
  // resources
  CrundDriver &driver;

  // benchmark data & types
  typedef vector<int> Ids;     // key values and collections thereof
  vector<string> *sdata;       // configured character data sets
  typedef vector<char> bytes;  // binary data (C++11: array<char>)
  vector<bytes> *bdata;        // configured binary data sets

  // benchmark operations & types
  struct Op {
    const string name;

    Op(const string &name) : name(name) {}
    virtual ~Op() {}
    virtual void run(const Ids &id) = 0;
  };
  typedef vector<Op *> Operations;  // collection of operations
  Operations operations;            // filled by subclasses

  // initializers/finalizers
  virtual void initProperties() {}
  virtual void printProperties() {}

  // benchmark operations
  virtual void initOperations() = 0;
  virtual void closeOperations() = 0;
  virtual void clearPersistenceContext() {}
  virtual void runOperations(int nOps);
  virtual void runOperation(Op &op, const vector<int> &id);
  virtual bool excludedOperation(const string &name);

  // helpers
  template <typename T>
  void verify(int exp, T act);
  void verify(const string &exp, const string &act);
  void verify(const bytes &exp, const bytes &act);
};

// ----------------------------------------------------------------------
// helpers
// ----------------------------------------------------------------------

template <typename T>
inline void CrundLoad::verify(int exp, T act) {
  if (exp != act) {
    ostringstream msg;
    msg << "numeric data verification failed:"
        << " expected = " << exp << ", actual = " << act;
    driver.logError(name, msg.str());
    // cout << endl << "!!! load: " << name << endl << msg.str() << endl;
  }
}

inline void CrundLoad::verify(const string &exp, const string &act) {
  // if (exp.compare(act) != 0) {
  if (exp != act) {
    ostringstream msg;
    msg << "string data verification failed:"
        << " expected size = " << exp.size() << ", actual size = '"
        << act.size();
    if (exp.size() > 0 && act.size() > 0) {
      msg << endl
          << "  expected = '" << exp[0] << "...'" << endl
          << "  actual   = '" << act[0] << "...'";
    }
    driver.logError(name, msg.str());
    // cout << endl << "!!! load: " << name << endl << msg.str() << endl;
  }
}

inline void CrundLoad::verify(const CrundLoad::bytes &exp,
                              const CrundLoad::bytes &act) {
  if (exp != act) {
    ostringstream msg;
    msg << "binary data verification failed:"
        << " expected size = " << exp.size() << ", actual size = '"
        << act.size();
    if (exp.size() > 0 && act.size() > 0) {
      msg << endl
          << "  expected = [" << int(exp[0]) << "...]" << endl
          << "  actual   = [" << int(act[0]) << "...]";
    }
    driver.logError(name, msg.str());
    // cout << endl << "!!! load: " << name << endl << msg.str() << endl;
  }
}

#endif  // CrundLoad_hpp
