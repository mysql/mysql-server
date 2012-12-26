/*
  Copyright (C) 2003 MySQL AB
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

#include "HugoQueries.hpp"
#include <NDBT_Stats.hpp>
#include <NdbSleep.h>
#include <NdbTick.h>
#include "../../src/ndbapi/NdbQueryOperation.hpp"

HugoQueries::HugoQueries(const NdbQueryDef & query)
{
  m_retryMax = 100;
  m_query_def = &query;

  for (Uint32 i = 0; i<query.getNoOfOperations(); i++)
  {
    struct Op op;
    op.m_query_op = query.getQueryOperation(i);
    op.m_calc = 0;
    if (op.m_query_op->getTable())
    {
      op.m_calc = new HugoCalculator(* op.m_query_op->getTable());
    }
    m_ops.push_back(op);
  }
}

HugoQueries::~HugoQueries()
{
  for (unsigned o = 0; o<m_ops.size(); o++)
  {
    while (m_ops[o].m_rows.size())
    {
      delete m_ops[o].m_rows.back();
      m_ops[o].m_rows.erase(m_ops[o].m_rows.size() - 1);
    }
    if (m_ops[o].m_calc)
      delete m_ops[o].m_calc;
  }
}

void
HugoQueries::allocRows(int batch)
{
  for (unsigned o = 0; o<m_ops.size(); o++)
  {
    const NdbQueryOperationDef * pOp =m_query_def->getQueryOperation((Uint32)o);
    const NdbDictionary::Table* tab = pOp->getTable();

    if (tab)
    {
      while (m_ops[o].m_rows.size() < (unsigned)batch)
      {
        m_ops[o].m_rows.push_back(new NDBT_ResultRow(* tab));
      }
    }
  }
}

int
HugoQueries::equalForParameters(char * buf,
                                Op & op,
                                NdbQueryParamValue params[],
                                int rowNo)
{
  Uint32 no = 0;
  HugoCalculator & calc = * op.m_calc;
  const NdbDictionary::Table & tab = calc.getTable();
  if (op.m_query_op->getType() == NdbQueryOperationDef::TableScan)
  {

  }
  else if (op.m_query_op->getType() == NdbQueryOperationDef::PrimaryKeyAccess)
  {
    for (int i = 0; i<tab.getNoOfColumns(); i++)
    {
      const NdbDictionary::Column* attr = tab.getColumn(i);
      if (attr->getPrimaryKey())
      {
        Uint32 len = attr->getSizeInBytes();
        Uint32 real_len;
        bzero(buf, len);
        calc.calcValue((Uint32)rowNo, i, 0, buf, len, &real_len);
        params[no++]= NdbQueryParamValue((void*)buf);
        buf += len;
      }
    }
  }
  else if (op.m_query_op->getType() == NdbQueryOperationDef::UniqueIndexAccess||
           op.m_query_op->getType() == NdbQueryOperationDef::OrderedIndexScan)
  {
    const NdbDictionary::Index* idx = op.m_query_op->getIndex();
    for (unsigned i = 0; i < idx->getNoOfColumns(); i++)
    {
      const NdbDictionary::Column* attr = 
        tab.getColumn(idx->getColumn(i)->getName());
      Uint32 len = attr->getSizeInBytes();
      Uint32 real_len;
      bzero(buf, len);
      calc.calcValue((Uint32)rowNo, attr->getColumnNo(), 
                     0, buf, len, &real_len);
      params[no++]= NdbQueryParamValue((void*)buf);
      buf += len;
    }
  }
  return 0;
}

int
HugoQueries::getValueForQueryOp(NdbQueryOperation* pOp, NDBT_ResultRow * pRow)
{
  const NdbDictionary::Table & tab = pRow->getTable();
  for(int a = 0; a<tab.getNoOfColumns(); a++)
  {
    pRow->attributeStore(a) = pOp->getValue(tab.getColumn(a)->getName());
  }

  return 0;
}

int
HugoQueries::runLookupQuery(Ndb* pNdb,
                            int queries,
                            int batch)
{
  int q = 0;
  int retryAttempt = 0;

  m_rows_found.clear();
  Uint32 zero = 0;
  m_rows_found.fill(m_query_def->getNoOfOperations() - 1, zero);

  if (batch == 0) {
    g_info << "ERROR: Argument batch == 0 in runLookupQuery. Not allowed."
           << endl;
    return NDBT_FAILED;
  }

  allocRows(batch);

  while (q < queries)
  {
    if (q + batch > queries)
      batch = queries - q;

    if (retryAttempt >= m_retryMax)
    {
      g_info << "ERROR: has retried this operation " << retryAttempt
             << " times, failing!" << endl;
      return NDBT_FAILED;
    }

    Vector<Uint32> batch_rows_found;
    batch_rows_found.fill(m_query_def->getNoOfOperations() - 1, zero);
    Vector<NdbQuery*> queries;

    NdbTransaction * pTrans = pNdb->startTransaction();
    if (pTrans == NULL)
    {
      const NdbError err = pNdb->getNdbError();

      if (err.status == NdbError::TemporaryError){
        ERR(err);
        NdbSleep_MilliSleep(50);
        retryAttempt++;
        continue;
      }
      ERR(err);
      return NDBT_FAILED;
    }

    for (int b = 0; b<batch; b++)
    {
      char buf[NDB_MAX_TUPLE_SIZE];
      NdbQueryParamValue params[NDB_MAX_NO_OF_ATTRIBUTES_IN_KEY];
      equalForParameters(buf, m_ops[0], params, b + q);

      NdbQuery * query = pTrans->createQuery(m_query_def, params);
      if (query == 0)
      {
        const NdbError err = pTrans->getNdbError();
        ERR(err);
        return NDBT_FAILED;
      }

      for (unsigned o = 0; o<m_ops.size(); o++)
      {
        NdbQueryOperation * pOp = query->getQueryOperation((Uint32)o);
        HugoQueries::getValueForQueryOp(pOp, m_ops[o].m_rows[b]);
      }
      queries.push_back(query);
    }

    int check = pTrans->execute(NoCommit, AbortOnError);
    if (check == -1)
    {
      const NdbError err = pTrans->getNdbError();
      ERR(err);
      if (err.status == NdbError::TemporaryError){
        pTrans->close();
        NdbSleep_MilliSleep(50);
        retryAttempt++;
        continue;
      }
      pTrans->close();
      return NDBT_FAILED;
    }
#if 0
    // Disabled, as this is incorrectly handled in SPJ API, will fix soon
    else
    {
      /**
       * If ::execute() didn't fail, there should not be an error on
       * its NdbError object either:
       */
      const NdbError err = pTrans->getNdbError();
      if (err.code)
      {
        ERR(err);
        ndbout_c("API INCONSISTENCY: NdbTransaction returned NdbError even if ::execute() succeeded");
        pTrans->close();
        return NDBT_FAILED;
      }
    }
