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

#include <stdio.h>
#include "OdbcData.hpp"
#include "DiagArea.hpp"

// DiagSpec

static const DiagSpec
diag_spec_list[] = {
    {   Diag_pos_header,
	SQL_DIAG_CURSOR_ROW_COUNT,
	OdbcData::Integer,
	Odbc_handle_stmt
    },
    {   Diag_pos_header,
	SQL_DIAG_DYNAMIC_FUNCTION,
	OdbcData::Sqlchar,
	Odbc_handle_stmt
    },
    {   Diag_pos_header,
	SQL_DIAG_DYNAMIC_FUNCTION_CODE,
	OdbcData::Integer,
	Odbc_handle_stmt
    },
    {   Diag_pos_header,
	SQL_DIAG_NUMBER,
	OdbcData::Integer,
	Odbc_handle_all
    },
    {   Diag_pos_header,
	SQL_DIAG_RETURNCODE,
	OdbcData::Smallint,
	Odbc_handle_all
    },
    {   Diag_pos_header,
	SQL_DIAG_ROW_COUNT,
	OdbcData::Integer,
	Odbc_handle_stmt
    },
    {   Diag_pos_status,
	SQL_DIAG_CLASS_ORIGIN,
	OdbcData::Sqlchar,
	Odbc_handle_all
    },
    {   Diag_pos_status,
	SQL_DIAG_COLUMN_NUMBER,
	OdbcData::Integer,
	Odbc_handle_all
    },
    {   Diag_pos_status,
	SQL_DIAG_CONNECTION_NAME,
	OdbcData::Sqlchar,
	Odbc_handle_all
    },
    {   Diag_pos_status,
	SQL_DIAG_MESSAGE_TEXT,
	OdbcData::Sqlchar,
	Odbc_handle_all
    },
    {   Diag_pos_status,
	SQL_DIAG_NATIVE,
	OdbcData::Integer,
	Odbc_handle_all
    },
    {   Diag_pos_status,
	SQL_DIAG_ROW_NUMBER,
	OdbcData::Integer,
	Odbc_handle_all
    },
    {   Diag_pos_status,
	SQL_DIAG_SERVER_NAME,
	OdbcData::Sqlchar,
	Odbc_handle_all
    },
    {   Diag_pos_status,
	SQL_DIAG_SQLSTATE,
	OdbcData::Sqlchar,
	Odbc_handle_all
    },
    {   Diag_pos_status,
	SQL_DIAG_SUBCLASS_ORIGIN,
	OdbcData::Sqlchar,
	Odbc_handle_all
    },
    {   Diag_pos_end,
        0,
	OdbcData::Undef,
	0
    }
};

const DiagSpec&
DiagSpec::find(int id)
{
    const DiagSpec* p;
    for (p = diag_spec_list; p->m_pos != Diag_pos_end; p++) {
	if (p->m_id == id)
	    break;
    }
    return *p;
}

// DiagField

// DiagRec

void
DiagRec::setField(int id, const OdbcData& data)
{
    Fields::iterator iter;
    iter = m_fields.find(id);
    if (iter != m_fields.end()) {
	DiagField& field = (*iter).second;
	field.setData(data);
	return;
    }
    const DiagSpec& spec = DiagSpec::find(id);
    if (spec.m_pos != Diag_pos_end) {
	DiagField field(spec, data);
	m_fields.insert(Fields::value_type(id, field));
	return;
    }
    ctx_assert(false);
}

void
DiagRec::getField(Ctx& ctx, int id, OdbcData& data)
{
    Fields::iterator iter;
    iter = m_fields.find(id);
    if (iter != m_fields.end()) {
	DiagField& field = (*iter).second;
	data.setValue(field.getData());
	return;
    }
    const DiagSpec& spec = DiagSpec::find(id);
    if (spec.m_pos != Diag_pos_end) {
	// success and undefined value says the MS doc
	data.setValue();
	return;
    }
    ctx_assert(false);
}

// DiagArea

DiagArea::DiagArea() :
    m_recs(1),			// add header
    m_code(SQL_SUCCESS),
    m_recNumber(0)
{
    setHeader(SQL_DIAG_NUMBER, (SQLINTEGER)0);
}

DiagArea::~DiagArea() {
}

unsigned
DiagArea::numStatus()
{
    ctx_assert(m_recs.size() > 0);
    return m_recs.size() - 1;
}

void
DiagArea::pushStatus()
{
    ctx_assert(m_recs.size() > 0);
    DiagRec rec;
    m_recs.push_back(rec);
    SQLINTEGER diagNumber = m_recs.size() - 1;
    setHeader(SQL_DIAG_NUMBER, diagNumber);
}

void
DiagArea::setHeader(int id, const OdbcData& data)
{
    ctx_assert(m_recs.size() > 0);
    getHeader().setField(id, data);
}

// set status

void
DiagArea::setStatus(int id, const OdbcData& data)
{
    getStatus().setField(id, data);
}

void
DiagArea::setStatus(const Sqlstate& state)
{
    getStatus().setField(SQL_DIAG_SQLSTATE, state);
    setCode(state.getCode(m_code));
}

void
DiagArea::setStatus(const Error& error)
{
    BaseString message("");
    // bracketed prefixes
    message.append(NDB_ODBC_COMPONENT_VENDOR);
    message.append(NDB_ODBC_COMPONENT_DRIVER);
    if (! error.driverError())
	message.append(NDB_ODBC_COMPONENT_DATABASE);
    // native error code
    char nativeString[20];
    sprintf(nativeString, "%02d%02d%04d",
    	(unsigned)error.m_status % 100,
	(unsigned)error.m_classification % 100,
	(unsigned)error.m_code % 10000);
    SQLINTEGER native = atoi(nativeString);
    message.appfmt("NDB-%s", nativeString);
    // message text
    message.append(" ");
    message.append(error.m_message);
    if (error.m_sqlFunction != 0)
	message.appfmt(" (in %s)", error.m_sqlFunction);
    // set diag fields
    setStatus(error.m_sqlstate);
    setStatus(SQL_DIAG_NATIVE, native);
    setStatus(SQL_DIAG_MESSAGE_TEXT, message.c_str());
}

// push status

void
DiagArea::pushStatus(const Error& error)
{
    pushStatus();
    setStatus(error);
}

// record access

DiagRec&
DiagArea::getHeader()
{
    ctx_assert(m_recs.size() > 0);
    return m_recs[0];
}

DiagRec&
DiagArea::getStatus()
{
    ctx_assert(m_recs.size() > 1);
    return m_recs.back();
}

DiagRec&
DiagArea::getRecord(unsigned num)
{
    ctx_assert(num < m_recs.size());
    return m_recs[num];
}

void
DiagArea::getRecord(Ctx& ctx, unsigned num, int id, OdbcData& data)
{
    DiagRec& rec = getRecord(num);
    rec.getField(ctx, id, data);
}

void
DiagArea::setCode(SQLRETURN code)
{
    m_code = code;
    getHeader().setField(SQL_DIAG_RETURNCODE, (SQLSMALLINT)code);
}
