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
#include "Code_expr_func.hpp"
#include "Code_expr_conv.hpp"
#include "Code_root.hpp"
#include "PortDefs.h"


// Expr_func

static const struct { const char* alias; const char* name; }
expr_func_alias[] = {
    { "SUBSTRING",	"SUBSTR"	},
    { 0,		0		}
};

static const Expr_func
expr_func[] = {
    Expr_func(Expr_func::Substr,	"SUBSTR",	false	),
    Expr_func(Expr_func::Left,		"LEFT",		false	),
    Expr_func(Expr_func::Right,		"RIGHT",	false	),
    Expr_func(Expr_func::Count,		"COUNT",	true	),
    Expr_func(Expr_func::Max,		"MAX",		true	),
    Expr_func(Expr_func::Min,		"MIN",		true	),
    Expr_func(Expr_func::Sum,		"SUM",		true	),
    Expr_func(Expr_func::Avg,		"AVG",		true	),
    Expr_func(Expr_func::Rownum,	"ROWNUM",	false	),
    Expr_func(Expr_func::Sysdate,	"SYSDATE",	false	),
    Expr_func(Expr_func::Undef,		0,		false	)
};

const Expr_func&
Expr_func::find(const char* name)
{
    for (unsigned i = 0; expr_func_alias[i].alias != 0; i++) {
	if (strcasecmp(expr_func_alias[i].alias, name) == 0) {
	    name = expr_func_alias[i].name;
	    break;
	}
    }
    const Expr_func* p;
    for (p = expr_func; p->m_name != 0; p++) {
	if (strcasecmp(p->m_name, name) == 0)
	    break;
    }
    return *p;
}

// Plan_expr_func

Plan_expr_func::~Plan_expr_func()
{
    delete[] m_conv;
    m_conv = 0;
}

