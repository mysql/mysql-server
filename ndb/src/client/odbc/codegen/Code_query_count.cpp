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

#include <common/StmtArea.hpp>
#include <dictionary/DictTable.hpp>
#include <dictionary/DictColumn.hpp>
#include "Code_query_count.hpp"
#include "Code_column.hpp"
#include "Code_expr_row.hpp"
#include "Code_root.hpp"

// Plan_query_count

Plan_query_count::~Plan_query_count()
{
}

Plan_expr_row*
Plan_query_count::getRow()
{
    ctx_assert(m_exprRow != 0);
    return m_exprRow;
}

Plan_base*
Plan_query_count::analyze(Ctx& ctx, Ctl& ctl)
{
    ctx_assert(m_exprRow != 0);
    ctl.m_aggrok = true;
    ctl.m_aggrin = false;
    m_exprRow->analyze(ctx, ctl);
    ctl.m_aggrok = false;
    if (! ctx.ok())
	return 0;
    ctx_assert(m_query != 0);
    m_query->analyze(ctx, ctl);
    if (! ctx.ok())
	return 0;
    return this;
}

Exec_base*
Plan_query_count::codegen(Ctx& ctx, Ctl& ctl)
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
    Exec_query_count* exec = new Exec_query_count(ctl.m_execRoot);
    ctl.m_execRoot->saveNode(exec);
    // re-use SqlSpecs from the row
    const SqlSpecs& sqlSpecs = execRow->getCode().sqlSpecs();
    Exec_query_count::Code& code = *new Exec_query_count::Code(sqlSpecs);
    exec->setCode(code);
    exec->setQuery(execQuery);
    exec->setRow(execRow);
    return exec;
}

void
Plan_query_count::print(Ctx& ctx)
{
    ctx.print(" [query_count");
    Plan_base* a[] = { m_query, m_exprRow };
    printList(ctx, a, sizeof(a)/sizeof(a[0]));
    ctx.print("]");
}

// Exec_query_count

Exec_query_count::Code::~Code()
{
}

Exec_query_count::Data::~Data()
{
}

Exec_query_count::~Exec_query_count()
{
}

const Exec_query*
Exec_query_count::getRawQuery() const
{
    ctx_assert(m_query != 0);
    return m_query;
}

void
Exec_query_count::alloc(Ctx& ctx, Ctl& ctl)
{
    // allocate the subquery
    ctx_assert(m_query != 0);
    m_query->alloc(ctx, ctl);
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
Exec_query_count::execImpl(Ctx& ctx, Ctl& ctl)
{
    Data& data = getData();
    // zero counters
    ctx_assert(m_exprRow != 0);
    m_exprRow->close(ctx);
    data.m_done = false;
    // execute the subquery
    ctx_assert(m_query != 0);
    m_query->execute(ctx, ctl);
}

bool
Exec_query_count::fetchImpl(Ctx& ctx, Ctl& ctl)
{
    Data& data = getData();
    // returns one row only
    if (data.m_done)
	return false;
    ctx_assert(m_query != 0 && m_exprRow != 0);
    while (m_query->fetch(ctx, ctl)) {
	// accumulate values
	m_exprRow->evaluate(ctx, ctl);
	if (! ctx.ok())
	    return false;
    }
    data.m_done = true;
    return true;
}

void
Exec_query_count::close(Ctx& ctx)
{
    ctx_assert(m_query != 0);
    m_query->close(ctx);
    ctx_assert(m_exprRow != 0);
    m_exprRow->close(ctx);
}

void
Exec_query_count::print(Ctx& ctx)
{
    ctx.print(" [query_count");
    Exec_base* a[] = { m_query };
    printList(ctx, a, 1);
    ctx.print("]");
}
