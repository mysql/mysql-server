/*
   Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

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

#include <new>
#include <assert.h>
#include <mysql.h>
#include <mysqld_error.h>

#include <ndb_global.h>
#include <ndb_opts.h>
#include <NDBT.hpp>
#include <NdbApi.hpp>
#include "../../src/ndbapi/NdbQueryBuilder.hpp"
#include "../../src/ndbapi/NdbQueryOperation.hpp"


/* TODO:
 - RecAttr and setResultRowBuff result retrieval.
 - Parameter operands.
 - Add another table type (e.g. with CHAR() fields.)
*/

#ifdef NDEBUG
// Some asserts have side effects, and there is no other error handling anyway.
#define ASSERT_ALWAYS(cond) if(!(cond)){abort();}
#else
#define ASSERT_ALWAYS assert
#endif
/* Query-related error codes. Used for negative testing. */
#define QRY_REQ_ARG_IS_NULL 4800
#define QRY_TOO_FEW_KEY_VALUES 4801
#define QRY_TOO_MANY_KEY_VALUES 4802
#define QRY_OPERAND_HAS_WRONG_TYPE 4803
#define QRY_CHAR_OPERAND_TRUNCATED 4804
#define QRY_NUM_OPERAND_RANGE 4805
#define QRY_MULTIPLE_PARENTS 4806
#define QRY_UNKONWN_PARENT 4807
#define QRY_UNKNOWN_COLUMN 4808
#define QRY_UNRELATED_INDEX 4809
#define QRY_WRONG_INDEX_TYPE 4810
#define QRY_OPERAND_ALREADY_BOUND 4811
#define QRY_DEFINITION_TOO_LARGE 4812
#define QRY_SEQUENTIAL_SCAN_SORTED 4813
#define QRY_RESULT_ROW_ALREADY_DEFINED 4814
#define QRY_HAS_ZERO_OPERATIONS 4815
#define QRY_IN_ERROR_STATE 4816
#define QRY_ILLEGAL_STATE 4817
#define QRY_WRONG_OPERATION_TYPE 4820
#define QRY_SCAN_ORDER_ALREADY_SET 4821
#define QRY_PARAMETER_HAS_WRONG_TYPE 4822
#define QRY_CHAR_PARAMETER_TRUNCATED 4823
#define QRY_MULTIPLE_SCAN_SORTED 4824
#define QRY_BATCH_SIZE_TOO_SMALL 4825


namespace SPJSanityTest{

  static void resetError(const NdbError& err)
  {
    new (&const_cast<NdbError&>(err)) NdbError;
  }

  class IntField{
  public:
    static const char* getType(){
      return "INT";
    }

    IntField(int i=0): 
      m_val(i)
    {}

    const char* toStr(char* buff) const {
      sprintf(buff, "%d", m_val);
      return buff;
    }
    
    int compare(const IntField& other) const{
      if (m_val > other.m_val)
        return 1;
      else if (m_val == other.m_val)
        return 0;
      else
        return -1;
    }

    Uint64 getValue() const{
      return m_val;
    }

    Uint32 getSize() const{
      return sizeof m_val;
   }

  private:
    uint m_val;
  };

  class StrField{
  public:
    static const char* getType(){
      return "VARCHAR(10)";
    }

    StrField(int i=0):
      m_len(6){
      // bzero(m_val, sizeof m_val);
      sprintf(m_val, "c%5d", i);
    }

    const char* toStr(char* buff) const {
      sprintf(buff, "'%s'", getValue());
      return buff;
    }
    
    int compare(const StrField& other) const{
      return strcmp(getValue(), other.getValue());
    }

    const char* getValue() const{
      m_val[m_len] = '\0';
      return m_val;
    }

    Uint32 getSize() const{
      m_val[m_len] = '\0';
      return strlen(m_val);
    }

  private:
    Uint8 m_len;
    mutable char m_val[10];
  };

  
  /* Key class.*/
  template <typename FieldType>
  class GenericKey{
  public:
    static const int size = 2;
    FieldType m_values[size];

    NdbConstOperand* makeConstOperand(NdbQueryBuilder& builder, 
                                      int fieldNo) const {
      ASSERT_ALWAYS(fieldNo<size);
      //return builder.constValue(m_values[fieldNo]);
      return builder.constValue(m_values[fieldNo].getValue());
    }
  };

  /* Concrete Row class.*/
  template <typename FieldType>
  class GenericRow{
  public:
    static const int size = 4;

    FieldType m_values[size];

    explicit GenericRow<FieldType>(int rowNo){
      /* Attribute values are chosen such that rows are sorted on 
       * all attribtes, and that any pair of consecutive columns can be
       * used as a foreign key to the table itself.*/
      for(int i = 0; i<size; i++){
        m_values[i] = FieldType(i+rowNo);
      }
    }

    static const char *getType(int colNo){
      //return "INT";
      return FieldType::getType();
    }
   
