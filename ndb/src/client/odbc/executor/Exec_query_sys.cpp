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

#include <NdbApi.hpp>
#include <common/ResultArea.hpp>
#include <codegen/Code_query_sys.hpp>
#include <codegen/Code_table.hpp>

#define NULL_CHAR	((char*)0)
#define NULL_INT	(-2147483647)

struct Typeinfo {
    const char* m_type_name;		// 1
    int m_data_type;			// 2
    int m_column_size;			// 3
    const char* m_literal_prefix;	// 4
    const char* m_literal_suffix;	// 5
    const char* m_create_params;	// 6
    int m_nullable;			// 7
    int m_case_sensitive;		// 8
    int m_searchable;			// 9
    int m_unsigned_attribute;		// 10
    int m_fixed_prec_scale;		// 11
    int m_auto_unique_value;		// 12
    const char* m_local_type_name;	// 13
    int m_minimum_scale;		// 14
    int m_maximum_scale;		// 15
    int m_sql_data_type;		// 16
    int m_sql_datetime_sub;		// 17
    int m_num_prec_radix;		// 18
    int m_interval_precision;		// 19
};

static const Typeinfo
typeinfoList[] = {
    {	"CHAR",				// 1
	SQL_CHAR,			// 2
	8000,				// 3
	"'",				// 4
	"'",				// 5
	"length",			// 6
	SQL_NULLABLE,			// 7
	SQL_TRUE,			// 8
	SQL_SEARCHABLE,			// 9
	NULL_INT,			// 10
	SQL_FALSE,			// 11
	NULL_INT,			// 12
	NULL_CHAR,			// 13
	NULL_INT,			// 14
	NULL_INT,			// 15
	SQL_CHAR,			// 16
	NULL_INT,			// 17
	NULL_INT,			// 18
	NULL_INT			// 19
    },
    {	"VARCHAR",			// 1
	SQL_VARCHAR,			// 2
	8000,				// 3
	"'",				// 4
	"'",				// 5
	"length",			// 6
	SQL_NULLABLE,			// 7
	SQL_TRUE,			// 8
	SQL_SEARCHABLE,			// 9
	NULL_INT,			// 10
	SQL_FALSE,			// 11
	NULL_INT,			// 12
	NULL_CHAR,			// 13
	NULL_INT,			// 14
	NULL_INT,			// 15
	SQL_VARCHAR,			// 16
	NULL_INT,			// 17
	NULL_INT,			// 18
	NULL_INT			// 19
    },
    {	"BINARY",			// 1
	SQL_BINARY,			// 2
	8000,				// 3
	"'",				// 4
	"'",				// 5
	"length",			// 6
	SQL_NULLABLE,			// 7
	SQL_TRUE,			// 8
	SQL_SEARCHABLE,			// 9
	NULL_INT,			// 10
	SQL_FALSE,			// 11
	NULL_INT,			// 12
	NULL_CHAR,			// 13
	NULL_INT,			// 14
	NULL_INT,			// 15
	SQL_BINARY,			// 16
	NULL_INT,			// 17
	NULL_INT,			// 18
	NULL_INT			// 19
    },
    {	"VARBINARY",			// 1
	SQL_VARBINARY,			// 2
	8000,				// 3
	"'",				// 4
	"'",				// 5
	"length",			// 6
	SQL_NULLABLE,			// 7
	SQL_TRUE,			// 8
	SQL_SEARCHABLE,			// 9
	NULL_INT,			// 10
	SQL_FALSE,			// 11
	NULL_INT,			// 12
	NULL_CHAR,			// 13
	NULL_INT,			// 14
	NULL_INT,			// 15
	SQL_VARBINARY,			// 16
	NULL_INT,			// 17
	NULL_INT,			// 18
	NULL_INT			// 19
    },
    {	"SMALLINT",			// 1
	SQL_SMALLINT,			// 2
	4,				// 3
	NULL_CHAR,			// 4
	NULL_CHAR,			// 5
	NULL_CHAR,			// 6
	SQL_NULLABLE,			// 7
	SQL_FALSE,			// 8
	SQL_SEARCHABLE,			// 9
	SQL_FALSE,			// 10
	SQL_TRUE,			// 11
	SQL_FALSE,			// 12
	NULL_CHAR,			// 13
	NULL_INT,			// 14
	NULL_INT,			// 15
	SQL_SMALLINT,			// 16
	NULL_INT,			// 17
	10,				// 18
	NULL_INT			// 19
    },
    {	"SMALLINT UNSIGNED",		// 1
	SQL_SMALLINT,			// 2
	4,				// 3
	NULL_CHAR,			// 4
	NULL_CHAR,			// 5
	NULL_CHAR,			// 6
	SQL_NULLABLE,			// 7
	SQL_FALSE,			// 8
	SQL_SEARCHABLE,			// 9
	SQL_TRUE,			// 10
	SQL_TRUE,			// 11
	SQL_FALSE,			// 12
	NULL_CHAR,			// 13
	NULL_INT,			// 14
	NULL_INT,			// 15
	SQL_SMALLINT,			// 16
	NULL_INT,			// 17
	10,				// 18
	NULL_INT			// 19
    },
    {	"INT",				// 1
	SQL_INTEGER,			// 2
	9,				// 3
	NULL_CHAR,			// 4
	NULL_CHAR,			// 5
	NULL_CHAR,			// 6
	SQL_NULLABLE,			// 7
	SQL_FALSE,			// 8
	SQL_SEARCHABLE,			// 9
	SQL_FALSE,			// 10
	SQL_TRUE,			// 11
	SQL_FALSE,			// 12
	NULL_CHAR,			// 13
	NULL_INT,			// 14
	NULL_INT,			// 15
	SQL_INTEGER,			// 16
	NULL_INT,			// 17
	10,				// 18
	NULL_INT			// 19
    },
    {	"INT UNSIGNED",			// 1
	SQL_INTEGER,			// 2
	9,				// 3
	NULL_CHAR,			// 4
	NULL_CHAR,			// 5
	NULL_CHAR,			// 6
	SQL_NULLABLE,			// 7
	SQL_FALSE,			// 8
	SQL_SEARCHABLE,			// 9
	SQL_TRUE,			// 10
	SQL_TRUE,			// 11
	SQL_FALSE,			// 12
	NULL_CHAR,			// 13
	NULL_INT,			// 14
	NULL_INT,			// 15
	SQL_INTEGER,			// 16
	NULL_INT,			// 17
	10,				// 18
	NULL_INT			// 19
    },
    {	"BIGINT",			// 1
	SQL_BIGINT,			// 2
	19,				// 3
	NULL_CHAR,			// 4
	NULL_CHAR,			// 5
	NULL_CHAR,			// 6
	SQL_NULLABLE,			// 7
	SQL_FALSE,			// 8
	SQL_SEARCHABLE,			// 9
	SQL_FALSE,			// 10
	SQL_TRUE,			// 11
	SQL_FALSE,			// 12
	NULL_CHAR,			// 13
	NULL_INT,			// 14
	NULL_INT,			// 15
	SQL_BIGINT,			// 16
	NULL_INT,			// 17
	10,				// 18
	NULL_INT			// 19
    },
    {	"BIGINT UNSIGNED",		// 1
	SQL_BIGINT,			// 2
	19,				// 3
	NULL_CHAR,			// 4
	NULL_CHAR,			// 5
	NULL_CHAR,			// 6
	SQL_NULLABLE,			// 7
	SQL_FALSE,			// 8
	SQL_SEARCHABLE,			// 9
	SQL_TRUE,			// 10
	SQL_TRUE,			// 11
	SQL_FALSE,			// 12
	NULL_CHAR,			// 13
	NULL_INT,			// 14
	NULL_INT,			// 15
	SQL_BIGINT,			// 16
	NULL_INT,			// 17
	10,				// 18
	NULL_INT			// 19
    },
    {	"REAL",				// 1
	SQL_REAL,			// 2
	31,				// 3
	NULL_CHAR,			// 4
	NULL_CHAR,			// 5
	NULL_CHAR,			// 6
	SQL_NULLABLE,			// 7
	SQL_FALSE,			// 8
	SQL_SEARCHABLE,			// 9
	SQL_FALSE,			// 10
	SQL_TRUE,			// 11
	SQL_FALSE,			// 12
	NULL_CHAR,			// 13
	NULL_INT,			// 14
	NULL_INT,			// 15
	SQL_REAL,			// 16
	NULL_INT,			// 17
	2,				// 18
	NULL_INT			// 19
    },
    {	"FLOAT",			// 1
	SQL_DOUBLE,			// 2
	63,				// 3
	NULL_CHAR,			// 4
	NULL_CHAR,			// 5
	NULL_CHAR,			// 6
	SQL_NULLABLE,			// 7
	SQL_FALSE,			// 8
	SQL_SEARCHABLE,			// 9
	SQL_FALSE,			// 10
	SQL_TRUE,			// 11
	SQL_FALSE,			// 12
	NULL_CHAR,			// 13
	NULL_INT,			// 14
	NULL_INT,			// 15
	SQL_DOUBLE,			// 16
	NULL_INT,			// 17
	2,				// 18
	NULL_INT			// 19
    },
    {	"DATETIME",			// 1
	SQL_TYPE_TIMESTAMP,		// 2
	30,				// 3
	NULL_CHAR,			// 4
	NULL_CHAR,			// 5
	NULL_CHAR,			// 6
	SQL_NULLABLE,			// 7
	SQL_FALSE,			// 8
	SQL_SEARCHABLE,			// 9
	SQL_FALSE,			// 10
	SQL_TRUE,			// 11
	SQL_FALSE,			// 12
	NULL_CHAR,			// 13
	NULL_INT,			// 14
	NULL_INT,			// 15
	SQL_DATETIME,			// 16	XXX
	NULL_INT,			// 17
	2,				// 18
	NULL_INT			// 19
    }
};

