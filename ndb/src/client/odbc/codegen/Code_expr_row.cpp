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

#include <common/DataType.hpp>
#include "Code_expr_row.hpp"
#include "Code_expr.hpp"
#include "Code_expr_conv.hpp"
#include "Code_dml_row.hpp"
#include "Code_root.hpp"

// Plan_expr_row

Plan_expr_row::~Plan_expr_row()
{
}

Plan_base*
Plan_expr_row::analyze(Ctx& ctx, Ctl& ctl)
{
    unsigned size = getSize();
    // analyze subexpressions
    m_anyAggr = false;
    m_allBound = true;
    for (unsigned i = 1; i <= size; i++) {
	Plan_expr* expr1 = getExpr(i);
	Plan_expr* expr2 = static_cast<Plan_expr*>(expr1->analyze(ctx, ctl));
	if (! ctx.ok())
	    return 0;
	setExpr(i, expr2);
	if (expr2->isAggr())
	    m_anyAggr = true;
	if (! expr2->isBound())
	    m_allBound = false;
    }
    // insert conversions if requested  XXX ugly hack
    if (ctl.m_dmlRow != 0) {
	if (ctl.m_dmlRow->getSize() > getSize()) {
	    ctx.pushStatus(Sqlstate::_21S01, Error::Gen, "not enough values (%u > %u)", ctl.m_dmlRow->getSize(), getSize());
	    return 0;
	}
	if (ctl.m_dmlRow->getSize() < getSize()) {
	    ctx.pushStatus(Sqlstate::_21S01, Error::Gen, "too many values (%u < %u)", ctl.m_dmlRow->getSize(), getSize());
	    return 0;
	}
	for (unsigned i = 1; i <= size; i++) {
	    const SqlType& sqlType = ctl.m_dmlRow->getColumn(i)->sqlType();
	    Plan_expr_conv* exprConv = new Plan_expr_conv(m_root, sqlType);
	    m_root->saveNode(exprConv);
	    exprConv->setExpr(getExpr(i));
	    Plan_expr* expr = static_cast<Plan_expr*>(exprConv->analyze(ctx, ctl));
	    if (! ctx.ok())
		return 0;
	    ctx_assert(expr != 0);
	    setExpr(i, expr);
	}
    }
    // set aliases
    m_aliasList.resize(1 + size);
    for (unsigned i = 1; i <= size; i++) {
	if (m_aliasList[i].empty()) {
	    setAlias(i, getExpr(i)->getAlias());
	}
    }
    // node was not replaced
    return this;
}

Exec_base*
Plan_expr_row::codegen(Ctx& ctx, Ctl& ctl)
{
    unsigned size = getSize();
    Exec_expr_row* exec = new Exec_expr_row(ctl.m_execRoot, size);
    ctl.m_execRoot->saveNode(exec);
    SqlSpecs sqlSpecs(size);
    // create code for subexpressions
    for (unsigned i = 1; i <= size; i++) {
	Plan_expr* planExpr = getExpr(i);
	Exec_expr* execExpr = static_cast<Exec_expr*>(planExpr->codegen(ctx, ctl));
	if (! ctx.ok())
	    return 0;
	ctx_assert(execExpr != 0);
	exec->setExpr(i, execExpr);
	const SqlSpec sqlSpec(execExpr->getCode().sqlSpec(), SqlSpec::Reference);
	sqlSpecs.setEntry(i, sqlSpec);
    }
    // create alias list
    Exec_expr_row::Code::Alias* aliasList = new Exec_expr_row::Code::Alias[1 + size];
    strcpy(aliasList[0], "?");
    for (unsigned i = 1; i <= size; i++) {
	const char* s = m_aliasList[i].c_str();
	if (strlen(s) == 0)
	    s = getExpr(i)->getAlias().c_str();
	unsigned n = strlen(s);
	if (n >= sizeof(Exec_expr_row::Code::Alias))
	    n = sizeof(Exec_expr_row::Code::Alias) - 1;
	strncpy(aliasList[i], s, n);
	aliasList[i][n] = 0;
    }
    // create the code
    Exec_expr_row::Code& code = *new Exec_expr_row::Code(sqlSpecs, aliasList);
    exec->setCode(code);
    return exec;
}

void
Plan_expr_row::print(Ctx& ctx)
{
    const unsigned size = getSize();
    ctx.print(" [expr_row");
    for (unsigned i = 1; i <= size; i++) {
	Plan_base* a = m_exprList[i];
	a == 0 ?  ctx.print(" -") : a->print(ctx);
    }
    ctx.print("]");
}

bool
Plan_expr_row::isAllGroupBy(const Plan_expr_row* row) const
{
    const unsigned size = getSize();
    for (unsigned i = 1; i <= size; i++) {
	if (! getExpr(i)->isGroupBy(row))
	    return false;
    }
    return true;
}

// Exec_expr_row

Exec_expr_row::Code::~Code()
{
    delete[] m_aliasList;
}

Exec_expr_row::Data::~Data()
{
}

Exec_expr_row::~Exec_expr_row()
{
    delete[] m_expr;
}

void
Exec_expr_row::alloc(Ctx& ctx, Ctl& ctl)
{
    // allocate subexpressions
    for (unsigned i = 1; i <= m_size; i++) {
	getExpr(i)->alloc(ctx, ctl);
    }
    // construct SqlRow of references
    const Code& code = getCode();
    SqlRow sqlRow(getCode().m_sqlSpecs);
    for (unsigned i = 1; i <= m_size; i++) {
	const Exec_expr::Data& dataExpr = getExpr(i)->getData();
	const SqlSpec& sqlSpec = code.m_sqlSpecs.getEntry(i);
	const SqlField sqlField(sqlSpec, &dataExpr.sqlField());
	sqlRow.setEntry(i, sqlField);
    }
    // create the data
    Data& data = *new Data(sqlRow);
    setData(data);
}

void
Exec_expr_row::evaluate(Ctx& ctx, Ctl& ctl)
{
    for (unsigned i = 1; i <= m_size; i++) {
	getExpr(i)->evaluate(ctx, ctl);
	if (! ctx.ok())
	    return;
    }
}

void
Exec_expr_row::close(Ctx& ctx)
{
    for (unsigned i = 1; i <= m_size; i++) {
	getExpr(i)->close(ctx);
    }
}

void
Exec_expr_row::print(Ctx& ctx)
{
    ctx.print(" [expr_row");
    for (unsigned i = 1; i <= m_size; i++) {
	getExpr(i)->print(ctx);
    }
    ctx.print("]");
}
