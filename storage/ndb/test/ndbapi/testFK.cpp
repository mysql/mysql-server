/*
   Copyright (c) 2013, 2023, Oracle and/or its affiliates.

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

#include "util/require.h"
#include <cstring>
#include <NDBT.hpp>
#include <NDBT_Test.hpp>
#include <HugoTransactions.hpp>
#include <UtilTransactions.hpp>
#include <NdbRestarter.hpp>
#include <NdbRestarts.hpp>
#include <Vector.hpp>
#include <signaldata/DumpStateOrd.hpp>
#include <NodeBitmask.hpp>
#include <NdbEnv.h>
#include "portlib/NdbSleep.h"

#define DBG(x) \
  do { g_info << x << " at line " << __LINE__ << endl; } while (0)

#define CHK1(b) \
  if (!(b)) { \
    g_err << "ERR: " << #b << " failed at line " << __LINE__ \
          << endl; \
    return NDBT_FAILED; \
  }

#define CHK2(b, e) \
  if (!(b)) { \
    g_err << "ERR: " << #b << " failed at line " << __LINE__ \
          << ": " << e << endl; \
    return NDBT_FAILED; \
  }

#define CHK_RET_FAILED(x) if (!(x)) return NDBT_FAILED

static int runLongSignalMemorySnapshot(NDBT_Context* ctx, NDBT_Step* step);

/**
 * Choose a low batch size to avoid trigger out of buffer problems...
 */
#define DEFAULT_BATCH_SIZE 5

#define DEFAULT_FK_RAND Uint32(0)
#define DEFAULT_FK_UNIQ Uint32(2)
#define DEFAULT_FK_MANY Uint32(1)

#define DEFAULT_IDX_RAND Uint32(0)
#define DEFAULT_IDX_UNIQ Uint32(2)
#define DEFAULT_IDX_MANY Uint32(1)

#define T_RAND 0
#define T_UNIQ 1
#define T_MANY 2

#define T_PK_IDX 1
#define T_UK_IDX 2

#define PKNAME "$PK$"

extern unsigned opt_seed;

static int schema_rand_init = 0;
static unsigned schema_rand_seed = 0;

static
int
schema_rand()
{
  if (schema_rand_init == 0)
  {
    schema_rand_init = 1;
    schema_rand_seed = opt_seed;
  }
  return ndb_rand_r(&schema_rand_seed);
}

static
int
runLoadTable(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  const int rows = ctx->getNumRecords();
  const int batchSize = ctx->getProperty("BatchSize", DEFAULT_BATCH_SIZE);
  HugoTransactions hugoTrans(*ctx->getTab());

  const bool concurrent = false;
  CHK2(hugoTrans.loadTable(pNdb, rows, batchSize, concurrent) == 0,
       "rows:" << rows << ", batchSize:" << batchSize);

  return NDBT_OK;
}


static
int
runClearTable(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  const int parallel = 10 * (rand() % 5);

  UtilTransactions utilTrans(*ctx->getTab());
  CHK2(utilTrans.clearTable(pNdb, 0, parallel) == 0,
       "Table :" << ctx->getTab()->getName());

  return NDBT_OK;
}

static
int
runTransactions(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  const int rows = ctx->getNumRecords();
  const int batchSize = ctx->getProperty("BatchSize", DEFAULT_BATCH_SIZE);
  const int parallel = 10 * (rand() % 5);
  const int loops = ctx->getNumLoops();
  const bool until_stopped = ctx->getProperty("TransactionsUntilStopped",
                                              Uint32(0));
  const bool concurrent = ctx->getProperty("concurrent", Uint32(0)) != 0;

  HugoTransactions hugoTrans(*ctx->getTab());
  UtilTransactions utilTrans(*ctx->getTab());

  const int expectrows = concurrent ? 0 : rows;

  for (int i = 0; ((i < loops) || until_stopped) && !ctx->isTestStopped(); i++)
  {
    CHK2((hugoTrans.loadTable(pNdb, rows, batchSize, concurrent) == 0),
          "rows:" << rows << ", batchSize:" << batchSize << ", concurrent:" << concurrent);

    if (ctx->isTestStopped())
      break;

    if (concurrent == false)
    {
      CHK2((hugoTrans.pkUpdateRecords(pNdb, rows, batchSize) == 0),
            "rows:" << rows << ", batchSize:" << batchSize);
    }

    if (ctx->isTestStopped())
      break;

    CHK2((hugoTrans.scanUpdateRecords(pNdb, expectrows, 5, parallel) == 0),
          "expectrows:" << expectrows << ", parallel:" << parallel);

    if (ctx->isTestStopped())
      break;

    CHK2((utilTrans.clearTable(pNdb, expectrows, parallel) == 0),
          "expectrows:" << expectrows << ", parallel:" << parallel);
  }
  return NDBT_OK;
}