#endif

    bool retry = false;
    for (int b = 0; b<batch; b++)
    {
      NdbQuery * query = queries[b];

      /**
       * As NdbQuery is always 'dirty read' (impl. limitations), 'AbortOnError'
       * is ignored and handled as 'IgnoreError'. We will therefore not get
       * errors returned from ::execute() or set into 'pTrans->getNdbError()':
       * Has to check for errors on the NdbQuery object instead:
       */
      const NdbError& err = query->getNdbError();
      if (err.code)
      {
        ERR(err);
        if (err.status == NdbError::TemporaryError){
          pTrans->close();
          retry = true;
          break;
        }
        pTrans->close();
        return NDBT_FAILED;
      }

      const NdbQuery::NextResultOutcome stat = query->nextResult();
      if (stat == NdbQuery::NextResult_gotRow)
      {
        for (unsigned o = 0; o<m_ops.size(); o++)
        {
          NdbQueryOperation * pOp = query->getQueryOperation((Uint32)o);
          if (!pOp->isRowNULL())
          {
            batch_rows_found[o]++;
            if (m_ops[o].m_calc->verifyRowValues(m_ops[o].m_rows[b]) != 0)
            {
              pTrans->close();
              return NDBT_FAILED;
            }
          }
        }
      }
      else if (stat == NdbQuery::NextResult_error)
      {
        const NdbError& err = query->getNdbError();
        ERR(err);
        if (err.status == NdbError::TemporaryError){
          pTrans->close();
          retry = true;
          break;
        }
        pTrans->close();
        return NDBT_FAILED;
      }
    }
    if (retry)
    {
      NdbSleep_MilliSleep(50);
      retryAttempt++;
      continue;
    }

    pTrans->close();
    q += batch;

    for (unsigned i = 0; i<batch_rows_found.size(); i++)
      m_rows_found[i] += batch_rows_found[i];
  }

  return NDBT_OK;
}

