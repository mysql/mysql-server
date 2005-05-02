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

#include "HandleDbc.hpp"

static void
callback_SQL_ATTR_ACCESS_MODE_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleDbc* pDbc = static_cast<HandleDbc*>(self);
    ctx_assert(pDbc != 0 && data.type() == OdbcData::Uinteger);
    SQLUINTEGER value = data.uinteger();
    switch (value) {
    case SQL_MODE_READ_ONLY:
	break;
    case SQL_MODE_READ_WRITE:
	break;
    default:
	ctx.pushStatus(Sqlstate::_HY024, Error::Gen, "invalid access mode value %u", (unsigned)value);
	break;
    }
}

static void
callback_SQL_ATTR_ACCESS_MODE_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleDbc* pDbc = static_cast<HandleDbc*>(self);
    ctx_assert(pDbc != 0);
    SQLUINTEGER value = SQL_MODE_READ_WRITE;
    data.setValue(value);
}

static void
callback_SQL_ATTR_ASYNC_ENABLE_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleDbc* pDbc = static_cast<HandleDbc*>(self);
    ctx_assert(pDbc != 0 && data.type() == OdbcData::Uinteger);
    SQLUINTEGER value = data.uinteger();
    switch (value) {
    case SQL_ASYNC_ENABLE_OFF:
	break;
    case SQL_ASYNC_ENABLE_ON:
	ctx.pushStatus(Sqlstate::_HYC00, Error::Gen, "async enable on not supported");
	break;
    default:
	ctx.pushStatus(Sqlstate::_HY024, Error::Gen, "invalid async enable value %u", (unsigned)value);
	break;
    }
}

static void
callback_SQL_ATTR_ASYNC_ENABLE_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleDbc* pDbc = static_cast<HandleDbc*>(self);
    ctx_assert(pDbc != 0);
    SQLUINTEGER value = SQL_ASYNC_ENABLE_OFF;
    data.setValue(value);
}

static void
callback_SQL_ATTR_AUTO_IPD_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleDbc* pDbc = static_cast<HandleDbc*>(self);
    ctx_assert(pDbc != 0 && data.type() == OdbcData::Uinteger);
    ctx_assert(false);		// read-only
}

static void
callback_SQL_ATTR_AUTO_IPD_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleDbc* pDbc = static_cast<HandleDbc*>(self);
    ctx_assert(pDbc != 0);
    SQLUINTEGER value = SQL_FALSE;
    data.setValue(value);
}

static void
callback_SQL_ATTR_AUTOCOMMIT_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleDbc* pDbc = static_cast<HandleDbc*>(self);
    ctx_assert(pDbc != 0 && data.type() == OdbcData::Uinteger);
    SQLUINTEGER value = data.uinteger();
    switch (value) {
    case SQL_AUTOCOMMIT_OFF:
	if (pDbc->autocommit()) {
	    pDbc->autocommit(false);
	    pDbc->useConnection(ctx, true);
	}
	break;
    case SQL_AUTOCOMMIT_ON:
	if (! pDbc->autocommit()) {
	    pDbc->autocommit(true);
	    pDbc->sqlEndTran(ctx, SQL_COMMIT);
	    pDbc->useConnection(ctx, false);
	}
	break;
    default:
	ctx.pushStatus(Sqlstate::_HY024, Error::Gen, "invalid autocommit value %u", (unsigned)value);
	break;
    }
}

static void
callback_SQL_ATTR_AUTOCOMMIT_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleDbc* pDbc = static_cast<HandleDbc*>(self);
    ctx_assert(pDbc != 0);
    SQLUINTEGER value = pDbc->autocommit() ? SQL_AUTOCOMMIT_ON : SQL_AUTOCOMMIT_OFF;
    data.setValue(value);
}

static void
callback_SQL_ATTR_CONNECTION_DEAD_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleDbc* pDbc = static_cast<HandleDbc*>(self);
    ctx_assert(pDbc != 0 && data.type() == OdbcData::Uinteger);
    ctx_assert(false);		// read-only
}

