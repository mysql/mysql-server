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
#include "Code_ddl_column.hpp"
#include "Code_expr_conv.hpp"
#include "Code_root.hpp"

// Plan_ddl_column

Plan_ddl_column::~Plan_ddl_column()
{
}

Plan_base*
Plan_ddl_column::analyze(Ctx& ctx, Ctl& ctl)
{
    ctx_assert(m_type != 0);
    if (! m_type->m_sqlType.nullable()) {
	m_nullable = false;
    }
    m_sqlType = m_type->m_sqlType;
    m_sqlType.nullable(m_nullable);
    const BaseString& name = getName();
    if (m_unSigned) {
	switch (m_sqlType.type()) {
	case SqlType::Smallint:
	case SqlType::Integer:
	case SqlType::Bigint:
	    break;
	default:
	    ctx.pushStatus(Error::Gen, "invalid unsigned qualifier on column %s", name.c_str());
	    return 0;
	}
	m_sqlType.unSigned(true);
    }
    if (strcmp(name.c_str(), "NDB$TID") == 0) {
	if (! m_primaryKey) {
	    ctx.pushStatus(Error::Gen, "column %s must be a primary key", name.c_str());
	    return 0;
	}
	if (sqlType().type() != SqlType::Bigint || ! sqlType().unSigned()) {
	    ctx.pushStatus(Error::Gen, "tuple id %s must have type BIGINT UNSIGNED", name.c_str());
	    return 0;
	}
	setTupleId();
    }
    if (m_autoIncrement) {
	if (! m_primaryKey) {
	    ctx.pushStatus(Error::Gen, "auto-increment column %s must be a primary key", name.c_str());
	    return 0;
	}
	if (sqlType().type() != SqlType::Smallint && sqlType().type() != SqlType::Integer && sqlType().type() != SqlType::Bigint) {
	    ctx.pushStatus(Error::Gen, "auto-increment column %s must have an integral type", name.c_str());
	    return 0;
	}
    }
    if (m_defaultValue != 0) {
	if (m_primaryKey) {
	    ctx.pushStatus(Sqlstate::_42000, Error::Gen, "default value not allowed on primary key column %s", name.c_str());
	    return 0;
	}
	m_defaultValue->analyze(ctx, ctl);
	if (! ctx.ok())
	    return 0;
	// insert conversion node
	Plan_expr_conv* exprConv = new Plan_expr_conv(m_root, sqlType());
	m_root->saveNode(exprConv);
	exprConv->setExpr(m_defaultValue);
	Plan_expr* expr = static_cast<Plan_expr*>(exprConv->analyze(ctx, ctl));
	if (! ctx.ok())
	    return 0;
	ctx_assert(expr != 0);
	m_defaultValue = expr;
    }
    return this;
}

Exec_base*
Plan_ddl_column::codegen(Ctx& ctx, Ctl& ctl)
{
    ctx_assert(false);
    return 0;
}

void
Plan_ddl_column::print(Ctx& ctx)
{
    ctx.print(" [ddl_column %s key=%d id=%d]", getPrintName(), m_primaryKey, m_tupleId);
}
