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

#ifndef ODBC_HANDLES_HandleDbc_hpp
#define ODBC_HANDLES_HandleDbc_hpp

#include <list>
#include <common/common.hpp>
#include <common/ConnArea.hpp>
#include "HandleBase.hpp"

class HandleRoot;
class HandleEnv;
class HandleStmt;
class HandleDesc;

/**
 * @class HandleDbc
 * @brief Connection handle (SQLHDBC).
 */
class HandleDbc : public HandleBase, public ConnArea {
public:
    HandleDbc(HandleEnv* pEnv);
    ~HandleDbc();
    void ctor(Ctx& ctx);
    void dtor(Ctx& ctx);
    HandleEnv* getEnv();
    HandleBase* getParent();
    HandleRoot* getRoot();
    OdbcHandle odbcHandle();
    // allocate and free handles
    void sqlAllocStmt(Ctx& ctx, HandleStmt** ppStmt);
    void sqlAllocDesc(Ctx& ctx, HandleDesc** ppDesc);
    void sqlAllocHandle(Ctx& ctx, SQLSMALLINT childType, HandleBase** ppChild);
    void sqlFreeStmt(Ctx& ctx, HandleStmt* pStmt, SQLUSMALLINT iOption);
    void sqlFreeDesc(Ctx& ctx, HandleDesc* pDesc);
    void sqlFreeHandle(Ctx& ctx, SQLSMALLINT childType, HandleBase* pChild);
    // attributes and info functions
    void sqlSetConnectAttr(Ctx& ctx, SQLINTEGER attribute, SQLPOINTER value, SQLINTEGER stringLength);
    void sqlGetConnectAttr(Ctx& ctx, SQLINTEGER attribute, SQLPOINTER value, SQLINTEGER bufferLength, SQLINTEGER* stringLength);
    void sqlSetConnectOption(Ctx& ctx, SQLUSMALLINT option, SQLUINTEGER value); // odbc2.0
    void sqlGetConnectOption(Ctx& ctx, SQLUSMALLINT option, SQLPOINTER value); // odbc2.0
    void sqlGetFunctions(Ctx& ctx, SQLUSMALLINT functionId, SQLUSMALLINT* supported);
    void sqlGetInfo(Ctx& ctx, SQLUSMALLINT infoType, SQLPOINTER infoValue, SQLSMALLINT bufferLength, SQLSMALLINT* stringLength);
    int getOdbcVersion(Ctx& ctx);
    // connect and transactions
    void sqlConnect(Ctx& ctx, SQLCHAR* serverName, SQLSMALLINT nameLength1, SQLCHAR* userName, SQLSMALLINT nameLength2, SQLCHAR* authentication, SQLSMALLINT nameLength3);
    void sqlDriverConnect(Ctx& ctx, SQLHWND hwnd, SQLCHAR* szConnStrIn, SQLSMALLINT cbConnStrIn, SQLCHAR* szConnStrOut, SQLSMALLINT cbConnStrOutMax, SQLSMALLINT* pcbConnStrOut, SQLUSMALLINT fDriverCompletion);
    void sqlDisconnect(Ctx& ctx);
    void sqlEndTran(Ctx& ctx, SQLSMALLINT completionType);
    void sqlTransact(Ctx& ctx, SQLUSMALLINT completionType); // odbc2.0
private:
    HandleEnv* const m_env;
    typedef std::list<HandleStmt*> ListStmt;
    ListStmt m_listStmt;
    typedef std::list<HandleDesc*> ListDesc;
    ListDesc m_listDesc;
    static AttrSpec m_attrSpec[];
    AttrArea m_attrArea;
    struct FuncTab {
	SQLUSMALLINT m_functionId;
	int m_supported;
    };
    static FuncTab m_funcTab[];
    struct InfoTab {
	SQLUSMALLINT m_id;
	enum { Char, YesNo, Short, Long, Bitmask, End } m_format;
	SQLUINTEGER m_int;
	const char* m_str;
    };
    static InfoTab m_infoTab[];
};

inline HandleEnv*
HandleDbc::getEnv()
{
    return m_env;
}

inline HandleBase*
HandleDbc::getParent()
{
    return (HandleBase*)getEnv();
}

inline HandleRoot*
HandleDbc::getRoot()
{
    return getParent()->getRoot();
}

inline OdbcHandle
HandleDbc::odbcHandle()
{
    return Odbc_handle_dbc;
}

#endif
