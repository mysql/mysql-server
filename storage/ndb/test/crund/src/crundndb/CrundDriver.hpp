/* -*- mode: java; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=4:tabstop=4:smarttab:
 *
 *  Copyright (C) 2009 MySQL
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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
    bool renewOperations;
    bool logSumOfOps;
    //bool allowExtendedPC; // not used
    int aStart;
    int bStart;
    int aEnd;
    int bEnd;
    int aScale;
    int bScale;
    int maxVarbinaryBytes;
    int maxVarcharChars;
    int maxBlobBytes;
    int maxTextChars;
    set< string > exclude;

    // benchmark intializers/finalizers
    virtual void initProperties();
    virtual void printProperties();

    // a database operation to be benchmarked
    struct Op {
        const string name;

        virtual void run(int countA, int countB) const = 0;

        Op(const string& name) : name(name) {}

        virtual ~Op() {}
    };

    // the list of database operations to be benchmarked
    typedef vector< const Op* > Operations;
    Operations operations;

    // benchmark operations
    virtual void initOperations() = 0;
    virtual void closeOperations() = 0;
    virtual void runTests();
    virtual void runOperations(int countA, int countB);
    virtual void runOp(const Op& op, int countA, int countB);

    // reports an error if a condition is not met // XXX not covered yet
    //static void verify(bool cond);
};

#endif // CrundDriver_hpp