static void
callback_SQL_ATTR_CONNECTION_DEAD_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleDbc* pDbc = static_cast<HandleDbc*>(self);
    ctx_assert(pDbc != 0);
    SQLUINTEGER value = pDbc->getState() == HandleDbc::Free ? SQL_CD_TRUE : SQL_CD_FALSE;
    data.setValue(value);
}

static void
callback_SQL_ATTR_CONNECTION_TIMEOUT_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleDbc* pDbc = static_cast<HandleDbc*>(self);
    ctx_assert(pDbc != 0 && data.type() == OdbcData::Uinteger);
}

static void
callback_SQL_ATTR_CONNECTION_TIMEOUT_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleDbc* pDbc = static_cast<HandleDbc*>(self);
    ctx_assert(pDbc != 0);
    SQLUINTEGER value = 0;
    data.setValue(value);
}

static void
callback_SQL_ATTR_CURRENT_CATALOG_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleDbc* pDbc = static_cast<HandleDbc*>(self);
    ctx_assert(pDbc != 0 && data.type() == OdbcData::Sqlchar);
}

static void
callback_SQL_ATTR_CURRENT_CATALOG_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleDbc* pDbc = static_cast<HandleDbc*>(self);
    ctx_assert(pDbc != 0);
    const char* value = "DEFAULT";
    data.setValue(value);
}

static void
callback_SQL_ATTR_LOGIN_TIMEOUT_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    callback_SQL_ATTR_CONNECTION_TIMEOUT_set(ctx, self, data);
}

static void
callback_SQL_ATTR_LOGIN_TIMEOUT_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    callback_SQL_ATTR_CONNECTION_TIMEOUT_default(ctx, self, data);
}

static void
callback_SQL_ATTR_METADATA_ID_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleDbc* pDbc = static_cast<HandleDbc*>(self);
    ctx_assert(pDbc != 0 && data.type() == OdbcData::Uinteger);
    SQLUINTEGER value = data.uinteger();
    switch (value) {
    case SQL_FALSE:
	break;
    case SQL_TRUE:
	break;
    default:
	ctx.pushStatus(Sqlstate::_HY024, Error::Gen, "invalid metadata id value %u", (unsigned)value);
	break;
    }
}

static void
callback_SQL_ATTR_METADATA_ID_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleDbc* pDbc = static_cast<HandleDbc*>(self);
    ctx_assert(pDbc != 0);
    SQLUINTEGER value = SQL_FALSE;
    data.setValue(value);
}

static void
callback_SQL_ATTR_ODBC_CURSORS_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleDbc* pDbc = static_cast<HandleDbc*>(self);
    ctx_assert(pDbc != 0 && data.type() == OdbcData::Uinteger);
    SQLUINTEGER value = data.uinteger();
    switch (value) {
    case SQL_CUR_USE_DRIVER:
	ctx.pushStatus(Sqlstate::_HYC00, Error::Gen, "cur use driver not supported");
	break;
    case SQL_CUR_USE_IF_NEEDED:
	ctx.pushStatus(Sqlstate::_HYC00, Error::Gen, "cur use if needed not supported");
	break;
    case SQL_CUR_USE_ODBC:
	break;
    default:
	ctx.pushStatus(Sqlstate::_HY024, Error::Gen, "invalid odbc cursors value %u", (unsigned)value);
	break;
    }
}

static void
callback_SQL_ATTR_ODBC_CURSORS_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleDbc* pDbc = static_cast<HandleDbc*>(self);
    ctx_assert(pDbc != 0);
    SQLUINTEGER value = SQL_CUR_USE_ODBC;
    data.setValue(value);
}

static void
callback_SQL_ATTR_PACKET_SIZE_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleDbc* pDbc = static_cast<HandleDbc*>(self);
    ctx_assert(pDbc != 0 && data.type() == OdbcData::Uinteger);
    SQLUINTEGER value = data.uinteger();
    ctx.pushStatus(Sqlstate::_HYC00, Error::Gen, "packet size (%u) not supported", (unsigned)value);
}

static void
callback_SQL_ATTR_PACKET_SIZE_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleDbc* pDbc = static_cast<HandleDbc*>(self);
    ctx_assert(pDbc != 0);
    SQLUINTEGER value = 0;
    data.setValue(value);
}

