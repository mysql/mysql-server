/*
   Copyright (c) 2011, 2023, Oracle and/or its affiliates.

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
#include <HugoQueryBuilder.hpp>

static
bool
isScan(const NdbQueryOperationDef* def)
{
  return
    def->getType() == NdbQueryOperationDef::TableScan ||
    def->getType() == NdbQueryOperationDef::OrderedIndexScan;
}

NdbOut&
operator<<(NdbOut& out, const HugoQueryBuilder::Op& op)
{
  out << "[" << op.m_idx << " : " << op.m_op->getTable()->getName() << ": ";
  switch(op.m_op->getType()){
  case NdbQueryOperationDef::TableScan:
    out << "table-scan";
    break;
  case NdbQueryOperationDef::OrderedIndexScan:
    out << "index-scan";
    break;
  default:
    out << "lookup";
  }

  out << " : parent: " << op.m_parent << " ]";
  return out;
}

void
HugoQueryBuilder::init()
{
  m_options = 0;
  setMinJoinLevel(2);
  setMaxJoinLevel(4);
}

HugoQueryBuilder::~HugoQueryBuilder()
{
  for (unsigned i = 0; i<m_queries.size(); i++)
    m_queries[i]->destroy();
}

void
HugoQueryBuilder::fixOptions()
{
  setOption(O_PK_INDEX);
  setOption(O_UNIQUE_INDEX);
  setOption(O_TABLE_SCAN);
  setOption(O_ORDERED_INDEX);
  setOption(O_GRANDPARENT);
  if (testOption(O_LOOKUP))
  {
    clearOption(O_TABLE_SCAN);
    clearOption(O_ORDERED_INDEX);
  }
}

void
HugoQueryBuilder::addTable(const NdbDictionary::Table* tab)
{
  for (unsigned i = 0; i<m_tables.size(); i++)
  {
    if (m_tables[i].m_table == tab)
      return;
  }

  TableDef def;
  def.m_table = tab;

  NdbDictionary::Dictionary* pDict = m_ndb->getDictionary();
  NdbDictionary::Dictionary::List l;
  int res = pDict->listIndexes(l, tab->getName());
  if (res == 0)
  {
    for (unsigned i = 0; i<l.count; i++)
    {
      const NdbDictionary::Index * idx = pDict->getIndex(l.elements[i].name,
                                                         tab->getName());
      if (idx)
      {
        if (idx->getType() == NdbDictionary::Index::UniqueHashIndex)
        {
          def.m_unique_indexes.push_back(idx);
        }
        else if (idx->getType() == NdbDictionary::Index::OrderedIndex)
        {
          def.m_ordered_indexes.push_back(idx);
        }
      }
    }
  }

  m_tables.push_back(def);
}

int
HugoQueryBuilder::getJoinLevel() const
{
  int m0 = m_joinLevel[0]; // min
  int m1 = m_joinLevel[1]; // max
  if (m0 > m1)
  {
    int m = m0;
    m0 = m1;
    m1 = m;
  }

  int d = m1 - m0;
  if (d == 0)
    return m0;
  else
    return m0 + (rand() % d);
}

void
HugoQueryBuilder::removeTable(const NdbDictionary::Table* tab)
{
  for (unsigned i = 0; i<m_tables.size(); i++)
  {
    if (m_tables[i].m_table == tab)
    {
      m_tables.erase(i);
      return;
    }
  }
}

HugoQueryBuilder::TableDef
HugoQueryBuilder::getTable() const
{
  int i = rand() % m_tables.size();
  return m_tables[i];
}

HugoQueryBuilder::OpIdx
HugoQueryBuilder::getOp() const
{
  OpIdx oi;
  TableDef tab = getTable();
  oi.m_index = 0;
  oi.m_table = tab.m_table;

  OptionMask save = m_options;
  if (tab.m_unique_indexes.size() == 0)
  {
    clearOption(O_UNIQUE_INDEX);
  }
  if (tab.m_ordered_indexes.size() == 0)
  {
    clearOption(O_ORDERED_INDEX);
  }

loop:
  switch(rand() % 4){
  case 0:
    if (testOption(O_PK_INDEX))
    {
      oi.m_type = NdbQueryOperationDef::PrimaryKeyAccess;
      goto found;
    }
    break;
  case 1:
    if (testOption(O_TABLE_SCAN))
    {
      oi.m_type = NdbQueryOperationDef::TableScan;
      goto found;
    }
    break;
  case 2:
    if (testOption(O_UNIQUE_INDEX))
    {
      oi.m_type = NdbQueryOperationDef::UniqueIndexAccess;
      int cnt = tab.m_unique_indexes.size();
      oi.m_index = tab.m_unique_indexes[rand() % cnt];
      goto found;
    }
    break;
  case 3:
    if (testOption(O_ORDERED_INDEX))
    {
      oi.m_type = NdbQueryOperationDef::OrderedIndexScan;
      int cnt = tab.m_ordered_indexes.size();
      oi.m_index = tab.m_ordered_indexes[rand() % cnt];
      goto found;
    }
    break;
  }
  goto loop;

found:
  m_options = save;
  return oi;
}

bool
HugoQueryBuilder::checkBindable(Vector<const NdbDictionary::Column*> cols,
                                Vector<Op> ops,
                                bool allow_bind_nullable)
{
  for (unsigned c = 0; c < cols.size(); c++)
  {
    const NdbDictionary::Column * col = cols[c];
    bool found = false;
    for (unsigned t = 0; !found && t<ops.size(); t++)
    {
      const NdbDictionary::Table * tab = ops[t].m_op->getTable();
      if (tab)
      {
        for (int i = 0; i<tab->getNoOfColumns(); i++)
        {
          if (!allow_bind_nullable && tab->getColumn(i)->getNullable())
            continue;
          else if (col->isBindable(* tab->getColumn(i)) == 0)
          {
            found = true;
            break;
          }
        }
      }
    }
    if (!found)
      return false;
  }
  return true;
}

bool
HugoQueryBuilder::isAncestor(const Op& parent, const Op& child) const
{
  int pi = parent.m_idx;
  int ci = child.m_idx;
  require(ci != pi);

  while (ci != 0)
  {
    if (m_query[ci].m_parent == pi)
      return true;
    require(m_query[ci].m_parent != -1);
    ci = m_query[m_query[ci].m_parent].m_idx;
  }
  return false;
}

bool
HugoQueryBuilder::checkBusyScan(Op op) const
{
  /**
   * Iterate upwards until we find first scan...
   */
  while (op.m_parent != -1)
  {
    if (isScan(op.m_op))
    {
      break;
    }
    op = m_query[op.m_parent];
  }

  for (unsigned i = op.m_idx + 1; i < m_query.size(); i++)
    if (isAncestor(op, m_query[i]) && isScan(m_query[i].m_op))
      return true;

  return false;
}

