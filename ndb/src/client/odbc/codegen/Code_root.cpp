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

#include <common/StmtArea.hpp>
#include "Code_root.hpp"
#include "Code_stmt.hpp"
#include "Code_query.hpp"
#include "Code_expr_param.hpp"
#include "Code_root.hpp"

// Plan_root

Plan_root::~Plan_root()
{
}

Plan_base*
Plan_root::analyze(Ctx& ctx, Ctl& ctl)
{
    // analyze statement
    ctx_assert(m_stmt != 0);
    m_stmt = static_cast<Plan_stmt*>(m_stmt->analyze(ctx, ctl));
    if (! ctx.ok())
	return 0;
    ctx_assert(m_stmt != 0);
    // analyze parameters
    ctx_assert(m_paramList.size() > 0);
    const unsigned paramCount = m_paramList.size() - 1;
    DescArea& ipd = descArea(Desc_usage_IPD);
    ipd.setCount(ctx, paramCount);
    for (unsigned i = 1; i <= paramCount; i++) {
	Plan_expr_param* param = m_paramList[i];
	ctx_assert(param != 0);
	// analyze the parameter
	param->analyze(ctx, ctl);
	if (! ctx.ok())
	    return 0;
    }
    // must return self
    return this;
}

void
Plan_root::describe(Ctx& ctx)
{
    // describe statement
    ctx_assert(m_stmt != 0);
    m_stmt->describe(ctx);
    // describe parameters
    ctx_assert(m_paramList.size() > 0);
    const unsigned paramCount = m_paramList.size() - 1;
    DescArea& ipd = descArea(Desc_usage_IPD);
    ipd.setCount(ctx, paramCount);
    unsigned unbound = 0;
    for (unsigned i = 1; i <= paramCount; i++) {
	Plan_expr_param* param = m_paramList[i];
	ctx_assert(param != 0);
	// describe the parameter
	param->describe(ctx);
	// check if SQL type is bound
	ctx_assert(param->sqlType().type() != SqlType::Undef);
	if (param->sqlType().type() == SqlType::Unbound)
	    unbound++;
    }
    if (unbound > 0)
	ctx_log2(("%u out of %u params have unbound SQL type", unbound, paramCount));
    m_stmtArea.m_unbound = unbound;
}

Exec_base*
Plan_root::codegen(Ctx& ctx, Ctl& ctl)
{
    Exec_root* execRoot = new Exec_root(m_stmtArea);
    Exec_root::Code& code = *new Exec_root::Code;
    execRoot->setCode(code);
    // set root in helper struct
    ctl.m_execRoot = execRoot;
    // generate code for the statement
    ctx_assert(m_stmt != 0);
    Exec_stmt* execStmt = static_cast<Exec_stmt*>(m_stmt->codegen(ctx, ctl));
    if (! ctx.ok())
	return 0;
    execRoot->setStmt(execStmt);
    // create parameters list
    execRoot->m_paramList.resize(m_paramList.size());
    for (unsigned i = 1; i < m_paramList.size(); i++) {
	Plan_expr_param* param = m_paramList[i];
	ctx_assert(param != 0);
	Exec_expr_param* execParam = static_cast<Exec_expr_param*>(param->codegen(ctx, ctl));
	ctx_assert(execParam != 0);
	execRoot->m_paramList[i] = execParam;
    }
    return execRoot;
}

void
Plan_root::print(Ctx& ctx)
{
    ctx.print("[root");
    Plan_base* a[] = { m_stmt };
    printList(ctx, a, 1);
    ctx.print("]\n");
}

void
Plan_root::saveNode(Plan_base* node)
{
    ctx_assert(node != 0);
    m_nodeList.push_back(node);
}

void
Plan_root::freeNodeList()
{
    for (NodeList::iterator i = m_nodeList.begin(); i != m_nodeList.end(); i++) {
	Plan_base* node = *i;
	*i = 0;
	delete node;
    }
    m_nodeList.clear();
}

// Exec_root

Exec_root::Code::~Code()
{
}

Exec_root::Data::~Data()
{
}

Exec_root::~Exec_root()
{
}

StmtArea&
Exec_root::stmtArea() const
{
    return m_stmtArea;
}

void
Exec_root::alloc(Ctx& ctx, Ctl& ctl)
{
    ctx_assert(m_stmt != 0);
    m_stmt->alloc(ctx, ctl);
}

void
Exec_root::bind(Ctx& ctx)
{
    // bind output cols
    ctx_assert(m_stmt != 0);
    m_stmt->bind(ctx);
    // bind input params
    for (unsigned i = 1; i < m_paramList.size(); i++) {
	Exec_expr_param* param = m_paramList[i];
	ctx_assert(param != 0);
	param->bind(ctx);
	if (! ctx.ok())
	    return;
    }
}

