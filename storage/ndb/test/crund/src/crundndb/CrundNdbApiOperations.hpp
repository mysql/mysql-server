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

#ifndef CrundNdbApiOperations_hpp
#define CrundNdbApiOperations_hpp

#include <NdbApi.hpp>
#include <NdbError.hpp>

/**
 * Holds shortcuts to the benchmark's schema information.
 */
struct CrundModel
{
    const NdbDictionary::Table* table_A;
    const NdbDictionary::Table* table_B0;
    const NdbDictionary::Column* column_A_id;
    const NdbDictionary::Column* column_A_cint;
    const NdbDictionary::Column* column_A_clong;
    const NdbDictionary::Column* column_A_cfloat;
    const NdbDictionary::Column* column_A_cdouble;
    const NdbDictionary::Column* column_B0_id;
    const NdbDictionary::Column* column_B0_cint;
    const NdbDictionary::Column* column_B0_clong;
    const NdbDictionary::Column* column_B0_cfloat;
    const NdbDictionary::Column* column_B0_cdouble;
    const NdbDictionary::Column* column_B0_a_id;
    const NdbDictionary::Column* column_B0_cvarbinary_def;
    const NdbDictionary::Column* column_B0_cvarchar_def;
    const NdbDictionary::Index* idx_B0_a_id;

    int attr_id;
    int attr_cint;
    int attr_clong;
    int attr_cfloat;
    int attr_cdouble;
    int attr_B0_a_id;
    int attr_B0_cvarbinary_def;
    int attr_B0_cvarchar_def;
    int attr_idx_B0_a_id;

    // initialize this instance from the dictionary
    void init(Ndb* ndb);
};

/**
 * Implements the benchmark's basic database operations.
 */
class CrundNdbApiOperations
{
// For a better locality of information, consider refactorizing this
// class into separate classes: Cluster, Db, Tx, and Operations by
// use of delegation (private inheritance doesn't match cardinalities).
// Another advantage: can use *const members/references then and
// initialize them in the constructor's initializer lists.
// But for now, having all in one class is good enough.

public:

    CrundNdbApiOperations()
        : model(NULL), mgmd(NULL), ndb(NULL), tx(NULL) {
    }

    ~CrundNdbApiOperations() {
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

    CrundNdbApiOperations(const CrundNdbApiOperations&);
    CrundNdbApiOperations& operator=(const CrundNdbApiOperations&);

public:

    void init(const char* mgmd_conn_str);

    void close();

    void initConnection(const char* catalog, const char* schema,
                        NdbOperation::LockMode defaultLockMode);

    void closeConnection();

    void clearData();

    void delByScan(const NdbDictionary::Table* table, int& count,
                   bool batch);

    void ins(const NdbDictionary::Table* table, int from, int to,
             bool setAttrs, bool batch);

    void delByPK(const NdbDictionary::Table* table, int from, int to,
                 bool batch);

    void setByPK(const NdbDictionary::Table* table, int from, int to,
                 bool batch);

    void getByPK_bb(const NdbDictionary::Table* table, int from, int to,
                    bool batch);

    void getByPK_ar(const NdbDictionary::Table* table, int from, int to,
                    bool batch);

    void setVarbinary(const NdbDictionary::Table* table,
                      int from, int to, bool batch, int length);

    void getVarbinary(const NdbDictionary::Table* table,
                      int from, int to, bool batch, int length);

    void setVarchar(const NdbDictionary::Table* table,
                    int from, int to, bool batch, int length);

    void getVarchar(const NdbDictionary::Table* table,
                    int from, int to, bool batch, int length);

    void setB0ToA(int nOps, bool batch);

    void navB0ToA(int nOps, bool batch);

    void navB0ToAalt(int nOps, bool batch);

    void navAToB0(int nOps, bool forceSend);

    void navAToB0alt(int nOps, bool forceSend);

    void nullB0ToA(int nOps, bool batch);

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
                int from, int to, bool batch, const char* str);

    void getVar(const NdbDictionary::Table* table, int attr_cvar,
                int from, int to, bool batch, const char* str);

    // XXX not used yet, see TwsDriver
    //static void ndbapiToBuffer1blp(void* to, const char* from, size_t width);
    //static void ndbapiToString1blp(char* to, const void* from, size_t width);
};

#endif // CrundNdbApiOperations_hpp
