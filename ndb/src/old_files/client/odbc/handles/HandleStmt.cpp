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

#include <common/OdbcData.hpp>
#include <codegen/CodeGen.hpp>
#include "HandleRoot.hpp"
#include "HandleDbc.hpp"
#include "HandleStmt.hpp"
#include "HandleDesc.hpp"

HandleStmt::HandleStmt(HandleDbc* pDbc) :
    StmtArea(*pDbc),
    m_dbc(pDbc),
    m_attrArea(m_attrSpec)
{
    m_attrArea.setHandle(this);
    for (unsigned i = 0; i <= 1; i++) {
	for (unsigned u = 0; u <= 4; u++) {
	    m_handleDesc[i][u] = 0;
	}
    }
}

HandleStmt::~HandleStmt()
{
}

void
HandleStmt::ctor(Ctx& ctx)
{
    for (unsigned u = 1; u <= 4; u++) {
	HandleDesc** ppDesc = &m_handleDesc[0][u];
	m_dbc->sqlAllocDesc(ctx, ppDesc);
	if (! ctx.ok())
	    return;
	m_descArea[u] = &(*ppDesc)->descArea();
	m_descArea[u]->setAlloc(Desc_alloc_auto);
	m_descArea[u]->setUsage((DescUsage)u);
    }
}

void
HandleStmt::dtor(Ctx& ctx)
{
    free(ctx);
    for (unsigned u = 1; u <= 4; u++) {
	HandleDesc** ppDesc = &m_handleDesc[0][u];
	if (*ppDesc != 0)
	    m_dbc->sqlFreeDesc(ctx, *ppDesc);
	*ppDesc = 0;
    }
}

// descriptor handles

HandleDesc*
HandleStmt::getHandleDesc(Ctx& ctx, DescUsage u) const
{
    ctx_assert(1 <= u && u <= 4);
    if (m_handleDesc[1][u] != 0)
	return m_handleDesc[1][u];
    return m_handleDesc[0][u];
}

void
HandleStmt::setHandleDesc(Ctx& ctx, DescUsage u, SQLPOINTER handle)
{
    ctx_assert(1 <= u && u <= 4);
    if (handle == SQL_NULL_HDESC) {
	m_handleDesc[1][u] = 0;		// revert to implicit
	m_descArea[u] = &m_handleDesc[0][u]->descArea();
	return;
    }
    HandleDesc* pDesc = getRoot()->findDesc(handle);
    if (pDesc == 0) {
	ctx.pushStatus(Sqlstate::_HY024, Error::Gen, "cannot set %s handle to invalid descriptor handle %x", DescArea::nameUsage(u), (unsigned)handle);
	return;
    }
    if (pDesc == m_handleDesc[0][u]) {
	m_handleDesc[1][u] = 0;		// revert to implicit
	m_descArea[u] = &m_handleDesc[0][u]->descArea();
	return;
    }
    // XXX should check implicit handles on all statements
    for (unsigned v = 1; v <= 4; v++) {
	if (pDesc == m_handleDesc[0][v]) {
	    ctx.pushStatus(Sqlstate::_HY024, Error::Gen, "cannot set %s handle to implicitly allocated %s handle", DescArea::nameUsage(u), DescArea::nameUsage((DescUsage)v));
	    return;
	}
    }
    m_handleDesc[1][u] = pDesc;
    m_descArea[u] = &m_handleDesc[1][u]->descArea();
}

// allocate and free handles (no valid case)

void
HandleStmt::sqlAllocHandle(Ctx& ctx, SQLSMALLINT childType, HandleBase** ppChild)
{
    ctx.pushStatus(Sqlstate::_HY092, Error::Gen, "inappropriate handle type");
}

void
HandleStmt::sqlFreeHandle(Ctx& ctx, SQLSMALLINT childType, HandleBase* ppChild)
{
    ctx.pushStatus(Sqlstate::_HY092, Error::Gen, "inappropriate handle type");
}

// attributes and info

static bool
ignore_attr(Ctx& ctx, SQLINTEGER attribute)
{
    switch (attribute) {
    case 1211:
    case 1227:
    case 1228:
	ctx_log2(("ignore unknown ADO.NET stmt attribute %d", (int)attribute));
	return true;
    }
    return false;
}