static void
callback_SQL_ATTR_QUIET_MODE_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleDbc* pDbc = static_cast<HandleDbc*>(self);
    ctx_assert(pDbc != 0 && data.type() == OdbcData::Pointer);
}

static void
callback_SQL_ATTR_QUIET_MODE_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleDbc* pDbc = static_cast<HandleDbc*>(self);
    ctx_assert(pDbc != 0);
    data.setValue();
}

static void
callback_SQL_ATTR_TRACE_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleDbc* pDbc = static_cast<HandleDbc*>(self);
    ctx_assert(pDbc != 0 && data.type() == OdbcData::Uinteger);
}

static void
callback_SQL_ATTR_TRACE_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleDbc* pDbc = static_cast<HandleDbc*>(self);
    ctx_assert(pDbc != 0);
    data.setValue();
}

static void
callback_SQL_ATTR_TRACEFILE_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleDbc* pDbc = static_cast<HandleDbc*>(self);
    ctx_assert(pDbc != 0 && data.type() == OdbcData::Sqlchar);
}

static void
callback_SQL_ATTR_TRACEFILE_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleDbc* pDbc = static_cast<HandleDbc*>(self);
    ctx_assert(pDbc != 0);
    data.setValue();
}

static void
callback_SQL_ATTR_TRANSLATE_LIB_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleDbc* pDbc = static_cast<HandleDbc*>(self);
    ctx_assert(pDbc != 0 && data.type() == OdbcData::Sqlchar);
}

static void
callback_SQL_ATTR_TRANSLATE_LIB_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleDbc* pDbc = static_cast<HandleDbc*>(self);
    ctx_assert(pDbc != 0);
    data.setValue();
}

static void
callback_SQL_ATTR_TRANSLATE_OPTION_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleDbc* pDbc = static_cast<HandleDbc*>(self);
    ctx_assert(pDbc != 0 && data.type() == OdbcData::Uinteger);
}

static void
callback_SQL_ATTR_TRANSLATE_OPTION_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleDbc* pDbc = static_cast<HandleDbc*>(self);
    ctx_assert(pDbc != 0);
    data.setValue();
}

static void
callback_SQL_ATTR_TXN_ISOLATION_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleDbc* pDbc = static_cast<HandleDbc*>(self);
    ctx_assert(pDbc != 0 && data.type() == OdbcData::Uinteger);
    if (pDbc->getState() == HandleDbc::Free) {
	ctx.pushStatus(Sqlstate::_08003, Error::Gen, "not connected");
	return;
    }
    if (pDbc->getState() == HandleDbc::Transacting) {
	ctx.pushStatus(Sqlstate::_HY011, Error::Gen, "transaction is open");
	return;
    }
    SQLUINTEGER value = data.uinteger();
    SQLUINTEGER mask = SQL_TXN_READ_COMMITTED;
    if (! (value & mask)) {
	ctx.pushStatus(Sqlstate::_HYC00, Error::Gen, "txn isolation level %u not supported", (unsigned)value);
	return;
    }
}

static void
callback_SQL_ATTR_TXN_ISOLATION_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleDbc* pDbc = static_cast<HandleDbc*>(self);
    ctx_assert(pDbc != 0);
    SQLUINTEGER value = SQL_TXN_READ_COMMITTED;
    data.setValue(value);
}

