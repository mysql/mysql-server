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

#include "NdbapiDriver.hpp"
#include "CrundNdbapiOperations.hpp"

#include <iostream>
#include <string>
#include <cassert>

#include "helpers.hpp"
#include "string_helpers.hpp"

using std::cout;
using std::flush;
using std::endl;
using std::ios_base;
using std::ostringstream;
using std::string;

using utils::toString;

// ----------------------------------------------------------------------

CrundNdbapiOperations* NdbapiDriver::ops = NULL;

// ISO C++ 98 does not allow for a string literal as a template argument
// for a non-type template parameter, because string literals are objects
// with internal linkage.  This restriction maybe lifted in C++0x.
//
// Until then, we have to allocate the operation names as variables
// (which are external at file scope by default).
const char* delAByPK_s = "delAByPK";
const char* delB0ByPK_s = "delB0ByPK";
const char* setAByPK_s = "setAByPK";
const char* setB0ByPK_s = "setB0ByPK";
const char* getAByPK_bb_s = "getAByPK_bb";
const char* getB0ByPK_bb_s = "getB0ByPK_bb";
const char* getAByPK_ar_s = "getAByPK_ar";
const char* getB0ByPK_ar_s = "getB0ByPK_ar";

const char* setVarbinary_s = "setVarbinary";
const char* getVarbinary_s = "getVarbinary";
const char* clearVarbinary_s = "clearVarbinary";
const char* setVarchar_s = "setVarchar";
const char* getVarchar_s = "getVarchar";
const char* clearVarchar_s = "clearVarchar";

const char* setB0ToA_s = "setB0->A";
const char* navB0ToA_s = "navB0->A";
const char* navB0ToAalt_s = "navB0->A_alt";
const char* navAToB0_s = "navA->B0";
const char* navAToB0alt_s = "navA->B0_alt";
const char* nullB0ToA_s = "nullB0->A";

//---------------------------------------------------------------------------

void
NdbapiDriver::init() {
    CrundDriver::init();

    // initialize the benchmark's resources
    ops = new CrundNdbapiOperations();
    assert(!mgmdConnect.empty());
    ops->init(mgmdConnect.c_str());
}

void
NdbapiDriver::close() {
    // release the benchmark's resources
    assert(ops);
    ops->close();
    delete ops;
    ops = NULL;

    CrundDriver::close();
}

void
NdbapiDriver::initProperties() {
    CrundDriver::initProperties();

    cout << "setting ndb properties ..." << flush;

    ostringstream msg;

    mgmdConnect = toString(props[L"ndb.mgmdConnect"]);
    if (mgmdConnect.empty()) {
        mgmdConnect = string("localhost");
    }

    catalog = toString(props[L"ndb.catalog"]);
    if (catalog.empty()) {
        catalog = string("crunddb");
    }

    schema = toString(props[L"ndb.schema"]);
    if (schema.empty()) {
        schema = string("def");
    }

    //if (msg.tellp() == 0) {
    if (msg.str().empty()) {
        cout << "      [ok]" << endl;
    } else {
        cout << endl << msg.str() << endl;
    }

    descr = "ndbapi(" + mgmdConnect + ")";
}

void
NdbapiDriver::printProperties() {
    CrundDriver::printProperties();

    const ios_base::fmtflags f = cout.flags();
    // no effect calling manipulator function, not sure why
    //cout << ios_base::boolalpha;
    cout.flags(ios_base::boolalpha);

    cout << endl << "ndb settings ..." << endl;
    cout << "ndb.mgmdConnect:                \"" << mgmdConnect << "\"" << endl;
    cout << "ndb.catalog:                    \"" << catalog << "\"" << endl;
    cout << "ndb.schema:                     \"" << schema << "\"" << endl;

    cout.flags(f);
}

//---------------------------------------------------------------------------

void
NdbapiDriver::initOperations() {
    cout << "initializing operations ..." << flush;

    const bool feat = true;
    initOperationsFeat< !feat >();
    initOperationsFeat< feat >();

    cout << "     [Op: " << operations.size() << "]" << endl;
}

// the operation invocation templates look a bit complex, but they help
// a lot to factorize code over the operations' parameter signatures