void
HandleStmt::sqlSetStmtAttr(Ctx& ctx, SQLINTEGER attribute, SQLPOINTER value, SQLINTEGER stringLength)
{
    if (ignore_attr(ctx, attribute))
	return;
    baseSetHandleAttr(ctx, m_attrArea, attribute, value, stringLength);
}

void
HandleStmt::sqlGetStmtAttr(Ctx& ctx, SQLINTEGER attribute, SQLPOINTER value, SQLINTEGER bufferLength, SQLINTEGER* stringLength)
{
    if (ignore_attr(ctx, attribute))
	return;
    baseGetHandleAttr(ctx, m_attrArea, attribute, value, bufferLength, stringLength);
}

void
HandleStmt::sqlSetStmtOption(Ctx& ctx, SQLUSMALLINT option, SQLUINTEGER value)
{
    if (ignore_attr(ctx, option))
	return;
    baseSetHandleOption(ctx, m_attrArea, option, value);
}

void
HandleStmt::sqlGetStmtOption(Ctx& ctx, SQLUSMALLINT option, SQLPOINTER value)
{
    if (ignore_attr(ctx, option))
	return;
    baseGetHandleOption(ctx, m_attrArea, option, value);
}

void
HandleStmt::sqlGetTypeInfo(Ctx& ctx, SQLSMALLINT dataType)
{
    BaseString text;
    // odbc$typeinfo is a (possible unordered) view matching SQLGetTypeInfo result set
    text.append("SELECT * FROM odbc$typeinfo");
    if (dataType != SQL_ALL_TYPES) {
	switch (dataType) {
	case SQL_CHAR:
	case SQL_VARCHAR:
	case SQL_BINARY:
	case SQL_VARBINARY:
	case SQL_SMALLINT:
	case SQL_INTEGER:
	case SQL_BIGINT:
	case SQL_REAL:
	case SQL_DOUBLE:
	    break;
	default:
	    // XXX unsupported vs illegal
	    ctx_log1(("sqlGetTypeInfo: unknown data type %d", (int)dataType));
	    break;
	}
	text.appfmt(" WHERE data_type = %d", (int)dataType);
    }
    // sort signed before unsigned
    text.append(" ORDER BY data_type, unsigned_attribute, type_name");
    sqlExecDirect(ctx, (SQLCHAR*)text.c_str(), text.length());
}

void
HandleStmt::sqlTables(Ctx& ctx, SQLCHAR* catalogName, SQLSMALLINT nameLength1, SQLCHAR* schemaName, SQLSMALLINT nameLength2, SQLCHAR* tableName, SQLSMALLINT nameLength3, SQLCHAR* tableType, SQLSMALLINT nameLength4)
{
    BaseString text;
    // odbc$tables is a (possibly unordered) view matching SQLTables result set
    text.append("SELECT * FROM odbc$tables");
    SQLUINTEGER metadata_id = SQL_FALSE;
    sqlGetStmtAttr(ctx, SQL_ATTR_METADATA_ID, (SQLPOINTER)&metadata_id, SQL_IS_POINTER, 0);
    if (! ctx.ok())
	return;
    unsigned count = 0;
    if (catalogName != 0) {
	OdbcData data;
	data.copyin(ctx, OdbcData::Sqlchar, (SQLPOINTER)catalogName, nameLength1);
	if (! ctx.ok())
	    return;
	text.append(++count == 1 ? " WHERE" : " AND");
	if (getOdbcVersion(ctx) == 2)
	    text.appfmt(" table_cat = '%s'", data.sqlchar());
	else if (metadata_id == SQL_TRUE)
	    text.appfmt(" table_cat = '%s'", data.sqlchar());		// XXX UPPER
	else
	    text.appfmt(" table_cat LIKE '%s'", data.sqlchar());
    }
    if (schemaName != 0) {
	OdbcData data;
	data.copyin(ctx, OdbcData::Sqlchar, (SQLPOINTER)schemaName, nameLength2);
	if (! ctx.ok())
	    return;
	text.append(++count == 1 ? " WHERE" : " AND");
	if (metadata_id == SQL_TRUE)
	    text.appfmt(" table_schem = '%s'", data.sqlchar());	// XXX UPPER
	else
	    text.appfmt(" table_schem LIKE '%s'", data.sqlchar());
    }
    if (tableName != 0) {
	OdbcData data;
	data.copyin(ctx, OdbcData::Sqlchar, (SQLPOINTER)tableName, nameLength3);
	if (! ctx.ok())
	    return;
	text.append(++count == 1 ? " WHERE" : " AND");
	if (metadata_id == SQL_TRUE)
	    text.appfmt(" table_name = '%s'", data.sqlchar());		// XXX UPPER
	else
	    text.appfmt(" table_name LIKE '%s'", data.sqlchar());
    }
    if (tableType != 0) {
	OdbcData data;
	data.copyin(ctx, OdbcData::Sqlchar, (SQLPOINTER)tableType, nameLength4);
	if (! ctx.ok())
	    return;
	text.append(++count == 1 ? " WHERE" : " AND");
	text.appfmt(" table_type IN (%s)", data.sqlchar());		// XXX UPPER, quotes
    }
    text.append(" ORDER BY table_type, table_cat, table_schem, table_name");
    sqlExecDirect(ctx, (SQLCHAR*)text.c_str(), text.length());
}

