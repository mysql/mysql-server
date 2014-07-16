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

#ifndef Load_hpp
#define Load_hpp

#include <string>

using std::string;

class Load {
    Load(const Load&);
    Load& operator=(const Load&);

protected:
    // short name of load
    string name;

public:

    // usage
    Load(const string& name) : name(name) {}
    virtual ~Load() {};
    virtual string getName() { return name; }

    // intializers/finalizers
    virtual void init() = 0;
    virtual void close() = 0;

    // datastore operations
    virtual void initConnection() = 0;
    virtual void closeConnection() = 0;
    virtual void clearData() = 0;

    // benchmark operations
    virtual void runOperations(int nOps) = 0;
};

#endif // Load_hpp
