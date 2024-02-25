/*
   Copyright (c) 2004, 2023, Oracle and/or its affiliates.

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

#include <NDBT_Test.hpp>
#include <NDBT_ReturnCodes.h>
#include <HugoTransactions.hpp>
#include <UtilTransactions.hpp>
#include <NdbRestarter.hpp>

static Uint32 max_dks = 0;
static const Uint32 MAX_FRAGS=48 * 8 * 4; // e.g. 48 nodes, 8 frags/node, 4 replicas
static Uint32 frag_ng_mappings[MAX_FRAGS];
static const char* DistTabName= "DistTest";
static const char* DistTabDKeyCol= "DKey";
static const char* DistTabPKey2Col= "PKey2";
static const char* DistTabResultCol= "Result";
static const char* DistIdxName= "ResultIndex";

static
int
run_drop_table(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbDictionary::Dictionary* dict = GETNDB(step)->getDictionary();
  dict->dropTable(ctx->getTab()->getName());
  return 0;
}

static
int
setNativePartitioning(Ndb* ndb, NdbDictionary::Table& tab, int when, void* arg)
{
  switch(when){
  case 0: // Before
    break;
  case 1: // After
    return 0;
  default:
    return 0;
  }

  /* Use rand to choose one of the native partitioning schemes */
  const Uint32 rType= rand() % 3;
  Uint32 fragType= -1;
  switch(rType)
  {
  case 0 :
    fragType = NdbDictionary::Object::DistrKeyHash;
    break;
  case 1 :
    fragType = NdbDictionary::Object::DistrKeyLin;
    break;
  case 2:
    fragType = NdbDictionary::Object::HashMapPartition;
    break;
  }

  ndbout << "Setting fragment type to " << fragType << endl;
  tab.setFragmentType((NdbDictionary::Object::FragmentType)fragType);
  return 0;
}


static
int
add_distribution_key(Ndb* ndb, NdbDictionary::Table& tab, int when, void* arg)
{
  switch(when){
  case 0: // Before
    break;
  case 1: // After
    return 0;
  default:
    return 0;
  }

  /* Choose a partitioning type */
  setNativePartitioning(ndb, tab, when, arg);

  int keys = tab.getNoOfPrimaryKeys();
  Uint32 dks = (2 * keys + 2) / 3; dks = (dks > max_dks ? max_dks : dks);
  
  for(int i = 0; i<tab.getNoOfColumns(); i++)
    if(tab.getColumn(i)->getPrimaryKey() && 
       tab.getColumn(i)->getCharset() != 0)
      keys--;
  
  Uint32 max = NDB_MAX_NO_OF_ATTRIBUTES_IN_KEY - tab.getNoOfPrimaryKeys();

  if(max_dks < max)
    max = max_dks;
  
  if(keys <= 1 && max > 0)
  {
    dks = 1 + (rand() % max);
    ndbout_c("%s pks: %d dks: %d", tab.getName(), keys, dks);
    while(dks--)
    {
      NdbDictionary::Column col;
      BaseString name;
      name.assfmt("PK_DK_%d", dks);
      col.setName(name.c_str());
      if((rand() % 100) > 50)
      {
	col.setType(NdbDictionary::Column::Unsigned);
	col.setLength(1); 
      }
      else
      {
	col.setType(NdbDictionary::Column::Varbinary);
	col.setLength(1+(rand() % 25));
      }
      col.setNullable(false);
      col.setPrimaryKey(true);
      col.setDistributionKey(true);
      tab.addColumn(col);
    }
  } 
  else 
  {
    for(int i = 0; i<tab.getNoOfColumns(); i++)
    {
      NdbDictionary::Column* col = tab.getColumn(i);
      if(col->getPrimaryKey() && col->getCharset() == 0)
      {
	if((int)dks >= keys || (rand() % 100) > 50)
	{
	  col->setDistributionKey(true);
	  dks--;
	}
	keys--;
      }
    }
  }

  ndbout << (NDBT_Table&)tab << endl;

  return 0;
}


static
int
setupUDPartitioning(Ndb* ndb, NdbDictionary::Table& tab)
{
  NdbRestarter restarter;
  Vector<int> node_groups;
  int max_alive_replicas;
  if (restarter.getNodeGroups(node_groups, &max_alive_replicas) == -1)
  {
    return -1;
  }

  const Uint32 numNgs = node_groups.size();

  // Assume at least one node group had all replicas alive.
  const Uint32 numReplicas = max_alive_replicas;

  /**
   * The maximum number of partitions that may be defined explicitly
   * for any NDB table is =
   * 8 * [number of LDM threads] * [number of node groups]
   * In this case, we consider the number of LDM threads to be 1
   * (min. no of LDMs). This calculated number of partitions works for
   * higher number of LDMs as well.
   */
  const Uint32 numFragsPerNode = (rand() % (8 / numReplicas)) + 1;
  const Uint32 numPartitions = numReplicas * numNgs * numFragsPerNode;

  tab.setFragmentType(NdbDictionary::Table::UserDefined);
  tab.setFragmentCount(numPartitions);
  tab.setPartitionBalance(NdbDictionary::Object::PartitionBalance_Specific);
  for (Uint32 i = 0; i < numPartitions; i++)
  {
    frag_ng_mappings[i] = node_groups[i % numNgs];
  }
  tab.setFragmentData(frag_ng_mappings, numPartitions);

  return 0;
}

static
int
setUserDefPartitioning(Ndb* ndb, NdbDictionary::Table& tab, int when, void* arg)
{
  switch(when){
  case 0: // Before
    break;
  case 1: // After
    return 0;
  default:
    return 0;
  }
  
  setupUDPartitioning(ndb, tab);

  ndbout << (NDBT_Table&)tab << endl;

  return 0;
}

