/* -*- mode: java; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=4:tabstop=4:smarttab:
 *
 *  Copyright (c) 2010, 2011, Oracle and/or its affiliates. All rights reserved.
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
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef NdbApiDriver_hpp
#define NdbApiDriver_hpp

#include <string>

#include "CrundDriver.hpp"
#include "CrundNdbApiOperations.hpp"

using std::string;

// global type aliases
typedef const NdbDictionary::Table* NdbTable;

class NdbApiDriver : public CrundDriver {
public:

    // the generated features are OK
    //NdbApiDriver() {}
    //virtual ~NdbApsDriver() {}
    //NdbApiDriver(const NdbApiDriver&) {}
    //NdbApiDriver& operator=(const NdbApiDriver&) {}

protected:

    // NDB API resources
    string mgmdConnect;
    string catalog;
    string schema;

    // the benchmark's basic database operations
    static CrundNdbApiOperations* ops;

    // NDB API intializers/finalizers
    virtual void initProperties();
    virtual void printProperties();
    virtual void init();
    virtual void close();

    // NDB API operations
    template< bool feat > void initOperationsFeat();
    template< bool > struct ADelAllOp;
    template< bool > struct B0DelAllOp;
    template< bool, bool > struct AInsOp;
    template< bool, bool > struct B0InsOp;
    template< const char**,
              void (CrundNdbApiOperations::*)(NdbTable,int,int,bool),
              bool >
    struct AByPKOp;
    template< const char**,
              void (CrundNdbApiOperations::*)(NdbTable,int,int,bool),
              bool >
    struct B0ByPKOp;
    template< const char**,
              void (CrundNdbApiOperations::*)(NdbTable,int,int,bool,int),
              bool >
    struct LengthOp;
    template< const char**,
              void (CrundNdbApiOperations::*)(NdbTable,int,int,bool,int),
              bool >
    struct ZeroLengthOp;
    template< const char**,
              void (CrundNdbApiOperations::*)(int,bool),
              bool >
    struct RelOp;
    virtual void initOperations();
    virtual void closeOperations();

    // NDB API datastore operations
    virtual void initConnection();
    virtual void closeConnection();
    virtual void clearPersistenceContext();
    virtual void clearData();
};

#endif // Driver_hpp
