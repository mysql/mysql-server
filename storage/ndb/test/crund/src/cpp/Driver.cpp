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

#include "Driver.hpp"

#include <cassert>
#include <cstddef>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "helpers.hpp"
#include "string_helpers.hpp"

using std::cout;
using std::endl;
using std::flush;
using std::ios_base;
using std::ostringstream;
using std::string;
using std::vector;

// ----------------------------------------------------------------------
// usage
// ----------------------------------------------------------------------

vector<string> Driver::propFileNames;
string Driver::logFileName;

void Driver::exitUsage() {
  cout << "usage: [options]" << endl
       << "    [-p <file name>]...    properties file name" << endl
       << "    [-l <file name>]       log file name for results" << endl
       << "    [-h|--help]            print usage message and exit" << endl
       << endl;
  exit(1);  // return an error code
}

void Driver::parseArguments(int argc, const char *argv[]) {
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
    assert(nchars == size - 1);
    (void)nchars;

    logFileName += dest;
    logFileName += ".txt";
  }
  // cout << "logFileName='" << logFileName << "'" << endl;
}

void Driver::run() {
  init();
  runLoads();
  close();
}

// ----------------------------------------------------------------------
// initializers/finalizers
// ----------------------------------------------------------------------

void Driver::init() {
  cout.flags(ios_base::boolalpha);  // print booleans as strings
  loadProperties();
  initProperties();
  printProperties();
  openLogFile();
  clearLogBuffers();
  initLoads();
}

void Driver::close() {
  closeLoads();
  clearLogBuffers();
  closeLogFile();
  props.clear();
}

void Driver::loadProperties() {
  cout << endl;
  for (vector<string>::const_iterator i = propFileNames.begin();
       i != propFileNames.end(); ++i) {
    cout << "reading properties file:        " << *i << endl;
    props.load(i->c_str());
    // cout << "props = {" << endl << props << "}" << endl;
  }
}

void Driver::initProperties() {
  cout << endl << "reading driver properties ..." << flush;
  ostringstream msg;
  hasIgnoredSettings = false;

  nRuns = toI(props[L"nRuns"], 1, -1);
  if (nRuns < 0) {
    msg << "[IGNORED] nRuns:             '" << toS(props[L"nRuns"]) << "'"
        << endl;
    nRuns = 1;
  }

  logRealTime = toB(props[L"logRealTime"], true);
  logCpuTime = toB(props[L"logCpuTime"], false);
  logSumOfOps = toB(props[L"logSumOfOps"], true);
  failOnError = toB(props[L"failOnError"], true);

  const string prefix("Ndbapi");  // name prefix for native loads
  vector<string> ln;
  split(toS(props[L"loads"]), ',', std::back_inserter(ln));
  for (vector<string>::iterator i = ln.begin(); i != ln.end(); ++i) {
    const string &l = *i;
    if (l.compare(0, prefix.size(), prefix)) {
      msg << "[IGNORED] non-Ndbapi load:      '" << l << "'" << endl;
    } else {
      loadClassNames.push_back(l);
    }
  }

  if (!msg.tellp()) {  // or msg.str().empty() if ambiguous
    cout << "   [ok]" << endl;
  } else {
    setIgnoredSettings();
    cout << endl << msg.str() << flush;
  }
}

void Driver::printProperties() {
  cout << endl
       << "driver settings ..." << endl
       << "nRuns:                          " << nRuns << endl
       << "logRealTime:                    " << logRealTime << endl
       << "logCpuTime:                     " << logCpuTime << endl
       << "logSumOfOps:                    " << logSumOfOps << endl
       << "failOnError:                    " << failOnError << endl
       << "loadClassNames:                 " << toString(loadClassNames)
       << endl;
}

void Driver::openLogFile() {
  cout << endl << "opening results file:           " << logFileName << endl;
  log.open(logFileName.c_str(), ios_base::out | ios_base::trunc);
  assert(log.good());
}

void Driver::closeLogFile() {
  cout << endl << "closing files ..." << flush;
  log.close();
  cout << "               [ok]" << endl;
}

void Driver::initLoads() {
  if (loads.empty()) addLoads();

  if (loads.empty())
    cout << endl
         << "++++++++++  NOTHING TO TO, NO LOAD CLASSES GIVEN  ++++++++++"
         << endl;

  for (Loads::iterator i = loads.begin(); i != loads.end(); ++i) {
    Load *l = *i;
    assert(l);
    l->init();
  }
}

void Driver::closeLoads() {
  for (Loads::iterator i = loads.begin(); i != loads.end(); ++i) {
    Load *l = *i;
    assert(l);
    l->close();
  }
  loads.clear();
}

