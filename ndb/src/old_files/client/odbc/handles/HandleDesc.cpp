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

#include <common/OdbcData.hpp>
#include <common/DiagArea.hpp>
#include <common/DataType.hpp>
#include "HandleRoot.hpp"
#include "HandleDbc.hpp"
#include "HandleDesc.hpp"

HandleDesc::HandleDesc(HandleDbc* pDbc) :
    m_dbc(pDbc),
    m_descArea(this, m_descSpec)
{
}

HandleDesc::~HandleDesc()
{
}

void
HandleDesc::ctor(Ctx& ctx)
{
}

void
HandleDesc::dtor(Ctx& ctx)
{
}

// allocate and free handles (no valid case)

void
HandleDesc::sqlAllocHandle(Ctx& ctx, SQLSMALLINT childType, HandleBase** ppChild)
{
    ctx.pushStatus(Sqlstate::_HY092, Error::Gen, "inappropriate handle type");
}

void
HandleDesc::sqlFreeHandle(Ctx& ctx, SQLSMALLINT childType, HandleBase* ppChild)
{
    ctx.pushStatus(Sqlstate::_HY092, Error::Gen, "inappropriate handle type");
}

// set and get descriptor values

void
HandleDesc::sqlSetDescField(Ctx& ctx, SQLSMALLINT recNumber, SQLSMALLINT fieldIdentifier, SQLPOINTER value, SQLINTEGER bufferLength)
{
    const DescSpec& spec = m_descArea.findSpec(fieldIdentifier);
    if (spec.m_pos == Desc_pos_end) {
	ctx.pushStatus(Sqlstate::_HY091, Error::Gen, "invalid descriptor id %d", (int)fieldIdentifier);
	return;
    }
    OdbcData data;
    data.copyin(ctx, spec.m_type, value, bufferLength);
    if (! ctx.ok())
	return;
    const bool header = (spec.m_pos == Desc_pos_header);
    const bool record = (spec.m_pos == Desc_pos_record);
    ctx_assert(header || record);
    DescArea& area = m_descArea;
    if (header) {
	area.getHeader().setField(ctx, fieldIdentifier, data);
    }
    if (record) {
	if (recNumber < 0) {
	    ctx.pushStatus(Sqlstate::_07009, Error::Gen, "invalid record number %d", (int)recNumber);
	    return;
	}
	if (recNumber == 0) {	// bookmark record
	    if (area.getUsage() == Desc_usage_IPD) {
		ctx.pushStatus(Sqlstate::_07009, Error::Gen, "cannot set bookmark IPD");
		return;
	    }
	    if (area.getUsage() == Desc_usage_APD) {
		ctx.pushStatus(Sqlstate::_07009, Error::Gen, "cannot set bookmark APD");
		return;
	    }
	}
	area.getRecord(recNumber).setField(ctx, fieldIdentifier, data);
    }
}

void
HandleDesc::sqlGetDescField(Ctx& ctx, SQLSMALLINT recNumber, SQLSMALLINT fieldIdentifier, SQLPOINTER value, SQLINTEGER bufferLength, SQLINTEGER* stringLength, SQLSMALLINT* stringLength2)
{
    const DescSpec& spec = m_descArea.findSpec(fieldIdentifier);
    if (spec.m_pos == Desc_pos_end) {
	ctx.pushStatus(Sqlstate::_HY091, Error::Gen, "invalid descriptor id %d", (int)fieldIdentifier);
	return;
    }
    const bool header = (spec.m_pos == Desc_pos_header);
    const bool record = (spec.m_pos == Desc_pos_record);
    ctx_assert(header || record);
    DescArea& area = m_descArea;
    OdbcData data;
    if (header) {
	area.getHeader().getField(ctx, fieldIdentifier, data);
	if (! ctx.ok())
	    return;
    }
    if (record) {
	if (recNumber < 0) {
	    ctx.pushStatus(Sqlstate::_07009, Error::Gen, "invalid record number %d", (int)recNumber);
	    return;
	}
	if (recNumber == 0) {	// bookmark record
	    if (area.getUsage() == Desc_usage_IPD) {
		ctx.pushStatus(Sqlstate::_07009, Error::Gen, "cannot get bookmark IPD");
		return;
	    }
	    if (area.getUsage() == Desc_usage_IRD) {
		// XXX check SQL_ATTR_USE_BOOKMARK != SQL_UB_OFF
	    }
	}
	if ((unsigned)recNumber > area.getCount()) {
	    ctx.setCode(SQL_NO_DATA);
	    return;
	}
	area.getRecord(recNumber).getField(ctx, fieldIdentifier, data);
	if (! ctx.ok())
	    return;
    }
    // if no data, return success and undefined value
    if (data.type() == OdbcData::Undef)
	return;
    data.copyout(ctx, value, bufferLength, stringLength, stringLength2);
}