template< bool OB >
struct NdbapiDriver::ADelAllOp : Op {
    ADelAllOp() : Op(string("delAllA")
                     + (OB ? "_bulk" : "")) {
    }

    virtual void run(int nOps) const {
        int count;
        ops->delByScan(ops->model->table_A, count, OB);
        assert(count == nOps);
    }
};

template< bool OB >
struct NdbapiDriver::B0DelAllOp : Op {
    B0DelAllOp() : Op(string("delAllB0")
                      + (OB ? "_bulk" : "")) {
    }

    virtual void run(int nOps) const {
        int count;
        ops->delByScan(ops->model->table_B0, count, OB);
        assert(count == nOps);
    }
};

template< bool OSA, bool OB >
struct NdbapiDriver::AInsOp : Op {
    AInsOp() : Op(string("insA")
                  + (OSA ? "_attr" : "")
                  + (OB ? "_bulk" : "")) {
    }

    virtual void run(int nOps) const {
        ops->ins(ops->model->table_A, 1, nOps, OSA, OB);
    }
};

template< bool OSA, bool OB >
struct NdbapiDriver::B0InsOp : Op {
    B0InsOp() : Op(string("insB0")
                   + (OSA ? "_attr" : "")
                   + (OB ? "_bulk" : "")) {
    }

    virtual void run(int nOps) const {
        ops->ins(ops->model->table_B0, 1, nOps, OSA, OB);
    }
};

template< const char** ON,
          void (CrundNdbapiOperations::*OF)(NdbTable,int,int,bool),
          bool OB >
struct NdbapiDriver::AByPKOp : Op {
    AByPKOp() : Op(string(*ON)
                   + (OB ? "_bulk" : "")) {
    }

    virtual void run(int nOps) const {
        (ops->*OF)(ops->model->table_A, 1, nOps, OB);
    }
};

template< const char** ON,
          void (CrundNdbapiOperations::*OF)(NdbTable,int,int,bool),
          bool OB >
struct NdbapiDriver::B0ByPKOp : Op {
    B0ByPKOp() : Op(string(*ON)
                    + (OB ? "_bulk" : "")) {
    }

    virtual void run(int nOps) const {
        (ops->*OF)(ops->model->table_B0, 1, nOps, OB);
    }
};

template< const char** ON,
          void (CrundNdbapiOperations::*OF)(NdbTable,int,int,bool,int),
          bool OB >
struct NdbapiDriver::LengthOp : Op {
    const int length;

    LengthOp(int length) : Op(string(*ON)
                              + toString(length)
                              + (OB ? "_bulk" : "")),
                           length(length) {
    }

    virtual void run(int nOps) const {
        (ops->*OF)(ops->model->table_B0, 1, nOps, OB, length);
    }
};

template< const char** ON,
          void (CrundNdbapiOperations::*OF)(NdbTable,int,int,bool,int),
          bool OB >
struct NdbapiDriver::ZeroLengthOp : LengthOp< ON, OF, OB > {
    ZeroLengthOp(int length) : LengthOp< ON, OF, OB >(length) {
    }

    virtual void run(int nOps) const {
        (ops->*OF)(ops->model->table_B0, 1, nOps, OB, 0);
    }
};

template< const char** ON,
          void (CrundNdbapiOperations::*OF)(int,bool),
          bool OFS >
struct NdbapiDriver::RelOp : Op {
    RelOp() : Op(string(*ON)
                 + (OFS ? "_forceSend" : "")) {
    }

    virtual void run(int nOps) const {
        (ops->*OF)(nOps, OFS);
    }
};

