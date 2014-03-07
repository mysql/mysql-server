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

#ifndef CrundNdbapiOperations_hpp
#define CrundNdbapiOperations_hpp

#include <NdbApi.hpp>
#include <NdbError.hpp>

/**
 * Holds shortcuts to the benchmark's schema information.
 */
struct CrundModel
{
    const NdbDictionary::Table* table_A;
    const NdbDictionary::Table* table_B;
    const NdbDictionary::Column* column_A_id;
    const NdbDictionary::Column* column_A_cint;
    const NdbDictionary::Column* column_A_clong;
    const NdbDictionary::Column* column_A_cfloat;
    const NdbDictionary::Column* column_A_cdouble;
    const NdbDictionary::Column* column_B_id;
    const NdbDictionary::Column* column_B_cint;
    const NdbDictionary::Column* column_B_clong;
    const NdbDictionary::Column* column_B_cfloat;
    const NdbDictionary::Column* column_B_cdouble;
    const NdbDictionary::Column* column_B_a_id;
    const NdbDictionary::Column* column_B_cvarbinary_def;
    const NdbDictionary::Column* column_B_cvarchar_def;
    const NdbDictionary::Index* idx_B_a_id;

    int attr_id;
    int attr_cint;
    int attr_clong;
    int attr_cfloat;
    int attr_cdouble;
    int attr_B_a_id;
    int attr_B_cvarbinary_def;
    int attr_B_cvarchar_def;
    int attr_idx_B_a_id;

    // initialize this instance from the dictionary
    void init(Ndb* ndb);
};

/**
 * Implements the benchmark's basic database operations.
 */
class CrundNdbapiOperations
{
// For a better locality of information, consider refactorizing this
// class into separate classes: Cluster, Db, Tx, and Operations by
// use of delegation (private inheritance doesn't match cardinalities).
// Another advantage: can use *const members/references then and
// initialize them in the constructor's initializer lists.
// But for now, having all in one class is good enough.

public:

    CrundNdbapiOperations()
        : model(NULL), mgmd(NULL), ndb(NULL), tx(NULL) {
    }

    ~CrundNdbapiOperations() {
        assert(model == NULL);
        assert(mgmd == NULL); assert(ndb == NULL); assert(tx == NULL);
    }

    // NDB Api metadata resources
    const CrundModel* model;

protected:

    // NDB API resources
    Ndb_cluster_connection* mgmd;
    Ndb* ndb;
    NdbTransaction* tx;
    NdbOperation::LockMode ndbOpLockMode;

    // NDB Api data resources
    // XXX not used yet, see TwsDriver
    //char* bb;
    //char* bb_pos;
    //NdbRecAttr** ra;
    //NdbRecAttr** ra_pos;

private:

    CrundNdbapiOperations(const CrundNdbapiOperations&);
    CrundNdbapiOperations& operator=(const CrundNdbapiOperations&);

public:

    void init(const char* mgmd_conn_str);

    void close();

    void initConnection(const char* catalog, const char* schema,
                        NdbOperation::LockMode defaultLockMode);

    void closeConnection();

    void clearData();

    void delByScan(const NdbDictionary::Table* table, int& count,
                   bool bulk);

    void ins(const NdbDictionary::Table* table, int from, int to,
             bool setAttrs, bool bulk);

    void delByPK(const NdbDictionary::Table* table, int from, int to,
                 bool bulk);

    void setByPK(const NdbDictionary::Table* table, int from, int to,
                 bool bulk);

    void getByPK_bb(const NdbDictionary::Table* table, int from, int to,
                    bool bulk);

    void getByPK_ah(const NdbDictionary::Table* table, int from, int to,
                    bool bulk);

    void setVarbinary(const NdbDictionary::Table* table,
                      int from, int to, bool bulk, int length);

    void getVarbinary(const NdbDictionary::Table* table,
                      int from, int to, bool bulk, int length);

    void setVarchar(const NdbDictionary::Table* table,
                    int from, int to, bool bulk, int length);

    void getVarchar(const NdbDictionary::Table* table,
                    int from, int to, bool bulk, int length);

    void setBToA(int nOps, bool bulk);

    void navBToA(int nOps, bool bulk);

    void navBToAalt(int nOps, bool bulk);

    void navAToB(int nOps, bool forceSend);

    void navAToBalt(int nOps, bool forceSend);

    void clearBToA(int nOps, bool bulk);

protected:

    // XXX not used yet, see TwsDriver
    //void ndbapiBeginTransaction();
    //void ndbapiExecuteTransaction();
    //void ndbapiCommitTransaction();
    //void ndbapiCloseTransaction();
    void beginTransaction();
    void executeOperations();
    void commitTransaction();
    void closeTransaction();

    void setVar(const NdbDictionary::Table* table, int attr_cvar,
                int from, int to, bool bulk, const char* str);

    void getVar(const NdbDictionary::Table* table, int attr_cvar,
                int from, int to, bool bulk, const char* str);

    // XXX not used yet, see TwsDriver
    //static void ndbapiToBuffer1blp(void* to, const char* from, size_t width);
    //static void ndbapiToString1blp(char* to, const void* from, size_t width);
};

#endif // CrundNdbapiOperations_hpp
