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

#include "Code_expr_param.hpp"
#include "Code_root.hpp"

// Plan_expr_param

Plan_expr_param::~Plan_expr_param()
{
}

Plan_base*
Plan_expr_param::analyze(Ctx& ctx, Ctl& ctl)
{
    m_exec = 0;
    ctx_assert(m_paramNumber != 0);
    ctx_assert(m_paramNumber < m_root->m_paramList.size());
    m_root->m_paramList[m_paramNumber] = this;
    m_sqlType.setType(ctx, SqlType::Unbound);
    // check if type is bound now
    DescArea& ipd = descArea(Desc_usage_IPD);
    if (m_paramNumber <= ipd.getCount()) {
	DescRec& rec = ipd.getRecord(m_paramNumber);
	OdbcData descData;
	rec.getField(ctx, SQL_DESC_TYPE, descData);
	if (descData.type() != OdbcData::Undef) {
	    SQLSMALLINT desc_TYPE = descData.smallint();
	    // XXX wrong but fixes sun.jdbc.odbc
	    if (desc_TYPE == SQL_CHAR)
		desc_TYPE = SQL_VARCHAR;
	    if (desc_TYPE == SQL_CHAR) {
		rec.getField(ctx, SQL_DESC_LENGTH, descData);
		if (descData.type() != OdbcData::Undef) {
		    unsigned desc_LENGTH = descData.uinteger();
		    m_sqlType.setType(ctx, SqlType::Char, desc_LENGTH);
		}
	    } else if (desc_TYPE == SQL_VARCHAR) {
		rec.getField(ctx, SQL_DESC_LENGTH, descData);
		if (descData.type() != OdbcData::Undef) {
		    unsigned desc_LENGTH = descData.uinteger();
		    m_sqlType.setType(ctx, SqlType::Varchar, desc_LENGTH);
		}
	    } else if (desc_TYPE == SQL_BINARY) {
		rec.getField(ctx, SQL_DESC_LENGTH, descData);
		if (descData.type() != OdbcData::Undef) {
		    unsigned desc_LENGTH = descData.uinteger();
		    m_sqlType.setType(ctx, SqlType::Binary, desc_LENGTH);
		}
	    } else if (desc_TYPE == SQL_VARBINARY) {
		rec.getField(ctx, SQL_DESC_LENGTH, descData);
		if (descData.type() != OdbcData::Undef) {
		    unsigned desc_LENGTH = descData.uinteger();
		    m_sqlType.setType(ctx, SqlType::Varbinary, desc_LENGTH);
		} else {
		    // XXX BLOB hack
		    unsigned desc_LENGTH = FAKE_BLOB_SIZE;
		    m_sqlType.setType(ctx, SqlType::Varbinary, desc_LENGTH);
		}
	    } else if (desc_TYPE == SQL_SMALLINT) {
		m_sqlType.setType(ctx, SqlType::Smallint);
	    } else if (desc_TYPE == SQL_INTEGER) {
		m_sqlType.setType(ctx, SqlType::Integer);
	    } else if (desc_TYPE == SQL_BIGINT) {
		m_sqlType.setType(ctx, SqlType::Bigint);
	    } else if (desc_TYPE == SQL_REAL) {
		m_sqlType.setType(ctx, SqlType::Real);
	    } else if (desc_TYPE == SQL_DOUBLE) {
		m_sqlType.setType(ctx, SqlType::Double);
	    } else if (desc_TYPE == SQL_TYPE_TIMESTAMP) {
		m_sqlType.setType(ctx, SqlType::Datetime);
	    // XXX BLOB hack
	    } else if (desc_TYPE == SQL_LONGVARBINARY) {
		m_sqlType.setType(ctx, SqlType::Varbinary, (unsigned)FAKE_BLOB_SIZE);
	    } else {
		ctx.pushStatus(Error::Gen, "parameter %u unsupported SQL type %d", m_paramNumber, (int)desc_TYPE);
		return 0;
	    }
	    char buf[100];
	    m_sqlType.print(buf, sizeof(buf));
	    ctx_log2(("parameter %u SQL type bound to %s", m_paramNumber, buf));
	}
    }
    return this;
}

void
Plan_expr_param::describe(Ctx& ctx)
{
    DescArea& ipd = descArea(Desc_usage_IPD);
    if (ipd.getCount() < m_paramNumber)
	ipd.setCount(ctx, m_paramNumber);
    // XXX describe if possible
    DescRec& rec = ipd.getRecord(m_paramNumber);
}

Exec_base*
Plan_expr_param::codegen(Ctx& ctx, Ctl& ctl)
{
    if (m_exec != 0)
	return m_exec;
    SqlSpec sqlSpec(m_sqlType, SqlSpec::Physical);
    // create code
    Exec_expr_param* exec = new Exec_expr_param(ctl.m_execRoot);
    ctl.m_execRoot->saveNode(exec);
    ctx_assert(m_paramNumber != 0);
    Exec_expr_param::Code& code = *new Exec_expr_param::Code(sqlSpec, m_paramNumber);
    exec->setCode(code);
    m_exec = exec;
    return exec;
}

void
Plan_expr_param::print(Ctx& ctx)
{
    ctx.print(" [param %u]", m_paramNumber);
}

