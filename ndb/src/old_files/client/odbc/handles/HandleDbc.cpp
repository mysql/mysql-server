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

#include <limits.h>
#include <NdbApi.hpp>
#include <common/common.hpp>
#include <common/DiagArea.hpp>
#include <common/StmtArea.hpp>
#include "HandleRoot.hpp"
#include "HandleEnv.hpp"
#include "HandleDbc.hpp"
#include "HandleStmt.hpp"
#include "HandleDesc.hpp"
#include "PoolNdb.hpp"

#ifndef INT_MAX
#define INT_MAX		2147483647
#endif

HandleDbc::HandleDbc(HandleEnv* pEnv) :
    m_env(pEnv),
    m_attrArea(m_attrSpec)
{
    m_attrArea.setHandle(this);
}

HandleDbc::~HandleDbc()
{
}

void
HandleDbc::ctor(Ctx& ctx)
{
}

void
HandleDbc::dtor(Ctx& ctx)
{
    if (m_state == Connected) {
	ctx.pushStatus(Sqlstate::_HY010, Error::Gen, "cannot delete connection handle - connection is open");
	return;
    }
    if (m_state == Transacting) {
	ctx.pushStatus(Sqlstate::_HY010, Error::Gen, "cannot delete connection handle - transaction is active");
	return;
    }
}

// allocate and free handles

void
HandleDbc::sqlAllocStmt(Ctx& ctx, HandleStmt** ppStmt)
{
    if (ppStmt == 0) {
	ctx.pushStatus(Sqlstate::_HY009, Error::Gen, "cannot allocate statement handle - null return address");
	return;
    }
    if (m_state == Free) {
	ctx.pushStatus(Sqlstate::_08003, Error::Gen, "cannot allocate statement handle - not connected to database");
	return;
    }
    HandleStmt* pStmt = new HandleStmt(this);
    pStmt->ctor(ctx);
    if (! ctx.ok()) {
	pStmt->dtor(ctx);
	delete pStmt;
	return;
    }
    m_listStmt.push_back(pStmt);
    getRoot()->record(SQL_HANDLE_STMT, pStmt, true);
    *ppStmt = pStmt;
}

void
HandleDbc::sqlAllocDesc(Ctx& ctx, HandleDesc** ppDesc)
{
    if (ppDesc == 0) {
	ctx.pushStatus(Sqlstate::_HY009, Error::Gen, "cannot allocate descriptor handle - null return address");
	return;
    }
    if (m_state == Free) {
	ctx.pushStatus(Sqlstate::_08003, Error::Gen, "cannot allocate descriptor handle - not connected to database");
	return;
    }
    HandleDesc* pDesc = new HandleDesc(this);
    pDesc->ctor(ctx);
    if (! ctx.ok()) {
	pDesc->dtor(ctx);
	delete pDesc;
	return;
    }
    m_listDesc.push_back(pDesc);
    getRoot()->record(SQL_HANDLE_DESC, pDesc, true);
    *ppDesc = pDesc;
}

void
HandleDbc::sqlAllocHandle(Ctx& ctx, SQLSMALLINT childType, HandleBase** ppChild)
{
    switch (childType) {
    case SQL_HANDLE_STMT:
	sqlAllocStmt(ctx, (HandleStmt**)ppChild);
	return;
    case SQL_HANDLE_DESC:
	sqlAllocDesc(ctx, (HandleDesc**)ppChild);
	return;
    }
    ctx.pushStatus(Sqlstate::_HY092, Error::Gen, "invalid child handle type %d", (int)childType);
}

