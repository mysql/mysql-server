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

#include "Code_expr_const.hpp"
#include "Code_root.hpp"

// Plan_expr_const

Plan_expr_const::~Plan_expr_const()
{
}

Plan_base*
Plan_expr_const::analyze(Ctx& ctx, Ctl& ctl)
{
    m_exec = 0;
    // convert data type
    m_lexType.convert(ctx, m_sqlType, m_string.length());
    if (! ctx.ok())
	return 0;
    // depends on no tables
    // set alias name
    m_alias = m_string;
    // node was not changed
    return this;
}

Exec_base*
Plan_expr_const::codegen(Ctx& ctx, Ctl& ctl)
{
    if (m_exec != 0)
	return m_exec;
    // convert data
    SqlSpec sqlSpec(m_sqlType, SqlSpec::Physical);
    SqlField sqlField(sqlSpec);
    LexSpec lexSpec(m_lexType);
    lexSpec.convert(ctx, m_string, sqlField);
    if (! ctx.ok())
	return 0;
    // create code
    Exec_expr_const* exec = new Exec_expr_const(ctl.m_execRoot);
    ctl.m_execRoot->saveNode(exec);
    Exec_expr_const::Code& code = *new Exec_expr_const::Code(sqlField);
    exec->setCode(code);
    m_exec = exec;
    return exec;
}

void
Plan_expr_const::print(Ctx& ctx)
{
    ctx.print(" [const %s]", m_string.c_str());
}

bool
Plan_expr_const::isEqual(const Plan_expr* expr) const
{
    ctx_assert(expr != 0);
    if (expr->type() != Plan_expr::TypeConst)
	return false;
    const Plan_expr_const* expr2 = static_cast<const Plan_expr_const*>(expr);
    if (strcmp(m_string.c_str(), expr2->m_string.c_str()) != 0)
	return false;
    return true;
}

bool
Plan_expr_const::isGroupBy(const Plan_expr_row* row) const
{
    return true;
}

// Exec_expr_const

Exec_expr_const::Code::~Code()
{
}

Exec_expr_const::Data::~Data()
{
}

Exec_expr_const::~Exec_expr_const()
{
}

void
Exec_expr_const::alloc(Ctx& ctx, Ctl& ctl)
{
    if (m_data != 0)
	return;
    // copy the value for const correctness reasons
    SqlField sqlField(getCode().m_sqlField);
    Data& data = *new Data(sqlField);
    setData(data);
}

void
Exec_expr_const::evaluate(Ctx& ctx, Ctl& ctl)
{
    const Code& code = getCode();
    Data& data = getData();
    if (ctl.m_groupIndex != 0) {
	SqlField& out = data.groupField(code.sqlSpec().sqlType(), ctl.m_groupIndex, ctl.m_groupInit);
	data.sqlField().copy(ctx, out);
    }
}

void
Exec_expr_const::close(Ctx& ctx)
{
    Data& data = getData();
    data.m_groupField.clear();
}

void
Exec_expr_const::print(Ctx& ctx)
{
    const Code& code = getCode();
    ctx.print(" [");
    char buf[500];
    code.m_sqlField.print(buf, sizeof(buf));
    ctx.print("%s", buf);
    ctx.print("]");
}
