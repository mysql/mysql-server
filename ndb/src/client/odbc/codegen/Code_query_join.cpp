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

#include "Code_query.hpp"
#include "Code_query_join.hpp"
#include "Code_query_scan.hpp"
#include "Code_root.hpp"

// Plan_query_join

Plan_query_join::~Plan_query_join()
{
}

Plan_base*
Plan_query_join::analyze(Ctx& ctx, Ctl& ctl)
{
    ctx_assert(m_inner != 0);
    m_inner->analyze(ctx, ctl);
    if (! ctx.ok())
	return 0;
    ctx_assert(m_outer != 0);
    m_outer->analyze(ctx, ctl);
    if (! ctx.ok())
	return 0;
    return this;
}

Exec_base*
Plan_query_join::codegen(Ctx& ctx, Ctl& ctl)
{
    // generate code for subqueries
    ctx_assert(m_inner != 0);
    Exec_query* execInner = static_cast<Exec_query*>(m_inner->codegen(ctx, ctl));
    if (! ctx.ok())
	return 0;
    ctx_assert(execInner != 0);
    ctx_assert(m_outer != 0);
    ctl.m_execQuery = execInner;
    Exec_query* execOuter = static_cast<Exec_query*>(m_outer->codegen(ctx, ctl));
    if (! ctx.ok())
	return 0;
    ctx_assert(execOuter != 0);
    // combine sql specs from subqueries
    const SqlSpecs& specsInner = execInner->getCode().sqlSpecs();
    const SqlSpecs& specsOuter = execOuter->getCode().sqlSpecs();
    SqlSpecs sqlSpecs(specsInner.count() + specsOuter.count());
    for (unsigned i = 1; i <= specsInner.count(); i++) {
	const SqlSpec sqlSpec(specsInner.getEntry(i), SqlSpec::Reference);
	sqlSpecs.setEntry(i, sqlSpec);
    }
    for (unsigned i = 1; i <= specsOuter.count(); i++) {
	const SqlSpec sqlSpec(specsOuter.getEntry(i), SqlSpec::Reference);
	sqlSpecs.setEntry(specsInner.count() + i, sqlSpec);
    }
    // create the code
    Exec_query_join* exec = new Exec_query_join(ctl.m_execRoot);
    ctl.m_execRoot->saveNode(exec);
    exec->setInner(execInner);
    exec->setOuter(execOuter);
    Exec_query_join::Code& code = *new Exec_query_join::Code(sqlSpecs);
    exec->setCode(code);
    return exec;
}

void
Plan_query_join::print(Ctx& ctx)
{
    ctx.print(" [query_join");
    Plan_base* a[] = { m_inner, m_outer };
    printList(ctx, a, 2);
    ctx.print("]");
}

// Exec_query_join

Exec_query_join::Code::~Code()
{
}

Exec_query_join::Data::~Data()
{
}

Exec_query_join::~Exec_query_join()
{
}

void
Exec_query_join::alloc(Ctx& ctx, Ctl& ctl)
{
    // allocate the subqueries
    ctx_assert(m_inner != 0);
    m_inner->alloc(ctx, ctl);
    if (! ctx.ok())
	return;
    ctx_assert(m_outer != 0);
    ctl.m_query = m_inner;
    m_outer->alloc(ctx, ctl);
    if (! ctx.ok())
	return;
    // combine data rows from subqueries
    const Code& code = getCode();
    const SqlRow& rowInner = m_inner->getData().sqlRow();
    const SqlRow& rowOuter = m_outer->getData().sqlRow();
    SqlRow sqlRow(code.m_sqlSpecs);
    for (unsigned i = 1; i <= rowInner.count(); i++) {
	const SqlSpec& sqlSpec = code.m_sqlSpecs.getEntry(i);
	const SqlField sqlField(sqlSpec, &rowInner.getEntry(i));
	sqlRow.setEntry(i, sqlField);
    }
    for (unsigned i = 1; i <= rowOuter.count(); i++) {
	const SqlSpec& sqlSpec = code.m_sqlSpecs.getEntry(rowInner.count() + i);
	const SqlField sqlField(sqlSpec, &rowOuter.getEntry(i));
	sqlRow.setEntry(rowInner.count() + i, sqlField);
    }
    // create the data
    Data& data = *new Data(this, sqlRow);
    setData(data);
}

void
Exec_query_join::execImpl(Ctx& ctx, Ctl& ctl)
{
    // execute only inner query
    ctx_assert(m_inner != 0);
    m_inner->execute(ctx, ctl);
}

bool
Exec_query_join::fetchImpl(Ctx& ctx, Ctl& ctl)
{
    ctx_assert(m_inner != 0);
    ctx_assert(m_outer != 0);
    if (getData().getState() == ResultSet::State_init) {
	// fetch first row from inner
	if (! m_inner->fetch(ctx, ctl))
	    return false;
    }
    while (1) {
	if (m_outer->getData().getState() == ResultSet::State_end) {
	    // execute or re-execute outer
	    Ctl ctl(0);
	    m_outer->close(ctx);
	    if (! ctx.ok())
		return false;
	    m_outer->execute(ctx, ctl);
	    if (! ctx.ok())
		return false;
	}
	if (! m_outer->fetch(ctx, ctl)) {
	    if (! ctx.ok())
		return false;
	    // fetch next row from inner
	    if (! m_inner->fetch(ctx, ctl))
		return false;
	}
	else
	    return true;
    }
}

void
Exec_query_join::close(Ctx& ctx)
{
    ctx_assert(m_inner != 0);
    m_inner->close(ctx);
    ctx_assert(m_outer != 0);
    m_outer->close(ctx);
}

void
Exec_query_join::print(Ctx& ctx)
{
    ctx.print(" [query_join");
    Exec_base* a[] = { m_inner, m_outer };
    printList(ctx, a, 2);
    ctx.print("]");
}