Plan_base*
Plan_expr_func::analyze(Ctx& ctx, Ctl& ctl)
{
    m_exec = 0;
    ctx_assert(m_narg == 0 || m_args != 0);
    // aggregate check
    if (m_func.m_aggr) {
	if (! ctl.m_aggrok) {
	    ctx.pushStatus(Error::Gen, "%s: invalid use of aggregate function", m_func.m_name);
	    return 0;
	}
	if (ctl.m_aggrin) {
	    // XXX actually possible with group-by but too hard
	    ctx.pushStatus(Error::Gen, "%s: nested aggregate function", m_func.m_name);
	    return 0;
	}
	ctl.m_aggrin = true;
	m_isAggr = true;
	m_isBound = true;
    }
    // analyze argument expressions
    if (m_func.m_code != Expr_func::Rownum)
	m_isBound = true;
    for (unsigned i = 1; i <= m_narg; i++) {
	Plan_expr* expr = m_args->getExpr(i);
	expr = static_cast<Plan_expr*>(expr->analyze(ctx, ctl));
	if (! ctx.ok())
	    return 0;
	ctx_assert(expr != 0);
	if (expr->m_isAggr)
	    m_isAggr = true;
	if (! m_func.m_aggr && ! expr->m_isBound)
	    m_isBound = false;
    }
    if (m_func.m_aggr)
	ctl.m_aggrin = false;
    // find result type and conversion types
    SqlType res;
    const Expr_func::Code fc = m_func.m_code;
    const char* const invalidArgCount = "%s: argument count %u is invalid";
    const char* const invalidArgType = "%s: argument %u has invalid type";
    if (fc == Expr_func::Substr || fc == Expr_func::Left || fc == Expr_func::Right) {
	if ((m_narg != (unsigned)2) && (m_narg != (unsigned)(fc == Expr_func::Substr ? 3 : 2))) {
	    ctx.pushStatus(Error::Gen, invalidArgCount, m_func.m_name, m_narg);
	    return 0;
	}
	const SqlType& t1 = m_args->getExpr(1)->sqlType();
	switch (t1.type()) {
	case SqlType::Char:
	    {
		// XXX convert to varchar for now to get length right
		SqlType tx(SqlType::Varchar, t1.length());
		res = m_conv[1] = tx;
	    }
	    break;
	case SqlType::Varchar:
	case SqlType::Unbound:
	    res = m_conv[1] = t1;
	    break;
	default:
	    ctx.pushStatus(Error::Gen, invalidArgType, m_func.m_name, 1);
	    return 0;
	}
	for (unsigned i = 2; i <= m_narg; i++) {
	    const SqlType& t2 = m_args->getExpr(i)->sqlType();
	    switch (t2.type()) {
	    case SqlType::Smallint:
	    case SqlType::Integer:
	    case SqlType::Bigint:
	    case SqlType::Unbound:
		m_conv[i] = t2;
		break;
	    default:
		ctx.pushStatus(Error::Gen, invalidArgType, m_func.m_name, i);
		return 0;
	    }
	}
    } else if (fc == Expr_func::Count) {
	ctx_assert(m_args != 0);
	if (m_args->getAsterisk()) {
	    ctx_assert(m_narg == 0);
	} else {
	    if (m_narg != 1) {
		ctx.pushStatus(Error::Gen, invalidArgCount, m_func.m_name, m_narg);
		return 0;
	    }
	    m_conv[1] = m_args->getExpr(1)->sqlType();
	}
	res.setType(ctx, SqlType::Bigint);
    } else if (fc == Expr_func::Min || fc == Expr_func::Max) {
	if (m_narg != 1) {
	    ctx.pushStatus(Error::Gen, invalidArgCount, m_func.m_name, m_narg);
	    return 0;
	}
	const SqlType& t1 = m_args->getExpr(1)->sqlType();
	res = m_conv[1] = t1;
    } else if (fc == Expr_func::Sum) {
	if (m_narg != 1) {
	    ctx.pushStatus(Error::Gen, invalidArgCount, m_func.m_name, m_narg);
	    return 0;
	}
	const SqlType& t1 = m_args->getExpr(1)->sqlType();
	switch (t1.type()) {
	case SqlType::Smallint:
	case SqlType::Integer:
	case SqlType::Bigint:
	    res.setType(ctx, SqlType::Bigint);
	    m_conv[1] = res;
	    break;
	case SqlType::Real:
	case SqlType::Double:
	    res.setType(ctx, SqlType::Double);
	    m_conv[1] = res;
	    break;
	case SqlType::Unbound:
	    res = m_conv[1] = t1;
	    break;
	default:
	    ctx.pushStatus(Error::Gen, invalidArgType, m_func.m_name, 1);
	    return 0;
	}
    } else if (fc == Expr_func::Avg) {
	if (m_narg != 1) {
	    ctx.pushStatus(Error::Gen, invalidArgCount, m_func.m_name, m_narg);
	    return 0;
	}
	const SqlType& t1 = m_args->getExpr(1)->sqlType();
	switch (t1.type()) {
	case SqlType::Smallint:
	case SqlType::Integer:
	case SqlType::Bigint:
	case SqlType::Real:
	case SqlType::Double:
	    res.setType(ctx, SqlType::Double);
	    m_conv[1] = res;
	    break;
	case SqlType::Unbound:
	    res = m_conv[1] = t1;
	    break;
	default:
	    ctx.pushStatus(Error::Gen, invalidArgType, m_func.m_name, 1);
	    return 0;
	}
    } else if (fc == Expr_func::Rownum) {
	ctx_assert(m_narg == 0 && m_args == 0);
	res.setType(ctx, SqlType::Bigint);
    } else if (fc == Expr_func::Sysdate) {
	ctx_assert(m_narg == 0 && m_args == 0);
	res.setType(ctx, SqlType::Datetime);
    } else {
	ctx_assert(false);
    }
    // insert required conversions
    for (unsigned i = 1; i <= m_narg; i++) {
	if (m_conv[i].type() == SqlType::Unbound) {
	    // parameter type not yet bound
	    continue;
	}
	Plan_expr_conv* exprConv = new Plan_expr_conv(m_root, m_conv[i]);
	m_root->saveNode(exprConv);
	exprConv->setExpr(m_args->getExpr(i));
	Plan_expr* expr = static_cast<Plan_expr*>(exprConv->analyze(ctx, ctl));
	if (! ctx.ok())
	    return 0;
	m_args->setExpr(i, expr);
    }
    // set result type
    m_sqlType = res;
    // set table dependencies
    for (unsigned i = 1; i <= m_narg; i++) {
	const TableSet& ts = m_args->getExpr(i)->tableSet();
	m_tableSet.insert(ts.begin(), ts.end());
    }
    // set alias name
    m_alias.assign(m_func.m_name);
    if (m_narg == 0) {
	if (fc == Expr_func::Count)
	    m_alias.append("(*)");
    } else {
	m_alias.append("(");
	for (unsigned i = 1; i <= m_narg; i++) {
	    if (i > 1)
		m_alias.append(",");
	    m_alias.append(m_args->getExpr(i)->getAlias());
	}
	m_alias.append(")");
    }
    return this;
}

