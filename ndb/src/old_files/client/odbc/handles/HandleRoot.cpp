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

#include <common/common.hpp>
#include <NdbMutex.h>
#include <common/DiagArea.hpp>
#include "HandleRoot.hpp"
#include "HandleEnv.hpp"
#include "HandleDbc.hpp"
#include "HandleStmt.hpp"
#include "HandleDesc.hpp"
#include "PoolNdb.hpp"

HandleRoot::HandleRoot() :
    m_attrArea(m_attrSpec)
{
    m_attrArea.setHandle(this);
    m_poolNdb = new PoolNdb();
}

HandleRoot::~HandleRoot()
{
}

#ifdef NDB_WIN32
static NdbMutex & root_mutex = * NdbMutex_Create();
#else
static NdbMutex root_mutex = NDB_MUTEX_INITIALIZER;
#endif

HandleRoot*
HandleRoot::instance()
{
    NdbMutex_Lock(&root_mutex);
    if (m_instance == 0)
	m_instance = new HandleRoot();
    NdbMutex_Unlock(&root_mutex);
    return m_instance;
}

void
HandleRoot::lockHandle()
{
    NdbMutex_Lock(&root_mutex);
}

void
HandleRoot::unlockHandle()
{
    NdbMutex_Unlock(&root_mutex);
}

// check and find handle types and handles

SQLSMALLINT
HandleRoot::findParentType(SQLSMALLINT childType)
{
    switch (childType) {
    case SQL_HANDLE_ENV:
	return SQL_HANDLE_ROOT;
    case SQL_HANDLE_DBC:
	return SQL_HANDLE_ENV;
    case SQL_HANDLE_STMT:
	return SQL_HANDLE_DBC;
    case SQL_HANDLE_DESC:
	return SQL_HANDLE_DBC;
    }
    return -1;
}

HandleBase*
HandleRoot::findBase(SQLSMALLINT handleType, void* pHandle)
{
    switch (handleType) {
    case SQL_HANDLE_ROOT:
	return getRoot();
    case SQL_HANDLE_ENV:
	return findEnv(pHandle);
    case SQL_HANDLE_DBC:
	return findDbc(pHandle);
    case SQL_HANDLE_STMT:
	return findStmt(pHandle);
    case SQL_HANDLE_DESC:
	return findDesc(pHandle);
    }
    return 0;
}

HandleEnv*
HandleRoot::findEnv(void* pHandle)
{
    lockHandle();
    ValidList::iterator i = m_validList.find(pHandle);
    if (i == m_validList.end() || (*i).second != SQL_HANDLE_ENV) {
	unlockHandle();
	return 0;
    }
    unlockHandle();
    ctx_assert(pHandle != 0);
    return static_cast<HandleEnv*>(pHandle);
}

HandleDbc*
HandleRoot::findDbc(void* pHandle)
{
    lockHandle();
    ValidList::iterator i = m_validList.find(pHandle);
    if (i == m_validList.end() || (*i).second != SQL_HANDLE_DBC) {
	unlockHandle();
	return 0;
    }
    unlockHandle();
    ctx_assert(pHandle != 0);
    return static_cast<HandleDbc*>(pHandle);
}

HandleStmt*
HandleRoot::findStmt(void* pHandle)
{
    lockHandle();
    ValidList::iterator i = m_validList.find(pHandle);
    if (i == m_validList.end() || (*i).second != SQL_HANDLE_STMT) {
	unlockHandle();
	return 0;
    }
    unlockHandle();
    ctx_assert(pHandle != 0);
    return static_cast<HandleStmt*>(pHandle);
}

HandleDesc*
HandleRoot::findDesc(void* pHandle)
{
    lockHandle();
    ValidList::iterator i = m_validList.find(pHandle);
    if (i == m_validList.end() || (*i).second != SQL_HANDLE_DESC) {
	unlockHandle();
	return 0;
    }
    unlockHandle();
    ctx_assert(pHandle != 0);
    return static_cast<HandleDesc*>(pHandle);
}

// add or remove handle from validation list

void
HandleRoot::record(SQLSMALLINT handleType, HandleBase* pHandle, bool add)
{
    switch (handleType) {
    case SQL_HANDLE_ENV:
    case SQL_HANDLE_DBC:
    case SQL_HANDLE_STMT:
    case SQL_HANDLE_DESC:
	break;
    default:
	ctx_assert(false);
	break;
    }
    ctx_assert(pHandle != 0);
    lockHandle();
    ValidList::iterator i = m_validList.find(pHandle);
    if (add) {
	if (i != m_validList.end()) {
	    unlockHandle();
	    ctx_assert(false);
	}
	m_validList.insert(ValidList::value_type(pHandle, handleType));
    } else {
	if (i == m_validList.end() || (*i).second != handleType) {
	    unlockHandle();
	    ctx_assert(false);
	}
	m_validList.erase(i);
    }
    unlockHandle();
}

// allocate and free handles

void
HandleRoot::sqlAllocEnv(Ctx& ctx, HandleEnv** ppEnv)
{
    if (ppEnv == 0) {
	ctx.pushStatus(Sqlstate::_HY009, Error::Gen, "cannot allocate environment handle - null return address");
	return;
    }
    HandleEnv* pEnv = new HandleEnv(this);
    pEnv->ctor(ctx);
    if (! ctx.ok()) {
	pEnv->dtor(ctx);
	delete pEnv;
	return;
    }
    lockHandle();
    m_listEnv.push_back(pEnv);
    unlockHandle();
    getRoot()->record(SQL_HANDLE_ENV, pEnv, true);
    *ppEnv = pEnv;
}

void
HandleRoot::sqlAllocHandle(Ctx& ctx, SQLSMALLINT childType, HandleBase** ppChild)
{
    switch (childType) {
    case SQL_HANDLE_ENV:
	sqlAllocEnv(ctx, (HandleEnv**)ppChild);
	return;
    }
    ctx.pushStatus(Sqlstate::_HY092, Error::Gen, "invalid child handle type %d", (int)childType);
}

void
HandleRoot::sqlFreeEnv(Ctx& ctx, HandleEnv* pEnv)
{
    pEnv->dtor(ctx);
    if (! ctx.ok()) {
	return;
    }
    lockHandle();
    m_listEnv.remove(pEnv);
    unlockHandle();
    getRoot()->record(SQL_HANDLE_ENV, pEnv, false);
    delete pEnv;
}

void
HandleRoot::sqlFreeHandle(Ctx& ctx, SQLSMALLINT childType, HandleBase* pChild)
{
    switch (childType) {
    case SQL_HANDLE_ENV:
	sqlFreeEnv(ctx, (HandleEnv*)pChild);
	return;
    }
    ctx.pushStatus(Sqlstate::_HY092, Error::Gen, "invalid child handle type %d", (int)childType);
}

// process-level attributes

void
HandleRoot::sqlSetRootAttr(Ctx& ctx, SQLINTEGER attribute, SQLPOINTER value, SQLINTEGER stringLength)
{
    lockHandle();
    baseSetHandleAttr(ctx, m_attrArea, attribute, value, stringLength);
    unlockHandle();
}

void
HandleRoot::sqlGetRootAttr(Ctx& ctx, SQLINTEGER attribute, SQLPOINTER value, SQLINTEGER bufferLength, SQLINTEGER* stringLength)
{
    lockHandle();
    baseGetHandleAttr(ctx, m_attrArea, attribute, value, bufferLength, stringLength);
    unlockHandle();
}

// the instance

HandleRoot* HandleRoot::m_instance = 0;