void
HandleStmt::sqlColumns(Ctx& ctx, SQLCHAR* catalogName, SQLSMALLINT nameLength1, SQLCHAR* schemaName, SQLSMALLINT nameLength2, SQLCHAR* tableName, SQLSMALLINT nameLength3, SQLCHAR* columnName, SQLSMALLINT nameLength4)
{
    BaseString text;
    // odbc$columns is a (possibly unordered) view matching SQLColumns result set
    text.append("SELECT * FROM odbc$columns");
    SQLUINTEGER metadata_id = SQL_FALSE;
    sqlGetStmtAttr(ctx, SQL_ATTR_METADATA_ID, (SQLPOINTER)&metadata_id, SQL_IS_POINTER, 0);
    if (! ctx.ok())
	return;
    unsigned count = 0;
    if (catalogName != 0) {
	OdbcData data;
	data.copyin(ctx, OdbcData::Sqlchar, (SQLPOINTER)catalogName, nameLength1);
	if (! ctx.ok())
	    return;
	text.append(++count == 1 ? " WHERE" : " AND");
	if (getOdbcVersion(ctx) == 2)
	    text.appfmt(" table_cat = '%s'", data.sqlchar());
	else if (metadata_id == SQL_TRUE)
	    text.appfmt(" table_cat = '%s'", data.sqlchar());		// XXX UPPER
	else
	    text.appfmt(" table_cat LIKE '%s'", data.sqlchar());
    }
    if (schemaName != 0) {
	OdbcData data;
	data.copyin(ctx, OdbcData::Sqlchar, (SQLPOINTER)schemaName, nameLength2);
	if (! ctx.ok())
	    return;
	text.append(++count == 1 ? " WHERE" : " AND");
	if (metadata_id == SQL_TRUE)
	    text.appfmt(" table_schem = '%s'", data.sqlchar());		// XXX UPPER
	else
	    text.appfmt(" table_schem LIKE '%s'", data.sqlchar());
    }
    if (tableName != 0) {
	OdbcData data;
	data.copyin(ctx, OdbcData::Sqlchar, (SQLPOINTER)tableName, nameLength3);
	if (! ctx.ok())
	    return;
	text.append(++count == 1 ? " WHERE" : " AND");
	if (metadata_id == SQL_TRUE)
	    text.appfmt(" table_name = '%s'", data.sqlchar());		// XXX UPPER
	else
	    text.appfmt(" table_name LIKE '%s'", data.sqlchar());
    }
    if (columnName != 0) {
	OdbcData data;
	data.copyin(ctx, OdbcData::Sqlchar, (SQLPOINTER)columnName, nameLength4);
	if (! ctx.ok())
	    return;
	text.append(++count == 1 ? " WHERE" : " AND");
	if (metadata_id == SQL_TRUE)
	    text.appfmt(" column_name = '%s'", data.sqlchar());		// XXX UPPER
	else
	    text.appfmt(" column_name LIKE '%s'", data.sqlchar());
    }
    text.append(" ORDER BY table_cat, table_schem, table_name, ordinal_position");
    sqlExecDirect(ctx, (SQLCHAR*)text.c_str(), text.length());
}

