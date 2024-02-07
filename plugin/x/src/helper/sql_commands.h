/*
 * Copyright (c) 2019, 2024, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_SRC_HELPER_SQL_COMMANDS_H_
#define PLUGIN_X_SRC_HELPER_SQL_COMMANDS_H_

namespace xpl {

#define DOC_MEMBER_REGEX                                                     \
  R"(\\$((\\*{2})?(\\[([[:digit:]]+|\\*)\\]|\\.([[:alpha:]_\\$][[:alnum:]_)" \
  R"(\\$]*|\\*|\\".*\\"|`.*`)))*)"
#define DOC_MEMBER_REGEX_NO_BACKSLASH_ESCAPES                         \
  R"(\$((\*{2})?(\[([[:digit:]]+|\*)\]|\.([[:alpha:]_\$][[:alnum:]_)" \
  R"(\$]*|\*|\".*\"|`.*`)))*)"

#define DOC_ID_REGEX R"(\\$\\._id)"
#define DOC_ID_REGEX_NO_BACKSLASH_ESCAPES R"(\$\._id)"

#define JSON_EXTRACT_REGEX(member) \
  R"(json_extract\\(`doc`,(_[[:alnum:]]+)?\\\\'')" member R"(\\\\''\\))"
#define JSON_EXTRACT_REGEX_NO_BACKSLASH_ESCAPES(member) \
  R"(json_extract\(`doc`,(_[[:alnum:]]+)?\\'')" member R"(\\''\))"

#define JSON_EXTRACT_UNQUOTE_REGEX(member) \
  R"(^json_unquote\\()" JSON_EXTRACT_REGEX(member) R"(\\)$)"

#define JSON_EXTRACT_UNQUOTE_REGEX_NO_BACKSLASH_ESCAPES(member) \
  R"(^json_unquote\()" JSON_EXTRACT_REGEX_NO_BACKSLASH_ESCAPES(member) R"(\)$)"

#define COUNT_WHEN(expresion) \
  "COUNT(CASE WHEN (" expresion ") THEN 1 ELSE NULL END)"

const char *const k_count_doc =
    COUNT_WHEN("column_name = 'doc' AND data_type = 'json'");
const char *const k_count_id = COUNT_WHEN(
    "column_name = '_id' AND generation_expression RLIKE "
    "'" JSON_EXTRACT_UNQUOTE_REGEX(DOC_ID_REGEX) "'");
const char *const k_count_gen = COUNT_WHEN(
    "column_name != '_id' AND column_name != 'doc' AND column_name != "
    "'_json_schema' AND "
    "generation_expression RLIKE '" JSON_EXTRACT_REGEX(DOC_MEMBER_REGEX) "'");

const char *const k_count_id_no_backslash_escapes = COUNT_WHEN(
    "column_name = '_id' AND generation_expression RLIKE "
    "'" JSON_EXTRACT_UNQUOTE_REGEX_NO_BACKSLASH_ESCAPES(
        DOC_ID_REGEX_NO_BACKSLASH_ESCAPES) "'");
const char *const k_count_gen_no_backslash_escapes = COUNT_WHEN(
    "column_name != '_id' AND column_name != 'doc' AND column_name != "
    "'_json_schema' AND "
    "generation_expression RLIKE '" JSON_EXTRACT_REGEX_NO_BACKSLASH_ESCAPES(
        DOC_MEMBER_REGEX_NO_BACKSLASH_ESCAPES) "'");

const char *const k_count_schema =
    COUNT_WHEN(R"(column_name = '_json_schema')");
const char *const k_count_without_schema =
    COUNT_WHEN(R"(column_name != '_json_schema')");

}  // namespace xpl

#endif  // PLUGIN_X_SRC_HELPER_SQL_COMMANDS_H_