void
HandleDbc::sqlFreeStmt(Ctx& ctx, HandleStmt* pStmt, SQLUSMALLINT iOption)
{
    switch (iOption) {
    case SQL_CLOSE:
	// no error if not open
	if (pStmt->getState() == HandleStmt::Open)
	    pStmt->sqlCloseCursor(ctx);
	return;
    case SQL_DROP:
	pStmt->dtor(ctx);
	if (! ctx.ok())
	    return;
	m_listStmt.remove(pStmt);
	getRoot()->record(SQL_HANDLE_STMT, pStmt, false);
	delete pStmt;
	return;
    case SQL_UNBIND: {
	DescArea& ard = pStmt->getHandleDesc(ctx, Desc_usage_ARD)->descArea();
	ard.setCount(ctx, 0);
	return;
	}
    case SQL_RESET_PARAMS: {
	DescArea& apd = pStmt->getHandleDesc(ctx, Desc_usage_APD)->descArea();
	apd.setCount(ctx, 0);
	// SQLFreeStmt doc misses this part
	DescArea& ipd = pStmt->getHandleDesc(ctx, Desc_usage_IPD)->descArea();
	ipd.setCount(ctx, 0);
	return;
	}
    }
    ctx.pushStatus(Sqlstate::_HY092, Error::Gen, "invalid free statement option %u", (unsigned)iOption);
}

void
HandleDbc::sqlFreeDesc(Ctx& ctx, HandleDesc* pDesc)
{
    pDesc->dtor(ctx);
    if (! ctx.ok())
	return;
    m_listDesc.remove(pDesc);
    getRoot()->record(SQL_HANDLE_DESC, pDesc, false);
    delete pDesc;
}

void
HandleDbc::sqlFreeHandle(Ctx& ctx, SQLSMALLINT childType, HandleBase* pChild)
{
    switch (childType) {
    case SQL_HANDLE_STMT:
	sqlFreeStmt(ctx, (HandleStmt*)pChild, SQL_DROP);
	return;
    case SQL_HANDLE_DESC:
	sqlFreeDesc(ctx, (HandleDesc*)pChild);
	return;
    }
    ctx.pushStatus(Sqlstate::_HY092, Error::Gen, "invalid child handle type %d", (int)childType);
}

// attributes and info functions

static bool
ignore_attr(Ctx& ctx, SQLINTEGER attribute)
{
    switch (attribute) {
    case 1246:
	ctx_log2(("ignore unknown ADO.NET connect attribute %d", (int)attribute));
	return true;
    }
    return false;
}

void
HandleDbc::sqlSetConnectAttr(Ctx& ctx, SQLINTEGER attribute, SQLPOINTER value, SQLINTEGER stringLength)
{
    if (ignore_attr(ctx, attribute))
	return;
    baseSetHandleAttr(ctx, m_attrArea, attribute, value, stringLength);
}

void
HandleDbc::sqlGetConnectAttr(Ctx& ctx, SQLINTEGER attribute, SQLPOINTER value, SQLINTEGER bufferLength, SQLINTEGER* stringLength)
{
    if (ignore_attr(ctx, attribute))
	return;
    baseGetHandleAttr(ctx, m_attrArea, attribute, value, bufferLength, stringLength);
}

void
HandleDbc::sqlSetConnectOption(Ctx& ctx, SQLUSMALLINT option, SQLUINTEGER value)
{
    if (ignore_attr(ctx, option))
	return;
    baseSetHandleOption(ctx, m_attrArea, option, value);
}

void
HandleDbc::sqlGetConnectOption(Ctx& ctx, SQLUSMALLINT option, SQLPOINTER value)
{
    if (ignore_attr(ctx, option))
	return;
    baseGetHandleOption(ctx, m_attrArea, option, value);
}