Vector<HugoQueryBuilder::Op>
HugoQueryBuilder::getParents(OpIdx oi)
{
  /**
   * We need to be able to bind all columns in table/index
   */
  bool allow_bind_nullable = false;
  bool check_bushy_scan = false;
  Vector<const NdbDictionary::Column*> cols;
  if (oi.m_index == 0)
  {
    for (int i = 0; i<oi.m_table->getNoOfColumns(); i++)
      if (oi.m_table->getColumn(i)->getPrimaryKey())
        cols.push_back(oi.m_table->getColumn(i));
  }
  else if (oi.m_index->getType() == NdbDictionary::Index::UniqueHashIndex)
  {
    for (unsigned i = 0; i < oi.m_index->getNoOfColumns(); i++)
      cols.push_back(oi.m_table->getColumn
                     (oi.m_index->getColumn(i)->getName()));
  }
  else if (oi.m_index->getType() == NdbDictionary::Index::OrderedIndex)
  {
    /**
     * Binding a prefix is ok...but skip this for now...
     */
    allow_bind_nullable = true;
    check_bushy_scan = true;
    unsigned cnt = oi.m_index->getNoOfColumns();
    unsigned val = cnt; // cnt > 1 ? (1 + (rand() % (cnt - 1))) : cnt;
    for (unsigned i = 0; i < val; i++)
      cols.push_back(oi.m_table->getColumn
                     (oi.m_index->getColumn(i)->getName()));
  }

  int r = rand() % m_query.size();
  int cnt = (int)m_query.size();
  for (int i = 0; i < cnt; i++)
  {
    Vector<Op> set;
    Op op = m_query[(i + r) % cnt];
    if (check_bushy_scan && checkBusyScan(op))
      continue;
    set.push_back(op);

    /**
     * Also add grandparents
     */
    if (testOption(O_GRANDPARENT))
    {
      while (op.m_parent != -1)
      {
        op = m_query[op.m_parent];
        set.push_back(op);
      }
    }

    if (checkBindable(cols, set, allow_bind_nullable))
      return set;
  }

  Vector<Op> ret;
  return ret;
}

