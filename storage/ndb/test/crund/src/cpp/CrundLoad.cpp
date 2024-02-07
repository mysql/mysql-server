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

#include "CrundLoad.hpp"

#include <cassert>
#include <cstddef>
#include <iostream>
#include <string>

#include "helpers.hpp"
#include "string_helpers.hpp"

using std::cout;
using std::endl;
using std::flush;
using std::string;

// ----------------------------------------------------------------------
// initializers/finalizers
// ----------------------------------------------------------------------

static void fillString(string &s,
                       int n) {  // XXX fix numbering in CrundLoad.java?
  switch (n) {
    case 0:
      s.append(1, 'i');
      break;
    case 1:
      s.append("0123456789");
      break;
    case 2:
      s.insert(0, 100, 'c');
      break;
    case 3:
      s.insert(0, 1000, 'm');
      break;
    case 4:
      s.insert(0, 10000, 'X');
      break;
    case 5:
      s.insert(0, 100000, 'C');
      break;
    case 6:
      s.insert(0, 1000000, 'M');
      break;
    default:
      string msg = string("unsupported 10**n = ") + toString(n);
      ABORT_ERROR(msg);
  }
}

void CrundLoad::init() {
  initProperties();
  printProperties();

  // initialize benchmark data
  const int max_length = 7;
  sdata = new vector<string>();
  bdata = new vector<bytes>();
  for (int i = 0; i < max_length; ++i) {
    string s;
    fillString(s, i);
    sdata->push_back(s);
    bytes b(s.c_str(), s.c_str() + s.size());
    bdata->push_back(b);
  }
}

void CrundLoad::close() {
  // release benchmark data
  delete bdata;
  delete sdata;
}

// ----------------------------------------------------------------------
// benchmark operations
// ----------------------------------------------------------------------

void CrundLoad::runOperations(int nOps) {
  vector<int> id(nOps);
  for (int i = 0; i < nOps; i++) id[i] = i * 2;

  for (Operations::iterator i = operations.begin(); i != operations.end();
       ++i) {
    clearPersistenceContext();
    runOperation(**i, id);
  }
}

void CrundLoad::runOperation(Op &op, const vector<int> &id) {
  const string &on = op.name;
  if (on.empty()) return;

  if (!excludedOperation(on)) {
    driver.beginOp(on);
    op.run(id);
    // XXX in absence of exceptions make operations/verify call
    // driver.logError(getName(), "op: " + op.getName() + ", ex:" + e);
    driver.finishOp(on, id.size());
  }
}

bool CrundLoad::excludedOperation(const string &name) {
  // XXX in absence of C++11 <regex> library
  //     std::regex_match(tested_str, std::regex(pattern_str))
  // only a simple "startsWith" test is done against include/exclude
  const vector<string> &in = driver.include;
  const vector<string> &ex = driver.exclude;

  for (vector<string>::const_iterator r = ex.begin(); r != ex.end(); ++r) {
    if (startsWith(name, *r)) {  // if (name.matches(*r))
      // cout << "* exclude " << name << ": match (" << *r << ")" << endl;
      return true;
    }
  }

  if (in.empty()) {
    // cout << "* include " << name << ": empty includes" << endl;
    return false;
  }

  for (vector<string>::const_iterator r = in.begin(); r != in.end(); ++r) {
    if (startsWith(name, *r)) {  // if (name.matches(*r))
      // cout << "* include " << name << ": match (" << *r << ")" << endl;
      return false;
    }
  }

  // cout << "* exclude " << name << ": non-match includes" << endl;
  return true;
}
