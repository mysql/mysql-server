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

#include <NdbApi.hpp>
#include <common/StmtArea.hpp>
#include <dictionary/DictSchema.hpp>
#include <dictionary/DictColumn.hpp>
#include "Code_query.hpp"
#include "Code_table.hpp"
#include "Code_expr_column.hpp"
#include "Code_root.hpp"

// Plan_expr_column

Plan_expr_column::~Plan_expr_column()
{
}

Plan_base*
Plan_expr_column::analyze(Ctx& ctx, Ctl& ctl)
{
    m_exec = 0;
    analyzeColumn(ctx, ctl);
    if (! ctx.ok())
	return 0;
    Plan_expr::m_sqlType = Plan_column::m_sqlType;
    // depends on one table
    m_tableSet.insert(m_resTable);
    // not constant as set-value
    ctl.m_const = false;
    // set alias name
    m_alias = m_name;
    return this;
}

Exec_base*
Plan_expr_column::codegen(Ctx& ctx, Ctl& ctl)
{
    if (m_exec != 0)
	return m_exec;
    // connect column to query column
    const Exec_query* execQuery = ctl.m_execQuery;
    ctx_assert(execQuery != 0);
    const Exec_query::Code& codeQuery = execQuery->getCode();
    const SqlSpec sqlSpec(Plan_expr::m_sqlType, SqlSpec::Reference);
    // offset in final output row
    ctx_assert(m_resTable != 0 && m_resTable->m_resOff != 0 && m_resPos != 0);
    unsigned resOff = m_resTable->m_resOff + (m_resPos - 1);
    // create the code
    Exec_expr_column* exec = new Exec_expr_column(ctl.m_execRoot);
    ctl.m_execRoot->saveNode(exec);
    Exec_expr_column::Code& code = *new Exec_expr_column::Code(sqlSpec, resOff);
    exec->setCode(code);
    m_exec = exec;
    return exec;
}

bool
Plan_expr_column::resolveEq(Ctx& ctx, Plan_expr* expr)
{
    ctx_assert(m_resTable != 0 && expr != 0);
    return m_resTable->resolveEq(ctx, this, expr);
}

void
Plan_expr_column::print(Ctx& ctx)
{
    ctx.print(" [expr_column %s]", getPrintName());
}

bool
Plan_expr_column::isEqual(const Plan_expr* expr) const
{
    ctx_assert(expr != 0);
    if (expr->type() != Plan_expr::TypeColumn)
	return false;
    const Plan_expr_column* expr2 = static_cast<const Plan_expr_column*>(expr);
    ctx_assert(m_resTable != 0 && expr2->m_resTable != 0);
    if (m_resTable != expr2->m_resTable)
	return false;
    ctx_assert(m_dictColumn != 0 && expr2->m_dictColumn != 0);
    if (m_dictColumn != expr2->m_dictColumn)
	return false;
    return true;
}

bool
Plan_expr_column::isGroupBy(const Plan_expr_row* row) const
{
    if (isAnyEqual(row))
	return true;
    return false;
}

// Exec_expr_column

Exec_expr_column::Code::~Code()
{
}

Exec_expr_column::Data::~Data()
{
}

Exec_expr_column::~Exec_expr_column()
{
}

void
Exec_expr_column::alloc(Ctx& ctx, Ctl& ctl)
{
    if (m_data != 0)
	return;
    const Code& code = getCode();
    // connect column to query column
    ctx_assert(ctl.m_query != 0);
    const SqlRow& sqlRow = ctl.m_query->getData().sqlRow();
    SqlField& sqlField = sqlRow.getEntry(code.m_resOff);
    // create the data
    Data& data = *new Data(sqlField);
    setData(data);
}

void
Exec_expr_column::evaluate(Ctx& ctx, Ctl& ctl)
{
    const Code& code = getCode();
    Data& data = getData();
    if (ctl.m_groupIndex != 0) {
	SqlField& out = data.groupField(code.sqlSpec().sqlType(), ctl.m_groupIndex, ctl.m_groupInit);
	data.sqlField().copy(ctx, out);
    }
}

void
Exec_expr_column::close(Ctx& ctx)
{
    Data& data = getData();
    data.m_groupField.clear();
}

void
Exec_expr_column::print(Ctx& ctx)
{
    const Code& code = getCode();
    ctx.print(" [column %d]", code.m_resOff);
}
