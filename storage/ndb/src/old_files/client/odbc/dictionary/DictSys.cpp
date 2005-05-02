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
#include <common/ConnArea.hpp>
#include "DictSchema.hpp"
#include "DictTable.hpp"
#include "DictColumn.hpp"
#include "DictSys.hpp"

#define arraySize(x)	sizeof(x)/sizeof(x[0])

#define MAX_SCHEMA_NAME_LENGTH	32
#define MAX_REMARKS_LENGTH	256

// typeinfo

static DictSys::Column
column_ODBC_TYPEINFO[] = {
    DictSys::Column(
	1,
	"TYPE_NAME",
	false,
	SqlType(SqlType::Varchar, 20, false)
    ),
    DictSys::Column(
	2,
	"DATA_TYPE",
	false,
	SqlType(SqlType::Integer, false)
    ),
    DictSys::Column(
	3,
	"COLUMN_SIZE",
	false,
	SqlType(SqlType::Integer, true)
    ),
    DictSys::Column(
	4,
	"LITERAL_PREFIX",
	false,
	SqlType(SqlType::Varchar, 1, true)
    ),
    DictSys::Column(
	5,
	"LITERAL_SUFFIX",
	false,
	SqlType(SqlType::Varchar, 1, true)
    ),
    DictSys::Column(
	6,
	"CREATE_PARAMS",
	false,
	SqlType(SqlType::Varchar, 20, true)
    ),
    DictSys::Column(
	7,
	"NULLABLE",
	false,
	SqlType(SqlType::Integer, false)
    ),
    DictSys::Column(
	8,
	"CASE_SENSITIVE",
	false,
	SqlType(SqlType::Integer, false)
    ),
    DictSys::Column(
	9,
	"SEARCHABLE",
	false,
	SqlType(SqlType::Integer, false)
    ),
    DictSys::Column(
	10,
	"UNSIGNED_ATTRIBUTE",
	false,
	SqlType(SqlType::Integer, true)
    ),
    DictSys::Column(
	11,
	"FIXED_PREC_SCALE",
	false,
	SqlType(SqlType::Integer, false)
    ),
    DictSys::Column(
	12,
	"AUTO_UNIQUE_VALUE",
	false,
	SqlType(SqlType::Integer, true)
    ),
    DictSys::Column(
	13,
	"LOCAL_TYPE_NAME",
	false,
	SqlType(SqlType::Varchar, 20, true)
    ),
    DictSys::Column(
	14,
	"MINIMUM_SCALE",
	false,
	SqlType(SqlType::Integer, true)
    ),
    DictSys::Column(
	15,
	"MAXIMUM_SCALE",
	false,
	SqlType(SqlType::Integer, true)
    ),
    DictSys::Column(
	16,
	"SQL_DATA_TYPE",
	false,
	SqlType(SqlType::Integer, false)
    ),
    DictSys::Column(
	17,
	"SQL_DATETIME_SUB",
	false,
	SqlType(SqlType::Integer, true)
    ),
    DictSys::Column(
	18,
	"NUM_PREC_RADIX",
	false,
	SqlType(SqlType::Integer, true)
    ),
    DictSys::Column(
	19,
	"INTERVAL_PRECISION",
	false,
	SqlType(SqlType::Integer, true)
    )
};

static DictSys::Table
table_ODBC_TYPEINFO(
    DictSys::OdbcTypeinfo,
    "ODBC$TYPEINFO",
    column_ODBC_TYPEINFO,
    arraySize(column_ODBC_TYPEINFO)
);

// tables

static DictSys::Column
column_ODBC_TABLES[] = {
    // perl docs/systables.pl tables -c
    DictSys::Column(
	1,
	"TABLE_CAT",
	false,
	SqlType(SqlType::Varchar, MAX_SCHEMA_NAME_LENGTH, true)
    ),
    DictSys::Column(
	2,
	"TABLE_SCHEM",
	false,
	SqlType(SqlType::Varchar, MAX_SCHEMA_NAME_LENGTH, true)
    ),
    DictSys::Column(
	3,
	"TABLE_NAME",
	false,
	SqlType(SqlType::Varchar, MAX_TAB_NAME_SIZE, false)
    ),
    DictSys::Column(
	4,
	"TABLE_TYPE",
	false,
	SqlType(SqlType::Varchar, 20, false)
    ),
    DictSys::Column(
	5,
	"REMARKS",
	false,
	SqlType(SqlType::Varchar, MAX_REMARKS_LENGTH, true)
    )
};

static DictSys::Table
table_ODBC_TABLES(
    DictSys::OdbcTables,
    "ODBC$TABLES",
    column_ODBC_TABLES,
    arraySize(column_ODBC_TABLES)
);

// columns