int
runMixedDML(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  const NdbDictionary::Table* pTab = ctx->getTab();

  unsigned seed = (unsigned)NdbTick_CurrentMillisecond();

  const int rows = ctx->getNumRecords();
  const int loops = 10 * ctx->getNumLoops();
  const int until_stopped = ctx->getProperty("TransactionsUntilStopped");
  const int deferred = ctx->getProperty("Deferred");
  const int minbatch = ctx->getProperty("MinBatch", Uint32(10));
  const int maxbatch = ctx->getProperty("MaxBatch", Uint32(50));
  const int longsignalmemorysnapshot =
    ctx->getProperty("LongSignalMemorySnapshot", Uint32(0));

  const NdbRecord * pRowRecord = pTab->getDefaultRecord();

  const Uint32 len = NdbDictionary::getRecordRowLength(pRowRecord);
  Uint8 * pRow = new Uint8[len];

  int result = 0;
  int count_ok = 0;
  int count_failed = 0;
  NdbError err;
  for (int i = 0; i < loops || (until_stopped && !ctx->isTestStopped()); i++)
  {
    NdbTransaction* pTrans = pNdb->startTransaction();
    if (pTrans == 0)
    {
      err = pNdb->getNdbError();
      goto start_err;
    }

    {
      int lastrow = 0;
      int batch = minbatch + (rand() % (maxbatch - minbatch));
      for (int rowNo = 0; rowNo < batch; rowNo++)
      {
        int left = rows - lastrow;
        int rowId = lastrow;
        if (left)
        {
          rowId += ndb_rand_r(&seed) % (left / 10 + 1);
        }
        else
        {
          break;
        }
        lastrow = rowId;

        std::memset(pRow, 0, len);

        HugoCalculator calc(* pTab);
        calc.setValues(pRow, pRowRecord, rowId, rand());

        NdbOperation::OperationOptions opts;
        std::memset(&opts, 0, sizeof(opts));
        if (deferred)
        {
          opts.optionsPresent =
            NdbOperation::OperationOptions::OO_DEFERRED_CONSTAINTS;
        }

        const NdbOperation* pOp = 0;
        switch(ndb_rand_r(&seed) % 3){
        case 0:
          pOp = pTrans->writeTuple(pRowRecord, (char*)pRow,
                                   pRowRecord, (char*)pRow,
                                   0,
                                   &opts,
                                   sizeof(opts));
          break;
        case 1:
          pOp = pTrans->deleteTuple(pRowRecord, (char*)pRow,
                                    pRowRecord, (char*)pRow,
                                    0,
                                    &opts,
                                    sizeof(opts));
          break;
        case 2:
          pOp = pTrans->updateTuple(pRowRecord, (char*)pRow,
                                    pRowRecord, (char*)pRow,
                                    0,
                                    &opts,
                                    sizeof(opts));
          break;
        }
        CHK_RET_FAILED(pOp != 0);
        result = pTrans->execute(NoCommit, AO_IgnoreError);
        if (result != 0)
        {
          goto found_error;
        }
      }
    }
    result = pTrans->execute(Commit, AO_IgnoreError);
    if (result != 0)
    {
  found_error:
      err = pTrans->getNdbError();
  start_err:
      count_failed++;
      ndbout << err << endl;
      CHK_RET_FAILED(err.code == 1235 ||
                     err.code == 1236 ||
                     err.code == 5066 ||
                     err.status == NdbError::TemporaryError ||
                     err.classification == NdbError::NoDataFound ||
                     err.classification == NdbError::ConstraintViolation);

      if (longsignalmemorysnapshot)
      {
        runLongSignalMemorySnapshot(ctx, step);
      }
    }
    else
    {
      count_ok++;
    }
    pTrans->close();
  }

  ndbout_c("count_ok: %d count_failed: %d",
           count_ok, count_failed);
  delete [] pRow;

  return NDBT_OK;
}

static unsigned tableIndexes = 0; // #indexes on table @start
static unsigned tableFKs = 0;     // #fks on table @start
static Vector<const NdbDictionary::Index*> indexes;
static Vector<const NdbDictionary::ForeignKey *> fks;

static
int
runDiscoverTable(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  const char * tableName = ctx->getTab()->getName();

  NdbDictionary::Dictionary * dict = pNdb->getDictionary();
  const NdbDictionary::Table * pTab = dict->getTable(tableName);

  tableIndexes = tableFKs = 0;
  indexes.clear();
  fks.clear();

  /**
   * Create "fake" unique hash index representing PK has index...
   *   for easier logic...
   */
  {
    NdbDictionary::Index * pIdx = new NdbDictionary::Index(PKNAME);
    pIdx->setTable(tableName);
    pIdx->setType(NdbDictionary::Index::UniqueHashIndex);
    for (int i = 0; i < pTab->getNoOfColumns(); i++)
    {
      if (pTab->getColumn(i)->getPrimaryKey())
      {
        pIdx->addIndexColumn(pTab->getColumn(i)->getName());
      }
    }
    indexes.push_back(pIdx);
  }

  /**
   * List...
   */
  {
    NdbDictionary::Dictionary::List list;
    dict->listDependentObjects(list, * pTab);
    for (unsigned i = 0; i < list.count; i++)
    {
      switch(list.elements[i].type){
      case NdbDictionary::Object::UniqueHashIndex:
      case NdbDictionary::Object::OrderedIndex:{
        const NdbDictionary::Index * p = dict->getIndex(list.elements[i].name,
                                                        * pTab);
        if (p != 0)
        {
          indexes.push_back(p);
        }
        break;
      }
      case NdbDictionary::Object::ForeignKey:{
        NdbDictionary::ForeignKey fk;
        if (dict->getForeignKey(fk, list.elements[i].name) == 0)
        {
          fks.push_back(new NdbDictionary::ForeignKey(fk));
        }
        break;
      }
      default:
        break;
      }
    }
  }
  tableFKs = fks.size();
  tableIndexes = indexes.size();

  return NDBT_OK;
}

static
bool
match(const NdbDictionary::Index * parent,
      const NdbDictionary::Index * childCandidate)
{
  if (childCandidate->getNoOfColumns() < parent->getNoOfColumns())
    return false;

  if (childCandidate->getType() == NdbDictionary::Index::UniqueHashIndex)
  {
    if (childCandidate->getNoOfColumns() != parent->getNoOfColumns())
      return false;
  }

  for (unsigned i = 0; i < parent->getNoOfColumns(); i++)
  {
    if (strcmp(parent->getColumn(i)->getName(),
               childCandidate->getColumn(i)->getName()) != 0)
      return false;
  }

  return true;
}

static
const NdbDictionary::Column *
find(const NdbDictionary::Table * pTab, const char * name)
{
  for (int i = 0; i < pTab->getNoOfColumns(); i++)
    if (strcmp(pTab->getColumn(i)->getName(), name) == 0)
      return pTab->getColumn(i);

  return 0;
}

static
const NdbDictionary::Column *
find(const NdbDictionary::Index * pIdx, const char * name)
{
  for (unsigned i = 0; i < pIdx->getNoOfColumns(); i++)
    if (strcmp(pIdx->getColumn(i)->getName(), name) == 0)
      return pIdx->getColumn(i);

  return 0;
}

static
bool
nullonly(const NdbDictionary::Table * pTab,
         const NdbDictionary::Index * pIdx,
         unsigned cnt)
{
  for (unsigned i = 0; i < cnt; i++)
  {
    if (find(pTab, pIdx->getColumn(i)->getName())->getNullable() == false)
      return false;
  }
  return true;
}