static
int
one_distribution_key(Ndb* ndb, NdbDictionary::Table& tab, int when, void* arg)
{
  switch(when){
  case 0: // Before
    break;
  case 1: // After
    return 0;
  default:
    return 0;
  }

  setNativePartitioning(ndb, tab, when, arg);

  int keys = tab.getNoOfPrimaryKeys();
  int dist_key_no = rand()% keys;
  
  for(int i = 0; i<tab.getNoOfColumns(); i++)
  {
    if(tab.getColumn(i)->getPrimaryKey())
    {
      if (dist_key_no-- == 0)
      {
        tab.getColumn(i)->setDistributionKey(true);
      }
      else
      {
        tab.getColumn(i)->setDistributionKey(false);
      }
    }
  }
  ndbout << (NDBT_Table&)tab << endl;
  
  return 0;
}

static 
const NdbDictionary::Table*
create_dist_table(Ndb* pNdb, 
                  bool userDefined)
{
  NdbDictionary::Dictionary* dict= pNdb->getDictionary();

  do {
    NdbDictionary::Table tab;
    tab.setName(DistTabName);
    
    if (userDefined)
    {
      setupUDPartitioning(pNdb, tab);
    }
    else
    {
      setNativePartitioning(pNdb, tab, 0, 0);
    }
    
    NdbDictionary::Column dk;
    dk.setName(DistTabDKeyCol);
    dk.setType(NdbDictionary::Column::Unsigned);
    dk.setLength(1);
    dk.setNullable(false);
    dk.setPrimaryKey(true);
    dk.setPartitionKey(true);
    tab.addColumn(dk);

    NdbDictionary::Column pk2;
    pk2.setName(DistTabPKey2Col);
    pk2.setType(NdbDictionary::Column::Unsigned);
    pk2.setLength(1);
    pk2.setNullable(false);
    pk2.setPrimaryKey(true);
    pk2.setPartitionKey(false);
    tab.addColumn(pk2);

    NdbDictionary::Column result;
    result.setName(DistTabResultCol);
    result.setType(NdbDictionary::Column::Unsigned);
    result.setLength(1);
    result.setNullable(true);
    result.setPrimaryKey(false);
    tab.addColumn(result);

    dict->dropTable(tab.getName());
    if(dict->createTable(tab) == 0)
    {
      ndbout << (NDBT_Table&)tab << endl;

      do {
        /* Primary key index */
        NdbDictionary::Index idx;
        idx.setType(NdbDictionary::Index::OrderedIndex);
        idx.setLogging(false);
        idx.setTable(DistTabName);
        idx.setName("PRIMARY");
        idx.addColumnName(DistTabDKeyCol);
        idx.addColumnName(DistTabPKey2Col);
      
        dict->dropIndex("PRIMARY",
                        tab.getName());
        
        if (dict->createIndex(idx) == 0)
        {
          ndbout << "Primary Index created successfully" << endl;          
          break;
        }
        ndbout << "Primary Index create failed with " << 
          dict->getNdbError().code << 
          " retrying " << endl;
      } while (0);

      do {
        /* Now the index on the result column */
        NdbDictionary::Index idx;
        idx.setType(NdbDictionary::Index::OrderedIndex);
        idx.setLogging(false);
        idx.setTable(DistTabName);
        idx.setName(DistIdxName);
        idx.addColumnName(DistTabResultCol);
      
        dict->dropIndex(idx.getName(),
                        tab.getName());
        
        if (dict->createIndex(idx) == 0)
        {
          ndbout << "Index on Result created successfully" << endl;          
          return dict->getTable(tab.getName());
        }
        ndbout << "Index create failed with " << 
          dict->getNdbError().code << endl;
      } while (0);
    }
  } while (0);
  return 0;
}

static int
run_create_table(NDBT_Context* ctx, NDBT_Step* step)
{
  /* Create table, optionally with extra distribution keys
   * or UserDefined partitioning
   */
  max_dks = ctx->getProperty("distributionkey", (unsigned)0);
  bool userDefined = ctx->getProperty("UserDefined", (unsigned) 0);

  if(NDBT_Tables::createTable(GETNDB(step), 
			      ctx->getTab()->getName(), 
			      false, false, 
			      max_dks?
                              add_distribution_key:
                              userDefined? 
                              setUserDefPartitioning :
                              setNativePartitioning) == NDBT_OK)
  {
    return NDBT_OK;
  }

  if(GETNDB(step)->getDictionary()->getNdbError().code == 745)
    return NDBT_OK;

  return NDBT_FAILED;
}

static int
run_create_table_smart_scan(NDBT_Context* ctx, NDBT_Step* step)
{
  if(NDBT_Tables::createTable(GETNDB(step), 
			      ctx->getTab()->getName(), 
			      false, false, 
			      one_distribution_key) == NDBT_OK)
  {
    return NDBT_OK;
  }

  if(GETNDB(step)->getDictionary()->getNdbError().code == 745)
    return NDBT_OK;

  return NDBT_FAILED;
}