static DictSys::Column
column_ODBC_COLUMNS[] = {
    // perl docs/systables.pl columns -c
    DictSys::Column(
	1,
	"TABLE_CAT",
	false,
	SqlType(SqlType::Varchar, MAX_SCHEMA_NAME_LENGTH, true)
    ),
    DictSys::Column(
	2,
	"TABLE_SCHEM",
	false,
	SqlType(SqlType::Varchar, MAX_SCHEMA_NAME_LENGTH, true)
    ),
    DictSys::Column(
	3,
	"TABLE_NAME",
	false,
	SqlType(SqlType::Varchar, MAX_TAB_NAME_SIZE, false)
    ),
    DictSys::Column(
	4,
	"COLUMN_NAME",
	false,
	SqlType(SqlType::Varchar, MAX_ATTR_NAME_SIZE, false)
    ),
    DictSys::Column(
	5,
	"DATA_TYPE",
	false,
	SqlType(SqlType::Integer, false)
    ),
    DictSys::Column(
	6,
	"TYPE_NAME",
	false,
	SqlType(SqlType::Varchar, 20, false)
    ),
    DictSys::Column(
	7,
	"COLUMN_SIZE",
	false,
	SqlType(SqlType::Integer, true)
    ),
    DictSys::Column(
	8,
	"BUFFER_LENGTH",
	false,
	SqlType(SqlType::Integer, true)
    ),
    DictSys::Column(
	9,
	"DECIMAL_DIGITS",
	false,
	SqlType(SqlType::Integer, true)
    ),
    DictSys::Column(
	10,
	"NUM_PREC_RADIX",
	false,
	SqlType(SqlType::Integer, true)
    ),
    DictSys::Column(
	11,
	"NULLABLE",
	false,
	SqlType(SqlType::Integer, false)
    ),
    DictSys::Column(
	12,
	"REMARKS",
	false,
	SqlType(SqlType::Varchar, MAX_REMARKS_LENGTH, true)
    ),
    DictSys::Column(
	13,
	"COLUMN_DEF",
	false,
	SqlType(SqlType::Varchar, MAX_ATTR_DEFAULT_VALUE_SIZE, true)
    ),
    DictSys::Column(
	14,
	"SQL_DATA_TYPE",
	false,
	SqlType(SqlType::Integer, false)
    ),
    DictSys::Column(
	15,
	"SQL_DATETIME_SUB",
	false,
	SqlType(SqlType::Integer, true)
    ),
    DictSys::Column(
	16,
	"CHAR_OCTET_LENGTH",
	false,
	SqlType(SqlType::Integer, true)
    ),
    DictSys::Column(
	17,
	"ORDINAL_POSITION",
	false,
	SqlType(SqlType::Integer, false)
    ),
    DictSys::Column(
	18,
	"IS_NULLABLE",
	false,
	SqlType(SqlType::Varchar, 3, true)
    )
};

static DictSys::Table
table_ODBC_COLUMNS(
    DictSys::OdbcColumns,
    "ODBC$COLUMNS",
    column_ODBC_COLUMNS,
    arraySize(column_ODBC_COLUMNS)
);

// primarykeys

static DictSys::Column
column_ODBC_PRIMARYKEYS[] = {
    DictSys::Column(
	1,
	"TABLE_CAT",
	false,
	SqlType(SqlType::Varchar, MAX_SCHEMA_NAME_LENGTH, true)
    ),
    DictSys::Column(
	2,
	"TABLE_SCHEM",
	false,
	SqlType(SqlType::Varchar, MAX_SCHEMA_NAME_LENGTH, true)
    ),
    DictSys::Column(
	3,
	"TABLE_NAME",
	false,
	SqlType(SqlType::Varchar, MAX_TAB_NAME_SIZE, false)
    ),
    DictSys::Column(
	4,
	"COLUMN_NAME",
	false,
	SqlType(SqlType::Varchar, MAX_ATTR_NAME_SIZE, false)
    ),
    DictSys::Column(
	5,
	"KEY_SEQ",
	false,
	SqlType(SqlType::Integer, false)
    ),
    DictSys::Column(
	6,
	"PK_NAME",
	false,
	SqlType(SqlType::Varchar, MAX_ATTR_NAME_SIZE, true)
    )
};

static DictSys::Table
table_ODBC_PRIMARYKEYS(
    DictSys::OdbcPrimarykeys,
    "ODBC$PRIMARYKEYS",
    column_ODBC_PRIMARYKEYS,
    arraySize(column_ODBC_PRIMARYKEYS)
);

static DictSys::Column
column_DUAL[] = {
    DictSys::Column(
	1,
	"DUMMY",
	false,
	SqlType(SqlType::Varchar, 1, false)
    )
};

static DictSys::Table
table_DUAL(
    DictSys::Dual,
    "DUAL",
    column_DUAL,
    arraySize(column_DUAL)
);

// all tables

static const DictSys::Table*
tableList[] = {
    &table_ODBC_TYPEINFO,
    &table_ODBC_TABLES,
    &table_ODBC_COLUMNS,
    &table_ODBC_PRIMARYKEYS,
    &table_DUAL
};

static const unsigned tableCount = arraySize(tableList);

DictTable*
DictSys::loadTable(Ctx& ctx, DictSchema* schema, const BaseString& name)
{
    const Table* tp = 0;
    for (unsigned i = 0; i < tableCount; i++) {
	if (strcmp(tableList[i]->m_name, name.c_str()) == 0) {
	    tp = tableList[i];
	    break;
	}
    }
    if (tp == 0)
	return 0;
    DictTable* table = new DictTable(schema->m_connArea, tp->m_name, tp->m_columnCount);
    table->sysId(tp->m_id);
    schema->addTable(table);
    for (unsigned position = 1; position <= tp->m_columnCount; position++) {
	const Column* cp = &tp->m_columnList[position - 1];
	ctx_assert(cp->m_position == position);
	const SqlType& sqlType = cp->m_sqlType;
	DictColumn* column = new DictColumn(table->m_connArea, cp->m_name, sqlType);
	table->setColumn(position, column);
	column->m_key = cp->m_key;
	if (column->m_key)
	    table->m_keys.push_back(column);
    }
    ctx_log3(("%s: system table defined", name.c_str()));
    return table;
}
