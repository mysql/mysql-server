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
#include "Code_drop_table.hpp"
#include "Code_root.hpp"

// Plan_drop_table

Plan_drop_table::~Plan_drop_table()
{
}

Plan_base*
Plan_drop_table::analyze(Ctx& ctx, Ctl& ctl)
{
    stmtArea().stmtInfo().setName(Stmt_name_drop_table);
    return this;
}

void
Plan_drop_table::describe(Ctx& ctx)
{
    stmtArea().setFunction(ctx, "DROP TABLE", SQL_DIAG_DROP_TABLE);
}

Exec_base*
Plan_drop_table::codegen(Ctx& ctx, Ctl& ctl)
{
    Exec_drop_table* exec = new Exec_drop_table(ctl.m_execRoot);
    ctl.m_execRoot->saveNode(exec);
    Exec_drop_table::Code& code = *new Exec_drop_table::Code(m_name);
    exec->setCode(code);
    return exec;
}

void
Plan_drop_table::print(Ctx& ctx)
{
    ctx.print(" [drop_table %s]", m_name.c_str());
}

// Exec_drop_table

Exec_drop_table::Code::~Code()
{
}

Exec_drop_table::Data::~Data()
{
}

Exec_drop_table::~Exec_drop_table()
{
}

void
Exec_drop_table::alloc(Ctx& ctx, Ctl& ctl)
{
    Data& data = *new Data;
    setData(data);
}

void
Exec_drop_table::close(Ctx& ctx)
{
}

void
Exec_drop_table::print(Ctx& ctx)
{
    const Code& code = getCode();
    ctx.print(" [drop_table %s]", code.m_tableName.c_str());
}
