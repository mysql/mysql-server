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
#include "Code_query_group.hpp"
#include "Code_root.hpp"

// Plan_query_group

Plan_query_group::~Plan_query_group()
{
}

Plan_expr_row*
Plan_query_group::getRow()
{
    ctx_assert(m_dataRow != 0);
    return m_dataRow;
}

Plan_base*
Plan_query_group::analyze(Ctx& ctx, Ctl& ctl)
{
    ctx_assert(m_query != 0);
    m_query->analyze(ctx, ctl);
    if (! ctx.ok())
	return 0;
    ctx_assert(m_dataRow != 0);
    m_dataRow->analyze(ctx, ctl);
    if (! ctx.ok())
	return 0;
    ctx_assert(m_groupRow != 0);
    m_groupRow->analyze(ctx, ctl);
    if (! ctx.ok())
	return 0;
    if (m_havingPred != 0) {
	ctl.m_having = true;
	m_havingPred->analyze(ctx, ctl);
	if (! ctx.ok())
	    return 0;
	ctl.m_having = false;
    }
    return this;
}

Exec_base*
Plan_query_group::codegen(Ctx& ctx, Ctl& ctl)
{
    // create code for the subquery
    ctx_assert(m_query != 0);
    Exec_query* execQuery = static_cast<Exec_query*>(m_query->codegen(ctx, ctl));
    if (! ctx.ok())
	return 0;
    ctx_assert(execQuery != 0);
    // create code for the rows based on query code
    ctl.m_execQuery = execQuery;
    ctx_assert(m_dataRow != 0);
    Exec_expr_row* execDataRow = static_cast<Exec_expr_row*>(m_dataRow->codegen(ctx, ctl));
    if (! ctx.ok())
	return 0;
    ctx_assert(execDataRow != 0);
    ctx_assert(m_groupRow != 0);
    Exec_expr_row* execGroupRow = static_cast<Exec_expr_row*>(m_groupRow->codegen(ctx, ctl));
    if (! ctx.ok())
	return 0;
    ctx_assert(execGroupRow != 0);
    Exec_pred* execHavingPred = 0;
    if (m_havingPred != 0) {
	ctl.m_having = true;
	execHavingPred = static_cast<Exec_pred*>(m_havingPred->codegen(ctx, ctl));
	if (! ctx.ok())
	    return 0;
	ctx_assert(execHavingPred != 0);
	ctl.m_having = false;
    }
    // the exec node
    Exec_query_group* exec = new Exec_query_group(ctl.m_execRoot);
    ctl.m_execRoot->saveNode(exec);
    // re-use SqlSpecs from data row
    const SqlSpecs& sqlSpecs = execDataRow->getCode().sqlSpecs();
    Exec_query_group::Code& code = *new Exec_query_group::Code(sqlSpecs);
    exec->setCode(code);
    exec->setQuery(execQuery);
    exec->setDataRow(execDataRow);
    exec->setGroupRow(execGroupRow);
    if (execHavingPred != 0)
	exec->setHavingPred(execHavingPred);
    return exec;
}

void
Plan_query_group::print(Ctx& ctx)
{
    ctx.print(" [query_group");
    Plan_base* a[] = { m_query, m_dataRow, m_groupRow };
    printList(ctx, a, 3);
    ctx.print("]");
}

// Exec_query_group

Exec_query_group::Code::~Code()
{
}

Exec_query_group::Data::~Data()
{
    for (GroupList::iterator i = m_groupList.begin(); i != m_groupList.end(); i++) {
	delete (*i).first;
    }
}

Exec_query_group::~Exec_query_group()
{
}

const Exec_query*
Exec_query_group::getRawQuery() const
{
    ctx_assert(m_query != 0);
    return m_query;
}

void
Exec_query_group::alloc(Ctx& ctx, Ctl& ctl)
{
    // allocate subquery
    ctx_assert(m_query != 0);
    m_query->alloc(ctx, ctl);
    if (! ctx.ok())
	return;
    // allocate rows based on subquery data
    ctl.m_query = m_query;
    ctx_assert(m_dataRow != 0);
    m_dataRow->alloc(ctx, ctl);
    if (! ctx.ok())
	return;
    ctx_assert(m_groupRow != 0);
    m_groupRow->alloc(ctx, ctl);
    if (! ctx.ok())
	return;
    if (m_havingPred != 0) {
	m_havingPred->alloc(ctx, ctl);
	if (! ctx.ok())
	    return;
    }
    Data& data = *new Data(this, getCode().sqlSpecs());
    setData(data);
}