void
Exec_root::execute(Ctx& ctx, Ctl& ctl)
{
    ctx_assert(m_stmt != 0);
    // check if data is needed
    for (unsigned i = 1; i < m_paramList.size(); i++) {
	Exec_expr_param* param = m_paramList[i];
	ctx_assert(param != 0);
	Exec_expr_param::Data& paramData = param->getData();
	if (paramData.m_atExec && paramData.m_extPos == -1) {
	    ctx.setCode(SQL_NEED_DATA);
	    return;
	}
    }
    m_stmt->execute(ctx, ctl);
}

void
Exec_root::fetch(Ctx& ctx, Ctl& ctl)
{
    ctx_assert(m_stmt != 0);
    Exec_query* query = static_cast<Exec_query*>(m_stmt);
    ctx_assert(query != 0);
    query->fetch(ctx, ctl);
}

void
Exec_root::close(Ctx& ctx)
{
    ctx_assert(m_stmt != 0);
    m_stmt->close(ctx);
    for (unsigned i = 1; i < m_paramList.size(); i++) {
	Exec_expr_param* param = m_paramList[i];
	ctx_assert(param != 0);
	param->close(ctx);
    }
}

void
Exec_root::print(Ctx& ctx)
{
    ctx.print("[root");
    Exec_base* a[] = { m_stmt };
    printList(ctx, a, sizeof(a)/sizeof(a[0]));
    ctx.print("]\n");
}

void
Exec_root::saveNode(Exec_base* node)
{
    ctx_assert(node != 0);
    m_nodeList.push_back(node);
}

void
Exec_root::freeNodeList()
{
    for (NodeList::iterator i = m_nodeList.begin(); i != m_nodeList.end(); i++) {
	Exec_base* node = *i;
	*i = 0;
	delete node;
    }
    m_nodeList.clear();
}

// odbc support

void
Exec_root::sqlGetData(Ctx& ctx, SQLUSMALLINT columnNumber, SQLSMALLINT targetType, SQLPOINTER targetValue, SQLINTEGER bufferLength, SQLINTEGER* strlen_or_Ind)
{
    ctx_assert(m_stmt != 0);
    Exec_query* query = static_cast<Exec_query*>(m_stmt);
    ctx_assert(query != 0);
    query->sqlGetData(ctx, columnNumber, targetType, targetValue, bufferLength, strlen_or_Ind);
}

void
Exec_root::sqlParamData(Ctx& ctx, SQLPOINTER* value)
{
    ctx_assert(m_paramList.size() > 0);
    unsigned count = m_paramList.size() - 1;
    for (unsigned i = 1; i <= count; i++) {
	Exec_expr_param* param = m_paramList[i];
	ctx_assert(param != 0);
	Exec_expr_param::Data& paramData = param->getData();
	if (! paramData.m_atExec || paramData.m_extPos >= 0)
	    continue;
	ctx_assert(paramData.m_extField != 0);
	ExtField& extField = *paramData.m_extField;
	if (value != 0)
	    *value = extField.m_dataPtr;
	m_paramData = i;
	ctx.setCode(SQL_NEED_DATA);
	return;
    }
}

void
Exec_root::sqlPutData(Ctx& ctx, SQLPOINTER data, SQLINTEGER strlen_or_Ind)
{
    ctx_assert(m_paramList.size() > 0);
    unsigned count = m_paramList.size() - 1;
    unsigned i = m_paramData;
    if (i == 0) {
	ctx.pushStatus(Sqlstate::_HY010, Error::Gen, "missing call to SQLParamData");
	return;
    }
    if (i > count) {
	ctx.pushStatus(Sqlstate::_HY010, Error::Gen, "parameter %u out of range 1 to %u", i, count);
	return;
    }
    Exec_expr_param* param = m_paramList[i];
    ctx_assert(param != 0);
    Exec_expr_param::Data& paramData = param->getData();
    if (! paramData.m_atExec) {
	ctx.pushStatus(Sqlstate::_HY010, Error::Gen, "parameter %u not marked for data-at-exec", i);
	return;
    }
    ctx_assert(paramData.m_extField != 0);
    ExtField extField(paramData.m_extField->extSpec(), data, 0, &strlen_or_Ind, i);
    if (paramData.m_extPos == -1)
	paramData.m_extPos = 0;
    extField.setPos(paramData.m_extPos);
    // copy in and update position
    SqlField& sqlField = paramData.m_sqlField;
    sqlField.copyin(ctx, extField);
    paramData.m_extPos = extField.getPos();
    ctx_log4(("parameter %u data received", i));
}