NdbQueryOperand *
HugoQueryBuilder::createLink(NdbQueryBuilder& builder,
                             const NdbDictionary::Column* pCol,
                             Vector<Op> & parents,
                             bool allow_bind_nullable)
{
  int cnt = (int)parents.size();
  int r = rand();

  Op op;
  const NdbDictionary::Column* col = 0;

  /**
   * Start linking with primary key...(for now)
   */
  for (int i = 0; i<cnt; i++)
  {
    op = parents[(i + r) % cnt];
    const NdbDictionary::Table* tab = op.m_op->getTable();

    int cntpk = tab->getNoOfPrimaryKeys();
    int rpk = rand();
    for (int j = 0; j<cntpk; j++)
    {
      col = tab->getColumn(tab->getPrimaryKey((j + rpk) % cntpk));
      if (pCol->isBindable(* col) == 0)
      {
        goto found;
      }
    }
  }

  /**
   * Check other columns
   */
  r = rand();
  for (int i = 0; i<cnt; i++)
  {
    op = parents[(i + r) % cnt];
    const NdbDictionary::Table* tab = op.m_op->getTable();

    int cntcol = tab->getNoOfColumns();
    int rcol = rand();
    for (int j = 0; j<cntcol; j++)
    {
      col = tab->getColumn((j + rcol) % cntcol);
      if (col->getPrimaryKey())
      {
        // already checked
        continue;
      }
      if (!allow_bind_nullable && col->getNullable())
        continue;

      if (pCol->isBindable(* col) == 0)
      {
        goto found;
      }
    }
  }
  return 0;

found:
  NdbQueryOperand * ret = builder.linkedValue(op.m_op, col->getName());
  require(ret);
  return ret;
}

const NdbQueryOperationDef*
HugoQueryBuilder::createOp(NdbQueryBuilder& builder)
{
  struct Op op;
  op.m_parent = -1;
  op.m_op = 0;
  op.m_idx = m_query.size();

  if (m_query.size() == 0)
  {
    /**
     * This is root operation...no linked-values
     */
    OpIdx oi = getOp();
    switch(oi.m_type){
    case NdbQueryOperationDef::PrimaryKeyAccess:{
      int opNo = 0;
      NdbQueryOperand * operands[NDB_MAX_NO_OF_ATTRIBUTES_IN_KEY + 1];
      for (int a = 0; a<oi.m_table->getNoOfColumns(); a++)
      {
        if (oi.m_table->getColumn(a)->getPrimaryKey())
        {
          operands[opNo++] = builder.paramValue();
        }
      }
      operands[opNo] = 0;
      op.m_op = builder.readTuple(oi.m_table, operands);
      break;
    }
    case NdbQueryOperationDef::TableScan:
      op.m_op = builder.scanTable(oi.m_table);
      break;
    case NdbQueryOperationDef::OrderedIndexScan:{
      int opNo = 0;
      NdbQueryOperand * operands[NDB_MAX_NO_OF_ATTRIBUTES_IN_KEY + 1];
      for (unsigned a = 0; a<oi.m_index->getNoOfColumns(); a++)
      {
        operands[opNo++] = builder.paramValue();
      }
      operands[opNo] = 0;
      NdbQueryIndexBound bounds(operands);
      op.m_op = builder.scanIndex(oi.m_index, oi.m_table, &bounds);
      break;
    }
    case NdbQueryOperationDef::UniqueIndexAccess:
      int opNo = 0;
      NdbQueryOperand * operands[NDB_MAX_NO_OF_ATTRIBUTES_IN_KEY + 1];
      for (unsigned a = 0; a<oi.m_index->getNoOfColumns(); a++)
      {
        operands[opNo++] = builder.paramValue();
      }
      operands[opNo] = 0;
      op.m_op = builder.readTuple(oi.m_index, oi.m_table, operands);
      break;
    }
  }
  else
  {
loop:
    OpIdx oi = getOp();
    Vector<Op> parents = getParents(oi);
    NdbQueryOptions options;
    if (parents.size() == 0)
    {
      // no possible parents found for pTab...try another
      goto loop;
    }
    if (parents.size() > 1)
    {
      // We have grandparents, 'parents[0]' is real parent
      options.setParent(parents[0].m_op);
    }
    if ((rand() % 2) == 0)
    {
      // Set INNER-join options (no NULL extended rows returned)
      options.setMatchType(NdbQueryOptions::MatchNonNull);
    }
    switch(oi.m_type){
    case NdbQueryOperationDef::PrimaryKeyAccess:{
      int opNo = 0;
      NdbQueryOperand * operands[NDB_MAX_NO_OF_ATTRIBUTES_IN_KEY + 1];
      for (int a = 0; a<oi.m_table->getNoOfColumns(); a++)
      {
        if (oi.m_table->getColumn(a)->getPrimaryKey())
        {
          operands[opNo++] = createLink(builder, oi.m_table->getColumn(a),
                                        parents, false);
        }
      }
      operands[opNo] = 0;

      op.m_parent = parents[0].m_idx;
      op.m_op = builder.readTuple(oi.m_table, operands, &options);
      break;
    }
    case NdbQueryOperationDef::UniqueIndexAccess: {
      int opNo = 0;
      NdbQueryOperand * operands[NDB_MAX_NO_OF_ATTRIBUTES_IN_KEY + 1];
      for (unsigned a = 0; a<oi.m_index->getNoOfColumns(); a++)
      {
        operands[opNo++] = 
          createLink(builder, 
                     oi.m_table->getColumn(oi.m_index->getColumn(a)->getName()),
                     parents, false);
      }
      operands[opNo] = 0;

      op.m_parent = parents[0].m_idx;
      op.m_op = builder.readTuple(oi.m_index, oi.m_table, operands, &options);
      break;
    }
    case NdbQueryOperationDef::TableScan:
      // not supported
      abort();
    case NdbQueryOperationDef::OrderedIndexScan:{
      int opNo = 0;
      NdbQueryOperand * operands[NDB_MAX_NO_OF_ATTRIBUTES_IN_KEY + 1];
      for (unsigned a = 0; a<oi.m_index->getNoOfColumns(); a++)
      {
        operands[opNo++] = 
          createLink(builder, 
                     oi.m_table->getColumn(oi.m_index->getColumn(a)->getName()),
                     parents, true);
      }
      operands[opNo] = 0;

      op.m_parent = parents[0].m_idx;
      NdbQueryIndexBound bounds(operands); // Only EQ for now
      op.m_op = builder.scanIndex(oi.m_index, oi.m_table, &bounds, &options);
      if (op.m_op == 0)
      {
        ndbout << "Failed to add to " << endl;
        for (unsigned i = 0; i<m_query.size(); i++)
          ndbout << m_query[i] << endl;

        ndbout << "Parents: " << endl;
        for (unsigned i = 0; i<parents.size(); i++)
          ndbout << parents[i].m_idx << " ";
        ndbout << endl;
      }
      break;
    }
    }
  }

  if (op.m_op == 0)
  {
    NdbError err = builder.getNdbError();
    ndbout << err << endl;
    return 0;
  }

  m_query.push_back(op);
  return op.m_op;
}