void
Exec_query_group::execImpl(Ctx& ctx, Ctl& ctl)
{
    ctx_assert(m_query != 0);
    m_query->execute(ctx, ctl);
}

bool
GroupLess::operator()(const SqlRow* s1, const SqlRow* s2) const
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
Exec_query_group::fetchImpl(Ctx& ctx, Ctl& ctl)
{
    Data& data = getData();
    ctx_assert(m_query != 0 && m_groupRow != 0);
    if (! data.m_grouped) {
	// read and group all rows
	while (m_query->fetch(ctx, ctl)) {
	    // evaluate and insert group-by values
	    m_groupRow->evaluate(ctx, ctl);
	    if (! ctx.ok())
		return false;
	    const SqlRow* groupRow = 0;
	    unsigned index = 0;
	    bool init;
	    GroupList::iterator i = data.m_groupList.find(&m_groupRow->getData().sqlRow());
	    if (i == data.m_groupList.end()) {
		groupRow = m_groupRow->getData().sqlRow().copy();
		index = ++data.m_count;
		const GroupList::value_type groupPair(groupRow, index);
		data.m_groupList.insert(groupPair);
		init = true;
	    } else {
		groupRow = (*i).first;
		index = (*i).second;
		ctx_assert(groupRow != 0 && index != 0);
		init = false;
	    }
	    // evaluate rows saving expression values at index position
	    ctl.m_groupIndex = index;
	    ctl.m_groupInit = init;
	    m_dataRow->evaluate(ctx, ctl);
	    if (! ctx.ok())
		return false;
	    if (m_havingPred != 0) {
		m_havingPred->evaluate(ctx, ctl);
		if (! ctx.ok())
		    return false;
	    }
	    if (ctl.m_sortRow != 0) {
		ctl.m_sortRow->evaluate(ctx, ctl);
		if (! ctx.ok())
		    return false;
	    }
	    ctl.m_groupIndex = 0;
	}
	if (! ctx.ok())
	    return false;
	data.m_iterator = data.m_groupList.begin();
	data.m_grouped = true;
    }
    while (data.m_iterator != data.m_groupList.end()) {
	const SqlRow* groupRow = (*data.m_iterator).first;
	const unsigned index = (*data.m_iterator).second;
	ctx_assert(groupRow != 0 && index != 0);
	if (m_havingPred != 0) {
	    Pred_value v = m_havingPred->getData().groupValue(index);
	    if (v != Pred_value_true) {
		data.m_iterator++;
		continue;
	    }
	}
	// make our SqlRow reference to the saved values
	for (unsigned i = 1; i <= data.m_sqlRow.count(); i++) {
	    const SqlField& currField = m_dataRow->getExpr(i)->getData().groupField(index);
	    SqlSpec sqlSpec(currField.sqlSpec(), SqlSpec::Reference);
	    SqlField sqlField(sqlSpec, &currField);
	    data.m_sqlRow.setEntry(i, sqlField);
	}
	// send group index up for possible order by
	ctl.m_groupIndex = index;
	data.m_iterator++;
	return true;
    }
    return false;
}

void
Exec_query_group::close(Ctx& ctx)
{
    Data& data = getData();
    ctx_assert(m_query != 0);
    m_query->close(ctx);
    ctx_assert(m_dataRow != 0);
    m_dataRow->close(ctx);
    ctx_assert(m_groupRow != 0);
    m_groupRow->close(ctx);
    if (m_havingPred != 0)
	m_havingPred->close(ctx);
    data.m_grouped = false;
    data.m_count = 0;
    for (GroupList::iterator i = data.m_groupList.begin(); i != data.m_groupList.end(); i++) {
	delete (*i).first;
    }
    data.m_groupList.clear();
}

void
Exec_query_group::print(Ctx& ctx)
{
    ctx.print(" [query_group");
    Exec_base* a[] = { m_query, m_dataRow, m_groupRow };
    printList(ctx, a, 3);
    ctx.print("]");
}
