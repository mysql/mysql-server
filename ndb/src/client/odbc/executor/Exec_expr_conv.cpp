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

#include <codegen/Code_expr_conv.hpp>

void
Exec_expr_conv::evaluate(Ctx& ctx, Ctl& ctl)
{
    const Code& code = getCode();
    Data& data = getData();
    const SqlType& t1 = code.sqlSpec().sqlType();
    SqlField& f1 = ctl.m_groupIndex == 0 ? data.m_sqlField : data.groupField(code.sqlSpec().sqlType() , ctl.m_groupIndex, ctl.m_groupInit);
    // evaluate the subexpression
    ctx_assert(m_expr != 0);
    m_expr->evaluate(ctx, ctl);
    if (! ctx.ok())
	return;
    if (ctl.m_postEval)
	return;
    const SqlType& t2 = m_expr->getCode().sqlSpec().sqlType();
    const SqlField& f2 = ctl.m_groupIndex == 0 ? m_expr->getData().sqlField() : m_expr->getData().groupField(ctl.m_groupIndex);
    // conversion to null type
    if (t1.type() == SqlType::Null) {
	f1.sqlNull(true);
	return;
    }
    // conversion of null data
    if (f2.sqlNull()) {
	f1.sqlNull(true);
	return;
    }
    // try to convert
    if (! f2.cast(ctx, f1)) {
	char b1[40], b2[40], b3[40];
	t1.print(b1, sizeof(b1));
	t2.print(b2, sizeof(b2));
	f2.print(b3, sizeof(b3));
	ctx.pushStatus(Sqlstate::_22003, Error::Gen, "cannot convert %s [%s] to %s", b2, b3, b1);
	return;
    }
}
