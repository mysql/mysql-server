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

#include <dictionary/DictColumn.hpp>
#include "Code_pred.hpp"
#include "Code_comp_op.hpp"
#include "Code_expr_conv.hpp"
#include "Code_expr_column.hpp"
#include "Code_table.hpp"
#include "Code_root.hpp"

// Comp_op

const char*
Comp_op::name() const
{
    switch (m_opcode) {
    case Eq:
	return "=";
    case Noteq:
	return "!=";
    case Lt:
	return "<";
    case Lteq:
	return "<=";
    case Gt:
	return ">";
    case Gteq:
	return ">=";
    case Like:
	return "like";
    case Notlike:
	return "not like";
    case Isnull:
	return "is null";
    case Isnotnull:
	return "is not null";
    }
    ctx_assert(false);
    return "";
}

unsigned
Comp_op::arity() const
{
    switch (m_opcode) {
    case Eq:
    case Noteq:
    case Lt:
    case Lteq:
    case Gt:
    case Gteq:
    case Like:
    case Notlike:
	return 2;
    case Isnull:
    case Isnotnull:
	return 1;
    }
    ctx_assert(false);
    return 0;
}

// Plan_comp_op

Plan_comp_op::~Plan_comp_op()
{
}

Plan_base*
Plan_comp_op::analyze(Ctx& ctx, Ctl& ctl)
{
    m_exec = 0;
    const unsigned arity = m_op.arity();
    // analyze operands
    for (unsigned i = 1; i <= arity; i++) {
	ctx_assert(m_expr[i] != 0);
	m_expr[i]->analyze(ctx, ctl);
	if (! ctx.ok())
	    return 0;
    }
    // for each operand, find type to convert to
    SqlType con[1 + 2];
    if (arity == 1) {
	const SqlType& t1 = m_expr[1]->sqlType();
	switch (t1.type()) {
	case SqlType::Char:
	case SqlType::Varchar:
	case SqlType::Smallint:
	case SqlType::Integer:
	case SqlType::Bigint:
	case SqlType::Real:
	case SqlType::Double:
	case SqlType::Datetime:
	case SqlType::Null:
	case SqlType::Unbound:
	    con[1] = t1;
	    break;
	default:
	    break;
	}
	if (con[1].type() == SqlType::Undef) {
	    char b1[40];
	    t1.print(b1, sizeof(b1));
	    ctx.pushStatus(Error::Gen, "type mismatch in comparison: %s %s", b1, m_op.name());
	    return 0;
	}
    } else if (arity == 2) {
	const SqlType& t1 = m_expr[1]->sqlType();
	const SqlType& t2 = m_expr[2]->sqlType();
	switch (t1.type()) {
	case SqlType::Char:
	    switch (t2.type()) {
	    case SqlType::Char:
	    case SqlType::Varchar:
	    case SqlType::Null:
		con[1] = t1;
		con[2] = t2;
		break;
	    case SqlType::Unbound:
		con[1] = con[2] = t2;
		break;
	    default:
		break;
	    }
	    break;
	case SqlType::Varchar:
	    switch (t2.type()) {
	    case SqlType::Char:
	    case SqlType::Varchar:
	    case SqlType::Null:
		con[1] = t1;
		con[2] = t2;
		break;
	    case SqlType::Unbound:
		con[1] = con[2] = t2;
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
		// conversion would mask primary key optimization
		con[1] = t1;
		con[2] = t2;
		break;
	    case SqlType::Real:
	    case SqlType::Double:
		con[1].setType(ctx, SqlType::Double);
		con[2] = con[1];
		break;
	    case SqlType::Null:
		con[1] = t1;
		con[2] = t2;
		break;
	    case SqlType::Unbound:
		con[1] = con[2] = t2;
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
		con[1].setType(ctx, SqlType::Double);
		con[2] = con[1];
		break;
	    case SqlType::Null:
		con[1] = t1;
		con[2] = t2;
		break;
	    case SqlType::Unbound:
		con[1] = con[2] = t2;
		break;
	    default:
		break;
	    }
	    break;
	case SqlType::Datetime:
	    switch (t2.type()) {
	    case SqlType::Datetime:
		con[1] = t1;
		con[2] = t2;
		break;
	    case SqlType::Unbound:
		con[1] = con[2] = t2;
		break;
	    default:
		break;
	    }
	    break;
	case SqlType::Null:
	    switch (t2.type()) {
	    case SqlType::Char:
	    case SqlType::Varchar:
	    case SqlType::Smallint:
	    case SqlType::Integer:
	    case SqlType::Bigint:
	    case SqlType::Real:
	    case SqlType::Double:
	    case SqlType::Datetime:
		con[1] = t1;
		con[2] = t2;
		break;
	    case SqlType::Unbound:
		con[1] = con[2] = t2;
		break;
	    default:
		break;
	    }
	    break;
	case SqlType::Unbound:
	    con[1] = con[2] = t1;
	    break;
	default:
	    break;
	}
	if (con[1].type() == SqlType::Undef || con[2].type() == SqlType::Undef) {
	    char b1[40], b2[40];
	    t1.print(b1, sizeof(b1));
	    t2.print(b2, sizeof(b2));
	    ctx.pushStatus(Error::Gen, "type mismatch in comparison: %s %s %s", b1, m_op.name(), b2);
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
	if (con[i].type() == SqlType::Unbound) {
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
    // look for column=expr
    if (ctl.m_topand && m_op.m_opcode == Comp_op::Eq) {
	ctx_assert(arity == 2);
	for (unsigned i = 1, j = 2; i <= 2; i++, j--) {
	    if (m_expr[i]->type() != Plan_expr::TypeColumn)
		continue;
	    Plan_expr_column* column = static_cast<Plan_expr_column*>(m_expr[i]);
	    if (! column->resolveEq(ctx, m_expr[j]))
		ctl.m_extra = true;
	}
    } else {
	ctl.m_extra = true;
    }
    // save top level comparison on list
    if (ctl.m_topand) {
	ctl.m_topcomp.push_back(this);
    }
    // table dependencies are union from operands
    m_tableSet.clear();
    for (unsigned i = 1; i <= arity; i++) {
	const TableSet& ts = m_expr[i]->tableSet();
	m_tableSet.insert(ts.begin(), ts.end());
    }
    // set of tables for which interpreter cannot be used
    m_noInterp.clear();
    // convenient
#undef ustype
#define ustype(b, n)	(((b) ? 1 : 0) * 100 + (n))
    if (arity == 1) {
	for (unsigned i = 1; i <= 1; i++) {
	    const SqlType t1 = m_expr[i]->sqlType();
	    switch (m_op.m_opcode) {
	    case Comp_op::Isnull:
	    case Comp_op::Isnotnull:
		if (m_expr[i]->type() == Plan_expr::TypeColumn) {
		    switch (ustype(t1.unSigned(), t1.type())) {
		    // all types accepted now
		    default:
			{
			    Plan_expr_column* column = static_cast<Plan_expr_column*>(m_expr[i]);
			    ctx_assert(column->m_resTable != 0);
			    m_interpColumn[i] = column;
			    continue;	// ok
			}
			break;
		    }
		}
		break;
	    default:
		break;
	    }
	    const TableSet& ts = m_expr[i]->tableSet();
	    m_noInterp.insert(ts.begin(), ts.end());
	}
    } else if (arity == 2) {
	for (unsigned i = 1, j = 2; i <= 2; i++, j--) {
	    const SqlType t1 = m_expr[i]->sqlType();
	    switch (m_op.m_opcode) {
	    case Comp_op::Like:
	    case Comp_op::Notlike:
		if (i == 2)	// col like val but not val like col
		    break;
		/*FALLTHRU*/
	    case Comp_op::Eq:
	    case Comp_op::Noteq:
	    case Comp_op::Lt:
	    case Comp_op::Lteq:
	    case Comp_op::Gt:
	    case Comp_op::Gteq:
		if (m_expr[i]->type() == Plan_expr::TypeColumn) {
		    switch (ustype(t1.unSigned(), t1.type())) {
		    case ustype(false, SqlType::Char):
		    case ustype(false, SqlType::Varchar):
		    case ustype(true, SqlType::Smallint):
		    case ustype(true, SqlType::Integer):
		    case ustype(true, SqlType::Bigint):
			{
			    Plan_expr_column* column = static_cast<Plan_expr_column*>(m_expr[i]);
			    ctx_assert(column->m_resTable != 0);
			    const TableSet& ts = m_expr[j]->tableSet();
			    if (ts.find(column->m_resTable) == ts.end()) {
				// candidate for column=const
				m_interpColumn[i] = column;
				continue;	// ok
			    }
			}
			break;
		    default:
			break;
		    }
		}
		break;
	    default:
		break;
	    }
	    const TableSet& ts = m_expr[i]->tableSet();
	    m_noInterp.insert(ts.begin(), ts.end());
	}
    } else {
	ctx_assert(false);
	return 0;
    }
#undef ustype
    return this;
}

Exec_base*
Plan_comp_op::codegen(Ctx& ctx, Ctl& ctl)
{
    if (m_exec != 0)
	return m_exec;
    const unsigned arity = m_op.arity();
    Exec_comp_op* exec = new Exec_comp_op(ctl.m_execRoot);
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
    Exec_comp_op::Code& code = *new Exec_comp_op::Code(m_op);
    // interpreted column=const
    if (! ctl.m_having) {
	ctx_assert(ctl.m_topTable != 0);
	for (unsigned i = 1; i <= arity; i++) {
	    Plan_expr_column* column = m_interpColumn[i];
	    if (column == 0)
		continue;
	    ctx_assert(column->m_resTable != 0);
	    if (column->m_resTable != ctl.m_topTable)
		continue;
	    ctx_assert(code.m_interpColumn == 0);
	    code.m_interpColumn = i;
	    code.m_interpAttrId = column->dictColumn().getAttrId();
	    ctx_log2(("can use interpreter on %s", column->getPrintName()));
	}
    }
    exec->setCode(code);
    m_exec = exec;
    return exec;
}

void
Plan_comp_op::print(Ctx& ctx)
{
    ctx.print(" [%s", m_op.name());
    Plan_base* a[] = { m_expr[1], m_expr[2] };
    printList(ctx, a, m_op.arity());
    ctx.print("]");
}

bool
Plan_comp_op::isGroupBy(const Plan_expr_row* row) const
{
    const unsigned arity = m_op.arity();
    for (unsigned i = 1; i <= arity; i++) {
	ctx_assert(m_expr[i] != 0);
	if (! m_expr[i]->isGroupBy(row))
	    return false;
    }
    return true;
}

// Code_comp_op

Exec_comp_op::Code::~Code()
{
}

Exec_comp_op::Data::~Data()
{
}

Exec_comp_op::~Exec_comp_op()
{
}

void
Exec_comp_op::alloc(Ctx& ctx, Ctl& ctl)
{
    const Code& code = getCode();
    // allocate subexpressions
    unsigned arity = code.m_op.arity();
    for (unsigned i = 1; i <= arity; i++) {
	ctx_assert(m_expr[i] != 0);
	m_expr[i]->alloc(ctx, ctl);
	if (! ctx.ok())
	    return;
    }
    Data& data = *new Data;
    setData(data);
}

void
Exec_comp_op::close(Ctx& ctx)
{
    const Code& code = getCode();
    unsigned arity = code.m_op.arity();
    for (unsigned i = 1; i <= arity; i++) {
	ctx_assert(m_expr[i] != 0);
	m_expr[i]->close(ctx);
    }
}

void
Exec_comp_op::print(Ctx& ctx)
{
    const Code& code = getCode();
    ctx.print(" [%s", code.m_op.name());
    Exec_base* a[] = { m_expr[1], m_expr[2] };
    printList(ctx, a, code.m_op.arity());
    ctx.print("]");
}
