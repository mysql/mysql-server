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
#include "HandleStmt.hpp"
#include "HandleDesc.hpp"

static void
callback_SQL_ATTR_APP_PARAM_DESC_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0 && data.type() == OdbcData::Pointer);
    pStmt->setHandleDesc(ctx, Desc_usage_APD, data.pointer());
}

static void
callback_SQL_ATTR_APP_PARAM_DESC_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0);
    HandleDesc* apd = pStmt->getHandleDesc(ctx, Desc_usage_APD);
    OdbcData value(reinterpret_cast<SQLPOINTER>(apd));
    data.setValue(value);
}

static void
callback_SQL_ATTR_APP_ROW_DESC_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0 && data.type() == OdbcData::Pointer);
    pStmt->setHandleDesc(ctx, Desc_usage_ARD, data.pointer());
}

static void
callback_SQL_ATTR_APP_ROW_DESC_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0);
    HandleDesc* ard = pStmt->getHandleDesc(ctx, Desc_usage_ARD);
    OdbcData value(reinterpret_cast<SQLPOINTER>(ard));
    data.setValue(value);
}

static void
callback_SQL_ATTR_ASYNC_ENABLE_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0 && data.type() == OdbcData::Uinteger);
    SQLUINTEGER value = data.uinteger();
    switch (value) {
    case SQL_ASYNC_ENABLE_OFF:
	break;
    case SQL_ASYNC_ENABLE_ON:
//	ctx.pushStatus(Sqlstate::_HYC00, Error::Gen, "async enable ON not supported");
	break;
    default:
	ctx.pushStatus(Sqlstate::_HY024, Error::Gen, "invalid async enable value %u", (unsigned)value);
	break;
    }
}

static void
callback_SQL_ATTR_ASYNC_ENABLE_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0);
    SQLUINTEGER value = SQL_ASYNC_ENABLE_OFF;
    data.setValue(value);
}

static void
callback_SQL_ATTR_CONCURRENCY_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0 && data.type() == OdbcData::Uinteger);
    SQLUINTEGER value = data.uinteger();
    switch (value) {
    case SQL_CONCUR_READ_ONLY:
	break;
    case SQL_CONCUR_LOCK:
	ctx.pushStatus(Sqlstate::_HYC00, Error::Gen, "concur lock not supported");
	break;
    case SQL_CONCUR_ROWVER:
	ctx.pushStatus(Sqlstate::_HYC00, Error::Gen, "concur rowver not supported");
	break;
    case SQL_CONCUR_VALUES:
	ctx.pushStatus(Sqlstate::_HYC00, Error::Gen, "concur values not supported");
	break;
    default:
	ctx.pushStatus(Sqlstate::_HY024, Error::Gen, "invalid concurrency value %u", (unsigned)value);
	break;
    }
}

static void
callback_SQL_ATTR_CONCURRENCY_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0);
    SQLUINTEGER value = SQL_CONCUR_READ_ONLY;
    data.setValue(value);
}

static void
callback_SQL_ATTR_CURSOR_SCROLLABLE_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0 && data.type() == OdbcData::Uinteger);
    SQLUINTEGER value = data.uinteger();
    switch (value) {
    case SQL_NONSCROLLABLE:
	break;
    case SQL_SCROLLABLE:
	ctx.pushStatus(Sqlstate::_HYC00, Error::Gen, "cursor scrollable not supported");
	break;
    default:
	ctx.pushStatus(Sqlstate::_HY024, Error::Gen, "invalid concurrency value %u", (unsigned)value);
	break;
    }
}

static void
callback_SQL_ATTR_CURSOR_SCROLLABLE_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0);
    SQLUINTEGER value = SQL_NONSCROLLABLE;
    data.setValue(value);
}

static void
callback_SQL_ATTR_CURSOR_SENSITIVITY_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0 && data.type() == OdbcData::Uinteger);
    SQLUINTEGER value = data.uinteger();
    switch (value) {
    case SQL_UNSPECIFIED:
    case SQL_INSENSITIVE:
	break;
    case SQL_SENSITIVE:
	ctx.pushStatus(Sqlstate::_HYC00, Error::Gen, "cursor sensitive not supported");
	break;
    default:
	ctx.pushStatus(Sqlstate::_HY024, Error::Gen, "invalid cursor sensitivity value %u", (unsigned)value);
	break;
    }
}