static int
run_create_pk_index(NDBT_Context* ctx, NDBT_Step* step){
  bool orderedIndex = ctx->getProperty("OrderedIndex", (unsigned)0);

  Ndb* pNdb = GETNDB(step);
  const NdbDictionary::Table *pTab = 
    pNdb->getDictionary()->getTable(ctx->getTab()->getName());
  
  if(!pTab)
    return NDBT_OK;
  
  bool logged = ctx->getProperty("LoggedIndexes", orderedIndex ? 0 : 1);

  BaseString name;
  name.assfmt("IND_%s_PK_%c", pTab->getName(), orderedIndex ? 'O' : 'U');
  
  // Create index    
  if (orderedIndex)
    ndbout << "Creating " << ((logged)?"logged ": "temporary ") << "ordered index "
	   << name.c_str() << " (";
  else
    ndbout << "Creating " << ((logged)?"logged ": "temporary ") << "unique index "
	   << name.c_str() << " (";

  NdbDictionary::Index pIdx(name.c_str());
  pIdx.setTable(pTab->getName());
  if (orderedIndex)
    pIdx.setType(NdbDictionary::Index::OrderedIndex);
  else
    pIdx.setType(NdbDictionary::Index::UniqueHashIndex);
  for (int c = 0; c< pTab->getNoOfColumns(); c++){
    const NdbDictionary::Column * col = pTab->getColumn(c);
    if(col->getPrimaryKey()){
      pIdx.addIndexColumn(col->getName());
      ndbout << col->getName() <<" ";
    }
  }
  
  pIdx.setStoredIndex(logged);
  ndbout << ") ";
  if (pNdb->getDictionary()->createIndex(pIdx) != 0){
    ndbout << "FAILED!" << endl;
    const NdbError err = pNdb->getDictionary()->getNdbError();
    NDB_ERR(err);
    return NDBT_FAILED;
  }
  
  ndbout << "OK!" << endl;
  return NDBT_OK;
}

static int run_create_pk_index_drop(NDBT_Context* ctx, NDBT_Step* step){
  bool orderedIndex = ctx->getProperty("OrderedIndex", (unsigned)0);

  Ndb* pNdb = GETNDB(step);
  const NdbDictionary::Table *pTab = 
    pNdb->getDictionary()->getTable(ctx->getTab()->getName());
  
  if(!pTab)
    return NDBT_OK;
  
  BaseString name;
  name.assfmt("IND_%s_PK_%c", pTab->getName(), orderedIndex ? 'O' : 'U');
  
  ndbout << "Dropping index " << name.c_str() << " ";
  if (pNdb->getDictionary()->dropIndex(name.c_str(), pTab->getName()) != 0){
    ndbout << "FAILED!" << endl;
    NDB_ERR(pNdb->getDictionary()->getNdbError());
    return NDBT_FAILED;
  } else {
    ndbout << "OK!" << endl;
  }
  
  return NDBT_OK;
}

static int
run_create_dist_table(NDBT_Context* ctx, NDBT_Step* step)
{
  bool userDefined = ctx->getProperty("UserDefined", (unsigned)0);
  if(create_dist_table(GETNDB(step),
                       userDefined))
    return NDBT_OK;
  
  return NDBT_FAILED;
}

static int
run_drop_dist_table(NDBT_Context* ctx, NDBT_Step* step)
{
  GETNDB(step)->getDictionary()->dropTable(DistTabName);
  return NDBT_OK;
}

static int
run_tests(Ndb* p_ndb, HugoTransactions& hugoTrans, int records, Uint32 batchSize = 1)
{
  if (hugoTrans.loadTable(p_ndb, records, batchSize) != 0)
  {
    return NDBT_FAILED;
  }

  if(hugoTrans.pkReadRecords(p_ndb, records, batchSize) != 0)
  {
    return NDBT_FAILED;
  }

  if(hugoTrans.pkUpdateRecords(p_ndb, records, batchSize) != 0)
  {
    return NDBT_FAILED;
  }

  if(hugoTrans.pkDelRecords(p_ndb, records, batchSize) != 0)
  {
    return NDBT_FAILED;
  }

  if (hugoTrans.loadTable(p_ndb, records, batchSize) != 0)
  {
    return NDBT_FAILED;
  }

  if(hugoTrans.scanUpdateRecords(p_ndb, records) != 0)
  {
    return NDBT_FAILED;
  }

  Uint32 abort = 23;
  for(Uint32 j = 0; j<5; j++){
    Uint32 parallelism = (j == 1 ? 1 : j * 3);
    ndbout_c("parallelism: %d", parallelism);
    if (hugoTrans.scanReadRecords(p_ndb, records, abort, parallelism,
				  NdbOperation::LM_Read) != 0)
    {
      return NDBT_FAILED;
    }
    if (hugoTrans.scanReadRecords(p_ndb, records, abort, parallelism,
				  NdbOperation::LM_Exclusive) != 0)
    {
      return NDBT_FAILED;
    }
    if (hugoTrans.scanReadRecords(p_ndb, records, abort, parallelism,
				  NdbOperation::LM_CommittedRead) != 0)
    {
      return NDBT_FAILED;
    }
  }
  
  if(hugoTrans.clearTable(p_ndb, records) != 0)
  {
    return NDBT_FAILED;
  }

  return 0;
}

static int
run_pk_dk(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* p_ndb = GETNDB(step);
  int records = ctx->getNumRecords();
  const NdbDictionary::Table *tab = 
    p_ndb->getDictionary()->getTable(ctx->getTab()->getName());

  if(!tab)
    return NDBT_OK;

  HugoTransactions hugoTrans(*tab);

  Uint32 batchSize= ctx->getProperty("BatchSize", (unsigned) 1);
  
  return run_tests(p_ndb, hugoTrans, records, batchSize);
}

int
run_index_dk(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* p_ndb = GETNDB(step);
  int records = ctx->getNumRecords();
  const NdbDictionary::Table *pTab = 
    p_ndb->getDictionary()->getTable(ctx->getTab()->getName());
  
  if(!pTab)
    return NDBT_OK;

  bool orderedIndex = ctx->getProperty("OrderedIndex", (unsigned)0);
  
  BaseString name;
  name.assfmt("IND_%s_PK_%c", pTab->getName(), orderedIndex ? 'O' : 'U');
  
  const NdbDictionary::Index * idx = 
    p_ndb->getDictionary()->getIndex(name.c_str(), pTab->getName());
  
  if(!idx)
  {
    ndbout << "Failed to retreive index: " << name.c_str() << endl;
    return NDBT_FAILED;
  }
  Uint32 batchSize= ctx->getProperty("BatchSize", (unsigned) 1);

  HugoTransactions hugoTrans(*pTab, idx);
  
  return run_tests(p_ndb, hugoTrans, records, batchSize);
}

