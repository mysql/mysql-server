/* -*- mode: java; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=4:tabstop=4:smarttab:
 *
 *  Copyright (c) 2010, 2011, Oracle and/or its affiliates. All rights reserved.
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

#include <iostream>
#include <sstream>
#include <string>
#include <cassert>

#include "helpers.hpp"
#include "string_helpers.hpp"

#include "CrundDriver.hpp"

using std::cout;
using std::flush;
using std::endl;
using std::ios_base;
using std::ostringstream;
using std::string;
using std::wstring;

using utils::toBool;
using utils::toInt;
using utils::toString;

// ----------------------------------------------------------------------

void
CrundDriver::init() {
    Driver::init();
    // do work here
}

void
CrundDriver::close() {
    // do work here
    Driver::close();
}

void
CrundDriver::initProperties() {
    Driver::initProperties();

    cout << "setting crund properties ..." << flush;

    ostringstream msg;

    renewConnection = toBool(props[L"renewConnection"], false);
    renewOperations = toBool(props[L"renewOperations"], false);

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

    logSumOfOps = toBool(props[L"logSumOfOps"], true);
    //allowExtendedPC = toBool(props[L"allowExtendedPC"], false); // not used

    nOpsStart = toInt(props[L"nOpsStart"], 256, 0);
    if (nOpsStart < 1) {
        msg << "[ignored] nOpsStart:            '"
            << toString(props[L"nOpsStart"]) << "'" << endl;
        nOpsStart = 256;
    }
    nOpsEnd = toInt(props[L"nOpsEnd"], nOpsStart, 0);
    if (nOpsEnd < nOpsStart) {
        msg << "[ignored] nOpsEnd:              '"
            << toString(props[L"nOpsEnd"]) << "'" << endl;
        nOpsEnd = nOpsStart;
    }
    nOpsScale = toInt(props[L"nOpsScale"], 2, 0);
    if (nOpsScale < 2) {
        msg << "[ignored] nOpsScale:            '"
            << toString(props[L"nOpsScale"]) << "'" << endl;
        nOpsScale = 2;
    }

    maxVarbinaryBytes = toInt(props[L"maxVarbinaryBytes"], 100, 0);
    if (maxVarbinaryBytes < 1) {
        msg << "[ignored] maxVarbinaryBytes:    '"
            << toString(props[L"maxVarbinaryBytes"]) << "'" << endl;
        maxVarbinaryBytes = 100;
    }
    maxVarcharChars = toInt(props[L"maxVarcharChars"], 100, 0);
    if (maxVarcharChars < 1) {
        msg << "[ignored] maxVarcharChars:      '"
            << toString(props[L"maxVarcharChars"]) << "'" << endl;
        maxVarcharChars = 100;
    }

    maxBlobBytes = toInt(props[L"maxBlobBytes"], 1000, 0);
    if (maxBlobBytes < 1) {
        msg << "[ignored] maxBlobBytes:         '"
            << toString(props[L"maxBlobBytes"]) << "'" << endl;
        maxBlobBytes = 1000;
    }
    maxTextChars = toInt(props[L"maxTextChars"], 1000, 0);
    if (maxTextChars < 1) {
        msg << "[ignored] maxTextChars:         '"
            << toString(props[L"maxTextChars"]) << "'" << endl;
        maxTextChars = 1000;
    }

    // initialize exclude set
    const wstring& estr = props[L"exclude"];
    //cout << "estr='" << toString(estr) << "'" << endl;
    const size_t len = estr.length();
    size_t beg = 0, next;
    while (beg < len
           && ((next = estr.find_first_of(L",", beg)) != wstring::npos)) {
        // add substring if not empty
        if (beg < next) {
            const wstring& s = estr.substr(beg, next - beg);
            exclude.insert(toString(s));
        }
        beg = next + 1;
    }
    // add last substring if any
    if (beg < len) {
        const wstring& s = estr.substr(beg, len - beg);
        exclude.insert(toString(s));
    }

    if (!msg.tellp()) {
        cout << "    [ok: "
             << "nOps=" << nOpsStart << ".." << nOpsEnd << "]" << endl;
    } else {
        cout << endl << msg.str() << endl;
    }
}

void
CrundDriver::printProperties() {
    Driver::printProperties();

    const ios_base::fmtflags f = cout.flags();
    // no effect calling manipulator function, not sure why
    //cout << ios_base::boolalpha;
    cout.flags(ios_base::boolalpha);

    cout << endl << "crund settings ..." << endl;
    cout << "renewConnection:                " << renewConnection << endl;
    cout << "renewOperations:                " << renewOperations << endl;
    cout << "lockMode:                       " << toStr(lockMode) << endl;
    cout << "logSumOfOps:                    " << logSumOfOps << endl;
    //cout << "allowExtendedPC:                " << allowExtendedPC << endl;
    cout << "nOpsStart:                      " << nOpsStart << endl;
    cout << "nOpsEnd:                        " << nOpsEnd << endl;
    cout << "nOpsScale:                      " << nOpsScale << endl;
    cout << "maxVarbinaryBytes:              " << maxVarbinaryBytes << endl;
    cout << "maxVarcharChars:                " << maxVarcharChars << endl;
    cout << "maxBlobBytes:                   " << maxBlobBytes << endl;
    cout << "maxTextChars:                   " << maxTextChars << endl;
    cout << "exclude:                        " << toString(exclude) << endl;

    cout.flags(f);
}

// ----------------------------------------------------------------------

void
CrundDriver::runTests() {
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
CrundDriver::runLoads(int nOps) {
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
CrundDriver::runOperations(int nOps) {
    for (Operations::const_iterator i = operations.begin();
         i != operations.end(); ++i) {
        // no need for pre-tx cleanup with NDBAPI-based loads
        //if (!allowExtendedPC) {
        //    // effectively prevent caching beyond Tx scope by clearing
        //    // any data/result caches before the next transaction
        //    clearPersistenceContext();
        //}
        runOp(**i, nOps);
    }
}

void
CrundDriver::runOp(const Op& op, int nOps) {
    const string& name = op.name;
    if (exclude.find(name) == exclude.end()) {
        begin(name);
        op.run(nOps);
        commit(name);
    }
}

const char*
CrundDriver::toStr(XMode mode) {
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
CrundDriver::toStr(LockMode mode) {
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
