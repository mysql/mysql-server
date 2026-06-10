/* Copyright (c) 2025, 2026, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef EXTERNAL_TABLE_CONST_INCLUDED
#define EXTERNAL_TABLE_CONST_INCLUDED

/**
   @file
   Constants used in engine attributes for external tables
*/

#include <string_view>

namespace external_table {
using std::string_view_literals::operator""sv;

/* ENGINE_ATTRIBUTE JSON keys*/
constexpr auto kFileParam = "file"sv;
constexpr auto kDialectParam = "dialect"sv;

/* ENGINE_ATTRIBUTE file JSON keys*/
constexpr auto kRegionParam = "region"sv;
constexpr auto kNamespaceParam = "namespace"sv;
constexpr auto kBucketParam = "bucket"sv;
constexpr auto kParParam = "par"sv;
constexpr auto kNameParam = "name"sv;
constexpr auto kPrefixParam = "prefix"sv;
constexpr auto kPatternParam = "pattern"sv;

/* ENGINE_ATTRIBUTE dialect JSON keys*/
constexpr auto kFormatParam = "format"sv;
constexpr auto kQuotationMarksParam = "quotation_marks"sv;
constexpr auto kEscapeCharacterParam = "escape_character"sv;
constexpr auto kSkipRowsParam = "skip_rows"sv;
constexpr auto kRecordDelimiterParam = "record_delimiter"sv;
constexpr auto kFieldDelimiterParam = "field_delimiter"sv;
constexpr auto kEncodingParam = "encoding"sv;
constexpr auto kDateFormatParam = "date_format"sv;
constexpr auto kTimeFormatParam = "time_format"sv;
constexpr auto kTimestampFormatParam = "timestamp_format"sv;
constexpr auto kDatetimeFormatParam = "datetime_format"sv;
constexpr auto kIsStrictModeParam = "is_strict_mode"sv;
constexpr auto kConstraintCheckParam = "check_constraints"sv;
constexpr auto kHasHeaderParam = "has_header"sv;
constexpr auto kAllowMissingParam = "allow_missing_files"sv;
constexpr auto kNullValueParam = "null_value"sv;
constexpr auto kEmptyValueParam = "empty_value"sv;
constexpr auto kCompressionParam = "compression"sv;

/* ENGINE_ATTRIBUTE location parameters*/
constexpr auto kUriParam = "uri"sv;

/*OCI par info*/
constexpr auto kParPattern = "https://.*/p/.*/n/.*/b/";
}  // namespace external_table

#endif /* EXTERNAL_TABLE_CONST_INCLUDED */
