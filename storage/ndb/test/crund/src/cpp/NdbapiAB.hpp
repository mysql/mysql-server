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

#ifndef NdbapiAB_hpp
#define NdbapiAB_hpp

#include <string>

#include <NdbApi.hpp>

#include "CrundLoad.hpp"

using std::string;

// internal crund schema model
struct Model;

class NdbapiAB : public CrundLoad {
public:

    // usage
    NdbapiAB(CrundDriver& driver)
        : CrundLoad("ndbapi", driver),
          mgmd(NULL), ndb(NULL), tx(NULL), model(NULL) {
    }
    virtual ~NdbapiAB() {
        assert(mgmd == NULL); assert(ndb == NULL); assert(tx == NULL);
        assert(model == NULL);
    }

    // settings
    string mgmdConnect;
    string catalog;
    string schema;
    int nMaxConcTx;
    int nConcScans;

protected:

    // resources
    Ndb_cluster_connection* mgmd;
    Ndb* ndb;
    NdbTransaction* tx;
    NdbOperation::LockMode ndbOpLockMode; // XXX move under settings
    Model* model;

    // type shortcuts
    typedef CrundDriver::XMode XMode;

    // intializers/finalizers
    virtual void initProperties();
    virtual void printProperties();
    virtual void init();
    virtual void close();

    // datastore operations
    virtual void initConnection();
    virtual void closeConnection();
    virtual void clearData();

    // benchmark operations
    virtual void initOperations();
    virtual void closeOperations();
    virtual void buildOperations();
    template< XMode::E xMode > void addOperations();

    // general benchmark operation types
    template< XMode::E xMode > struct NdbapiOp;
    template< XMode::E xMode > struct WriteOp;
    template< XMode::E xMode > struct UpdateOp;
    template< XMode::E xMode > struct InsertOp;
    template< XMode::E xMode > struct DeleteOp;
    template< XMode::E xMode > struct ReadOp;
    template< XMode::E xMode > struct IndexScanOp;
    template< XMode::E xMode, typename ElementT > struct BufUpdateOp;
    template< XMode::E xMode, typename ElementT > struct BufReadOp;
    template< XMode::E xMode, typename ElementT > struct BufIndexScanOp;
    struct TableScanDeleteOp;

    // crund benchmark operation types
    template< XMode::E xMode, bool setAttr > struct AB_insAttr;
    template< XMode::E xMode, bool setAttr > struct A_insAttr;
    template< XMode::E xMode, bool setAttr > struct B_insAttr;
    template< XMode::E xMode > struct AB_setAttr;
    template< XMode::E xMode > struct A_setAttr;
    template< XMode::E xMode > struct B_setAttr;
    template< XMode::E xMode > struct AB_del;
    template< XMode::E xMode > struct A_del;
    template< XMode::E xMode > struct B_del;
    template< XMode::E xMode, typename HolderT > struct AB_getAttr;
    template< XMode::E xMode > struct A_getAttr_bb;
    template< XMode::E xMode > struct A_getAttr_ra;
    template< XMode::E xMode > struct B_getAttr_bb;
    template< XMode::E xMode > struct B_getAttr_ra;
    template< XMode::E xMode > struct B_setVarbinary;
    template< XMode::E xMode > struct B_clearVarbinary;
    template< XMode::E xMode > struct B_getVarbinary;
    template< XMode::E xMode > struct B_setVarchar;
    template< XMode::E xMode > struct B_clearVarchar;
    template< XMode::E xMode > struct B_getVarchar;
    template< XMode::E xMode > struct B_setA;
    template< XMode::E xMode > struct B_clearA;
    template< XMode::E xMode, typename HolderT > struct B_getA;
    template< XMode::E xMode > struct B_getA_bb;
    template< XMode::E xMode > struct B_getA_ra;
    template< XMode::E xMode, typename HolderT > struct A_getBs;
    template< XMode::E xMode > struct A_getBs_bb;
    template< XMode::E xMode > struct A_getBs_ra;
    struct A_delAll;
    struct B_delAll;

    // benchmark operation functions
    struct Holder;
    struct ValIdHolder;
    struct ValAttrHolder;
    struct RecIdHolder;
    struct RecAttrHolder;
    void setKeyAB(NdbOperation* op, int id);
    void getKeyAB(NdbOperation* op, ValIdHolder* vh);
    void getKeyAB(NdbOperation* op, RecIdHolder* rh);
    void checkKeyAB(int i, ValIdHolder* vh);
    void checkKeyAB(int i, RecIdHolder* ah);
    void setAttrAB(NdbOperation* op, int i);
    void getAttrAB(NdbOperation* op, ValAttrHolder* vh);
    void getAttrAB(NdbOperation* op, RecAttrHolder* rh);
    void checkAttrAB(int i, ValAttrHolder* vh);
    void checkAttrAB(int i, RecAttrHolder* rh);
    void setVarbinaryB(NdbOperation* op,
                     char*& buf, const bytes* data);
    void getVarbinaryB(NdbOperation* op, char* pos);
    void checkVarbinaryB(const bytes* data, char* pos);
    void setVarcharB(NdbOperation* op,
                     char*& buf, const string* data);
    void getVarcharB(NdbOperation* op, char* pos);
    void checkVarcharB(const string* data, char* pos);
    void setAIdB(NdbOperation* op, int aid);
    void getAIdB(NdbOperation* op, ValIdHolder* vh);
    void getAIdB(NdbOperation* op, RecIdHolder* rh);
    void setBoundEqAIdB(NdbIndexScanOperation* op, int id);

    static void writeLengthPrefix(char*& to, int length, int width);
    static int readLengthPrefix(const char*& from, int width);
    static char* writeBytes(char*& to, const bytes& from, int width);
    static char* writeString(char*& to, const string& from, int width);
    static void readBytes(bytes& to, const char* from, int width);
    static void readString(string& to, const char* from, int width);
};

#endif // NdbapiAB_hpp
