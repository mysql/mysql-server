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
#include "Code_query_distinct.hpp"
#include "Code_root.hpp"

// Plan_query_distinct

Plan_query_distinct::~Plan_query_distinct()
{
}

Plan_expr_row*
Plan_query_distinct::getRow()
{
    ctx_assert(m_query != 0);
    return m_query->getRow();
}

Plan_base*
Plan_query_distinct::analyze(Ctx& ctx, Ctl& ctl)
{
    ctx_assert(m_query != 0);
    m_query->analyze(ctx, ctl);
    if (! ctx.ok())
	return 0;
    return this;
}

Exec_base*
Plan_query_distinct::codegen(Ctx& ctx, Ctl& ctl)
{
    // create code for the subquery
    ctx_assert(m_query != 0);
    Exec_query* execQuery = static_cast<Exec_query*>(m_query->codegen(ctx, ctl));
    if (! ctx.ok())
	return 0;
    ctx_assert(execQuery != 0);
    // the exec node
    Exec_query_distinct* exec = new Exec_query_distinct(ctl.m_execRoot);
    ctl.m_execRoot->saveNode(exec);
    // re-use SqlSpecs from subquery
    const Exec_query::Code& codeQuery = execQuery->getCode();
    const SqlSpecs& sqlSpecs = codeQuery.sqlSpecs();
    Exec_query_distinct::Code& code = *new Exec_query_distinct::Code(sqlSpecs);
    exec->setCode(code);
    exec->setQuery(execQuery);
    return exec;
}

void
Plan_query_distinct::print(Ctx& ctx)
{
    ctx.print(" [query_distinct");
    Plan_base* a[] = { m_query };
    printList(ctx, a, 1);
    ctx.print("]");
}

// Exec_query_distinct

Exec_query_distinct::Code::~Code()
{
}

Exec_query_distinct::Data::~Data()
{
    for (DistinctList::iterator i = m_groupList.begin(); i != m_groupList.end(); i++) {
	delete (*i).first;
    }
}

Exec_query_distinct::~Exec_query_distinct()
{
}

const Exec_query*
Exec_query_distinct::getRawQuery() const
{
    ctx_assert(m_query != 0);
    return m_query->getRawQuery();
}

void
Exec_query_distinct::alloc(Ctx& ctx, Ctl& ctl)
{
    // allocate subquery
    ctx_assert(m_query != 0);
    m_query->alloc(ctx, ctl);
    if (! ctx.ok())
	return;
    Data& data = *new Data(this, getCode().sqlSpecs());
    setData(data);
}

void
Exec_query_distinct::execImpl(Ctx& ctx, Ctl& ctl)
{
    ctx_assert(m_query != 0);
    m_query->execute(ctx, ctl);
}

bool
DistinctLess::operator()(const SqlRow* s1, const SqlRow* s2) const
{
    ctx_assert(s1 != 0 && s2 != 0);
    const SqlRow& r1 = *s1;
    const SqlRow& r2 = *s2;
    for (unsigned i = 1; i <= r1.count(); i++) {
	const SqlField& f1 = r1.getEntry(i);
	const SqlField& f2 = r2.getEntry(i);
	// nulls last is default in oracle
	const bool f1null = f1.sqlNull();
	const bool f2null = f2.sqlNull();
	if (f1null && f2null)
	    continue;
	if (! f1null && f2null)
	    return true;
	if (f1null && ! f2null)
	    return false;
	if (f1.less(f2))
	    return true;
	if (f2.less(f1))
	    return false;
    }
    return false;
}

bool
Exec_query_distinct::fetchImpl(Ctx& ctx, Ctl& ctl)
{
    Data& data = getData();
    ctx_assert(m_query != 0);
    if (! data.m_grouped) {
	// read and group all rows
	while (m_query->fetch(ctx, ctl)) {
	    const SqlRow* dataRow = &m_query->getData().sqlRow();
	    DistinctList::iterator i = data.m_groupList.find(dataRow);
	    if (i != data.m_groupList.end())
		continue;
	    unsigned index = data.m_count++;
	    dataRow = dataRow->copy();
	    const DistinctList::value_type groupPair(dataRow, index);
	    data.m_groupList.insert(groupPair);
	    data.m_groupVector.push_back(dataRow);
	}
	if (! ctx.ok())
	    return false;
	data.m_index = 0;
	data.m_grouped = true;
    }
    ctx_assert(data.m_count == data.m_groupVector.size());
    if (data.m_index < data.m_count) {
	const SqlRow* currRow = data.m_groupVector[data.m_index];
	// make our SqlRow reference to it
	for (unsigned i = 1; i <= data.m_sqlRow.count(); i++) {
	    const SqlField& currField = currRow->getEntry(i);
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
Exec_query_distinct::close(Ctx& ctx)
{
    Data& data = getData();
    ctx_assert(m_query != 0);
    m_query->close(ctx);
    data.m_grouped = false;
    data.m_count = 0;
    for (DistinctList::iterator i = data.m_groupList.begin(); i != data.m_groupList.end(); i++) {
	delete (*i).first;
    }
    data.m_groupList.clear();
    data.m_groupVector.clear();
}

void
Exec_query_distinct::print(Ctx& ctx)
{
    ctx.print(" [query_distinct");
    Exec_base* a[] = { m_query };
    printList(ctx, a, 1);
    ctx.print("]");
}
