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

#include <common/DiagArea.hpp>
#include "HandleRoot.hpp"
#include "HandleEnv.hpp"
#include "HandleDbc.hpp"

HandleEnv::HandleEnv(HandleRoot* pRoot) :
    m_root(pRoot),
    m_attrArea(m_attrSpec)
{
    m_attrArea.setHandle(this);
}

HandleEnv::~HandleEnv() {
}

void
HandleEnv::ctor(Ctx& ctx)
{
}

void
HandleEnv::dtor(Ctx& ctx)
{
    if (! m_listDbc.empty()) {
	ctx.pushStatus(Sqlstate::_HY010, Error::Gen, "cannot delete environment handle - has %u associated connection handles", (unsigned)m_listDbc.size());
	return;
    }
}

// allocate and free handles

void
HandleEnv::sqlAllocConnect(Ctx& ctx, HandleDbc** ppDbc)
{
    if (getOdbcVersion(ctx) == -1)
	return;
    if (ppDbc == 0) {
	ctx.pushStatus(Sqlstate::_HY009, Error::Gen, "cannot allocate connection handle - null return address");
	return;
    }
    HandleDbc* pDbc = new HandleDbc(this);
    pDbc->ctor(ctx);
    if (! ctx.ok()) {
	pDbc->dtor(ctx);
	delete pDbc;
	return;
    }
    m_listDbc.push_back(pDbc);
    getRoot()->record(SQL_HANDLE_DBC, pDbc, true);
    *ppDbc = pDbc;
}

void
HandleEnv::sqlAllocHandle(Ctx& ctx, SQLSMALLINT childType, HandleBase** ppChild)
{
    if (getOdbcVersion(ctx) == -1)
	return;
    switch (childType) {
    case SQL_HANDLE_DBC:
	sqlAllocConnect(ctx, (HandleDbc**)ppChild);
	return;
    }
    ctx.pushStatus(Sqlstate::_HY092, Error::Gen, "invalid child handle type %d", (int)childType);
}

void
HandleEnv::sqlFreeConnect(Ctx& ctx, HandleDbc* pDbc)
{
    if (getOdbcVersion(ctx) == -1)
	return;
    pDbc->dtor(ctx);
    if (! ctx.ok())
	return;
    m_listDbc.remove(pDbc);
    getRoot()->record(SQL_HANDLE_DBC, pDbc, false);
    delete pDbc;
}

void
HandleEnv::sqlFreeHandle(Ctx& ctx, SQLSMALLINT childType, HandleBase* pChild)
{
    if (getOdbcVersion(ctx) == -1)
	return;
    switch (childType) {
    case SQL_HANDLE_DBC:
	sqlFreeConnect(ctx, (HandleDbc*)pChild);
	return;
    }
    ctx.pushStatus(Sqlstate::_HY092, Error::Gen, "invalid child handle type %d", (int)childType);
}

// attributes

void
HandleEnv::sqlSetEnvAttr(Ctx& ctx, SQLINTEGER attribute, SQLPOINTER value, SQLINTEGER stringLength)
{
    baseSetHandleAttr(ctx, m_attrArea, attribute, value, stringLength);
}

void
HandleEnv::sqlGetEnvAttr(Ctx& ctx, SQLINTEGER attribute, SQLPOINTER value, SQLINTEGER bufferLength, SQLINTEGER* stringLength)
{
    baseGetHandleAttr(ctx, m_attrArea, attribute, value, bufferLength, stringLength);
}

int
HandleEnv::getOdbcVersion(Ctx& ctx)
{
    OdbcData data;
    m_attrArea.getAttr(ctx, SQL_ATTR_ODBC_VERSION, data);
    if (! ctx.ok())
	return -1;
    return data.integer();
}

// transactions

void
HandleEnv::sqlEndTran(Ctx& ctx, SQLSMALLINT completionType)
{
    ctx_assert(false);	// XXX
}

void
HandleEnv::sqlTransact(Ctx& ctx, SQLUSMALLINT completionType)
{
    ctx_assert(false);	// XXX
}
