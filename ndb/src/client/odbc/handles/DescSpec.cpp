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
#include "HandleDesc.hpp"

static void
callback_SQL_DESC_ALLOC_TYPE_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0 && data.type() == OdbcData::Smallint);
}

static void
callback_SQL_DESC_ALLOC_TYPE_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0);
    data.setValue();
}

static void
callback_SQL_DESC_ARRAY_SIZE_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0 && data.type() == OdbcData::Uinteger);
}

static void
callback_SQL_DESC_ARRAY_SIZE_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0);
    data.setValue();
}

static void
callback_SQL_DESC_ARRAY_STATUS_PTR_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0 && data.type() == OdbcData::UsmallintPtr);
}

static void
callback_SQL_DESC_ARRAY_STATUS_PTR_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0);
    data.setValue();
}

static void
callback_SQL_DESC_BIND_OFFSET_PTR_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0 && data.type() == OdbcData::IntegerPtr);
}

static void
callback_SQL_DESC_BIND_OFFSET_PTR_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0);
    data.setValue();
}

static void
callback_SQL_DESC_BIND_TYPE_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0 && data.type() == OdbcData::Integer);
}

static void
callback_SQL_DESC_BIND_TYPE_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0);
    data.setValue();
}

static void
callback_SQL_DESC_COUNT_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0 && data.type() == OdbcData::Smallint);
}

static void
callback_SQL_DESC_COUNT_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0);
    data.setValue();
}

static void
callback_SQL_DESC_ROWS_PROCESSED_PTR_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0 && data.type() == OdbcData::UintegerPtr);
}

static void
callback_SQL_DESC_ROWS_PROCESSED_PTR_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0);
    data.setValue();
}

static void
callback_SQL_DESC_AUTO_UNIQUE_VALUE_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0 && data.type() == OdbcData::Integer);
}

static void
callback_SQL_DESC_AUTO_UNIQUE_VALUE_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0);
    data.setValue();
}

static void
callback_SQL_DESC_BASE_COLUMN_NAME_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0 && data.type() == OdbcData::Sqlchar);
}

static void
callback_SQL_DESC_BASE_COLUMN_NAME_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0);
    data.setValue();
}

static void
callback_SQL_DESC_BASE_TABLE_NAME_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0 && data.type() == OdbcData::Sqlchar);
}

static void
callback_SQL_DESC_BASE_TABLE_NAME_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0);
    data.setValue();
}

static void
callback_SQL_DESC_CASE_SENSITIVE_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0 && data.type() == OdbcData::Integer);
}

static void
callback_SQL_DESC_CASE_SENSITIVE_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0);
    data.setValue();
}

static void
callback_SQL_DESC_CATALOG_NAME_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0 && data.type() == OdbcData::Sqlchar);
}

static void
callback_SQL_DESC_CATALOG_NAME_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0);
    data.setValue();
}

static void
callback_SQL_DESC_CONCISE_TYPE_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0 && data.type() == OdbcData::Smallint);
}

static void
callback_SQL_DESC_CONCISE_TYPE_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0);
    data.setValue();
}

static void
callback_SQL_DESC_DATA_PTR_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0 && data.type() == OdbcData::Pointer);
}

static void
callback_SQL_DESC_DATA_PTR_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0);
    data.setValue();
}

static void
callback_SQL_DESC_DATETIME_INTERVAL_CODE_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0 && data.type() == OdbcData::Smallint);
}

static void
callback_SQL_DESC_DATETIME_INTERVAL_CODE_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0);
    data.setValue();
}

static void
callback_SQL_DESC_DATETIME_INTERVAL_PRECISION_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0 && data.type() == OdbcData::Integer);
}

static void
callback_SQL_DESC_DATETIME_INTERVAL_PRECISION_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0);
    data.setValue();
}

static void
callback_SQL_DESC_DISPLAY_SIZE_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0 && data.type() == OdbcData::Integer);
}