    static void makeSQLValues(char* buffer, int rowNo){
      const GenericRow<FieldType> row(rowNo);
      sprintf(buffer, "values(");
      char* tail = buffer+strlen(buffer);
      for(int i = 0; i<size; i++){
        char tmp[11];
        if(i<size-1){
          // sprintf(tail, "%d,", row.m_values[i].toStr(tmp));
          sprintf(tail, "%s,", row.m_values[i].toStr(tmp));
        }else{
          sprintf(tail, "%s)", row.m_values[i].toStr(tmp));
        }
        tail = buffer+strlen(buffer);
      }
    } 

    GenericKey<FieldType> getPrimaryKey() const;
    
    GenericKey<FieldType> getIndexKey() const;
    
    GenericKey<FieldType> getForeignKey(int keyNo) const;

    void makeLessThanCond(NdbScanFilter& scanFilter){
      //ASSERT_ALWAYS(scanFilter.lt(0, m_values[0].getValue())==0); 
      ASSERT_ALWAYS(scanFilter.cmp(NdbScanFilter::COND_LT, 0, m_values, m_values[0].getSize())==0); 
    }

    /** Get the row column number that corresponds to the n'th column 
     * of the index.*/
    static int getIndexKeyColNo(int indexCol);

    /** Get the row column number that corresponds to the n'th column 
     * of the m'th foreign key..*/
    static int getForeignKeyColNo(int keyNo, int keyCol);
  };

  template <typename FieldType>
  GenericKey<FieldType> GenericRow<FieldType>::getPrimaryKey() const {
    GenericKey<FieldType> key;
    for(int i = 0; i<GenericKey<FieldType>::size; i++){
      key.m_values[i] = m_values[i];    
    }
    return key;
  }

  template <typename FieldType>
  GenericKey<FieldType> GenericRow<FieldType>::getIndexKey() const {
    return getForeignKey(1);
  }

  template <typename FieldType>
  GenericKey<FieldType> GenericRow<FieldType>::getForeignKey(int keyNo) const {
    ASSERT_ALWAYS(keyNo<=1);
    GenericKey<FieldType> key;
    for(int i = 0; i<GenericKey<FieldType>::size; i++){
      key.m_values[i] = m_values[getForeignKeyColNo(keyNo,i)];    
    }
    return key;
  }

  template <typename FieldType>
  int GenericRow<FieldType>::getIndexKeyColNo(int indexCol){
    return getForeignKeyColNo(1, indexCol);
  }

  template <typename FieldType>
  int GenericRow<FieldType>::getForeignKeyColNo(int keyNo, int keyCol){
    ASSERT_ALWAYS(keyNo<GenericRow<FieldType>::size-GenericKey<FieldType>::size);
    ASSERT_ALWAYS(keyCol<GenericKey<FieldType>::size);
    return size-GenericKey<FieldType>::size-keyNo+keyCol;
  }

  template <typename FieldType>
  static bool operator==(const GenericRow<FieldType>& a, const GenericRow<FieldType>& b){
    for(int i = 0; i<GenericRow<FieldType>::size; i++){
      if(a.m_values[i].compare(b.m_values[i]) != 0){
        return false;
      }
    }
    return true;
  }

  template <typename FieldType>
  static bool operator==(const GenericKey<FieldType>& a, const GenericKey<FieldType>& b){
    for(int i = 0; i<GenericKey<FieldType>::size; i++){
      if(a.m_values[i].compare(b.m_values[i]) != 0){
        return false;
      }
    }
    return true;
  }

  /** Returns true if key of a <= key of b.*/
  template <typename FieldType>
  static bool lessOrEqual(const GenericRow<FieldType>& a, const GenericRow<FieldType>& b){
    for(int i = 0; i<GenericKey<FieldType>::size; i++){
      if(a.m_values[i].compare(b.m_values[i]) == 1){
        return false;
      }
    }
    return true;
  }

  template <typename FieldType>
  static NdbOut& operator<<(NdbOut& out, const GenericRow<FieldType>& row){
    char buff[11];
    out << "{";
    for(int i = 0; i<GenericRow<FieldType>::size; i++){
      out << row.m_values[i].toStr(buff);
      if(i<GenericRow<FieldType>::size-1){
        out << ", ";
      }
    }
    out << "}";
    return out;
  }

  //typedef GenericRow<IntField> Row;
  //typedef GenericKey<IntField> Key;
  typedef GenericRow<StrField> Row;
  typedef GenericKey<StrField> Key;


  static const char* colName(int colNo){
    static const char* names[] = {
      "c1", "c2", "c3", "c4", "c5", "c6", "c7", "c8", "c9", "c10"
    };
    ASSERT_ALWAYS(static_cast<unsigned int>(colNo)< sizeof names/sizeof names[0]);
    return names[colNo];
  };
  
  static void printMySQLError(MYSQL& mysql, const char* before=NULL){
    if(before!=NULL){
      ndbout << before;
    }
    ndbout << mysql_error(&mysql) << endl;
    exit(-1);
  }

  static void mySQLExec(MYSQL& mysql, const char* stmt){
    ndbout << stmt << ";" << endl;
    if(mysql_query(&mysql, stmt) != 0){
      ndbout << "Error executing '" << stmt << "' : ";
      printMySQLError(mysql);
    }
  }

  class Query;