const unsigned
typeinfoCount = sizeof(typeinfoList)/sizeof(typeinfoList[0]);

void
Exec_query_sys::execImpl(Ctx& ctx, Ctl& ctl)
{
    const Code& code = getCode();
    Data& data = getData();
    Ndb* const ndb = ndbObject();
    NdbDictionary::Dictionary* ndbDictionary = ndb->getDictionary();
    if (ndbDictionary == 0) {
	ctx.pushStatus(ndb, "getDictionary");
	return;
    }
    if (code.m_sysId == DictSys::OdbcTypeinfo || code.m_sysId == DictSys::Dual) {
	data.m_rowPos = 0;	// at first entry
	return;
    }
    if (code.m_sysId == DictSys::OdbcTables || code.m_sysId == DictSys::OdbcColumns || code.m_sysId == DictSys::OdbcPrimarykeys) {
	// take all objects
	if (ndbDictionary->listObjects(data.m_objectList) == -1) {
	    ctx.pushStatus(ndb, "listObjects");
	    return;
	}
	data.m_tablePos = 0;	// at first entry
	data.m_attrPos = 0;
	data.m_keyPos = 0;
	return;
    }
    ctx_assert(false);
}

static bool
isNdbTable(const NdbDictionary::Dictionary::List::Element& element)
{
    switch (element.type) {
    //case NdbDictionary::Object::SystemTable:
    case NdbDictionary::Object::UserTable:
    case NdbDictionary::Object::UniqueHashIndex:
    case NdbDictionary::Object::HashIndex:
    case NdbDictionary::Object::UniqueOrderedIndex:
    case NdbDictionary::Object::OrderedIndex:
	return true;
    default:
	break;
    }
    return false;
}