void
HandleDesc::sqlColAttribute(Ctx& ctx, SQLUSMALLINT columnNumber, SQLUSMALLINT fieldIdentifier, SQLPOINTER characterAttribute, SQLSMALLINT bufferLength, SQLSMALLINT* stringLength, SQLPOINTER numericAttribute)
{
    ctx_log3(("sqlColAttribute col=%d id=%d", columnNumber, fieldIdentifier));
    if (fieldIdentifier == SQL_COLUMN_LENGTH) {		// XXX iODBC workaround
	fieldIdentifier = SQL_DESC_LENGTH;
    }
    if (fieldIdentifier == 1205 || fieldIdentifier == 1206) {
	ctx_log2(("ignore unknown OSQL fieldIdentifier %d", (int)fieldIdentifier));
	if (characterAttribute != 0)
	    *(char*)characterAttribute = 0;
	if (stringLength != 0)
	    *stringLength = 0;
	return;
    }
    const DescSpec& spec = m_descArea.findSpec(fieldIdentifier);
    if (spec.m_pos == Desc_pos_end) {
	ctx.pushStatus(Sqlstate::_HY091, Error::Gen, "invalid descriptor id %d", (int)fieldIdentifier);
	return;
    }
    if (spec.m_type == OdbcData::Sqlchar || spec.m_type == OdbcData::Sqlstate)
	sqlGetDescField(ctx, columnNumber, fieldIdentifier, characterAttribute, bufferLength, 0, stringLength);
    else {
	sqlGetDescField(ctx, columnNumber, fieldIdentifier, numericAttribute, -1, 0);
    }
    if (ctx.getCode() == SQL_NO_DATA) {
	ctx.setCode(SQL_SUCCESS);
	ctx.pushStatus(Sqlstate::_07009, Error::Gen, "invalid column number %d", (int)columnNumber);
    }
}

void
HandleDesc::sqlColAttributes(Ctx& ctx, SQLUSMALLINT icol, SQLUSMALLINT fdescType, SQLPOINTER rgbDesc, SQLSMALLINT cbDescMax, SQLSMALLINT* pcbDesc, SQLINTEGER* pfDesc)
{
    ctx_log3(("sqlColAttributes col=%hu id=%hu", icol, fdescType));
    SQLUSMALLINT columnNumber = icol;
    SQLUSMALLINT fieldIdentifier;
    // XXX incomplete
    if (fdescType == SQL_COLUMN_TYPE) {
	fieldIdentifier = SQL_DESC_TYPE;
    } else if (fdescType == SQL_COLUMN_PRECISION) {
	SQLSMALLINT type;
	sqlGetDescField(ctx, columnNumber, SQL_DESC_TYPE, static_cast<SQLPOINTER>(&type), -1, 0);
	if (! ctx.ok())
	    return;
	switch (type) {
	case SQL_CHAR:
	case SQL_VARCHAR:
	case SQL_BINARY:
	case SQL_VARBINARY:
	case SQL_LONGVARCHAR:
	case SQL_LONGVARBINARY:
	case SQL_DATE:
	    fieldIdentifier = SQL_DESC_LENGTH;
	    break;
	default:
	    fieldIdentifier = SQL_DESC_PRECISION;
	    break;
	}
    } else if (fdescType == SQL_COLUMN_SCALE) {
	SQLSMALLINT type;
	sqlGetDescField(ctx, columnNumber, SQL_DESC_TYPE, static_cast<SQLPOINTER>(&type), -1, 0);
	if (! ctx.ok())
	    return;
	switch (type) {
	default:
	    fieldIdentifier = SQL_DESC_SCALE;
	    break;
	}
    } else if (fdescType == SQL_COLUMN_LENGTH) {
	SQLSMALLINT type;
	sqlGetDescField(ctx, columnNumber, SQL_DESC_TYPE, static_cast<SQLPOINTER>(&type), -1, 0);
	if (! ctx.ok())
	    return;
	switch (type) {
	default:
	    fieldIdentifier = SQL_DESC_LENGTH;
	    break;
	}
    } else {
	fieldIdentifier = fdescType;
    }
    sqlColAttribute(ctx, columnNumber, fieldIdentifier, rgbDesc, cbDescMax, pcbDesc, pfDesc);
}

