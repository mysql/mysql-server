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

#include <NdbApi.hpp>
#include <common/StmtArea.hpp>
#include <dictionary/DictSchema.hpp>
#include <dictionary/DictTable.hpp>
#include <dictionary/DictColumn.hpp>
#include "Code_table.hpp"
#include "Code_column.hpp"
#include "Code_expr_column.hpp"

Plan_table::~Plan_table()
{
}

Plan_base*
Plan_table::analyze(Ctx& ctx, Ctl& ctl)
{
    if (m_dictTable != 0)	// already done
	return this;
    DictTable* table = dictSchema().findTable(m_name);
    if (table == 0) {
	table = dictSchema().loadTable(ctx, m_name);
	if (table == 0) {
	    ctx.pushStatus(Sqlstate::_42S02, Error::Gen, "table %s not found", m_name.c_str());
	    return 0;
	}
    }
    m_dictTable = table;
    // indexes
    m_indexList.resize(1 + m_dictTable->indexCount());
    for (unsigned i = 0; i <= indexCount(); i++) {
	Index& index = m_indexList[i];
	index.m_pos = i;
	if (index.m_pos == 0) {
	    index.m_keyCount = m_dictTable->keyCount();
	    index.m_rank = 0;
	} else {
	    index.m_dictIndex = m_dictTable->getIndex(i);
	    index.m_keyCount = index.m_dictIndex->getSize();
	    if (index.m_dictIndex->getType() == NdbDictionary::Object::UniqueHashIndex) {
		index.m_rank = 1;
	    } else if (index.m_dictIndex->getType() == NdbDictionary::Object::OrderedIndex) {
		index.m_rank = 2;
	    } else {
		ctx_assert(false);
	    }
	}
	index.m_keyEqList.resize(1 + index.m_keyCount);
    }
    return this;
}

int
Plan_table::resolveColumn(Ctx& ctx, Plan_column* column, bool stripSchemaName)
{
    ctx_assert(column != 0);
    bool dml, unq;
    switch (column->m_type) {
    case Plan_column::Type_expr:
	dml = false;
	unq = false;
	break;
    case Plan_column::Type_dml:
	dml = true;
	unq = true;
	break;
    case Plan_column::Type_idx:
	dml = false;
	unq = true;
	break;
    default:
	ctx_assert(false);
	break;
    }
    ColumnVector& columns = ! dml ? m_exprColumns : m_dmlColumns;
    const BaseString& name = column->m_name;
    const BaseString& cname = column->m_cname;
    ctx_log3(("resolve %s column %s in table %s", ! dml ? "expr" : "dml", column->getPrintName(), getPrintName()));
    // find column in table
    DictColumn* dictColumn = dictTable().findColumn(name);
    if (dictColumn == 0)
	return 0;
    // qualified column must match table correlation name
    if (! cname.empty()) {
	const char* str;
	if (! m_cname.empty()) {
	    str = m_cname.c_str();
	} else {
	    str = m_name.c_str();
	    if (stripSchemaName && strrchr(str, '.') != 0)
		str = strrchr(str, '.') + 1;
	}
	if (strcmp(cname.c_str(), str) != 0)
	    return 0;
    }
    // find in positional list or add to it
    unsigned resPos;
    for (resPos = 1; resPos < columns.size(); resPos++) {
	if (strcmp(columns[resPos]->getName().c_str(), name.c_str()) != 0)
	    continue;
	// these columns must be unique
	if (unq) {
	    ctx.pushStatus(Error::Gen, "duplicate column %s", column->getName().c_str());
	    return -1;
	}
	break;
    }
    if (resPos >= columns.size()) {
	columns.push_back(column);
    }
    ctx_log3(("resolve to attrId %u pos %u", (unsigned)dictColumn->getAttrId(), resPos));
    column->m_dictColumn = dictColumn;
    column->m_resTable = this;
    column->m_resPos = resPos;
    // found
    return 1;
}

