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

#ifndef HUGO_QUERIES_HPP
#define HUGO_QUERIES_HPP


#include <NDBT.hpp>
#include <HugoCalculator.hpp>
#include <HugoOperations.hpp>
#include "../../src/ndbapi/NdbQueryBuilder.hpp"
#include "../../src/ndbapi/NdbQueryOperation.hpp"

class HugoQueries
{
  struct Op
  {
    const NdbQueryOperationDef* m_query_op;
    Vector<NDBT_ResultRow*> m_rows;
    HugoCalculator * m_calc;
  };

public:
  HugoQueries(const NdbQueryDef & query);
  virtual ~HugoQueries();

  // Rows for for each of the operations
  Vector<Uint32> m_rows_found;

  int runLookupQuery(Ndb*,
                     int queries = 100,
                     int batchsize = 1);
  int runScanQuery(Ndb*,
                   int abort = 4,
                   int parallelism = 0,
                   int scan_flags = 0);

  static int equalForParameters(char * buf,
                                Op&,
                                NdbQueryParamValue params[],
                                int rowNo);
  static int getValueForQueryOp(NdbQueryOperation* pOp, NDBT_ResultRow* pRow);


  void allocRows(int batch);
protected:

  const NdbQueryDef* m_query_def;
  Vector<Op> m_ops;
  int m_retryMax;
};

#endif