static void
callback_SQL_ATTR_CURSOR_SENSITIVITY_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0);
    SQLUINTEGER value = SQL_INSENSITIVE;
    data.setValue(value);
}

static void
callback_SQL_ATTR_CURSOR_TYPE_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0 && data.type() == OdbcData::Uinteger);
    SQLUINTEGER value = data.uinteger();
    switch (value) {
    case SQL_CURSOR_FORWARD_ONLY:
	break;
    case SQL_CURSOR_STATIC:
	ctx.pushStatus(Sqlstate::_HYC00, Error::Gen, "cursor static not supported");
	break;
    case SQL_CURSOR_KEYSET_DRIVEN:
	ctx.pushStatus(Sqlstate::_HYC00, Error::Gen, "cursor keyset driven not supported");
	break;
    case SQL_CURSOR_DYNAMIC:
	ctx.pushStatus(Sqlstate::_HYC00, Error::Gen, "cursor dynamic not supported");
	break;
    default:
	ctx.pushStatus(Sqlstate::_HY024, Error::Gen, "invalid cursor type value %u", (unsigned)value);
	break;
    }
}

static void
callback_SQL_ATTR_CURSOR_TYPE_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0);
    SQLUINTEGER value = SQL_CURSOR_FORWARD_ONLY;
    data.setValue(value);
}

static void
callback_SQL_ATTR_ENABLE_AUTO_IPD_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0 && data.type() == OdbcData::Uinteger);
    SQLUINTEGER value = data.uinteger();
    switch (value) {
    case SQL_FALSE:
	break;
    case SQL_TRUE:
	ctx.pushStatus(Sqlstate::_HYC00, Error::Gen, "enable auto IPD not supported");
	break;
    default:
	ctx.pushStatus(Sqlstate::_HY024, Error::Gen, "invalid enable auto IPD value %u", (unsigned)value);
	break;
    }
}

static void
callback_SQL_ATTR_ENABLE_AUTO_IPD_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0);
    SQLUINTEGER value = SQL_FALSE;
    data.setValue(value);
}

static void
callback_SQL_ATTR_FETCH_BOOKMARK_PTR_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0 && data.type() == OdbcData::Pointer);
    SQLPOINTER value = data.pointer();
    if (value != 0) {
	ctx.pushStatus(Sqlstate::_HYC00, Error::Gen, "fetch bookmark ptr not supported");
	return;
    }
}

static void
callback_SQL_ATTR_FETCH_BOOKMARK_PTR_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0);
    SQLPOINTER value = 0;
    data.setValue(value);
}

static void
callback_SQL_ATTR_IMP_PARAM_DESC_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0 && data.type() == OdbcData::Pointer);
    ctx_assert(false);		// read-only
}

static void
callback_SQL_ATTR_IMP_PARAM_DESC_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0);
    HandleDesc* ipd = pStmt->getHandleDesc(ctx, Desc_usage_IPD);
    OdbcData value(reinterpret_cast<SQLPOINTER>(ipd));
    data.setValue(value);
}

static void
callback_SQL_ATTR_IMP_ROW_DESC_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0 && data.type() == OdbcData::Pointer);
    ctx_assert(false);		// read-only
}

static void
callback_SQL_ATTR_IMP_ROW_DESC_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0);
    HandleDesc* ird = pStmt->getHandleDesc(ctx, Desc_usage_IRD);
    OdbcData value(reinterpret_cast<SQLPOINTER>(ird));
    data.setValue(value);
}

static void
callback_SQL_ATTR_KEYSET_SIZE_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0 && data.type() == OdbcData::Uinteger);
    SQLUINTEGER value = data.uinteger();
    if (value != 0) {
	ctx.pushStatus(Sqlstate::_HYC00, Error::Gen, "keyset size not supported");
	return;
    }
}

static void
callback_SQL_ATTR_KEYSET_SIZE_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0);
    SQLUINTEGER value = 0;
    data.setValue(value);
}

static void
callback_SQL_ATTR_MAX_LENGTH_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0 && data.type() == OdbcData::Uinteger);
    SQLUINTEGER value = data.uinteger();
    if (value != 0) {
	ctx.pushStatus(Sqlstate::_HYC00, Error::Gen, "max length not supported");
	return;
    }
}

