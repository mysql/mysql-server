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

#include <HugoQueryBuilder.hpp>

void
HugoQueryBuilder::init()
{
  m_options = 0;
  setMinJoinLevel(2);
  setMaxJoinLevel(4);
}

HugoQueryBuilder::~HugoQueryBuilder()
{
  for (size_t i = 0; i<m_queries.size(); i++)
    m_queries[i]->release();
}

void
HugoQueryBuilder::fixOptions()
{
  setOption(O_PK_INDEX);
  setOption(O_UNIQUE_INDEX);
  setOption(O_TABLE_SCAN);
  setOption(O_ORDERED_INDEX);
  if (testOption(O_LOOKUP))
  {
    clearOption(O_TABLE_SCAN);
    clearOption(O_ORDERED_INDEX);
  }
}

void
HugoQueryBuilder::addTable(Ndb* ndb, const NdbDictionary::Table* tab)
{
  for (size_t i = 0; i<m_tables.size(); i++)
  {
    if (m_tables[i].m_table == tab)
      return;
  }

  TableDef def;
  def.m_table = tab;

  /**
   * TODO discover indexes...
   */

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
  for (size_t i = 0; i<m_tables.size(); i++)
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
                                Vector<Op> ops)
{
  for (size_t c = 0; c < cols.size(); c++)
  {
    const NdbDictionary::Column * col = cols[c];
    bool found = false;
    for (size_t t = 0; !found && t<ops.size(); t++)
    {
      const NdbDictionary::Table * tab = ops[t].m_op->getTable();
      if (tab)
      {
        for (int i = 0; i<tab->getNoOfColumns(); i++)
        {
          if (col->isBindable(* tab->getColumn(i)) == 0)
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


Vector<HugoQueryBuilder::Op>
HugoQueryBuilder::getParents(OpIdx oi)
{
  /**
   * We need to be able to bind all columns in table/index
   */
  Vector<const NdbDictionary::Column*> cols;
  if (oi.m_index == 0)
  {
    for (int i = 0; i<oi.m_table->getNoOfColumns(); i++)
      if (oi.m_table->getColumn(i)->getPrimaryKey())
        cols.push_back(oi.m_table->getColumn(i));
  }
  else
  {
    abort(); // TODO
  }

  int r = rand() % m_query.size();
  int cnt = (int)m_query.size();
  for (int i = 0; i < cnt; i++)
  {
    Vector<Op> set;
    Op op = m_query[(i + r) % cnt];
    set.push_back(op);

#if 0
    /**
     * add parents
     */
    if (testOption(O_MULTI_PARENT))
    {
      while (op.m_parent != -1)
      {
        op = m_query[op.m_parent];
        set.push_back(op);
      }
    }
#endif

    if (checkBindable(cols, set))
      return set;
  }

  Vector<Op> ret;
  return ret;
}

NdbQueryOperand *
HugoQueryBuilder::createLink(NdbQueryBuilder& builder,
                             const NdbDictionary::Column* pCol,
                             Vector<Op> & parents)
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
      if (pCol->isBindable(* col) == 0)
      {
        goto found;
      }
    }
  }
  return 0;

found:
  NdbQueryOperand * ret = builder.linkedValue(op.m_op, col->getName());
  assert(ret);
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
    case NdbQueryOperationDef::OrderedIndexScan:
      abort();
      break;
    case NdbQueryOperationDef::UniqueIndexAccess:
      // TODO
      abort();
    }
  }
  else
  {
loop:
    OpIdx oi = getOp();
    Vector<Op> parents = getParents(oi);
    if (parents.size() == 0)
    {
      // no possible parents found for pTab...try another
      goto loop;
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
                                        parents);
        }
      }
      operands[opNo] = 0;

      op.m_parent = parents[0].m_idx;
      op.m_op = builder.readTuple(oi.m_table, operands);
      break;
    }
    case NdbQueryOperationDef::TableScan:
      // not supported
      abort();
    case NdbQueryOperationDef::OrderedIndexScan:
    case NdbQueryOperationDef::UniqueIndexAccess:
      // TODO
      abort();
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
HugoQueryBuilder::createQuery(Ndb* pNdb, bool takeOwnership)
{
  NdbQueryBuilder builder(*pNdb);

  {
    OptionMask save = m_options;
    if (testOption(O_SCAN))
    {
      clearOption(O_PK_INDEX);
      clearOption(O_UNIQUE_INDEX);
    }
    const NdbQueryOperationDef * rootOp = createOp(builder);
    assert(rootOp != 0);
    m_options = save;
  }

  /**
   * We only support lookups as child operations...
   */
  OptionMask save = m_options;
  clearOption(O_ORDERED_INDEX);
  clearOption(O_TABLE_SCAN);

  int levels = getJoinLevel();
  while (levels --)
  {
    createOp(builder);
  }

  m_options = save;

  const NdbQueryDef * def = builder.prepare();
  if (def != 0 && !takeOwnership)
  {
    m_queries.push_back(def);
  }

  m_query.clear();

  return def;
}

template class Vector<const NdbQueryDef*>;
template class Vector<HugoQueryBuilder::Op>;
template class Vector<NdbQueryOperationDef::Type>;
template class Vector<const NdbDictionary::Column*>;
template class Vector<HugoQueryBuilder::TableDef>;
template class Vector<const NdbDictionary::Index*>;
