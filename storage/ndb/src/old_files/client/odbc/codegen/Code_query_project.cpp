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

#include "Code_query_project.hpp"
#include "Code_root.hpp"

// Plan_query_project

Plan_query_project::~Plan_query_project()
{
}

Plan_base*
Plan_query_project::analyze(Ctx& ctx, Ctl& ctl)
{
    ctx_assert(m_query != 0);
    m_query->analyze(ctx, ctl);
    if (! ctx.ok())
	return 0;
    ctx_assert(m_exprRow != 0);
    ctl.m_aggrok = true;
    ctl.m_aggrin = false;
    m_exprRow->analyze(ctx, ctl);
    ctl.m_aggrok = false;
    if (! ctx.ok())
	return 0;
    return this;
}

Plan_expr_row*
Plan_query_project::getRow()
{
    ctx_assert(m_exprRow != 0);
    return m_exprRow;
}

Exec_base*
Plan_query_project::codegen(Ctx& ctx, Ctl& ctl)
{
    // create code for the subquery
    ctx_assert(m_query != 0);
    Exec_query* execQuery = static_cast<Exec_query*>(m_query->codegen(ctx, ctl));
    if (! ctx.ok())
	return 0;
    ctx_assert(execQuery != 0);
    // create code for the row based on query code
    ctx_assert(m_exprRow != 0);
    ctl.m_execQuery = execQuery;
    Exec_expr_row* execRow = static_cast<Exec_expr_row*>(m_exprRow->codegen(ctx, ctl));
    if (! ctx.ok())
	return 0;
    ctx_assert(execRow != 0);
    Exec_query_project* exec = new Exec_query_project(ctl.m_execRoot);
    ctl.m_execRoot->saveNode(exec);
    // re-use SqlSpecs from the row
    const SqlSpecs& sqlSpecs = execRow->getCode().sqlSpecs();
    Exec_query_project::Code& code = *new Exec_query_project::Code(sqlSpecs);
    code.m_limitOff = m_limitOff;
    code.m_limitCnt = m_limitCnt;
    exec->setCode(code);
    exec->setQuery(execQuery);
    exec->setRow(execRow);
    return exec;
}

void
Plan_query_project::print(Ctx& ctx)
{
    ctx.print(" [query_project");
    Plan_base* a[] = { m_query, m_exprRow };
    printList(ctx, a, 2);
    ctx.print("]");
}

// Exec_query_project

Exec_query_project::Code::~Code()
{
}

Exec_query_project::Data::~Data()
{
}

Exec_query_project::~Exec_query_project()
{
}

const Exec_query*
Exec_query_project::getRawQuery() const
{
    ctx_assert(m_query != 0);
    return m_query;
}

void
Exec_query_project::alloc(Ctx& ctx, Ctl& ctl)
{
    // allocate the subquery
    ctx_assert(m_query != 0);
    m_query->alloc(ctx, ctl);
    if (! ctx.ok())
	return;
    // allocate the row based on subquery data
    ctx_assert(m_exprRow != 0);
    ctl.m_query = m_query;
    m_exprRow->alloc(ctx, ctl);
    if (! ctx.ok())
	return;
    // re-use SqlRow from the expression row
    const SqlRow& sqlRow = m_exprRow->getData().sqlRow();
    Data& data = *new Data(this, sqlRow);
    setData(data);
}

void
Exec_query_project::execImpl(Ctx& ctx, Ctl& ctl)
{
    ctx_assert(m_query != 0);
    m_query->execute(ctx, ctl);
}

bool
Exec_query_project::fetchImpl(Ctx& ctx, Ctl& ctl)
{
    const Code& code = getCode();
    Data& data = getData();
    ctx_assert(m_query != 0);
    while (1) {
	if (! m_query->fetch(ctx, ctl))
	    return false;
	ctx_assert(m_exprRow != 0);
	m_exprRow->evaluate(ctx, ctl);
	if (! ctx.ok())
	    return false;
	ctl.m_postEval = true;
	m_exprRow->evaluate(ctx, ctl);
	ctl.m_postEval = false;
	const int n = ++data.m_cnt;
	const int o = code.m_limitOff <= 0 ? 0 : code.m_limitOff;
	const int c = code.m_limitCnt;
	if (n <= o)
	    continue;
	if (c < 0)
	    break;
	if (n - o <= c)
	    break;
	return false;
    }
    return true;
}

void
Exec_query_project::close(Ctx& ctx)
{
    Data& data = getData();
    data.m_cnt = 0;
    ctx_assert(m_query != 0);
    m_query->close(ctx);
    ctx_assert(m_exprRow != 0);
    m_exprRow->close(ctx);
}

void
Exec_query_project::print(Ctx& ctx)
{
    ctx.print(" [query_project");
    Exec_base* a[] = { m_query, m_exprRow };
    printList(ctx, a, 2);
    ctx.print("]");
}