bool
Exec_query_sys::fetchImpl(Ctx &ctx, Ctl& ctl)
{
    const Code& code = getCode();
    Data& data = getData();
    Ndb* const ndb = ndbObject();
    NdbDictionary::Dictionary* ndbDictionary = ndb->getDictionary();
    if (ndbDictionary == 0) {
	ctx.pushStatus(ndb, "getDictionary");
	return false;
    }
    if (code.m_sysId == DictSys::OdbcTypeinfo) {
	if (data.m_rowPos >= typeinfoCount) {
	    return false;
	}
	SqlRow& row = data.m_sqlRow;
	const Typeinfo& typeinfo = typeinfoList[data.m_rowPos++];
	for (unsigned i = 1; i <= code.m_attrCount; i++) {
	    SqlField& f = data.m_sqlRow.getEntry(i);
	    switch (1 + code.m_attrId[i]) {
	    case 1:
		if (typeinfo.m_type_name == NULL_CHAR)
		    f.sqlNull(true);
		else
		    f.sqlVarchar(typeinfo.m_type_name, SQL_NTS);
		break;
	    case 2:
		if (typeinfo.m_data_type == NULL_INT)
		    f.sqlNull(true);
		else
		    f.sqlInteger(typeinfo.m_data_type);
		break;
	    case 3:
		if (typeinfo.m_column_size == NULL_INT)
		    f.sqlNull(true);
		else
		    f.sqlInteger(typeinfo.m_column_size);
		break;
	    case 4:
		if (typeinfo.m_literal_prefix == NULL_CHAR)
		    f.sqlNull(true);
		else
		    f.sqlVarchar(typeinfo.m_literal_prefix, SQL_NTS);
		break;
	    case 5:
		if (typeinfo.m_literal_suffix == NULL_CHAR)
		    f.sqlNull(true);
		else
		    f.sqlVarchar(typeinfo.m_literal_suffix, SQL_NTS);
		break;
	    case 6:
		if (typeinfo.m_create_params == NULL_CHAR)
		    f.sqlNull(true);
		else
		    f.sqlVarchar(typeinfo.m_create_params, SQL_NTS);
		break;
	    case 7:
		if (typeinfo.m_nullable == NULL_INT)
		    f.sqlNull(true);
		else
		    f.sqlInteger(typeinfo.m_nullable);
		break;
	    case 8:
		if (typeinfo.m_case_sensitive == NULL_INT)
		    f.sqlNull(true);
		else
		    f.sqlInteger(typeinfo.m_case_sensitive);
		break;
	    case 9:
		if (typeinfo.m_searchable == NULL_INT)
		    f.sqlNull(true);
		else
		    f.sqlInteger(typeinfo.m_searchable);
		break;
	    case 10:
		if (typeinfo.m_unsigned_attribute == NULL_INT)
		    f.sqlNull(true);
		else
		    f.sqlInteger(typeinfo.m_unsigned_attribute);
		break;
	    case 11:
		if (typeinfo.m_fixed_prec_scale == NULL_INT)
		    f.sqlNull(true);
		else
		    f.sqlInteger(typeinfo.m_fixed_prec_scale);
		break;
	    case 12:
		if (typeinfo.m_auto_unique_value == NULL_INT)
		    f.sqlNull(true);
		else
		    f.sqlInteger(typeinfo.m_auto_unique_value);
		break;
	    case 13:
		if (typeinfo.m_local_type_name == NULL_CHAR)
		    f.sqlNull(true);
		else
		    f.sqlVarchar(typeinfo.m_local_type_name, SQL_NTS);
		break;
	    case 14:
		if (typeinfo.m_minimum_scale == NULL_INT)
		    f.sqlNull(true);
		else
		    f.sqlInteger(typeinfo.m_minimum_scale);
		break;
	    case 15:
		if (typeinfo.m_maximum_scale == NULL_INT)
		    f.sqlNull(true);
		else
		    f.sqlInteger(typeinfo.m_maximum_scale);
		break;
	    case 16:
		if (typeinfo.m_sql_data_type == NULL_INT)
		    f.sqlNull(true);
		else
		    f.sqlInteger(typeinfo.m_sql_data_type);
		break;
	    case 17:
		if (typeinfo.m_sql_datetime_sub == NULL_INT)
		    f.sqlNull(true);
		else
		    f.sqlInteger(typeinfo.m_sql_datetime_sub);
		break;
	    case 18:
		if (typeinfo.m_sql_datetime_sub == NULL_INT)
		    f.sqlNull(true);
		else
		    f.sqlInteger(typeinfo.m_sql_datetime_sub);
		break;
	    case 19:
		if (typeinfo.m_interval_precision == NULL_INT)
		    f.sqlNull(true);
		else
		    f.sqlInteger(typeinfo.m_interval_precision);
		break;
	    default:
		ctx_assert(false);
		break;
	    }
	}
	return true;
    }
    if (code.m_sysId == DictSys::OdbcTables) {
	if (data.m_tablePos >= data.m_objectList.count) {
	    return false;
	}
	NdbDictionary::Dictionary::List::Element& element = data.m_objectList.elements[data.m_tablePos++];
	for (unsigned i = 1; i <= code.m_attrCount; i++) {
	    SqlField& f = data.m_sqlRow.getEntry(i);
	    switch (1 + code.m_attrId[i]) {
	    case 1:		// TABLE_CAT
		f.sqlNull(true);
		break;
	    case 2:		// TABLE_SCHEM
		f.sqlNull(true);
		break;
	    case 3:		// TABLE_NAME
		f.sqlVarchar(element.name, SQL_NTS);
		break;
	    case 4: {		// TABLE_TYPE
		if (element.type == NdbDictionary::Object::SystemTable)
		    f.sqlVarchar("SYSTEM TABLE", SQL_NTS);
		else if (element.type == NdbDictionary::Object::UserTable)
		    f.sqlVarchar("TABLE", SQL_NTS);
		else if (element.type == NdbDictionary::Object::UniqueHashIndex)
		    f.sqlVarchar("UNIQUE HASH INDEX", SQL_NTS);
		else if (element.type == NdbDictionary::Object::HashIndex)
		    f.sqlVarchar("HASH INDEX", SQL_NTS);
		else if (element.type == NdbDictionary::Object::UniqueOrderedIndex)
		    f.sqlVarchar("UNIQUE INDEX", SQL_NTS);
		else if (element.type == NdbDictionary::Object::OrderedIndex)
		    f.sqlVarchar("INDEX", SQL_NTS);
		else if (element.type == NdbDictionary::Object::IndexTrigger)
		    f.sqlVarchar("INDEX TRIGGER", SQL_NTS);
		else if (element.type == NdbDictionary::Object::SubscriptionTrigger)
		    f.sqlVarchar("SUBSCRIPTION TRIGGER", SQL_NTS);
		else if (element.type == NdbDictionary::Object::ReadOnlyConstraint)
		    f.sqlVarchar("READ ONLY CONSTRAINT", SQL_NTS);
		else
		    f.sqlVarchar("UNKNOWN", SQL_NTS);
		}
		break;
	    case 5:		// REMARKS
		f.sqlNull(true);
		break;
	    default:
		ctx_assert(false);
		break;
	    }
	}
	return true;
    }
    if (code.m_sysId == DictSys::OdbcColumns) {
	if (data.m_tablePos >= data.m_objectList.count) {
	    return false;
	}
	const NdbDictionary::Dictionary::List::Element& element = data.m_objectList.elements[data.m_tablePos];
	unsigned nattr;
	const NdbDictionary::Table* ndbTable;
	nattr = 0;
	if (isNdbTable(element)) {
	    ndbTable = ndbDictionary->getTable(element.name);
	    if (ndbTable == 0) {
		ctx.pushStatus(ndbDictionary->getNdbError(), "getTable %s", element.name);
		return false;
	    }
	    nattr = ndbTable->getNoOfColumns();
	}
	while (data.m_attrPos >= nattr) {
	    if (++data.m_tablePos >= data.m_objectList.count) {
		return false;
	    }
	    const NdbDictionary::Dictionary::List::Element& element = data.m_objectList.elements[data.m_tablePos];
	    nattr = 0;
	    if (isNdbTable(element)) {
		ndbTable = ndbDictionary->getTable(element.name);
		if (ndbTable == 0) {
		    ctx.pushStatus(ndbDictionary->getNdbError(), "getTable %s", element.name);
		    return false;
		}
		data.m_attrPos = 0;
		nattr = ndbTable->getNoOfColumns();
	    }
	}
	int attrId = data.m_attrPos++;
	const NdbDictionary::Column* ndbColumn = ndbTable->getColumn(attrId);
	if (ndbColumn == 0) {
	    ctx.pushStatus(ndbDictionary->getNdbError(), "getColumn %s.%d", ndbTable->getName(), attrId);
	    return false;
	}
	SqlType sqlType(ctx, ndbColumn);
	if (! ctx.ok())
	    return false;
	const char* p;
	for (unsigned i = 1; i <= code.m_attrCount; i++) {
	    SqlField& f = data.m_sqlRow.getEntry(i);
	    switch (1 + code.m_attrId[i]) {
	    case 1:		// TABLE_CAT
		f.sqlNull(true);
		break;
	    case 2:		// TABLE_SCHEM
		f.sqlNull(true);
		break;
	    case 3:		// TABLE_NAME
		f.sqlVarchar(ndbTable->getName(), SQL_NTS);
		break;
	    case 4:		// COLUMN_NAME
		f.sqlVarchar(ndbColumn->getName(), SQL_NTS);
		break;
	    case 5:		// DATA_TYPE
		f.sqlInteger(sqlType.type());
		break;
	    case 6:		// TYPE_NAME
		f.sqlVarchar(sqlType.typeName(), SQL_NTS);
		break;
	    case 7:		// COLUMN_SIZE
		f.sqlInteger(sqlType.displaySize());
		break;
	    case 8:		// BUFFER_LENGTH
		f.sqlInteger(sqlType.size());
		break;
	    case 9:		// DECIMAL_DIGITS
		if (sqlType.type() == SqlType::Char)
		    f.sqlNull(true);
		else
		    f.sqlInteger(0);
		break;
	    case 10:		// NUM_PREC_RADIX
		if (sqlType.type() == SqlType::Integer || sqlType.type() == SqlType::Bigint)
		    f.sqlInteger(10);
		else
		    f.sqlNull(true);
		break;
	    case 11:		// NULLABLE
		if (sqlType.nullable())
		    f.sqlInteger(SQL_NULLABLE);
		else
		    f.sqlInteger(SQL_NO_NULLS);
		break;
	    case 12:		// REMARKS
		f.sqlNull(true);
		break;
	    case 13:		// COLUMN_DEF
		if ((p = ndbColumn->getDefaultValue()) != 0)
		    f.sqlVarchar(p, SQL_NTS);
		else
		    f.sqlNull(true);
		break;
	    case 14:		// SQL_DATA_TYPE
		f.sqlInteger(sqlType.type());
		break;
	    case 15:		// SQL_DATETIME_SUB
		f.sqlNull(true);
		break;
	    case 16:		// CHAR_OCTET_LENGTH
		if (sqlType.type() == SqlType::Char)
		    f.sqlInteger(sqlType.length());
		else
		    f.sqlNull(true);
		break;
	    case 17:		// ORDINAL_POSITION
		f.sqlInteger(1 + attrId);
		break;
	    case 18:		// IS_NULLABLE
		if (sqlType.nullable())
		    f.sqlVarchar("YES", SQL_NTS);
		else
		    f.sqlVarchar("NO", SQL_NTS);
		break;
		break;
	    default:
		ctx_assert(false);
		break;
	    }
	}
	return true;
    }
    if (code.m_sysId == DictSys::OdbcPrimarykeys) {
	if (data.m_tablePos >= data.m_objectList.count) {
	    return false;
	}
	NdbDictionary::Dictionary::List::Element& element = data.m_objectList.elements[data.m_tablePos];
	unsigned nkeys;
	const NdbDictionary::Table* ndbTable;
	nkeys = 0;
	if (isNdbTable(element)) {
	    ndbTable = ndbDictionary->getTable(element.name);
	    if (ndbTable == 0) {
		ctx.pushStatus(ndbDictionary->getNdbError(), "getTable %s", element.name);
		return false;
	    }
	    nkeys = ndbTable->getNoOfPrimaryKeys();
	}
	while (data.m_keyPos >= nkeys) {
	    if (++data.m_tablePos >= data.m_objectList.count) {
		return false;
	    }
	    NdbDictionary::Dictionary::List::Element& element = data.m_objectList.elements[data.m_tablePos];
	    nkeys = 0;
	    if (isNdbTable(element)) {
		ndbTable = ndbDictionary->getTable(element.name);
		if (ndbTable == 0) {
		    ctx.pushStatus(ndbDictionary->getNdbError(), "getTable %s", element.name);
		    return false;
		}
		data.m_keyPos = 0;
		nkeys = ndbTable->getNoOfPrimaryKeys();
	    }
	}
	unsigned keyPos = data.m_keyPos++;
	const char* keyName = ndbTable->getPrimaryKey(keyPos);
	if (keyName == 0)
	    keyName = "?";
	for (unsigned i = 1; i <= code.m_attrCount; i++) {
	    SqlField& f = data.m_sqlRow.getEntry(i);
	    switch (1 + code.m_attrId[i]) {
	    case 1:		// TABLE_CAT
		f.sqlNull(true);
		break;
	    case 2:		// TABLE_SCHEM
		f.sqlNull(true);
		break;
	    case 3:		// TABLE_NAME
		f.sqlVarchar(ndbTable->getName(), SQL_NTS);
		break;
	    case 4:		// COLUMN_NAME
		f.sqlVarchar(keyName, SQL_NTS);
		break;
	    case 5:		// KEY_SEQ
		f.sqlInteger(1 + keyPos);
		break;
	    case 6:		// PK_NAME
		f.sqlNull(true);
		break;
	    default:
		ctx_assert(false);
		break;
	    }
	}
	return true;
    }
    if (code.m_sysId == DictSys::Dual) {
	if (data.m_rowPos > 0) {
	    return false;
	}
	data.m_rowPos++;
	for (unsigned i = 1; i <= code.m_attrCount; i++) {
	    SqlField& f = data.m_sqlRow.getEntry(i);
	    switch (1 + code.m_attrId[i]) {
	    case 1:		// DUMMY
		f.sqlVarchar("X", 1);
		break;
	    default:
		ctx_assert(false);
		break;
	    }
	}
	return true;
    }
    ctx_assert(false);
    return false;
}
