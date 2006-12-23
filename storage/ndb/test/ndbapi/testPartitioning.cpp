/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <NDBT_Test.hpp>
#include <NDBT_ReturnCodes.h>
#include <HugoTransactions.hpp>
#include <UtilTransactions.hpp>
#include <NdbRestarter.hpp>

#define GETNDB(ps) ((NDBT_NdbApiStep*)ps)->getNdb()

static Uint32 max_dks = 0;

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
add_distribution_key(Ndb*, NdbDictionary::Table& tab, int when, void* arg)
{
  switch(when){
  case 0: // Before
    break;
  case 1: // After
    return 0;
  default:
    return 0;
  }

  int keys = tab.getNoOfPrimaryKeys();
  int dks = (2 * keys + 2) / 3; dks = (dks > max_dks ? max_dks : dks);
  int cnt = 0;
  
  for(unsigned i = 0; i<tab.getNoOfColumns(); i++)
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
    for(unsigned i = 0; i<tab.getNoOfColumns(); i++)
    {
      NdbDictionary::Column* col = tab.getColumn(i);
      if(col->getPrimaryKey() && col->getCharset() == 0)
      {
	if(dks >= keys || (rand() % 100) > 50)
	{
	  col->setDistributionKey(true);
	  dks--;
	}
	keys--;
      }
    }
  }

  Uint32 linear_hash_ind = rand() & 1;
  NdbDictionary::Table::FragmentType ftype;
  if (linear_hash_ind)
    ftype = NdbDictionary::Table::DistrKeyLin;
  else
    ftype = NdbDictionary::Table::DistrKeyHash;
  tab.setFragmentType(ftype);

  ndbout << (NDBT_Table&)tab << endl;

  return 0;
}

static int
run_create_table(NDBT_Context* ctx, NDBT_Step* step)
{
  max_dks = ctx->getProperty("distributionkey", (unsigned)0);
  
  if(NDBT_Tables::createTable(GETNDB(step), 
			      ctx->getTab()->getName(), 
			      false, false, 
			      max_dks?add_distribution_key:0) == NDBT_OK)
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
    ERR(err);
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
    ERR(pNdb->getDictionary()->getNdbError());
    return NDBT_FAILED;
  } else {
    ndbout << "OK!" << endl;
  }
  
  return NDBT_OK;
}

static int
run_tests(Ndb* p_ndb, HugoTransactions& hugoTrans, int records)
{
  if (hugoTrans.loadTable(p_ndb, records) != 0)
  {
    return NDBT_FAILED;
  }

  if(hugoTrans.pkReadRecords(p_ndb, records) != 0)
  {
    return NDBT_FAILED;
  }

  if(hugoTrans.pkUpdateRecords(p_ndb, records) != 0)
  {
    return NDBT_FAILED;
  }

  if(hugoTrans.pkDelRecords(p_ndb, records) != 0)
  {
    return NDBT_FAILED;
  }

  if (hugoTrans.loadTable(p_ndb, records) != 0)
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
  
  return run_tests(p_ndb, hugoTrans, records);
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

  HugoTransactions hugoTrans(*pTab, idx);
  
  return run_tests(p_ndb, hugoTrans, records);
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
    char buffer[8000];
    char* start= buffer + (rand() & 7);
    char* pos= start;
    
    for(int j = 0; j<tab->getNoOfColumns(); j++)
    {
      if(tab->getColumn(j)->getPartitionKey())
      {
	ndbout_c(tab->getColumn(j)->getName());
	int sz = tab->getColumn(j)->getSizeInBytes();
	int aligned_size = 4 * ((sz + 3) >> 2);
	memset(pos, 0, aligned_size);
	Uint32 real_size;
	dummy.calcValue(i, j, 0, pos, sz, &real_size);
	pos += (real_size + 3) & ~3;
      }
    }
    // Now we have the pk
    NdbTransaction* pTrans= p_ndb->startTransaction(tab, start,(pos - start));
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
	 "Unique index operatations with distribution key")
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
	 "Ordered index operatations with distribution key")
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
TESTCASE("startTransactionHint", 
	 "Test startTransactionHint wo/ distribution key")
{
  TC_PROPERTY("distributionkey", (unsigned)0);
  INITIALIZER(run_drop_table);
  INITIALIZER(run_create_table);
  INITIALIZER(run_startHint);
  INITIALIZER(run_drop_table);
}
TESTCASE("startTransactionHint_dk", 
	 "Test startTransactionHint with distribution key")
{
  TC_PROPERTY("distributionkey", (unsigned)~0);
  INITIALIZER(run_drop_table);
  INITIALIZER(run_create_table);
  INITIALIZER(run_startHint);
  INITIALIZER(run_drop_table);
}
NDBT_TESTSUITE_END(testPartitioning);

int main(int argc, const char** argv){
  ndb_init();
  testPartitioning.setCreateTable(false);
  return testPartitioning.execute(argc, argv);
}



