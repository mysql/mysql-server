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

#include "HandleEnv.hpp"

static void
callback_SQL_ATTR_CP_MATCH_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleEnv* pEnv = static_cast<HandleEnv*>(self);
    ctx_assert(pEnv != 0 && data.type() == OdbcData::Uinteger);
    SQLUINTEGER value = data.uinteger();
    switch (value) {
    case SQL_CP_STRICT_MATCH:
	break;
    case SQL_CP_RELAXED_MATCH:
	break;
    default:
	ctx.pushStatus(Sqlstate::_HY024, Error::Gen, "invalid cp match value %u", (unsigned)value);
	break;
    }
}

static void
callback_SQL_ATTR_CP_MATCH_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleEnv* pEnv = static_cast<HandleEnv*>(self);
    ctx_assert(pEnv != 0);
    SQLUINTEGER value = SQL_CP_STRICT_MATCH;
    data.setValue(value);
}

static void
callback_SQL_ATTR_ODBC_VERSION_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleEnv* pEnv = static_cast<HandleEnv*>(self);
    ctx_assert(pEnv != 0 && data.type() == OdbcData::Integer);
    int version = data.integer();
    switch (version) {
    case SQL_OV_ODBC2:
    case SQL_OV_ODBC3:
	break;
    default:
	ctx.pushStatus(Sqlstate::_HY024, Error::Gen, "invalid ODBC version %d", version);
	return;
    }
    ctx_log1(("odbc version set to %d", version));
}

static void
callback_SQL_ATTR_ODBC_VERSION_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleEnv* pEnv = static_cast<HandleEnv*>(self);
    ctx_assert(pEnv != 0);
    // no default
    ctx_log1(("odbc version has not been set"));
    ctx.pushStatus(Sqlstate::_HY010, Error::Gen, "odbc version has not been set");
}

static void
callback_SQL_ATTR_OUTPUT_NTS_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleEnv* pEnv = static_cast<HandleEnv*>(self);
    ctx_assert(pEnv != 0 && data.type() == OdbcData::Integer);
    SQLINTEGER value = data.integer();
    switch (value) {
    case SQL_TRUE:
	break;
    case SQL_FALSE:
	ctx.pushStatus(Sqlstate::_HYC00, Error::Gen, "output nts FALSE not supported");
	break;
    default:
	ctx.pushStatus(Sqlstate::_HY024, Error::Gen, "invalid output nts value %u", (unsigned)value);
	break;
    }
}

static void
callback_SQL_ATTR_OUTPUT_NTS_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleEnv* pEnv = static_cast<HandleEnv*>(self);
    ctx_assert(pEnv != 0);
    data.setValue();
}

AttrSpec HandleEnv::m_attrSpec[] = {
    {   SQL_ATTR_CP_MATCH,
        OdbcData::Uinteger,
        Attr_mode_readwrite,
        callback_SQL_ATTR_CP_MATCH_set,
        callback_SQL_ATTR_CP_MATCH_default,
    },
    {   SQL_ATTR_ODBC_VERSION,
        OdbcData::Integer,
        Attr_mode_readwrite,
        callback_SQL_ATTR_ODBC_VERSION_set,
        callback_SQL_ATTR_ODBC_VERSION_default,
    },
    {   SQL_ATTR_OUTPUT_NTS,
        OdbcData::Integer,
        Attr_mode_readwrite,
        callback_SQL_ATTR_OUTPUT_NTS_set,
        callback_SQL_ATTR_OUTPUT_NTS_default,
    },
    {   0,
        OdbcData::Undef,
        Attr_mode_undef,
        0,
        0,
    },
};
