/*
  Copyright (c) 2010, 2023, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NdbapiTwsDriver_hpp
#define NdbapiTwsDriver_hpp

#include "TwsDriver.hpp"

#include <cstddef>
#include <cassert>

#include <NdbApi.hpp>
#include <NdbError.hpp>

using std::string;

struct NdbapiTwsModel {
    const NdbDictionary::Table* table_t0;
    const NdbDictionary::Column* column_c0;
    const NdbDictionary::Column* column_c1;
    const NdbDictionary::Column* column_c2;
    const NdbDictionary::Column* column_c3;
    const NdbDictionary::Column* column_c4;
    const NdbDictionary::Column* column_c5;
    const NdbDictionary::Column* column_c6;
    const NdbDictionary::Column* column_c7;
    const NdbDictionary::Column* column_c8;
    const NdbDictionary::Column* column_c9;
    const NdbDictionary::Column* column_c10;
    const NdbDictionary::Column* column_c11;
    const NdbDictionary::Column* column_c12;
    const NdbDictionary::Column* column_c13;
    const NdbDictionary::Column* column_c14;

    int attr_c0;
    int attr_c1;
    int attr_c2;
    int attr_c3;
    int attr_c4;
    int attr_c5;
    int attr_c6;
    int attr_c7;
    int attr_c8;
    int attr_c9;
    int attr_c10;
    int attr_c11;
    int attr_c12;
    int attr_c13;
    int attr_c14;

    int width_c0;
    int width_c1;
    int width_c2;
    int width_c3;
    int width_c4;
    int width_c5;
    int width_c6;
    int width_c7;
    int width_c8;
    int width_c9;
    int width_c10;
    int width_c11;
    int width_c12;
    int width_c13;
    int width_c14;
    int width_row; // sum of {width_c0 .. width_c14}
    static const int nCols = 15;

    NdbapiTwsModel(Ndb* ndb);

    ~NdbapiTwsModel() {}

    static int columnWidth(const NdbDictionary::Column* c) {
        int s = c->getSize(); // size of type or of base type
        int al = c->getLength(); // length or max length, 1 for scalars
        int at = c->getArrayType(); // size of length prefix, practically
        return (s * al) + at;
    }
};

class NdbapiTwsDriver : public TwsDriver {
public:

    NdbapiTwsDriver()
        : mgmd(NULL), ndb(NULL), tx(NULL), model(NULL), bb(NULL), ra(NULL) {
    }

    virtual ~NdbapiTwsDriver() {
        assert(mgmd == NULL); assert(ndb == NULL); assert(tx == NULL);
        assert(model == NULL); assert(bb == NULL); assert(ra == NULL);
    }

protected:

    // NDB API settings
    string mgmdConnect;
    string catalog;
    string schema;

    // NDB API resources
    Ndb_cluster_connection* mgmd;
    Ndb* ndb;
    NdbTransaction* tx;
    NdbOperation::LockMode ndbOpLockMode;

    // NDB Api metadata resources
    NdbapiTwsModel* model;

    // NDB Api data resources
    char* bb;
    char* bb_pos;
    NdbRecAttr** ra;
    NdbRecAttr** ra_pos;

    // NDB API initializers/finalizers
    virtual void init();
    virtual void close();
    virtual void initProperties();
    virtual void printProperties();

    // NDB API operations
    virtual void runOperations(int nOps);
    virtual void runInserts(XMode mode, int nOps);
    virtual void runLookups(XMode mode, int nOps);
    virtual void runUpdates(XMode mode, int nOps);
    virtual void runDeletes(XMode mode, int nOps);
    void initNdbapiBuffers(int nOps);
    void closeNdbapiBuffers(int nOps);
    void ndbapiInsert(int c0);
    void ndbapiLookup(int c0);
    void ndbapiRead(int c0);
    void ndbapiUpdate(int c0);
    void ndbapiDelete(int c0);
    void ndbapiBeginTransaction();
    void ndbapiExecuteTransaction();
    void ndbapiCommitTransaction();
    void ndbapiCloseTransaction();
    static void ndbapiToBuffer1blp(void* to, const char* from, size_t width);
    static void ndbapiToString1blp(char* to, const void* from, size_t width);
    virtual void initOperations();
    virtual void closeOperations();

    // NDB API datastore operations
    virtual void initConnection();
    virtual void closeConnection();
    virtual void clearData();
};

#endif // NdbapiTwsDriver_hpp
