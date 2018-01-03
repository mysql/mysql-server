/*
 * Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
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

namespace xpl {

namespace {
// list of built-in function names for MySQL
// taken from item_create.cc
// keep in ASC order
const char *const native_mysql_functions[] = {
    "ABS",                               "ACOS",
    "ADDTIME",                           "AES_DECRYPT",
    "AES_ENCRYPT",                       "ANY_VALUE",
    "AREA",                              "ASBINARY",
    "ASIN",                              "ASTEXT",
    "ASWKB",                             "ASWKT",
    "ATAN",                              "ATAN2",
    "AVG",                               "BENCHMARK",
    "BIN",                               "BIT_COUNT",
    "BIT_LENGTH",                        "BUFFER",
    "CEIL",                              "CEILING",
    "CENTROID",                          "CHARACTER_LENGTH",
    "CHAR_LENGTH",                       "COERCIBILITY",
    "COMPRESS",                          "CONCAT",
    "CONCAT_WS",                         "CONNECTION_ID",
    "CONV",                              "CONVERT_TZ",
    "CONVEXHULL",                        "COS",
    "COT",                               "CRC32",
    "CROSSES",                           "DATEDIFF",
    "DATE_FORMAT",                       "DAYNAME",
    "DAYOFMONTH",                        "DAYOFWEEK",
    "DAYOFYEAR",                         "DECODE",
    "DEGREES",                           "DES_DECRYPT",
    "DES_ENCRYPT",                       "DIMENSION",
    "DISJOINT",                          "DISTANCE",
    "ELT",                               "ENCODE",
    "ENCRYPT",                           "ENDPOINT",
    "ENVELOPE",                          "EQUALS",
    "EXP",                               "EXPORT_SET",
    "EXTERIORRING",                      "EXTRACTVALUE",
    "FIELD",                             "FIND_IN_SET",
    "FLOOR",                             "FOUND_ROWS",
    "FROM_BASE64",                       "FROM_DAYS",
    "FROM_UNIXTIME",                     "GEOMCOLLFROMTEXT",
    "GEOMCOLLFROMWKB",                   "GEOMETRYCOLLECTIONFROMTEXT",
    "GEOMETRYCOLLECTIONFROMWKB",         "GEOMETRYFROMTEXT",
    "GEOMETRYFROMWKB",                   "GEOMETRYN",
    "GEOMETRYTYPE",                      "GEOMFROMTEXT",
    "GEOMFROMWKB",                       "GET_LOCK",
    "GLENGTH",                           "GREATEST",
    "GTID_SUBSET",                       "GTID_SUBTRACT",
    "HEX",                               "IFNULL",
    "INET6_ATON",                        "INET6_NTOA",
    "INET_ATON",                         "INET_NTOA",
    "INSTR",                             "INTERIORRINGN",
    "INTERSECTS",                        "ISCLOSED",
    "ISEMPTY",                           "ISNULL",
    "ISSIMPLE",                          "IS_FREE_LOCK",
    "IS_IPV4",                           "IS_IPV4_COMPAT",
    "IS_IPV4_MAPPED",                    "IS_IPV6",
    "IS_USED_LOCK",                      "LAST_DAY",
    "LAST_INSERT_ID",                    "LCASE",
    "LEAST",                             "LENGTH",
    "LIKE_RANGE_MAX",                    "LIKE_RANGE_MIN",
    "LINEFROMTEXT",                      "LINEFROMWKB",
    "LINESTRINGFROMTEXT",                "LINESTRINGFROMWKB",
    "LN",                                "LOAD_FILE",
    "LOCATE",                            "LOG",
    "LOG10",                             "LOG2",
    "LOWER",                             "LPAD",
    "LTRIM",                             "MAKEDATE",
    "MAKETIME",                          "MAKE_SET",
    "MASTER_POS_WAIT",                   "MBRCONTAINS",
    "MBRCOVEREDBY",                      "MBRCOVERS",
    "MBRDISJOINT",                       "MBREQUAL",
    "MBREQUALS",                         "MBRINTERSECTS",
    "MBROVERLAPS",                       "MBRTOUCHES",
    "MBRWITHIN",                         "MD5",
    "MLINEFROMTEXT",                     "MLINEFROMWKB",
    "MONTHNAME",                         "MPOINTFROMTEXT",
    "MPOINTFROMWKB",                     "MPOLYFROMTEXT",
    "MPOLYFROMWKB",                      "MULTILINESTRINGFROMTEXT",
    "MULTILINESTRINGFROMWKB",            "MULTIPOINTFROMTEXT",
    "MULTIPOINTFROMWKB",                 "MULTIPOLYGONFROMTEXT",
    "MULTIPOLYGONFROMWKB",               "NAME_CONST",
    "NULLIF",                            "NUMGEOMETRIES",
    "NUMINTERIORRINGS",                  "NUMPOINTS",
    "OCT",                               "OCTET_LENGTH",
    "ORD",                               "OVERLAPS",
    "PERIOD_ADD",                        "PERIOD_DIFF",
    "PI",                                "POINTFROMTEXT",
    "POINTFROMWKB",                      "POINTN",
    "POLYFROMTEXT",                      "POLYFROMWKB",
    "POLYGONFROMTEXT",                   "POLYGONFROMWKB",
    "POW",                               "POWER",
    "QUOTE",                             "RADIANS",
    "RAND",                              "RANDOM_BYTES",
    "RELEASE_ALL_LOCKS",                 "RELEASE_LOCK",
    "REVERSE",                           "ROUND",
    "RPAD",                              "RTRIM",
    "SEC_TO_TIME",                       "SHA",
    "SHA1",                              "SHA2",
    "SIGN",                              "SIN",
    "SLEEP",                             "SOUNDEX",
    "SPACE",                             "SQRT",
    "SRID",                              "STARTPOINT",
    "STRCMP",                            "STR_TO_DATE",
    "ST_AREA",                           "ST_ASBINARY",
    "ST_ASGEOJSON",                      "ST_ASTEXT",
    "ST_ASWKB",                          "ST_ASWKT",
    "ST_BUFFER",                         "ST_BUFFER_STRATEGY",
    "ST_CENTROID",                       "ST_CONTAINS",
    "ST_CONVEXHULL",                     "ST_CROSSES",
    "ST_DIFFERENCE",                     "ST_DIMENSION",
    "ST_DISJOINT",                       "ST_DISTANCE",
    "ST_DISTANCE_SPHERE",                "ST_ENDPOINT",
    "ST_ENVELOPE",                       "ST_EQUALS",
    "ST_EXTERIORRING",                   "ST_GEOHASH",
    "ST_GEOMCOLLFROMTEXT",               "ST_GEOMCOLLFROMTXT",
    "ST_GEOMCOLLFROMWKB",                "ST_GEOMETRYCOLLECTIONFROMTEXT",
    "ST_GEOMETRYCOLLECTIONFROMWKB",      "ST_GEOMETRYFROMTEXT",
    "ST_GEOMETRYFROMWKB",                "ST_GEOMETRYN",
    "ST_GEOMETRYTYPE",                   "ST_GEOMFROMGEOJSON",
    "ST_GEOMFROMTEXT",                   "ST_GEOMFROMWKB",
    "ST_INTERIORRINGN",                  "ST_INTERSECTION",
    "ST_INTERSECTS",                     "ST_ISCLOSED",
    "ST_ISEMPTY",                        "ST_ISSIMPLE",
    "ST_ISVALID",                        "ST_LATFROMGEOHASH",
    "ST_LENGTH",                         "ST_LINEFROMTEXT",
    "ST_LINEFROMWKB",                    "ST_LINESTRINGFROMTEXT",
    "ST_LINESTRINGFROMWKB",              "ST_LONGFROMGEOHASH",
    "ST_MAKEENVELOPE",                   "ST_MLINEFROMTEXT",
    "ST_MLINEFROMWKB",                   "ST_MPOINTFROMTEXT",
    "ST_MPOINTFROMWKB",                  "ST_MPOLYFROMTEXT",
    "ST_MPOLYFROMWKB",                   "ST_MULTILINESTRINGFROMTEXT",
    "ST_MULTILINESTRINGFROMWKB",         "ST_MULTIPOINTFROMTEXT",
    "ST_MULTIPOINTFROMWKB",              "ST_MULTIPOLYGONFROMTEXT",
    "ST_MULTIPOLYGONFROMWKB",            "ST_NUMGEOMETRIES",
    "ST_NUMINTERIORRING",                "ST_NUMINTERIORRINGS",
    "ST_NUMPOINTS",                      "ST_OVERLAPS",
    "ST_POINTFROMGEOHASH",               "ST_POINTFROMTEXT",
    "ST_POINTFROMWKB",                   "ST_POINTN",
    "ST_POLYFROMTEXT",                   "ST_POLYFROMWKB",
    "ST_POLYGONFROMTEXT",                "ST_POLYGONFROMWKB",
    "ST_SIMPLIFY",                       "ST_SRID",
    "ST_STARTPOINT",                     "ST_SYMDIFFERENCE",
    "ST_TOUCHES",                        "ST_UNION",
    "ST_VALIDATE",                       "ST_WITHIN",
    "ST_X",                              "ST_Y",
    "SUBSTRING_INDEX",                   "SUBTIME",
    "TAN",                               "TIMEDIFF",
    "TIME_FORMAT",                       "TIME_TO_SEC",
    "TOUCHES",                           "TO_BASE64",
    "TO_DAYS",                           "TO_SECONDS",
    "UCASE",                             "UNCOMPRESS",
    "UNCOMPRESSED_LENGTH",               "UNHEX",
    "UNIX_TIMESTAMP",                    "UPDATEXML",
    "UPPER",                             "UUID",
    "UUID_SHORT",                        "VALIDATE_PASSWORD_STRENGTH",
    "VERSION",                           "WAIT_FOR_EXECUTED_GTID_SET",
    "WAIT_UNTIL_SQL_THREAD_AFTER_GTIDS", "WEEKDAY",
    "WEEKOFYEAR",                        "WITHIN",
    "X",                                 "Y",
    "YEARWEEK"};


static const char *const mysql_functions_that_operate_on_json[] = {
    "JSON_CONTAINS",
    "JSON_CONTAINS_PATH",
    "JSON_DEPTH",
    "JSON_LENGTH",
    "JSON_TYPE",
    "JSON_UNQUOTE",
    "JSON_VALID"
};


const char *const mysql_functions_that_return_json[] = {
    "JSON_ARRAY",
    "JSON_ARRAY_APPEND",
    "JSON_ARRAY_INSERT",
    "JSON_EXTRACT",
    "JSON_INSERT",
    "JSON_KEYS",
    "JSON_MERGE",
    "JSON_MERGE_PATCH",
    "JSON_MERGE_PRESERVE",
    "JSON_OBJECT",
    "JSON_QUOTE",
    "JSON_REMOVE",
    "JSON_REPLACE",
    "JSON_SEARCH",
    "JSON_SET"
};

// taken from lex.h (SYM_FN)
// keep in ASC order
const char *const special_mysql_functions[] = {
    "ADDDATE",   "BIT_AND",    "BIT_OR",       "BIT_XOR",      "CAST",
    "COUNT",     "CURDATE",    "CURTIME",      "DATE_ADD",     "DATE_SUB",
    "DISTINCT",  "EXTRACT",    "GROUP_CONCAT", "MAX",          "MID",
    "MIN",       "NOW",        "POSITION",     "SESSION_USER", "STD",
    "STDDEV",    "STDDEV_POP", "STDDEV_SAMP",  "SUBDATE",      "SUBSTR",
    "SUBSTRING", "SUM",        "SYSDATE",      "SYSTEM_USER",  "TRIM",
    "VARIANCE",  "VAR_POP",    "VAR_SAMP"};

// taken from sql_yacc.yy
// keep in ASC order
const char *const other_mysql_functions[] = {
    "ASCII",             "BINARY",             "CHAR",           "CHARSET",
    "COALESCE",          "COLLATION",          "CONTAINS",       "CURDATE",
    "CURRENT_USER",      "CURTIME",            "DATABASE",       "DATE",
    "DATE_ADD_INTERVAL", "DATE_SUB_INTERVAL",  "DAY",            "EXTRACT",
    "FORMAT",            "GEOMETRYCOLLECTION", "HOUR",           "IF",
    "IN",                "INSERT",             "INTERVAL",       "LEFT",
    "LINESTRING",        "MICROSECOND",        "MINUTE",         "MOD",
    "MONTH",             "MULTILINESTRING",    "MULTIPOINT",     "MULTIPOLYGON",
    "PASSWORD",          "POINT",              "POLYGON",        "POSITION",
    "QUARTER",           "REPEAT",             "REPLACE",        "REVERSE",
    "RIGHT",             "ROW_COUNT",          "SECOND",         "STRONGLY",
    "SUBDATE",           "SUBSTRING",          "SYSDATE",        "TIME",
    "TIMESTAMP",         "TIMESTAMP_ADD",      "TIMESTAMP_DIFF", "TRIM",
    "TRIM_LEADING",      "TRUNCATE",           "USER",           "USING",
    "UTC_DATE",          "UTC_TIME",           "UTC_TIMESTAMP",  "WEEK",
    "WEIGHT_STRING",     "YEAR"};

template <typename Container, typename Value>
bool contains(const Container &container, const Value &value) {
  return std::binary_search(
            std::begin(container),
            std::end(container),
            value.c_str(),
            Is_less());
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

}  // namespace xpl