const NdbQueryDef *
HugoQueryBuilder::createQuery(bool takeOwnership)
{
  NdbQueryBuilder* const builder = NdbQueryBuilder::create();
  if (builder == NULL)
  {
    ndbout << "Failed to create NdbQueryBuilder." << endl;
    return 0;
  }

  {
    OptionMask save = m_options;
    if (testOption(O_SCAN))
    {
      clearOption(O_PK_INDEX);
      clearOption(O_UNIQUE_INDEX);
    }
    const NdbQueryOperationDef * rootOp = createOp(*builder);
    require(rootOp != 0);
    m_options = save;
  }

  /**
   * We only don't support table scans as child operations...
   */
  OptionMask save = m_options;
  clearOption(O_TABLE_SCAN);
  
  /**
   * Iff root is lookup...ordered index scans are not allowed as
   *   children
   */
  if (!isScan(m_query[0].m_op))
  {
    clearOption(O_ORDERED_INDEX);
  }

  int levels = getJoinLevel();
  while (levels --)
  {
    createOp(*builder);
  }

  m_options = save;

  const NdbQueryDef * def = builder->prepare(m_ndb);
  if (def == nullptr)
  {
    NdbError err = builder->getNdbError();
    ndbout << "OJA1: " << err << endl;
  }
  else if (!takeOwnership)
  {
    m_queries.push_back(def);
  }
  builder->destroy();
  m_query.clear();

  return def;
}

template class Vector<const NdbQueryDef*>;
template class Vector<HugoQueryBuilder::Op>;
template class Vector<NdbQueryOperationDef::Type>;
template class Vector<const NdbDictionary::Column*>;
template class Vector<HugoQueryBuilder::TableDef>;
template class Vector<const NdbDictionary::Index*>;