  /** Class representing a single NdbQueryOperation. 'Row'
   * is a template argument, to allow different table defintions.*/
  class Operation{
  public:

    explicit Operation(class Query& query, Operation* parent);

    virtual ~Operation(){}

    //protected: FIXME
  public:
    friend class Query;
    /** Enclosing NdbQuery.*/
    Query& m_query;
    /** Optional parent operation.*/
    const Operation* m_parent;
    Vector<Operation*> m_children;
    const NdbQueryOperationDef* m_operationDef;
    // For now, only setResultRowRef() style result retrieval is tested.
    union
    {
      const Row* m_resultPtr;
      // Use union to avoid strict-aliasing problems.
      const char* m_resultCharPtr;
    };
    // Corresponds to NdbQueryOperationDef operation numbering
    Uint32 m_operationId;
    // Number among siblings.
    const Uint32 m_childNo;
    
    /** Check that result of this op and descendans is ok.*/
    void verifyRow();

    /** Check that result of this op is ok.*/
    virtual void verifyOwnRow() = 0;

    /** Build operation definition.*/
    virtual void buildThis(NdbQueryBuilder& builder, 
                           const NdbDictionary::Table& tab) = 0;

    /** Build this and descendants.*/
    void build(NdbQueryBuilder& builder,
               const NdbDictionary::Table& tab);
    /** Set up result retrieval before execution.*/
    virtual void submit()=0;

    void compareRows(const char* text, 
                     const Row* expected, 
                     const Row* actual) const;
  };

  class Query{
  public:

    explicit Query(Ndb& ndb);

    ~Query(){
      m_builder->destroy();
      if (m_queryDef != NULL)
      {
        m_queryDef->destroy();
      }
    }

    /** Build query definition.*/
    void build(const NdbDictionary::Table& tab, int tableSize);

    /** Execute within transaction.*/
    void submit(NdbTransaction& transaction);

    void submitOperation(Operation& operation) const;

    void setRoot(Operation& root){ m_root = &root;}

    NdbQuery::NextResultOutcome nextResult(){ 
      return m_query->nextResult(true, false);
    }

    /** Verify current row for all operations.*/
    void verifyRow() const {
      m_root->verifyRow();
    }

    Uint32 allocOperationId(){ 
      return m_operationCount++;
    }

    NdbQueryOperation* getOperation(Uint32 ident) const {
      return m_query->getQueryOperation(ident);
    }

    int getTableSize() const { 
      return m_tableSize; 
    }

    const NdbRecord* getNdbRecord() const {
      return m_ndbRecord;
    }

    const NdbDictionary::Dictionary* getDictionary() const {
      return m_ndb.getDictionary();
    }

    void close(bool forceSend = false){
      m_query->close(forceSend);
    }
  private:
    Ndb& m_ndb;
    NdbQueryBuilder* const m_builder;
    Operation* m_root;
    const NdbQueryDef* m_queryDef;
    NdbQuery* m_query;
    Uint32 m_operationCount;
    int m_tableSize;
    const NdbRecord* m_ndbRecord;
  };

  
  class LookupOperation: public Operation{
  public:
    explicit LookupOperation(Query& query, 
                             Operation* parent = NULL);
    virtual void verifyOwnRow();

    /** Set up result retrieval before execution.*/
    virtual void submit();
  protected:
    virtual void buildThis(NdbQueryBuilder& builder, 
                           const NdbDictionary::Table& tab);
  };

  class IndexLookupOperation: public Operation{
  public:
    explicit IndexLookupOperation(Query& query, 
                                  const char* indexName,
                                  Operation* parent = NULL);
    virtual void verifyOwnRow();

    /** Set up result retrieval before execution.*/
    virtual void submit();
  protected:
    virtual void buildThis(NdbQueryBuilder& builder, 
                           const NdbDictionary::Table& tab);
  private:
    const char* const m_indexName;
  };

  class TableScanOperation: public Operation{
  public:

    explicit TableScanOperation(Query& query, int lessThanRow=-1);

    virtual ~TableScanOperation() {
      delete[] m_rowFound;
    } 

    virtual void verifyOwnRow();

    /** Set up result retrieval before execution.*/
    virtual void submit();

  protected:
    virtual void buildThis(NdbQueryBuilder& builder, 
                           const NdbDictionary::Table& tab);
  private:
    bool* m_rowFound;
    int m_lessThanRow;
  };

  class IndexScanOperation: public Operation{
  public:

    explicit IndexScanOperation(Query& query,
                                const char* indexName,
                                int lowerBoundRowNo, 
                                int upperBoundRowNo,
                                NdbQueryOptions::ScanOrdering ordering);

    virtual ~IndexScanOperation() {
      delete[] m_rowFound;
    } 

    virtual void verifyOwnRow();

    /** Set up result retrieval before execution.*/
    virtual void submit();

  protected:
    virtual void buildThis(NdbQueryBuilder& builder, 
                           const NdbDictionary::Table& tab);
  private:
    const char* const m_indexName;
    /** Number of table row from which get key to use as lower bound.*/
    const int m_lowerBoundRowNo;
    /** Number of table row from which get key to use as upper bound.*/
    const int m_upperBoundRowNo;
    /** An entry per row. True if row has been seen in the result stream.*/
    bool* m_rowFound;
    /** Ordering of results.*/
    NdbQueryOptions::ScanOrdering m_ordering;
    /** Previous row, for verifying ordering.*/
    Row m_previousRow;
    /** True from the second row and onwards.*/
    bool m_hasPreviousRow;
  };