static
const NdbDictionary::Index*
findOI(const NdbDictionary::Index * pIdx,
       Vector<const NdbDictionary::Index*>& list)
{
  for (unsigned i = 0; i < list.size(); i++)
  {
    if (list[i]->getType() == NdbDictionary::Index::OrderedIndex)
    {
      if (list[i]->getNoOfColumns() < pIdx->getNoOfColumns())
        continue;

      bool found = true;
      for (unsigned c = 0; c < pIdx->getNoOfColumns(); c++)
      {
        if (strcmp(pIdx->getColumn(c)->getName(),
                   list[i]->getColumn(c)->getName()) != 0)
        {
          found = false;
          break;
        }
      }
      if (found)
      {
        return list[i];
      }
    }
  }
  return 0;
}

static
bool
indexable(const NdbDictionary::Column * c)
{
  if (c->getType() == NdbDictionary::Column::Blob ||
      c->getType() == NdbDictionary::Column::Text ||
      c->getType() == NdbDictionary::Column::Bit ||
      c->getStorageType() == NdbDictionary::Column::StorageTypeDisk)
  {
    return false;
  }
  return true;
}

static
int
createIDX(NdbDictionary::Dictionary * dict,
          const NdbDictionary::Table * pTab,
          int type)
{
  /**
   * 1) Create OI for every unique index
   */
  if (type == T_RAND || type == T_MANY)
  {
    for (unsigned i = 0; i < indexes.size(); i++)
    {
      if (indexes[i]->getType() == NdbDictionary::Index::UniqueHashIndex)
      {
        bool f = (findOI(indexes[i], indexes) != 0);
        if (f == false)
        {
          BaseString tmp;
          tmp.assfmt("IDX_%s_%u", pTab->getName(), indexes.size());
          NdbDictionary::Index pIdx(tmp.c_str());
          pIdx.setTable(pTab->getName());
          pIdx.setType(NdbDictionary::Index::OrderedIndex);
          pIdx.setStoredIndex(false);
          for (unsigned c = 0; c < indexes[i]->getNoOfColumns(); c++)
          {
            pIdx.addIndexColumn(indexes[i]->getColumn(c)->getName());
          }

          DBG("CREATE index " << pIdx.getName());
          CHK2(dict->createIndex(pIdx) == 0,
               tmp.c_str() << ": " << dict->getNdbError());

          const NdbDictionary::Index * idx = dict->getIndex(tmp.c_str(),
                                                            pTab->getName());
          if (idx != 0)
          {
            indexes.push_back(idx);
          }

          return NDBT_OK;
        }
      }
    }
  }

  if (type == T_MANY)
  {
    return NDBT_WRONGARGS;
  }

  /**
   * 2) Create a new unique index...(include PK...to make unique)
   */
  {
    BaseString tmp;
    tmp.assfmt("IDX_%s_%u", pTab->getName(), indexes.size());
    NdbDictionary::Index pIdx(tmp.c_str());
    pIdx.setTable(pTab->getName());
    pIdx.setType(NdbDictionary::Index::UniqueHashIndex);
    pIdx.setStoredIndex(false);
    for (int c = 0; c < pTab->getNoOfColumns(); c++)
    {
      if (pTab->getColumn(c)->getPrimaryKey())
      {
        pIdx.addIndexColumn(pTab->getColumn(c)->getName());
      }
    }

    /**
     * How many possible columns do we have "left"
     */
    unsigned possible = pTab->getNoOfColumns() - pIdx.getNoOfColumns();

    if (possible > NDB_MAX_ATTRIBUTES_IN_INDEX)
      possible = NDB_MAX_ATTRIBUTES_IN_INDEX - 1;

    if (possible > 0)
    {
      unsigned add = possible == 1 ? 1 :
        (1 + (schema_rand() % (possible - 1)));
      for (unsigned i = 0; i < add; i++)
      {
        int c = schema_rand() % pTab->getNoOfColumns();
        do
        {
          c = (c + 1) % pTab->getNoOfColumns();
          const NdbDictionary::Column * col = pTab->getColumn(c);
          if (!indexable(col))
          {
            add--;
            continue;
          }
          if (col->getPrimaryKey())
            continue;

          if (find(&pIdx, col->getName()) != 0)
            continue;

          break;

        } while (add > 0);

        if (add > 0)
        {
          pIdx.addIndexColumn(pTab->getColumn(c)->getName());
        }
      }
    }

    DBG("CREATE index " << pIdx.getName());
    CHK2(dict->createIndex(pIdx) == 0,
         tmp.c_str() << ": " << dict->getNdbError());

    const NdbDictionary::Index * idx = dict->getIndex(tmp.c_str(),
                                                      pTab->getName());
    if (idx != 0)
    {
      indexes.push_back(idx);
    }
  }
  return NDBT_OK;
}

