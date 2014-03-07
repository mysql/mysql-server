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

// Operation names as variables with external linkage.
// (ISO C++ 98 does not allow for string literals as template arguments.)
const char* delAByPK_s = "A_del";
const char* delBByPK_s = "B_del";
const char* setAByPK_s = "A_set_attr";
const char* setBByPK_s = "B_set_attr";
const char* getAByPK_bb_s = "A_get_attr_bb";
const char* getBByPK_bb_s = "B_get_attr_bb";
const char* getAByPK_ah_s = "A_get_attr_ah";
const char* getBByPK_ah_s = "B_get_attr_ah";

const char* setVarbinary_s = "B_set_varbinary";
const char* getVarbinary_s = "B_get_varbinary";
const char* clearVarbinary_s = "B_clear_varbinary";
const char* setVarchar_s = "B_set_varchar";
const char* getVarchar_s = "B_get_varchar";
const char* clearVarchar_s = "B_clear_varchar";

const char* setBToA_s = "B_set_A";
const char* navBToA_s = "B_get_A";
const char* navBToAalt_s = "B_get_A_alt";
const char* navAToB_s = "A_get_Bs";
const char* navAToBalt_s = "A_get_B_alt";
const char* clearBToA_s = "B_clear_A";

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
struct NdbapiDriver::BDelAllOp : Op {
    BDelAllOp() : Op(string("delAllB")
                      + (OB ? "_bulk" : "")) {
    }

    virtual void run(int nOps) const {
        int count;
        ops->delByScan(ops->model->table_B, count, OB);
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
struct NdbapiDriver::BInsOp : Op {
    BInsOp() : Op(string("insB")
                   + (OSA ? "_attr" : "")
                   + (OB ? "_bulk" : "")) {
    }

    virtual void run(int nOps) const {
        ops->ins(ops->model->table_B, 1, nOps, OSA, OB);
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
struct NdbapiDriver::BByPKOp : Op {
    BByPKOp() : Op(string(*ON)
                    + (OB ? "_bulk" : "")) {
    }

    virtual void run(int nOps) const {
        (ops->*OF)(ops->model->table_B, 1, nOps, OB);
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
        (ops->*OF)(ops->model->table_B, 1, nOps, OB, length);
    }
};

template< const char** ON,
          void (CrundNdbapiOperations::*OF)(NdbTable,int,int,bool,int),
          bool OB >
struct NdbapiDriver::ZeroLengthOp : LengthOp< ON, OF, OB > {
    ZeroLengthOp(int length) : LengthOp< ON, OF, OB >(length) {
    }

    virtual void run(int nOps) const {
        (ops->*OF)(ops->model->table_B, 1, nOps, OB, 0);
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
        new BInsOp< !setAttr, feat >());

    operations.push_back(
        new AByPKOp< &setAByPK_s, &CrundNdbapiOperations::setByPK, feat >());

    operations.push_back(
        new BByPKOp< &setBByPK_s, &CrundNdbapiOperations::setByPK, feat >());

    operations.push_back(
        new AByPKOp< &getAByPK_bb_s, &CrundNdbapiOperations::getByPK_bb, feat >());

    operations.push_back(
        new AByPKOp< &getAByPK_ah_s, &CrundNdbapiOperations::getByPK_ah, feat >());

    operations.push_back(
        new BByPKOp< &getBByPK_bb_s, &CrundNdbapiOperations::getByPK_bb, feat >());

    operations.push_back(
        new BByPKOp< &getBByPK_ah_s, &CrundNdbapiOperations::getByPK_ah, feat >());

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
        new RelOp< &setBToA_s, &CrundNdbapiOperations::setBToA, feat >());

    operations.push_back(
        new RelOp< &navBToA_s, &CrundNdbapiOperations::navBToA, feat >());

    operations.push_back(
        new RelOp< &navBToAalt_s, &CrundNdbapiOperations::navBToAalt, feat >());

    operations.push_back(
        new RelOp< &navAToB_s, &CrundNdbapiOperations::navAToB, feat >());

    operations.push_back(
        new RelOp< &navAToBalt_s, &CrundNdbapiOperations::navAToBalt, feat >());

    operations.push_back(
        new RelOp< &clearBToA_s, &CrundNdbapiOperations::clearBToA, feat >());

    operations.push_back(
        new BByPKOp< &delAByPK_s, &CrundNdbapiOperations::delByPK, feat >());

    operations.push_back(
        new AByPKOp< &delBByPK_s, &CrundNdbapiOperations::delByPK, feat >());

    operations.push_back(
        new AInsOp< setAttr, feat >());

    operations.push_back(
        new BInsOp< setAttr, feat >());

    operations.push_back(
        new ADelAllOp< feat >());

    operations.push_back(
        new BDelAllOp< feat >());
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
