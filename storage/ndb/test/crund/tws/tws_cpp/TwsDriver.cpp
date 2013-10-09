/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=4:tabstop=4:smarttab:
 *
 *  Copyright (c) 2010, 2012, Oracle and/or its affiliates. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
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

#include "TwsDriver.hpp"

#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <cassert>

#include "helpers.hpp"
#include "string_helpers.hpp"
#include "Properties.hpp"

using std::cout;
using std::flush;
using std::endl;
using std::ios_base;
using std::string;
using std::ostringstream;

using utils::Properties;
using utils::toBool;
using utils::toInt;
using utils::toString;

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
TwsDriver::init() {
    Driver::init();
}

void
TwsDriver::close() {
    Driver::close();
}

void
TwsDriver::initProperties() {
    Driver::initProperties();

    cout << "setting tws properties ..." << flush;

    ostringstream msg;

    renewConnection = toBool(props[L"renewConnection"], false);
    doInsert = toBool(props[L"doInsert"], true);
    doLookup = toBool(props[L"doLookup"], true);
    doUpdate = toBool(props[L"doUpdate"], true);
    doDelete = toBool(props[L"doDelete"], true);
    doSingle = toBool(props[L"doSingle"], true);
    doBulk = toBool(props[L"doBulk"], true);
    doBatch = toBool(props[L"doBatch"], true);
    doVerify = toBool(props[L"doVerify"], true);

    string lm = toString(props[L"lockMode"]);
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

    nRows = toInt(props[L"nRows"], 256, 0);
    if (nRows < 1) {
        msg << "[ignored] nRows:            '"
            << toString(props[L"nRows"]) << "'" << endl;
        nRows = 256;
    }

    nRuns = toInt(props[L"nRuns"], 1, -1);
    if (nRuns < 0) {
        msg << "[ignored] nRuns:             '"
            << toString(props[L"nRuns"]) << "'" << endl;
        nRuns = 1;
    }

    //if (msg.tellp() == 0) // netbeans reports amibuities
    if (msg.str().empty()) {
        cout << "      [ok]" << endl;
    } else {
        cout << endl << msg.str() << endl;
    }
}

void
TwsDriver::printProperties() {
    Driver::printProperties();

    const ios_base::fmtflags f = cout.flags();
    // no effect calling manipulator function, not sure why
    //cout << ios_base::boolalpha;
    cout.flags(ios_base::boolalpha);

    cout << endl << "tws settings..." << endl;
    cout << "renewConnection:                " << renewConnection << endl;
    cout << "doInsert:                       " << doInsert << endl;
    cout << "doLookup:                       " << doLookup << endl;
    cout << "doUpdate:                       " << doUpdate << endl;
    cout << "doDelete:                       " << doDelete << endl;
    cout << "doSingle:                       " << doSingle << endl;
    cout << "doBulk:                         " << doBulk << endl;
    cout << "doBatch:                        " << doBatch << endl;
    cout << "doVerify:                       " << doVerify << endl;
    cout << "lockMode:                       " << toStr(lockMode) << endl;
    cout << "nRows:                          " << nRows << endl;
    cout << "nRuns:                          " << nRuns << endl;

    cout.flags(f);
}

// ----------------------------------------------------------------------

void
TwsDriver::runTests() {
    //initConnection();

    //assert(rStart <= rEnd && rScale > 1);
    //for (int i = rStart; i <= rEnd; i *= rScale)
    runLoads();

    //closeConnection();
}

void
TwsDriver::runLoads() {
    // anticipating multiple loads to be run here
    runSeries();
}

void
TwsDriver::runSeries() {
    if (nRuns == 0)
        return; // nothing to do

    cout << endl
         << "------------------------------------------------------------" << endl;
    cout << "running " << nRuns << " iterations on load: " << descr;

    for (int i = 0; i < nRuns; i++) {
        cout << endl
             << "------------------------------------------------------------" << endl;
        runOperations();
    }

    writeLogBuffers();
    clearLogBuffers();
}

void
TwsDriver::runOperations() {
    // log buffers
    rtimes << "nRows=" << nRows;
    rta = 0L;

    // pre-run cleanup
    if (renewConnection) {
        closeConnection();
        initConnection();
    }
    //clearData(); // not used

    runLoadOperations();

    cout << endl
         << "total" << endl;
    cout << "tx real time                    " << rta
         << "\tms" << endl;

    // log buffers
    if (logHeader) {
        header << "\ttotal";
        logHeader = false;
    }
    rtimes << "\t" << rta << endl;
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
    case SINGLE:
        return "single";
    case BULK:
        return "bulk";
    case BATCH:
        return "batch";
    default:
        assert(false);
        return "<invalid value>";
    };
}

const char*
TwsDriver::toStr(LockMode mode) {
    switch (mode) {
    case SINGLE:
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
