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
#include "Code_create_table.hpp"
#include "Code_root.hpp"

// Plan_create_table

Plan_create_table::~Plan_create_table()
{
}

Plan_base*
Plan_create_table::analyze(Ctx& ctx, Ctl& ctl)
{
    stmtArea().stmtInfo().setName(Stmt_name_create_table);
    // analyze the create row
    ctx_assert(m_createRow != 0);
    m_createRow->analyze(ctx, ctl);
    if (! ctx.ok())
	return 0;
    return this;
}

void
Plan_create_table::describe(Ctx& ctx)
{
    stmtArea().setFunction(ctx, "CREATE TABLE", SQL_DIAG_CREATE_TABLE);
}

Exec_base*
Plan_create_table::codegen(Ctx& ctx, Ctl& ctl)
{
    ctx_assert(m_createRow != 0);
    Exec_create_table* exec = new Exec_create_table(ctl.m_execRoot);
    ctl.m_execRoot->saveNode(exec);
    const unsigned count = m_createRow->countColumn();
    Exec_create_table::Code::Attr* attrList = new Exec_create_table::Code::Attr[1 + count];
    unsigned tupleId = 0;
    unsigned autoIncrement = 0;
    for (unsigned i = 1; i <= count; i++) {
	Plan_ddl_column* column = m_createRow->getColumn(i);
	Exec_create_table::Code::Attr& attr = attrList[i];
	attr.m_attrName.assign(column->getName());
	attr.m_sqlType = column->sqlType();
	attr.m_tupleKey = column->getPrimaryKey();
	attr.m_tupleId = column->getTupleId();
	attr.m_autoIncrement = column->getAutoIncrement();
	if (attr.m_tupleId)
	    tupleId = i;
	if (attr.m_autoIncrement)
	    autoIncrement = i;
	attr.m_defaultValue = 0;
	Plan_expr* expr;
	if ((expr = column->getDefaultValue()) != 0) {
	    Exec_expr* execExpr = static_cast<Exec_expr*>(expr->codegen(ctx, ctl));
	    if (! ctx.ok())
		return 0;
	    ctx_assert(execExpr != 0);
	    attr.m_defaultValue = execExpr;
	}
    }
    Exec_create_table::Code& code = *new Exec_create_table::Code(m_name, count, attrList, tupleId, autoIncrement);
    exec->setCode(code);
    code.m_fragmentType = m_fragmentType;
    code.m_logging = m_logging;
    return exec;
}

void
Plan_create_table::print(Ctx& ctx)
{
    ctx.print(" [create_table '%s'", m_name.c_str());
    Plan_base* a[] = { m_createRow };
    printList(ctx, a, 1);
    ctx.print("]");
}

// Exec_create_table

Exec_create_table::Code::~Code()
{
    delete[] m_attrList;
}

Exec_create_table::Data::~Data()
{
}

Exec_create_table::~Exec_create_table()
{
}

void
Exec_create_table::alloc(Ctx& ctx, Ctl& ctl)
{
    const Code& code = getCode();
    for (unsigned i = 1; i <= code.m_attrCount; i++) {
	const Code::Attr& attr = code.m_attrList[i];
	if (attr.m_defaultValue != 0)
	    attr.m_defaultValue->alloc(ctx, ctl);
    }
    Data& data = *new Data;
    setData(data);
}

void
Exec_create_table::close(Ctx& ctx)
{
    const Code& code = getCode();
    for (unsigned i = 1; i <= code.m_attrCount; i++) {
	const Code::Attr& attr = code.m_attrList[i];
	if (attr.m_defaultValue != 0)
	    attr.m_defaultValue->close(ctx);
    }
}

void
Exec_create_table::print(Ctx& ctx)
{
    const Code& code = getCode();
    ctx.print(" [create_table %s]", code.m_tableName.c_str());
}