void
HandleStmt::sqlPrimaryKeys(Ctx& ctx, SQLCHAR* szCatalogName, SQLSMALLINT cbCatalogName, SQLCHAR* szSchemaName, SQLSMALLINT cbSchemaName, SQLCHAR* szTableName, SQLSMALLINT cbTableName)
{
    BaseString text;
    // odbc$primarykeys is a (possible unordered) view matching SQLPrimaryKeys result set
    text.append("SELECT * FROM odbc$primarykeys");
    SQLUINTEGER metadata_id = SQL_FALSE;
    sqlGetStmtAttr(ctx, SQL_ATTR_METADATA_ID, (SQLPOINTER)&metadata_id, SQL_IS_POINTER, 0);
    if (! ctx.ok())
	return;
    unsigned count = 0;
    if (szCatalogName != 0) {
	OdbcData data;
	data.copyin(ctx, OdbcData::Sqlchar, (SQLPOINTER)szCatalogName, cbCatalogName);
	if (! ctx.ok())
	    return;
	text.append(++count == 1 ? " WHERE" : " AND");
	if (getOdbcVersion(ctx) == 2)
	    text.appfmt(" table_cat = '%s'", data.sqlchar());
	else if (metadata_id == SQL_TRUE)
	    text.appfmt(" table_cat = '%s'", data.sqlchar());		// XXX UPPER
	else
	    text.appfmt(" table_cat = '%s'", data.sqlchar());		// no pattern
    }
    if (szSchemaName != 0) {
	OdbcData data;
	data.copyin(ctx, OdbcData::Sqlchar, (SQLPOINTER)szSchemaName, cbSchemaName);
	if (! ctx.ok())
	    return;
	text.append(++count == 1 ? " WHERE" : " AND");
	if (metadata_id == SQL_TRUE)
	    text.appfmt(" table_schem = '%s'", data.sqlchar());		// XXX UPPER
	else
	    text.appfmt(" table_schem = '%s'", data.sqlchar());		// no pattern
    }
    if (szTableName != 0) {
	OdbcData data;
	data.copyin(ctx, OdbcData::Sqlchar, (SQLPOINTER)szTableName, cbTableName);
	if (! ctx.ok())
	    return;
	text.append(++count == 1 ? " WHERE" : " AND");
	if (metadata_id == SQL_TRUE)
	    text.appfmt(" table_name = '%s'", data.sqlchar());		// XXX UPPER
	else
	    text.appfmt(" table_name = '%s'", data.sqlchar());		// no pattern
    } else {								// no null
	ctx.pushStatus(Sqlstate::_HY009, Error::Gen, "null table name");
	return;
    }
    text.append(" ORDER BY table_cat, table_schem, table_name, key_seq");
    sqlExecDirect(ctx, (SQLCHAR*)text.c_str(), text.length());
}

int
HandleStmt::getOdbcVersion(Ctx& ctx)
{
    return m_dbc->getOdbcVersion(ctx);
}

// prepare and execute

void
HandleStmt::sqlPrepare(Ctx& ctx, SQLCHAR* statementText, SQLINTEGER textLength)
{
    if (m_state == Open) {
	ctx.pushStatus(Sqlstate::_24000, Error::Gen, "cursor is open");
	return;
    }
    free(ctx);
    const char* text = reinterpret_cast<char*>(statementText);
    if (textLength == SQL_NTS) {
	m_sqlText.assign(text);
    } else if (textLength >= 0) {
	m_sqlText.assign(text, textLength);
    } else {
	ctx.pushStatus(Sqlstate::_HY090, Error::Gen, "missing SQL text");
	return;
    }
    if (! useSchemaCon(ctx, true))
	return;
    CodeGen codegen(*this);
    codegen.prepare(ctx);
    useSchemaCon(ctx, false);
    if (! ctx.ok()) {
	free(ctx);
	return;
    }
    ctx_log2(("prepared %s statement", m_stmtInfo.getDesc()));
    m_state = Prepared;
}

