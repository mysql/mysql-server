/* -*- mode: java; c-basic-offset: 4; indent-tabs-mode: nil; -*-
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

#ifndef Driver_hpp
#define Driver_hpp

#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <vector>

#include "Properties.hpp"
#include "hrt_utils.h"

using std::ofstream;
using std::ostringstream;
using std::string;
using std::vector;

using utils::Properties;

class Driver {
public:

    /**
     * Parses the benchmark's command-line arguments.
     */
    static void parseArguments(int argc, const char* argv[]);

    /**
     * Creates an instance.
     */
    Driver() {}

    /**
     * Deletes an instance.
     */
    virtual ~Driver() {}

    /**
     * Runs the benchmark.
     */
    void run();

protected:

    // command-line arguments
    static vector< string > propFileNames;
    static string logFileName;

    static void exitUsage();

    // driver settings
    Properties props;
    bool logRealTime;
    bool logCpuTime;
    int nRuns;

    // driver resources
    ofstream log;
    string descr;
    bool logHeader;
    ostringstream header;
    ostringstream rtimes;
    ostringstream ctimes;
    int s0, s1;
    hrt_tstamp t0, t1;
    long rta, cta;

    // driver intializers/finalizers
    virtual void init();
    virtual void close();
    virtual void loadProperties();
    virtual void initProperties();
    virtual void printProperties();
    virtual void openLogFile();
    virtual void closeLogFile();

    // benchmark operations
    virtual void runTests() = 0;
    virtual void begin(const string& name);
    virtual void commit(const string& name);
};

#endif // Driver_hpp