void
HandleDbc::sqlGetFunctions(Ctx& ctx, SQLUSMALLINT functionId, SQLUSMALLINT* supported)
{
    if (functionId == SQL_API_ALL_FUNCTIONS) {
	for (int i = 0; i < 100; i++)
	    supported[i] = SQL_FALSE;
	FuncTab* f;
	for (f = m_funcTab; f->m_supported != -1; f++) {
	    SQLUSMALLINT id = f->m_functionId;
	    if (id < 100 && f->m_supported)
		supported[id] = SQL_TRUE;
	}
    } else if (functionId == SQL_API_ODBC3_ALL_FUNCTIONS) {
	for (int i = 0; i < SQL_API_ODBC3_ALL_FUNCTIONS_SIZE; i++)
	    supported[i] = 0;
	FuncTab* f;
	for (f = m_funcTab; f->m_supported != -1; f++) {
	    SQLUSMALLINT id = f->m_functionId;
	    ctx_assert((id >> 4) < SQL_API_ODBC3_ALL_FUNCTIONS_SIZE);
	    if (f->m_supported)
		supported[id >> 4] |= (1 << (id & 0xf));
	}
    } else {
	FuncTab* f;
	for (f = m_funcTab; f->m_supported != -1; f++) {
	    if (f->m_functionId == functionId)
		break;
	}
	if (f->m_supported != -1)
	    supported[0] = f->m_supported ? SQL_TRUE : SQL_FALSE;
	else
	    ctx.pushStatus(Sqlstate::_HY095, Error::Gen, "invalid function id %u", (unsigned)functionId);
    }
}

void
HandleDbc::sqlGetInfo(Ctx& ctx, SQLUSMALLINT infoType, SQLPOINTER infoValue, SQLSMALLINT bufferLength, SQLSMALLINT* stringLength)
{
    InfoTab* f;
    for (f = m_infoTab; f->m_format != InfoTab::End; f++) {
	if (f->m_id == infoType)
	    break;
    }
    if (f->m_format == InfoTab::End) {
	ctx.pushStatus(Sqlstate::_HY096, Error::Gen, "invalid info type %u", (unsigned)infoType);
	return;
    }
    if (f->m_format == InfoTab::Char || f->m_format == InfoTab::YesNo) {
	ctx_log3(("SQLGetInfo: type=%u value='%s'", (unsigned)infoType, f->m_str));
	OdbcData data(f->m_str);
	data.copyout(ctx, infoValue, bufferLength, 0, stringLength);
	return;
    }
    if (f->m_format == InfoTab::Short) {
	ctx_log3(("SQLGetInfo: type=%u value=%d", (unsigned)infoType, (int)f->m_int));
	OdbcData data((SQLUSMALLINT)f->m_int);
	data.copyout(ctx, infoValue, 0, 0);
	return;
    }
    if (f->m_format == InfoTab::Long || f->m_format == InfoTab::Bitmask) {
	ctx_log3(("SQLGetInfo: type=%u value=0x%x", (unsigned)infoType, (int)f->m_int));
	OdbcData data((SQLUINTEGER)f->m_int);
	data.copyout(ctx, infoValue, 0, 0);
	return;
    }
    ctx_assert(false);
}

int
HandleDbc::getOdbcVersion(Ctx& ctx)
{
    return m_env->getOdbcVersion(ctx);
}

// connect and transactions

void
HandleDbc::sqlConnect(Ctx& ctx, SQLCHAR* serverName, SQLSMALLINT nameLength1, SQLCHAR* userName, SQLSMALLINT nameLength2, SQLCHAR* authentication, SQLSMALLINT nameLength3)
{
    if (m_state != Free) {
	ctx.pushStatus(Sqlstate::_HY010, Error::Gen, "already connected");
	return;
    }
    OdbcData data;
    m_attrArea.getAttr(ctx, SQL_ATTR_CONNECTION_TIMEOUT, data);
    int timeout = data.uinteger();
    if (timeout <= 0)
	timeout = INT_MAX;
    PoolNdb* poolNdb = getRoot()->getPoolNdb();
    Ndb* pNdb = poolNdb->allocate(ctx, timeout);
    if (pNdb == 0) {
	return;
    }
    m_ndbObject = pNdb;
    m_state = Connected;
    m_autocommit = true;
}

