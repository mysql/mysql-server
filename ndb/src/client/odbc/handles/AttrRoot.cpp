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

#include "HandleRoot.hpp"

static void
callback_SQL_ATTR_CONNECTION_POOLING_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleRoot* pRoot = static_cast<HandleRoot*>(self);
    ctx_assert(pRoot != 0 && data.type() == OdbcData::Uinteger);
    SQLUINTEGER value = data.uinteger();
    switch (value) {
    case SQL_CP_OFF:
	break;
    case SQL_CP_ONE_PER_DRIVER:
    case SQL_CP_ONE_PER_HENV:
	ctx.pushStatus(Sqlstate::_HYC00, Error::Gen, "connection pooling not supported");
	break;
    default:
	ctx.pushStatus(Sqlstate::_HY024, Error::Gen, "invalid connection pooling value %u", (unsigned)value);
	break;
    }
}

static void
callback_SQL_ATTR_CONNECTION_POOLING_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleRoot* pRoot = static_cast<HandleRoot*>(self);
    ctx_assert(pRoot != 0);
    SQLUINTEGER value = SQL_CP_OFF;
    data.setValue(value);
}

static void
callback_SQL_ATTR_CP_MATCH_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleRoot* pRoot = static_cast<HandleRoot*>(self);
    ctx_assert(pRoot != 0 && data.type() == OdbcData::Uinteger);
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
    HandleRoot* pRoot = static_cast<HandleRoot*>(self);
    ctx_assert(pRoot != 0);
    SQLUINTEGER value = SQL_CP_STRICT_MATCH;
    data.setValue(value);
}

AttrSpec HandleRoot::m_attrSpec[] = {
    {   SQL_ATTR_CONNECTION_POOLING,
        OdbcData::Uinteger,
        Attr_mode_readwrite,
        callback_SQL_ATTR_CONNECTION_POOLING_set,
        callback_SQL_ATTR_CONNECTION_POOLING_default,
    },
    {   SQL_ATTR_CP_MATCH,
        OdbcData::Uinteger,
        Attr_mode_readwrite,
        callback_SQL_ATTR_CP_MATCH_set,
        callback_SQL_ATTR_CP_MATCH_default,
    },
    {   0,
        OdbcData::Undef,
        Attr_mode_undef,
        0,
        0,
    },
};