static int
run_startHint(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* p_ndb = GETNDB(step);
  int records = ctx->getNumRecords();
  const NdbDictionary::Table *tab = 
    p_ndb->getDictionary()->getTable(ctx->getTab()->getName());
  
  if(!tab)
    return NDBT_OK;

  HugoTransactions hugoTrans(*tab);
  if (hugoTrans.loadTable(p_ndb, records) != 0)
  {
    return NDBT_FAILED;
  }

  NdbRestarter restarter;
  if(restarter.insertErrorInAllNodes(8050) != 0)
    return NDBT_FAILED;
  
  HugoCalculator dummy(*tab);
  int result = NDBT_OK;
  for(int i = 0; i<records && result == NDBT_OK; i++)
  {
    char buffer[NDB_MAX_TUPLE_SIZE];
    char* start= buffer + (rand() & 7);
    char* pos= start;
    
    int k = 0;
    Ndb::Key_part_ptr ptrs[NDB_MAX_NO_OF_ATTRIBUTES_IN_KEY+1];
    for(int j = 0; j<tab->getNoOfColumns(); j++)
    {
      if(tab->getColumn(j)->getPartitionKey())
      {
	//ndbout_c(tab->getColumn(j)->getName());
	int sz = tab->getColumn(j)->getSizeInBytes();
	Uint32 real_size;
	dummy.calcValue(i, j, 0, pos, sz, &real_size);
	ptrs[k].ptr = pos;
	ptrs[k++].len = real_size;
	pos += (real_size + 3) & ~3;
      }
    }
    ptrs[k].ptr = 0;
    
    // Now we have the pk
    NdbTransaction* pTrans= p_ndb->startTransaction(tab, ptrs);
    HugoOperations ops(*tab);
    ops.setTransaction(pTrans);
    if(ops.pkReadRecord(p_ndb, i, 1) != NDBT_OK)
    {
      result = NDBT_FAILED;
      break;
    }
    
    if(ops.execute_Commit(p_ndb) != 0)
    {
      result = NDBT_FAILED;
      break;
    }
    
    ops.closeTransaction(p_ndb);
  }
  restarter.insertErrorInAllNodes(0);
  return result;
}

static int
run_startHint_ordered_index(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* p_ndb = GETNDB(step);
  int records = ctx->getNumRecords();
  const NdbDictionary::Table *tab = 
    p_ndb->getDictionary()->getTable(ctx->getTab()->getName());
  
  if(!tab)
    return NDBT_OK;

  BaseString name;
  name.assfmt("IND_%s_PK_O", tab->getName());
  
  const NdbDictionary::Index * idx = 
    p_ndb->getDictionary()->getIndex(name.c_str(), tab->getName());
  
  if(!idx)
  {
    ndbout << "Failed to retreive index: " << name.c_str() << endl;
    return NDBT_FAILED;
  }

  HugoTransactions hugoTrans(*tab, idx);
  if (hugoTrans.loadTable(p_ndb, records) != 0)
  {
    return NDBT_FAILED;
  }

  const Uint32 errorInsert = ctx->getProperty("errorinsertion", (unsigned) 8050);

  NdbRestarter restarter;
  if(restarter.insertErrorInAllNodes(errorInsert) != 0)
    return NDBT_FAILED;
  
  HugoCalculator dummy(*tab);
  int result = NDBT_OK;
  for(int i = 0; i<records && result == NDBT_OK; i++)
  {
    char buffer[NDB_MAX_TUPLE_SIZE];
    NdbTransaction* pTrans= NULL;

    char* start= buffer + (rand() & 7);
    char* pos= start;
    
    int k = 0;
    Ndb::Key_part_ptr ptrs[NDB_MAX_NO_OF_ATTRIBUTES_IN_KEY+1];
    for(int j = 0; j<tab->getNoOfColumns(); j++)
    {
      if(tab->getColumn(j)->getPartitionKey())
      {
        //ndbout_c(tab->getColumn(j)->getName());
        int sz = tab->getColumn(j)->getSizeInBytes();
        Uint32 real_size;
        dummy.calcValue(i, j, 0, pos, sz, &real_size);
        ptrs[k].ptr = pos;
        ptrs[k++].len = real_size;
        pos += (real_size + 3) & ~3;
      }
    }
    ptrs[k].ptr = 0;
    
    // Now we have the pk, start a hinted transaction
    pTrans= p_ndb->startTransaction(tab, ptrs);
  
    // Because we pass an Ordered index here, pkReadRecord will
    // use an index scan on the Ordered index
    HugoOperations ops(*tab, idx);
    ops.setTransaction(pTrans);
    /* Despite it's name, it will actually perform index scans
     * as there is an index.
     * Error 8050 will cause an NDBD assertion failure in 
     * Dbtc::execDIGETPRIMCONF() if TC needs to scan a fragment
     * which is not on the TC node
     * So for this TC to pass with no failures we need transaction
     * hinting and scan partition pruning on equal() to work 
     * correctly.
     * TODO : Get coverage of Index scan which is equal on dist
     * key cols, but has an inequality on some other column.
     */
    if(ops.pkReadRecord(p_ndb, i, 1) != NDBT_OK)
    {
      result = NDBT_FAILED;
      break;
    }
    
    if(ops.execute_Commit(p_ndb) != 0)
    {
      result = NDBT_FAILED;
      break;
    }
    
    ops.closeTransaction(p_ndb);
  }
  restarter.insertErrorInAllNodes(0);
  return result;
}

#define CHECK(x, y) {int res= (x);                    \
    if (res != 0) { ndbout << "Assert failed at "     \
                           << __LINE__ << endl        \
                           << res << endl             \
                           << " error : "             \
                           << (y)->getNdbError().code \
                           << endl;                   \
      return NDBT_FAILED; } }

