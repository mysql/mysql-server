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

#include "Code_expr.hpp"
#include "Code_expr_op.hpp"
#include "Code_expr_conv.hpp"
#include "Code_root.hpp"

// Expr_op

const char*
Expr_op::name() const
{
    switch (m_opcode) {
    case Add:
	return "+";
    case Subtract:
	return "-";
    case Multiply:
	return "*";
    case Divide:
	return "/";
    case Plus:
	return "+";
    case Minus:
	return "-";
    }
    ctx_assert(false);
    return "";
}

unsigned
Expr_op::arity() const
{
    switch (m_opcode) {
    case Add:
    case Subtract:
    case Multiply:
    case Divide:
	return 2;
    case Plus:
    case Minus:
	return 1;
    }
    ctx_assert(false);
    return 0;
}

// Plan_expr_op

Plan_expr_op::~Plan_expr_op()
{
}

Plan_base*
Plan_expr_op::analyze(Ctx& ctx, Ctl& ctl)
{
    m_exec = 0;
    unsigned arity = m_op.arity();
    // analyze operands
    m_isAggr = false;
    m_isBound = true;
    for (unsigned i = 1; i <= arity; i++) {
	ctx_assert(m_expr[i] != 0);
	m_expr[i]->analyze(ctx, ctl);
	if (! ctx.ok())
	    return 0;
	if (m_expr[i]->m_isAggr)
	    m_isAggr = true;
	if (! m_expr[i]->m_isBound)
	    m_isBound = false;
    }
    // find result type and conversion types (currently same)
    SqlType res;
    SqlType con[1 + 2];
    if (arity == 1) {
	const SqlType& t1 = m_expr[1]->sqlType();
	switch (t1.type()) {
	case SqlType::Char:
	case SqlType::Varchar:
	    break;
	case SqlType::Smallint:
	case SqlType::Integer:
	case SqlType::Bigint:
	    res.setType(ctx, SqlType::Bigint);
	    con[1] = res;
	    break;
	case SqlType::Real:
	case SqlType::Double:
	    res.setType(ctx, SqlType::Double);
	    con[1] = res;
	    break;
	case SqlType::Null:
	    res.setType(ctx, SqlType::Null);
	    con[1] = res;
	    break;
	case SqlType::Unbound:
	    res.setType(ctx, SqlType::Unbound);
	    con[1] = res;
	default:
	    break;
	}
	if (con[1].type() == SqlType::Undef) {
	    char b1[40];
	    t1.print(b1, sizeof(b1));
	    ctx.pushStatus(Error::Gen, "type mismatch in operation: %s %s", m_op.name(), b1);
	    return 0;
	}
    } else if (arity == 2) {
	const SqlType& t1 = m_expr[1]->sqlType();
	const SqlType& t2 = m_expr[2]->sqlType();
	switch (t1.type()) {
	case SqlType::Char:		// handle char types as in oracle
	    switch (t2.type()) {
	    case SqlType::Char:
		res.setType(ctx, SqlType::Char, t1.length() + t2.length());
		con[1] = t1;
		con[2] = t2;
		break;
	    case SqlType::Varchar:
		res.setType(ctx, SqlType::Varchar, t1.length() + t2.length());
		con[1] = t1;
		con[2] = t2;
		break;
	    case SqlType::Null:
		res.setType(ctx, SqlType::Varchar, t1.length());
		con[1] = t1;
		con[2] = t2;
		break;
	    case SqlType::Unbound:
		res.setType(ctx, SqlType::Unbound);
		con[1] = con[2] = res;
		break;
	    default:
		break;
	    }
	    break;
	case SqlType::Varchar:
	    switch (t2.type()) {
	    case SqlType::Char:
		res.setType(ctx, SqlType::Varchar, t1.length() + t2.length());
		con[1] = t1;
		con[2] = t2;
		break;
	    case SqlType::Varchar:
		res.setType(ctx, SqlType::Varchar, t1.length() + t2.length());
		con[1] = t1;
		con[2] = t2;
		break;
	    case SqlType::Null:
		res.setType(ctx, SqlType::Varchar, t1.length());
		con[1] = t1;
		con[2] = t2;
		break;
	    case SqlType::Unbound:
		res.setType(ctx, SqlType::Unbound);
		con[1] = con[2] = res;
		break;
	    default:
		break;
	    }
	    break;
	case SqlType::Smallint:
	case SqlType::Integer:
	case SqlType::Bigint:
	    switch (t2.type()) {
	    case SqlType::Smallint:
	    case SqlType::Integer:
	    case SqlType::Bigint:
		res.setType(ctx, SqlType::Bigint);
		con[1] = con[2] = res;
		if (t1.unSigned() || t2.unSigned()) {
		    con[1].unSigned(true);
		    con[2].unSigned(true);
		}
		break;
	    case SqlType::Real:
	    case SqlType::Double:
		res.setType(ctx, SqlType::Double);
		con[1] = con[2] = res;
		break;
	    case SqlType::Null:
		res.setType(ctx, SqlType::Null);
		con[1] = con[2] = res;
		break;
	    case SqlType::Unbound:
		res.setType(ctx, SqlType::Unbound);
		con[1] = con[2] = res;
		break;
	    default:
		break;
	    }
	    break;
	case SqlType::Real:
	case SqlType::Double:
	    switch (t2.type()) {
	    case SqlType::Smallint:
	    case SqlType::Integer:
	    case SqlType::Bigint:
	    case SqlType::Real:
	    case SqlType::Double:
		res.setType(ctx, SqlType::Double);
		con[1] = con[2] = res;
		break;
	    case SqlType::Null:
		res.setType(ctx, SqlType::Null);
		con[1] = con[2] = res;
		break;
	    case SqlType::Unbound:
		res.setType(ctx, SqlType::Unbound);
		con[1] = con[2] = res;
		break;
	    default:
		break;
	    }
	    break;
	case SqlType::Null:
	    switch (t2.type()) {
	    case SqlType::Char:
	    case SqlType::Varchar:
		res.setType(ctx, SqlType::Varchar, t2.length());
		con[1] = con[2] = res;
		break;
	    case SqlType::Unbound:
		res.setType(ctx, SqlType::Unbound);
		con[1] = con[2] = res;
		break;
	    default:
		res.setType(ctx, SqlType::Null);
		con[1] = con[2] = res;
		break;
	    }
	    break;
	case SqlType::Unbound:
	    res.setType(ctx, SqlType::Unbound);
	    con[1] = con[2] = res;
	    break;
	default:
	    break;
	}
	if (con[1].type() == SqlType::Undef || con[2].type() == SqlType::Undef) {
	    char b1[40], b2[40];
	    t1.print(b1, sizeof(b1));
	    t2.print(b2, sizeof(b2));
	    ctx.pushStatus(Error::Gen, "type mismatch in operation: %s %s %s", b1, m_op.name(), b2);
	    return 0;
	}
    } else {
	ctx_assert(false);
	return 0;
    }
    if (! ctx.ok())
	return 0;
    // insert required conversions
    for (unsigned i = 1; i <= arity; i++) {
	if (con[i].type() == SqlType::Undef) {
	    ctx.pushStatus(Error::Gen, "mismatched types in operation");
	    return 0;
	}
	if (con[i].type() == SqlType::Unbound) {
	    // parameter type not yet bound
	    continue;
	}
	Plan_expr_conv* exprConv = new Plan_expr_conv(m_root, con[i]);
	m_root->saveNode(exprConv);
	exprConv->setExpr(m_expr[i]);
	m_expr[i] = static_cast<Plan_expr*>(exprConv->analyze(ctx, ctl));
	if (! ctx.ok())
	    return 0;
	ctx_assert(m_expr[i] != 0);
    }
    // set result type
    m_sqlType = res;
    // table dependencies are union from operands
    for (unsigned i = 1; i <= arity; i++) {
	const TableSet& ts = m_expr[i]->tableSet();
	m_tableSet.insert(ts.begin(), ts.end());
    }
    // set alias name  XXX misses operator precedence
    if (arity == 1) {
	m_alias.assign(m_op.name());
	m_alias.append(m_expr[1]->m_alias);
    } else if (arity == 2) {
	m_alias.assign(m_expr[1]->m_alias);
	m_alias.append(m_op.name());
	m_alias.append(m_expr[2]->m_alias);
    }
    return this;
}