void
HandleStmt::sqlExecute(Ctx& ctx)
{
    if (m_state == Open) {
	ctx.pushStatus(Sqlstate::_24000, Error::Gen, "cursor is open");
	return;
    }
    StmtType stmtType = m_stmtInfo.getType();
    switch (stmtType) {
    case Stmt_type_DDL:
	if (! useSchemaCon(ctx, true))
	    return;
	break;
    case Stmt_type_query:
    case Stmt_type_DML:
	if (! useConnection(ctx, true))
	    return;
	break;
    default:
	ctx_assert(false);
	break;
    }
    CodeGen codegen(*this);
    codegen.execute(ctx);
    // valid only after execute says M$  XXX create this diag only on demand
    setFunction(ctx);
    if (ctx.getCode() == SQL_NEED_DATA) {
	m_state = NeedData;
	return;
    }
    ctx_log2(("execute: fetched %u tuples from NDB", (unsigned)m_tuplesFetched));
    switch (stmtType) {
    case Stmt_type_DDL:
	codegen.close(ctx);
	useSchemaCon(ctx, false);
	m_state = Prepared;
	break;
    case Stmt_type_query:
	if (! ctx.ok()) {
	    codegen.close(ctx);
	    useConnection(ctx, false);
	    m_state = Prepared;
	} else {
	    m_state = Open;
	}
	break;
    case Stmt_type_DML:
	codegen.close(ctx);
	if (m_dbc->autocommit()) {
	    // even if error
	    m_dbc->sqlEndTran(ctx, SQL_COMMIT);
	} else {
	    m_dbc->uncommitted(true);	// uncommitted changes possible
	}
	useConnection(ctx, false);
	m_state = Prepared;
	break;
    default:
	ctx_assert(false);
	break;
    }
}

void
HandleStmt::sqlExecDirect(Ctx& ctx, SQLCHAR* statementText, SQLINTEGER textLength)
{
    sqlPrepare(ctx, statementText, textLength);
    if (! ctx.ok())
	return;
    sqlExecute(ctx);
}

void
HandleStmt::sqlFetch(Ctx& ctx)
{
    if (m_state != Open) {
	ctx.pushStatus(Sqlstate::_24000, Error::Gen, "cursor is not open");
	return;
    }
    StmtType stmtType = m_stmtInfo.getType();
    switch (stmtType) {
    case Stmt_type_query: {
	CodeGen codegen(*this);
	codegen.fetch(ctx);
	if (! ctx.ok()) {
	    codegen.close(ctx);
	    useConnection(ctx, false);
	}
	break;
	}
    default:
	ctx_assert(false);
	break;
    }
}

void
HandleStmt::sqlRowCount(Ctx& ctx, SQLINTEGER* rowCount)
{
    *rowCount = m_rowCount;
}

void
HandleStmt::sqlMoreResults(Ctx& ctx)
{
    if (m_state == Open) {
	sqlCloseCursor(ctx);
	if (! ctx.ok())
	    return;
    }
    ctx.setCode(SQL_NO_DATA);
}

void
HandleStmt::sqlCancel(Ctx& ctx)
{
    if (m_state == NeedData) {
	CodeGen codegen(*this);
	codegen.close(ctx);
	m_state = Prepared;
    }
}

void
HandleStmt::sqlCloseCursor(Ctx& ctx)
{
    if (m_state != Open) {
	ctx.pushStatus(Sqlstate::_24000, Error::Gen, "cursor is not open");
	return;
    }
    ctx_log2(("execute: fetched %u tuples from NDB", (unsigned)m_tuplesFetched));
    StmtType stmtType = m_stmtInfo.getType();
    switch (stmtType) {
    case Stmt_type_query: {
	CodeGen codegen(*this);
	codegen.close(ctx);
	useConnection(ctx, false);
	m_state = Prepared;
	m_rowCount = 0;
	m_tuplesFetched = 0;
	break;
	}
    default:
	ctx_assert(false);
	break;
    }
}

void
HandleStmt::sqlGetCursorName(Ctx& ctx, SQLCHAR* cursorName, SQLSMALLINT bufferLength, SQLSMALLINT* nameLength)
{
    OdbcData name("SQL_CUR_DUMMY");
    name.copyout(ctx, cursorName, bufferLength, 0, nameLength);
}

void
HandleStmt::sqlSetCursorName(Ctx& ctx, SQLCHAR* cursorName, SQLSMALLINT nameLength)
{
}

// special data access