#define CHECKNOTNULL(x, y) {                              \
    if ((x) == NULL) { ndbout << "Assert failed at line "    \
                              << __LINE__ << endl            \
                              << " with "                    \
                              << (y)->getNdbError().code     \
                              << endl;                       \
      return NDBT_FAILED; } }


static int
load_dist_table(Ndb* pNdb, int records, int parts)
{
  const NdbDictionary::Table* tab= pNdb->getDictionary()->getTable(DistTabName);
  bool userDefined= (tab->getFragmentType() == 
                     NdbDictionary::Object::UserDefined);

  const NdbRecord* distRecord= tab->getDefaultRecord();
  CHECKNOTNULL(distRecord, pNdb);
  
  char* buf= (char*) malloc(NdbDictionary::getRecordRowLength(distRecord));

  CHECKNOTNULL(buf, pNdb);

  /* We insert a number of records with a constrained number of
   * values for the distribution key column
   */
  for (int r=0; r < records; r++)
  {
    NdbTransaction* trans= pNdb->startTransaction();
    CHECKNOTNULL(trans, pNdb);

    {
      const int dKeyVal= r % parts;
      const Uint32 dKeyAttrid= tab->getColumn(DistTabDKeyCol)->getAttrId();
      memcpy(NdbDictionary::getValuePtr(distRecord, buf,
                                        dKeyAttrid),
             &dKeyVal, sizeof(dKeyVal));
    }

    {
      const int pKey2Val= r;
      const Uint32 pKey2Attrid= tab->getColumn(DistTabPKey2Col)->getAttrId();
      memcpy(NdbDictionary::getValuePtr(distRecord, buf,
                                        pKey2Attrid),
             &pKey2Val, sizeof(pKey2Val));
    }

    {
      const int resultVal= r*r;
      const Uint32 resultValAttrid=
        tab->getColumn(DistTabResultCol)->getAttrId();
      memcpy(NdbDictionary::getValuePtr(distRecord, buf,
                                        resultValAttrid),
             &resultVal, sizeof(resultVal));

      // set not NULL
      NdbDictionary::setNull(distRecord, buf, resultValAttrid, false);
    }


    NdbOperation::OperationOptions opts;
    opts.optionsPresent= 0;

    if (userDefined)
    {
      /* For user-defined partitioning, we set the partition id
       * to be the distribution key value modulo the number
       * of partitions in the table
       */
      opts.optionsPresent= NdbOperation::OperationOptions::OO_PARTITION_ID;
      opts.partitionId= (r%parts) % tab->getFragmentCount();
    }
    
    CHECKNOTNULL(trans->insertTuple(distRecord, buf, 
                                    NULL, &opts, sizeof(opts)), trans);

    if (trans->execute(NdbTransaction::Commit) != 0)
    {
      NdbError err = trans->getNdbError();
      if (err.status == NdbError::TemporaryError)
      {
        ndbout << err << endl;
        NdbSleep_MilliSleep(50);
        r--; // just retry
      }
      else
      {
        CHECK(-1, trans);
      }
    }
    trans->close();
  }

  free(buf);
  
  return NDBT_OK;
}

struct PartInfo
{
  NdbTransaction* trans;
  NdbIndexScanOperation* op;
  int dKeyVal;
  int valCount;
};

class Ap
{
public:
  void* ptr;
  
  Ap(void* _ptr) : ptr(_ptr)
    {}
  ~Ap()
    {
      if (ptr != 0)
      {
        free(ptr);
        ptr= 0;
      }
    }
};

