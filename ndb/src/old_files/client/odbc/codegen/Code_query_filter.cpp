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

#include "Code_query_filter.hpp"
#include "Code_query_join.hpp"
#include "Code_query_scan.hpp"
#include "Code_root.hpp"

// Plan_query_filter

Plan_query_filter::~Plan_query_filter()
{
}

Plan_base*
Plan_query_filter::analyze(Ctx& ctx, Ctl& ctl)
{
    ctx_assert(m_query != 0);
    m_query->analyze(ctx, ctl);
    if (! ctx.ok())
	return 0;
    ctx_assert(m_pred != 0);
    m_pred->analyze(ctx, ctl);
    if (! ctx.ok())
	return 0;
    return this;
}

Exec_base*
Plan_query_filter::codegen(Ctx& ctx, Ctl& ctl)
{
    // generate code for the subquery
    ctx_assert(m_query != 0);
    Exec_query* execQuery = static_cast<Exec_query*>(m_query->codegen(ctx, ctl));
    if (! ctx.ok())
	return 0;
    ctx_assert(execQuery != 0);
    // create code for the predicate based on query code
    Exec_pred* execPred = 0;
    ctl.m_execQuery = execQuery;
    ctx_assert(m_topTable != 0);
    ctl.m_topTable = m_topTable;
    ctx_assert(m_pred != 0);
    execPred = static_cast<Exec_pred*>(m_pred->codegen(ctx, ctl));
    if (! ctx.ok())
	return 0;
    ctx_assert(execPred != 0);
    ctl.m_topTable = 0;
    // re-use SqlSpecs from subquery
    const Exec_query::Code& codeQuery = execQuery->getCode();
    const SqlSpecs& sqlSpecs = codeQuery.sqlSpecs();
    Exec_query_filter* exec = new Exec_query_filter(ctl.m_execRoot);
    ctl.m_execRoot->saveNode(exec);
    Exec_query_filter::Code& code = *new Exec_query_filter::Code(sqlSpecs);
    exec->setCode(code);
    exec->setQuery(execQuery);
    exec->setPred(execPred);
    return exec;
}

void
Plan_query_filter::print(Ctx& ctx)
{
    ctx.print(" [query_filter");
    Plan_base* a[] = { m_query, m_pred };
    printList(ctx, a, 2);
    ctx.print("]");
}

// Exec_query_filter

Exec_query_filter::Code::~Code()
{
}

Exec_query_filter::Data::~Data()
{
}

Exec_query_filter::~Exec_query_filter()
{
}

void
Exec_query_filter::alloc(Ctx& ctx, Ctl& ctl)
{
    // allocate the subquery
    ctx_assert(m_query != 0);
    m_query->alloc(ctx, ctl);
    if (! ctx.ok())
	return;
    // allocate the predicate
    ctl.m_query = m_query;
    ctx_assert(m_pred != 0);
    m_pred->alloc(ctx, ctl);
    if (! ctx.ok())
	return;
    // re-use SqlRow from subquery
    Exec_query::Data& dataQuery = m_query->getData();
    Data& data = *new Data(this, dataQuery.sqlRow());
    setData(data);
}

void
Exec_query_filter::execImpl(Ctx& ctx, Ctl& ctl)
{
    // execute subquery
    ctx_assert(m_query != 0);
    m_query->execute(ctx, ctl);
}

bool
Exec_query_filter::fetchImpl(Ctx& ctx, Ctl& ctl)
{
    // invoke fetch on subquery until predicate is true
    ctx_assert(m_query != 0);
    while (m_query->fetch(ctx, ctl)) {
	ctx_assert(m_pred != 0);
	m_pred->evaluate(ctx, ctl);
	if (! ctx.ok())
	    return false;
	if (m_pred->getData().getValue() == Pred_value_true) {
	    ctl.m_postEval = true;
	    m_pred->evaluate(ctx, ctl);
	    ctl.m_postEval = false;
	    return true;
	}
    }
    return false;
}

void
Exec_query_filter::close(Ctx& ctx)
{
    ctx_assert(m_query != 0);
    m_query->close(ctx);
    ctx_assert(m_pred != 0);
    m_pred->close(ctx);
}

void
Exec_query_filter::print(Ctx& ctx)
{
    ctx.print(" [query_filter");
    Exec_base* a[] = { m_query, m_pred };
    printList(ctx, a, 2);
    ctx.print("]");
}
