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
#include "Code_query.hpp"
#include "Code_query_project.hpp"
#include "Code_query_count.hpp"

// Plan_query

Plan_query::~Plan_query()
{
}

Plan_expr_row*
Plan_query::getRow()
{
    ctx_assert(false);
    return 0;
}

void
Plan_query::describe(Ctx& ctx)
{
    const Plan_expr_row* exprRow = getRow();
    const unsigned count = exprRow->getSize();
    // create IRD
    DescArea& ird = descArea(Desc_usage_IRD);
    ird.setCount(ctx, count);
    for (unsigned i = 1; i <= count; i++) {
	DescRec& rec = ird.getRecord(i);
	const Plan_expr* expr = exprRow->m_exprList[i];
	const SqlType& sqlType = expr->sqlType();
	// data type
	SQLSMALLINT desc_TYPE = sqlType.type();
	rec.setField(SQL_DESC_TYPE, desc_TYPE);
	SQLSMALLINT desc_CONCISE_TYPE = desc_TYPE;
	rec.setField(SQL_DESC_CONCISE_TYPE, desc_CONCISE_TYPE);
	SQLSMALLINT desc_DESC_DATETIME_INTERVAL_CODE = 0;
	rec.setField(SQL_DESC_DATETIME_INTERVAL_CODE, desc_DESC_DATETIME_INTERVAL_CODE);
	// nullable
	SQLSMALLINT desc_NULLABLE = sqlType.nullable() ? SQL_NULLABLE : SQL_NO_NULLS;
	rec.setField(SQL_DESC_NULLABLE, desc_NULLABLE);
	// unsigned
	SQLSMALLINT desc_UNSIGNED;
	switch (sqlType.type()) {
	case SQL_SMALLINT:
	case SQL_INTEGER:
	case SQL_BIGINT:
	    desc_UNSIGNED = sqlType.unSigned() ? SQL_TRUE : SQL_FALSE;
	    break;
	default:
	    desc_UNSIGNED = SQL_TRUE;	// thus spake microsoft
	    break;
	}
	rec.setField(SQL_DESC_UNSIGNED, desc_UNSIGNED);
	// sizes
	SQLUINTEGER desc_LENGTH = sqlType.length();
	rec.setField(SQL_DESC_LENGTH, desc_LENGTH);
	SQLINTEGER desc_OCTET_LENGTH = sqlType.size();
	rec.setField(SQL_DESC_OCTET_LENGTH, desc_OCTET_LENGTH);
	SQLINTEGER desc_DISPLAY_SIZE = sqlType.displaySize();
	rec.setField(SQL_DESC_DISPLAY_SIZE, desc_DISPLAY_SIZE);
	// name
	ctx_assert(i < exprRow->m_aliasList.size());
	const char* desc_NAME = exprRow->m_aliasList[i].c_str();
	rec.setField(SQL_DESC_NAME, desc_NAME);
    }
    ctx_log3(("describe %u columns done", count));
    stmtArea().setFunction(ctx, "SELECT CURSOR", SQL_DIAG_SELECT_CURSOR);
}

// Exec_query

Exec_query::Code::~Code()
{
}

Exec_query::Data::~Data()
{
    delete m_extRow;
    m_extRow = 0;
    delete[] m_extPos;
    m_extPos = 0;
}

Exec_query::~Exec_query()
{
}

const Exec_query*
Exec_query::getRawQuery() const
{
    ctx_assert(false);
    return 0;
}

void
Exec_query::bind(Ctx& ctx)
{
    const Code& code = getCode();
    const SqlSpecs& sqlSpecs = code.sqlSpecs();
    const unsigned count = sqlSpecs.count();
    // read ARD
    DescArea& ard = descArea(Desc_usage_ARD);
    const unsigned ardCount = ard.getCount();
    // create specification row
    ExtSpecs extSpecs(count);
    for (unsigned i = 1; i <= count; i++) {
	ExtType extType;
	if (i <= ardCount) {
	    OdbcData descData;
	    DescRec& rec = ard.getRecord(i);
	    // check for unbound column
	    rec.getField(ctx, SQL_DESC_DATA_PTR, descData);
	    SQLPOINTER desc_DATA_PTR = descData.type() != OdbcData::Undef ? descData.pointer() : 0;
	    if (desc_DATA_PTR == 0) {
		extType.setType(ctx, ExtType::Unbound);
	    } else {
		rec.getField(ctx, SQL_DESC_TYPE, descData);
		if (descData.type() == OdbcData::Undef) {
		    ctx.pushStatus(Error::Gen, "query column %u: external type not defined", i);
		    return;
		}
		SQLSMALLINT desc_TYPE = descData.smallint();
		if (desc_TYPE == SQL_C_DEFAULT) {
		    if (i <= code.m_sqlSpecs.count())
			desc_TYPE = code.m_sqlSpecs.getEntry(i).sqlType().sqlcdefault(ctx);
		}
		switch (desc_TYPE) {
		case SQL_C_CHAR:
		case SQL_C_BINARY:
		case SQL_C_SHORT:	// for sun.jdbc.odbc
		case SQL_C_SSHORT:
		case SQL_C_USHORT:
		case SQL_C_LONG:	// for sun.jdbc.odbc
		case SQL_C_SLONG:
		case SQL_C_ULONG:
		case SQL_C_SBIGINT:
		case SQL_C_UBIGINT:
		case SQL_C_FLOAT:
		case SQL_C_DOUBLE:
		case SQL_C_TYPE_TIMESTAMP:
		    break;
		default:
		    ctx.pushStatus(Error::Gen, "query column %u: unsupported external type %d", i, (int)desc_TYPE);
		    return;
		}
		extType.setType(ctx, static_cast<ExtType::Type>(desc_TYPE));
	    }
	} else {
	    extType.setType(ctx, ExtType::Unbound);
	}
	const ExtSpec extSpec(extType);
	extSpecs.setEntry(i, extSpec);
    }
    // create data row
    ExtRow& extRow = *new ExtRow(extSpecs);
    unsigned boundCount = 0;
    for (unsigned i = 1; i <= count; i++) {
	const ExtSpec& extSpec = extSpecs.getEntry(i);
	if (extSpec.extType().type() != ExtType::Unbound) {
	    OdbcData descData;
	    DescRec& rec = ard.getRecord(i);
	    rec.getField(ctx, SQL_DESC_DATA_PTR, descData);
	    SQLPOINTER desc_DATA_PTR = descData.type() != OdbcData::Undef ? descData.pointer() : 0;
	    rec.getField(ctx, SQL_DESC_OCTET_LENGTH, descData);
	    SQLINTEGER desc_OCTET_LENGTH = descData.type() != OdbcData::Undef ? descData.integer() : 0;
	    rec.getField(ctx, SQL_DESC_INDICATOR_PTR, descData);
	    SQLINTEGER* desc_INDICATOR_PTR = descData.type() != OdbcData::Undef ? descData.integerPtr() : 0;
	    ctx_log4(("column %u: bind to 0x%x %d 0x%x", i, (unsigned)desc_DATA_PTR, (int)desc_OCTET_LENGTH, (unsigned)desc_INDICATOR_PTR));
	    ExtField extField(extSpec, desc_DATA_PTR, desc_OCTET_LENGTH, desc_INDICATOR_PTR, i);
	    extRow.setEntry(i, extField);
	    boundCount++;
	} else {
	    ExtField extField(extSpec, i);
	    extRow.setEntry(i, extField);
	}
    }
    Data& data = getData();
    delete data.m_extRow;
    data.m_extRow = &extRow;
    ctx_log3(("bound %u out of %u columns", boundCount, count));
}

