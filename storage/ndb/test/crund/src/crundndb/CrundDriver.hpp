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

#ifndef CrundDriver_hpp
#define CrundDriver_hpp

#include <string>
#include <vector>
#include <set>

#include "hrt_utils.h"

#include "Driver.hpp"

using std::string;
using std::vector;
using std::set;

class CrundDriver : public Driver {
protected:

    // benchmark settings
    enum LockMode { READ_COMMITTED, SHARED, EXCLUSIVE };
    static const char* toStr(LockMode mode);
    enum XMode { SINGLE, BULK, BATCH }; // XXX not used yet
    static const char* toStr(XMode mode); // XXX not used yet
    bool renewConnection;
    bool renewOperations;
    LockMode lockMode;
    bool logSumOfOps;
    //bool allowExtendedPC; // not used
    int nOpsStart;
    int nOpsEnd;
    int nOpsScale;
    int maxVarbinaryBytes;
    int maxVarcharChars;
    int maxBlobBytes;
    int maxTextChars;
    set< string > exclude;

    // benchmark intializers/finalizers
    virtual void init();
    virtual void close();
    virtual void initProperties();
    virtual void printProperties();

    // measured units of work
    struct Op {
        const string name;

        virtual void run(int nOps) const = 0;

        Op(const string& name) : name(name) {}

        virtual ~Op() {}
    };
    typedef vector< const Op* > Operations;
    Operations operations;

    // benchmark operations
    virtual void initOperations() = 0;
    virtual void closeOperations() = 0;
    virtual void runTests();
    virtual void runLoads(int nOps);
    virtual void runOperations(int nOps);
    virtual void runOp(const Op& op, int nOps);

    // datastore operations
    virtual void initConnection() = 0;
    virtual void closeConnection() = 0;
    //virtual void clearPersistenceContext() = 0; // not used
    virtual void clearData() = 0;
};

#endif // CrundDriver_hpp