static void
callback_SQL_DESC_DISPLAY_SIZE_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0);
    data.setValue();
}

static void
callback_SQL_DESC_FIXED_PREC_SCALE_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0 && data.type() == OdbcData::Smallint);
}

static void
callback_SQL_DESC_FIXED_PREC_SCALE_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0);
    data.setValue();
}

static void
callback_SQL_DESC_INDICATOR_PTR_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0 && data.type() == OdbcData::IntegerPtr);
}

static void
callback_SQL_DESC_INDICATOR_PTR_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0);
    data.setValue();
}

static void
callback_SQL_DESC_LABEL_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0 && data.type() == OdbcData::Sqlchar);
}

static void
callback_SQL_DESC_LABEL_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0);
    data.setValue();
}

static void
callback_SQL_DESC_LENGTH_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0 && data.type() == OdbcData::Uinteger);
}

static void
callback_SQL_DESC_LENGTH_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0);
    data.setValue();
}

static void
callback_SQL_DESC_LITERAL_PREFIX_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0 && data.type() == OdbcData::Sqlchar);
}

static void
callback_SQL_DESC_LITERAL_PREFIX_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0);
    data.setValue();
}

static void
callback_SQL_DESC_LITERAL_SUFFIX_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0 && data.type() == OdbcData::Sqlchar);
}

static void
callback_SQL_DESC_LITERAL_SUFFIX_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0);
    data.setValue();
}

static void
callback_SQL_DESC_LOCAL_TYPE_NAME_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0 && data.type() == OdbcData::Sqlchar);
}

static void
callback_SQL_DESC_LOCAL_TYPE_NAME_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0);
    data.setValue();
}

static void
callback_SQL_DESC_NAME_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0 && data.type() == OdbcData::Sqlchar);
}

static void
callback_SQL_DESC_NAME_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0);
    data.setValue();
}

static void
callback_SQL_DESC_NULLABLE_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0 && data.type() == OdbcData::Smallint);
}

static void
callback_SQL_DESC_NULLABLE_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0);
    data.setValue();
}

static void
callback_SQL_DESC_NUM_PREC_RADIX_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0 && data.type() == OdbcData::Integer);
}

static void
callback_SQL_DESC_NUM_PREC_RADIX_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0);
    data.setValue();
}

static void
callback_SQL_DESC_OCTET_LENGTH_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0 && data.type() == OdbcData::Integer);
}

static void
callback_SQL_DESC_OCTET_LENGTH_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0);
    data.setValue();
}

static void
callback_SQL_DESC_OCTET_LENGTH_PTR_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0 && data.type() == OdbcData::IntegerPtr);
}

static void
callback_SQL_DESC_OCTET_LENGTH_PTR_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0);
    data.setValue();
}

static void
callback_SQL_DESC_PARAMETER_TYPE_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0 && data.type() == OdbcData::Smallint);
}

static void
callback_SQL_DESC_PARAMETER_TYPE_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0);
    data.setValue();
}

static void
callback_SQL_DESC_PRECISION_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0 && data.type() == OdbcData::Smallint);
}

static void
callback_SQL_DESC_PRECISION_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0);
    data.setValue();
}

static void
callback_SQL_DESC_ROWVER_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0 && data.type() == OdbcData::Smallint);
}

static void
callback_SQL_DESC_ROWVER_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0);
    data.setValue();
}

static void
callback_SQL_DESC_SCALE_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0 && data.type() == OdbcData::Smallint);
}

static void
callback_SQL_DESC_SCALE_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0);
    data.setValue();
}

static void
callback_SQL_DESC_SCHEMA_NAME_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0 && data.type() == OdbcData::Sqlchar);
}

static void
callback_SQL_DESC_SCHEMA_NAME_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0);
    data.setValue();
}

static void
callback_SQL_DESC_SEARCHABLE_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0 && data.type() == OdbcData::Smallint);
}

static void
callback_SQL_DESC_SEARCHABLE_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0);
    data.setValue();
}

