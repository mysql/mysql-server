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
#include "Code_drop_index.hpp"
#include "Code_root.hpp"

// Plan_drop_index

Plan_drop_index::~Plan_drop_index()
{
}

Plan_base*
Plan_drop_index::analyze(Ctx& ctx, Ctl& ctl)
{
    stmtArea().stmtInfo().setName(Stmt_name_drop_index);
    return this;
}

void
Plan_drop_index::describe(Ctx& ctx)
{
    stmtArea().setFunction(ctx, "DROP INDEX", SQL_DIAG_DROP_INDEX);
}

Exec_base*
Plan_drop_index::codegen(Ctx& ctx, Ctl& ctl)
{
    Exec_drop_index* exec = new Exec_drop_index(ctl.m_execRoot);
    ctl.m_execRoot->saveNode(exec);
    Exec_drop_index::Code& code = *new Exec_drop_index::Code(m_name, m_tableName);
    exec->setCode(code);
    return exec;
}

void
Plan_drop_index::print(Ctx& ctx)
{
    ctx.print(" [drop_index %s]", m_name.c_str());
}

// Exec_drop_index

Exec_drop_index::Code::~Code()
{
}

Exec_drop_index::Data::~Data()
{
}

Exec_drop_index::~Exec_drop_index()
{
}

void
Exec_drop_index::alloc(Ctx& ctx, Ctl& ctl)
{
    Data& data = *new Data;
    setData(data);
}

void
Exec_drop_index::close(Ctx& ctx)
{
}

void
Exec_drop_index::print(Ctx& ctx)
{
    const Code& code = getCode();
    ctx.print(" [drop_index %s]", code.m_indexName.c_str());
}
