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

#ifndef ODBC_HANDLES_HandleStmt_hpp
#define ODBC_HANDLES_HandleStmt_hpp

#include <common/common.hpp>
#include <common/DescArea.hpp>
#include <common/StmtArea.hpp>
#include "HandleBase.hpp"

class HandleDbc;
class HandleDesc;

/**
 * @class HandleStmt
 * @brief Statement handle (SQLHSTMT).
 */
class HandleStmt : public HandleBase, public StmtArea {
public:
    HandleStmt(HandleDbc* pDbc);
    ~HandleStmt();
    void ctor(Ctx& ctx);
    void dtor(Ctx& ctx);
    HandleDbc* getDbc();
    HandleBase* getParent();
    HandleRoot* getRoot();
    OdbcHandle odbcHandle();
    // descriptor handles
    HandleDesc* getHandleDesc(Ctx& ctx, DescUsage u) const;
    void setHandleDesc(Ctx& ctx, DescUsage u, SQLPOINTER handle);
    // allocate and free handles (no valid case)
    void sqlAllocHandle(Ctx& ctx, SQLSMALLINT childType, HandleBase** ppChild);
    void sqlFreeHandle(Ctx& ctx, SQLSMALLINT childType, HandleBase* pChild);
    // attributes and info
    void sqlSetStmtAttr(Ctx& ctx, SQLINTEGER attribute, SQLPOINTER value, SQLINTEGER stringLength);
    void sqlGetStmtAttr(Ctx& ctx, SQLINTEGER attribute, SQLPOINTER value, SQLINTEGER bufferLength, SQLINTEGER* stringLength);
    void sqlSetStmtOption(Ctx& ctx, SQLUSMALLINT option, SQLUINTEGER value); // odbc2.0
    void sqlGetStmtOption(Ctx& ctx, SQLUSMALLINT option, SQLPOINTER value); // odbc2.0
    void sqlGetTypeInfo(Ctx& ctx, SQLSMALLINT dataType);
    void sqlTables(Ctx& ctx, SQLCHAR* catalogName, SQLSMALLINT nameLength1, SQLCHAR* schemaName, SQLSMALLINT nameLength2, SQLCHAR* tableName, SQLSMALLINT nameLength3, SQLCHAR* tableType, SQLSMALLINT nameLength4);
    void sqlColumns(Ctx& ctx, SQLCHAR* catalogName, SQLSMALLINT nameLength1, SQLCHAR* schemaName, SQLSMALLINT nameLength2, SQLCHAR* tableName, SQLSMALLINT nameLength3, SQLCHAR* columnName, SQLSMALLINT nameLength4);
    void sqlPrimaryKeys(Ctx& ctx, SQLCHAR* szCatalogName, SQLSMALLINT cbCatalogName, SQLCHAR* szSchemaName, SQLSMALLINT cbSchemaName, SQLCHAR* szTableName, SQLSMALLINT cbTableName);
    int getOdbcVersion(Ctx& ctx);
    // prepare and execute
    void sqlPrepare(Ctx& ctx, SQLCHAR* statementText, SQLINTEGER textLength);
    void sqlExecute(Ctx& ctx);
    void sqlExecDirect(Ctx& ctx, SQLCHAR* statementText, SQLINTEGER textLength);
    void sqlFetch(Ctx& ctx);
    void sqlRowCount(Ctx& ctx, SQLINTEGER* rowCount);
    void sqlMoreResults(Ctx& ctx);
    void sqlCancel(Ctx& ctx);
    void sqlCloseCursor(Ctx& ctx);
    void sqlGetCursorName(Ctx& ctx, SQLCHAR* cursorName, SQLSMALLINT bufferLength, SQLSMALLINT* nameLength);
    void sqlSetCursorName(Ctx& ctx, SQLCHAR* cursorName, SQLSMALLINT nameLength);
    // special data access
    void sqlGetData(Ctx& ctx, SQLUSMALLINT columnNumber, SQLSMALLINT targetType, SQLPOINTER targetValue, SQLINTEGER bufferLength, SQLINTEGER* strlen_or_Ind);
    void sqlParamData(Ctx& ctx, SQLPOINTER* value);
    void sqlPutData(Ctx& ctx, SQLPOINTER data, SQLINTEGER strlen_or_Ind);
    // describe statement
    void sqlNumParams(Ctx& ctx, SQLSMALLINT* ParameterCountPtr);
    void sqlDescribeParam(Ctx& ctx, SQLUSMALLINT ipar, SQLSMALLINT* pfSqlType, SQLUINTEGER* pcbParamDef, SQLSMALLINT* pibScale, SQLSMALLINT* pfNullable);
    void sqlNumResultCols(Ctx& ctx, SQLSMALLINT* columnCount);
    void sqlDescribeCol(Ctx& ctx, SQLUSMALLINT columnNumber, SQLCHAR* columnName, SQLSMALLINT bufferLength, SQLSMALLINT* nameLength, SQLSMALLINT* dataType, SQLUINTEGER* columnSize, SQLSMALLINT* decimalDigits, SQLSMALLINT* nullable);
    void sqlColAttribute(Ctx& ctx, SQLUSMALLINT columnNumber, SQLUSMALLINT fieldIdentifier, SQLPOINTER characterAttribute, SQLSMALLINT bufferLength, SQLSMALLINT* stringLength, SQLPOINTER numericAttribute);
    void sqlColAttributes(Ctx& ctx, SQLUSMALLINT icol, SQLUSMALLINT fdescType, SQLPOINTER rgbDesc, SQLSMALLINT cbDescMax, SQLSMALLINT* pcbDesc, SQLINTEGER* pfDesc); // odbc2.0
    // descriptor manipulation
    void sqlBindCol(Ctx& ctx, SQLUSMALLINT columnNumber, SQLSMALLINT targetType, SQLPOINTER targetValue, SQLINTEGER bufferLength, SQLINTEGER* strlen_or_Ind);
    void sqlBindParameter(Ctx& ctx, SQLUSMALLINT ipar, SQLSMALLINT fParamType, SQLSMALLINT fCType, SQLSMALLINT fSqlType, SQLUINTEGER cbColDef, SQLSMALLINT ibScale, SQLPOINTER rgbValue, SQLINTEGER cbValueMax, SQLINTEGER* pcbValue);
    void sqlBindParam(Ctx& ctx, SQLUSMALLINT parameterNumber, SQLSMALLINT valueType, SQLSMALLINT parameterType, SQLUINTEGER lengthPrecision, SQLSMALLINT parameterScale, SQLPOINTER parameterValue, SQLINTEGER* strLen_or_Ind);
    void sqlSetParam(Ctx& ctx, SQLUSMALLINT parameterNumber, SQLSMALLINT valueType, SQLSMALLINT parameterType, SQLUINTEGER lengthPrecision, SQLSMALLINT parameterScale, SQLPOINTER parameterValue, SQLINTEGER* strLen_or_Ind);
private:
    HandleDbc* const m_dbc;
    static AttrSpec m_attrSpec[];
    AttrArea m_attrArea;
    // descriptor handles (0-automatic 1-application)
    HandleDesc* m_handleDesc[2][1+4];
};

inline HandleDbc*
HandleStmt::getDbc()
{
    return m_dbc;
}

inline HandleBase*
HandleStmt::getParent()
{
    return (HandleBase*)getDbc();
}

inline HandleRoot*
HandleStmt::getRoot()
{
    return getParent()->getRoot();
}

inline OdbcHandle
HandleStmt::odbcHandle()
{
    return Odbc_handle_stmt;
}

#endif