static
int
createFK(NdbDictionary::Dictionary * dict,
         const NdbDictionary::Table * pParent,
         int parent_type,
         const NdbDictionary::Table * pChild,
         int type,
         unsigned onupdateactionmask = ~(unsigned)0,
         unsigned ondeleteactionmask = ~(unsigned)0)
{
  /**
   * Note, it's assumed that pParent and pChild has identical structure
   *       and indexes
   */

  const NdbDictionary::Index * parentIdx = 0;
  const NdbDictionary::Index * childIdx = 0;

  /**
   * Create self referencing FK based on random index...
   */
  {
    unsigned p = schema_rand() % indexes.size();
    for (unsigned i = 0; i < indexes.size(); i++)
    {
      unsigned no = (i+p) % indexes.size();
      if (indexes[no]->getType() == NdbDictionary::Index::UniqueHashIndex)
      {
        bool pk = strcmp(indexes[no]->getName(), PKNAME) == 0;
        if (parent_type == T_RAND ||
            (parent_type == T_PK_IDX && pk == true) ||
            (parent_type == T_UK_IDX && pk == false))
        {
          parentIdx = indexes[no];
          break;
        }
      }
    }
  }

  if (parentIdx == 0)
  {
    return NDBT_WRONGARGS;
  }

  /**
   * Find child index...
   */
  {
    unsigned p = schema_rand() % indexes.size();
    for (unsigned i = 0; i < indexes.size(); i++)
    {
      unsigned no = (i+p) % indexes.size();
      if (match(parentIdx, indexes[no]) &&
          (type == T_RAND
           ||
           (type == T_MANY &&
            indexes[no]->getType() == NdbDictionary::Index::OrderedIndex)
           ||
           (type == T_UNIQ &&
            indexes[no]->getType() == NdbDictionary::Index::UniqueHashIndex)))
      {
        childIdx = indexes[no];
        break;
      }
    }
  }

  if (childIdx == 0)
  {
    return NDBT_WRONGARGS;
  }

  if (strcmp(childIdx->getName(), PKNAME) != 0)
  {
    const NdbDictionary::Index * idx = dict->getIndex(childIdx->getName(),
                                                      pChild->getName());
    require(idx != 0);
    childIdx = idx;
  }

  const NdbDictionary::Column * cols[NDB_MAX_ATTRIBUTES_IN_INDEX + 1];
  for (unsigned i = 0; i < parentIdx->getNoOfColumns(); i++)
  {
    cols[i] = find(pParent, parentIdx->getColumn(i)->getName());
  }
  cols[parentIdx->getNoOfColumns()] = 0;

  NdbDictionary::ForeignKey ndbfk;
  BaseString name;
  name.assfmt("FK_%s_%u", pParent->getName(), fks.size());
  ndbfk.setName(name.c_str());
  ndbfk.setParent(* pParent,
                  (strcmp(parentIdx->getName(), PKNAME) == 0 ? 0 : parentIdx),
                  cols);
  ndbfk.setChild(* pChild,
                 (strcmp(childIdx->getName(), PKNAME) == 0 ? 0 : childIdx),
                 cols);

  const unsigned alt_update =
    (strcmp(parentIdx->getName(), PKNAME) == 0) ? 2 :
    (nullonly(pChild, childIdx, parentIdx->getNoOfColumns())) ? 4
    : 3;

  if ((((1 << alt_update) - 1) & onupdateactionmask) == 0)
  {
    return NDBT_WRONGARGS;
  }
  int val = 0;
  do {
    val = schema_rand() % alt_update;
  } while (((1 << val) & onupdateactionmask) == 0);
  switch(val) {
  case 0:
    ndbfk.setOnUpdateAction(NdbDictionary::ForeignKey::NoAction);
    break;
  case 1:
    ndbfk.setOnUpdateAction(NdbDictionary::ForeignKey::Restrict);
    break;
  case 2:
    ndbfk.setOnUpdateAction(NdbDictionary::ForeignKey::Cascade);
    break;
  case 3:
    ndbfk.setOnUpdateAction(NdbDictionary::ForeignKey::SetNull);
    break;
  case 4:
    ndbfk.setOnUpdateAction(NdbDictionary::ForeignKey::SetDefault);
    break;
  }

  const unsigned alt_delete =
    (nullonly(pChild, childIdx, parentIdx->getNoOfColumns())) ? 4
    : 3;

  if ((((1 << alt_delete) - 1) & ondeleteactionmask) == 0)
  {
    return NDBT_WRONGARGS;
  }
  val = 0;
  do {
    val = schema_rand() % alt_delete;
  } while (((1 << val) & ondeleteactionmask) == 0);

  switch(val) {
  case 0:
    ndbfk.setOnDeleteAction(NdbDictionary::ForeignKey::NoAction);
    break;
  case 1:
    ndbfk.setOnDeleteAction(NdbDictionary::ForeignKey::Restrict);
    break;
  case 2:
    ndbfk.setOnDeleteAction(NdbDictionary::ForeignKey::Cascade);
    break;
  case 3:
    ndbfk.setOnDeleteAction(NdbDictionary::ForeignKey::SetNull);
    break;
  case 4:
    ndbfk.setOnDeleteAction(NdbDictionary::ForeignKey::SetDefault);
    break;
  }

  CHK2(dict->createForeignKey(ndbfk) == 0,
       pChild->getName() << ": " << dict->getNdbError());

  {
    // bug#19122346 TODO: provide new NdbDictionary methods
    char fullname[MAX_TAB_NAME_SIZE];
    sprintf(fullname, "%d/%d/%s", pParent->getObjectId(), pChild->getObjectId(),
                                  ndbfk.getName());
    NdbDictionary::ForeignKey * get = new NdbDictionary::ForeignKey();
    DBG("CREATE fk " << fullname);
    CHK2(dict->getForeignKey(* get, fullname) == 0,
         fullname << ": " << dict->getNdbError());
    fks.push_back(get);
  }
  return NDBT_OK;
}

static
int
runCreateRandom(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  const char * tableName = ctx->getTab()->getName();

  NdbDictionary::Dictionary * dict = pNdb->getDictionary();
  const NdbDictionary::Table * pTab = dict->getTable(tableName);

  const int uiindexcnt = ctx->getProperty("IDX_UNIQ", DEFAULT_IDX_UNIQ);
  const int oiindexcnt = ctx->getProperty("IDX_MANY", DEFAULT_IDX_MANY);
  const int indexcnt   = ctx->getProperty("IDX_RAND", DEFAULT_IDX_RAND);

  const int uifkcount = ctx->getProperty("FK_UNIQ", DEFAULT_FK_UNIQ);
  const int oifkcount = ctx->getProperty("FK_MANY", DEFAULT_FK_MANY);
  const int fkcount   = ctx->getProperty("FK_RAND", DEFAULT_FK_RAND);

  for (int i = 0; i < indexcnt; i++)
  {
    CHK1(createIDX(dict, pTab, T_RAND) != NDBT_FAILED);
  }
  for (int i = 0; i < uiindexcnt; i++)
  {
    CHK1(createIDX(dict, pTab, T_UNIQ) != NDBT_FAILED);
  }
  for (int i = 0; i < oiindexcnt; i++)
  {
    CHK1(createIDX(dict, pTab, T_MANY) != NDBT_FAILED);
  }
  for (int i = 0; i < fkcount; i++)
  {
    CHK1(createFK(dict, pTab, T_RAND, pTab, T_RAND) != NDBT_FAILED);
  }
  for (int i = 0; i < uifkcount; i++)
  {
    CHK1(createFK(dict, pTab, T_RAND, pTab, T_UNIQ) != NDBT_FAILED);
  }
  for (int i = 0; i < oifkcount; i++)
  {
    CHK1(createFK(dict, pTab, T_RAND, pTab, T_MANY) != NDBT_FAILED);
  }

  if (1)
  {
    ndbout << "DESC " << pTab->getName() << endl;
    dict->print(ndbout, * pTab);
  }

  return NDBT_OK;
}