AttrSpec HandleDbc::m_attrSpec[] = {
    {   SQL_ATTR_ACCESS_MODE,
        OdbcData::Uinteger,
        Attr_mode_readwrite,
        callback_SQL_ATTR_ACCESS_MODE_set,
        callback_SQL_ATTR_ACCESS_MODE_default,
    },
    {   SQL_ATTR_ASYNC_ENABLE,
        OdbcData::Uinteger,
        Attr_mode_readwrite,
        callback_SQL_ATTR_ASYNC_ENABLE_set,
        callback_SQL_ATTR_ASYNC_ENABLE_default,
    },
    {   SQL_ATTR_AUTO_IPD,
        OdbcData::Uinteger,
        Attr_mode_readonly,
        callback_SQL_ATTR_AUTO_IPD_set,
        callback_SQL_ATTR_AUTO_IPD_default,
    },
    {   SQL_ATTR_AUTOCOMMIT,
        OdbcData::Uinteger,
        Attr_mode_readwrite,
        callback_SQL_ATTR_AUTOCOMMIT_set,
        callback_SQL_ATTR_AUTOCOMMIT_default,
    },
    {   SQL_ATTR_CONNECTION_DEAD,
        OdbcData::Uinteger,
        Attr_mode_readonly,
        callback_SQL_ATTR_CONNECTION_DEAD_set,
        callback_SQL_ATTR_CONNECTION_DEAD_default,
    },
    {   SQL_ATTR_CONNECTION_TIMEOUT,
        OdbcData::Uinteger,
        Attr_mode_readwrite,
        callback_SQL_ATTR_CONNECTION_TIMEOUT_set,
        callback_SQL_ATTR_CONNECTION_TIMEOUT_default,
    },
    {   SQL_ATTR_CURRENT_CATALOG,
        OdbcData::Sqlchar,
        Attr_mode_readwrite,
        callback_SQL_ATTR_CURRENT_CATALOG_set,
        callback_SQL_ATTR_CURRENT_CATALOG_default,
    },
    {   SQL_ATTR_LOGIN_TIMEOUT,
        OdbcData::Uinteger,
        Attr_mode_readwrite,
        callback_SQL_ATTR_LOGIN_TIMEOUT_set,
        callback_SQL_ATTR_LOGIN_TIMEOUT_default,
    },
    {   SQL_ATTR_METADATA_ID,
        OdbcData::Uinteger,
        Attr_mode_readwrite,
        callback_SQL_ATTR_METADATA_ID_set,
        callback_SQL_ATTR_METADATA_ID_default,
    },
    {   SQL_ATTR_ODBC_CURSORS,
        OdbcData::Uinteger,
        Attr_mode_readwrite,
        callback_SQL_ATTR_ODBC_CURSORS_set,
        callback_SQL_ATTR_ODBC_CURSORS_default,
    },
    {   SQL_ATTR_PACKET_SIZE,
        OdbcData::Uinteger,
        Attr_mode_readwrite,
        callback_SQL_ATTR_PACKET_SIZE_set,
        callback_SQL_ATTR_PACKET_SIZE_default,
    },
    {   SQL_ATTR_QUIET_MODE,
        OdbcData::Pointer,
        Attr_mode_readwrite,
        callback_SQL_ATTR_QUIET_MODE_set,
        callback_SQL_ATTR_QUIET_MODE_default,
    },
    {   SQL_ATTR_TRACE,
        OdbcData::Uinteger,
        Attr_mode_readwrite,
        callback_SQL_ATTR_TRACE_set,
        callback_SQL_ATTR_TRACE_default,
    },
    {   SQL_ATTR_TRACEFILE,
        OdbcData::Sqlchar,
        Attr_mode_readwrite,
        callback_SQL_ATTR_TRACEFILE_set,
        callback_SQL_ATTR_TRACEFILE_default,
    },
    {   SQL_ATTR_TRANSLATE_LIB,
        OdbcData::Sqlchar,
        Attr_mode_readwrite,
        callback_SQL_ATTR_TRANSLATE_LIB_set,
        callback_SQL_ATTR_TRANSLATE_LIB_default,
    },
    {   SQL_ATTR_TRANSLATE_OPTION,
        OdbcData::Uinteger,
        Attr_mode_readwrite,
        callback_SQL_ATTR_TRANSLATE_OPTION_set,
        callback_SQL_ATTR_TRANSLATE_OPTION_default,
    },
    {   SQL_ATTR_TXN_ISOLATION,
        OdbcData::Uinteger,
        Attr_mode_readwrite,
        callback_SQL_ATTR_TXN_ISOLATION_set,
        callback_SQL_ATTR_TXN_ISOLATION_default,
    },
    {   0,
        OdbcData::Undef,
        Attr_mode_undef,
        0,
        0,
    },
};
