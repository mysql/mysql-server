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

#include <codegen/Code_expr_op.hpp>

void
Exec_expr_op::evaluate(Ctx& ctx, Ctl& ctl)
{
    const Code& code = getCode();
    Data& data = getData();
    const SqlType& t = code.sqlSpec().sqlType();
    SqlField& f = ctl.m_groupIndex == 0 ? data.m_sqlField : data.groupField(code.sqlSpec().sqlType(), ctl.m_groupIndex, ctl.m_groupInit);
    if (code.m_op.arity() == 1) {
	// evaluate sub-expression
	ctx_assert(m_expr[1] != 0);
	m_expr[1]->evaluate(ctx, ctl);
	if (! ctx.ok())
	    return;
	if (ctl.m_postEval)
	    return;
	const SqlField& f1 = ctl.m_groupIndex == 0 ? m_expr[1]->getData().sqlField() : m_expr[1]->getData().groupField(ctl.m_groupIndex);
	// handle null
	if (f1.sqlNull()) {
	    f.sqlNull(true);
	    return;
	}
	if (t.type() == SqlType::Bigint) {
	    SqlBigint v = 0;
	    SqlBigint v1 = f1.sqlBigint();
	    switch (code.m_op.m_opcode) {
	    case Expr_op::Plus:
		v = v1;
		break;
	    case Expr_op::Minus:
		v = - v1;
		break;
	    default:
		ctx_assert(false);
		break;
	    }
	    f.sqlBigint(v);
	} else if (t.type() == SqlType::Double) {
	    SqlDouble v = 0;
	    SqlDouble v1 = f1.sqlDouble();
	    switch (code.m_op.m_opcode) {
	    case Expr_op::Plus:
		v = v1;
		break;
	    case Expr_op::Minus:
		v = - v1;
		break;
	    default:
		ctx_assert(false);
		break;
	    }
	    f.sqlDouble(v);
	} else {
	    ctx_assert(false);
	}
    } else if (code.m_op.arity() == 2) {
	// evaluate sub-expressions
	ctx_assert(m_expr[1] != 0 && m_expr[2] != 0);
	m_expr[1]->evaluate(ctx, ctl);
	if (! ctx.ok())
	    return;
	m_expr[2]->evaluate(ctx, ctl);
	if (! ctx.ok())
	    return;
	if (ctl.m_postEval)
	    return;
	const SqlField& f1 = ctl.m_groupIndex == 0 ? m_expr[1]->getData().sqlField() : m_expr[1]->getData().groupField(ctl.m_groupIndex);
	const SqlField& f2 = ctl.m_groupIndex == 0 ? m_expr[2]->getData().sqlField() : m_expr[2]->getData().groupField(ctl.m_groupIndex);
	// handle null
	if (f1.sqlNull() || f2.sqlNull()) {
	    f.sqlNull(true);
	    return;
	}
	if (t.type() == SqlType::Bigint) {
	    SqlBigint v = 0;
	    SqlBigint v1 = f1.sqlBigint();
	    SqlBigint v2 = f2.sqlBigint();
	    switch (code.m_op.m_opcode) {
	    case Expr_op::Add:
		v = v1 + v2;
		break;
	    case Expr_op::Subtract:
		v = v1 - v2;
		break;
	    case Expr_op::Multiply:
		v = v1 * v2;
		break;
	    case Expr_op::Divide:
		if (v2 == 0) {
		    ctx.pushStatus(Sqlstate::_22012, Error::Gen, "integer division by zero");
		    return;
		}
		v = v1 / v2;
		break;
	    default:
		ctx_assert(false);
		break;
	    }
	    f.sqlBigint(v);
	} else if (t.type() == SqlType::Double) {
	    SqlDouble v = 0;
	    SqlDouble v1 = f1.sqlDouble();
	    SqlDouble v2 = f2.sqlDouble();
	    switch (code.m_op.m_opcode) {
	    case Expr_op::Add:
		v = v1 + v2;
		break;
	    case Expr_op::Subtract:
		v = v1 - v2;
		break;
	    case Expr_op::Multiply:
		v = v1 * v2;
		break;
	    case Expr_op::Divide:
		v = v1 / v2;
		break;
	    default:
		ctx_assert(false);
		break;
	    }
	    f.sqlDouble(v);		// XXX isnan()
	} else {
	    ctx_assert(false);
	}
    } else {
	ctx_assert(false);
    }
    // result is not null
    f.sqlNull(false);
}
