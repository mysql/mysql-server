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

#ifndef ODBC_HANDLES_HandleEnv_hpp
#define ODBC_HANDLES_HandleEnv_hpp

#include <list>
#include <common/common.hpp>
#include "HandleBase.hpp"

class HandleRoot;
class HandleDbc;

/**
 * @class HandleEnv
 * @brief Environment handle (SQLHENV).
 */
class HandleEnv : public HandleBase {
public:
    HandleEnv(HandleRoot* pRoot);
    ~HandleEnv();
    void ctor(Ctx& ctx);
    void dtor(Ctx& ctx);
    HandleRoot* getRoot();
    HandleBase* getParent();
    OdbcHandle odbcHandle();
    // allocate and free handles
    void sqlAllocConnect(Ctx& ctx, HandleDbc** ppDbc);
    void sqlAllocHandle(Ctx& ctx, SQLSMALLINT childType, HandleBase** ppChild);
    void sqlFreeConnect(Ctx& ctx, HandleDbc* pDbc);
    void sqlFreeHandle(Ctx& ctx, SQLSMALLINT childType, HandleBase* pChild);
    // attributes
    void sqlSetEnvAttr(Ctx& ctx, SQLINTEGER attribute, SQLPOINTER value, SQLINTEGER stringLength);
    void sqlGetEnvAttr(Ctx& ctx, SQLINTEGER attribute, SQLPOINTER value, SQLINTEGER bufferLength, SQLINTEGER* stringLength);
    int getOdbcVersion(Ctx& ctx);		// returns -1 if not set
    // transactions
    void sqlEndTran(Ctx& ctx, SQLSMALLINT completionType);
    void sqlTransact(Ctx& ctx, SQLUSMALLINT completionType); // odbc2.0
private:
    HandleRoot* const m_root;
    std::list<HandleDbc*> m_listDbc;
    static AttrSpec m_attrSpec[];
    AttrArea m_attrArea;
};

inline HandleRoot*
HandleEnv::getRoot()
{
    return m_root;
}

inline HandleBase*
HandleEnv::getParent()
{
    return (HandleBase*)getRoot();
}

inline OdbcHandle
HandleEnv::odbcHandle()
{
    return Odbc_handle_env;
}

#endif
