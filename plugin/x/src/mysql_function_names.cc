/*
 * Copyright (c) 2015, 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#include "plugin/x/src/mysql_function_names.h"

#include <algorithm>
#include <iterator>
#include <vector>

#include "plugin/x/src/helper/string_case.h"

namespace xpl {

namespace {

// list of built-in function names for MySQL
// taken from item_create.cc
// keep in ASC order
const char *const native_mysql_functions[] = {
    "ABS",
    "ACOS",
    "ADDTIME",
    "AES_DECRYPT",
    "AES_ENCRYPT",
    "ANY_VALUE",
    "ASIN",
    "ATAN",
    "ATAN2",
    "BENCHMARK",
    "BIN",
    "BIN_TO_UUID",
    "BIT_COUNT",
    "BIT_LENGTH",
    "CEIL",
    "CEILING",
    "CHARACTER_LENGTH",
    "CHAR_LENGTH",
    "COERCIBILITY",
    "COMPRESS",
    "CONCAT",
    "CONCAT_WS",
    "CONNECTION_ID",
    "CONV",
    "CONVERT_TZ",
    "COS",
    "COT",
    "CRC32",
    "CURRENT_ROLE",
    "DATEDIFF",
    "DATE_FORMAT",
    "DAYNAME",
    "DAYOFMONTH",
    "DAYOFWEEK",
    "DAYOFYEAR",
    "DEGREES",
    "ELT",
    "EXP",
    "EXPORT_SET",
    "EXTRACTVALUE",
    "FIELD",
    "FIND_IN_SET",
    "FLOOR",
    "FORMAT_BYTES",
    "FORMAT_PICO_TIME",
    "FOUND_ROWS",
    "FROM_BASE64",
    "FROM_DAYS",
    "FROM_UNIXTIME",
    "GET_LOCK",
    "GREATEST",
    "GTID_SUBSET",
    "GTID_SUBTRACT",
    "HEX",
    "ICU_VERSION",
    "IFNULL",
    "INET6_ATON",
    "INET6_NTOA",
    "INET_ATON",
    "INET_NTOA",
    "INSTR",
    "ISNULL",
    "IS_FREE_LOCK",
    "IS_IPV4",
    "IS_IPV4_COMPAT",
    "IS_IPV4_MAPPED",
    "IS_IPV6",
    "IS_USED_LOCK",
    "IS_UUID",
    "LAST_DAY",
    "LAST_INSERT_ID",
    "LCASE",
    "LEAST",
    "LENGTH",
    "LIKE_RANGE_MAX",
    "LIKE_RANGE_MIN",
    "LN",
    "LOAD_FILE",
    "LOCATE",
    "LOG",
    "LOG10",
    "LOG2",
    "LOWER",
    "LPAD",
    "LTRIM",
    "MAKEDATE",
    "MAKETIME",
    "MAKE_SET",
    "SOURCE_POS_WAIT",
    "MBRCONTAINS",
    "MBRCOVEREDBY",
    "MBRCOVERS",
    "MBRDISJOINT",
    "MBREQUALS",
    "MBRINTERSECTS",
    "MBROVERLAPS",
    "MBRTOUCHES",
    "MBRWITHIN",
    "MD5",
    "MONTHNAME",
    "NAME_CONST",
    "NULLIF",
    "OCT",
    "OCTET_LENGTH",
    "ORD",
    "PERIOD_ADD",
    "PERIOD_DIFF",
    "PI",
    "POW",
    "POWER",
    "PS_CURRENT_THREAD_ID",
    "PS_THREAD_ID",
    "QUOTE",
    "RADIANS",
    "RAND",
    "RANDOM_BYTES",
    "REGEXP_INSTR",
    "REGEXP_LIKE",
    "REGEXP_REPLACE",
    "REGEXP_SUBSTR",
    "RELEASE_ALL_LOCKS",
    "RELEASE_LOCK",
    "REVERSE",
    "ROLES_GRAPHML",
    "ROUND",
    "RPAD",
    "RTRIM",
    "SEC_TO_TIME",
    "SHA",
    "SHA1",
    "SHA2",
    "SIGN",
    "SIN",
    "SLEEP",
    "SOUNDEX",
    "SPACE",
    "SQRT",
    "STATEMENT_DIGEST",
    "STATEMENT_DIGEST_TEXT",
    "STRCMP",
    "STR_TO_DATE",
    "ST_AREA",
    "ST_ASBINARY",
    "ST_ASGEOJSON",
    "ST_ASTEXT",
    "ST_ASWKB",
    "ST_ASWKT",
    "ST_BUFFER",
    "ST_BUFFER_STRATEGY",
    "ST_CENTROID",
    "ST_CONTAINS",
    "ST_CONVEXHULL",
    "ST_CROSSES",
    "ST_DIFFERENCE",
    "ST_DIMENSION",
    "ST_DISJOINT",
    "ST_DISTANCE",
    "ST_DISTANCE_SPHERE",
    "ST_ENDPOINT",
    "ST_ENVELOPE",
    "ST_EQUALS",
    "ST_EXTERIORRING",
    "ST_FRECHETDISTANCE",
    "ST_GEOHASH",
    "ST_GEOMCOLLFROMTEXT",
    "ST_GEOMCOLLFROMTXT",
    "ST_GEOMCOLLFROMWKB",
    "ST_GEOMETRYCOLLECTIONFROMTEXT",
    "ST_GEOMETRYCOLLECTIONFROMWKB",
    "ST_GEOMETRYFROMTEXT",
    "ST_GEOMETRYFROMWKB",
    "ST_GEOMETRYN",
    "ST_GEOMETRYTYPE",
    "ST_GEOMFROMGEOJSON",
    "ST_GEOMFROMTEXT",
    "ST_GEOMFROMWKB",
    "ST_HAUSDORFFDISTANCE",
    "ST_INTERIORRINGN",
    "ST_INTERSECTION",
    "ST_INTERSECTS",
    "ST_ISCLOSED",
    "ST_ISEMPTY",
    "ST_ISSIMPLE",
    "ST_ISVALID",
    "ST_LATFROMGEOHASH",
    "ST_LATITUDE",
    "ST_LENGTH",
    "ST_LINEFROMTEXT",
    "ST_LINEFROMWKB",
    "ST_LINESTRINGFROMTEXT",
    "ST_LINESTRINGFROMWKB",
    "ST_LONGFROMGEOHASH",
    "ST_LONGITUDE",
    "ST_MAKEENVELOPE",
    "ST_MLINEFROMTEXT",
    "ST_MLINEFROMWKB",
    "ST_MPOINTFROMTEXT",
    "ST_MPOINTFROMWKB",
    "ST_MPOLYFROMTEXT",
    "ST_MPOLYFROMWKB",
    "ST_MULTILINESTRINGFROMTEXT",
    "ST_MULTILINESTRINGFROMWKB",
    "ST_MULTIPOINTFROMTEXT",
    "ST_MULTIPOINTFROMWKB",
    "ST_MULTIPOLYGONFROMTEXT",
    "ST_MULTIPOLYGONFROMWKB",
    "ST_NUMGEOMETRIES",
    "ST_NUMINTERIORRING",
    "ST_NUMINTERIORRINGS",
    "ST_NUMPOINTS",
    "ST_OVERLAPS",
    "ST_POINTFROMGEOHASH",
    "ST_POINTFROMTEXT",
    "ST_POINTFROMWKB",
    "ST_POINTN",
    "ST_POLYFROMTEXT",
    "ST_POLYFROMWKB",
    "ST_POLYGONFROMTEXT",
    "ST_POLYGONFROMWKB",
    "ST_SIMPLIFY",
    "ST_SRID",
    "ST_STARTPOINT",
    "ST_SWAPXY",
    "ST_SYMDIFFERENCE",
    "ST_TOUCHES",
    "ST_TRANSFORM",
    "ST_UNION",
    "ST_VALIDATE",
    "ST_WITHIN",
    "ST_X",
    "ST_Y",
    "SUBSTRING_INDEX",
    "SUBTIME",
    "TAN",
    "TIMEDIFF",
    "TIME_FORMAT",
    "TIME_TO_SEC",
    "TO_BASE64",
    "TO_DAYS",
    "TO_SECONDS",
    "UCASE",
    "UNCOMPRESS",
    "UNCOMPRESSED_LENGTH",
    "UNHEX",
    "UNIX_TIMESTAMP",
    "UPDATEXML",
    "UPPER",
    "UUID",
    "UUID_SHORT",
    "UUID_TO_BIN",
    "VALIDATE_PASSWORD_STRENGTH",
    "VERSION",
    "WAIT_FOR_EXECUTED_GTID_SET",
    "WEEKDAY",
    "WEEKOFYEAR",
    "YEARWEEK",
};

// keep in ASC order
const char *const mysql_functions_that_operate_on_json[] = {
    "JSON_CONTAINS",     "JSON_CONTAINS_PATH",
    "JSON_DEPTH",        "JSON_LENGTH",
    "JSON_OVERLAPS",     "JSON_PRETTY",
    "JSON_SCHEMA_VALID", "JSON_SCHEMA_VALIDATION_REPORT",
    "JSON_STORAGE_FREE", "JSON_STORAGE_SIZE",
    "JSON_TYPE",         "JSON_UNQUOTE",
    "JSON_VALID",        "JSON_VALUE",
};

// keep in ASC order
const char *const mysql_functions_that_return_json[] = {
    "JSON_ARRAY",        "JSON_ARRAYAGG",
    "JSON_ARRAY_APPEND", "JSON_ARRAY_INSERT",
    "JSON_EXTRACT",      "JSON_INSERT",
    "JSON_KEYS",         "JSON_MERGE",
    "JSON_MERGE_PATCH",  "JSON_MERGE_PRESERVE",
    "JSON_OBJECT",       "JSON_OBJECTAGG",
    "JSON_QUOTE",        "JSON_REMOVE",
    "JSON_REPLACE",      "JSON_SCHEMA_VALIDATION_REPORT",
    "JSON_SEARCH",       "JSON_SET",
};

// taken from lex.h (SYM_FN)
// keep in ASC order
const char *const special_mysql_functions[] = {
    "ADDDATE",    "BIT_AND",      "BIT_OR",       "BIT_XOR",  "CAST",
    "COUNT",      "CURDATE",      "CURTIME",      "DATE_ADD", "DATE_SUB",
    "EXTRACT",    "GROUP_CONCAT", "MAX",          "MID",      "MIN",
    "NOW",        "POSITION",     "SESSION_USER", "STD",      "STDDEV",
    "STDDEV_POP", "STDDEV_SAMP",  "SUBDATE",      "SUBSTR",   "SUBSTRING",
    "SUM",        "SYSDATE",      "SYSTEM_USER",  "TRIM",     "VARIANCE",
    "VAR_POP",    "VAR_SAMP",
};

// taken from sql_yacc.yy
// keep in ASC order
const char *const other_mysql_functions[] = {
    "ASCII",
    "AVG",
    "CHAR",
    "CHARSET",
    "COALESCE",
    "COLLATION",
    "CONVERT",
    "CUME_DIST",
    "CURDATE",
    "CURRENT_DATE",
    "CURRENT_TIME",
    "CURRENT_TIMESTAMP",
    "CURRENT_USER",
    "CURTIME",
    "DATABASE",
    "DATE",
    "DATE_ADD_INTERVAL",
    "DATE_SUB_INTERVAL",
    "DAY",
    "DEFAULT",
    "DENSE_RANK",
    "DISTINCT",
    "EXTRACT",
    "FIRST_VALUE",
    "FORMAT",
    "GEOMCOLLECTION",
    "GEOMETRYCOLLECTION",
    "GET_FORMAT",
    "GROUPING",
    "HOUR",
    "IF",
    "INSERT",
    "INTERVAL",
    "LAG",
    "LAST_VALUE",
    "LEAD",
    "LEFT",
    "LINESTRING",
    "LOCALTIME",
    "LOCALTIMESTAMP",
    "MICROSECOND",
    "MINUTE",
    "MOD",
    "MONTH",
    "MULTILINESTRING",
    "MULTIPOINT",
    "MULTIPOLYGON",
    "NTH_VALUE",
    "PASSWORD",
    "PERCENT_RANK",
    "POINT",
    "POLYGON",
    "POSITION",
    "QUARTER",
    "RANK",
    "REPEAT",
    "REPLACE",
    "REVERSE",
    "RIGHT",
    "ROW_COUNT",
    "ROW_NUMBER",
    "SCHEMA",
    "SECOND",
    "SUBDATE",
    "SUBSTRING",
    "SYSDATE",
    "TIME",
    "TIMESTAMP",
    "TIMESTAMPADD",
    "TIMESTAMPDIFF",
    "TRIM",
    "TRUNCATE",
    "USER",
    "UTC_DATE",
    "UTC_TIME",
    "UTC_TIMESTAMP",
    "VALUES",
    "WEEK",
    "WEIGHT_STRING",
    "YEAR",
};

template <typename Container, typename Value>
bool contains(const Container &container, const Value &value) {
  return std::binary_search(std::begin(container), std::end(container),
                            value.c_str(), Is_less());
}

template <typename Container>
void copy(const Container &container, std::vector<const char *> *result) {
  std::copy(std::begin(container), std::end(container),
            std::back_inserter(*result));
}

}  // namespace

bool does_return_json_mysql_function(const std::string &name) {
  const std::string source = to_upper(name);
  return contains(mysql_functions_that_return_json, source);
}

bool is_native_mysql_json_function(const std::string &name) {
  const std::string source = to_upper(name);
  return contains(mysql_functions_that_return_json, source) ||
         contains(mysql_functions_that_operate_on_json, source);
}

bool is_native_mysql_function(const std::string &name) {
  const std::string source = to_upper(name);
  return contains(mysql_functions_that_return_json, source) ||
         contains(mysql_functions_that_operate_on_json, source) ||
         contains(native_mysql_functions, source) ||
         contains(special_mysql_functions, source) ||
         contains(other_mysql_functions, source);
}

namespace mysql_function_names_test {

std::vector<const char *> get_native_mysql_functions() {
  std::vector<const char *> result;
  copy(native_mysql_functions, &result);
  return result;
}

std::vector<const char *> get_special_mysql_functions() {
  std::vector<const char *> result;
  copy(special_mysql_functions, &result);
  return result;
}
std::vector<const char *> get_other_mysql_functions() {
  std::vector<const char *> result;
  copy(other_mysql_functions, &result);
  return result;
}

std::vector<const char *> get_mysql_functions_that_return_json() {
  std::vector<const char *> result;
  copy(mysql_functions_that_return_json, &result);
  return result;
}

std::vector<const char *> get_mysql_functions_that_operate_on_json() {
  std::vector<const char *> result;
  copy(mysql_functions_that_operate_on_json, &result);
  return result;
}

std::vector<const char *> get_all_mysql_function_names() {
  std::vector<const char *> result;
  copy(mysql_functions_that_return_json, &result);
  copy(mysql_functions_that_operate_on_json, &result);
  copy(native_mysql_functions, &result);
  copy(special_mysql_functions, &result);
  copy(other_mysql_functions, &result);
  return result;
}
}  // namespace mysql_function_names_test
}  // namespace xpl
