/* -*- mode: c++; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=4:tabstop=4:smarttab:
 *
 *  Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.
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

#ifndef TwsDriver_hpp
#define TwsDriver_hpp

#include "Driver.hpp"

class TwsDriver : public Driver {
protected:

    // benchmark settings
    enum LockMode { READ_COMMITTED, SHARED, EXCLUSIVE };
    static const char* toStr(LockMode mode);
    enum XMode { SINGLE, BULK, BATCH };
    static const char* toStr(XMode mode);
    bool renewConnection;
    bool doInsert;
    bool doLookup;
    bool doUpdate;
    bool doDelete;
    bool doSingle;
    bool doBulk;
    bool doBatch;
    bool doVerify;
    LockMode lockMode;
    int nRows;
    int nRuns;

    // benchmark intializers/finalizers
    virtual void init();
    virtual void close();
    virtual void initProperties();
    virtual void printProperties();

    // benchmark operations
    virtual void runTests();
    virtual void runLoads();
    virtual void runSeries();
    virtual void runOperations();
    virtual void runLoadOperations() = 0;
    void verify(int exp, int act);
    void verify(long exp, long act);
    void verify(long long exp, long long act);
    void verify(const char* exp, const char* act);

    // datastore operations
    virtual void initConnection() = 0;
    virtual void closeConnection() = 0;
    //virtual void clearPersistenceContext() = 0; // not used
    //virtual void clearData() = 0; // not used
};

#endif // TwsDriver_hpp