template< bool feat > void
NdbapiDriver::initOperationsFeat() {

    const bool setAttr = true;
    operations.push_back(
        new AInsOp< !setAttr, feat >());

    operations.push_back(
        new B0InsOp< !setAttr, feat >());

    operations.push_back(
        new AByPKOp< &setAByPK_s, &CrundNdbapiOperations::setByPK, feat >());

    operations.push_back(
        new B0ByPKOp< &setB0ByPK_s, &CrundNdbapiOperations::setByPK, feat >());

    operations.push_back(
        new AByPKOp< &getAByPK_bb_s, &CrundNdbapiOperations::getByPK_bb, feat >());

    operations.push_back(
        new AByPKOp< &getAByPK_ar_s, &CrundNdbapiOperations::getByPK_ar, feat >());

    operations.push_back(
        new B0ByPKOp< &getB0ByPK_bb_s, &CrundNdbapiOperations::getByPK_bb, feat >());

    operations.push_back(
        new B0ByPKOp< &getB0ByPK_ar_s, &CrundNdbapiOperations::getByPK_ar, feat >());

    for (int i = 1; i <= maxVarbinaryBytes; i *= 10) {
        const int length = i;

        operations.push_back(
            new LengthOp< &setVarbinary_s, &CrundNdbapiOperations::setVarbinary, feat >(length));

        operations.push_back(
            new LengthOp< &getVarbinary_s, &CrundNdbapiOperations::getVarbinary, feat >(length));

        operations.push_back(
            new ZeroLengthOp< &clearVarbinary_s, &CrundNdbapiOperations::setVarbinary, feat >(length));
    }

    for (int i = 1; i <= maxVarcharChars; i *= 10) {
        const int length = i;

        operations.push_back(
            new LengthOp< &setVarchar_s, &CrundNdbapiOperations::setVarchar, feat >(length));

        operations.push_back(
            new LengthOp< &getVarchar_s, &CrundNdbapiOperations::getVarchar, feat >(length));

        operations.push_back(
            new ZeroLengthOp< &clearVarchar_s, &CrundNdbapiOperations::setVarchar, feat >(length));
    }

    operations.push_back(
        new RelOp< &setB0ToA_s, &CrundNdbapiOperations::setB0ToA, feat >());

    operations.push_back(
        new RelOp< &navB0ToA_s, &CrundNdbapiOperations::navB0ToA, feat >());

    operations.push_back(
        new RelOp< &navB0ToAalt_s, &CrundNdbapiOperations::navB0ToAalt, feat >());

    operations.push_back(
        new RelOp< &navAToB0_s, &CrundNdbapiOperations::navAToB0, feat >());

    operations.push_back(
        new RelOp< &navAToB0alt_s, &CrundNdbapiOperations::navAToB0alt, feat >());

    operations.push_back(
        new RelOp< &nullB0ToA_s, &CrundNdbapiOperations::nullB0ToA, feat >());

    operations.push_back(
        new B0ByPKOp< &delAByPK_s, &CrundNdbapiOperations::delByPK, feat >());

    operations.push_back(
        new AByPKOp< &delB0ByPK_s, &CrundNdbapiOperations::delByPK, feat >());

    operations.push_back(
        new AInsOp< setAttr, feat >());

    operations.push_back(
        new B0InsOp< setAttr, feat >());

    operations.push_back(
        new ADelAllOp< feat >());

    operations.push_back(
        new B0DelAllOp< feat >());
}

void
NdbapiDriver::closeOperations() {
    cout << "closing operations ..." << flush;
    for (Operations::const_iterator i = operations.begin();
         i != operations.end(); ++i) {
        delete *i;
    }
    operations.clear();
    cout << "          [ok]" << endl;
}

//---------------------------------------------------------------------------

void
NdbapiDriver::initConnection() {
    NdbOperation::LockMode ndbOpLockMode;
    switch (lockMode) {
    case READ_COMMITTED:
        ndbOpLockMode = NdbOperation::LM_CommittedRead;
        break;
    case SHARED:
        ndbOpLockMode = NdbOperation::LM_Read;
        break;
    case EXCLUSIVE:
        ndbOpLockMode = NdbOperation::LM_Exclusive;
        break;
    default:
        ndbOpLockMode = NdbOperation::LM_CommittedRead;
        assert(false);
    }

    ops->initConnection(catalog.c_str(), schema.c_str(), ndbOpLockMode);
}

void
NdbapiDriver::closeConnection() {
    ops->closeConnection();
}

void
NdbapiDriver::clearData()
{
    ops->clearData();
}

//---------------------------------------------------------------------------

int
main(int argc, const char* argv[])
{
    TRACE("main()");

    NdbapiDriver::parseArguments(argc, argv);
    NdbapiDriver d;
    d.run();

    return 0;
}

//---------------------------------------------------------------------------