void
HandleStmt::sqlGetData(Ctx& ctx, SQLUSMALLINT columnNumber, SQLSMALLINT targetType, SQLPOINTER targetValue, SQLINTEGER bufferLength, SQLINTEGER* strlen_or_Ind)
{
    if (m_state != Open) {
	ctx.pushStatus(Sqlstate::_24000, Error::Gen, "cursor is not open");
	return;
    }
    if (bufferLength < 0) {
	ctx.pushStatus(Sqlstate::_HY090, Error::Gen, "invalid buffer length %d", (int)bufferLength);
	return;
    }
    CodeGen codegen(*this);
    codegen.sqlGetData(ctx, columnNumber, targetType, targetValue, bufferLength, strlen_or_Ind);
}

void
HandleStmt::sqlParamData(Ctx& ctx, SQLPOINTER* value)
{
    if (m_state != NeedData) {
	ctx.pushStatus(Sqlstate::_HY010, Error::Gen, "not expecting data-at-exec");
	return;
    }
    CodeGen codegen(*this);
    codegen.sqlParamData(ctx, value);
    if (! ctx.ok())
	return;
    sqlExecute(ctx);
}

void
HandleStmt::sqlPutData(Ctx& ctx, SQLPOINTER data, SQLINTEGER strlen_or_Ind)
{
    if (m_state != NeedData) {
	ctx.pushStatus(Sqlstate::_HY010, Error::Gen, "not expecting data-at-exec");
	return;
    }
    CodeGen codegen(*this);
    codegen.sqlPutData(ctx, data, strlen_or_Ind);
}

// describe statement

void
HandleStmt::sqlNumParams(Ctx& ctx, SQLSMALLINT* parameterCountPtr)
{
    if (m_state == Free) {
	ctx.pushStatus(Sqlstate::_HY010, Error::Gen, "statement is not prepared");
	return;
    }
    HandleDesc* ipd = getHandleDesc(ctx, Desc_usage_IPD);
    ipd->sqlGetDescField(ctx, 0, SQL_DESC_COUNT, static_cast<SQLPOINTER>(parameterCountPtr), -1, 0);
}

void
HandleStmt::sqlDescribeParam(Ctx& ctx, SQLUSMALLINT ipar, SQLSMALLINT* pfSqlType, SQLUINTEGER* pcbParamDef, SQLSMALLINT* pibScale, SQLSMALLINT* pfNullable)
{
    if (m_state == Free) {
	ctx.pushStatus(Sqlstate::_HY010, Error::Gen, "statement is not prepared");
	return;
    }
    HandleDesc* ipd = getHandleDesc(ctx, Desc_usage_IPD);
    ipd->sqlGetDescField(ctx, ipar, SQL_DESC_CONCISE_TYPE, static_cast<SQLPOINTER>(pfSqlType), -1, 0);
    {	// XXX fix it
	OdbcData data((SQLUINTEGER)0);
	data.copyout(ctx, (SQLPOINTER)pcbParamDef, -1, 0);
    }
    {	// XXX fix it
	OdbcData data((SQLSMALLINT)0);
	data.copyout(ctx, (SQLPOINTER)pibScale, -1, 0);
    }
    ipd->sqlGetDescField(ctx, ipar, SQL_DESC_NULLABLE, static_cast<SQLPOINTER>(pfNullable), -1, 0);
}

void
HandleStmt::sqlNumResultCols(Ctx& ctx, SQLSMALLINT* columnCount)
{
    if (m_state == Free) {
	ctx.pushStatus(Sqlstate::_HY010, Error::Gen, "statement is not prepared");
	return;
    }
    HandleDesc* const ird = getHandleDesc(ctx, Desc_usage_IRD);
    ird->sqlGetDescField(ctx, 0, SQL_DESC_COUNT, static_cast<SQLPOINTER>(columnCount), -1, 0);
    setFunction(ctx);	// unixODBC workaround
}