bool
Plan_table::resolveEq(Ctx& ctx, Plan_expr_column* column, Plan_expr* expr)
{
    ctx_assert(m_dictTable != 0);
    const TableSet& ts = expr->tableSet();
    TableSet::const_iterator i = ts.find(this);
    if (i != ts.end())
	return false;
    unsigned found = 0;
    for (unsigned i = 0; i <= indexCount(); i++) {
	Index& index = m_indexList[i];
	for (unsigned n = 1, cnt = 0; n <= index.m_keyCount; n++) {
	    const DictColumn* dictColumn = 0;
	    if (index.m_pos == 0) {
		ctx_assert(m_dictTable != 0);
		dictColumn = m_dictTable->getKey(n);
	    } else {
		ctx_assert(index.m_dictIndex != 0);
		dictColumn = index.m_dictIndex->getColumn(n);
	    }
	    if (dictColumn != &column->dictColumn())
		continue;
	    ctx_assert(++cnt == 1);
	    index.m_keyEqList[n].push_back(expr);
	    if (index.m_pos == 0)
		ctx_log2(("%s: found match to primary key column %s pos %u", getPrintName(), column->getPrintName(), n));
	    else
		ctx_log2(("%s: found match to index %s column %s pos %u", getPrintName(), index.m_dictIndex->getName().c_str(), column->getPrintName(), n));
	    found++;
	}
    }
    return (found != 0);
}

void
Plan_table::resolveSet(Ctx& ctx, Index& index, const TableSet& tsDone)
{
    index.m_keyFound = false;
    ExprVector keyEq;
    keyEq.resize(1 + index.m_keyCount);
    resolveSet(ctx, index, tsDone, keyEq, 1);
}

void
Plan_table::resolveSet(Ctx& ctx, Index& index, const TableSet& tsDone, ExprVector& keyEq, unsigned n)
{
    if (n <= index.m_keyCount) {
	// building up combinations
	ExprList& keyEqList = index.m_keyEqList[n];
	for (ExprList::iterator i = keyEqList.begin(); i != keyEqList.end(); i++) {
	    keyEq[n] = *i;
	    resolveSet(ctx, index, tsDone, keyEq, n + 1);
	}
	if (! keyEqList.empty() || index.m_rank <= 1 || n == 1)
	    return;
	// ordered index with maximal initial key match
    }
    TableSet keySet;
    for (unsigned i = 1; i <= n - 1; i++) {
	const TableSet& tableSet = keyEq[i]->tableSet();
	for (TableSet::const_iterator j = tableSet.begin(); j != tableSet.end(); j++) {
	    if (tsDone.find(*j) == tsDone.end())
		keySet.insert(*j);
	}
    }
    if (! index.m_keyFound || index.m_keySet.size() > keySet.size()) {
	index.m_keyFound = true;
	index.m_keyEq = keyEq;
	index.m_keySet = keySet;
	index.m_keyCountUsed = n - 1;
	index.m_keyCountUnused = index.m_keyCount - index.m_keyCountUsed;
	// set matching size
	index.m_keyEq.resize(1 + index.m_keyCountUsed);
    }
}

bool
Plan_table::exactKey(Ctx& ctx, const Index* indexKey) const
{
    ctx_assert(indexKey != 0 && indexKey == &m_indexList[indexKey->m_pos]);
    for (unsigned i = 0; i <= indexCount(); i++) {
	const Index& index = m_indexList[i];
	const ExprListVector& keyEqList = index.m_keyEqList;
	for (unsigned n = 1; n <= index.m_keyCount; n++) {
	    if (index.m_pos == indexKey->m_pos) {
		ctx_assert(keyEqList[n].size() >= 1);
		if (keyEqList[n].size() > 1) {
		    ctx_log2(("index %u not exact: column %u has %u > 1 matches",
                             indexKey->m_pos,
                             n,
                             (unsigned)keyEqList[n].size()));
		    return false;
		}
	    } else {
		if (keyEqList[n].size() > 0) {
		    ctx_log2(("index %u not exact: index %u column %u has %u > 0 matches",
                             indexKey->m_pos,
                             index.m_pos,
                             n,
                             (unsigned)keyEqList[n].size()));
		    return false;
		}
	    }
	}
    }
    ctx_log2(("index %u is exact", indexKey->m_pos));
    return true;
}

Exec_base*
Plan_table::codegen(Ctx& ctx, Ctl& ctl)
{
    ctx_assert(false);
    return 0;
}

void
Plan_table::print(Ctx& ctx)
{
    ctx.print(" [table %s]", getPrintName());
}