// execute and fetch

void
Exec_query::execute(Ctx& ctx, Ctl& ctl)
{
    Data& data = getData();
    execImpl(ctx, ctl);
    if (! ctx.ok())
	return;
    data.initState();
    if (m_topLevel) {
	stmtArea().setRowCount(ctx, data.getCount());
    }
}

bool
Exec_query::fetch(Ctx& ctx, Ctl& ctl)
{
    const Code& code = getCode();
    Data& data = getData();
    if (data.fetch(ctx, ctl)) {
	if (m_topLevel) {
	    stmtArea().setRowCount(ctx, data.getCount());
	}
	if (data.m_extRow != 0) {
	    data.sqlRow().copyout(ctx, *data.m_extRow);
	    if (! ctx.ok())
		return false;
	}
	if (data.m_extPos != 0) {
	    const unsigned count = code.sqlSpecs().count();
	    for (unsigned i = 0; i <= count; i++) {
		data.m_extPos[i] = 0;
	    }
	}
	return true;
    }
    if (m_topLevel) {
	stmtArea().setRowCount(ctx, data.getCount());
	if (ctx.ok()) {
	    ctx.setCode(SQL_NO_DATA);
	}
    }
    return false;
}

// odbc support

void
Exec_query::sqlGetData(Ctx& ctx, SQLUSMALLINT columnNumber, SQLSMALLINT targetType, SQLPOINTER targetValue, SQLINTEGER bufferLength, SQLINTEGER* strlen_or_Ind)
{
    const Code& code = getCode();
    Data& data = getData();
    const SqlSpecs& sqlSpecs = code.m_sqlSpecs;
    const unsigned count = sqlSpecs.count();
    if (columnNumber == 0 || columnNumber > count) {
	ctx.pushStatus(Sqlstate::_07009, Error::Gen, "column index %u is not within 1 to %u", (unsigned)columnNumber, count);
	return;
    }
    // create positions array on first use
    if (data.m_extPos == 0) {
	data.m_extPos = new int[1 + count];
	for (unsigned i = 0; i <= count; i++) {
	    data.m_extPos[i] = 0;
	}
    }
    if (targetType == SQL_ARD_TYPE) {
	// get type from ARD
	DescArea& ard = descArea(Desc_usage_ARD);
	const unsigned ardCount = ard.getCount();
	if (columnNumber <= ardCount) {
	    OdbcData descData;
	    DescRec& rec = ard.getRecord(columnNumber);
	    rec.getField(ctx, SQL_DESC_CONCISE_TYPE, descData);
	    if (descData.type() != OdbcData::Undef) {
		targetType = descData.smallint();
	    }
	}
	if (targetType == SQL_ARD_TYPE) {
	    ctx.pushStatus(Sqlstate::_07009, Error::Gen, "output column %u type not bound - cannot use SQL_ARD_TYPE", (unsigned)columnNumber);
	    return;
	}
    }
    ExtType extType;
    if (targetValue != 0) {
	extType.setType(ctx, static_cast<ExtType::Type>(targetType));
	// check if supported
	if (! ctx.ok())
	    return;
    } else {
	extType.setType(ctx, ExtType::Unbound);
    }
    ExtSpec extSpec(extType);
    ExtField extField(extSpec, targetValue, bufferLength, strlen_or_Ind, columnNumber);
    // copy out and update position
    extField.setPos(data.m_extPos[columnNumber]);
    const SqlRow& sqlRow = data.sqlRow();
    const SqlField& sqlField = sqlRow.getEntry(columnNumber);
    sqlField.copyout(ctx, extField);
    data.m_extPos[columnNumber] = extField.getPos();
}