  // Query methods.

  Query::Query(Ndb& ndb):
    m_ndb(ndb),
    m_builder(NdbQueryBuilder::create()),
    m_root(NULL),
    m_queryDef(NULL),
    m_query(NULL),
    m_operationCount(0),
    m_tableSize(-1),
    m_ndbRecord(NULL)
  {
    ASSERT_ALWAYS(m_builder != NULL);
  }

  void Query::build(const NdbDictionary::Table& tab, int tableSize){
    m_tableSize = tableSize;
    m_root->build(*m_builder, tab);
    m_queryDef = m_builder->prepare();
    m_ndbRecord = tab.getDefaultRecord();
  }

  void Query::submit(NdbTransaction& transaction){
    m_query = transaction.createQuery(m_queryDef);
    ASSERT_ALWAYS(m_query!=NULL);
    submitOperation(*m_root);
  }

  void Query::submitOperation(Operation& operation) const{
    // Do a depth first traversal of the operations graph.
    operation.submit();
    for(Uint32 i = 0; i<operation.m_children.size(); i++){
      submitOperation(*operation.m_children[i]);
    }
  }

  // Operation methods.
  Operation::Operation(class Query& query, 
                                 Operation* parent):
    m_query(query),
    m_parent(parent),
    m_operationDef(NULL),
    m_resultPtr(NULL),
    m_childNo(parent == NULL ? 0 : parent->m_children.size())
  {
    if(parent==NULL){
      query.setRoot(*this);
    }else{
      parent->m_children.push_back(this);
    }
  }

  void Operation::build(NdbQueryBuilder& builder,
                                  const NdbDictionary::Table& tab){
    m_operationId = m_query.allocOperationId();
    buildThis(builder, tab);
    ASSERT_ALWAYS(builder.getNdbError().code==0);
    for(Uint32 i = 0; i<m_children.size(); i++){
      m_children[i]->build(builder, tab);
    }
  }

  void Operation::verifyRow(){
    verifyOwnRow();
    for(Uint32 i = 0; i<m_children.size(); i++){
      m_children[i]->verifyRow();
    }
  }

  typedef const char* constCharPtr;

  void Operation::compareRows(const char* text, 
                                   const Row* expected, 
                                   const Row* actual) const{
    if(expected==NULL){
      if(actual==NULL){
        ndbout << text << " operationId=" << m_operationId
               << " expected NULL and got it." << endl;
      }else{
        ndbout << text << " operationId=" << m_operationId
               << " expected NULL but got." << *actual
               << endl;
        ASSERT_ALWAYS(false);
      }
    }else{
      if(actual==NULL){
        ndbout << text << " operationId=" << m_operationId
               << " expected: " << *expected
               << " but got NULL." << endl;
        ASSERT_ALWAYS(false);
      }else{
        ndbout << text << " operationId=" << m_operationId
               << " expected: " << *expected;
        if(*expected == *actual){
          ndbout << " and got it." << endl;
        }else{
          ndbout << " but got: " << *actual;
          ASSERT_ALWAYS(false);
        }
      }
    }
  };
  
  // LookupOperation methods.

  LookupOperation
  ::LookupOperation(Query& query, 
                    Operation* parent):
    Operation(query, parent){
  }

  void LookupOperation::buildThis(NdbQueryBuilder& builder,
                                            const NdbDictionary::Table& tab){
    NdbQueryOperand* keyOperands[Key::size+2];
    if(m_parent==NULL){
      const Key key = Row(0).getPrimaryKey();
      for(int i = 0; i<Key::size; i++){
        keyOperands[i] = key.makeConstOperand(builder, i);
      }
    }else{
      // Negative testing
      ASSERT_ALWAYS(builder.linkedValue(m_parent->m_operationDef, 
                                        "unknown_col") == NULL);
      ASSERT_ALWAYS(builder.getNdbError().code == QRY_UNKNOWN_COLUMN);

      for(int i = 0; i<Key::size; i++){
        keyOperands[i] = 
          builder.linkedValue(m_parent->m_operationDef, 
                              colName(Row::getForeignKeyColNo(
                                        m_childNo, i)));
        ASSERT_ALWAYS(keyOperands[i]!=NULL);
        /*Row::makeLinkedKey(builder, keyOperands, 
                         Operation::m_parent->m_operationDef, 
                         Operation::m_childNo);*/
      }
    }
    // Negative testing
    keyOperands[Key::size] = keyOperands[0];
    keyOperands[Key::size+1] = NULL;
    ASSERT_ALWAYS(builder.readTuple(&tab, keyOperands)== NULL);
    ASSERT_ALWAYS(builder.getNdbError().code == QRY_TOO_MANY_KEY_VALUES);
    resetError(builder.getNdbError());

    keyOperands[Key::size] = NULL;
    m_operationDef = builder.readTuple(&tab, keyOperands);
    ASSERT_ALWAYS(m_operationDef != NULL);

    // Negative testing
    keyOperands[Key::size-1] = builder.constValue(0x1fff1fff);
    ASSERT_ALWAYS(keyOperands[Key::size-1] != NULL);
    ASSERT_ALWAYS(builder.readTuple(&tab, keyOperands) == NULL);
    ASSERT_ALWAYS(builder.getNdbError().code == QRY_OPERAND_HAS_WRONG_TYPE);

    // Negative testing
    keyOperands[Key::size-1] = NULL;
    ASSERT_ALWAYS(builder.readTuple(&tab, keyOperands) == NULL);
    ASSERT_ALWAYS(builder.getNdbError().code == QRY_TOO_FEW_KEY_VALUES);
    resetError(builder.getNdbError());
  }