static void
callback_SQL_ATTR_MAX_LENGTH_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0);
    SQLUINTEGER value = 0;
    data.setValue(value);
}

static void
callback_SQL_ATTR_MAX_ROWS_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0 && data.type() == OdbcData::Uinteger);
    SQLUINTEGER value = data.uinteger();
    if (value != 0) {
	ctx.pushStatus(Sqlstate::_HYC00, Error::Gen, "max rows not supported");
	return;
    }
}

static void
callback_SQL_ATTR_MAX_ROWS_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0);
    SQLUINTEGER value = 0;
    data.setValue(value);
}

static void
callback_SQL_ATTR_METADATA_ID_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0 && data.type() == OdbcData::Uinteger);
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
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0);
    SQLUINTEGER value = SQL_FALSE;
    data.setValue(value);
}

static void
callback_SQL_ATTR_NOSCAN_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0 && data.type() == OdbcData::Uinteger);
    SQLUINTEGER value = data.uinteger();
    switch (value) {
    case SQL_NOSCAN_OFF:
	ctx.pushStatus(Sqlstate::_HYC00, Error::Gen, "noscan OFF not supported");
	break;
    case SQL_NOSCAN_ON:
	break;
    default:
	ctx.pushStatus(Sqlstate::_HY024, Error::Gen, "invalid no scan value %u", (unsigned)value);
	break;
    }
}

static void
callback_SQL_ATTR_NOSCAN_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0);
    SQLUINTEGER value = SQL_NOSCAN_ON;
    data.setValue(value);
}

static void
callback_SQL_ATTR_PARAM_BIND_OFFSET_PTR_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0 && data.type() == OdbcData::UintegerPtr);
    SQLUINTEGER* value = data.uintegerPtr();
    if (value != 0) {
	ctx.pushStatus(Sqlstate::_HYC00, Error::Gen, "param bind offset ptr not supported");
	return;
    }
}

static void
callback_SQL_ATTR_PARAM_BIND_OFFSET_PTR_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0);
    SQLUINTEGER* value = 0;
    data.setValue(value);
}

static void
callback_SQL_ATTR_PARAM_BIND_TYPE_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0 && data.type() == OdbcData::Uinteger);
    SQLUINTEGER value = data.uinteger();
    if (value != SQL_PARAM_BIND_BY_COLUMN) {
	ctx.pushStatus(Sqlstate::_HYC00, Error::Gen, "row-wise param binding not supported");
	return;
    }
}

static void
callback_SQL_ATTR_PARAM_BIND_TYPE_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0);
    SQLUINTEGER value = SQL_PARAM_BIND_BY_COLUMN;
    data.setValue(value);
}

static void
callback_SQL_ATTR_PARAM_OPERATION_PTR_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0 && data.type() == OdbcData::UsmallintPtr);
    SQLUSMALLINT* value = data.usmallintPtr();
    if (value != 0) {
	ctx.pushStatus(Sqlstate::_HYC00, Error::Gen, "param operation ptr not supported");
	return;
    }
}

static void
callback_SQL_ATTR_PARAM_OPERATION_PTR_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0);
    SQLUSMALLINT* value = 0;
    data.setValue(value);
}

static void
callback_SQL_ATTR_PARAM_STATUS_PTR_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0 && data.type() == OdbcData::UsmallintPtr);
    SQLUSMALLINT* value = data.usmallintPtr();
    if (value != 0) {
	ctx.pushStatus(Sqlstate::_HYC00, Error::Gen, "param status ptr not supported");
	return;
    }
}

static void
callback_SQL_ATTR_PARAM_STATUS_PTR_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0);
    SQLUSMALLINT* value = 0;
    data.setValue(value);
}

static void
callback_SQL_ATTR_PARAMS_PROCESSED_PTR_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0 && data.type() == OdbcData::UintegerPtr);
    SQLUINTEGER* value = data.uintegerPtr();
    if (value != 0) {
	ctx.pushStatus(Sqlstate::_HYC00, Error::Gen, "params processed ptr not supported");
	return;
    }
}

static void
callback_SQL_ATTR_PARAMS_PROCESSED_PTR_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0);
    SQLUINTEGER* value = 0;
    data.setValue(value);
}