static int
dist_scan_body(Ndb* pNdb, int records, int parts, PartInfo* partInfo, bool usePrimary)
{
  const NdbDictionary::Table* tab= pNdb->getDictionary()->getTable(DistTabName);
  CHECKNOTNULL(tab, pNdb->getDictionary());
  const char* indexName= usePrimary ? "PRIMARY" : DistIdxName;
  const NdbDictionary::Index* idx= pNdb->getDictionary()->getIndex(indexName,
                                                                   DistTabName);
  CHECKNOTNULL(idx, pNdb->getDictionary());
  const NdbRecord* tabRecord= tab->getDefaultRecord();
  const NdbRecord* idxRecord= idx->getDefaultRecord();
  bool userDefined= (tab->getFragmentType() == 
                     NdbDictionary::Object::UserDefined);

  char* boundBuf= (char*) malloc(NdbDictionary::getRecordRowLength(idx->getDefaultRecord()));

  if (usePrimary)
    ndbout << "Checking MRR indexscan distribution awareness when distribution key part of bounds" << endl;
  else
    ndbout << "Checking MRR indexscan distribution awareness when distribution key provided explicitly" << endl;

  if (userDefined)
    ndbout << "User Defined Partitioning scheme" << endl;
  else
    ndbout << "Native Partitioning scheme" << endl;

  Ap boundAp(boundBuf);

  for (int r=0; r < records; r++)
  {
    int partValue= r % parts;
    PartInfo& pInfo= partInfo[partValue];

    if (pInfo.trans == NULL)
    {
      /* Provide the partition key as a hint for this transaction */
      if (!userDefined)
      {
        Ndb::Key_part_ptr keyParts[2];
        keyParts[0].ptr= &partValue;
        keyParts[0].len= sizeof(partValue);
        keyParts[1].ptr= NULL;
        keyParts[1].len= 0;
        
        /* To test that bad hinting causes failure, uncomment */
        // int badPartVal= partValue+1;
        // keyParts[0].ptr= &badPartVal;
        
        CHECKNOTNULL(pInfo.trans= pNdb->startTransaction(tab, keyParts), 
                     pNdb);
      }
      else
      {
        /* User Defined partitioning */
        Uint32 partId= partValue % tab->getFragmentCount();
        CHECKNOTNULL(pInfo.trans= pNdb->startTransaction(tab,
                                                         partId),
                     pNdb);
      }
      pInfo.valCount= 0;
      pInfo.dKeyVal= partValue;

      NdbScanOperation::ScanOptions opts;
      opts.optionsPresent= NdbScanOperation::ScanOptions::SO_SCANFLAGS;
      opts.scan_flags= NdbScanOperation::SF_MultiRange;

      // Define the scan operation for this partition.
      CHECKNOTNULL(pInfo.op= pInfo.trans->scanIndex(idx->getDefaultRecord(),
                                                    tab->getDefaultRecord(),
                                                    NdbOperation::LM_Read,
                                                    NULL,
                                                    NULL,
                                                    &opts,
                                                    sizeof(opts)), 
                   pInfo.trans);
    }
    
    NdbIndexScanOperation* op= pInfo.op;

    if (usePrimary)
    {
      {
        int dKeyVal= partValue;
        int pKey2Val= r;
        /* Scanning the primary index, set bound on the pk */
        memcpy(NdbDictionary::getValuePtr(idxRecord,
                                          boundBuf,
                                          tab->getColumn(DistTabDKeyCol)->getAttrId()),
               &dKeyVal,
               sizeof(dKeyVal));
        memcpy(NdbDictionary::getValuePtr(idxRecord,
                                          boundBuf,
                                          tab->getColumn(DistTabPKey2Col)->getAttrId()),
               &pKey2Val,
               sizeof(pKey2Val));
        
      }

      NdbIndexScanOperation::IndexBound ib;
      ib.low_key= boundBuf;
      ib.low_key_count= 2;
      ib.low_inclusive= true;
      ib.high_key= ib.low_key;
      ib.high_key_count= ib.low_key_count;
      ib.high_inclusive= true;
      ib.range_no= pInfo.valCount++;

      /* No partitioning info for native, PK index scan
       * NDBAPI can determine it from PK */
      Ndb::PartitionSpec pSpec;
      pSpec.type= Ndb::PartitionSpec::PS_NONE;

      if (userDefined)
      {
        /* We'll provide partition info */
        pSpec.type= Ndb::PartitionSpec::PS_USER_DEFINED;
        pSpec.UserDefined.partitionId= partValue % tab->getFragmentCount();
      }

      CHECK(op->setBound(idxRecord,
                         ib,
                         &pSpec,
                         sizeof(pSpec)),
            op);
    }
    else
    {
      Uint32 resultValAttrId= tab->getColumn(DistTabResultCol)->getAttrId();
      /* Scanning the secondary index, set bound on the result */
      {
        int resultVal= r*r;
        memcpy(NdbDictionary::getValuePtr(idxRecord,
                                          boundBuf,
                                          resultValAttrId),
               &resultVal,
               sizeof(resultVal));
      }

      NdbDictionary::setNull(idxRecord,
                             boundBuf,
                             resultValAttrId,
                             false);
      
      NdbIndexScanOperation::IndexBound ib;
      ib.low_key= boundBuf;
      ib.low_key_count= 1;
      ib.low_inclusive= true;
      ib.high_key= ib.low_key;
      ib.high_key_count= ib.low_key_count;
      ib.high_inclusive= true;      
      ib.range_no= pInfo.valCount++;

      Ndb::Key_part_ptr keyParts[2];
      keyParts[0].ptr= &partValue;
      keyParts[0].len= sizeof(partValue);
      keyParts[1].ptr= NULL;
      keyParts[1].len= 0;
      
      /* To test that bad hinting causes failure, uncomment */
      //int badPartVal= partValue+1;
      //keyParts[0].ptr= &badPartVal;
      
      Ndb::PartitionSpec pSpec;
      char* tabRow= NULL;
      
      if (userDefined)
      {
        /* We'll provide partition info */
        pSpec.type= Ndb::PartitionSpec::PS_USER_DEFINED;
        pSpec.UserDefined.partitionId= partValue % tab->getFragmentCount();
      }
      else
      {
        /* Can set either using an array of Key parts, or a KeyRecord
         * structure.  Let's test both
         */
        if (rand() % 2)
        {
          //ndbout << "Using Key Parts to set range partition info" << endl;
          pSpec.type= Ndb::PartitionSpec::PS_DISTR_KEY_PART_PTR;
          pSpec.KeyPartPtr.tableKeyParts= keyParts;
          pSpec.KeyPartPtr.xfrmbuf= NULL;
          pSpec.KeyPartPtr.xfrmbuflen= 0;
        }
        else
        {
          //ndbout << "Using KeyRecord to set range partition info" << endl;
          
          /* Setup a row in NdbRecord format with the distkey value set */
          tabRow= (char*)malloc(NdbDictionary::getRecordRowLength(tabRecord));
          int& dKeyVal= *((int*) NdbDictionary::getValuePtr(tabRecord,
                                                            tabRow,
                                                            tab->getColumn(DistTabDKeyCol)->getAttrId()));
          dKeyVal= partValue;
          // dKeyVal= partValue + 1; // Test failure case
          
          pSpec.type= Ndb::PartitionSpec::PS_DISTR_KEY_RECORD;
          pSpec.KeyRecord.keyRecord= tabRecord;
          pSpec.KeyRecord.keyRow= tabRow;
          pSpec.KeyRecord.xfrmbuf= 0;
          pSpec.KeyRecord.xfrmbuflen= 0;
        }
      }

      CHECK(op->setBound(idxRecord,
                         ib,
                         &pSpec,
                         sizeof(pSpec)),
            op);

      if (tabRow)
        free(tabRow);
      tabRow= NULL;

    }
  }

  for (int p=0; p < parts; p++)
  {
    PartInfo& pInfo= partInfo[p];
    //ndbout << "D-key val " << p << " has " << pInfo.valCount
    //       << " ranges specified. " << endl;
    //ndbout << "Is Pruned? " << pInfo.op->getPruned() << endl;
    if (! pInfo.op->getPruned())
    {
      ndbout << "MRR Scan Operation should have been pruned, but was not." << endl;
      return NDBT_FAILED;
    }
    
    CHECK(pInfo.trans->execute(NdbTransaction::NoCommit), pInfo.trans);
    
    int resultCount=0;
    
    const char* resultPtr;
    int rc= 0;
    
    while ((rc= pInfo.op->nextResult(&resultPtr, true, true)) == 0)
    {
      int dKeyVal;
      memcpy(&dKeyVal, NdbDictionary::getValuePtr(tabRecord,
                                                  resultPtr,
                                                  tab->getColumn(DistTabDKeyCol)->getAttrId()),
             sizeof(dKeyVal));

      int pKey2Val;
      memcpy(&pKey2Val, NdbDictionary::getValuePtr(tabRecord,
                                                   resultPtr,
                                                   tab->getColumn(DistTabPKey2Col)->getAttrId()),
             sizeof(pKey2Val));

      int resultVal;
      memcpy(&resultVal, NdbDictionary::getValuePtr(tabRecord,
                                                    resultPtr,
                                                    tab->getColumn(DistTabResultCol)->getAttrId()),
             sizeof(resultVal));
      
      if ((dKeyVal != pInfo.dKeyVal) ||
          (resultVal != (pKey2Val * pKey2Val)))
      {
        ndbout << "Got bad values.  Dkey : " << dKeyVal
               << " Pkey2 : " << pKey2Val
               << " Result : " << resultVal
               << endl;
        return NDBT_FAILED;
      }
      resultCount++;
    }

    if (rc != 1)
    {
      ndbout << "Got bad scan rc " << rc << endl;
      ndbout << "Error : " << pInfo.op->getNdbError().code << endl;
      ndbout << "Trans Error : " << pInfo.trans->getNdbError().code << endl;
      return NDBT_FAILED;
    }
  
    if (resultCount != pInfo.valCount)
    {
      ndbout << "Error resultCount was " << resultCount << endl;
      return NDBT_FAILED;
    }
    CHECK(pInfo.trans->execute(NdbTransaction::Commit), pInfo.trans);
    pInfo.trans->close();
  };

  ndbout << "Success" << endl;

  return NDBT_OK;
}

