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

#ifndef ODBC_HANDLES_HandleBase_hpp
#define ODBC_HANDLES_HandleBase_hpp

#include <common/common.hpp>
#include <common/OdbcData.hpp>
#include <common/DiagArea.hpp>
#include <common/AttrArea.hpp>

/**
 * @class HandleBase
 * @brief Base class for handles
 *
 * Following types of handles exist:
 * - HandleRoot : root node
 * - HandleEnv  : environment handle (SQLHENV)
 * - HandleDbc  : connection handle (SQLHDBC)
 * - HandleStmt : statement handle (SQLHSTMT)
 * - HandleDesc : descriptor handle (SQLHDESC)
 */
class HandleRoot;
class HandleBase {
public:
    HandleBase();
    virtual ~HandleBase() = 0;
    virtual HandleBase* getParent() = 0;
    virtual HandleRoot* getRoot() = 0;
    virtual OdbcHandle odbcHandle() = 0;
    void saveCtx(Ctx& ctx);
    // allocate and free handles
    virtual void sqlAllocHandle(Ctx& ctx, SQLSMALLINT childType, HandleBase** ppChild) = 0;
    virtual void sqlFreeHandle(Ctx& ctx, SQLSMALLINT childType, HandleBase* pChild) = 0;
    // get diagnostics
    void sqlGetDiagField(Ctx& ctx, SQLSMALLINT recNumber, SQLSMALLINT diagIdentifier, SQLPOINTER diagInfo, SQLSMALLINT bufferLength, SQLSMALLINT* stringLength);
    void sqlGetDiagRec(Ctx& ctx, SQLSMALLINT recNumber, SQLCHAR* sqlstate, SQLINTEGER* nativeError, SQLCHAR* messageText, SQLSMALLINT bufferLength, SQLSMALLINT* textLength);
    void sqlError(Ctx& ctx, SQLCHAR* sqlstate, SQLINTEGER* nativeError, SQLCHAR* messageText, SQLSMALLINT bufferLength, SQLSMALLINT* textLength); // odbc2.0
    // common code for attributes
    void baseSetHandleAttr(Ctx& ctx, AttrArea& attrArea, SQLINTEGER attribute, SQLPOINTER value, SQLINTEGER stringLength);
    void baseGetHandleAttr(Ctx& ctx, AttrArea& attrArea, SQLINTEGER attribute, SQLPOINTER value, SQLINTEGER bufferLength, SQLINTEGER* stringLength);
    void baseSetHandleOption(Ctx& ctx, AttrArea& attrArea, SQLUSMALLINT option, SQLUINTEGER value); // odbc2.0
    void baseGetHandleOption(Ctx& ctx, AttrArea& attrArea, SQLUSMALLINT option, SQLPOINTER value); // odbc2.0
protected:
    Ctx* m_ctx;		// saved from last ODBC function
};

inline
HandleBase::HandleBase() :
    m_ctx(0)
{
}

#endif
