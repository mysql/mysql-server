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
#include "Code_create_index.hpp"
#include "Code_root.hpp"

// Plan_create_index

Plan_create_index::~Plan_create_index()
{
}

Plan_base*
Plan_create_index::analyze(Ctx& ctx, Ctl& ctl)
{
    stmtArea().stmtInfo().setName(Stmt_name_create_index);
    // analyze the table
    ctx_assert(m_table != 0);
    m_table->analyze(ctx, ctl);
    if (! ctx.ok())
	return 0;
    // analyze the columns
    ctl.m_tableList.resize(1 + 1);	// indexed from 1
    ctl.m_tableList[1] = m_table;
    for (unsigned i = 1, n = countColumn(); i <= n; i++) {
	Plan_idx_column* column = getColumn(i);
	column->analyze(ctx, ctl);
	if (! ctx.ok())
	    return 0;
    }
    return this;
}

void
Plan_create_index::describe(Ctx& ctx)
{
    stmtArea().setFunction(ctx, "CREATE INDEX", SQL_DIAG_CREATE_INDEX);
}

Exec_base*
Plan_create_index::codegen(Ctx& ctx, Ctl& ctl)
{
    Exec_create_index* exec = new Exec_create_index(ctl.m_execRoot);
    ctl.m_execRoot->saveNode(exec);
    const unsigned count = countColumn();
    const char** attrList = new const char* [1 + count];
    attrList[0] = 0;	// unused
    for (unsigned i = 1; i <= count; i++) {
	Plan_idx_column* column = getColumn(i);
	const char* cname = column->getName().c_str();
	attrList[i] = strcpy(new char[strlen(cname) + 1], cname);
    }
    Exec_create_index::Code& code = *new Exec_create_index::Code(m_name, m_table->getName(), m_type, count, attrList);
    exec->setCode(code);
    code.m_fragmentType = m_fragmentType;
    code.m_logging = m_logging;
    return exec;
}

void
Plan_create_index::print(Ctx& ctx)
{
    ctx.print(" [create_index name=%s table=%s type=%d", m_name.c_str(), m_table->getName().c_str(), (int)m_type);
    ctx.print(" [");
    for (unsigned i = 1; i <= countColumn(); i++) {
	Plan_idx_column* column = getColumn(i);
	if (i > 1)
	    ctx.print(" ");
	column->print(ctx);
    }
    ctx.print("]");
}

// Exec_create_index

Exec_create_index::Code::~Code()
{
    for (unsigned i = 1; i <= m_attrCount; i++) {
	delete[] m_attrList[i];
	m_attrList[i] = 0;
    }
    delete[] m_attrList;
}

Exec_create_index::Data::~Data()
{
}

Exec_create_index::~Exec_create_index()
{
}

void
Exec_create_index::alloc(Ctx& ctx, Ctl& ctl)
{
    Data& data = *new Data;
    setData(data);
}

void
Exec_create_index::close(Ctx& ctx)
{
}

void
Exec_create_index::print(Ctx& ctx)
{
    const Code& code = getCode();
    ctx.print(" [create_index %s]", code.m_tableName.c_str());
}
