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

HandleDbc::InfoTab
HandleDbc::m_infoTab[] = {
    {	SQL_ACCESSIBLE_PROCEDURES,
	InfoTab::YesNo,
	0L,
	"N"
    },
    {	SQL_ACCESSIBLE_TABLES,
	InfoTab::YesNo,
	0L,
	"Y"
    },
    {	SQL_ACTIVE_ENVIRONMENTS,
	InfoTab::Short,
	0L,
	0
    },
    {	SQL_AGGREGATE_FUNCTIONS,
	InfoTab::Bitmask,
	SQL_AF_AVG | SQL_AF_COUNT | SQL_AF_MAX | SQL_AF_MIN | SQL_AF_SUM,
	0
    },
    {	SQL_ALTER_DOMAIN,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_ALTER_TABLE,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_ASYNC_MODE,
	InfoTab::Long,
	SQL_AM_NONE,
	0
    },
    {	SQL_BATCH_ROW_COUNT,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_BATCH_SUPPORT,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_BOOKMARK_PERSISTENCE,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_CATALOG_LOCATION,
	InfoTab::Short,
	0L,
	0
    },
    {	SQL_CATALOG_NAME,
	InfoTab::YesNo,
	0L,
	"N"
    },
    {	SQL_CATALOG_NAME_SEPARATOR,
	InfoTab::Char,
	0L,
	""
    },
    {	SQL_CATALOG_TERM,
	InfoTab::Char,
	0L,
	""
    },
    {	SQL_CATALOG_USAGE,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_COLLATION_SEQ,
	InfoTab::Char,
	0L,
	"ISO 8859-1"
    },
    {	SQL_COLUMN_ALIAS,
	InfoTab::YesNo,
	0L,
	"Y"
    },
    {	SQL_CONCAT_NULL_BEHAVIOR,
	InfoTab::Short,
	0L,
	0
    },
    {	SQL_CONVERT_BIGINT,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_CONVERT_BINARY,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_CONVERT_BIT,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_CONVERT_CHAR,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_CONVERT_DATE,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_CONVERT_DECIMAL,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_CONVERT_DOUBLE,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_CONVERT_FLOAT,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_CONVERT_FUNCTIONS,
	InfoTab::Bitmask,
	0L,
	0
    },
#if 0
    {	SQL_CONVERT_GUID,
	InfoTab::Bitmask,
	0L,
	0
    },
#endif
    {	SQL_CONVERT_INTEGER,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_CONVERT_INTERVAL_DAY_TIME,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_CONVERT_INTERVAL_YEAR_MONTH,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_CONVERT_LONGVARBINARY,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_CONVERT_LONGVARCHAR,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_CONVERT_NUMERIC,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_CONVERT_REAL,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_CONVERT_SMALLINT,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_CONVERT_TIME,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_CONVERT_TIMESTAMP,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_CONVERT_TINYINT,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_CONVERT_VARBINARY,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_CONVERT_VARCHAR,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_CORRELATION_NAME,
	InfoTab::Bitmask,
	SQL_CN_ANY,
	0
    },
    {	SQL_CREATE_ASSERTION,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_CREATE_CHARACTER_SET,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_CREATE_COLLATION,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_CREATE_DOMAIN,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_CREATE_SCHEMA,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_CREATE_TABLE,
	InfoTab::Bitmask,
	SQL_CT_CREATE_TABLE,
	0
    },
    {	SQL_CREATE_TRANSLATION,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_CREATE_VIEW,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_CURSOR_COMMIT_BEHAVIOR,
	InfoTab::Short,
	SQL_CB_CLOSE,
	0
    },
    {	SQL_CURSOR_ROLLBACK_BEHAVIOR,
	InfoTab::Short,
	SQL_CB_CLOSE,
	0
    },
    {	SQL_CURSOR_SENSITIVITY,
	InfoTab::Long,
	0L,
	0
    },
    {	SQL_DATABASE_NAME,
	InfoTab::Char,
	0L,
	""
    },
    {	SQL_DATA_SOURCE_NAME,
	InfoTab::Char,
	0L,
	""
    },
    {	SQL_DATA_SOURCE_READ_ONLY,
	InfoTab::YesNo,
	0L,
	"N"
    },
    {	SQL_DATETIME_LITERALS,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_DBMS_NAME,
	InfoTab::Char,
	0L,
	""
    },
    {	SQL_DBMS_VER,
	InfoTab::Char,
	0L,
	"01.43.0000"
    },
    {	SQL_DDL_INDEX,
	InfoTab::Long,
	0L,
	0
    },
    {	SQL_DEFAULT_TXN_ISOLATION,
	InfoTab::Long,
	SQL_TXN_READ_COMMITTED,
	0
    },
    {	SQL_DESCRIBE_PARAMETER,
	InfoTab::YesNo,
	0L,
	"N"
    },
    {	SQL_DM_VER,
	InfoTab::Char,
	0L,
	""
    },
    {	SQL_DRIVER_HDBC,
	InfoTab::Long,
	0L,
	0
    },
    {	SQL_DRIVER_HDESC,
	InfoTab::Long,
	0L,
	0
    },
    {	SQL_DRIVER_HLIB,
	InfoTab::Long,
	0L,
	0
    },
    {	SQL_DRIVER_HSTMT,
	InfoTab::Long,
	0L,
	0
    },
    {	SQL_DRIVER_NAME,
	InfoTab::Char,
	0L,
	""
    },
    {	SQL_DRIVER_ODBC_VER,
	InfoTab::Char,
	0L,
	"03.00"
    },
    {	SQL_DRIVER_VER,
	InfoTab::Char,
	0L,
	"00.10.0000"
    },
    {	SQL_DROP_ASSERTION,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_DROP_CHARACTER_SET,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_DROP_COLLATION,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_DROP_DOMAIN,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_DROP_SCHEMA,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_DROP_TABLE,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_DROP_TRANSLATION,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_DROP_VIEW,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_DTC_TRANSITION_COST,	// not in older MS docs
	InfoTab::Bitmask,
	0L,
	0				// SQL_DTC_ENLIST_EXPENSIVE | SQL_DTC_UNENLIST_EXPENSIVE
    },
    {	SQL_DYNAMIC_CURSOR_ATTRIBUTES1,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_DYNAMIC_CURSOR_ATTRIBUTES2,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_EXPRESSIONS_IN_ORDERBY,
	InfoTab::Char,
	0L,
	"Y"
    },
    {	SQL_FILE_USAGE,
	InfoTab::Short,
	0L,
	0
    },
    {	SQL_FORWARD_ONLY_CURSOR_ATTRIBUTES1,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_FORWARD_ONLY_CURSOR_ATTRIBUTES2,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_GETDATA_EXTENSIONS,
	InfoTab::Bitmask,
	SQL_GD_ANY_COLUMN | SQL_GD_ANY_ORDER | SQL_GD_BOUND,
	0
    },
    {	SQL_GROUP_BY,
	InfoTab::Short,
	SQL_GB_NOT_SUPPORTED,
	0
    },
    {	SQL_IDENTIFIER_CASE,
	InfoTab::Short,
	SQL_IC_UPPER,
	0
    },
    {	SQL_IDENTIFIER_QUOTE_CHAR,
	InfoTab::Char,
	0L,
	"\""
    },
    {	SQL_INDEX_KEYWORDS,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_INFO_SCHEMA_VIEWS,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_INSERT_STATEMENT,
	InfoTab::Bitmask,
	SQL_IS_INSERT_LITERALS | SQL_IS_SELECT_INTO,
	0
    },
    {	SQL_INTEGRITY,
	InfoTab::YesNo,
	0L,
	"N"
    },
    {	SQL_KEYSET_CURSOR_ATTRIBUTES1,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_KEYSET_CURSOR_ATTRIBUTES2,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_KEYWORDS,
	InfoTab::Char,
	0L,
	""
    },
    {	SQL_LIKE_ESCAPE_CLAUSE,
	InfoTab::YesNo,
	0L,
	"N"
    },
    {	SQL_MAX_ASYNC_CONCURRENT_STATEMENTS,
	InfoTab::Long,
	0L,
	0
    },
    {	SQL_MAX_BINARY_LITERAL_LEN,
	InfoTab::Long,
	0L,
	0
    },
    {	SQL_MAX_CATALOG_NAME_LEN,
	InfoTab::Short,
	0L,
	0
    },
    {	SQL_MAX_CHAR_LITERAL_LEN,
	InfoTab::Long,
	0L,
	0
    },
    {	SQL_MAX_COLUMN_NAME_LEN,
	InfoTab::Short,
	16,
	0
    },
    {	SQL_MAX_COLUMNS_IN_GROUP_BY,
	InfoTab::Short,
	0L,
	0
    },
    {	SQL_MAX_COLUMNS_IN_INDEX,
	InfoTab::Short,
	0L,
	0
    },
    {	SQL_MAX_COLUMNS_IN_ORDER_BY,
	InfoTab::Short,
	0L,
	0
    },
    {	SQL_MAX_COLUMNS_IN_SELECT,
	InfoTab::Short,
	0L,
	0
    },
    {	SQL_MAX_COLUMNS_IN_TABLE,
	InfoTab::Short,
	0L,
	0
    },
    {	SQL_MAX_CONCURRENT_ACTIVITIES,
	InfoTab::Short,
	0L,
	0
    },
    {	SQL_MAX_CURSOR_NAME_LEN,
	InfoTab::Short,
	0L,
	0
    },
    {	SQL_MAX_DRIVER_CONNECTIONS,
	InfoTab::Short,
	0L,
	0
    },
    {	SQL_MAX_IDENTIFIER_LEN,
	InfoTab::Short,
	0L,
	0
    },
    {	SQL_MAX_INDEX_SIZE,
	InfoTab::Long,
	0L,
	0
    },
    {	SQL_MAX_PROCEDURE_NAME_LEN,
	InfoTab::Short,
	0L,
	0
    },
    {	SQL_MAX_ROW_SIZE,
	InfoTab::Long,
	8000,
	0
    },
    {	SQL_MAX_ROW_SIZE_INCLUDES_LONG,
	InfoTab::YesNo,
	0L,
	"Y"
    },
    {	SQL_MAX_SCHEMA_NAME_LEN,
	InfoTab::Short,
	0L,
	0
    },
    {	SQL_MAX_STATEMENT_LEN,
	InfoTab::Long,
	0L,
	0
    },
    {	SQL_MAX_TABLE_NAME_LEN,
	InfoTab::Short,
	0L,
	0
    },
    {	SQL_MAX_TABLES_IN_SELECT,
	InfoTab::Short,
	0L,
	0
    },
    {	SQL_MAX_USER_NAME_LEN,
	InfoTab::Short,
	0L,
	0
    },
    {	SQL_MULTIPLE_ACTIVE_TXN,
	InfoTab::YesNo,
	0L,
	"N"
    },
    {	SQL_MULT_RESULT_SETS,
	InfoTab::YesNo,
	0L,
	"N"
    },
    {	SQL_NEED_LONG_DATA_LEN,
	InfoTab::YesNo,
	0L,
	"N"
    },
    {	SQL_NON_NULLABLE_COLUMNS,
	InfoTab::Short,
	SQL_NNC_NON_NULL,
	0
    },
    {	SQL_NULL_COLLATION,
	InfoTab::Short,
	SQL_NC_HIGH,
	0
    },
    {	SQL_NUMERIC_FUNCTIONS,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_ODBC_INTERFACE_CONFORMANCE,
	InfoTab::Long,
	SQL_OIC_CORE,
	0
    },
    {	SQL_ODBC_VER,
	InfoTab::Char,
	0L,
	""
    },
    {	SQL_OJ_CAPABILITIES,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_ORDER_BY_COLUMNS_IN_SELECT,
	InfoTab::YesNo,
	0L,
	"N"
    },
    {	SQL_PARAM_ARRAY_ROW_COUNTS,
	InfoTab::Long,
	0L,
	0
    },
    {	SQL_PARAM_ARRAY_SELECTS,
	InfoTab::Long,
	0L,
	0
    },
    {	SQL_POS_OPERATIONS,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_PROCEDURES,
	InfoTab::YesNo,
	0L,
	"N"
    },
    {	SQL_PROCEDURE_TERM,
	InfoTab::Char,
	0L,
	""
    },
    {	SQL_QUOTED_IDENTIFIER_CASE,
	InfoTab::Short,
	SQL_IC_SENSITIVE,
	0
    },
    {	SQL_ROW_UPDATES,
	InfoTab::YesNo,
	0L,
	"N"
    },
    {	SQL_SCHEMA_TERM,
	InfoTab::Char,
	0L,
	""
    },
    {	SQL_SCHEMA_USAGE,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_SCROLL_OPTIONS,
	InfoTab::Bitmask,
	SQL_SO_FORWARD_ONLY,
	0
    },
    {	SQL_SEARCH_PATTERN_ESCAPE,
	InfoTab::Char,
	0L,
	""
    },
    {	SQL_SERVER_NAME,
	InfoTab::Char,
	0L,
	""
    },
    {	SQL_SPECIAL_CHARACTERS,
	InfoTab::Char,
	0L,
	""
    },
    {	SQL_SQL92_DATETIME_FUNCTIONS,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_SQL92_FOREIGN_KEY_DELETE_RULE,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_SQL92_FOREIGN_KEY_UPDATE_RULE,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_SQL92_GRANT,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_SQL92_NUMERIC_VALUE_FUNCTIONS,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_SQL92_PREDICATES,
	InfoTab::Bitmask,
	SQL_SP_COMPARISON | SQL_SP_IN | SQL_SP_ISNOTNULL | SQL_SP_ISNULL | SQL_SP_LIKE,
	0
    },
    {	SQL_SQL92_RELATIONAL_JOIN_OPERATORS,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_SQL92_REVOKE,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_SQL92_ROW_VALUE_CONSTRUCTOR,
	InfoTab::Bitmask,
	SQL_SRVC_VALUE_EXPRESSION,
	0
    },
    {	SQL_SQL92_STRING_FUNCTIONS,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_SQL92_VALUE_EXPRESSIONS,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_SQL_CONFORMANCE,
	InfoTab::Long,
	0L,
	0
    },
    {	SQL_STANDARD_CLI_CONFORMANCE,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_STATIC_CURSOR_ATTRIBUTES1,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_STATIC_CURSOR_ATTRIBUTES2,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_STRING_FUNCTIONS,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_SUBQUERIES,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_SYSTEM_FUNCTIONS,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_TABLE_TERM,
	InfoTab::Char,
	0L,
	"TABLE"
    },
    {	SQL_TIMEDATE_ADD_INTERVALS,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_TIMEDATE_DIFF_INTERVALS,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_TIMEDATE_FUNCTIONS,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_TXN_CAPABLE,
	InfoTab::Short,
	SQL_TC_DDL_COMMIT,	// XXX do it
	0
    },
    {	SQL_TXN_ISOLATION_OPTION,
	InfoTab::Bitmask,
	SQL_TXN_READ_COMMITTED,
	0
    },
    {	SQL_UNION,
	InfoTab::Bitmask,
	0L,
	0
    },
    {	SQL_USER_NAME,
	InfoTab::Char,
	0L,
	""
    },
    {	SQL_XOPEN_CLI_YEAR,
	InfoTab::Char,
	0L,
	""
    },
    {   0,
	InfoTab::End,
	0L,
	0
    }
};