static int
dist_scan(Ndb* pNdb, int records, int parts, bool usePk)
{
  PartInfo* partInfo= new PartInfo[parts];

  NdbRestarter restarter;
  if(restarter.insertErrorInAllNodes(8050) != 0)
  {
    delete[] partInfo;
    return NDBT_FAILED;
  }

  for (int p=0; p<parts; p++)
  {
    partInfo[p].trans= NULL;
    partInfo[p].op= NULL;
    partInfo[p].dKeyVal= 0;
    partInfo[p].valCount= 0;
  }
  
  int result= dist_scan_body(pNdb, 
                             records, 
                             parts, 
                             partInfo, 
                             usePk);

  restarter.insertErrorInAllNodes(0);
  delete[] partInfo;
  
  return result;
}

static int
run_dist_test(NDBT_Context* ctx, NDBT_Step* step)
{
  int records= ctx->getNumRecords();
  
  /* Choose an interesting number of discrete
   * distribution key values to work with
   */
  int numTabPartitions= GETNDB(step)
    ->getDictionary()
    ->getTable(DistTabName)
    ->getFragmentCount();
  int numDkeyValues= 2*numTabPartitions + (rand() % 6);
  if (numDkeyValues > records)
  {
    // limit number of distributions keys to number of records
    numDkeyValues = records;
  }

  ndbout << "Table has " << numTabPartitions
         << " physical partitions" << endl;
  ndbout << "Testing with " << numDkeyValues 
         << " discrete distribution key values " << endl;

  if (load_dist_table(GETNDB(step), records, numDkeyValues) != NDBT_OK)
    return NDBT_FAILED;
  
  /* Test access via PK ordered index (including Dkey) */
  if (dist_scan(GETNDB(step), records, numDkeyValues, true) != NDBT_OK)
    return NDBT_FAILED;

  /* Test access via secondary ordered index (not including Dkey) */
  if (dist_scan(GETNDB(step), records, numDkeyValues, false) != NDBT_OK)
    return NDBT_FAILED;
  
  return NDBT_OK;
}



