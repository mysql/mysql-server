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

#ifndef ODBC_HANDLES_HandleRoot_hpp
#define ODBC_HANDLES_HandleRoot_hpp

#include <list>
#include <map>
#include <common/common.hpp>
#include "HandleBase.hpp"

class HandleEnv;
class HandleDbc;
class HandleStmt;
class HandleDesc;
class PoolNdb;

/**
 * @class HandleRoot
 * @brief The singleton root handle.
 *
 * This class is the level above HandleEnv.  It has a single
 * instance.  The instance is the root node of dynamically
 * allocated handles.  The instance is also used to call methods
 * not tied to any handle.
 */
class HandleRoot : public HandleBase {
protected:
    HandleRoot();
    ~HandleRoot();
public:
    static HandleRoot* instance();
    HandleRoot* getRoot();
    HandleBase* getParent();
    PoolNdb* getPoolNdb();
    OdbcHandle odbcHandle();
    void lockHandle();
    void unlockHandle();
    // check and find handle types and handles
    SQLSMALLINT findParentType(SQLSMALLINT childType);
    HandleBase* findBase(SQLSMALLINT handleType, void* pHandle);
    HandleEnv* findEnv(void* pHandle);
    HandleDbc* findDbc(void* pHandle);
    HandleStmt* findStmt(void* pHandle);
    HandleDesc* findDesc(void* pHandle);
    // add or remove handle from validation list
    void record(SQLSMALLINT handleType, HandleBase* pHandle, bool add);
    // allocate and free handles
    void sqlAllocEnv(Ctx& ctx, HandleEnv** ppEnv);
    void sqlAllocHandle(Ctx& ctx, SQLSMALLINT childType, HandleBase** ppChild);
    void sqlFreeEnv(Ctx& ctx, HandleEnv* pEnv);
    void sqlFreeHandle(Ctx& ctx, SQLSMALLINT childType, HandleBase* pChild);
    // process-level attributes
    void sqlSetRootAttr(Ctx& ctx, SQLINTEGER attribute, SQLPOINTER value, SQLINTEGER stringLength);
    void sqlGetRootAttr(Ctx& ctx, SQLINTEGER attribute, SQLPOINTER value, SQLINTEGER bufferLength, SQLINTEGER* stringLength);
private:
    static HandleRoot* m_instance;	// the instance
    std::list<HandleEnv*> m_listEnv;
    PoolNdb* m_poolNdb;
    typedef std::map<void*, SQLSMALLINT> ValidList;
    ValidList m_validList;
    static AttrSpec m_attrSpec[];
    AttrArea m_attrArea;
};

inline HandleRoot*
HandleRoot::getRoot()
{
    return this;
}

inline HandleBase*
HandleRoot::getParent()
{
    return 0;
}

inline PoolNdb*
HandleRoot::getPoolNdb()
{
    return m_poolNdb;
}

inline OdbcHandle
HandleRoot::odbcHandle()
{
    return Odbc_handle_root;
}

#endif
