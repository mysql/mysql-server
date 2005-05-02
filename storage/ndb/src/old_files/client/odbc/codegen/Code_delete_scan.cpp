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
#include "Code_delete_scan.hpp"
#include "Code_root.hpp"

Plan_delete_scan::~Plan_delete_scan()
{
}

Plan_base*
Plan_delete_scan::analyze(Ctx& ctx, Ctl& ctl)
{
    ctx_assert(m_query != 0);
    m_query->analyze(ctx, ctl);
    if (! ctx.ok())
	return 0;
    return this;
}

void
Plan_delete_scan::describe(Ctx& ctx)
{
    stmtArea().setFunction(ctx, "DELETE WHERE", SQL_DIAG_DELETE_WHERE);
}

Exec_base*
Plan_delete_scan::codegen(Ctx& ctx, Ctl& ctl)
{   
    // create code for the subquery
    ctx_assert(m_query != 0);
    Exec_query* execQuery = static_cast<Exec_query*>(m_query->codegen(ctx, ctl));
    if (! ctx.ok())
	return 0;
    ctx_assert(execQuery != 0);
    // create the code
    Exec_delete_scan* exec = new Exec_delete_scan(ctl.m_execRoot);
    ctl.m_execRoot->saveNode(exec);
    Exec_delete_scan::Code& code = *new Exec_delete_scan::Code;
    exec->setCode(code);
    exec->setQuery(execQuery);
    return exec;
}    

void
Plan_delete_scan::print(Ctx& ctx)
{
    ctx.print(" [delete_scan");
    Plan_base* a[] = { m_query };
    printList(ctx, a, 1);
    ctx.print("]");
}

// Exec_delete_scan

Exec_delete_scan::Code::~Code()
{
}

Exec_delete_scan::Data::~Data()
{
}

Exec_delete_scan::~Exec_delete_scan()
{
}

void
Exec_delete_scan::alloc(Ctx& ctx, Ctl& ctl)
{
    // allocate the subquery
    ctx_assert(m_query != 0);
    m_query->alloc(ctx, ctl);
    if (! ctx.ok())
        return;
    // create data
    Data& data = *new Data;
    setData(data);
}

void
Exec_delete_scan::close(Ctx& ctx)
{
    ctx_assert(m_query != 0);
    m_query->close(ctx);
}

void
Exec_delete_scan::print(Ctx& ctx)
{
    ctx.print(" [delete_scan");
    Exec_base* a[] = { m_query };
    printList(ctx, a, 1);
    ctx.print("]");
}