bool
Plan_expr_param::isEqual(const Plan_expr* expr) const
{
    ctx_assert(expr != 0);
    if (expr->type() != Plan_expr::TypeParam)
	return false;
    const Plan_expr_param* expr2 = static_cast<const Plan_expr_param*>(expr);
    // params are not equal ever
    return false;
}

bool
Plan_expr_param::isGroupBy(const Plan_expr_row* row) const
{
    // params are constants
    return true;
}

// Exec_expr_param

Exec_expr_param::Code::~Code()
{
}

Exec_expr_param::Data::~Data()
{
    delete m_extField;
    m_extField = 0;
}

Exec_expr_param::~Exec_expr_param()
{
}

void
Exec_expr_param::alloc(Ctx& ctx, Ctl& ctl)
{
    if (m_data != 0)
	return;
    const Code& code = getCode();
    SqlField sqlField(code.sqlSpec());
    Data& data = *new Data(sqlField);
    setData(data);
}

void
Exec_expr_param::bind(Ctx& ctx)
{
    const Code& code = getCode();
    Data& data = getData();
    DescArea& apd = descArea(Desc_usage_APD);
    if (apd.getCount() < code.m_paramNumber) {
	ctx_log1(("parameter %u is not bound", code.m_paramNumber));
	return;
    }
    const unsigned paramNumber = code.m_paramNumber;
    DescRec& rec = apd.getRecord(paramNumber);
    OdbcData descData;
    // create type
    rec.getField(ctx, SQL_DESC_TYPE, descData);
    if (descData.type() == OdbcData::Undef) {
	ctx.pushStatus(Error::Gen, "parameter %u external type not defined", paramNumber);
	return;
    }
    ExtType extType;
    SQLSMALLINT desc_TYPE = descData.smallint();
    switch (desc_TYPE) {
    case SQL_C_CHAR:
    case SQL_C_SHORT:	// for sun.jdbc.odbc
    case SQL_C_SSHORT:
    case SQL_C_USHORT:
    case SQL_C_LONG:	// for sun.jdbc.odbc
    case SQL_C_SLONG:
    case SQL_C_ULONG:
    case SQL_C_SBIGINT:
    case SQL_C_UBIGINT:
    case SQL_C_FLOAT:
    case SQL_C_DOUBLE:
    case SQL_C_TYPE_TIMESTAMP:
    case SQL_C_BINARY:	// XXX BLOB hack
	break;
    default:
	ctx.pushStatus(Error::Gen, "parameter %u unsupported external type %d", paramNumber, (int)desc_TYPE);
	return;
    }
    extType.setType(ctx, static_cast<ExtType::Type>(desc_TYPE));
    ExtSpec extSpec(extType);
    // create data field
    rec.getField(ctx, SQL_DESC_DATA_PTR, descData);
    if (descData.type() == OdbcData::Undef) {
	ctx.pushStatus(Error::Gen, "parameter %u data address not defined", paramNumber);
	return;
    }
    SQLPOINTER desc_DATA_PTR = descData.pointer();
    rec.getField(ctx, SQL_DESC_OCTET_LENGTH, descData);
    if (descData.type() == OdbcData::Undef) {
	ctx.pushStatus(Error::Gen, "parameter %u data length not defined", paramNumber);
	return;
    }
    SQLINTEGER desc_OCTET_LENGTH = descData.integer();
    rec.getField(ctx, SQL_DESC_INDICATOR_PTR, descData);
    if (descData.type() == OdbcData::Undef) {
	ctx.pushStatus(Error::Gen, "parameter %u indicator address not defined", paramNumber);
	return;
    }
    SQLINTEGER* desc_INDICATOR_PTR = descData.integerPtr();
    ctx_log4(("parameter %u bind to 0x%x %d 0x%x", paramNumber, (unsigned)desc_DATA_PTR, (int)desc_OCTET_LENGTH, (unsigned)desc_INDICATOR_PTR));
    ExtField& extField = *new ExtField(extSpec, desc_DATA_PTR, desc_OCTET_LENGTH, desc_INDICATOR_PTR, paramNumber);
    data.m_atExec = false;
    if (desc_INDICATOR_PTR != 0 && *desc_INDICATOR_PTR < 0) {
	if (*desc_INDICATOR_PTR == SQL_NULL_DATA) {
	    ;
	} else if (*desc_INDICATOR_PTR == SQL_DATA_AT_EXEC) {
	    data.m_atExec = true;
	} else if (*desc_INDICATOR_PTR <= SQL_LEN_DATA_AT_EXEC(0)) {
	    data.m_atExec = true;
	}
    }
    delete data.m_extField;
    data.m_extField = &extField;
}

void
Exec_expr_param::evaluate(Ctx& ctx, Ctl& ctl)
{
    if (ctl.m_postEval)
	return;
    const Code& code = getCode();
    Data& data = getData();
    if (data.m_atExec)
	return;
    ctx_assert(data.m_extField != 0);
    data.m_sqlField.copyin(ctx, *data.m_extField);
}

void
Exec_expr_param::close(Ctx& ctx)
{
    Data& data = getData();
    data.m_extPos = -1;
}

void
Exec_expr_param::print(Ctx& ctx)
{
    const Code& code = getCode();
    ctx.print(" [param %u]", code.m_paramNumber);
}
