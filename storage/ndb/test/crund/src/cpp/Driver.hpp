/*
  Copyright (c) 2010, 2013, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef Driver_hpp
#define Driver_hpp

#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <vector>

#include "Properties.hpp"
#include "hrt_utils.h"
#include "Load.hpp"

using std::ofstream;
using std::ostringstream;
using std::string;
using std::vector;

using utils::Properties;

class Driver {
    Driver(const Driver&);
    Driver& operator=(const Driver&);

public:

    // usage
    static void parseArguments(int argc, const char* argv[]);
    Driver() {}
    virtual ~Driver() {}
    virtual void run();

    // settings
    int nRuns;
    bool logRealTime;
    bool logCpuTime;
    bool logSumOfOps;
    bool failOnError;
    vector< string > loadClassNames;

    // resources
    virtual Properties& getProperties() { return props; }
    virtual void setIgnoredSettings() { hasIgnoredSettings = true; };
    virtual void addLoad(Load& load) { loads.push_back(&load); }

    // operations
    virtual void logError(const string& load, const string& msg);
    virtual void logWarning(const string& load, const string& msg);
    virtual void beginOp(const string& name);
    virtual void finishOp(const string& name, int nOps);

protected:

    // usage
    static vector< string > propFileNames;
    static string logFileName;
    static void exitUsage();

    // resources
    Properties props;
    bool hasIgnoredSettings;
    ofstream log;
    string descr;
    bool logHeader;
    ostringstream header;
    ostringstream rtimes;
    ostringstream ctimes;
    ostringstream errors;
    int s0, s1;
    hrt_tstamp t0, t1;
    long rta, cta;
    typedef vector< Load * > Loads;
    Loads loads;

    // intializers/finalizers
    virtual void init();
    virtual void close();
    virtual void loadProperties();
    virtual void initProperties();
    virtual void printProperties();
    virtual void openLogFile();
    virtual void closeLogFile();
    virtual void initLoads();
    virtual void closeLoads();
    virtual void addLoads();
    virtual bool createLoad(const string& name) = 0;

    // operations
    virtual void runLoad(Load& load) = 0;
    virtual void runLoads();
    virtual void abortIfErrors();
    virtual void clearLogBuffers();
    virtual void writeLogBuffers(const string& prefix);
    virtual void beginOps(int nOps);
    virtual void finishOps(int nOps);
};

#endif // Driver_hpp