// set and get several common descriptor values

void
HandleDesc::sqlSetDescRec(Ctx& ctx, SQLSMALLINT recNumber, SQLSMALLINT type, SQLSMALLINT subType, SQLINTEGER length, SQLSMALLINT precision, SQLSMALLINT scale, SQLPOINTER data, SQLINTEGER* stringLength, SQLINTEGER* indicator)
{
    sqlSetDescField(ctx, recNumber, SQL_DESC_TYPE, reinterpret_cast<SQLPOINTER>(type), -1);
    sqlSetDescField(ctx, recNumber, SQL_DESC_DATETIME_INTERVAL_CODE, reinterpret_cast<SQLPOINTER>(subType), -1);
    sqlSetDescField(ctx, recNumber, SQL_DESC_OCTET_LENGTH, reinterpret_cast<SQLPOINTER>(length), -1);
    sqlSetDescField(ctx, recNumber, SQL_DESC_PRECISION, reinterpret_cast<SQLPOINTER>(precision), -1);
    sqlSetDescField(ctx, recNumber, SQL_DESC_SCALE, reinterpret_cast<SQLPOINTER>(scale), -1);
    sqlSetDescField(ctx, recNumber, SQL_DESC_DATA_PTR, data, -1);
    sqlSetDescField(ctx, recNumber, SQL_DESC_OCTET_LENGTH_PTR, reinterpret_cast<SQLPOINTER>(stringLength), -1);
    sqlSetDescField(ctx, recNumber, SQL_DESC_INDICATOR_PTR, reinterpret_cast<SQLPOINTER>(indicator), -1);
}

void
HandleDesc::sqlGetDescRec(Ctx& ctx, SQLSMALLINT recNumber, SQLCHAR* name, SQLSMALLINT bufferLength, SQLSMALLINT* stringLength, SQLSMALLINT* type, SQLSMALLINT* subType, SQLINTEGER* length, SQLSMALLINT* precision, SQLSMALLINT* scale, SQLSMALLINT* nullable)
{
    sqlGetDescField(ctx, recNumber, SQL_DESC_NAME, reinterpret_cast<SQLPOINTER>(name), bufferLength, 0, stringLength);
    sqlGetDescField(ctx, recNumber, SQL_DESC_TYPE, reinterpret_cast<SQLPOINTER>(type), -1, 0);
    sqlGetDescField(ctx, recNumber, SQL_DESC_DATETIME_INTERVAL_CODE, reinterpret_cast<SQLPOINTER>(subType), -1, 0);
    sqlGetDescField(ctx, recNumber, SQL_DESC_OCTET_LENGTH, reinterpret_cast<SQLPOINTER>(length), -1, 0);
    sqlGetDescField(ctx, recNumber, SQL_DESC_PRECISION, reinterpret_cast<SQLPOINTER>(precision), -1, 0);
    sqlGetDescField(ctx, recNumber, SQL_DESC_SCALE, reinterpret_cast<SQLPOINTER>(scale), -1, 0);
    sqlGetDescField(ctx, recNumber, SQL_DESC_NULLABLE, reinterpret_cast<SQLPOINTER>(nullable), -1, 0);
}