static void
callback_SQL_ATTR_PARAMSET_SIZE_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0 && data.type() == OdbcData::Uinteger);
    SQLUINTEGER value = data.uinteger();
    if (value != 1) {
	ctx.pushStatus(Sqlstate::_HYC00, Error::Gen, "paramset size %u not supported", (unsigned)value);
	return;
    }
}

static void
callback_SQL_ATTR_PARAMSET_SIZE_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0);
    SQLUINTEGER value = 1;
    data.setValue(value);
}

static void
callback_SQL_ATTR_QUERY_TIMEOUT_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0 && data.type() == OdbcData::Uinteger);
    SQLUINTEGER value = data.uinteger();
    if (value != 0) {
	ctx.pushStatus(Sqlstate::_01S02, Error::Gen, "query timeout %u replaced by 0", (unsigned)value);
	return;
    }
}

static void
callback_SQL_ATTR_QUERY_TIMEOUT_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0);
    SQLUINTEGER value = 0;
    data.setValue(value);
}

static void
callback_SQL_ATTR_RETRIEVE_DATA_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0 && data.type() == OdbcData::Uinteger);
    SQLUINTEGER value = data.uinteger();
    switch (value) {
    case SQL_RD_ON:
	break;
    case SQL_RD_OFF:
	ctx.pushStatus(Sqlstate::_HYC00, Error::Gen, "retrieve data OFF not supported");
	break;
    default:
	ctx.pushStatus(Sqlstate::_HY024, Error::Gen, "invalid retrieve data value %u", (unsigned)value);
	break;
    }
}

static void
callback_SQL_ATTR_RETRIEVE_DATA_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0);
    SQLUINTEGER value = SQL_RD_ON;
    data.setValue(value);
}

static void
callback_SQL_ATTR_ROW_ARRAY_SIZE_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0 && data.type() == OdbcData::Uinteger);
    SQLUINTEGER value = data.uinteger();
    if (value != 1) {
	ctx.pushStatus(Sqlstate::_HYC00, Error::Gen, "row array size %u != 1 not supported", (unsigned)value);
	return;
    }
}

static void
callback_SQL_ATTR_ROW_ARRAY_SIZE_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0);
    SQLUINTEGER value = 1;
    data.setValue(value);
}

static void
callback_SQL_ATTR_ROW_BIND_OFFSET_PTR_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0 && data.type() == OdbcData::UintegerPtr);
    SQLUINTEGER* value = data.uintegerPtr();
    if (value != 0) {
	ctx.pushStatus(Sqlstate::_HYC00, Error::Gen, "row bind offset ptr != 0 not supported");
	return;
    }
}

static void
callback_SQL_ATTR_ROW_BIND_OFFSET_PTR_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0);
    SQLUINTEGER* value = 0;
    data.setValue(value);
}

static void
callback_SQL_ATTR_ROW_BIND_TYPE_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0 && data.type() == OdbcData::Uinteger);
    SQLUINTEGER value = data.uinteger();
    if (value != SQL_BIND_BY_COLUMN) {
	ctx.pushStatus(Sqlstate::_HYC00, Error::Gen, "row-wise binding not supported");
	return;
    }
}

static void
callback_SQL_ATTR_ROW_BIND_TYPE_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0);
    SQLUINTEGER value = SQL_BIND_BY_COLUMN;
    data.setValue(value);
}

static void
callback_SQL_ATTR_ROW_NUMBER_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0 && data.type() == OdbcData::Uinteger);
    ctx_assert(false);		// read-only
}

static void
callback_SQL_ATTR_ROW_NUMBER_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0);
    SQLUINTEGER value = pStmt->getRowCount();
    data.setValue(value);
}

static void
callback_SQL_ATTR_ROW_OPERATION_PTR_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0 && data.type() == OdbcData::UsmallintPtr);
    SQLUSMALLINT* value = data.usmallintPtr();
    if (value != 0) {
	ctx.pushStatus(Sqlstate::_HYC00, Error::Gen, "row operation ptr not supported");
	return;
    }
}

static void
callback_SQL_ATTR_ROW_OPERATION_PTR_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0);
    SQLUSMALLINT* value = 0;
    data.setValue(value);
}

static void
callback_SQL_ATTR_ROW_STATUS_PTR_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0 && data.type() == OdbcData::UsmallintPtr);
    SQLUSMALLINT* value = data.usmallintPtr();
    if (value != 0) {
	ctx.pushStatus(Sqlstate::_HYC00, Error::Gen, "row status ptr not supported");
	return;
    }
}