void
HandleStmt::sqlDescribeCol(Ctx& ctx, SQLUSMALLINT columnNumber, SQLCHAR* columnName, SQLSMALLINT bufferLength, SQLSMALLINT* nameLength, SQLSMALLINT* dataType, SQLUINTEGER* columnSize, SQLSMALLINT* decimalDigits, SQLSMALLINT* nullable)
{
    if (m_state == Free) {
	ctx.pushStatus(Sqlstate::_HY010, Error::Gen, "statement is not prepared");
	return;
    }
    HandleDesc* const ird = getHandleDesc(ctx, Desc_usage_IRD);
    ird->sqlGetDescField(ctx, columnNumber, SQL_DESC_NAME, static_cast<SQLPOINTER>(columnName), bufferLength, 0, nameLength);
    ird->sqlGetDescField(ctx, columnNumber, SQL_DESC_CONCISE_TYPE, static_cast<SQLPOINTER>(dataType), -1, 0);
    if (! ctx.ok())
	return;
    if (columnSize != 0) {
	switch (*dataType) {
	case SQL_CHAR:
	case SQL_VARCHAR:
	case SQL_BINARY:
	case SQL_VARBINARY:
	    ird->sqlGetDescField(ctx, columnNumber, SQL_DESC_LENGTH, static_cast<SQLPOINTER>(columnSize), -1, 0);
	    break;
	case SQL_SMALLINT:
	    *columnSize = 5;
	    break;
	case SQL_INTEGER:
	    *columnSize = 10;
	    break;
	case SQL_BIGINT:
	    *columnSize = 20;	// XXX 19 for signed
	    break;
	case SQL_REAL:
	    *columnSize = 7;
	    break;
	case SQL_DOUBLE:
	    *columnSize = 15;
	    break;
	case SQL_TYPE_TIMESTAMP:
	    *columnSize = 30;
	    break;
	default:
	    *columnSize = 0;
	    break;
	}
    }
    if (decimalDigits != 0) {
	switch (*dataType) {
	case SQL_SMALLINT:
	case SQL_INTEGER:
	case SQL_BIGINT:
	    *decimalDigits = 0;
	    break;
	case SQL_TYPE_TIMESTAMP:
	    *decimalDigits = 10;
	    break;
	default:
	    *decimalDigits = 0;
	    break;
	}
    }
    ird->sqlGetDescField(ctx, columnNumber, SQL_DESC_NULLABLE, static_cast<SQLPOINTER>(nullable), -1, 0);
}

void
HandleStmt::sqlColAttribute(Ctx& ctx, SQLUSMALLINT columnNumber, SQLUSMALLINT fieldIdentifier, SQLPOINTER characterAttribute, SQLSMALLINT bufferLength, SQLSMALLINT* stringLength, SQLPOINTER numericAttribute)
{
    if (m_state == Free) {
	ctx.pushStatus(Sqlstate::_HY010, Error::Gen, "statement is not prepared");
	return;
    }
    HandleDesc* const ird = getHandleDesc(ctx, Desc_usage_IRD);
    ird->sqlColAttribute(ctx, columnNumber, fieldIdentifier, characterAttribute, bufferLength, stringLength, numericAttribute);
}

void
HandleStmt::sqlColAttributes(Ctx& ctx, SQLUSMALLINT icol, SQLUSMALLINT fdescType, SQLPOINTER rgbDesc, SQLSMALLINT cbDescMax, SQLSMALLINT* pcbDesc, SQLINTEGER* pfDesc)
{
    if (m_state == Free) {
	ctx.pushStatus(Sqlstate::_HY010, Error::Gen, "statement is nor prepared");
	return;
    }
    HandleDesc* const ird = getHandleDesc(ctx, Desc_usage_IRD);
    ird->sqlColAttributes(ctx, icol, fdescType, rgbDesc, cbDescMax, pcbDesc, pfDesc);
}

// descriptor manipulation

void
HandleStmt::sqlBindCol(Ctx& ctx, SQLUSMALLINT columnNumber, SQLSMALLINT targetType, SQLPOINTER targetValue, SQLINTEGER bufferLength, SQLINTEGER* strlen_or_Ind)
{
    HandleDesc* const ard = getHandleDesc(ctx, Desc_usage_ARD);
    DescArea& desc = ard->descArea();
    if (desc.getCount() < columnNumber) {
	desc.setCount(ctx, columnNumber);
    }
    DescRec& rec = desc.getRecord(columnNumber);
    rec.setField(ctx, SQL_DESC_TYPE, targetType);
    rec.setField(ctx, SQL_DESC_CONCISE_TYPE, targetType);
    rec.setField(ctx, SQL_DESC_DATA_PTR, targetValue);
    rec.setField(ctx, SQL_DESC_OCTET_LENGTH, bufferLength);
    rec.setField(ctx, SQL_DESC_OCTET_LENGTH_PTR, strlen_or_Ind);
    rec.setField(ctx, SQL_DESC_INDICATOR_PTR, strlen_or_Ind);
}

