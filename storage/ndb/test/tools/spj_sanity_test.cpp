#include <assert.h>
#include <mysql.h>
#include <mysqld_error.h>

#include <ndb_global.h>
#include <ndb_opts.h>
#include <NDBT.hpp>
#include <NdbApi.hpp>
#include <NdbQueryOperation.hpp>
#include <NdbQueryBuilder.hpp>

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

namespace SPJSanityTest{
  class RowInt;

  static bool lessOrEqual(const RowInt& a, const RowInt& b);

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
    ndbout << stmt << endl;
    if(mysql_query(&mysql, stmt) != 0){
      ndbout << "Error executing '" << stmt << "' : ";
      printMySQLError(mysql);
    }
  }

  template <typename Row>
  class Query;

  /** Class representing a single NdbQueryOperation. 'Row'
   * is a template argument, to allow different table defintions.*/
  template <typename Row>
  class Operation{
  public:

    explicit Operation(class Query<Row>& query, Operation* parent);

    virtual ~Operation(){}

    //protected: FIXME
  public:
    friend class Query<Row>;
    /** Enclosing NdbQuery.*/
    Query<Row>& m_query;
    /** Optional parent operation.*/
    const Operation<Row>* m_parent;
    Vector<Operation<Row>*> m_children;
    const NdbQueryOperationDef* m_operationDef;
    // For now, only setResultRowRef() style result retrieval is tested.
    const Row* m_resultPtr;
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

  template <typename Row>
  class Query{
  public:

    explicit Query(Ndb& ndb);

    /** Build query definition.*/
    void build(const NdbDictionary::Table& tab, int tableSize);

    /** Execute within transaction.*/
    void submit(NdbTransaction& transaction);

    void submitOperation(Operation<Row>& operation) const;

    void setRoot(Operation<Row>& root){ m_root = &root;}

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
    NdbQueryBuilder m_builder;
    Operation<Row>* m_root;
    const NdbQueryDef* m_queryDef;
    NdbQuery* m_query;
    Uint32 m_operationCount;
    int m_tableSize;
    const NdbRecord* m_ndbRecord;
  };

  
  template <typename Row, typename Key>
  class LookupOperation: public Operation<Row>{
  public:
    explicit LookupOperation(Query<Row>& query, 
                             Operation<Row>* parent = NULL);
    virtual void verifyOwnRow();

    /** Set up result retrieval before execution.*/
    virtual void submit();
  protected:
    virtual void buildThis(NdbQueryBuilder& builder, 
                           const NdbDictionary::Table& tab);
  };

  template <typename Row, typename Key>
  class IndexLookupOperation: public Operation<Row>{
  public:
    explicit IndexLookupOperation(Query<Row>& query, 
                                  const char* indexName,
                                  Operation<Row>* parent = NULL);
    virtual void verifyOwnRow();

    /** Set up result retrieval before execution.*/
    virtual void submit();
  protected:
    virtual void buildThis(NdbQueryBuilder& builder, 
                           const NdbDictionary::Table& tab);
  private:
    const char* const m_indexName;
  };

  template <typename Row>
  class TableScanOperation: public Operation<Row>{
  public:

    explicit TableScanOperation(Query<Row>& query, int lessThanRow=-1);

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

  template <typename Row, typename Key>
  class IndexScanOperation: public Operation<Row>{
  public:

    explicit IndexScanOperation(Query<Row>& query,
                                const char* indexName,
                                int lowerBoundRowNo, 
                                int upperBoundRowNo,
                                NdbScanOrdering ordering);

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
    NdbScanOrdering m_ordering;
    /** Previous row, for verifying ordering.*/
    Row m_previousRow;
    /** True from the second row and onwards.*/
    bool m_hasPreviousRow;
  };


  // Query methods.

  template <typename Row>
  Query<Row>::Query(Ndb& ndb):
    m_ndb(ndb),
    m_builder(ndb),
    m_root(NULL),
    m_queryDef(NULL),
    m_query(NULL),
    m_operationCount(0),
    m_tableSize(-1),
    m_ndbRecord(NULL)
  {}

  template <typename Row>
  void Query<Row>::build(const NdbDictionary::Table& tab, int tableSize){
    m_tableSize = tableSize;
    m_root->build(m_builder, tab);
    m_queryDef = m_builder.prepare();
    m_ndbRecord = tab.getDefaultRecord();
  }

  template <typename Row>
  void Query<Row>::submit(NdbTransaction& transaction){
    m_query = transaction.createQuery(m_queryDef);
    ASSERT_ALWAYS(m_query!=NULL);
    submitOperation(*m_root);
  }

  template <typename Row>
  void Query<Row>::submitOperation(Operation<Row>& operation) const{
    // Do a depth first traversal of the operations graph.
    operation.submit();
    for(Uint32 i = 0; i<operation.m_children.size(); i++){
      submitOperation(*operation.m_children[i]);
    }
  }

  // Operation methods.
  template <typename Row>
  Operation<Row>::Operation(class Query<Row>& query, 
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

  template <typename Row>
  void Operation<Row>::build(NdbQueryBuilder& builder,
                                  const NdbDictionary::Table& tab){
    m_operationId = m_query.allocOperationId();
    buildThis(builder, tab);
    ASSERT_ALWAYS(builder.getNdbError().code==0);
    for(Uint32 i = 0; i<m_children.size(); i++){
      m_children[i]->build(builder, tab);
    }
  }

  template <typename Row>
  void Operation<Row>::verifyRow(){
    verifyOwnRow();
    for(Uint32 i = 0; i<m_children.size(); i++){
      m_children[i]->verifyRow();
    }
  }

  typedef const char* constCharPtr;

  template <typename Row>
  void Operation<Row>::compareRows(const char* text, 
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

  template <typename Row, typename Key>
  LookupOperation<Row, Key>
  ::LookupOperation(Query<Row>& query, 
                    Operation<Row>* parent):
    Operation<Row>(query, parent){
  }

  template <typename Row, typename Key>
  void LookupOperation<Row, Key>::buildThis(NdbQueryBuilder& builder,
                                            const NdbDictionary::Table& tab){
    NdbQueryOperand* keyOperands[Key::size+1];
    if(Operation<Row>::m_parent==NULL){
      const Key key = Row(0).getPrimaryKey();
      for(int i = 0; i<Key::size; i++){
        keyOperands[i] = key.makeConstOperand(builder, i);
      }
    }else{
      for(int i = 0; i<Key::size; i++){
        keyOperands[i] = 
          builder.linkedValue(Operation<Row>::m_parent->m_operationDef, 
                              colName(Row::getForeignKeyColNo(
                                        Operation<Row>::m_childNo, i)));
        ASSERT_ALWAYS(keyOperands[i]!=NULL);
        /*Row::makeLinkedKey(builder, keyOperands, 
                         Operation<Row>::m_parent->m_operationDef, 
                         Operation<Row>::m_childNo);*/
      }
    }
    keyOperands[Key::size] = NULL;
    Operation<Row>::m_operationDef = builder.readTuple(&tab, keyOperands);
  }

  template <typename Row, typename Key>
  void LookupOperation<Row, Key>::submit(){
    NdbQueryOperation* queryOp 
      = Operation<Row>::m_query.getOperation(Operation<Row>::m_operationId);
    queryOp->setResultRowRef(Operation<Row>::m_query.getNdbRecord(), 
                             //*ptr,
                             reinterpret_cast<const char*&>(Operation<Row>::m_resultPtr),
                             NULL);
  }

  template <typename Row, typename Key>
  void LookupOperation<Row, Key>::verifyOwnRow(){
    if(Operation<Row>::m_parent==NULL){
      const Row expected(0);
      Operation<Row>::compareRows("lookup root operation",
                                  &expected, 
                                  Operation<Row>::m_resultPtr);
    }else{
      NdbQueryOperation* queryOp 
        = Operation<Row>::m_query
        .getOperation(Operation<Row>::m_operationId);
      if(!queryOp->getParentOperation(0)->isRowNULL()){
        const Key key = 
          Operation<Row>::m_parent->m_resultPtr
          ->getForeignKey(Operation<Row>::m_childNo);
        bool found = false;
        for(int i = 0; i<Operation<Row>::m_query.getTableSize(); i++){
          const Row row(i);
          if(row.getPrimaryKey() == key){
            found = true;
            Operation<Row>::compareRows("lookup child operation",
                                        &row, 
                                        Operation<Row>::m_resultPtr);
          }
        }
        if(!found && !queryOp->isRowNULL()){
          Operation<Row>::compareRows("lookup child operation",
                                      NULL,
                                      Operation<Row>::m_resultPtr);
        }
      }
    }
  }

  // IndexLookupOperation methods.

  template <typename Row, typename Key>
  IndexLookupOperation<Row, Key>
  ::IndexLookupOperation(Query<Row>& query, 
                         const char* indexName, 
                         Operation<Row>* parent):
    Operation<Row>(query, parent),
    m_indexName(indexName){
  }

  template <typename Row, typename Key>
  void IndexLookupOperation<Row, Key>
  ::buildThis(NdbQueryBuilder& builder,
              const NdbDictionary::Table& tab){
    const NdbDictionary::Dictionary* const dict 
      = Operation<Row>::m_query.getDictionary();
    char fullName[200];
    sprintf(fullName, "%s$unique", m_indexName);
    const NdbDictionary::Index* const index
      = dict->getIndex(fullName, tab.getName());
    ASSERT_ALWAYS(index!=NULL);

    NdbQueryOperand* keyOperands[Key::size+1];
    if(Operation<Row>::m_parent==NULL){
      const Key key = Row(0).getIndexKey();
      for(int i = 0; i<Key::size; i++){
        keyOperands[i] = key.makeConstOperand(builder, i);
      }
    }else{
      for(int i = 0; i<Key::size; i++){
        keyOperands[i] = 
          builder.linkedValue(Operation<Row>::m_parent->m_operationDef,
                              colName(Row::getForeignKeyColNo(
                                        Operation<Row>::m_childNo, i)));
        ASSERT_ALWAYS(keyOperands[i]!=NULL);
      }
      /*Row::makeLinkedKey(builder, keyOperands, 
                         Operation<Row>::m_parent->m_operationDef, 
                         Operation<Row>::m_childNo);*/
    }
    keyOperands[Key::size] = NULL;
    Operation<Row>::m_operationDef = builder.readTuple(index, 
                                                       &tab, 
                                                       keyOperands);
  }

  template <typename Row, typename Key>
  void IndexLookupOperation<Row, Key>::submit(){
    NdbQueryOperation* queryOp 
      = Operation<Row>::m_query.getOperation(Operation<Row>::m_operationId);
    queryOp->setResultRowRef(Operation<Row>::m_query.getNdbRecord(), 
                             //*ptr,
                             reinterpret_cast<const char*&>(Operation<Row>::m_resultPtr),
                             NULL);
  }

  template <typename Row, typename Key>
  void IndexLookupOperation<Row, Key>::verifyOwnRow(){
    if(Operation<Row>::m_parent==NULL){
      const Row expected(0);
      Operation<Row>::compareRows("index lookup root operation",
                                  &expected, 
                                  Operation<Row>::m_resultPtr);
    }else{
      NdbQueryOperation* queryOp 
        = Operation<Row>::m_query
        .getOperation(Operation<Row>::m_operationId);
      if(!queryOp->getParentOperation(0)->isRowNULL()){
        const Key key = 
          Operation<Row>::m_parent->m_resultPtr
          ->getForeignKey(Operation<Row>::m_childNo);
        bool found = false;
        for(int i = 0; i<Operation<Row>::m_query.getTableSize(); i++){
          const Row row(i);
          if(row.getIndexKey() == key){
            found = true;
            Operation<Row>::compareRows("index lookup child operation",
                                        &row, 
                                        Operation<Row>::m_resultPtr);
          }
        }
        if(!found && !queryOp->isRowNULL()){
          Operation<Row>::compareRows("index lookup child operation",
                                      NULL,
                                      Operation<Row>::m_resultPtr);
        }
      }
    }
  }

  // TableScanOperation methods.

  template <typename Row>
  TableScanOperation<Row>
  ::TableScanOperation(Query<Row>& query, int lessThanRow):
    Operation<Row>(query, NULL),
    m_rowFound(NULL),
    m_lessThanRow(lessThanRow){
  }

  template <typename Row>
  void TableScanOperation<Row>::buildThis(NdbQueryBuilder& builder,
                                          const NdbDictionary::Table& tab){
    Operation<Row>::m_operationDef = builder.scanTable(&tab);
    m_rowFound = new bool[Operation<Row>::m_query.getTableSize()];
    for(int i = 0; i<Operation<Row>::m_query.getTableSize(); i++){
      m_rowFound[i] = false;
    }
  }

  template <typename Row>
  void TableScanOperation<Row>::submit(){
    NdbQueryOperation* queryOp 
      = Operation<Row>::m_query.getOperation(Operation<Row>::m_operationId);
    queryOp->setResultRowRef(Operation<Row>::m_query.getNdbRecord(), 
                             //*ptr,
                             reinterpret_cast<const char*&>(Operation<Row>::m_resultPtr),
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

  template <typename Row>
  void TableScanOperation<Row>::verifyOwnRow(){
    bool found = false;
    const int upperBound = 
      m_lessThanRow==-1 ? 
      Operation<Row>::m_query.getTableSize() :
      m_lessThanRow;
    for(int i = 0; i<upperBound; i++){
      //const Row row(i);
      if(Row(i) == *Operation<Row>::m_resultPtr){
        found = true;
        if(m_rowFound[i]){
          ndbout << "Root table scan operation: " 
                 << *Operation<Row>::m_resultPtr
                 << "appeared twice." << endl;
          ASSERT_ALWAYS(false);
        }
        m_rowFound[i] = true;
      }
    }
    if(!found){
      ndbout << "Root table scan operation. Unexpected row: " 
             << *Operation<Row>::m_resultPtr << endl;
      ASSERT_ALWAYS(false);
    }else{
      ndbout << "Root table scan operation. Got row: " 
             << *Operation<Row>::m_resultPtr
             << " as expected." << endl;
    }
  }

    // IndexScanOperation methods.

  template <typename Row, typename Key>
  IndexScanOperation<Row, Key>
  ::IndexScanOperation(Query<Row>& query, 
                       const char* indexName,
                       int lowerBoundRowNo, 
                       int upperBoundRowNo,
                       NdbScanOrdering ordering):
    Operation<Row>(query, NULL),
    m_indexName(indexName),
    m_lowerBoundRowNo(lowerBoundRowNo),
    m_upperBoundRowNo(upperBoundRowNo),
    m_ordering(ordering),
    m_previousRow(0),
    m_hasPreviousRow(false){
  }

  template <typename Row, typename Key>
  void IndexScanOperation<Row, Key>::buildThis(NdbQueryBuilder& builder,
                                               const NdbDictionary::Table& tab){
    const NdbDictionary::Dictionary* const dict 
      = Operation<Row>::m_query.getDictionary();
    const NdbDictionary::Index* const index
      = dict->getIndex(m_indexName, tab.getName());
    ASSERT_ALWAYS(index!=NULL);

    const NdbQueryOperand* low[Key::size+1];
    const NdbQueryOperand* high[Key::size+1];
    // Code below assume that we use primary key index.
    ASSERT_ALWAYS(strcmp(m_indexName, "PRIMARY")==0);
    /* Tables are alway sorted on all columns. Using these bounds,
     we therefore get m_upperBoundRowNo - m_lowerBoundRowNo +1 rows.*/
    const Key lowKey = Row(m_lowerBoundRowNo).getPrimaryKey();
    const Key highKey = Row(m_upperBoundRowNo).getPrimaryKey();
    
    for(int i = 0; i<Key::size; i++){
      low[i] = lowKey.makeConstOperand(builder, i);
      high[i] = highKey.makeConstOperand(builder, i);
    }
    low[Key::size] = NULL;
    high[Key::size] = NULL;

    const NdbQueryIndexBound bound(low, high);
    NdbQueryIndexScanOperationDef* opDef 
      = builder.scanIndex(index, &tab, &bound);
    opDef->setOrdering(m_ordering);
    Operation<Row>::m_operationDef = opDef;
    ASSERT_ALWAYS(Operation<Row>::m_operationDef!=NULL);
    m_rowFound = new bool[Operation<Row>::m_query.getTableSize()];
    for(int i = 0; i<Operation<Row>::m_query.getTableSize(); i++){
      m_rowFound[i] = false;
    }
  }

  template <typename Row, typename Key>
  void IndexScanOperation<Row, Key>::submit(){
    NdbQueryOperation* queryOp 
      = Operation<Row>::m_query.getOperation(Operation<Row>::m_operationId);
    queryOp->setResultRowRef(Operation<Row>::m_query.getNdbRecord(), 
                             //*ptr,
                             reinterpret_cast<const char*&>(Operation<Row>::m_resultPtr),
                             NULL);
  }

  template <typename Row, typename Key>
  void IndexScanOperation<Row, Key>::verifyOwnRow(){
    bool found = false;
    for(int i = m_lowerBoundRowNo; i<=m_upperBoundRowNo; i++){
      //const Row row(i);
      if(Row(i) == *Operation<Row>::m_resultPtr){
        found = true;
        if(m_rowFound[i]){
          ndbout << "Root index scan operation: " 
                 << *Operation<Row>::m_resultPtr
                 << "appeared twice." << endl;
          ASSERT_ALWAYS(false);
        }
        m_rowFound[i] = true;
      }
    }
    if(!found){
      ndbout << "Root index scan operation. Unexpected row: " 
             << *Operation<Row>::m_resultPtr << endl;
      ASSERT_ALWAYS(false);
    }else{
      if(m_hasPreviousRow){
        switch(m_ordering){
        case NdbScanOrdering_ascending:
          if(!lessOrEqual(m_previousRow, *Operation<Row>::m_resultPtr)){
            ndbout << "Error in result ordering. Did not expect row "
                   <<  *Operation<Row>::m_resultPtr
                   << " now." << endl;
            ASSERT_ALWAYS(false);
          }
          break;
        case NdbScanOrdering_descending:
          if(lessOrEqual(m_previousRow, *Operation<Row>::m_resultPtr)){
            ndbout << "Error in result ordering. Did not expect row "
                   <<  *Operation<Row>::m_resultPtr
                   << " now." << endl;
            ASSERT_ALWAYS(false);
          }
          break;
        case NdbScanOrdering_unordered:
          break;
        default:
          ASSERT_ALWAYS(false);
        }
      }
      m_hasPreviousRow = true;
      m_previousRow = *Operation<Row>::m_resultPtr;
      ndbout << "Root index scan operation. Got row: " 
             << *Operation<Row>::m_resultPtr
             << " as expected." << endl;
    }
  }

  // Misc. functions.

  /** Make and populate SQL table.*/
  template <typename Row, typename Key>
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
  template <typename Row, typename Key>
  void runCase(MYSQL& mysql, 
               Ndb& ndb, 
               Query<Row>& query,
               const char* tabName, 
               int tabSize,
               int rowCount){
    // Populate test table.
    makeTable<Row, Key>(mysql, tabName, tabSize);
    NdbQueryBuilder builder(ndb);
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
  template <typename Row, typename Key>
  void runTestSuite(MYSQL& mysql, Ndb& ndb){
    for(int caseNo = 0; caseNo<7; caseNo++){
      ndbout << endl << "Running test case " << caseNo << endl;

      char tabName[20];
      sprintf(tabName, "t%d", caseNo);
      Query<Row> query(ndb);
      
      switch(caseNo){
      case 0:
        {
          LookupOperation<Row, Key> root(query);
          LookupOperation<Row, Key> child(query, &root);
          LookupOperation<Row, Key> child2(query, &root);
          runCase<Row, Key>(mysql, ndb, query, tabName, 1, 1);
        }
        break;
      case 1:
        {
          IndexLookupOperation<Row, Key> root(query, "UIX");
          IndexLookupOperation<Row, Key> child(query, "UIX", &root);
          runCase<Row, Key>(mysql, ndb, query, tabName, 5, 1);
        }
        break;
      case 2:
        {
          IndexScanOperation<Row, Key> root(query, "PRIMARY", 2, 4,
                                            NdbScanOrdering_unordered);
          LookupOperation<Row, Key> child(query, &root);
          IndexLookupOperation<Row, Key> child2(query, "UIX", &child);
          LookupOperation<Row, Key> child3(query, &child);
          runCase<Row, Key>(mysql, ndb, query, tabName, 5, 3);
        }
        break;
      case 3:
        {
          TableScanOperation<Row> root(query);
          LookupOperation<Row, Key> child(query, &root);
          runCase<Row, Key>(mysql, ndb, query, tabName, 5, 5);
        }
        break;
      case 4:
        {
          TableScanOperation<Row> root(query);
          IndexLookupOperation<Row, Key> child1(query, "UIX", &root);
          LookupOperation<Row, Key> child2(query, &child1);
          IndexLookupOperation<Row, Key> child3(query, "UIX", &child2);
          LookupOperation<Row, Key> child1_2(query, &root);
          LookupOperation<Row, Key> child2_2(query, &child1_2);
          runCase<Row, Key>(mysql, ndb, query, tabName, 10, 10);
        }
        break;
      case 5:
        {
          IndexScanOperation<Row, Key> root(query, "PRIMARY", 0, 10, 
                                            NdbScanOrdering_descending);
          LookupOperation<Row, Key> child(query, &root);
          runCase<Row, Key>(mysql, ndb, query, tabName, 10, 10);
        }
        break;
      case 6:
        {
          TableScanOperation<Row> root(query, 3);
          LookupOperation<Row, Key> child(query, &root);
          runCase<Row, Key>(mysql, ndb, query, tabName, 5, 3);
        }
        break;
#if 0
      default:
        //case 6:
        {
          IndexScanOperation<Row, Key> root(query, "PRIMARY", 0, 1000, 
                                            NdbScanOrdering_descending);
          LookupOperation<Row, Key> child(query, &root);
          runCase<Row, Key>(mysql, ndb, query, tabName, 10*(caseNo-6), 10*(caseNo-6));
        }
        break;
#endif
      }
    }
  }

  /* Concrete Key class.*/
  class KeyInt{
  public:
    static const int size = 2;
    int m_values[size];

    NdbConstOperand* makeConstOperand(NdbQueryBuilder& builder, 
                                      int fieldNo) const {
      ASSERT_ALWAYS(fieldNo<size);
      return builder.constValue(m_values[fieldNo]);
    }
  };

  /* Concrete Row class.*/
  class RowInt{
  public:
    static const int size = 4;

    int m_values[size];

    explicit RowInt(int rowNo){
      /* Attribute values are chosen such that rows are sorted on 
       * all attribtes, and that any pair of consecutive columns can be
       * used as a foreign key to the table itself.*/
      for(int i = 0; i<size; i++){
        m_values[i] = i+rowNo;
      }
    }

    static const char *getType(int colNo){
      return "INT";
    }
   
    static void makeSQLValues(char* buffer, int rowNo){
      const RowInt row(rowNo);
      sprintf(buffer, "values(");
      char* tail = buffer+strlen(buffer);
      for(int i = 0; i<size; i++){
        if(i<size-1){
          sprintf(tail, "%d,", row.m_values[i]);
        }else{
          sprintf(tail, "%d)", row.m_values[i]);
        }
        tail = buffer+strlen(buffer);
      }
    } 

    KeyInt getPrimaryKey() const;
    
    KeyInt getIndexKey() const;
    
    KeyInt getForeignKey(int keyNo) const;

    void makeLessThanCond(NdbScanFilter& scanFilter){
      ASSERT_ALWAYS(scanFilter.lt(0, static_cast<Uint64>(m_values[0]))==0); 
    }

    /** Get the row column number that corresponds to the n'th column 
     * of the index.*/
    static int getIndexKeyColNo(int indexCol);

    /** Get the row column number that corresponds to the n'th column 
     * of the m'th foreign key..*/
    static int getForeignKeyColNo(int keyNo, int keyCol);
  };

  KeyInt RowInt::getPrimaryKey() const {
    KeyInt key;
    for(int i = 0; i<KeyInt::size; i++){
      key.m_values[i] = m_values[i];    
    }
    return key;
  }

  KeyInt RowInt::getIndexKey() const {
    return getForeignKey(1);
  }

  KeyInt RowInt::getForeignKey(int keyNo) const {
    ASSERT_ALWAYS(keyNo<=1);
    KeyInt key;
    for(int i = 0; i<KeyInt::size; i++){
      key.m_values[i] = m_values[getForeignKeyColNo(keyNo,i)];    
    }
    return key;
  }

  int RowInt::getIndexKeyColNo(int indexCol){
    return getForeignKeyColNo(1, indexCol);
  }

  int RowInt::getForeignKeyColNo(int keyNo, int keyCol){
    ASSERT_ALWAYS(keyNo<RowInt::size-KeyInt::size);
    ASSERT_ALWAYS(keyCol<KeyInt::size);
    return size-KeyInt::size-keyNo+keyCol;
  }

  static bool operator==(const RowInt& a, const RowInt& b){
    for(int i = 0; i<RowInt::size; i++){
      if(a.m_values[i]!=b.m_values[i]){
        return false;
      }
    }
    return true;
  }

  static bool operator==(const KeyInt& a, const KeyInt& b){
    for(int i = 0; i<KeyInt::size; i++){
      if(a.m_values[i]!=b.m_values[i]){
        return false;
      }
    }
    return true;
  }

  /** Returns true if key of a <= key of b.*/
  static bool lessOrEqual(const RowInt& a, const RowInt& b){
    for(int i = 0; i<KeyInt::size; i++){
      if(a.m_values[i]>b.m_values[i]){
        return false;
      }
    }
    return true;
  }

  static NdbOut& operator<<(NdbOut& out, const RowInt& row){
    out << "{";
    for(int i = 0; i<RowInt::size; i++){
      out << row.m_values[i];
      if(i<RowInt::size-1){
        out << ", ";
      }
    }
    out << "}";
    return out;
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
    runTestSuite<RowInt, KeyInt>(mysql, ndb);
  } // Must call ~Ndb_cluster_connection() before ndb_end().
  ndb_end(0);
  return 0;
}

// Explicit template instantiations.
template class Vector<Operation<RowInt>*>;
template class Operation<RowInt>;
template class LookupOperation<RowInt, KeyInt>;
template class IndexLookupOperation<RowInt, KeyInt>;
template class TableScanOperation<RowInt>;
template class IndexScanOperation<RowInt, KeyInt>;
template class Query<RowInt>;
template void makeTable<RowInt, KeyInt>(MYSQL& mysql, 
                                        const char* name, 
                                        int rowCount);
template void runTestSuite<RowInt, KeyInt>(MYSQL& mysql, Ndb& ndb);
template void runCase<RowInt, KeyInt>(MYSQL& mysql, 
                                      Ndb& ndb, 
                                      Query<RowInt>& query,
                                      const char* tabName, 
                                      int tabSize,
                                      int rowCount);