static void
callback_SQL_ATTR_ROW_STATUS_PTR_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0);
    SQLUSMALLINT* value = 0;
    data.setValue(value);
}

static void
callback_SQL_ATTR_ROWS_FETCHED_PTR_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0 && data.type() == OdbcData::UintegerPtr);
    SQLUINTEGER* value = data.uintegerPtr();
    HandleDesc* ird = pStmt->getHandleDesc(ctx, Desc_usage_IRD);
    ird->sqlSetDescField(ctx, 0, SQL_DESC_ROWS_PROCESSED_PTR, static_cast<SQLPOINTER>(value), SQL_IS_POINTER);
}

static void
callback_SQL_ATTR_ROWS_FETCHED_PTR_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0);
    SQLUINTEGER* value = 0;
    HandleDesc* ird = pStmt->getHandleDesc(ctx, Desc_usage_IRD);
    ird->sqlGetDescField(ctx, 0, SQL_DESC_ROWS_PROCESSED_PTR, &value, SQL_IS_POINTER, 0);
    data.setValue(value);
}

static void
callback_SQL_ATTR_SIMULATE_CURSOR_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0 && data.type() == OdbcData::Uinteger);
    SQLUINTEGER value = data.uinteger();
    switch (value) {
    case SQL_SC_NON_UNIQUE:
	break;
    case SQL_SC_TRY_UNIQUE:
	break;
    case SQL_SC_UNIQUE:
	break;
    default:
	ctx.pushStatus(Sqlstate::_HY024, Error::Gen, "invalid simulate cursor value %u", (unsigned)value);
	break;
    }
}

static void
callback_SQL_ATTR_SIMULATE_CURSOR_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0);
    SQLUINTEGER value = SQL_SC_UNIQUE;	// XXX if we did
    data.setValue(value);
}

static void
callback_SQL_ATTR_USE_BOOKMARKS_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    SQLUINTEGER value = data.uinteger();
    switch (value) {
    case SQL_UB_OFF:
	break;
    case SQL_UB_VARIABLE:
    case SQL_UB_FIXED:
	 ctx.pushStatus(Sqlstate::_HYC00, Error::Gen, "bookmarks not supported");
	 return;
    }
}

static void
callback_SQL_ATTR_USE_BOOKMARKS_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0);
    SQLUINTEGER value = SQL_UB_OFF;
    data.setValue(value);
}

// driver specific

static void
callback_SQL_ATTR_NDB_TUPLES_FETCHED_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0 && data.type() == OdbcData::Uinteger);
    ctx_assert(false);		// read-only
}

static void
callback_SQL_ATTR_NDB_TUPLES_FETCHED_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleStmt* pStmt = static_cast<HandleStmt*>(self);
    ctx_assert(pStmt != 0);
    SQLUINTEGER value = pStmt->getTuplesFetched();
    data.setValue(value);
}

