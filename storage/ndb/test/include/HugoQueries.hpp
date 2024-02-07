/*
  Copyright (c) 2003, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef HUGO_QUERIES_HPP
#define HUGO_QUERIES_HPP

#include <HugoCalculator.hpp>
#include <HugoOperations.hpp>
#include <NDBT.hpp>
#include "../../src/ndbapi/NdbQueryBuilder.hpp"
#include "../../src/ndbapi/NdbQueryOperation.hpp"

class HugoQueries {
  struct Op {
    const NdbQueryOperationDef *m_query_op;
    Vector<NDBT_ResultRow *> m_rows;
    HugoCalculator *m_calc;
  };

 public:
  HugoQueries(const NdbQueryDef &query, int retryMax = 100);
  virtual ~HugoQueries();

  // Rows for for each of the operations
  Vector<Uint32> m_rows_found;

  int runLookupQuery(Ndb *, int queries = 100, int batchsize = 1);
  int runScanQuery(Ndb *, int abort = 4, int parallelism = 0,
                   int scan_flags = 0);

  const NdbError &getNdbError() const;

 protected:
  static int equalForParameters(char *buf, Op &, NdbQueryParamValue params[],
                                int rowNo);
  static int getValueForQueryOp(NdbQueryOperation *pOp, NDBT_ResultRow *pRow);

  void allocRows(int batch);

  void clearNdbError();
  void setNdbError(const NdbError &error);

  const NdbQueryDef *m_query_def;
  Vector<Op> m_ops;
  int m_retryMax;
  NdbError m_error;
};

#endif