void Driver::addLoads() {
  vector<string> &ln = loadClassNames;
  for (vector<string>::iterator i = ln.begin(); i != ln.end(); ++i) {
    const string &name = *i;
    ostringstream msg;
    cout << endl << "instantiating load ..." << flush;

    if (!createLoad(name)) {
      msg << "[SKIPPING] unknown load:        '" << name << "'" << endl;
    }

    if (!msg.tellp()) {  // or msg.str().empty() if ambiguous
      cout << "          [ok: " << name << "]" << endl;
    } else {
      setIgnoredSettings();
      cout << endl << msg.str() << flush;
    }
  }
}

// ----------------------------------------------------------------------
// operations
// ----------------------------------------------------------------------

void Driver::runLoads() {
  if (hasIgnoredSettings) {
    cout << endl
         << "++++++++++++  SOME SETTINGS IGNORED, SEE ABOVE  ++++++++++++"
         << endl;
  }

  for (Loads::iterator i = loads.begin(); i != loads.end(); ++i) runLoad(**i);
}

void Driver::logWarning(const string &load, const string &msg) {
  cout << "!!! WARNINGS OCCURRED, SEE LOG FILE: " << logFileName << endl;
  errors << endl
         << "****************************************" << endl
         << "Warning in load: " << load << endl
         << msg << endl;
}

void Driver::logError(const string &load, const string &msg) {
  cout << "!!! ERRORS OCCURRED, SEE LOG FILE: " << logFileName << endl;
  errors << endl
         << "****************************************" << endl
         << "Error in load: " << load << endl
         << msg << endl;

  if (failOnError) abortIfErrors();
}

void Driver::abortIfErrors() {
  if (errors.tellp()) {
    log << "!!! ERRORS OCCURRED:" << endl << errors.str() << endl;
    log.close();
    cout << endl
         << "!!! Errors occurred, see log file: " << logFileName << endl
         << "!!! Aborting..." << endl
         << endl;
    exit(-1);
  }
}

void Driver::clearLogBuffers() {
  logHeader = true;
  header.rdbuf()->str("");
  rtimes.rdbuf()->str("");
  ctimes.rdbuf()->str("");
  errors.rdbuf()->str("");
}

void Driver::writeLogBuffers(const string &prefix) {
  if (logRealTime) {
    log << "rtime[ms]," << prefix << header.str() << endl
        << rtimes.str() << endl
        << endl;
  }
  if (logCpuTime) {
    log << "ctime[ms]," << prefix << header.str() << endl
        << ctimes.str() << endl
        << endl;
  }
  abortIfErrors();
  clearLogBuffers();
}

void Driver::beginOps(int nOps) {
  if (logRealTime) {
    rtimes << nOps;
    rta = 0;
  }
  if (logCpuTime) {
    ctimes << nOps;
    cta = 0;
  }
}

void Driver::finishOps(int nOps) {
  if (logSumOfOps) {
    cout << endl << "total" << endl;
    if (logRealTime) {
      cout << "tx real time                    " << rta << " ms " << endl;
    }
    if (logCpuTime) {
      cout << "tx cpu time                     " << cta << " ms " << endl;
    }
  }

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

void Driver::beginOp(const string &name) {
  cout << endl << name << endl;

  if (logRealTime && logCpuTime) {
    s0 = hrt_tnow(&t0);
  } else if (logRealTime) {
    s0 = hrt_rtnow(&t0.rtstamp);
  } else if (logCpuTime) {
    s0 = hrt_ctnow(&t0.ctstamp);
  }
}

void Driver::finishOp(const string &name, int nOps) {
  if (logRealTime && logCpuTime) {
    s1 = hrt_tnow(&t1);
  } else if (logRealTime) {
    s1 = hrt_rtnow(&t1.rtstamp);
  } else if (logCpuTime) {
    s1 = hrt_ctnow(&t1.ctstamp);
  }

  if (logRealTime) {
    if (s0 | s1) {
      cout << "ERROR: failed to get the system's real time.";
      rtimes << "\tERROR";
    } else {
      const long t = long(hrt_rtmicros(&t1.rtstamp, &t0.rtstamp) / 1000);
      const long ops = (t > 0 ? (nOps * 1000) / t : 0);
      cout << "tx real time:                   " << t << "\tms\t" << ops
           << " ops/s" << endl;
      rtimes << "\t" << t;
      rta += t;
    }
  }

  if (logCpuTime) {
    if (s0 | s1) {
      cout << "ERROR: failed to get this process's cpu time.";
      ctimes << "\tERROR";
    } else {
      const long t = long(hrt_ctmicros(&t1.ctstamp, &t0.ctstamp) / 1000);
      const long ops = (t > 0 ? (nOps * 1000) / t : 0);
      cout << "tx cpu time:                    " << t << "\tms\t" << ops
           << " ops/s" << endl;
      ctimes << "\t" << t;
      cta += t;
    }
  }

  if (logHeader) header << "\t" << name;
}
