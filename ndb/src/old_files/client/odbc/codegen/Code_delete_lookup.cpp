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
#include "Code_expr.hpp"
#include "Code_delete_lookup.hpp"
#include "Code_table.hpp"
#include "Code_root.hpp"

Plan_delete_lookup::~Plan_delete_lookup()
{
}

Plan_base*
Plan_delete_lookup::analyze(Ctx& ctx, Ctl& ctl)
{
    ctx_assert(m_query != 0);
    m_query->analyze(ctx, ctl);
    if (! ctx.ok())
	return 0;
    ctx_assert(m_table != 0);
    m_table->analyze(ctx, ctl);
    if (! ctx.ok())
	return 0;
    return this;
}

void
Plan_delete_lookup::describe(Ctx& ctx)
{
    stmtArea().setFunction(ctx, "DELETE WHERE", SQL_DIAG_DELETE_WHERE);
}

Exec_base*
Plan_delete_lookup::codegen(Ctx& ctx, Ctl& ctl)
{   
    // create code for the query
    ctx_assert(m_query != 0);
    Exec_query* execQuery = static_cast<Exec_query*>(m_query->codegen(ctx, ctl));
    if (! ctx.ok())
	return 0;
    ctx_assert(execQuery != 0);
    // set up
    ctx_assert(m_table != 0);
    const BaseString& tableName = m_table->getName();
    const DictTable& dictTable = m_table->dictTable();
    const unsigned keyCount = dictTable.keyCount();
    // create the code
    Exec_delete_lookup::Code& code = *new Exec_delete_lookup::Code(keyCount);
    code.m_tableName = strcpy(new char[tableName.length() + 1], tableName.c_str());
    // key attributes
    code.m_keyId = new NdbAttrId[1 + keyCount];
    code.m_keyId[0] = (NdbAttrId)-1;
    for (unsigned k = 1; k <= keyCount; k++) {
	const DictColumn* keyColumn = dictTable.getKey(k);
	const SqlType& sqlType = keyColumn->sqlType();
	SqlSpec sqlSpec(sqlType, SqlSpec::Physical);
	code.m_keySpecs.setEntry(k, sqlSpec);
	code.m_keyId[k] = keyColumn->getAttrId();
    }
    // matching expressions
    const Plan_table::Index& index = m_table->m_indexList[0];
    ctx_assert(index.m_keyFound);
    const ExprVector& keyEq = index.m_keyEq;
    ctx_assert(keyEq.size() == 1 + keyCount);
    code.m_keyMatch = new Exec_expr* [1 + keyCount];
    code.m_keyMatch[0] = 0;
    for (unsigned k = 1; k <= keyCount; k++) {
	Plan_expr* expr = keyEq[k];
	Exec_expr* execExpr = static_cast<Exec_expr*>(expr->codegen(ctx, ctl));
	if (! ctx.ok())
	    return 0;
	ctx_assert(execExpr != 0);
	code.m_keyMatch[k] = execExpr;
    }
    // create the exec
    Exec_delete_lookup* exec = new Exec_delete_lookup(ctl.m_execRoot);
    ctl.m_execRoot->saveNode(exec);
    exec->setCode(code);
    exec->setQuery(execQuery);
    return exec;
}    

void
Plan_delete_lookup::print(Ctx& ctx)
{
    ctx.print(" [delete_lookup");
    Plan_base* a[] = { m_query, m_table };
    printList(ctx, a, 2);
    ctx.print("]");
}

// Exec_delete_lookup

Exec_delete_lookup::Code::~Code()
{
    delete[] m_tableName;
    delete[] m_keyId;
    delete[] m_keyMatch;
}

Exec_delete_lookup::Data::~Data()
{
}

Exec_delete_lookup::~Exec_delete_lookup()
{
}

void
Exec_delete_lookup::alloc(Ctx& ctx, Ctl& ctl)
{
    const Code& code = getCode();
    // allocate the subquery
    ctx_assert(m_query != 0);
    m_query->alloc(ctx, ctl);
    if (! ctx.ok())
        return;
    // allocate matching expressions
    for (unsigned k = 1; k <= code.m_keyCount; k++) {
	Exec_expr* expr = code.m_keyMatch[k];
	ctx_assert(expr != 0);
	expr->alloc(ctx, ctl);
	if (! ctx.ok())
	    return;
    }
    // create data
    Data& data = *new Data;
    setData(data);
}

void
Exec_delete_lookup::close(Ctx& ctx)
{
    ctx_assert(m_query != 0);
    m_query->close(ctx);
}

void
Exec_delete_lookup::print(Ctx& ctx)
{
    const Code& code = getCode();
    ctx.print(" [delete_lookup");
    Exec_base* a[] = { m_query };
    printList(ctx, a, 1);
    printList(ctx, (Exec_base**)&code.m_keyMatch[1], code.m_keyCount);
    ctx.print("]");
}
