/*
   Copyright (C) 2009 Sun Microsystems Inc.
   All rights reserved. Use is subject to license terms.

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

#include <NDBT.hpp>
#include <NDBT_Test.hpp>
#include "../../src/ndbapi/NdbInfo.hpp"


int runTestNdbInfo(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbInfo ndbinfo(&ctx->m_cluster_connection, "ndbinfo/");
  if (!ndbinfo.init())
  {
    g_err << "ndbinfo.init failed" << endl;
    return NDBT_FAILED;
  }

  const NdbInfo::Table* table;
  if (ndbinfo.openTable("ndbinfo/tables", &table) != 0)
  {
    g_err << "Failed to openTable(tables)" << endl;
    return NDBT_FAILED;
  }

  for (int l = 0; l < ctx->getNumLoops(); l++)
  {

    NdbInfoScanOperation* scanOp = NULL;
    if (ndbinfo.createScanOperation(table, &scanOp))
    {
      g_err << "No NdbInfoScanOperation" << endl;
      return NDBT_FAILED;
    }

    if (scanOp->readTuples() != 0)
    {
      g_err << "scanOp->readTuples failed" << endl;
      return NDBT_FAILED;
    }

    const NdbInfoRecAttr* tableName = scanOp->getValue("table_name");
    const NdbInfoRecAttr* comment = scanOp->getValue("comment");

    if(scanOp->execute() != 0)
    {
      g_err << "scanOp->execute failed" << endl;
      return NDBT_FAILED;
    }

    while(scanOp->nextResult() == 1)
    {
      g_info << "NAME: " << tableName->c_str() << endl;
      g_info << "COMMENT: " << comment->c_str() << endl;
    }
    ndbinfo.releaseScanOperation(scanOp);
  }

  ndbinfo.closeTable(table);
  return NDBT_OK;
}

int runScanAll(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbInfo ndbinfo(&ctx->m_cluster_connection, "ndbinfo/");
  if (!ndbinfo.init())
  {
    g_err << "ndbinfo.init failed" << endl;
    return NDBT_FAILED;
  }

  Uint32 tableId = 0;
  while(true) {
    const NdbInfo::Table* table;

    int err = ndbinfo.openTable(tableId, &table);
    if (err == NdbInfo::ERR_NoSuchTable)
    {
      // No more tables -> return
      return NDBT_OK;
    }
    else if (err != 0)
    {
      // Unexpected return code
      g_err << "Failed to openTable(" << tableId << "), err: " << err << endl;
      return NDBT_FAILED;
    }
    ndbout << "table("<<tableId<<"): " << table->getName() << endl;

    for (int l = 0; l < ctx->getNumLoops(); l++)
    {
      NdbInfoScanOperation* scanOp = NULL;
      if (ndbinfo.createScanOperation(table, &scanOp))
      {
        g_err << "No NdbInfoScanOperation" << endl;
        return NDBT_FAILED;
      }

      if (scanOp->readTuples() != 0)
      {
        g_err << "scanOp->readTuples failed" << endl;
        return NDBT_FAILED;
      }

      int columnId = 0;
      while (scanOp->getValue(columnId))
        columnId++;
      // At least one column
      assert(columnId >= 1);

      if(scanOp->execute() != 0)
      {
        g_err << "scanOp->execute failed" << endl;
        return NDBT_FAILED;
      }

      int row = 0;
      while(scanOp->nextResult() == 1)
        row++;
      ndbout << "rows: " << row << endl;
      ndbinfo.releaseScanOperation(scanOp);
    }
    ndbinfo.closeTable(table);
    tableId++;
  }

  // Should never come here
  assert(false);
  return NDBT_FAILED;
}

int runScanStop(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbInfo ndbinfo(&ctx->m_cluster_connection, "ndbinfo/");
  if (!ndbinfo.init())
  {
    g_err << "ndbinfo.init failed" << endl;
    return NDBT_FAILED;
  }

  Uint32 tableId = 0;
  while(true) {
    const NdbInfo::Table* table;

    int err = ndbinfo.openTable(tableId, &table);
    if (err == NdbInfo::ERR_NoSuchTable)
    {
      // No more tables -> return
      return NDBT_OK;
    }
    else if (err != 0)
    {
      // Unexpected return code
      g_err << "Failed to openTable(" << tableId << "), err: " << err << endl;
      return NDBT_FAILED;
    }
    ndbout << "table: " << table->getName() << endl;

    for (int l = 0; l < ctx->getNumLoops()*10; l++)
    {
      NdbInfoScanOperation* scanOp = NULL;
      if (ndbinfo.createScanOperation(table, &scanOp))
      {
        g_err << "No NdbInfoScanOperation" << endl;
        return NDBT_FAILED;
      }

      if (scanOp->readTuples() != 0)
      {
        g_err << "scanOp->readTuples failed" << endl;
        return NDBT_FAILED;
      }

      int columnId = 0;
      while (scanOp->getValue(columnId))
        columnId++;
      // At least one column
      assert(columnId >= 1);

      if(scanOp->execute() != 0)
      {
        g_err << "scanOp->execute failed" << endl;
        return NDBT_FAILED;
      }

      int stopRow = rand() % 100;
      int row = 0;
      while(scanOp->nextResult() == 1)
      {
        row++;
        if (row == stopRow)
        {
          ndbout_c("Aborting scan at row %d", stopRow);
          break;
        }
      }
      ndbinfo.releaseScanOperation(scanOp);
    }
    ndbinfo.closeTable(table);
    tableId++;
  }

  // Should never come here
  assert(false);
  return NDBT_FAILED;
}


int runRatelimit(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbInfo ndbinfo(&ctx->m_cluster_connection, "ndbinfo/");
  if (!ndbinfo.init())
  {
    g_err << "ndbinfo.init failed" << endl;
    return NDBT_FAILED;
  }

  Uint32 tableId = 0;
  while(true) {

    const NdbInfo::Table* table;

    int err = ndbinfo.openTable(tableId, &table);
    if (err == NdbInfo::ERR_NoSuchTable)
    {
      // No more tables -> return
      return NDBT_OK;
    }
    else if (err != 0)
    {
      // Unexpected return code
      g_err << "Failed to openTable(" << tableId << "), err: " << err << endl;
      return NDBT_FAILED;
    }
    ndbout << "table: " << table->getName() << endl;
    

    struct { Uint32 rows; Uint32 bytes; } limits[] = {
      { 0, 0 },
      { 1, 0 }, { 2, 0 }, { 10, 0 }, { 37, 0 }, { 1000, 0 },
      { 0, 1 }, { 0, 2 }, { 0, 10 }, { 0, 37 }, { 0, 1000 },
      { 1, 1 }, { 2, 2 }, { 10, 10 }, { 37, 37 }, { 1000, 1000 }
    };

    int lastRows = 0;
    for (int l = 0; l < (int)(sizeof(limits)/sizeof(limits[0])); l++)
    {

      Uint32 maxRows = limits[l].rows;
      Uint32 maxBytes = limits[l].bytes;

      NdbInfoScanOperation* scanOp = NULL;
      if (ndbinfo.createScanOperation(table, &scanOp, maxRows, maxBytes))
      {
        g_err << "No NdbInfoScanOperation" << endl;
        return NDBT_FAILED;
      }

      if (scanOp->readTuples() != 0)
      {
        g_err << "scanOp->readTuples failed" << endl;
        return NDBT_FAILED;
      }

      int columnId = 0;
      while (scanOp->getValue(columnId))
        columnId++;
      // At least one column
      assert(columnId >= 1);

      if(scanOp->execute() != 0)
      {
        g_err << "scanOp->execute failed" << endl;
        return NDBT_FAILED;
      }

      int row = 0;
      while(scanOp->nextResult() == 1)
        row++;
      ndbinfo.releaseScanOperation(scanOp);

      ndbout_c("[%u,%u] rows: %d", maxRows, maxBytes, row);
      if (lastRows != 0)
      {
        // Check that the number of rows is same as last round on same table
        if (lastRows != row)
        {
          g_err << "Got different number of rows this round, expected: "
                << lastRows << ", got: " << row << endl;
          ndbinfo.closeTable(table);
          return NDBT_FAILED;
        }
      }
      lastRows = row;
    }
    ndbinfo.closeTable(table);
    tableId++;
  }

  // Should never come here
  assert(false);
  return NDBT_FAILED;
}

int runTestTable(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbInfo ndbinfo(&ctx->m_cluster_connection, "ndbinfo/");
  if (!ndbinfo.init())
  {
    g_err << "ndbinfo.init failed" << endl;
    return NDBT_FAILED;
  }

  const NdbInfo::Table* table;
  if (ndbinfo.openTable("ndbinfo/test", &table) != 0)
  {
    g_err << "Failed to openTable(test)" << endl;
    return NDBT_FAILED;
  }

  for (int l = 0; l < ctx->getNumLoops(); l++)
  {

    NdbInfoScanOperation* scanOp = NULL;
    if (ndbinfo.createScanOperation(table, &scanOp))
    {
      g_err << "No NdbInfoScanOperation" << endl;
      return NDBT_FAILED;
    }

    if (scanOp->readTuples() != 0)
    {
      g_err << "scanOp->readTuples failed" << endl;
      return NDBT_FAILED;
    }

    const NdbInfoRecAttr* nodeId= scanOp->getValue("node_id");
    const NdbInfoRecAttr* blockNumber= scanOp->getValue("block_number");
    const NdbInfoRecAttr* blockInstance= scanOp->getValue("block_instance");
    const NdbInfoRecAttr* counter= scanOp->getValue("counter");

    if(scanOp->execute() != 0)
    {
      g_err << "scanOp->execute failed" << endl;
      return NDBT_FAILED;
    }

    int rows = 0;
    while(scanOp->nextResult() == 1)
    {
      rows++;//
    }
    ndbout << "rows: " << rows << endl;
    ndbinfo.releaseScanOperation(scanOp);
  }

  ndbinfo.closeTable(table);
  return NDBT_OK;
}


NDBT_TESTSUITE(testNdbinfo);
TESTCASE("Ndbinfo",
         "Test ndbapi interface to NDB$INFO"){
  INITIALIZER(runTestNdbInfo);
}
TESTCASE("Ndbinfo10",
         "Test ndbapi interface to NDB$INFO"){
  STEPS(runTestNdbInfo, 10);
}
TESTCASE("ScanAll",
         "Scan all colums of all table known to NdbInfo"){
  STEPS(runScanAll, 1);
}
TESTCASE("ScanStop",
         "Randomly stop the scan"){
  STEPS(runScanStop, 1);
}
TESTCASE("Ratelimit",
         "Scan wit different combinations of ratelimit"){
  STEPS(runRatelimit, 1);
}
TESTCASE("TestTable",
         "Scan the test table and make sure it returns correct number "
          "of rows which will depend on how many TUP blocks are configured"){
  STEP(runTestTable);
}
NDBT_TESTSUITE_END(testNdbinfo);


int main(int argc, const char** argv){
  ndb_init();
  NDBT_TESTSUITE_INSTANCE(testNdbinfo);
  testNdbinfo.setCreateTable(false);
  testNdbinfo.setRunAllTables(true);
  return testNdbinfo.execute(argc, argv);
}