  void LookupOperation::submit(){
    NdbQueryOperation* queryOp 
      = m_query.getOperation(m_operationId);
    // Negative testing
    ASSERT_ALWAYS(queryOp->setResultRowRef(NULL, 
                                           m_resultCharPtr,
                                           NULL) == -1);
    ASSERT_ALWAYS(queryOp->getQuery().getNdbError().code == 
                  QRY_REQ_ARG_IS_NULL);
    ASSERT_ALWAYS(queryOp->setOrdering(NdbQueryOptions::ScanOrdering_ascending)
                  == -1);
    ASSERT_ALWAYS(queryOp->getQuery().getNdbError().code == 
                  QRY_WRONG_OPERATION_TYPE);

    ASSERT_ALWAYS(queryOp->setResultRowRef(m_query.getNdbRecord(), 
                                           m_resultCharPtr,
                                           NULL) == 0);
    // Negative testing
    ASSERT_ALWAYS(queryOp->setResultRowRef(m_query.getNdbRecord(), 
                                           m_resultCharPtr,
                                           NULL) == -1);
    ASSERT_ALWAYS(queryOp->getQuery().getNdbError().code == 
                  QRY_RESULT_ROW_ALREADY_DEFINED);
  }

  void LookupOperation::verifyOwnRow(){
    if(m_parent==NULL){
      const Row expected(0);
      compareRows("lookup root operation",
                                  &expected, 
                                  m_resultPtr);
    }else{
      NdbQueryOperation* queryOp 
        = m_query
        .getOperation(m_operationId);
      if(!queryOp->getParentOperation(0)->isRowNULL()){
        const Key key = 
          m_parent->m_resultPtr
          ->getForeignKey(m_childNo);
        bool found = false;
        for(int i = 0; i<m_query.getTableSize(); i++){
          const Row row(i);
          if(row.getPrimaryKey() == key){
            found = true;
            compareRows("lookup child operation",
                                        &row, 
                                        m_resultPtr);
          }
        }
        if(!found && !queryOp->isRowNULL()){
          compareRows("lookup child operation",
                                      NULL,
                                      m_resultPtr);
        }
      }
    }
  }

  // IndexLookupOperation methods.

  IndexLookupOperation
  ::IndexLookupOperation(Query& query, 
                         const char* indexName, 
                         Operation* parent):
    Operation(query, parent),
    m_indexName(indexName){
  }

  void IndexLookupOperation
  ::buildThis(NdbQueryBuilder& builder,
              const NdbDictionary::Table& tab){
    const NdbDictionary::Dictionary* const dict 
      = m_query.getDictionary();
    char fullName[200];
    sprintf(fullName, "%s$unique", m_indexName);
    const NdbDictionary::Index* const index
      = dict->getIndex(fullName, tab.getName());
    ASSERT_ALWAYS(index!=NULL);

    NdbQueryOperand* keyOperands[Key::size+1];
    if(m_parent==NULL){
      const Key key = Row(0).getIndexKey();
      for(int i = 0; i<Key::size; i++){
        keyOperands[i] = key.makeConstOperand(builder, i);
      }
    }else{
      for(int i = 0; i<Key::size; i++){
        keyOperands[i] = 
          builder.linkedValue(m_parent->m_operationDef,
                              colName(Row::getForeignKeyColNo(
                                        m_childNo, i)));
        ASSERT_ALWAYS(keyOperands[i]!=NULL);
      }
      /*Row::makeLinkedKey(builder, keyOperands, 
                         Operation::m_parent->m_operationDef, 
                         Operation::m_childNo);*/
    }
    keyOperands[Key::size] = NULL;
    m_operationDef = builder.readTuple(index, &tab, keyOperands);

    // Negative testing
    const NdbDictionary::Index* const orderedIndex
      = dict->getIndex(m_indexName, tab.getName());
    ASSERT_ALWAYS(orderedIndex != NULL);
    ASSERT_ALWAYS(builder.readTuple(orderedIndex, &tab, keyOperands) == NULL);
    ASSERT_ALWAYS(builder.getNdbError().code == QRY_WRONG_INDEX_TYPE);
    resetError(builder.getNdbError());
  }

  void IndexLookupOperation::submit(){
    NdbQueryOperation* queryOp 
      = m_query.getOperation(m_operationId);
    queryOp->setResultRowRef(m_query.getNdbRecord(), 
                             m_resultCharPtr,
                             NULL);
  }