static void
callback_SQL_DESC_TABLE_NAME_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0 && data.type() == OdbcData::Sqlchar);
}

static void
callback_SQL_DESC_TABLE_NAME_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0);
    data.setValue();
}

static void
callback_SQL_DESC_TYPE_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0 && data.type() == OdbcData::Smallint);
}

static void
callback_SQL_DESC_TYPE_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0);
    data.setValue();
}

static void
callback_SQL_DESC_TYPE_NAME_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0 && data.type() == OdbcData::Sqlchar);
}

static void
callback_SQL_DESC_TYPE_NAME_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0);
    data.setValue();
}

static void
callback_SQL_DESC_UNNAMED_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0 && data.type() == OdbcData::Smallint);
}

static void
callback_SQL_DESC_UNNAMED_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0);
    data.setValue();
}

static void
callback_SQL_DESC_UNSIGNED_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0 && data.type() == OdbcData::Smallint);
}

static void
callback_SQL_DESC_UNSIGNED_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0);
    data.setValue();
}

static void
callback_SQL_DESC_UPDATABLE_set(Ctx& ctx, HandleBase* self, const OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0 && data.type() == OdbcData::Smallint);
}

static void
callback_SQL_DESC_UPDATABLE_default(Ctx& ctx, HandleBase* self, OdbcData& data)
{
    HandleDesc* pDesc = static_cast<HandleDesc*>(self);
    ctx_assert(pDesc != 0);
    data.setValue();
}

