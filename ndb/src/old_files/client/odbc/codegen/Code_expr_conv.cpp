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
#include "Code_expr_conv.hpp"
#include "Code_root.hpp"

// Plan_expr_conv

Plan_expr_conv::~Plan_expr_conv()
{
}

Plan_base*
Plan_expr_conv::analyze(Ctx& ctx, Ctl& ctl)
{
    m_exec = 0;
    const SqlType& t1 = sqlType();
    ctx_assert(m_expr != 0);
    m_expr->analyze(ctx, ctl);
    if (! ctx.ok())
	return 0;
    const SqlType& t2 = m_expr->sqlType();
    if (t2.type() == SqlType::Unbound) {
	return m_expr;
    }
    if (t1.equal(t2)) {
	return m_expr;
    }
    // XXX move to runtime or make table-driven
    bool ok = false;
    if (t2.type() == SqlType::Null) {
	ok = true;
    } else if (t1.type() == SqlType::Char) {
	if (t2.type() == SqlType::Char) {
	    ok = true;
	} else if (t2.type() == SqlType::Varchar) {
	    ok = true;
	} else if (t2.type() == SqlType::Binary) {
	    ok = true;
	} else if (t2.type() == SqlType::Varbinary) {
	    ok = true;
	}
    } else if (t1.type() == SqlType::Varchar) {
	if (t2.type() == SqlType::Char) {
	    ok = true;
	} else if (t2.type() == SqlType::Varchar) {
	    ok = true;
	} else if (t2.type() == SqlType::Binary) {
	    ok = true;
	} else if (t2.type() == SqlType::Varbinary) {
	    ok = true;
	}
    } else if (t1.type() == SqlType::Binary) {
	if (t2.type() == SqlType::Char) {
	    ok = true;
	} else if (t2.type() == SqlType::Varchar) {
	    ok = true;
	} else if (t2.type() == SqlType::Binary) {
	    ok = true;
	} else if (t2.type() == SqlType::Varbinary) {
	    ok = true;
	}
    } else if (t1.type() == SqlType::Varbinary) {
	if (t2.type() == SqlType::Char) {
	    ok = true;
	} else if (t2.type() == SqlType::Varchar) {
	    ok = true;
	} else if (t2.type() == SqlType::Binary) {
	    ok = true;
	} else if (t2.type() == SqlType::Varbinary) {
	    ok = true;
	}
    } else if (t1.type() == SqlType::Smallint) {
	if (t2.type() == SqlType::Smallint) {
	    ok = true;
	} else if (t2.type() == SqlType::Integer) {
	    ok = true;
	} else if (t2.type() == SqlType::Bigint) {
	    ok = true;
	} else if (t2.type() == SqlType::Real) {
	    ok = true;
	} else if (t2.type() == SqlType::Double) {
	    ok = true;
	}
    } else if (t1.type() == SqlType::Integer) {
	if (t2.type() == SqlType::Smallint) {
	    ok = true;
	} else if (t2.type() == SqlType::Integer) {
	    ok = true;
	} else if (t2.type() == SqlType::Bigint) {
	    ok = true;
	} else if (t2.type() == SqlType::Real) {
	    ok = true;
	} else if (t2.type() == SqlType::Double) {
	    ok = true;
	}
    } else if (t1.type() == SqlType::Bigint) {
	if (t2.type() == SqlType::Smallint) {
	    ok = true;
	} else if (t2.type() == SqlType::Integer) {
	    ok = true;
	} else if (t2.type() == SqlType::Bigint) {
	    ok = true;
	} else if (t2.type() == SqlType::Real) {
	    ok = true;
	} else if (t2.type() == SqlType::Double) {
	    ok = true;
	}
    } else if (t1.type() == SqlType::Real) {
	if (t2.type() == SqlType::Smallint) {
	    ok = true;
	} else if (t2.type() == SqlType::Integer) {
	    ok = true;
	} else if (t2.type() == SqlType::Bigint) {
	    ok = true;
	} else if (t2.type() == SqlType::Real) {
	    ok = true;
	} else if (t2.type() == SqlType::Double) {
	    ok = true;
	}
    } else if (t1.type() == SqlType::Double) {
	if (t2.type() == SqlType::Smallint) {
	    ok = true;
	} else if (t2.type() == SqlType::Integer) {
	    ok = true;
	} else if (t2.type() == SqlType::Bigint) {
	    ok = true;
	} else if (t2.type() == SqlType::Real) {
	    ok = true;
	} else if (t2.type() == SqlType::Double) {
	    ok = true;
	}
    } else if (t1.type() == SqlType::Datetime) {
	if (t2.type() == SqlType::Datetime) {
	    ok = true;
	}
    }
    if (! ok) {
	char b1[40], b2[40];
	t1.print(b1, sizeof(b1));
	t2.print(b2, sizeof(b2));
	ctx.pushStatus(Error::Gen, "cannot convert %s to %s", b2, b1);
	return 0;
    }
    // depend on same tables
    const TableSet& ts = m_expr->tableSet();
    m_tableSet.insert(ts.begin(), ts.end());
    // set alias
    m_alias = m_expr->getAlias();
    return this;
}