  void IndexLookupOperation::verifyOwnRow(){
    if(m_parent==NULL){
      const Row expected(0);
      compareRows("index lookup root operation",
                                  &expected, 
                                  m_resultPtr);
    }else{
      NdbQueryOperation* queryOp 
        = m_query
        .getOperation(m_operationId);
      if(!queryOp->getParentOperation(0)->isRowNULL()){
        const Key key = 
          m_parent->m_resultPtr
          ->getForeignKey(m_childNo);
        bool found = false;
        for(int i = 0; i<m_query.getTableSize(); i++){
          const Row row(i);
          if(row.getIndexKey() == key){
            found = true;
            compareRows("index lookup child operation",
                                        &row, 
                                        m_resultPtr);
          }
        }
        if(!found && !queryOp->isRowNULL()){
          compareRows("index lookup child operation",
                                      NULL,
                                      m_resultPtr);
        }
      }
    }
  }

  // TableScanOperation methods.

  TableScanOperation
  ::TableScanOperation(Query& query, int lessThanRow):
    Operation(query, NULL),
    m_rowFound(NULL),
    m_lessThanRow(lessThanRow){
  }

  void TableScanOperation::buildThis(NdbQueryBuilder& builder,
                                          const NdbDictionary::Table& tab){
    m_operationDef = builder.scanTable(&tab);
    m_rowFound = new bool[m_query.getTableSize()];
    for(int i = 0; i<m_query.getTableSize(); i++){
      m_rowFound[i] = false;
    }
  }

  void TableScanOperation::submit(){
    NdbQueryOperation* queryOp 
      = m_query.getOperation(m_operationId);
    queryOp->setResultRowRef(m_query.getNdbRecord(), 
                             m_resultCharPtr,
                             NULL);
    if(m_lessThanRow!=-1){
      NdbInterpretedCode code(queryOp->getQueryOperationDef().getTable());
      NdbScanFilter filter(&code);
      ASSERT_ALWAYS(filter.begin()==0);
      Row(m_lessThanRow).makeLessThanCond(filter);
      ASSERT_ALWAYS(filter.end()==0);
      ASSERT_ALWAYS(queryOp->setInterpretedCode(code)==0);
    }
  }

  void TableScanOperation::verifyOwnRow(){
    bool found = false;
    const int upperBound = 
      m_lessThanRow==-1 ? 
      m_query.getTableSize() :
      m_lessThanRow;
    for(int i = 0; i<upperBound; i++){
      //const Row row(i);
      if(Row(i) == *m_resultPtr){
        found = true;
        if(m_rowFound[i]){
          ndbout << "Root table scan operation: " 
                 << *m_resultPtr
                 << "appeared twice." << endl;
          ASSERT_ALWAYS(false);
        }
        m_rowFound[i] = true;
      }
    }
    if(!found){
      ndbout << "Root table scan operation. Unexpected row: " 
             << *m_resultPtr << endl;
      ASSERT_ALWAYS(false);
    }else{
      ndbout << "Root table scan operation. Got row: " 
             << *m_resultPtr
             << " as expected." << endl;
    }
  }

    // IndexScanOperation methods.

  IndexScanOperation
  ::IndexScanOperation(Query& query, 
                       const char* indexName,
                       int lowerBoundRowNo, 
                       int upperBoundRowNo,
                       NdbQueryOptions::ScanOrdering ordering):
    Operation(query, NULL),
    m_indexName(indexName),
    m_lowerBoundRowNo(lowerBoundRowNo),
    m_upperBoundRowNo(upperBoundRowNo),
    m_ordering(ordering),
    m_previousRow(0),
    m_hasPreviousRow(false){
  }

  void IndexScanOperation::buildThis(NdbQueryBuilder& builder,
                                               const NdbDictionary::Table& tab){
    const NdbDictionary::Dictionary* const dict 
      = m_query.getDictionary();
    const NdbDictionary::Index* const index
      = dict->getIndex(m_indexName, tab.getName());
    ASSERT_ALWAYS(index!=NULL);

    const NdbQueryOperand* low[Key::size+1];
    const NdbQueryOperand* high[Key::size+1];
    // Code below assume that we use primary key index.
    ASSERT_ALWAYS(strcmp(m_indexName, "PRIMARY")==0);
    /* Tables are alway sorted on all columns. Using these bounds,
     we therefore get m_upperBoundRowNo - m_lowerBoundRowNo +1 rows.*/
    const Key& lowKey = *new Key(Row(m_lowerBoundRowNo).getPrimaryKey());
    const Key& highKey = *new Key(Row(m_upperBoundRowNo).getPrimaryKey());
    
    for(int i = 0; i<Key::size; i++){
      low[i] = lowKey.makeConstOperand(builder, i);
      high[i] = highKey.makeConstOperand(builder, i);
    }
    low[Key::size] = NULL;
    high[Key::size] = NULL;

    NdbQueryOptions options;
    options.setOrdering(m_ordering);
    const NdbQueryIndexBound bound(low, high);
    const NdbQueryIndexScanOperationDef* opDef 
      = builder.scanIndex(index, &tab, &bound, &options);
    m_operationDef = opDef;
    ASSERT_ALWAYS(m_operationDef!=NULL);
    m_rowFound = new bool[m_query.getTableSize()];
    for(int i = 0; i<m_query.getTableSize(); i++){
      m_rowFound[i] = false;
    }
  }

