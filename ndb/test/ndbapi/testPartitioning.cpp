/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

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

static
int runLoadTable(NDBT_Context* ctx, NDBT_Step* step)
{
  int records = ctx->getNumRecords();
  HugoTransactions hugoTrans(*ctx->getTab());
  if (hugoTrans.loadTable(GETNDB(step), records) != 0){
    return NDBT_FAILED;
  }
  return NDBT_OK;
}

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
add_distribution_key(Ndb*, NdbDictionary::Table& tab, int when)
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
  int dks = (2 * keys + 2) / 3;
  int cnt = 0;
  ndbout_c("%s pks: %d dks: %d", tab.getName(), keys, dks);
  for(unsigned i = 0; i<tab.getNoOfColumns(); i++)
  {
    NdbDictionary::Column* col = tab.getColumn(i);
    if(col->getPrimaryKey())
    {
      if(dks >= keys || (rand() % 100) > 50)
      {
	col->setDistributionKey(true);
	dks--;
      }
      keys--;
    }
  }
  return 0;
}

int
run_create_table(NDBT_Context* ctx, NDBT_Step* step)
{
  bool dk = ctx->getProperty("distributionkey", (unsigned)0);
  return NDBT_Tables::createTable(GETNDB(step), 
				  ctx->getTab()->getName(), 
				  false, false, dk?add_distribution_key:0);
}

int
run_pk_dk(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* p_ndb = GETNDB(step);
  int records = ctx->getNumRecords();
  const NdbDictionary::Table *tab = 
    p_ndb->getDictionary()->getTable(ctx->getTab()->getName());
  HugoTransactions hugoTrans(*tab);

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

  if(hugoTrans.scanReadRecords(p_ndb, records) != 0)
  {
    return NDBT_FAILED;
  }

  if(hugoTrans.clearTable(p_ndb, records) != 0)
  {
    return NDBT_FAILED;
  }

  return 0;
}

int
run_hash_dk(NDBT_Context* ctx, NDBT_Step* step)
{
  return 0;
}

int
run_index_dk(NDBT_Context* ctx, NDBT_Step* step)
{
  return 0;
}


NDBT_TESTSUITE(testPartitioning);
TESTCASE("pk_dk", 
	 "Primary key operations with distribution key")
{
  TC_PROPERTY("distributionkey", 1);
  INITIALIZER(run_drop_table);
  INITIALIZER(run_create_table);
  INITIALIZER(run_pk_dk);
  INITIALIZER(run_drop_table);
}
TESTCASE("hash_index_dk", 
	 "Unique index operatations with distribution key")
{
  TC_PROPERTY("distributionkey", 1);
  INITIALIZER(run_drop_table);
  INITIALIZER(run_create_table);
  INITIALIZER(run_hash_dk);
  INITIALIZER(run_drop_table);
}
TESTCASE("ordered_index_dk", 
	 "Ordered index operatations with distribution key")
{
  TC_PROPERTY("distributionkey", 1);
  INITIALIZER(run_drop_table);
  INITIALIZER(run_create_table);
  INITIALIZER(run_index_dk);
  INITIALIZER(run_drop_table);
}
NDBT_TESTSUITE_END(testPartitioning);

int main(int argc, const char** argv){
  ndb_init();
  testPartitioning.setCreateTable(false);
  return testPartitioning.execute(argc, argv);
}



