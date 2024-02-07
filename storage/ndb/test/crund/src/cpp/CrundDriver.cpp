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

#include "CrundDriver.hpp"

#include <cassert>
#include <cstddef>
#include <iostream>
#include <sstream>
#include <string>

#include "helpers.hpp"
#include "string_helpers.hpp"

#include "NdbapiAB.hpp"

using std::cout;
using std::endl;
using std::flush;
using std::ios_base;
using std::ostringstream;
using std::string;

// ----------------------------------------------------------------------
// initializers/finalizers
// ----------------------------------------------------------------------

void CrundDriver::init() {
  cout << endl
       << "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++" << endl
       << "initializing benchmark ..." << endl
       << "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++"
       << endl;

  assert(myLoads.empty());
  Driver::init();
}

void CrundDriver::close() {
  cout << endl
       << "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++" << endl
       << "closing benchmark ..." << endl
       << "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++"
       << endl;

  Driver::close();
  for (Loads::iterator i = myLoads.begin(); i != myLoads.end(); ++i) delete *i;
  myLoads.clear();
}

bool CrundDriver::createLoad(const string &name) {
  if (!name.compare("NdbapiAB")) {
    Load *l = new NdbapiAB(*this);
    myLoads.push_back(l);
    return true;
  }
  return false;
}

void CrundDriver::initProperties() {
  Driver::initProperties();

  cout << endl << "reading crund properties ..." << flush;
  ostringstream msg;

  vector<string> xm;
  split(toS(props[L"xMode"]), ',', std::back_inserter(xm));
  for (vector<string>::iterator i = xm.begin(); i != xm.end(); ++i) {
    XMode::E m = XMode::valueOf(*i);
    if (m == XMode::undef) {
      msg << "[IGNORED] xMode:                '" << m << "'" << endl;
    } else {
      xModes.push_back(m);
    }
  }

  const string lm = toS(props[L"lockMode"], L"none");
  lockMode = LockMode::valueOf(lm);
  if (lockMode == LockMode::undef) {
    msg << "[IGNORED] lockMode:             '" << lm << "'" << endl;
    lockMode = LockMode::none;
  }

  renewConnection = toB(props[L"renewConnection"], false);

  nOpsStart = toI(props[L"nOpsStart"], 1000, 0);
  if (nOpsStart < 1) {
    msg << "[IGNORED] nOpsStart:            '" << toS(props[L"nOpsStart"])
        << "'" << endl;
    nOpsStart = 1000;
  }
  nOpsEnd = toI(props[L"nOpsEnd"], nOpsStart, 0);
  if (nOpsEnd < nOpsStart) {
    msg << "[IGNORED] nOpsEnd:              '" << toS(props[L"nOpsEnd"]) << "'"
        << endl;
    nOpsEnd = nOpsStart;
  }
  nOpsScale = toI(props[L"nOpsScale"], 10, 0);
  if (nOpsScale < 2) {
    msg << "[IGNORED] nOpsScale:            '" << toS(props[L"nOpsScale"])
        << "'" << endl;
    nOpsScale = 10;
  }

  maxVarbinaryBytes = toI(props[L"maxVarbinaryBytes"], 100, 0);
  if (maxVarbinaryBytes < 0) {
    msg << "[IGNORED] maxVarbinaryBytes:    '"
        << toS(props[L"maxVarbinaryBytes"]) << "'" << endl;
    maxVarbinaryBytes = 100;
  }
  maxVarcharChars = toI(props[L"maxVarcharChars"], 100, 0);
  if (maxVarcharChars < 0) {
    msg << "[IGNORED] maxVarcharChars:      '" << toS(props[L"maxVarcharChars"])
        << "'" << endl;
    maxVarcharChars = 100;
  }

  maxBlobBytes = toI(props[L"maxBlobBytes"], 1000, 0);
  if (maxBlobBytes < 0) {
    msg << "[IGNORED] maxBlobBytes:         '" << toS(props[L"maxBlobBytes"])
        << "'" << endl;
    maxBlobBytes = 1000;
  }
  maxTextChars = toI(props[L"maxTextChars"], 1000, 0);
  if (maxTextChars < 0) {
    msg << "[IGNORED] maxTextChars:         '" << toS(props[L"maxTextChars"])
        << "'" << endl;
    maxTextChars = 1000;
  }

  split(toS(props[L"include"]), ',', std::back_inserter(include));
  split(toS(props[L"exclude"]), ',', std::back_inserter(exclude));

  if (!msg.tellp()) {  // or msg.str().empty() if ambiguous
    cout << "    [ok: "
         << "nOps=" << nOpsStart << ".." << nOpsEnd << "]" << endl;
  } else {
    setIgnoredSettings();
    cout << endl << msg.str() << flush;
  }
}

void CrundDriver::printProperties() {
  Driver::printProperties();

  cout << endl
       << "crund settings ..." << endl
       << "xModes:                         " << toString(xModes) << endl
       << "lockMode:                       " << lockMode << endl
       << "renewConnection:                " << renewConnection << endl
       << "nOpsStart:                      " << nOpsStart << endl
       << "nOpsEnd:                        " << nOpsEnd << endl
       << "nOpsScale:                      " << nOpsScale << endl
       << "maxVarbinaryBytes:              " << maxVarbinaryBytes << endl
       << "maxVarcharChars:                " << maxVarcharChars << endl
       << "maxBlobBytes:                   " << maxBlobBytes << endl
       << "maxTextChars:                   " << maxTextChars << endl
       << "include:                        " << toString(include) << endl
       << "exclude:                        " << toString(exclude) << endl;
}

// ----------------------------------------------------------------------
// operations
// ----------------------------------------------------------------------

void CrundDriver::runLoad(Load &load) {
  connectDB(load);

  assert(nOpsStart <= nOpsEnd && nOpsScale > 1);
  for (int i = nOpsStart; i <= nOpsEnd; i *= nOpsScale) {
    cout << endl
         << "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++"
         << endl
         << "running load ...                [nOps=" << i << "]"
         << load.getName() << endl
         << "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++"
         << endl;
    runSeries(load, i);
  }

  disconnectDB(load);
}

void CrundDriver::connectDB(Load &load) {
  cout << endl
       << "------------------------------------------------------------" << endl
       << "init connection ... " << endl
       << "------------------------------------------------------------"
       << endl;
  load.initConnection();
}

void CrundDriver::disconnectDB(Load &load) {
  cout << endl
       << "------------------------------------------------------------" << endl
       << "close connection ... " << endl
       << "------------------------------------------------------------"
       << endl;
  load.closeConnection();
}

void CrundDriver::reconnectDB(Load &load) {
  cout << endl
       << "------------------------------------------------------------" << endl
       << "renew connection ... " << endl
       << "------------------------------------------------------------"
       << endl;
  load.closeConnection();
  load.initConnection();
}

void CrundDriver::runSeries(Load &load, int nOps) {
  if (nRuns == 0) return;  // nothing to do

  for (int i = 1; i <= nRuns; i++) {
    // pre-run cleanup
    if (renewConnection) reconnectDB(load);

    cout << endl
         << "------------------------------------------------------------"
         << endl
         << "run " << i << " of " << nRuns << " [nOps=" << nOps << "]" << endl
         << "------------------------------------------------------------"
         << endl;
    runOperations(load, nOps);
  }

  writeLogBuffers(load.getName());
}

void CrundDriver::runOperations(Load &load, int nOps) {
  beginOps(nOps);
  load.clearData();
  load.runOperations(nOps);
  finishOps(nOps);
}
