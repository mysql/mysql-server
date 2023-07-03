/*
  Copyright (c) 2010, 2023, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "TwsDriver.hpp"

#include <cstddef>
#include <cassert>
#include <iostream>
#include <sstream>
#include <string>

#include "helpers.hpp"
#include "string_helpers.hpp"

using std::cout;
using std::flush;
using std::endl;
using std::ostringstream;
using std::string;

// ---------------------------------------------------------------------------
// Helper Macros & Functions
// ---------------------------------------------------------------------------

#define ABORT_VERIFICATION_ERROR()                                      \
    do { cout << "!!! error in " << __FILE__ << ", line: " << __LINE__  \
              << ", failed data verification." << endl;                 \
        exit(-1);                                                       \
    } while (0)

// ---------------------------------------------------------------------------
// TwsDriver Implementation
// ---------------------------------------------------------------------------

void
TwsDriver::initProperties() {
    Driver::initProperties();

    cout << "setting tws properties ..." << flush;

    ostringstream msg;

    renewConnection = toB(props[L"renewConnection"], false);
    renewOperations = toB(props[L"renewOperations"], false);
    logSumOfOps = toB(props[L"logSumOfOps"], true);

    string lm = toS(props[L"lockMode"]);
    if (lm.empty()) {
        lockMode = READ_COMMITTED;
    } else if (lm.compare("READ_COMMITTED") == 0) {
        lockMode = READ_COMMITTED;
    } else if (lm.compare("SHARED") == 0) {
        lockMode = SHARED;
    } else if (lm.compare("EXCLUSIVE") == 0) {
        lockMode = EXCLUSIVE;
    } else {
        msg << "[ignored] lockMode:         '" << lm << "'" << endl;
        lockMode = READ_COMMITTED;
    }

    nOpsStart = toI(props[L"nOpsStart"], 256, 0);
    if (nOpsStart < 1) {
        msg << "[ignored] nOpsStart:            '"
            << toS(props[L"nOpsStart"]) << "'" << endl;
        nOpsStart = 256;
    }
    nOpsEnd = toI(props[L"nOpsEnd"], nOpsStart, 0);
    if (nOpsEnd < nOpsStart) {
        msg << "[ignored] nOpsEnd:              '"
            << toS(props[L"nOpsEnd"]) << "'" << endl;
        nOpsEnd = nOpsStart;
    }
    nOpsScale = toI(props[L"nOpsScale"], 2, 0);
    if (nOpsScale < 2) {
        msg << "[ignored] nOpsScale:            '"
            << toS(props[L"nOpsScale"]) << "'" << endl;
        nOpsScale = 2;
    }

    doInsert = toB(props[L"doInsert"], true);
    doLookup = toB(props[L"doLookup"], true);
    doUpdate = toB(props[L"doUpdate"], true);
    doDelete = toB(props[L"doDelete"], true);
    doBulk = toB(props[L"doBulk"], true);
    doEach = toB(props[L"doEach"], true);
    doIndy = toB(props[L"doIndy"], true);
    doVerify = toB(props[L"doVerify"], true);

    if (!msg.tellp()) {
        cout << "      [ok: "
             << "nOps=" << nOpsStart << ".." << nOpsEnd << "]" << endl;
    } else {
        cout << endl << msg.str() << endl;
    }
}

void
TwsDriver::printProperties() {
    Driver::printProperties();

    cout << endl << "tws settings..." << endl;
    cout << "renewConnection:                " << renewConnection << endl;
    cout << "renewOperations:                " << renewOperations << endl;
    cout << "logSumOfOps:                    " << logSumOfOps << endl;
    cout << "lockMode:                       " << toStr(lockMode) << endl;
    cout << "nOpsStart:                      " << nOpsStart << endl;
    cout << "nOpsEnd:                        " << nOpsEnd << endl;
    cout << "nOpsScale:                      " << nOpsScale << endl;
    cout << "doInsert:                       " << doInsert << endl;
    cout << "doLookup:                       " << doLookup << endl;
    cout << "doUpdate:                       " << doUpdate << endl;
    cout << "doDelete:                       " << doDelete << endl;
    cout << "doBulk:                         " << doBulk << endl;
    cout << "doEach:                         " << doEach << endl;
    cout << "doIndy:                         " << doIndy << endl;
    cout << "doVerify:                       " << doVerify << endl;
}

// ----------------------------------------------------------------------

void
TwsDriver::runTests() {
    cout << endl;
    initConnection();
    initOperations();

    assert(nOpsStart <= nOpsEnd && nOpsScale > 1);
    for (int i = nOpsStart; i <= nOpsEnd; i *= nOpsScale) {
        runLoads(i);
    }

    cout << endl
         << "------------------------------------------------------------" << endl
         << endl;
    clearData();
    closeOperations();
    closeConnection();
}

void
TwsDriver::runLoads(int nOps) {
    cout << endl
         << "------------------------------------------------------------" << endl;

    cout << "running operations ..."
         << "          [nOps=" << nOps << "]" << endl;

    // log buffers
    if (logRealTime) {
        rtimes << nOps;
        rta = 0L;
    }
    if (logCpuTime) {
        ctimes << nOps;
        cta = 0L;
    }

    // pre-run cleanup
    if (renewConnection) {
        closeOperations();
        closeConnection();
        initConnection();
        initOperations();
    } else if (renewOperations) {
        closeOperations();
        initOperations();
    }
    clearData();

    runOperations(nOps);

    if (logSumOfOps) {
        cout << endl
             << "total" << endl;
        if (logRealTime) {
            cout << "tx real time                    " << rta
                 << "\tms" << endl;
        }
        if (logSumOfOps) {
            cout << "tx cpu time                     " << cta
                 << "\tms" << endl;
        }
    }

    // log buffers
    if (logHeader) {
        if (logSumOfOps) {
            header << "\ttotal";
        }
        logHeader = false;
    }
    if (logRealTime) {
        if (logSumOfOps) {
            rtimes << "\t" << rta;
        }
        rtimes << endl;
    }
    if (logCpuTime) {
        if (logSumOfOps) {
            ctimes << "\t" << cta;
        }
        ctimes << endl;
    }
}

void
TwsDriver::runOperations(int nOps) {
    cout << endl
         << "running TWS operations ..." << "      [nOps=" << nOps << "]"
         << endl;

    if (doBulk) {
        if (doInsert) runInserts(BULK, nOps);
        if (doLookup) runLookups(BULK, nOps);
        if (doUpdate) runUpdates(BULK, nOps);
        if (doDelete) runDeletes(BULK, nOps);
    }
    if (doEach) {
        if (doInsert) runInserts(EACH, nOps);
        if (doLookup) runLookups(EACH, nOps);
        if (doUpdate) runUpdates(EACH, nOps);
        if (doDelete) runDeletes(EACH, nOps);
    }
    if (doIndy) {
        if (doInsert) runInserts(INDY, nOps);
        if (doLookup) runLookups(INDY, nOps);
        if (doUpdate) runUpdates(INDY, nOps);
        if (doDelete) runDeletes(INDY, nOps);
    }
}

void
TwsDriver::verify(int exp, int act) {
    if (doVerify) {
        //cout << "XXX exp=" << exp << ", act=" << act << endl;
        if (exp != act) {
            ABORT_VERIFICATION_ERROR();
        }
    }
}

void
TwsDriver::verify(long exp, long act) {
    if (doVerify) {
        //cout << "XXX exp=" << exp << ", act=" << act << endl;
        if (exp != act) {
            ABORT_VERIFICATION_ERROR();
        }
    }
}

void
TwsDriver::verify(long long exp, long long act) {
    if (doVerify) {
        //cout << "XXX exp=" << exp << ", act=" << act << endl;
        if (exp != act) {
            ABORT_VERIFICATION_ERROR();
        }
    }
}

void
TwsDriver::verify(const char* exp, const char* act) {
    if (doVerify) {
        //cout << "XXX exp='" << exp << "', act='" << act << "'" << endl;
        if (strcmp(exp, act) != 0) {
            ABORT_VERIFICATION_ERROR();
        }
    }
}

const char*
TwsDriver::toStr(XMode mode) {
    switch (mode) {
    case BULK:
        return "bulk";
    case EACH:
        return "each";
    case INDY:
        return "indy";
    default:
        assert(false);
        return "<invalid value>";
    };
}

const char*
TwsDriver::toStr(LockMode mode) {
    switch (mode) {
    case READ_COMMITTED:
        return "read_committed";
    case SHARED:
        return "shared";
    case EXCLUSIVE:
        return "exclusive";
    default:
        assert(false);
        return "<invalid value>";
    };
}

//---------------------------------------------------------------------------