Exec_base*
Plan_expr_op::codegen(Ctx& ctx, Ctl& ctl)
{
    if (m_exec != 0)
	return m_exec;
    unsigned arity = m_op.arity();
    Exec_expr_op* exec = new Exec_expr_op(ctl.m_execRoot);
    ctl.m_execRoot->saveNode(exec);
    // create code for operands
    for (unsigned i = 1; i <= arity; i++) {
	ctx_assert(m_expr[i] != 0);
	Exec_expr* execExpr = static_cast<Exec_expr*>(m_expr[i]->codegen(ctx, ctl));
	if (! ctx.ok())
	    return 0;
	ctx_assert(execExpr != 0);
	exec->setExpr(i, execExpr);
    }
    // create the code
    SqlSpec sqlSpec(sqlType(), SqlSpec::Physical);
    Exec_expr_op::Code& code = *new Exec_expr_op::Code(m_op, sqlSpec);
    exec->setCode(code);
    m_exec = exec;
    return exec;
}

void
Plan_expr_op::print(Ctx& ctx)
{
    ctx.print(" [%s", m_op.name());
    Plan_base* a[] = { m_expr[1], m_expr[2] };
    printList(ctx, a, m_op.arity());
    ctx.print("]");
}

bool
Plan_expr_op::isEqual(const Plan_expr* expr) const
{
    ctx_assert(expr != 0);
    if (expr->type() != Plan_expr::TypeOp)
	return false;
    const Plan_expr_op* expr2 = static_cast<const Plan_expr_op*>(expr);
    if (m_op.m_opcode != expr2->m_op.m_opcode)
	return false;
    const unsigned arity = m_op.arity();
    for (unsigned i = 1; i <= arity; i++) {
	ctx_assert(m_expr[i] != 0);
	if (! m_expr[i]->isEqual(expr2->m_expr[i]))
	    return false;
    }
    return true;
}