NDBT_TESTSUITE(testPartitioning);
TESTCASE("pk_dk", 
	 "Primary key operations with distribution key")
{
  TC_PROPERTY("distributionkey", ~0);
  INITIALIZER(run_drop_table);
  INITIALIZER(run_create_table);
  INITIALIZER(run_pk_dk);
  INITIALIZER(run_drop_table);
}
TESTCASE("hash_index_dk", 
	 "Unique index operations with distribution key")
{
  TC_PROPERTY("distributionkey", ~0);
  TC_PROPERTY("OrderedIndex", (unsigned)0);
  INITIALIZER(run_drop_table);
  INITIALIZER(run_create_table);
  INITIALIZER(run_create_pk_index);
  INITIALIZER(run_index_dk);
  INITIALIZER(run_create_pk_index_drop);
  INITIALIZER(run_drop_table);
}
TESTCASE("ordered_index_dk", 
	 "Ordered index operations with distribution key")
{
  TC_PROPERTY("distributionkey", (unsigned)1);
  TC_PROPERTY("OrderedIndex", (unsigned)1);
  INITIALIZER(run_drop_table);
  INITIALIZER(run_create_table);
  INITIALIZER(run_create_pk_index);
  INITIALIZER(run_index_dk);
  INITIALIZER(run_create_pk_index_drop);
  INITIALIZER(run_drop_table);
}
TESTCASE("smart_scan", 
	 "Ordered index operations with distribution key")
{
  TC_PROPERTY("OrderedIndex", (unsigned)1);
  INITIALIZER(run_drop_table);
  INITIALIZER(run_create_table_smart_scan);
  INITIALIZER(run_create_pk_index);
  INITIALIZER(run_index_dk);
  INITIALIZER(run_create_pk_index_drop);
  INITIALIZER(run_drop_table);
}
TESTCASE("startTransactionHint", 
	 "Test startTransactionHint wo/ distribution key")
{
  /* If hint is incorrect, node failure occurs */
  TC_PROPERTY("distributionkey", (unsigned)0);
  INITIALIZER(run_drop_table);
  INITIALIZER(run_create_table);
  INITIALIZER(run_startHint);
  INITIALIZER(run_drop_table);
}
TESTCASE("startTransactionHint_dk", 
	 "Test startTransactionHint with distribution key")
{
  /* If hint is incorrect, node failure occurs */
  TC_PROPERTY("distributionkey", (unsigned)~0);
  INITIALIZER(run_drop_table);
  INITIALIZER(run_create_table);
  INITIALIZER(run_startHint);
  INITIALIZER(run_drop_table);
}
TESTCASE("startTransactionHint_orderedIndex",
         "Test startTransactionHint and ordered index reads")
{
  /* If hint is incorrect, node failure occurs */
  TC_PROPERTY("distributionkey", (unsigned)0);
  TC_PROPERTY("OrderedIndex", (unsigned)1);
  INITIALIZER(run_drop_table);
  INITIALIZER(run_create_table);
  INITIALIZER(run_create_pk_index);
  INITIALIZER(run_startHint_ordered_index);
  INITIALIZER(run_create_pk_index_drop);
  INITIALIZER(run_drop_table);
}
TESTCASE("startTransactionHint_orderedIndex_dk",
         "Test startTransactionHint and ordered index reads with distribution key")
{
  /* If hint is incorrect, node failure occurs */
  TC_PROPERTY("distributionkey", (unsigned)~0);
  TC_PROPERTY("OrderedIndex", (unsigned)1);
  INITIALIZER(run_drop_table);
  INITIALIZER(run_create_table);
  INITIALIZER(run_create_pk_index);
  INITIALIZER(run_startHint_ordered_index);
  INITIALIZER(run_create_pk_index_drop);
  INITIALIZER(run_drop_table);
}
TESTCASE("startTransactionHint_orderedIndex_mrr_native",
         "Test hinting and MRR Ordered Index Scans for native partitioned table")
{
  TC_PROPERTY("UserDefined", (unsigned)0);
  INITIALIZER(run_create_dist_table);
  INITIALIZER(run_dist_test);
  INITIALIZER(run_drop_dist_table);
}
TESTCASE("pk_userDefined",
         "Test primary key operations on table with user-defined partitioning")
{
  /* Check PK ops against user-defined partitioned table */
  TC_PROPERTY("UserDefined", (unsigned) 1);
  INITIALIZER(run_drop_table);
  INITIALIZER(run_create_table);
  INITIALIZER(run_create_pk_index);
  INITIALIZER(run_pk_dk);
  INITIALIZER(run_create_pk_index_drop);
  INITIALIZER(run_drop_table);
};
TESTCASE("hash_index_userDefined",
         "Unique index operations on table with user-defined partitioning")
{
  /* Check hash index ops against user-defined partitioned table */
  TC_PROPERTY("OrderedIndex", (unsigned)0);
  TC_PROPERTY("UserDefined", (unsigned)1);
  INITIALIZER(run_drop_table);
  INITIALIZER(run_create_table);
  INITIALIZER(run_create_pk_index);
  INITIALIZER(run_index_dk);
  INITIALIZER(run_create_pk_index_drop);
  INITIALIZER(run_drop_table);
}
TESTCASE("ordered_index_userDefined", 
	 "Ordered index operations on table with user-defined partitioning")
{
  /* Check ordered index operations against user-defined partitioned table */
  TC_PROPERTY("OrderedIndex", (unsigned)1);
  TC_PROPERTY("UserDefined", (unsigned)1);
  INITIALIZER(run_drop_table);
  INITIALIZER(run_create_table);
  INITIALIZER(run_create_pk_index);
  INITIALIZER(run_index_dk);
  INITIALIZER(run_create_pk_index_drop);
  INITIALIZER(run_drop_table);
}
TESTCASE("startTransactionHint_orderedIndex_mrr_userDefined",
         "Test hinting and MRR Ordered Index Scans for user defined partitioned table")
{
  TC_PROPERTY("UserDefined", (unsigned)1);
  INITIALIZER(run_create_dist_table);
  INITIALIZER(run_dist_test);
  INITIALIZER(run_drop_dist_table);
}
TESTCASE("startTransactionHint_orderedIndex_MaxKey",
         "Test startTransactionHint with max hash value via error insert")
{
  /* Special regression case */
  TC_PROPERTY("distributionkey", (unsigned)0);
  TC_PROPERTY("OrderedIndex", (unsigned)1);
  TC_PROPERTY("errorinsertion", (unsigned) 8119);
  INITIALIZER(run_drop_table);
  INITIALIZER(run_create_table);
  INITIALIZER(run_create_pk_index);
  INITIALIZER(run_startHint_ordered_index);
  INITIALIZER(run_create_pk_index_drop);
  INITIALIZER(run_drop_table);
}

NDBT_TESTSUITE_END(testPartitioning)

int main(int argc, const char** argv){
  ndb_init();
  NDBT_TESTSUITE_INSTANCE(testPartitioning);
  testPartitioning.setCreateTable(false);
  return testPartitioning.execute(argc, argv);
}