AttrSpec HandleStmt::m_attrSpec[] = {
    {   SQL_ATTR_APP_PARAM_DESC,
        OdbcData::Pointer,
        Attr_mode_readwrite,
        callback_SQL_ATTR_APP_PARAM_DESC_set,
        callback_SQL_ATTR_APP_PARAM_DESC_default,
    },
    {   SQL_ATTR_APP_ROW_DESC,
        OdbcData::Pointer,
        Attr_mode_readwrite,
        callback_SQL_ATTR_APP_ROW_DESC_set,
        callback_SQL_ATTR_APP_ROW_DESC_default,
    },
    {   SQL_ATTR_ASYNC_ENABLE,
        OdbcData::Uinteger,
        Attr_mode_readwrite,
        callback_SQL_ATTR_ASYNC_ENABLE_set,
        callback_SQL_ATTR_ASYNC_ENABLE_default,
    },
    {   SQL_ATTR_CONCURRENCY,
        OdbcData::Uinteger,
        Attr_mode_readwrite,
        callback_SQL_ATTR_CONCURRENCY_set,
        callback_SQL_ATTR_CONCURRENCY_default,
    },
    {   SQL_ATTR_CURSOR_SCROLLABLE,
        OdbcData::Uinteger,
        Attr_mode_readwrite,
        callback_SQL_ATTR_CURSOR_SCROLLABLE_set,
        callback_SQL_ATTR_CURSOR_SCROLLABLE_default,
    },
    {   SQL_ATTR_CURSOR_SENSITIVITY,
        OdbcData::Uinteger,
        Attr_mode_readwrite,
        callback_SQL_ATTR_CURSOR_SENSITIVITY_set,
        callback_SQL_ATTR_CURSOR_SENSITIVITY_default,
    },
    {   SQL_ATTR_CURSOR_TYPE,
        OdbcData::Uinteger,
        Attr_mode_readwrite,
        callback_SQL_ATTR_CURSOR_TYPE_set,
        callback_SQL_ATTR_CURSOR_TYPE_default,
    },
    {   SQL_ATTR_ENABLE_AUTO_IPD,
        OdbcData::Uinteger,
        Attr_mode_readwrite,
        callback_SQL_ATTR_ENABLE_AUTO_IPD_set,
        callback_SQL_ATTR_ENABLE_AUTO_IPD_default,
    },
    {   SQL_ATTR_FETCH_BOOKMARK_PTR,
        OdbcData::Pointer,
        Attr_mode_readwrite,
        callback_SQL_ATTR_FETCH_BOOKMARK_PTR_set,
        callback_SQL_ATTR_FETCH_BOOKMARK_PTR_default,
    },
    {   SQL_ATTR_IMP_PARAM_DESC,
        OdbcData::Pointer,
        Attr_mode_readonly,
        callback_SQL_ATTR_IMP_PARAM_DESC_set,
        callback_SQL_ATTR_IMP_PARAM_DESC_default,
    },
    {   SQL_ATTR_IMP_ROW_DESC,
        OdbcData::Pointer,
        Attr_mode_readonly,
        callback_SQL_ATTR_IMP_ROW_DESC_set,
        callback_SQL_ATTR_IMP_ROW_DESC_default,
    },
    {   SQL_ATTR_KEYSET_SIZE,
        OdbcData::Uinteger,
        Attr_mode_readwrite,
        callback_SQL_ATTR_KEYSET_SIZE_set,
        callback_SQL_ATTR_KEYSET_SIZE_default,
    },
    {   SQL_ATTR_MAX_LENGTH,
        OdbcData::Uinteger,
        Attr_mode_readwrite,
        callback_SQL_ATTR_MAX_LENGTH_set,
        callback_SQL_ATTR_MAX_LENGTH_default,
    },
    {   SQL_ATTR_MAX_ROWS,
        OdbcData::Uinteger,
        Attr_mode_readwrite,
        callback_SQL_ATTR_MAX_ROWS_set,
        callback_SQL_ATTR_MAX_ROWS_default,
    },
    {   SQL_ATTR_METADATA_ID,
        OdbcData::Uinteger,
        Attr_mode_readwrite,
        callback_SQL_ATTR_METADATA_ID_set,
        callback_SQL_ATTR_METADATA_ID_default,
    },
    {   SQL_ATTR_NOSCAN,
        OdbcData::Uinteger,
        Attr_mode_readwrite,
        callback_SQL_ATTR_NOSCAN_set,
        callback_SQL_ATTR_NOSCAN_default,
    },
    {   SQL_ATTR_PARAM_BIND_OFFSET_PTR,
        OdbcData::UintegerPtr,
        Attr_mode_readwrite,
        callback_SQL_ATTR_PARAM_BIND_OFFSET_PTR_set,
        callback_SQL_ATTR_PARAM_BIND_OFFSET_PTR_default,
    },
    {   SQL_ATTR_PARAM_BIND_TYPE,
        OdbcData::Uinteger,
        Attr_mode_readwrite,
        callback_SQL_ATTR_PARAM_BIND_TYPE_set,
        callback_SQL_ATTR_PARAM_BIND_TYPE_default,
    },
    {   SQL_ATTR_PARAM_OPERATION_PTR,
        OdbcData::UsmallintPtr,
        Attr_mode_readwrite,
        callback_SQL_ATTR_PARAM_OPERATION_PTR_set,
        callback_SQL_ATTR_PARAM_OPERATION_PTR_default,
    },
    {   SQL_ATTR_PARAM_STATUS_PTR,
        OdbcData::UsmallintPtr,
        Attr_mode_readwrite,
        callback_SQL_ATTR_PARAM_STATUS_PTR_set,
        callback_SQL_ATTR_PARAM_STATUS_PTR_default,
    },
    {   SQL_ATTR_PARAMS_PROCESSED_PTR,
        OdbcData::UintegerPtr,
        Attr_mode_readwrite,
        callback_SQL_ATTR_PARAMS_PROCESSED_PTR_set,
        callback_SQL_ATTR_PARAMS_PROCESSED_PTR_default,
    },
    {   SQL_ATTR_PARAMSET_SIZE,
        OdbcData::Uinteger,
        Attr_mode_readwrite,
        callback_SQL_ATTR_PARAMSET_SIZE_set,
        callback_SQL_ATTR_PARAMSET_SIZE_default,
    },
    {   SQL_ATTR_QUERY_TIMEOUT,
        OdbcData::Uinteger,
        Attr_mode_readwrite,
        callback_SQL_ATTR_QUERY_TIMEOUT_set,
        callback_SQL_ATTR_QUERY_TIMEOUT_default,
    },
    {   SQL_ATTR_RETRIEVE_DATA,
        OdbcData::Uinteger,
        Attr_mode_readwrite,
        callback_SQL_ATTR_RETRIEVE_DATA_set,
        callback_SQL_ATTR_RETRIEVE_DATA_default,
    },
    {   SQL_ATTR_ROW_ARRAY_SIZE,
        OdbcData::Uinteger,
        Attr_mode_readwrite,
        callback_SQL_ATTR_ROW_ARRAY_SIZE_set,
        callback_SQL_ATTR_ROW_ARRAY_SIZE_default,
    },
    {   SQL_ATTR_ROW_BIND_OFFSET_PTR,
        OdbcData::UintegerPtr,
        Attr_mode_readwrite,
        callback_SQL_ATTR_ROW_BIND_OFFSET_PTR_set,
        callback_SQL_ATTR_ROW_BIND_OFFSET_PTR_default,
    },
    {   SQL_ATTR_ROW_BIND_TYPE,
        OdbcData::Uinteger,
        Attr_mode_readwrite,
        callback_SQL_ATTR_ROW_BIND_TYPE_set,
        callback_SQL_ATTR_ROW_BIND_TYPE_default,
    },
    {   SQL_ATTR_ROW_NUMBER,
        OdbcData::Uinteger,
        Attr_mode_readonly,
        callback_SQL_ATTR_ROW_NUMBER_set,
        callback_SQL_ATTR_ROW_NUMBER_default,
    },
    {   SQL_ATTR_ROW_OPERATION_PTR,
        OdbcData::UsmallintPtr,
        Attr_mode_readwrite,
        callback_SQL_ATTR_ROW_OPERATION_PTR_set,
        callback_SQL_ATTR_ROW_OPERATION_PTR_default,
    },
    {   SQL_ATTR_ROW_STATUS_PTR,
        OdbcData::UsmallintPtr,
        Attr_mode_readwrite,
        callback_SQL_ATTR_ROW_STATUS_PTR_set,
        callback_SQL_ATTR_ROW_STATUS_PTR_default,
    },
    {   SQL_ATTR_ROWS_FETCHED_PTR,
        OdbcData::UintegerPtr,
        Attr_mode_readwrite,
        callback_SQL_ATTR_ROWS_FETCHED_PTR_set,
        callback_SQL_ATTR_ROWS_FETCHED_PTR_default,
    },
    {   SQL_ATTR_SIMULATE_CURSOR,
        OdbcData::Uinteger,
        Attr_mode_readwrite,
        callback_SQL_ATTR_SIMULATE_CURSOR_set,
        callback_SQL_ATTR_SIMULATE_CURSOR_default,
    },
    {   SQL_ATTR_USE_BOOKMARKS,
        OdbcData::Uinteger,
        Attr_mode_readwrite,
        callback_SQL_ATTR_USE_BOOKMARKS_set,
        callback_SQL_ATTR_USE_BOOKMARKS_default,
    },
    // driver specific
    {   SQL_ATTR_NDB_TUPLES_FETCHED,
        OdbcData::Uinteger,
        Attr_mode_readonly,
        callback_SQL_ATTR_NDB_TUPLES_FETCHED_set,
        callback_SQL_ATTR_NDB_TUPLES_FETCHED_default,
    },
    {   0,
        OdbcData::Undef,
        Attr_mode_undef,
        0,
        0,
    },
};