  void IndexScanOperation::submit(){
    NdbQueryOperation* queryOp 
      = m_query.getOperation(m_operationId);
    queryOp->setResultRowRef(m_query.getNdbRecord(), 
                             m_resultCharPtr,
                             NULL);

    // Negative testing.
    if (m_ordering != NdbQueryOptions::ScanOrdering_unordered){
      ASSERT_ALWAYS(queryOp
                    ->setOrdering(NdbQueryOptions::ScanOrdering_ascending) != 0);
      ASSERT_ALWAYS(queryOp->getQuery().getNdbError().code == 
                    QRY_SCAN_ORDER_ALREADY_SET);

      ASSERT_ALWAYS(queryOp->setParallelism(1) != 0);
      ASSERT_ALWAYS(queryOp->getQuery().getNdbError().code == 
                    QRY_SEQUENTIAL_SCAN_SORTED);
    }
  }

  void IndexScanOperation::verifyOwnRow(){
    bool found = false;
    for(int i = m_lowerBoundRowNo; i<=m_upperBoundRowNo; i++){
      const Row row(i);
      if(row == *m_resultPtr){
        found = true;
        if(m_rowFound[i]){
          ndbout << "Root index scan operation: " 
                 << *m_resultPtr
                 << "appeared twice." << endl;
          ASSERT_ALWAYS(false);
        }
        m_rowFound[i] = true;
      }
    }
    if(!found){
      ndbout << "Root index scan operation. Unexpected row: " 
             << *m_resultPtr << endl;
      ASSERT_ALWAYS(false);
    }else{
      if(m_hasPreviousRow){
        switch(m_ordering){
        case NdbQueryOptions::ScanOrdering_ascending:
          if(!lessOrEqual(m_previousRow, *m_resultPtr)){
            ndbout << "Error in result ordering. Did not expect row "
                   <<  *m_resultPtr
                   << " now." << endl;
            ASSERT_ALWAYS(false);
          }
          break;
        case NdbQueryOptions::ScanOrdering_descending:
          if(lessOrEqual(m_previousRow, *m_resultPtr)){
            ndbout << "Error in result ordering. Did not expect row "
                   <<  *m_resultPtr
                   << " now." << endl;
            ASSERT_ALWAYS(false);
          }
          break;
        case NdbQueryOptions::ScanOrdering_unordered:
          break;
        default:
          ASSERT_ALWAYS(false);
        }
      }
      m_hasPreviousRow = true;
      m_previousRow = *m_resultPtr;
      ndbout << "Root index scan operation. Got row: " 
             << *m_resultPtr
             << " as expected." << endl;
    }
  }

  // Misc. functions.

  /** Make and populate SQL table.*/
  void makeTable(MYSQL& mysql, const char* name, int rowCount){
    char cmd[500];
    char piece[500];
    sprintf(cmd, "drop table if exists %s", name);
    mySQLExec(mysql, cmd);
    sprintf(cmd, "create table %s (\n", name);
    for(int i = 0; i<Row::size; i++){
      sprintf(piece, "   %s %s NOT NULL,\n", colName(i), Row::getType(i));
      strcat(cmd, piece);
    }
    strcat(cmd, "   PRIMARY KEY(");
    for(int i = 0; i<Key::size; i++){
      strcat(cmd, colName(i));
      if(i<Key::size - 1){
        strcat(cmd, ",");
      }else{
        strcat(cmd, "),\n");
      }
    }
    strcat(cmd, "   UNIQUE KEY UIX (");
    for(int i = 0; i<Key::size; i++){
      strcat(cmd, colName(Row::getIndexKeyColNo(i)));
      if(i<Key::size - 1){
        strcat(cmd, ",");
      }else{
        strcat(cmd, "))\n");
      }
    }
    strcat(cmd, "ENGINE=NDB");
    mySQLExec(mysql, cmd);
    for(int i = 0; i<rowCount; i++){
      Row::makeSQLValues(piece, i);
      sprintf(cmd, "insert into %s %s", name, piece);
      mySQLExec(mysql, cmd);
    }
  };


  /* Execute a test for a give operation graph.*/
  void runCase(MYSQL& mysql, 
               Ndb& ndb, 
               Query& query,
               const char* tabName, 
               int tabSize,
               int rowCount){
    // Populate test table.
    makeTable(mysql, tabName, tabSize);
    NdbDictionary::Dictionary*  const dict = ndb.getDictionary();
    const NdbDictionary::Table* const tab = dict->getTable(tabName);    
    ASSERT_ALWAYS(tab!=NULL);
    // Build generic query definition.
    query.build(*tab, tabSize);
    NdbTransaction* trans = ndb.startTransaction();
    ASSERT_ALWAYS(trans!=NULL);
    // instantiate query within transaction.
    query.submit(*trans);
    ASSERT_ALWAYS(trans->execute(NoCommit)==0);
    // Verify each row and total number of rows.
    for(int i = 0; i<rowCount; i++){
      ASSERT_ALWAYS(query.nextResult() ==  NdbQuery::NextResult_gotRow);
      query.verifyRow();
      if(false && i>3){ 
        // Enable to test close of incomplete scan.
        query.close();
        ndb.closeTransaction(trans);
        return;
      }
    }
    ASSERT_ALWAYS(query.nextResult() ==  NdbQuery::NextResult_scanComplete);
    ndb.closeTransaction(trans);
  }

