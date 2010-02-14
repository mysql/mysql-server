/*
 * Operations.hpp
 *
 */

#ifndef crundndb_Operations_hpp
#define crundndb_Operations_hpp

#include <NdbApi.hpp>
#include <NdbError.hpp>

namespace crund_ndb {

/**
 * Holds shortcuts to the benchmark's schema information.
 */
struct Meta
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
class Operations
{
// For a better locality of information, consider refactorizing this
// class into separate classes: Cluster, Db, Tx, and Operations by
// use of delegation (private inheritance doesn't match cardinalities).
// Another advantage: can use *const members/references then and
// initialize them in the constructor's initializer lists.
// But for now, having all in one class is good enough.

public:
    // the benchmark's metadata shortcuts
    const Meta* meta;

//protected:
    // singleton object representing the NDB cluster (one per process)
    Ndb_cluster_connection* mgmd;

    // object representing a connection to an NDB database
    Ndb* ndb;

    // object representing an NDB database transaction
    NdbTransaction* tx;

public:
    void init(const char* mgmd_conn_str);

    void close();

    void initConnection(const char* catalog, const char* schema);

    void closeConnection();

    void beginTransaction();

    void commitTransaction();

    void rollbackTransaction();

    void delByScan(const NdbDictionary::Table* table, int& count,
                   bool batch);

    void ins(const NdbDictionary::Table* table, int from, int to,
             bool setAttrs, bool batch);

    void delByPK(const NdbDictionary::Table* table, int from, int to,
                 bool batch);

    void setByPK(const NdbDictionary::Table* table, int from, int to,
                 bool batch);

    void getByPK(const NdbDictionary::Table* table, int from, int to,
                 bool batch);

    // XXX
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

    void setB0ToA(int count_A, int count_B,
                  bool batch);
    
    void navB0ToA(int count_A, int count_B,
                  bool batch);
    
    void navB0ToAalt(int count_A, int count_B,
                     bool batch);
    
    void navAToB0(int count_A, int count_B,
                  bool forceSend);
    
    void navAToB0alt(int count_A, int count_B,
                     bool forceSend);
    
    void nullB0ToA(int count_A, int count_B,
                   bool batch);
    
protected:
    // executes the operations in the current transaction
    void executeOperations();

    // closes the current transaction
    void closeTransaction();

    void setVar(const NdbDictionary::Table* table, int attr_cvar,
                int from, int to, bool batch, const char* str);
    
    void getVar(const NdbDictionary::Table* table, int attr_cvar,
                int from, int to, bool batch, const char* str);
};

} // crund_ndb

#endif // crundndb_Operations_hpp