static
int
runCleanupTable(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  const char * tableName = ctx->getTab()->getName();

  ndbout << "cleanup " << tableName << endl;
  NdbDictionary::Dictionary * dict = pNdb->getDictionary();
  while (fks.size() > tableFKs)
  {
    unsigned last = fks.size() - 1;
    DBG("DROP fk " << fks[last]->getName());
    CHK2(dict->dropForeignKey(* fks[last]) == 0,
        fks[last]->getName() << ": " << dict->getNdbError());
    delete fks[last];
    fks.erase(last);
  }
  ndbout << "FK done" << endl;

  while (indexes.size() > tableIndexes)
  {
    unsigned last = indexes.size() - 1;
    DBG("DROP index " << indexes[last]->getName());
    CHK2(dict->dropIndex(indexes[last]->getName(), tableName) == 0,
        indexes[last]->getName() << ": " << dict->getNdbError());
    indexes.erase(last);
  }

  ndbout << "indexes done" << endl;

  return NDBT_OK;
}

static
int
runCreateDropRandom(NDBT_Context* ctx, NDBT_Step* step)
{
  const int loops = ctx->getNumLoops();

  for (int i = 0; i < loops; i++)
  {
    CHK1(runCreateRandom(ctx, step) == NDBT_OK);

    if (ctx->getProperty("CreateAndLoad", Uint32(0)) != 0)
    {
      CHK1(runLoadTable(ctx, step) == NDBT_OK);
      CHK1(runClearTable(ctx, step) == NDBT_OK);
    }
    CHK1(runCleanupTable(ctx, step) == NDBT_OK);
  }

  ctx->stopTest();

  return NDBT_OK;
}

static
int
runCreateDropError(NDBT_Context* ctx, NDBT_Step* step)
{
  /**
   * TODO test create/drop FK with error insert,
   *      make sure that no resources are leaked
   */
  return NDBT_OK;
}

static
int
runRSSsnapshot(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter restarter;
  g_info << "save all resource usage" << endl;
  int dump1[] = { DumpStateOrd::SchemaResourceSnapshot };
  restarter.dumpStateAllNodes(dump1, 1);
  return NDBT_OK;
}

static
int
runRSSsnapshotCheck(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter restarter;
  g_info << "check all resource usage" << endl;
  NdbSleep_SecSleep(2);
  int dump1[] = { DumpStateOrd::SchemaResourceCheckLeak };
  restarter.dumpStateAllNodes(dump1, 1);
  return NDBT_OK;
}

/**
 * BUG#19643174
 * 
 * Test cases that use TcResourceSnapshot and TcResourceCheckLeak have
 * to be protected from race conditions. There are multiple variants of
 * races to protect against.
 * 
 * 1) We wake up the user thread before we send the TC_COMMIT_ACK, this could
 * lead to that we haven't released the commit ack markers before our
 * DUMP_STATE_ORD arrives in the DBTC instances. To handle this we set
 * TC_COMMIT_ACK to be sent immediate and even before the user thread is
 * signalled.
 * 
 * 2) The sending of TC_COMMIT_ACK uses a method to send the signal without
 * flushing for performance reasons. However in this case we need it to be
 * sent immediate, this is also handled by the same flag as for 1).
 *
 * 3) The sending of DUMP_STATE_ORD can race the TC_COMMIT_ACK if we send it
 * through the management server. To avoid this we send it directly to all
 * nodes through a signal.
 *
 * 4) The TC_COMMIT_ACK can still be raced by the DUMP_STATE_ORD if they arrive
 * in the same TCP/IP message. This is so since the data node receiver will
 * not flush the signals to the threads until it has received all signals or
 * some maximum value. When flushing it starts with low thread numbers, so the
 * thread where CMVMI belongs (the main thread) will get its signal flushed
 * before the TC threads gets their signals flushed. This means that a signal
 * directly to TC can be raced by a signal to the same TC routed via the thread
 * of the CMVMI. To avoid this we always route TC_COMMIT_ACK via CMVMI when
 * the immediate flag has been set.
 *
 * The above 4 measures handles the TC_COMMIT_ACK resources. There is however
 * also a number of resources kept until the complete phase is processed.
 * There is no signal sent back to the API when the complete phase is
 * completed, so there isn't much we can do in that respect. There is however
 * a signal WAIT_GCP_REQ that can be sent that waits for the current global
 * checkpoint to complete before sending WAIT_GCP_CONF, given that we have
 * received a transaction with a certain GCP, we know that this signal will
 * not return until the complete phase of our transactions are completed.
 * It will actually wait also for the logs to be written and so forth, but
 * this extra wait doesn't matter since it is simply delaying the test case
 * somewhat. So by adding a call to forceGCPWait(1) we ensure that the
 * complete phase is done before we proceed with checking for memory leaks.
 */

#include "../../src/ndbapi/ndb_internal.hpp"

int
runTransSnapshot(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter restarter;
  Ndb *pNdb = GETNDB(step);
  g_info << "save all resource usage" << endl;
  int dump1[] = { DumpStateOrd::TcResourceSnapshot };
  restarter.dumpStateAllNodes(dump1, 1);
  Ndb_internal::set_TC_COMMIT_ACK_immediate(pNdb, true);
  return NDBT_OK;
}

int
runTransSnapshotCheck(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb *pNdb = GETNDB(step);
  NdbDictionary::Dictionary *pDict = pNdb->getDictionary();
  g_info << "save all resource usage" << endl;
  pDict->forceGCPWait(1);
  Uint32 dump1[] = { DumpStateOrd::TcResourceCheckLeak };
  CHK1(Ndb_internal::send_dump_state_all(pNdb, dump1, 1) == 0);
  return NDBT_OK;
}

static
int
runLongSignalMemorySnapshotStart(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter restarter;
  g_info << "save all resource usage" << endl;
  int dump1[] = { DumpStateOrd::CmvmiLongSignalMemorySnapshotStart };
  restarter.dumpStateAllNodes(dump1, 1);
  return NDBT_OK;
}

static
int
runLongSignalMemorySnapshot(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter restarter;
  g_info << "save all resource usage" << endl;
  int dump1[] = { DumpStateOrd::CmvmiLongSignalMemorySnapshot };
  restarter.dumpStateAllNodes(dump1, 1);
  return NDBT_OK;
}

static
int
runLongSignalMemorySnapshotCheck(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter restarter;
  g_info << "save all resource usage" << endl;
  int dump1[] = { DumpStateOrd::CmvmiLongSignalMemorySnapshotCheck };
  restarter.dumpStateAllNodes(dump1, 1);
  return NDBT_OK;
}

