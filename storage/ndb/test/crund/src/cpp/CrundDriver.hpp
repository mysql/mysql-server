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

#ifndef CrundDriver_hpp
#define CrundDriver_hpp

#include <string>
#include <vector>
#include <algorithm>
#include <ostream>

#include "Driver.hpp"

using std::string;
using std::vector;
using std::ostream;

class CrundDriver : public Driver {
public:

    // usage
    CrundDriver() {}
    virtual ~CrundDriver() {}

    // operation execution and lock modes
    struct XMode {
        enum E { undef = 0x0, indy = 0x1, each = 0x2, bulk = 0x4 };

        static E valueOf(const string & s);
        static const char* toString(E e);
        friend ostream& operator<< (ostream& os, const E& rhs);
    };
    struct LockMode {
        enum E { undef = 0, none, shared, exclusive };

        static E valueOf(const string & s);
        static const char* toString(E e);
        friend ostream& operator<< (ostream& os, const E& rhs);
    };

    // settings
    vector< XMode::E > xModes;
    LockMode::E lockMode;
    bool renewConnection;
    int nOpsStart;
    int nOpsEnd;
    int nOpsScale;
    int maxVarbinaryBytes;
    int maxVarcharChars;
    int maxBlobBytes;
    int maxTextChars;
    vector< string > include;
    vector< string > exclude;

protected:

    // resources
    Loads myLoads;

    // intializers/finalizers
    virtual void init();
    virtual void close();
    virtual void initProperties();
    virtual void printProperties();
    virtual bool createLoad(const string& name);

    // operations
    virtual void runLoad(Load& load);
    virtual void connectDB(Load& load);
    virtual void disconnectDB(Load& load);
    virtual void reconnectDB(Load& load);
    virtual void runSeries(Load& load, int nOps);
    virtual void runOperations(Load& load, int nOps);
};

// string converters and ostream inserters for enum types

inline ostream&
operator<< (ostream& os, const CrundDriver::XMode::E& rhs) {
    os << CrundDriver::XMode::toString(rhs);
    return os;
}

inline ostream&
operator<< (ostream& os, const CrundDriver::LockMode::E& rhs) {
    os << CrundDriver::LockMode::toString(rhs);
    return os;
}

inline CrundDriver::XMode::E
CrundDriver::XMode::valueOf(const string & s) {
    string l(s);
    std::transform(s.begin(), s.end(), l.begin(), ::tolower);
    if (!l.compare("indy")) return CrundDriver::XMode::indy;
    if (!l.compare("each")) return CrundDriver::XMode::each;
    if (!l.compare("bulk")) return CrundDriver::XMode::bulk;
    return undef;
}

inline const char*
CrundDriver::XMode::toString(CrundDriver::XMode::E e) {
    switch (e) {
    case CrundDriver::XMode::indy: return "indy";
    case CrundDriver::XMode::each: return "each";
    case CrundDriver::XMode::bulk: return "bulk";
    default: return "<undef>";
    };
}

inline CrundDriver::LockMode::E
CrundDriver::LockMode::valueOf(const string & s) {
    string l(s);
    std::transform(s.begin(), s.end(), l.begin(), ::tolower);
    if (!l.compare("none")) return CrundDriver::LockMode::none;
    if (!l.compare("shared")) return CrundDriver::LockMode::shared;
    if (!l.compare("exclusive")) return CrundDriver::LockMode::exclusive;
    return undef;
}

inline const char*
CrundDriver::LockMode::toString(CrundDriver::LockMode::E e) {
    switch (e) {
    case CrundDriver::LockMode::none: return "none";
    case CrundDriver::LockMode::shared: return "shared";
    case CrundDriver::LockMode::exclusive: return "exclusive";
    default: return "<undef>";
    };
}

#endif // CrundDriver_hpp
