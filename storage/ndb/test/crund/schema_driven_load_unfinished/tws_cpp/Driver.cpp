/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=4:tabstop=4:smarttab:
 *
 *  Copyright (c) 2010, 2023, Oracle and/or its affiliates.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License, version 2.0,
 *  as published by the Free Software Foundation.
 *
 *  This program is also distributed with certain software (including
 *  but not limited to OpenSSL) that is licensed under separate terms,
 *  as designated in a particular file or component or in included license
 *  documentation.  The authors of MySQL hereby grant you an additional
 *  permission to link the program and your derivative works with the
 *  separately licensed software that they have included with MySQL.
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

#include "Driver.hpp"

#include <cassert>

#include "helpers.hpp"
#include "string_helpers.hpp"

using std::cout;
using std::flush;
using std::endl;
using std::ios_base;
using std::string;
using std::vector;

using utils::Properties;
using utils::toBool;
using utils::toInt;
using utils::toString;

// ---------------------------------------------------------------------------
// Helper Macros & Functions
// ---------------------------------------------------------------------------

#define ABORT_GETTIMEOFDAY_ERROR()                                      \
    do { cout << "!!! error in " << __FILE__ << ", line: " << __LINE__  \
              << ", gettimeofday() returned an error code." << endl;    \
        exit(-1);                                                       \
    } while (0)

// ---------------------------------------------------------------------------
// Driver Implementation
// ---------------------------------------------------------------------------

vector< string > Driver::propFileNames;
string Driver::logFileName;

void
Driver::exitUsage()
{
    cout << "usage: [options]" << endl
         << "    [-p <file name>]...    properties file name" << endl
         << "    [-l <file name>]       log file name for data output" << endl
         << "    [-h|--help]            print usage message and exit" << endl
         << endl;
    exit(1); // return an error code
}

void
Driver::parseArguments(int argc, const char* argv[])
{
    for (int i = 1; i < argc; i++) {
        const string arg = argv[i];
        if (arg.compare("-p") == 0) {
            if (i >= argc) {
                exitUsage();
            }
            propFileNames.push_back(argv[++i]);
        } else if (arg.compare("-l") == 0) {
            if (i >= argc) {
                exitUsage();
            }
            logFileName = argv[++i];
        } else if (arg.compare("-h") == 0 || arg.compare("--help") == 0) {
            exitUsage();
        } else {
            cout << "unknown option: " << arg << endl;
            exitUsage();
        }
    }

    if (propFileNames.size() == 0) {
        propFileNames.push_back("run.properties");
    }

    if (logFileName.empty()) {
        logFileName = "log_";

        // format, destination strings (static size)
        const char format[] = "%Y%m%d_%H%M%S";
        const int size = sizeof("yyyymmdd_HHMMSS");
        char dest[size];

        // get time, convert to timeinfo (statically allocated) then to string
        const time_t now = time(0);
        const int nchars = strftime(dest, size, format, localtime(&now));
        assert(nchars == size-1);
        (void)nchars;

        logFileName += dest;
        logFileName += ".txt";
    }
    //cout << "logFileName='" << logFileName << "'" << endl;
}

// ----------------------------------------------------------------------

void
Driver::run() {
    init();

    if (warmupRuns > 0) {
        cout << endl
             << "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++" << endl
             << "warmup runs ..." << endl
             << "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++" << endl;

        for (int i = 0; i < warmupRuns; i++) {
            runTests();
        }

        // truncate log file, reset log buffers
        closeLogFile();
        openLogFile();
        clearLogBuffers();
    }

    cout << endl
         << "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++" << endl
         << "hot runs ..." << endl
         << "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++" << endl;
    runTests();

    cout << endl
         << "++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++" << endl;
    close();
}

void
Driver::init() {
    loadProperties();
    initProperties();
    printProperties();
    openLogFile();
    clearLogBuffers();
}

void
Driver::close() {
    clearLogBuffers();
    closeLogFile();
}

void
Driver::loadProperties() {
    cout << endl;
    for (vector<string>::const_iterator i = propFileNames.begin();
         i != propFileNames.end(); ++i) {
        cout << "reading properties file:        " << *i << endl;
        props.load(i->c_str());
        //cout << "props = {" << endl << props << "}" << endl;
    }
}

void
Driver::initProperties() {
    cout << "setting driver properties ..." << flush;

    ostringstream msg;

    warmupRuns = toInt(props[L"warmupRuns"], 0, -1);
    if (warmupRuns < 0) {
        msg << "[ignored] warmupRuns:        '"
            << toString(props[L"warmupRuns"]) << "'" << endl;
        warmupRuns = 0;
    }

    //if (msg.tellp() == 0) // netbeans reports amibuities
    if (msg.str().empty()) {
        cout << "   [ok]" << endl;
    } else {
        cout << endl << msg.str() << endl;
    }
}

void
Driver::printProperties() {
    const ios_base::fmtflags f = cout.flags();
    // no effect calling manipulator function, not sure why
    //cout << ios_base::boolalpha;
    cout.flags(ios_base::boolalpha);

    cout << endl << "driver settings ..." << endl;
    cout << "warmupRuns:                     " << warmupRuns << endl;

    cout.flags(f);
}

void
Driver::openLogFile() {
    cout << endl
         << "opening results file:" << flush;
    log.open(logFileName.c_str(), ios_base::out | ios_base::trunc);
    assert(log.good());
    cout << "           [ok: " << logFileName << "]" << endl;
}

void
Driver::closeLogFile() {
    cout << endl
         << "closing results file:" << flush;
    log.close();
    cout << "           [ok: " << logFileName << "]" << endl;
}

// ---------------------------------------------------------------------------

void
Driver::clearLogBuffers() {
    logHeader = true;
    header.rdbuf()->str("");
    rtimes.rdbuf()->str("");
}

void
Driver::writeLogBuffers() {
    log << descr << ", rtime[ms]"
        << header.rdbuf()->str() << endl
        << rtimes.rdbuf()->str() << endl;
}

void
Driver::begin(const string& name) {
    cout << endl;
    cout << name << endl;

    if (gettimeofday(&t0, NULL) != 0)
        ABORT_GETTIMEOFDAY_ERROR();
}

void
Driver::finish(const string& name) {
    if (gettimeofday(&t1, NULL) != 0)
        ABORT_GETTIMEOFDAY_ERROR();

    const long r_usec = (((t1.tv_sec - t0.tv_sec) * 1000000)
                         + (t1.tv_usec - t0.tv_usec));
    const long r_msec = r_usec / 1000;

    cout << "tx real time:                   " << r_msec
         << "\tms" << endl;
    rtimes << "\t" << r_msec;
    rta += r_msec;

    if (logHeader)
        header << "\t" << name;
}

//---------------------------------------------------------------------------