static
int
runCreateCascadeChild(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  NdbDictionary::Dictionary* dict = pNdb->getDictionary();

  /**
   * We want to create a ON UPDATE CASCADE ON DELETE CASCADE
   *
   * We need a UK parent index
   * - we don't support ON UPDATE CASCADE for PK since we don't support
   *   update PK
   *
   * We need a PK, UK or OI child index
   */
  const NdbDictionary::Table * pTab = dict->getTable(ctx->getTab()->getName());
  for (int i = 0; i < 3; i++)
  {
    createIDX(dict, pTab, T_UNIQ);
    createIDX(dict, pTab, T_MANY);
  }

  /**
   * Now create a identical CHILD table
   */
  BaseString childname;
  childname.assfmt("%s_CHILD", pTab->getName());
  NdbDictionary::Table child(* pTab);
  child.setName(childname.c_str());

  if (dict->getTable(child.getName()) == 0)
  {
    CHK2(dict->getNdbError().code == 723,
         child.getName() << ": " << dict->getNdbError());
  }
  else
  {
    DBG("DROP old table" << child.getName());
    CHK2(dict->dropTable(child.getName()) == 0,
         child.getName() << ": " << dict->getNdbError());
  }

  DBG("CREATE table " << child.getName());
  CHK2(dict->createTable(child) == 0,
       child.getName() << ": " << dict->getNdbError());

  const NdbDictionary::Table * pChild = dict->getTable(childname.c_str());
  {
    NdbDictionary::Dictionary::List list;
    CHK2(dict->listIndexes(list, *pTab) == 0,
         child.getName() << ": " << dict->getNdbError());

    for (unsigned i = 0; i<list.count; i++)
    {
      const NdbDictionary::Index* idx = dict->getIndex(list.elements[i].name,
                                                       pTab->getName());
      if (idx)
      {
        NdbDictionary::Index copy;
        copy.setName(idx->getName());
        copy.setType(idx->getType());
        copy.setLogging(idx->getLogging());
        copy.setTable(pChild->getName());
        for (unsigned j = 0; j<idx->getNoOfColumns(); j++)
        {
          copy.addColumn(idx->getColumn(j)->getName());
        }
        DBG("CREATE index " << copy.getName());
        CHK2(dict->createIndex(copy) == 0,
             copy.getName() << ": " << dict->getNdbError());
      }
    }
  }

  /**
   * Now create FK
   */
  CHK1(createFK(dict, pTab, T_UK_IDX, pChild, T_RAND,
                (1 << NDB_FK_CASCADE),
                (1 << NDB_FK_CASCADE)) == 0);

  if (1)
  {
    ndbout << "DESC " << pChild->getName() << endl;
    dict->print(ndbout, * pChild);
  }

  const int rows = ctx->getNumRecords();
  const int batchSize = ctx->getProperty("BatchSize", DEFAULT_BATCH_SIZE);

  const NdbDictionary::Table * tables[] = { pChild, pTab, 0 };
  for (int i = 0; tables[i] != 0; i++)
  {
    HugoTransactions c(* tables[i]);
    CHK2(c.loadTable(pNdb, rows, batchSize) == 0,
         "Load table failed");
  }

  return NDBT_OK;
}

static
int
runMixedCascade(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  const NdbDictionary::Table* pTab = ctx->getTab();
  BaseString childname;
  childname.assfmt("%s_CHILD", pTab->getName());
  const NdbDictionary::Table * pChild =
    pNdb->getDictionary()->getTable(childname.c_str());

  unsigned seed = (unsigned)NdbTick_CurrentMillisecond();

  const int rows = ctx->getNumRecords();
  const int loops = 10 * ctx->getNumLoops();
  const int until_stopped = ctx->getProperty("TransactionsUntilStopped");
  const int deferred = ctx->getProperty("Deferred");
  const int minbatch = ctx->getProperty("MinBatch", Uint32(10));
  const int maxbatch = ctx->getProperty("MaxBatch", Uint32(50));
  const int longsignalmemorysnapshot =
    ctx->getProperty("LongSignalMemorySnapshot", Uint32(0));

  const NdbRecord * pRowRecord = pTab->getDefaultRecord();
  const NdbRecord * pRowRecord1 = pChild->getDefaultRecord();

  const Uint32 len = NdbDictionary::getRecordRowLength(pRowRecord);
  Uint8 * pRow = new Uint8[len];

  int result = 0;
  int count_ok = 0;
  int count_failed = 0;
  NdbError err;
  for (int i = 0; i < loops || (until_stopped && !ctx->isTestStopped()); i++)
  {
    NdbTransaction* pTrans = pNdb->startTransaction();
    if (pTrans == 0)
    {
      err = pNdb->getNdbError();
      goto start_err;
    }

    {
      int lastrow = 0;
      int batch = minbatch + (rand() % (maxbatch - minbatch));
      for (int rowNo = 0; rowNo < batch; rowNo++)
      {
        int left = rows - lastrow;
        int rowId = lastrow;
        if (left)
        {
          rowId += ndb_rand_r(&seed) % (left / 10 + 1);
        }
        else
        {
          break;
        }
        lastrow = rowId;

        std::memset(pRow, 0, len);

        HugoCalculator calc(* pTab);
        calc.setValues(pRow, pRowRecord, rowId, rand());

        NdbOperation::OperationOptions opts;
        std::memset(&opts, 0, sizeof(opts));
        if (deferred)
        {
          opts.optionsPresent =
            NdbOperation::OperationOptions::OO_DEFERRED_CONSTAINTS;
        }

        const NdbOperation* pOp = 0, * pOp1 = 0;
        switch(ndb_rand_r(&seed) % 3){
        case 0:
          pOp = pTrans->writeTuple(pRowRecord, (char*)pRow,
                                   pRowRecord, (char*)pRow,
                                   0,
                                   &opts,
                                   sizeof(opts));
          result = pTrans->execute(NoCommit, AO_IgnoreError);
          if (result != 0)
            goto found_error;
          pOp1 = pTrans->writeTuple(pRowRecord1, (char*)pRow,
                                    pRowRecord1, (char*)pRow,
                                    0,
                                    &opts,
                                    sizeof(opts));
          break;
        case 1:
          pOp = pTrans->deleteTuple(pRowRecord, (char*)pRow,
                                    pRowRecord, (char*)pRow,
                                    0,
                                    &opts,
                                    sizeof(opts));
          break;
        case 2:
          pOp = pTrans->updateTuple(pRowRecord, (char*)pRow,
                                    pRowRecord, (char*)pRow,
                                    0,
                                    &opts,
                                    sizeof(opts));
          break;
        }
        CHK_RET_FAILED(pOp != 0);
        result = pTrans->execute(NoCommit, AO_IgnoreError);
        if (result != 0)
        {
          goto found_error;
        }
      }
    }
    result = pTrans->execute(Commit, AO_IgnoreError);
    if (result != 0)
    {
  found_error:
      err = pTrans->getNdbError();
  start_err:
      count_failed++;
      ndbout << err << endl;
      CHK_RET_FAILED(err.code == 1235 ||
                     err.code == 1236 ||
                     err.code == 5066 ||
                     err.status == NdbError::TemporaryError ||
                     err.classification == NdbError::NoDataFound ||
                     err.classification == NdbError::ConstraintViolation);

      if (longsignalmemorysnapshot)
      {
        runLongSignalMemorySnapshot(ctx, step);
      }
    }
    else
    {
      count_ok++;
    }
    pTrans->close();
  }

  ndbout_c("count_ok: %d count_failed: %d",
           count_ok, count_failed);
  delete [] pRow;
  return NDBT_OK;
}