  /** Run a set of test cases.*/
  void runTestSuite(MYSQL& mysql, Ndb& ndb){
    for(int caseNo = 0; caseNo<7; caseNo++){
      ndbout << endl << "Running test case " << caseNo << endl;

      char tabName[20];
      sprintf(tabName, "t%d", caseNo);
      Query query(ndb);
      
      switch(caseNo){
      case 0:
        {
          LookupOperation root(query);
          LookupOperation child(query, &root);
          LookupOperation child2(query, &root);
          runCase(mysql, ndb, query, tabName, 1, 1);
        }
        break;
      case 1:
        {
          IndexLookupOperation root(query, "UIX");
          IndexLookupOperation child(query, "UIX", &root);
          runCase(mysql, ndb, query, tabName, 5, 1);
        }
        break;
      case 2:
        {
          IndexScanOperation root(query, "PRIMARY", 2, 4,
                                   NdbQueryOptions::ScanOrdering_unordered);
          LookupOperation child(query, &root);
          IndexLookupOperation child2(query, "UIX", &child);
          LookupOperation child3(query, &child);
          runCase(mysql, ndb, query, tabName, 5, 3);
        }
        break;
      case 3:
        {
          TableScanOperation root(query);
          LookupOperation child(query, &root);
          runCase(mysql, ndb, query, tabName, 5, 5);
        }
        break;
      case 4:
        {
          TableScanOperation root(query);
          IndexLookupOperation child1(query, "UIX", &root);
          LookupOperation child2(query, &child1);
          IndexLookupOperation child3(query, "UIX", &child2);
          LookupOperation child1_2(query, &root);
          LookupOperation child2_2(query, &child1_2);
          runCase(mysql, ndb, query, tabName, 10, 10);
        }
        break;
      case 5:
        {
          IndexScanOperation root(query, "PRIMARY", 0, 10, 
                                 NdbQueryOptions::ScanOrdering_descending);
          LookupOperation child(query, &root);
          runCase(mysql, ndb, query, tabName, 10, 10);
        }
        break;
      case 6:
        {
          TableScanOperation root(query, 3);
          LookupOperation child(query, &root);
          runCase(mysql, ndb, query, tabName, 5, 3);
        }
        break;
#if 0
      default:
        //case 6:
        {
          IndexScanOperation root(query, "PRIMARY", 0, 1000, 
                                 NdbQueryOptions::ScanOrdering_descending);
          LookupOperation child(query, &root);
          runCase(mysql, ndb, query, tabName, 10*(caseNo-6), 10*(caseNo-6));
        }
        break;
#endif
      }
    }
  }

};


using namespace SPJSanityTest;

int main(int argc, char* argv[]){
  if(argc!=4){
    ndbout << "Usage: " << argv[0] 
           << " <mysql IP address> <mysql port> <cluster connect string>" 
           << endl;
    return NDBT_ProgramExit(NDBT_FAILED);
  }
  const char* const host=argv[1];
  const int port = atoi(argv[2]);
  const char* const connectString = argv[3];

  NDB_INIT(argv[0]);
  MYSQL mysql;
  ASSERT_ALWAYS(mysql_init(&mysql));
  if(!mysql_real_connect(&mysql, host, "root", "", "",
                         port, NULL, 0)){
    printMySQLError(mysql, "mysql_real_connect() failed:");
    return NDBT_ProgramExit(NDBT_FAILED);
  }
  mySQLExec(mysql, "create database if not exists CK_DB");
  mySQLExec(mysql, "use CK_DB");
  {
    Ndb_cluster_connection con(connectString);
    if(con.connect(12, 5, 1) != 0){
      ndbout << "Unable to connect to management server." << endl;
      return NDBT_ProgramExit(NDBT_FAILED);
    }
    
    int res = con.wait_until_ready(30,30);
    if (res != 0){
      ndbout << "Cluster nodes not ready in 30 seconds." << endl;
      return NDBT_ProgramExit(NDBT_FAILED);
    }
    Ndb ndb(&con, "CK_DB");
    if(ndb.init() != 0){
      ERR(ndb.getNdbError());
      return NDBT_ProgramExit(NDBT_FAILED);
    }
    runTestSuite(mysql, ndb);
  } // Must call ~Ndb_cluster_connection() before ndb_end().
  ndb_end(0);
  return 0;
}

// Explicit template instantiations.
template class Vector<Operation*>;
template class GenericRow<IntField>;
template class GenericKey<IntField>;
template class GenericRow<StrField>;
template class GenericKey<StrField>;