void
HandleDbc::sqlDriverConnect(Ctx& ctx, SQLHWND hwnd, SQLCHAR* szConnStrIn, SQLSMALLINT cbConnStrIn, SQLCHAR* szConnStrOut, SQLSMALLINT cbConnStrOutMax, SQLSMALLINT* pcbConnStrOut, SQLUSMALLINT fDriverCompletion)
{
    ctx_log2(("driver connect %.*s", cbConnStrIn, szConnStrIn == 0 ? "" : (char*)szConnStrIn));
    sqlConnect(ctx, (SQLCHAR*)"", 0, (SQLCHAR*)"", 0, (SQLCHAR*)"", 0);
    if (! ctx.ok())
	return;
    OdbcData data("DNS=DEFAULT");
    if (szConnStrOut != 0)	// ADO NET
	data.copyout(ctx, static_cast<SQLPOINTER>(szConnStrOut), cbConnStrOutMax, 0, pcbConnStrOut);
}

void
HandleDbc::sqlDisconnect(Ctx& ctx)
{
    if (m_state == Free) {
	ctx.pushStatus(Sqlstate::_HY010, Error::Gen, "already disconnected");
	return;
    }
    // XXX missing check for uncommited changes
    ListStmt::iterator i = m_listStmt.begin();
    while (i != m_listStmt.end()) {
	HandleStmt* pStmt = *i;
	ListStmt::iterator j = i++;
	pStmt->dtor(ctx);
	getRoot()->record(SQL_HANDLE_STMT, pStmt, false);
	delete pStmt;
	m_listStmt.erase(j);
    }
    PoolNdb* poolNdb = getRoot()->getPoolNdb();
    poolNdb->release(ctx, m_ndbObject);
    m_ndbObject = 0;
    m_state = Free;
    m_autocommit = true;
}

void
HandleDbc::sqlEndTran(Ctx& ctx, SQLSMALLINT completionType)
{
    if (completionType != SQL_COMMIT && completionType != SQL_ROLLBACK) {
	ctx.pushStatus(Sqlstate::_HY012, Error::Gen, "invalid completion type %d", (int)completionType);
	return;
    }
    if (m_state == Free) {
	ctx.pushStatus(Sqlstate::_08003, Error::Gen, "not connected");
	return;
    }
    Ndb* ndb = m_ndbObject;
    ctx_assert(ndb != 0);
    if (m_state == Connected) {
	ctx_log2(("sqlEndTran: no transaction active"));
	return;
    }
    NdbConnection* tcon = m_ndbConnection;
    ctx_assert(tcon != 0);
    if (completionType == SQL_COMMIT) {
	if (tcon->execute(Commit) == -1) {
	    if (tcon->getNdbError().code != 626)
		ctx.pushStatus(ndb, tcon, 0, "execute commit");
	    else
		ctx_log1(("ignore no data (626) at execute commit"));
	} else {
	    ctx_log2(("sqlEndTran: transaction commit done"));
	    m_uncommitted = false;
	}
    } else {
	if (tcon->execute(Rollback) == -1) {
	    if (tcon->getNdbError().code != 626)
		ctx.pushStatus(ndb, tcon, 0, "execute rollback");
	    else
		ctx_log1(("ignore no data (626) at execute rollback"));
	} else {
	    ctx_log2(("sqlEndTran: transaction rollback done"));
	    m_uncommitted = false;
	}
    }
    for (ListStmt::iterator i = m_listStmt.begin(); i != m_listStmt.end(); i++) {
	HandleStmt* pStmt = *i;
	if (pStmt->getState() == HandleStmt::Open) {
	    pStmt->sqlCloseCursor(ctx);		// SQL_CB_CLOSE behaviour
	}
	pStmt->useConnection(ctx, false);
    }
    if (! m_autocommit) {
	useConnection(ctx, false);
	useConnection(ctx, true);
    }
}

void
HandleDbc::sqlTransact(Ctx& ctx, SQLUSMALLINT completionType)
{
    sqlEndTran(ctx, completionType);
}