static
int
runDropCascadeChild(NDBT_Context* ctx, NDBT_Step* step)
{
  Ndb* pNdb = GETNDB(step);
  const NdbDictionary::Table* pTab = ctx->getTab();
  BaseString childname;
  childname.assfmt("%s_CHILD", pTab->getName());

  DBG("DROP table " << childname.c_str());
  CHK2(pNdb->getDictionary()->dropTable(childname.c_str()) == 0,
       pNdb->getDictionary()->getNdbError());

  return NDBT_OK;
}

int
runRestartOneNodeNoStart(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter restarter;

  /* choose a random node and restart with nostart */
  int nodeId= restarter.getDbNodeId(rand() % restarter.getNumDbNodes());
  restarter.restartOneDbNode(nodeId, false, true);
  /* wait for it to go to no start phase */
  CHK2(restarter.waitNodesNoStart(&nodeId, 1) == 0,
       "Unable to restart node");
  return NDBT_OK;
}

int
runStartAllNodes(NDBT_Context* ctx, NDBT_Step* step){
  NdbRestarter restarter;

  CHK2(restarter.startAll() == 0, "Failed starting node");
  return NDBT_OK;
}

int
runCheckAllNodesStarted(NDBT_Context* ctx, NDBT_Step* step){
  NdbRestarter restarter;

  CHK2(restarter.waitClusterStarted() == 0,
       "All nodes were not started");
  return NDBT_OK;
}

static
int
terrorCodes[] =
{
  8106,
  8103,
  8104,
  8102,
  0
};

static
int
runTransError(NDBT_Context* ctx, NDBT_Step* step)
{
  NdbRestarter res;
  ctx->setProperty("LongSignalMemorySnapshot", Uint32(1));
  int mode = ctx->getProperty("TransMode", Uint32(0));

  for (int i = 0; terrorCodes[i] != 0; i++)
  {
#ifdef NDB_USE_GET_ENV
    char errbuf[256];
    if (NdbEnv_GetEnv("NDB_ERR_CODE", errbuf, sizeof(errbuf)) != 0 &&
        atoi(errbuf) != terrorCodes[i])
    {
      continue;
    }
#endif
    printf("testing errcode: %d\n", terrorCodes[i]);
    runTransSnapshot(ctx, step);
    runLongSignalMemorySnapshotStart(ctx, step);
    runRSSsnapshot(ctx, step);

    res.insertErrorInAllNodes(terrorCodes[i]);
    switch(mode) {
    case 0:
      runMixedDML(ctx, step);
      break;
    case 1:
      runMixedCascade(ctx, step);
      break;
    }
    /**
     * If we are not using Read Backup we can arrive here while the
     * commit is in progress, give the commit a chance to complete
     * before checking the memory allocation snapshots.
     */
    runRSSsnapshotCheck(ctx, step);
    runTransSnapshotCheck(ctx, step);
    runLongSignalMemorySnapshotCheck(ctx, step);
  }

  res.insertErrorInAllNodes(0);

  return NDBT_OK;
}

static
int
runAbortWithSlowChildScans(NDBT_Context* ctx, NDBT_Step* step)
{
  /**
   * FK parent update/delete causes child tables to be
   * scanned.
   * This scanning is not considered when the transaction
   * is being aborted, so a transaction causing child-table
   * scans can finish aborting before the child table scans
   * are complete.
   * This testcase gives some coverage to that scenario,
   * by initiating some parent deletes, resulting in
   * child table scans, then causing the scans to stall,
   * the transaction to abort, and then the scans to resume.
   */
  const int rows = ctx->getNumRecords();
  const int batchSize = ctx->getProperty("BatchSize", DEFAULT_BATCH_SIZE);

  Ndb* pNdb = GETNDB(step);
  const NdbDictionary::Table* pTab = ctx->getTab();
 
  { 
    HugoTransactions ht(*pTab);
    CHK2(ht.loadTable(pNdb, rows, batchSize) == 0,
         "Load table failed");
  }

  /* Originally used a separate row lock to cause stall,
   * but no need, as the blocking of the scans itself
   * causes a transaction timeout eventually
   */
//   int lockedRow = rand() % rows;
//   g_err << "Locking row " << lockedRow << endl;
  
//   HugoOperations ho(*pTab);
//   if ((ho.startTransaction(pNdb) != 0) ||
//       (ho.pkReadRecord(pNdb, 
//                        lockedRow, 
//                        1, 
//                        NdbOperation::LM_Exclusive) != 0) ||
//       (ho.execute_NoCommit(pNdb) != 0))
//   {
//     g_err << "Problem locking row : "
//           << ho.getNdbError()
//           << endl;
//     return NDBT_FAILED;
//   }
  
  /* Cause child table FK scans to block... */
  NdbRestarter restarter;
  /* Block FK-related child table scans... */
  restarter.insertErrorInAllNodes(8109);

  /* Now perform delete of parent rows in a separate connection
   * Separate connection used as some validation is performed by
   * TC at connection close time (TCRELEASEREQ)
   */
  
  {
    Ndb myNdb(&pNdb->get_ndb_cluster_connection());
  
    myNdb.init();

    myNdb.setDatabaseName(pNdb->getDatabaseName());
    
    HugoTransactions ht(*pTab);
  
    /* Avoid lots of retries for the deletes... */
    ht.setRetryMax(1);

    /* Attempt to delete everything, will fail
     * as triggered child table scans timeout
     */
    CHK2(ht.pkDelRecords(&myNdb, rows) != 0, //Expect error
	 "Unexpected success of ht!");
    
    /* Now close Ndb object, causing some TCRELEASEREQ validation */
  }

  /* Unblock child scans */
  restarter.insertErrorInAllNodes(0);
  
  return NDBT_OK;
}



