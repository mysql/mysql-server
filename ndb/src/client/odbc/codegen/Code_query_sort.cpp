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

#include <algorithm>
#include "Code_query_sort.hpp"
#include "Code_root.hpp"

// Plan_query_sort

Plan_query_sort::~Plan_query_sort()
{
}

Plan_expr_row*
Plan_query_sort::getRow()
{
    ctx_assert(m_query != 0);
    return m_query->getRow();
}

Plan_base*
Plan_query_sort::analyze(Ctx& ctx, Ctl& ctl)
{
    ctx_assert(m_query != 0);
    m_query->analyze(ctx, ctl);
    if (! ctx.ok())
	return 0;
    ctx_assert(m_sortRow != 0);
    m_sortRow->analyze(ctx, ctl);
    if (! ctx.ok())
	return 0;
    return this;
}

Exec_base*
Plan_query_sort::codegen(Ctx& ctx, Ctl& ctl)
{
    // create code for the subquery
    ctx_assert(m_query != 0);
    Exec_query* execQuery = static_cast<Exec_query*>(m_query->codegen(ctx, ctl));
    if (! ctx.ok())
	return 0;
    ctx_assert(execQuery != 0);
    // create code for the row based on query code
    ctx_assert(m_sortRow != 0);
    ctl.m_execQuery = execQuery->getRawQuery();
    Exec_expr_row* execRow = static_cast<Exec_expr_row*>(m_sortRow->codegen(ctx, ctl));
    if (! ctx.ok())
	return 0;
    ctx_assert(execRow != 0);
    Exec_query_sort* exec = new Exec_query_sort(ctl.m_execRoot);
    ctl.m_execRoot->saveNode(exec);
    // re-use SqlSpecs from subquery
    const Exec_query::Code& codeQuery = execQuery->getCode();
    const SqlSpecs& sqlSpecs = codeQuery.sqlSpecs();
    // make asc
    unsigned size = m_sortRow->getSize();
    bool* asc = new bool[1 + size];
    for (unsigned i = 1; i <= size; i++) {
	asc[i] = m_sortRow->m_ascList[i];
    }
    Exec_query_sort::Code& code = *new Exec_query_sort::Code(sqlSpecs, asc);
    exec->setCode(code);
    exec->setQuery(execQuery);
    exec->setRow(execRow);
    return exec;
}

void
Plan_query_sort::print(Ctx& ctx)
{
    ctx.print(" [query_sort");
    Plan_base* a[] = { m_query, m_sortRow };
    printList(ctx, a, 2);
    ctx.print("]");
}

// Exec_query_sort

Exec_query_sort::Code::~Code()
{
}

Exec_query_sort::Data::~Data()
{
    for (unsigned i = 0; i < m_sortList.size(); i++) {
	SortItem& sortItem = m_sortList[i];
	delete sortItem.m_dataRow;
	delete sortItem.m_sortRow;
    }
}

Exec_query_sort::~Exec_query_sort()
{
}

void
Exec_query_sort::alloc(Ctx& ctx, Ctl& ctl)
{
    // allocate subquery
    ctx_assert(m_query != 0);
    m_query->alloc(ctx, ctl);
    if (! ctx.ok())
	return;
    // allocate sort row based on subquery data
    ctx_assert(m_sortRow != 0);
    ctl.m_query = m_query->getRawQuery();
    m_sortRow->alloc(ctx, ctl);
    if (! ctx.ok())
	return;
    Data& data = *new Data(this, getCode().sqlSpecs());
    setData(data);
}

void
Exec_query_sort::execImpl(Ctx& ctx, Ctl& ctl)
{
    ctx_assert(m_query != 0 && m_sortRow != 0);
    ctl.m_sortRow = m_sortRow;
    m_query->execute(ctx, ctl);
}

bool
SortLess::operator()(SortItem s1, SortItem s2) const
{
    const Exec_query_sort::Code& code = m_node->getCode();
    const SqlRow& r1 = *s1.m_sortRow;
    const SqlRow& r2 = *s2.m_sortRow;
    for (unsigned i = 1; i <= r1.count(); i++) {
	const SqlField& f1 = r1.getEntry(i);
	const SqlField& f2 = r2.getEntry(i);
	// nulls last is default in oracle
	bool f1null = f1.sqlNull();
	bool f2null = f2.sqlNull();
	if (f1null && f2null)
	    continue;
	if (! f1null && f2null)
	    return code.getAsc(i) ? true : false;
	if (f1null && ! f2null)
	    return code.getAsc(i) ? false : true;
	if (f1.less(f2))
	    return code.getAsc(i) ? true : false;
	if (f2.less(f1))
	    return code.getAsc(i) ? false : true;
    }
    return false;
}

bool
Exec_query_sort::fetchImpl(Ctx& ctx, Ctl& ctl)
{
    Data& data = getData();
    ctx_assert(m_query != 0 && m_sortRow != 0);
    ctl.m_sortRow = m_sortRow;
    if (! data.m_sorted) {
	// read and cache all rows
	data.m_count = 0;
	while (m_query->fetch(ctx, ctl)) {
	    const SqlRow* dataRow = m_query->getData().sqlRow().copy();
	    const SqlRow* sortRow = 0;
	    if (ctl.m_groupIndex == 0) {
		// evaluate sort row
		m_sortRow->evaluate(ctx, ctl);
		if (! ctx.ok())
		    return false;
		sortRow = m_sortRow->getData().sqlRow().copy();
	    } else {
		// evaluate done by group-by
		SqlRow tmpSortRow(m_sortRow->getCode().sqlSpecs());
		for (unsigned i = 1; i <= tmpSortRow.count(); i++) {
		    tmpSortRow.setEntry(i, m_sortRow->getExpr(i)->getData().groupField(ctl.m_groupIndex));
		}
		sortRow = tmpSortRow.copy();
	    }
	    SortItem sortItem(dataRow, sortRow);
	    data.m_sortList.push_back(sortItem);
	    data.m_count++;
	}
	data.m_index = 0;
	if (! ctx.ok())
	    return false;
	// sort the rows  XXX use iterated stable_sort
	SortLess sortLess(this);
	std::sort(data.m_sortList.begin(), data.m_sortList.end(), sortLess);
	data.m_sorted = true;
    }
    if (data.m_index < data.m_count) {
	// make our SqlRow reference to current row
	const SqlRow& currRow = *data.m_sortList[data.m_index].m_dataRow;
	for (unsigned i = 1; i <= data.m_sqlRow.count(); i++) {
	    const SqlField& currField = currRow.getEntry(i);
	    SqlSpec sqlSpec(currField.sqlSpec(), SqlSpec::Reference);
	    SqlField sqlField(sqlSpec, &currField);
	    data.m_sqlRow.setEntry(i, sqlField);
	}
	data.m_index++;
	return true;
    }
    return false;
}

void
Exec_query_sort::close(Ctx& ctx)
{
    Data& data = getData();
    ctx_assert(m_query != 0);
    m_query->close(ctx);
    data.m_sorted = false;
    for (unsigned i = 0; i < data.m_sortList.size(); i++) {
	SortItem& sortItem = data.m_sortList[i];
	delete sortItem.m_dataRow;
	delete sortItem.m_sortRow;
    }
    data.m_sortList.clear();
    data.m_count = 0;
    data.m_index = 0;
}

void
Exec_query_sort::print(Ctx& ctx)
{
    ctx.print(" [query_sort");
    Exec_base* a[] = { m_query, m_sortRow };
    printList(ctx, a, 2);
    ctx.print("]");
}
