/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2.0,
 as published by the Free Software Foundation.

 This program is also distributed with certain software (including
 but not limited to OpenSSL) that is licensed under separate terms,
 as designated in a particular file or component or in included license
 documentation.  The authors of MySQL hereby grant you an additional
 permission to link the program and your derivative works with the
 separately licensed software that they have included with MySQL.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <gtest/gtest.h>

#include "plugin/x/src/mysql_function_names.h"

namespace xpl {
namespace test {

class Mysql_function_names_pass_test
    : public ::testing::TestWithParam<const char *> {};

TEST_P(Mysql_function_names_pass_test, is_mysqld_function) {
  ASSERT_TRUE(is_native_mysql_function(GetParam()));
}

const char *const native_mysql_functions[] = {
    "abs",                               "acos",
    "addtime",                           "aes_decrypt",
    "aes_encrypt",                       "any_value",
    "area",                              "asbinary",
    "asin",                              "astext",
    "aswkb",                             "aswkt",
    "atan",                              "atan2",
    "avg",                               "benchmark",
    "bin",                               "bit_count",
    "bit_length",                        "buffer",
    "ceil",                              "ceiling",
    "centroid",                          "char_length",
    "character_length",                  "coercibility",
    "compress",                          "concat_ws",
    "concat",                            "connection_id",
    "conv",                              "convert_tz",
    "convexhull",                        "cos",
    "cot",                               "crc32",
    "crosses",                           "date_format",
    "datediff",                          "dayname",
    "dayofmonth",                        "dayofweek",
    "dayofyear",                         "decode",
    "degrees",                           "des_decrypt",
    "des_encrypt",                       "dimension",
    "disjoint",                          "distance",
    "elt",                               "encode",
    "encrypt",                           "endpoint",
    "envelope",                          "equals",
    "exp",                               "export_set",
    "exteriorring",                      "extractvalue",
    "field",                             "find_in_set",
    "floor",                             "found_rows",
    "from_base64",                       "from_days",
    "from_unixtime",                     "geomcollfromtext",
    "geomcollfromwkb",                   "geometrycollectionfromtext",
    "geometrycollectionfromwkb",         "geometryfromtext",
    "geometryfromwkb",                   "geometryn",
    "geometrytype",                      "geomfromtext",
    "geomfromwkb",                       "get_lock",
    "glength",                           "greatest",
    "gtid_subset",                       "gtid_subtract",
    "hex",                               "ifnull",
    "inet_aton",                         "inet_ntoa",
    "inet6_aton",                        "inet6_ntoa",
    "instr",                             "interiorringn",
    "intersects",                        "is_free_lock",
    "is_ipv4_compat",                    "is_ipv4_mapped",
    "is_ipv4",                           "is_ipv6",
    "is_used_lock",                      "isclosed",
    "isempty",                           "isnull",
    "issimple",                          "json_array_append",
    "json_array_insert",                 "json_array",
    "json_contains_path",                "json_contains",
    "json_depth",                        "json_extract",
    "json_insert",                       "json_keys",
    "json_length",                       "json_merge",
    "json_object",                       "json_quote",
    "json_remove",                       "json_replace",
    "json_search",                       "json_set",
    "json_type",                         "json_unquote",
    "json_valid",                        "last_day",
    "last_insert_id",                    "lcase",
    "least",                             "length",
    "like_range_max",                    "like_range_min",
    "linefromtext",                      "linefromwkb",
    "linestringfromtext",                "linestringfromwkb",
    "ln",                                "load_file",
    "locate",                            "log",
    "log10",                             "log2",
    "lower",                             "lpad",
    "ltrim",                             "make_set",
    "makedate",                          "maketime",
    "master_pos_wait",                   "mbrcontains",
    "mbrcoveredby",                      "mbrcovers",
    "mbrdisjoint",                       "mbrequal",
    "mbrequals",                         "mbrintersects",
    "mbroverlaps",                       "mbrtouches",
    "mbrwithin",                         "md5",
    "mlinefromtext",                     "mlinefromwkb",
    "monthname",                         "mpointfromtext",
    "mpointfromwkb",                     "mpolyfromtext",
    "mpolyfromwkb",                      "multilinestringfromtext",
    "multilinestringfromwkb",            "multipointfromtext",
    "multipointfromwkb",                 "multipolygonfromtext",
    "multipolygonfromwkb",               "name_const",
    "nullif",                            "numgeometries",
    "numinteriorrings",                  "numpoints",
    "oct",                               "octet_length",
    "ord",                               "overlaps",
    "period_add",                        "period_diff",
    "pi",                                "pointfromtext",
    "pointfromwkb",                      "pointn",
    "polyfromtext",                      "polyfromwkb",
    "polygonfromtext",                   "polygonfromwkb",
    "pow",                               "power",
    "quote",                             "radians",
    "rand",                              "random_bytes",
    "release_all_locks",                 "release_lock",
    "reverse",                           "round",
    "rpad",                              "rtrim",
    "sec_to_time",                       "sha",
    "sha1",                              "sha2",
    "sign",                              "sin",
    "sleep",                             "soundex",
    "space",                             "sqrt",
    "srid",                              "st_area",
    "st_asbinary",                       "st_asgeojson",
    "st_astext",                         "st_aswkb",
    "st_aswkt",                          "st_buffer_strategy",
    "st_buffer",                         "st_centroid",
    "st_contains",                       "st_convexhull",
    "st_crosses",                        "st_difference",
    "st_dimension",                      "st_disjoint",
    "st_distance_sphere",                "st_distance",
    "st_endpoint",                       "st_envelope",
    "st_equals",                         "st_exteriorring",
    "st_geohash",                        "st_geomcollfromtext",
    "st_geomcollfromtxt",                "st_geomcollfromwkb",
    "st_geometrycollectionfromtext",     "st_geometrycollectionfromwkb",
    "st_geometryfromtext",               "st_geometryfromwkb",
    "st_geometryn",                      "st_geometrytype",
    "st_geomfromgeojson",                "st_geomfromtext",
    "st_geomfromwkb",                    "st_interiorringn",
    "st_intersection",                   "st_intersects",
    "st_isclosed",                       "st_isempty",
    "st_issimple",                       "st_isvalid",
    "st_latfromgeohash",                 "st_length",
    "st_linefromtext",                   "st_linefromwkb",
    "st_linestringfromtext",             "st_linestringfromwkb",
    "st_longfromgeohash",                "st_makeenvelope",
    "st_mlinefromtext",                  "st_mlinefromwkb",
    "st_mpointfromtext",                 "st_mpointfromwkb",
    "st_mpolyfromtext",                  "st_mpolyfromwkb",
    "st_multilinestringfromtext",        "st_multilinestringfromwkb",
    "st_multipointfromtext",             "st_multipointfromwkb",
    "st_multipolygonfromtext",           "st_multipolygonfromwkb",
    "st_numgeometries",                  "st_numinteriorring",
    "st_numinteriorrings",               "st_numpoints",
    "st_overlaps",                       "st_pointfromgeohash",
    "st_pointfromtext",                  "st_pointfromwkb",
    "st_pointn",                         "st_polyfromtext",
    "st_polyfromwkb",                    "st_polygonfromtext",
    "st_polygonfromwkb",                 "st_simplify",
    "st_srid",                           "st_startpoint",
    "st_symdifference",                  "st_touches",
    "st_union",                          "st_validate",
    "st_within",                         "st_x",
    "st_y",                              "startpoint",
    "str_to_date",                       "strcmp",
    "substring_index",                   "subtime",
    "tan",                               "time_format",
    "time_to_sec",                       "timediff",
    "to_base64",                         "to_days",
    "to_seconds",                        "touches",
    "ucase",                             "uncompress",
    "uncompressed_length",               "unhex",
    "unix_timestamp",                    "updatexml",
    "upper",                             "uuid_short",
    "uuid",                              "validate_password_strength",
    "version",                           "wait_for_executed_gtid_set",
    "wait_until_sql_thread_after_gtids", "weekday",
    "weekofyear",                        "within",
    "x",                                 "y",
    "yearweek"};

const char *const special_mysql_functions[] = {
    "adddate",    "bit_and",     "bit_or",       "bit_xor",      "cast",
    "count",      "curdate",     "curtime",      "date_add",     "date_sub",
    "distinct",   "extract",     "group_concat", "max",          "mid",
    "min",        "now",         "position",     "session_user", "std",
    "stddev_pop", "stddev_samp", "stddev",       "subdate",      "substr",
    "substring",  "sum",         "sysdate",      "system_user",  "trim",
    "var_pop",    "var_samp",    "variance"};

const char *const other_mysql_functions[] = {
    "ascii",             "binary",         "char",
    "charset",           "coalesce",       "collation",
    "contains",          "curdate",        "current_user",
    "curtime",           "database",       "date_add_interval",
    "date_sub_interval", "date",           "day",
    "extract",           "format",         "geometrycollection",
    "hour",              "if",             "in",
    "insert",            "interval",       "left",
    "linestring",        "microsecond",    "minute",
    "mod",               "month",          "multilinestring",
    "multipoint",        "multipolygon",   "password",
    "point",             "polygon",        "position",
    "quarter",           "repeat",         "replace",
    "reverse",           "right",          "row_count",
    "second",            "strongly",       "subdate",
    "substring",         "sysdate",        "time",
    "timestamp_add",     "timestamp_diff", "timestamp",
    "trim_leading",      "trim",           "truncate",
    "user",              "using",          "utc_date",
    "utc_time",          "utc_timestamp",  "week",
    "weight_string",     "year"};

INSTANTIATE_TEST_CASE_P(Native_mysql_functions, Mysql_function_names_pass_test,
                        ::testing::ValuesIn(native_mysql_functions));

INSTANTIATE_TEST_CASE_P(Special_mysql_functions, Mysql_function_names_pass_test,
                        ::testing::ValuesIn(special_mysql_functions));

INSTANTIATE_TEST_CASE_P(Other_mysql_functions, Mysql_function_names_pass_test,
                        ::testing::ValuesIn(other_mysql_functions));

class Mysql_function_names_fail_test
    : public ::testing::TestWithParam<const char *> {};

TEST_P(Mysql_function_names_fail_test, is_not_mysqld_function) {
  ASSERT_FALSE(is_native_mysql_function(GetParam()));
}

INSTANTIATE_TEST_CASE_P(Mysql_function_names_fail,
                        Mysql_function_names_fail_test,
                        ::testing::Values("meeny", "miny", "moe"));

}  // namespace test
}  // namespace xpl