NDBT_TESTSUITE(testFK);
TESTCASE("CreateDrop",
	 "Test random create/drop of FK")
{
  TC_PROPERTY("IDX_RAND", 5);
  TC_PROPERTY("FK_RAND", 10);
  INITIALIZER(runTransSnapshot);
  INITIALIZER(runRSSsnapshot);
  INITIALIZER(runDiscoverTable);
  INITIALIZER(runCreateDropRandom);
  INITIALIZER(runCleanupTable);
  INITIALIZER(runRSSsnapshotCheck);
  INITIALIZER(runTransSnapshotCheck);
}
TESTCASE("CreateDropWithData",
	 "Test random create/drop of FK with transactions in parallel")
{
  TC_PROPERTY("CreateAndLoad", 1);
  INITIALIZER(runTransSnapshot);
  INITIALIZER(runRSSsnapshot);
  INITIALIZER(runDiscoverTable);
  INITIALIZER(runCreateDropRandom);
  INITIALIZER(runCleanupTable);
  INITIALIZER(runRSSsnapshotCheck);
  INITIALIZER(runTransSnapshotCheck);
}
TESTCASE("CreateDropDuring",
	 "Test random create/drop of FK with transactions in parallel")
{
  TC_PROPERTY("TransactionsUntilStopped", 1);
  INITIALIZER(runDiscoverTable);
  STEP(runCreateDropRandom);
  STEPS(runTransactions, 1);
}
TESTCASE("CreateDropError",
	 "Test create/drop of FK with error inserts")
{
  INITIALIZER(runDiscoverTable);
  INITIALIZER(runCreateDropError);
}
TESTCASE("Basic1",
	 "Create random FK and run a single transaction")
{
  INITIALIZER(runTransSnapshot);
  INITIALIZER(runRSSsnapshot);
  INITIALIZER(runDiscoverTable);
  INITIALIZER(runCreateRandom);
  STEPS(runTransactions, 1);
  VERIFIER(runCleanupTable);
  VERIFIER(runRSSsnapshotCheck);
  VERIFIER(runTransSnapshotCheck);
}
TESTCASE("Basic5",
	 "Create random FK and run 5 transactions")
{
  TC_PROPERTY("concurrent", 1);
  INITIALIZER(runTransSnapshot);
  INITIALIZER(runRSSsnapshot);
  INITIALIZER(runDiscoverTable);
  INITIALIZER(runCreateRandom);
  STEPS(runTransactions, 5);
  VERIFIER(runCleanupTable);
  VERIFIER(runRSSsnapshotCheck);
  VERIFIER(runTransSnapshotCheck);
}
TESTCASE("Basic55",
	 "Create random FK and run a mix of transactions")
{
  TC_PROPERTY("concurrent", 1);
  INITIALIZER(runTransSnapshot);
  INITIALIZER(runRSSsnapshot);
  INITIALIZER(runDiscoverTable);
  INITIALIZER(runCreateRandom);
  STEPS(runTransactions, 5);
  STEPS(runMixedDML, 10);
  VERIFIER(runCleanupTable);
  VERIFIER(runRSSsnapshotCheck);
  VERIFIER(runTransSnapshotCheck);
}
TESTCASE("TransError",
	 "")
{
  INITIALIZER(runDiscoverTable);
  INITIALIZER(runCreateRandom);
  INITIALIZER(runTransError);
  INITIALIZER(runCleanupTable);
}
TESTCASE("Cascade1",
	 "")
{
  INITIALIZER(runDiscoverTable);
  INITIALIZER(runCreateCascadeChild);
  STEPS(runMixedCascade, 1);
  VERIFIER(runCleanupTable);
  VERIFIER(runDropCascadeChild);
}
TESTCASE("Cascade10",
	 "")
{
  INITIALIZER(runDiscoverTable);
  INITIALIZER(runCreateCascadeChild);
  STEPS(runMixedCascade, 10);
  VERIFIER(runCleanupTable);
  VERIFIER(runDropCascadeChild);
}
TESTCASE("CascadeError",
	 "")
{
  TC_PROPERTY("TransMode", Uint32(1));
  INITIALIZER(runDiscoverTable);
  INITIALIZER(runCreateCascadeChild);
  INITIALIZER(runTransError);
  VERIFIER(runCleanupTable);
  VERIFIER(runDropCascadeChild);
}
TESTCASE("DropTableWithFKDuringRestart",
         "1. Create a child table identical to the current table"
         "2. Create FK mapping the similar column from both tables"
         "3. Choose a random node and restart it with nostart"
         "4. Drop the child table"
         "5. Start the node at no start")
{
  INITIALIZER(runDiscoverTable);
  INITIALIZER(runCreateCascadeChild);
  INITIALIZER(runRestartOneNodeNoStart);
  INITIALIZER(runDropCascadeChild);
  STEP(runStartAllNodes);
  VERIFIER(runCheckAllNodesStarted);
}
TESTCASE("AbortWithSlowChildScans",
         "Some coverage of transaction abort with "
         "outstanding FK child table scans")
{
  TC_PROPERTY("IDX_UNIQ", Uint32(0));
  TC_PROPERTY("IDX_MANY", Uint32(1));
  TC_PROPERTY("IDX_RAND", Uint32(0));
  TC_PROPERTY("FK_UNIQ", Uint32(0));
  TC_PROPERTY("FK_MANY", Uint32(1));
  TC_PROPERTY("FK_RAND", Uint32(0));
  INITIALIZER(runDiscoverTable);
  INITIALIZER(runCreateRandom);
  INITIALIZER(runAbortWithSlowChildScans);
  FINALIZER(runCleanupTable);
}
NDBT_TESTSUITE_END(testFK)

int
main(int argc, const char** argv)
{
  ndb_init();
  NDBT_TESTSUITE_INSTANCE(testFK);
  return testFK.execute(argc, argv);
}