Exec_base*
Plan_expr_conv::codegen(Ctx& ctx, Ctl& ctl)
{
    if (m_exec != 0)
	return m_exec;
    Exec_expr_conv* exec = new Exec_expr_conv(ctl.m_execRoot);
    ctl.m_execRoot->saveNode(exec);
    // create code for subexpression
    ctx_assert(m_expr != 0);
    Exec_expr* execExpr = static_cast<Exec_expr*>(m_expr->codegen(ctx, ctl));
    if (! ctx.ok())
	return 0;
    exec->setExpr(execExpr);
    // create the code
    SqlSpec sqlSpec(sqlType(), SqlSpec::Physical);
    Exec_expr_conv::Code& code = *new Exec_expr_conv::Code(sqlSpec);
    exec->setCode(code);
    m_exec = exec;
    return exec;
}

void
Plan_expr_conv::print(Ctx& ctx)
{
    ctx.print(" [expr_conv ");
    char buf[100];
    m_sqlType.print(buf, sizeof(buf));
    ctx.print("%s", buf);
    Plan_base* a[] = { m_expr };
    printList(ctx, a, 1);
    ctx.print("]");
}

bool
Plan_expr_conv::isEqual(const Plan_expr* expr) const
{
    ctx_assert(expr != 0);
    if (expr->type() != Plan_expr::TypeConv)
	return false;
    const Plan_expr_conv* expr2 = static_cast<const Plan_expr_conv*>(expr);
    if (! m_sqlType.equal(expr2->m_sqlType))
	return false;
    ctx_assert(m_expr != 0 && expr2->m_expr != 0);
    if (! m_expr->isEqual(expr2->m_expr))
	return false;
    return true;
}

bool
Plan_expr_conv::isGroupBy(const Plan_expr_row* row) const
{
    if (isAnyEqual(row))
	return true;
    ctx_assert(m_expr != 0);
    if (m_expr->isGroupBy(row))
	return true;
    return false;
}

// Code_expr_conv

Exec_expr_conv::Code::~Code()
{
}

Exec_expr_conv::Data::~Data()
{
}

Exec_expr_conv::~Exec_expr_conv()
{
}

void
Exec_expr_conv::alloc(Ctx& ctx, Ctl& ctl)
{
    if (m_data != 0)
	return;
    const Code& code = getCode();
    // allocate subexpression
    ctx_assert(m_expr != 0);
    m_expr->alloc(ctx, ctl);
    if (! ctx.ok())
	return;
    SqlField sqlField(code.m_sqlSpec);
    Data& data = *new Data(sqlField);
    setData(data);
}

void
Exec_expr_conv::close(Ctx& ctx)
{
    ctx_assert(m_expr != 0);
    m_expr->close(ctx);
    Data& data = getData();
    data.m_groupField.clear();
}

void
Exec_expr_conv::print(Ctx& ctx)
{
    const Code& code = getCode();
    ctx.print(" [expr_conv");
    Exec_base* a[] = { m_expr };
    printList(ctx, a, sizeof(a)/sizeof(a[0]));
    ctx.print("]");
}