bool
Plan_expr_op::isGroupBy(const Plan_expr_row* row) const
{
    if (isAnyEqual(row))
	return true;
    const unsigned arity = m_op.arity();
    for (unsigned i = 1; i <= arity; i++) {
	ctx_assert(m_expr[i] != 0);
	if (! m_expr[i]->isGroupBy(row))
	    return false;
    }
    return true;
}

// Code_expr_op

Exec_expr_op::Code::~Code()
{
}

Exec_expr_op::Data::~Data()
{
}

Exec_expr_op::~Exec_expr_op()
{
}

void
Exec_expr_op::alloc(Ctx& ctx, Ctl& ctl)
{
    if (m_data != 0)
	return;
    const Code& code = getCode();
    // allocate subexpressions
    unsigned arity = code.m_op.arity();
    for (unsigned i = 1; i <= arity; i++) {
	ctx_assert(m_expr[i] != 0);
	m_expr[i]->alloc(ctx, ctl);
	if (! ctx.ok())
	    return;
    }
    SqlField sqlField(code.m_sqlSpec);
    Data& data = *new Data(sqlField);
    setData(data);
}

void
Exec_expr_op::close(Ctx& ctx)
{
    const Code& code = getCode();
    unsigned arity = code.m_op.arity();
    for (unsigned i = 1; i <= arity; i++) {
	ctx_assert(m_expr[i] != 0);
	m_expr[i]->close(ctx);
    }
    Data& data = getData();
    data.m_groupField.clear();
}

void
Exec_expr_op::print(Ctx& ctx)
{
    const Code& code = getCode();
    ctx.print(" [%s", code.m_op.name());
    Exec_base* a[] = { m_expr[1], m_expr[2] };
    printList(ctx, a, code.m_op.arity());
    ctx.print("]");
}