Exec_base*
Plan_expr_func::codegen(Ctx& ctx, Ctl& ctl)
{
    if (m_exec != 0)
	return m_exec;
    Exec_expr_func* exec = new Exec_expr_func(ctl.m_execRoot);
    ctl.m_execRoot->saveNode(exec);
    SqlSpec sqlSpec(sqlType(), SqlSpec::Physical);
    Exec_expr_func::Code& code = *new Exec_expr_func::Code(m_func, sqlSpec);
    exec->setCode(code);
    code.m_narg = m_narg;
    code.m_args = new Exec_expr* [1 + m_narg];
    for (unsigned i = 0; i <= m_narg; i++)
	code.m_args[i] = 0;
    // create code for arguments
    for (unsigned i = 1; i <= m_narg; i++) {
	Plan_expr* expr = m_args->getExpr(i);
	ctx_assert(expr != 0);
	Exec_expr* execExpr = static_cast<Exec_expr*>(expr->codegen(ctx, ctl));
	if (! ctx.ok())
	    return 0;
	ctx_assert(execExpr != 0);
	code.m_args[i] = execExpr;
    }
    m_exec = exec;
    return exec;
}

void
Plan_expr_func::print(Ctx& ctx)
{
    ctx.print(" [%s", m_func.m_name);
    Plan_base* a[] = { m_args };
    printList(ctx, a, sizeof(a)/sizeof(a[1]));
    ctx.print("]");
}

bool
Plan_expr_func::isEqual(const Plan_expr* expr) const
{
    ctx_assert(expr != 0);
    if (expr->type() != Plan_expr::TypeFunc)
	return false;
    const Plan_expr_func* expr2 = static_cast<const Plan_expr_func*>(expr);
    if (m_func.m_code != expr2->m_func.m_code)
	return false;
    if (m_narg != expr2->m_narg)
	return false;
    ctx_assert(m_args != 0 && expr2->m_args != 0);
    for (unsigned i = 1; i <= m_narg; i++) {
	if (! m_args->getExpr(i)->isEqual(expr2->m_args->getExpr(i)))
	    return false;
    }
    return true;
}

bool
Plan_expr_func::isGroupBy(const Plan_expr_row* row) const
{
    if (m_func.m_aggr)
	return true;
    switch (m_func.m_code) {
    case Expr_func::Substr:
    case Expr_func::Left:
    case Expr_func::Right:
	ctx_assert(m_narg >= 1);
	if (m_args->getExpr(1)->isGroupBy(row))
	    return true;
	break;
    case Expr_func::Sysdate:
	return true;
    default:
	break;
    }
    if (isAnyEqual(row))
	return true;
    return false;
}

// Exec_expr_func

Exec_expr_func::Code::~Code()
{
    delete[] m_args;
    m_args = 0;
}

Exec_expr_func::Data::~Data()
{
}

Exec_expr_func::~Exec_expr_func()
{
}

void
Exec_expr_func::alloc(Ctx& ctx, Ctl& ctl)
{
    if (m_data != 0)
	return;
    const Code& code = getCode();
    // allocate arguments
    for (unsigned i = 1; i <= code.m_narg; i++) {
	ctx_assert(code.m_args != 0 && code.m_args[i] != 0);
	code.m_args[i]->alloc(ctx, ctl);
	if (! ctx.ok())
	    return;
    }
    SqlField sqlField(code.m_sqlSpec);
    Data& data = *new Data(sqlField);
    setData(data);
    ctx_assert(ctl.m_groupIndex == 0);
    init(ctx, ctl);
}

void
Exec_expr_func::close(Ctx& ctx)
{
    const Code& code = getCode();
    Data& data = getData();
    for (unsigned i = 1; i <= code.m_narg; i++) {
	ctx_assert(code.m_args != 0 && code.m_args[i] != 0);
	code.m_args[i]->close(ctx);
    }
    data.m_groupField.clear();
    Ctl ctl(0);
    init(ctx, ctl);
}

void
Exec_expr_func::print(Ctx& ctx)
{
    const Code& code = getCode();
    ctx.print(" [%s", code.m_func.m_name);
    for (unsigned i = 1; i <= code.m_narg; i++) {
	Exec_base* a[] = { code.m_args[i] };
	printList(ctx, a, sizeof(a)/sizeof(a[0]));
    }
    ctx.print("]");
}
