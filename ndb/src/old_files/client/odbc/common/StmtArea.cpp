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

#include "DiagArea.hpp"
#include "StmtArea.hpp"
#include <codegen/CodeGen.hpp>

StmtArea::StmtArea(ConnArea& connArea) :
    m_connArea(connArea),
    m_state(Free),
    m_useSchemaCon(false),
    m_useConnection(false),
    m_planTree(0),
    m_execTree(0),
    m_unbound(0),
    m_rowCount(0),
    m_tuplesFetched(0)
{
    for (unsigned i = 0; i <= 4; i++)
	m_descArea[i] = 0;
}

StmtArea::~StmtArea()
{
}

void
StmtArea::free(Ctx &ctx)
{
    CodeGen codegen(*this);
    codegen.close(ctx);
    codegen.free(ctx);
    m_sqlText.assign("");
    m_nativeText.assign("");
    useSchemaCon(ctx, false);
    useConnection(ctx, false);
    m_stmtInfo.free(ctx);
    m_planTree = 0;
    m_execTree = 0;
    m_rowCount = 0;
    m_tuplesFetched = 0;
    m_unbound = 0;
    m_state = Free;
}

void
StmtArea::setRowCount(Ctx& ctx, CountType rowCount)
{
    m_rowCount = rowCount;
    // location
    DescArea& ird = descArea(Desc_usage_IRD);
    OdbcData data;
    ird.getHeader().getField(ctx, SQL_DESC_ROWS_PROCESSED_PTR, data);
    if (data.type() != OdbcData::Undef) {
	SQLUINTEGER* countPtr = data.uintegerPtr();
	if (countPtr != 0) {
	    *countPtr = static_cast<SQLUINTEGER>(m_rowCount);
	}
    }
    // diagnostic
    SQLINTEGER count = static_cast<SQLINTEGER>(m_rowCount);
    ctx.diagArea().setHeader(SQL_DIAG_ROW_COUNT, count);
}

void
StmtArea::setFunction(Ctx& ctx, const char* function, SQLINTEGER functionCode)
{
    m_stmtInfo.m_function = function;
    m_stmtInfo.m_functionCode = functionCode;
}

void
StmtArea::setFunction(Ctx& ctx)
{
    OdbcData function(m_stmtInfo.m_function);
    ctx.diagArea().setHeader(SQL_DIAG_DYNAMIC_FUNCTION, function);
    OdbcData functionCode(m_stmtInfo.m_functionCode);
    ctx.diagArea().setHeader(SQL_DIAG_DYNAMIC_FUNCTION_CODE, functionCode);
}

bool
StmtArea::useSchemaCon(Ctx& ctx, bool use)
{
    if (m_useSchemaCon != use)
	if (! m_connArea.useSchemaCon(ctx, use))
	    return false;
    m_useSchemaCon = use;
    return true;
}

bool
StmtArea::useConnection(Ctx& ctx, bool use)
{
    if (m_useConnection != use)
	if (! m_connArea.useConnection(ctx, use))
	    return false;
    m_useConnection = use;
    return true;
}