void
HandleStmt::sqlBindParameter(Ctx& ctx, SQLUSMALLINT ipar, SQLSMALLINT fParamType, SQLSMALLINT fCType, SQLSMALLINT fSqlType, SQLUINTEGER cbColDef, SQLSMALLINT ibScale, SQLPOINTER rgbValue, SQLINTEGER cbValueMax, SQLINTEGER* pcbValue)
{
    HandleDesc* const ipd = getHandleDesc(ctx, Desc_usage_IPD);
    HandleDesc* const apd = getHandleDesc(ctx, Desc_usage_APD);
    DescArea& descIPD = ipd->descArea();
    DescArea& descAPD = apd->descArea();
    if (ipar < 1) {
	ctx.pushStatus(Sqlstate::_07009, Error::Gen, "invalid parameter index %u", (unsigned)ipar);
	return;
    }
    if (descIPD.getCount() < ipar) {
	descIPD.setCount(ctx, ipar);
    }
    if (descAPD.getCount() < ipar) {
	descAPD.setCount(ctx, ipar);
    }
    DescRec& recIPD = descIPD.getRecord(ipar);
    DescRec& recAPD = descAPD.getRecord(ipar);
    recIPD.setField(ctx, SQL_DESC_PARAMETER_TYPE, fParamType);
    recAPD.setField(ctx, SQL_DESC_TYPE, fCType);
    recAPD.setField(ctx, SQL_DESC_CONCISE_TYPE, fCType);
    recIPD.setField(ctx, SQL_DESC_TYPE, fSqlType);
    recIPD.setField(ctx, SQL_DESC_CONCISE_TYPE, fSqlType);
    switch (fSqlType) {
    case SQL_CHAR:		// XXX not complete
    case SQL_VARCHAR:
    case SQL_BINARY:
    case SQL_VARBINARY:
	recIPD.setField(ctx, SQL_DESC_LENGTH, cbColDef);
	break;
    case SQL_DECIMAL:
    case SQL_NUMERIC:
    case SQL_FLOAT:
    case SQL_REAL:
    case SQL_DOUBLE:
	recIPD.setField(ctx, SQL_DESC_PRECISION, cbColDef);
	break;
    }
    switch (fSqlType) {
    case SQL_TYPE_TIME:		// XXX not complete
    case SQL_TYPE_TIMESTAMP:
	recIPD.setField(ctx, SQL_DESC_PRECISION, ibScale);
	break;
    case SQL_NUMERIC:
    case SQL_DECIMAL:
	recIPD.setField(ctx, SQL_DESC_SCALE, ibScale);
	break;
    }
    recAPD.setField(ctx, SQL_DESC_DATA_PTR, rgbValue);
    recAPD.setField(ctx, SQL_DESC_OCTET_LENGTH, cbValueMax);
    recAPD.setField(ctx, SQL_DESC_OCTET_LENGTH_PTR, pcbValue);
    recAPD.setField(ctx, SQL_DESC_INDICATOR_PTR, pcbValue);
}

void
HandleStmt::sqlBindParam(Ctx& ctx, SQLUSMALLINT parameterNumber, SQLSMALLINT valueType, SQLSMALLINT parameterType, SQLUINTEGER lengthPrecision, SQLSMALLINT parameterScale, SQLPOINTER parameterValue, SQLINTEGER* strLen_or_Ind)
{
    sqlBindParameter(ctx, parameterNumber, SQL_PARAM_INPUT_OUTPUT, valueType, parameterType, lengthPrecision, parameterScale, parameterValue, SQL_SETPARAM_VALUE_MAX, strLen_or_Ind);
}

void
HandleStmt::sqlSetParam(Ctx& ctx, SQLUSMALLINT parameterNumber, SQLSMALLINT valueType, SQLSMALLINT parameterType, SQLUINTEGER lengthPrecision, SQLSMALLINT parameterScale, SQLPOINTER parameterValue, SQLINTEGER* strLen_or_Ind)
{
    sqlBindParameter(ctx, parameterNumber, SQL_PARAM_INPUT_OUTPUT, valueType, parameterType, lengthPrecision, parameterScale, parameterValue, SQL_SETPARAM_VALUE_MAX, strLen_or_Ind);
}