int
HugoQueries::runScanQuery(Ndb * pNdb,
                          int abort,
                          int parallelism,
                          int scan_flags)
{
  int retryAttempt = 0;

  allocRows(1);

  while (retryAttempt < m_retryMax)
  {
    m_rows_found.clear();
    Uint32 zero = 0;
    m_rows_found.fill(m_query_def->getNoOfOperations() - 1, zero);

    NdbTransaction * pTrans = pNdb->startTransaction();
    if (pTrans == NULL)
    {
      const NdbError err = pNdb->getNdbError();
      ERR(err);
      if (err.status == NdbError::TemporaryError){
        NdbSleep_MilliSleep(50);
        retryAttempt++;
        continue;
      }
      return NDBT_FAILED;
    }

    NdbQuery * query = 0;

    char buf[NDB_MAX_TUPLE_SIZE];
    NdbQueryParamValue params[NDB_MAX_NO_OF_ATTRIBUTES_IN_KEY];
    equalForParameters(buf, m_ops[0], params, /* rowNo */ 0);
    query = pTrans->createQuery(m_query_def, params);
    if (query == 0)
    {
      const NdbError err = pTrans->getNdbError();
      ERR(err);
      return NDBT_FAILED;
    }

    for (unsigned o = 0; o<m_ops.size(); o++)
    {
      NdbQueryOperation * pOp = query->getQueryOperation((Uint32)o);
      HugoQueries::getValueForQueryOp(pOp, m_ops[o].m_rows[0]);
    }

    int check = pTrans->execute(NoCommit, AbortOnError);
    if (check == -1)
    {
      const NdbError err = pTrans->getNdbError();
      ERR(err);
      if (err.status == NdbError::TemporaryError){
        pTrans->close();
        NdbSleep_MilliSleep(50);
        retryAttempt++;
        continue;
      }
      pTrans->close();
      return NDBT_FAILED;
    }
    else
    {
      // Disabled, as this is incorrectly handled in SPJ API, will fix soon
#if 0
      /**
       * If ::execute() didn't fail, there should not be an error on
       * its NdbError object either:
       */
      const NdbError err = pTrans->getNdbError();
      if (err.code)
      {
        ERR(err);
        ndbout_c("API INCONSISTENCY: NdbTransaction returned NdbError even if ::execute() succeeded");
        pTrans->close();
        return NDBT_FAILED;
      }
#endif

      /**
       * As NdbQuery is always 'dirty read' (impl. limitations), 'AbortOnError'
       * is ignored and handled as 'IgnoreError'. We will therefore not get
       * errors returned from ::execute() or set into 'pTrans->getNdbError()':
       * Has to check for errors on the NdbQuery object instead:
       */
      NdbError err = query->getNdbError();
      if (err.code)
      {
        ERR(err);
        if (err.status == NdbError::TemporaryError){
          pTrans->close();
          NdbSleep_MilliSleep(50);
          retryAttempt++;
          continue;
        }
        pTrans->close();
        return NDBT_FAILED;
      }
    }

    int r = rand() % 100;
    if (r < abort && ((r & 1) == 0))
    {
      ndbout_c("Query aborted!");
      query->close();
      pTrans->close();
      m_rows_found.clear();
      return NDBT_OK;
    }

    NdbQuery::NextResultOutcome res;
    while ((res = query->nextResult()) == NdbQuery::NextResult_gotRow)
    {
      if (r < abort && ((r & 1) == 1))
      {
        ndbout_c("Query aborted 2!");
        query->close();
        pTrans->close();
        m_rows_found.clear();
      return NDBT_OK;
      }

      for (unsigned o = 0; o<m_ops.size(); o++)
      {
        NdbQueryOperation * pOp = query->getQueryOperation((Uint32)o);
        if (!pOp->isRowNULL())
        {
          m_rows_found[o]++;
          if (m_ops[o].m_calc->verifyRowValues(m_ops[o].m_rows[0]) != 0)
          {
            pTrans->close();
            return NDBT_FAILED;
          }
        }
      }
    }

    const NdbError err = query->getNdbError();
    query->close();
    pTrans->close();
    if (res == NdbQuery::NextResult_error)
    {
      ERR(err);
      if (err.status == NdbError::TemporaryError)
      {
        NdbSleep_MilliSleep(50);
        retryAttempt++;
        continue;
      }
      return NDBT_FAILED;
    }
    else if (res != NdbQuery::NextResult_scanComplete)
    {
      ndbout_c("Got %u from nextResult()", res);
      return NDBT_FAILED;
    }
    break;
  }

  return NDBT_OK;
}

template class Vector<HugoQueries::Op>;
template class Vector<NdbQuery*>;