DescSpec HandleDesc::m_descSpec[] = {
    {   Desc_pos_header,
	SQL_DESC_ALLOC_TYPE,
	OdbcData::Smallint,
	{   Desc_mode_undef,
	    Desc_mode_readonly,
	    Desc_mode_readonly,
	    Desc_mode_readonly,
	    Desc_mode_readonly,
	},
	callback_SQL_DESC_ALLOC_TYPE_set,
	callback_SQL_DESC_ALLOC_TYPE_default,
    },
    {   Desc_pos_header,
	SQL_DESC_ARRAY_SIZE,
	OdbcData::Uinteger,
	{   Desc_mode_undef,
	    Desc_mode_unused,
	    Desc_mode_readwrite,
	    Desc_mode_unused,
	    Desc_mode_readwrite
	},
	callback_SQL_DESC_ARRAY_SIZE_set,
	callback_SQL_DESC_ARRAY_SIZE_default,
    },
    {   Desc_pos_header,
	SQL_DESC_ARRAY_STATUS_PTR,
	OdbcData::UsmallintPtr,
	{   Desc_mode_undef,
	    Desc_mode_readwrite,
	    Desc_mode_readwrite,
	    Desc_mode_readwrite,
	    Desc_mode_readwrite
	},
	callback_SQL_DESC_ARRAY_STATUS_PTR_set,
	callback_SQL_DESC_ARRAY_STATUS_PTR_default,
    },
    {   Desc_pos_header,
	SQL_DESC_BIND_OFFSET_PTR,
	OdbcData::IntegerPtr,
	{   Desc_mode_undef,
	    Desc_mode_unused,
	    Desc_mode_readwrite,
	    Desc_mode_unused,
	    Desc_mode_readwrite
	},
	callback_SQL_DESC_BIND_OFFSET_PTR_set,
	callback_SQL_DESC_BIND_OFFSET_PTR_default,
    },
    {   Desc_pos_header,
	SQL_DESC_BIND_TYPE,
	OdbcData::Integer,
	{   Desc_mode_undef,
	    Desc_mode_unused,
	    Desc_mode_readwrite,
	    Desc_mode_unused,
	    Desc_mode_readwrite
	},
	callback_SQL_DESC_BIND_TYPE_set,
	callback_SQL_DESC_BIND_TYPE_default,
    },
    {   Desc_pos_header,
	SQL_DESC_COUNT,
	OdbcData::Smallint,
	{   Desc_mode_undef,
	    Desc_mode_readwrite,
	    Desc_mode_readwrite,
	    Desc_mode_readonly,
	    Desc_mode_readwrite
	},
	callback_SQL_DESC_COUNT_set,
	callback_SQL_DESC_COUNT_default,
    },
    {   Desc_pos_header,
	SQL_DESC_ROWS_PROCESSED_PTR,
	OdbcData::UintegerPtr,
	{   Desc_mode_undef,
	    Desc_mode_readwrite,
	    Desc_mode_unused,
	    Desc_mode_readwrite,
	    Desc_mode_unused
	},
	callback_SQL_DESC_ROWS_PROCESSED_PTR_set,
	callback_SQL_DESC_ROWS_PROCESSED_PTR_default,
    },
    {   Desc_pos_record,
	SQL_DESC_AUTO_UNIQUE_VALUE,
	OdbcData::Integer,
	{   Desc_mode_undef,
	    Desc_mode_unused,
	    Desc_mode_unused,
	    Desc_mode_readonly,
	    Desc_mode_unused
	},
	callback_SQL_DESC_AUTO_UNIQUE_VALUE_set,
	callback_SQL_DESC_AUTO_UNIQUE_VALUE_default,
    },
    {   Desc_pos_record,
	SQL_DESC_BASE_COLUMN_NAME,
	OdbcData::Sqlchar,
	{   Desc_mode_undef,
	    Desc_mode_unused,
	    Desc_mode_unused,
	    Desc_mode_readonly,
	    Desc_mode_unused
	},
	callback_SQL_DESC_BASE_COLUMN_NAME_set,
	callback_SQL_DESC_BASE_COLUMN_NAME_default,
    },
    {   Desc_pos_record,
	SQL_DESC_BASE_TABLE_NAME,
	OdbcData::Sqlchar,
	{   Desc_mode_undef,
	    Desc_mode_unused,
	    Desc_mode_unused,
	    Desc_mode_readonly,
	    Desc_mode_unused
	},
	callback_SQL_DESC_BASE_TABLE_NAME_set,
	callback_SQL_DESC_BASE_TABLE_NAME_default,
    },
    {   Desc_pos_record,
	SQL_DESC_CASE_SENSITIVE,
	OdbcData::Integer,
	{   Desc_mode_undef,
	    Desc_mode_readonly,
	    Desc_mode_unused,
	    Desc_mode_readonly,
	    Desc_mode_unused
	},
	callback_SQL_DESC_CASE_SENSITIVE_set,
	callback_SQL_DESC_CASE_SENSITIVE_default,
    },
    {   Desc_pos_record,
	SQL_DESC_CATALOG_NAME,
	OdbcData::Sqlchar,
	{   Desc_mode_undef,
	    Desc_mode_unused,
	    Desc_mode_unused,
	    Desc_mode_readonly,
	    Desc_mode_unused
	},
	callback_SQL_DESC_CATALOG_NAME_set,
	callback_SQL_DESC_CATALOG_NAME_default,
    },
    {   Desc_pos_record,
	SQL_DESC_CONCISE_TYPE,
	OdbcData::Smallint,
	{   Desc_mode_undef,
	    Desc_mode_readwrite,
	    Desc_mode_readwrite,
	    Desc_mode_readonly,
	    Desc_mode_readwrite
	},
	callback_SQL_DESC_CONCISE_TYPE_set,
	callback_SQL_DESC_CONCISE_TYPE_default,
    },
    {   Desc_pos_record,
	SQL_DESC_DATA_PTR,
	OdbcData::Pointer,
	{   Desc_mode_undef,
	    Desc_mode_unused,
	    Desc_mode_readwrite,
	    Desc_mode_unused,
	    Desc_mode_readwrite
	},
	callback_SQL_DESC_DATA_PTR_set,
	callback_SQL_DESC_DATA_PTR_default,
    },
    {   Desc_pos_record,
	SQL_DESC_DATETIME_INTERVAL_CODE,
	OdbcData::Smallint,
	{   Desc_mode_undef,
	    Desc_mode_readwrite,
	    Desc_mode_readwrite,
	    Desc_mode_readonly,
	    Desc_mode_readwrite
	},
	callback_SQL_DESC_DATETIME_INTERVAL_CODE_set,
	callback_SQL_DESC_DATETIME_INTERVAL_CODE_default,
    },
    {   Desc_pos_record,
	SQL_DESC_DATETIME_INTERVAL_PRECISION,
	OdbcData::Integer,
	{   Desc_mode_undef,
	    Desc_mode_readwrite,
	    Desc_mode_readwrite,
	    Desc_mode_readonly,
	    Desc_mode_readwrite
	},
	callback_SQL_DESC_DATETIME_INTERVAL_PRECISION_set,
	callback_SQL_DESC_DATETIME_INTERVAL_PRECISION_default,
    },
    {   Desc_pos_record,
	SQL_DESC_DISPLAY_SIZE,
	OdbcData::Integer,
	{   Desc_mode_undef,
	    Desc_mode_unused,
	    Desc_mode_unused,
	    Desc_mode_readonly,
	    Desc_mode_unused
	},
	callback_SQL_DESC_DISPLAY_SIZE_set,
	callback_SQL_DESC_DISPLAY_SIZE_default,
    },
    {   Desc_pos_record,
	SQL_DESC_FIXED_PREC_SCALE,
	OdbcData::Smallint,
	{   Desc_mode_undef,
	    Desc_mode_readonly,
	    Desc_mode_unused,
	    Desc_mode_readonly,
	    Desc_mode_unused
	},
	callback_SQL_DESC_FIXED_PREC_SCALE_set,
	callback_SQL_DESC_FIXED_PREC_SCALE_default,
    },
    {   Desc_pos_record,
	SQL_DESC_INDICATOR_PTR,
	OdbcData::IntegerPtr,
	{   Desc_mode_undef,
	    Desc_mode_unused,
	    Desc_mode_readwrite,
	    Desc_mode_unused,
	    Desc_mode_readwrite
	},
	callback_SQL_DESC_INDICATOR_PTR_set,
	callback_SQL_DESC_INDICATOR_PTR_default,
    },
    {   Desc_pos_record,
	SQL_DESC_LABEL,
	OdbcData::Sqlchar,
	{   Desc_mode_undef,
	    Desc_mode_unused,
	    Desc_mode_unused,
	    Desc_mode_readonly,
	    Desc_mode_unused
	},
	callback_SQL_DESC_LABEL_set,
	callback_SQL_DESC_LABEL_default,
    },
    {   Desc_pos_record,
	SQL_DESC_LENGTH,
	OdbcData::Uinteger,
	{   Desc_mode_undef,
	    Desc_mode_readwrite,
	    Desc_mode_readwrite,
	    Desc_mode_readonly,
	    Desc_mode_readwrite
	},
	callback_SQL_DESC_LENGTH_set,
	callback_SQL_DESC_LENGTH_default,
    },
    {   Desc_pos_record,
	SQL_DESC_LITERAL_PREFIX,
	OdbcData::Sqlchar,
	{   Desc_mode_undef,
	    Desc_mode_unused,
	    Desc_mode_unused,
	    Desc_mode_readonly,
	    Desc_mode_unused
	},
	callback_SQL_DESC_LITERAL_PREFIX_set,
	callback_SQL_DESC_LITERAL_PREFIX_default,
    },
    {   Desc_pos_record,
	SQL_DESC_LITERAL_SUFFIX,
	OdbcData::Sqlchar,
	{   Desc_mode_undef,
	    Desc_mode_unused,
	    Desc_mode_unused,
	    Desc_mode_readonly,
	    Desc_mode_unused
	},
	callback_SQL_DESC_LITERAL_SUFFIX_set,
	callback_SQL_DESC_LITERAL_SUFFIX_default,
    },
    {   Desc_pos_record,
	SQL_DESC_LOCAL_TYPE_NAME,
	OdbcData::Sqlchar,
	{   Desc_mode_undef,
	    Desc_mode_readonly,
	    Desc_mode_unused,
	    Desc_mode_readonly,
	    Desc_mode_unused
	},
	callback_SQL_DESC_LOCAL_TYPE_NAME_set,
	callback_SQL_DESC_LOCAL_TYPE_NAME_default,
    },
    {   Desc_pos_record,
	SQL_DESC_NAME,
	OdbcData::Sqlchar,
	{   Desc_mode_undef,
	    Desc_mode_readwrite,
	    Desc_mode_unused,
	    Desc_mode_readonly,
	    Desc_mode_unused
	},
	callback_SQL_DESC_NAME_set,
	callback_SQL_DESC_NAME_default,
    },
    {   Desc_pos_record,
	SQL_DESC_NULLABLE,
	OdbcData::Smallint,
	{   Desc_mode_undef,
	    Desc_mode_readonly,
	    Desc_mode_unused,
	    Desc_mode_readonly,
	    Desc_mode_unused
	},
	callback_SQL_DESC_NULLABLE_set,
	callback_SQL_DESC_NULLABLE_default,
    },
    {   Desc_pos_record,
	SQL_DESC_NUM_PREC_RADIX,
	OdbcData::Integer,
	{   Desc_mode_undef,
	    Desc_mode_readwrite,
	    Desc_mode_readwrite,
	    Desc_mode_readonly,
	    Desc_mode_readwrite
	},
	callback_SQL_DESC_NUM_PREC_RADIX_set,
	callback_SQL_DESC_NUM_PREC_RADIX_default,
    },
    {   Desc_pos_record,
	SQL_DESC_OCTET_LENGTH,
	OdbcData::Integer,
	{   Desc_mode_undef,
	    Desc_mode_readwrite,
	    Desc_mode_readwrite,
	    Desc_mode_readonly,
	    Desc_mode_readwrite
	},
	callback_SQL_DESC_OCTET_LENGTH_set,
	callback_SQL_DESC_OCTET_LENGTH_default,
    },
    {   Desc_pos_record,
	SQL_DESC_OCTET_LENGTH_PTR,
	OdbcData::IntegerPtr,
	{   Desc_mode_undef,
	    Desc_mode_unused,
	    Desc_mode_readwrite,
	    Desc_mode_unused,
	    Desc_mode_readwrite
	},
	callback_SQL_DESC_OCTET_LENGTH_PTR_set,
	callback_SQL_DESC_OCTET_LENGTH_PTR_default,
    },
    {   Desc_pos_record,
	SQL_DESC_PARAMETER_TYPE,
	OdbcData::Smallint,
	{   Desc_mode_undef,
	    Desc_mode_readwrite,
	    Desc_mode_unused,
	    Desc_mode_unused,
	    Desc_mode_unused
	},
	callback_SQL_DESC_PARAMETER_TYPE_set,
	callback_SQL_DESC_PARAMETER_TYPE_default,
    },
    {   Desc_pos_record,
	SQL_DESC_PRECISION,
	OdbcData::Smallint,
	{   Desc_mode_undef,
	    Desc_mode_readwrite,
	    Desc_mode_readwrite,
	    Desc_mode_readonly,
	    Desc_mode_readwrite
	},
	callback_SQL_DESC_PRECISION_set,
	callback_SQL_DESC_PRECISION_default,
    },
    {   Desc_pos_record,
	SQL_DESC_ROWVER,
	OdbcData::Smallint,
	{   Desc_mode_undef,
	    Desc_mode_readonly,
	    Desc_mode_unused,
	    Desc_mode_readonly,
	    Desc_mode_unused
	},
	callback_SQL_DESC_ROWVER_set,
	callback_SQL_DESC_ROWVER_default,
    },
    {   Desc_pos_record,
	SQL_DESC_SCALE,
	OdbcData::Smallint,
	{   Desc_mode_undef,
	    Desc_mode_readwrite,
	    Desc_mode_readwrite,
	    Desc_mode_readonly,
	    Desc_mode_readwrite
	},
	callback_SQL_DESC_SCALE_set,
	callback_SQL_DESC_SCALE_default,
    },
    {   Desc_pos_record,
	SQL_DESC_SCHEMA_NAME,
	OdbcData::Sqlchar,
	{   Desc_mode_undef,
	    Desc_mode_unused,
	    Desc_mode_unused,
	    Desc_mode_readonly,
	    Desc_mode_unused
	},
	callback_SQL_DESC_SCHEMA_NAME_set,
	callback_SQL_DESC_SCHEMA_NAME_default,
    },
    {   Desc_pos_record,
	SQL_DESC_SEARCHABLE,
	OdbcData::Smallint,
	{   Desc_mode_undef,
	    Desc_mode_unused,
	    Desc_mode_unused,
	    Desc_mode_readonly,
	    Desc_mode_unused
	},
	callback_SQL_DESC_SEARCHABLE_set,
	callback_SQL_DESC_SEARCHABLE_default,
    },
    {   Desc_pos_record,
	SQL_DESC_TABLE_NAME,
	OdbcData::Sqlchar,
	{   Desc_mode_undef,
	    Desc_mode_unused,
	    Desc_mode_unused,
	    Desc_mode_readonly,
	    Desc_mode_unused
	},
	callback_SQL_DESC_TABLE_NAME_set,
	callback_SQL_DESC_TABLE_NAME_default,
    },
    {   Desc_pos_record,
	SQL_DESC_TYPE,
	OdbcData::Smallint,
	{   Desc_mode_undef,
	    Desc_mode_readwrite,
	    Desc_mode_readwrite,
	    Desc_mode_readonly,
	    Desc_mode_readwrite
	},
	callback_SQL_DESC_TYPE_set,
	callback_SQL_DESC_TYPE_default,
    },
    {   Desc_pos_record,
	SQL_DESC_TYPE_NAME,
	OdbcData::Sqlchar,
	{   Desc_mode_undef,
	    Desc_mode_readonly,
	    Desc_mode_unused,
	    Desc_mode_readonly,
	    Desc_mode_unused
	},
	callback_SQL_DESC_TYPE_NAME_set,
	callback_SQL_DESC_TYPE_NAME_default,
    },
    {   Desc_pos_record,
	SQL_DESC_UNNAMED,
	OdbcData::Smallint,
	{   Desc_mode_undef,
	    Desc_mode_readwrite,
	    Desc_mode_unused,
	    Desc_mode_readonly,
	    Desc_mode_unused
	},
	callback_SQL_DESC_UNNAMED_set,
	callback_SQL_DESC_UNNAMED_default,
    },
    {   Desc_pos_record,
	SQL_DESC_UNSIGNED,
	OdbcData::Smallint,
	{   Desc_mode_undef,
	    Desc_mode_readonly,
	    Desc_mode_unused,
	    Desc_mode_readonly,
	    Desc_mode_unused
	},
	callback_SQL_DESC_UNSIGNED_set,
	callback_SQL_DESC_UNSIGNED_default,
    },
    {   Desc_pos_record,
	SQL_DESC_UPDATABLE,
	OdbcData::Smallint,
	{   Desc_mode_undef,
	    Desc_mode_unused,
	    Desc_mode_unused,
	    Desc_mode_readonly,
	    Desc_mode_unused
	},
	callback_SQL_DESC_UPDATABLE_set,
	callback_SQL_DESC_UPDATABLE_default,
    },
    {   Desc_pos_end,
	0,
	OdbcData::Undef,
	{   Desc_mode_undef,
	    Desc_mode_undef,
	    Desc_mode_undef,
	    Desc_mode_undef,
	    Desc_mode_undef
	},
	0,
	0
    },
};
