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

#include "HandleBase.hpp"

HandleBase::~HandleBase()
{
    delete m_ctx;
    m_ctx = 0;
}

void
HandleBase::saveCtx(Ctx& ctx)
{
    delete m_ctx;
    m_ctx = &ctx;
}

// get diagnostics

void
HandleBase::sqlGetDiagField(Ctx& ctx, SQLSMALLINT recNumber, SQLSMALLINT diagIdentifier, SQLPOINTER diagInfo, SQLSMALLINT bufferLength, SQLSMALLINT* stringLength)
{
    const DiagSpec& spec = DiagSpec::find(diagIdentifier);
    if (spec.m_pos == Diag_pos_end) {
	ctx.setCode(SQL_ERROR);
	return;
    }
    const bool header = (spec.m_pos == Diag_pos_header);
    const bool status = (spec.m_pos == Diag_pos_status);
    ctx_assert(header || status);
    if (! (spec.m_handles & odbcHandle())) {
	ctx.setCode(SQL_ERROR);
	return;
    }
    if (header) {
	recNumber = 0;		// ignored for header fields, so fix it
	if (m_ctx == 0) {
	    if (diagIdentifier == SQL_DIAG_NUMBER) {
		SQLINTEGER n = 0;
		OdbcData data(n);
		data.copyout(ctx, diagInfo, bufferLength, 0, stringLength);
		return;
	    }
	    if (diagIdentifier == SQL_DIAG_RETURNCODE) {
		SQLSMALLINT n = 0;
		OdbcData data(n);
		data.copyout(ctx, diagInfo, bufferLength, 0, stringLength);
		return;
	    }
	    return;
	}
    }
    if (status) {
	if (recNumber <= 0) {
	    ctx.setCode(SQL_ERROR);
	    return;
	}
	if (m_ctx == 0) {
	    if ((unsigned)recNumber > 0) {
		ctx.setCode(SQL_NO_DATA);
		return;
	    }
	    return;
	}
	if ((unsigned)recNumber > m_ctx->diagArea().numStatus()) {
	    ctx.setCode(SQL_NO_DATA);
	    return;
	}
    }
    OdbcData data;
    ctx_assert(m_ctx != 0);
    m_ctx->diagArea().getRecord(ctx, recNumber, diagIdentifier, data);
    if (data.type() != OdbcData::Undef)
	data.copyout(ctx, diagInfo, bufferLength, 0, stringLength);
}

void
HandleBase::sqlGetDiagRec(Ctx& ctx, SQLSMALLINT recNumber, SQLCHAR* sqlstate, SQLINTEGER* nativeError, SQLCHAR* messageText, SQLSMALLINT bufferLength, SQLSMALLINT* textLength)
{
    sqlGetDiagField(ctx, recNumber, SQL_DIAG_SQLSTATE, static_cast<SQLPOINTER>(sqlstate), 5 + 1, 0);
    sqlGetDiagField(ctx, recNumber, SQL_DIAG_NATIVE, static_cast<SQLPOINTER>(nativeError), -1, 0);
    sqlGetDiagField(ctx, recNumber, SQL_DIAG_MESSAGE_TEXT, static_cast<SQLPOINTER>(messageText), bufferLength, textLength);
}

void
HandleBase::sqlError(Ctx& ctx, SQLCHAR* sqlstate, SQLINTEGER* nativeError, SQLCHAR* messageText, SQLSMALLINT bufferLength, SQLSMALLINT* textLength)
{
    if (m_ctx == 0) {
	ctx.setCode(SQL_NO_DATA);
	return;
    }
    const SQLSMALLINT recNumber = m_ctx->diagArea().nextRecNumber();
    if (recNumber == 0) {
	ctx.setCode(SQL_NO_DATA);
	return;
    }
    sqlGetDiagField(ctx, recNumber, SQL_DIAG_SQLSTATE, static_cast<SQLPOINTER>(sqlstate), 5 + 1, 0);
    sqlGetDiagField(ctx, recNumber, SQL_DIAG_NATIVE, static_cast<SQLPOINTER>(nativeError), -1, 0);
    sqlGetDiagField(ctx, recNumber, SQL_DIAG_MESSAGE_TEXT, static_cast<SQLPOINTER>(messageText), bufferLength, textLength);
}

// common code for attributes

void
HandleBase::baseSetHandleAttr(Ctx& ctx, AttrArea& attrArea, SQLINTEGER attribute, SQLPOINTER value, SQLINTEGER stringLength)
{
    const AttrSpec& spec = attrArea.findSpec(attribute);
    if (spec.m_mode == Attr_mode_undef) {	// not found
	ctx.pushStatus(Sqlstate::_HY092, Error::Gen, "undefined attribute id %d", (int)attribute);
	return;
    }
    if (spec.m_mode == Attr_mode_readonly) {	// read only
	ctx.pushStatus(Sqlstate::_HY092, Error::Gen, "read-only attribute id %d", (int)attribute);
	return;
    }
    OdbcData data;
    data.copyin(ctx, spec.m_type, value, stringLength);
    if (! ctx.ok())
	return;
    attrArea.setAttr(ctx, attribute, data);
}

void
HandleBase::baseGetHandleAttr(Ctx& ctx, AttrArea& attrArea, SQLINTEGER attribute, SQLPOINTER value, SQLINTEGER bufferLength, SQLINTEGER* stringLength)
{
    const AttrSpec& spec = attrArea.findSpec(attribute);
    if (spec.m_mode == Attr_mode_undef) {	// not found
	ctx.pushStatus(Sqlstate::_HY092, Error::Gen, "undefined attribute id %d", (int)attribute);
	return;
    }
    OdbcData data;
    attrArea.getAttr(ctx, attribute, data);
    if (! ctx.ok())
	return;
    data.copyout(ctx, value, bufferLength, stringLength);
}

void
HandleBase::baseSetHandleOption(Ctx& ctx, AttrArea& attrArea, SQLUSMALLINT option, SQLUINTEGER value)
{
    baseSetHandleAttr(ctx, attrArea, static_cast<SQLINTEGER>(option), reinterpret_cast<SQLPOINTER>(value), 0);
}

void
HandleBase::baseGetHandleOption(Ctx& ctx, AttrArea& attrArea, SQLUSMALLINT option, SQLPOINTER value)
{
    baseGetHandleAttr(ctx, attrArea, static_cast<SQLINTEGER>(option), value, SQL_NTS, 0);
}
