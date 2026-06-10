// Copyright (c) 2024, 2026, Oracle and/or its affiliates.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is designed to work with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have either included with
// the program or referenced in the documentation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

#ifndef MYSQL_STRCONV_STRCONV_H
#define MYSQL_STRCONV_STRCONV_H

/// @file
/// Experimental API header
///
/// This is a high-level header, intended for users to include in order to
/// import all the decode functionality.

// ==== Formats ====

// Files are listed in order from more primitive to more complex; files closer
// to the top can't include files closer to the bottom.

// Base class for format type tags.
#include "mysql/strconv/formats/format.h"

// Internal logic to determine the format to use when invoking
// `decode_impl`
#include "mysql/strconv/formats/resolve_format.h"

// ==== Pre-defined formats ====

// Integers in ascii, strings copied verbatim.
#include "mysql/strconv/formats/text_format.h"

// Various internal objects written in ascii, for debugging purposes.
#include "mysql/strconv/formats/debug_format.h"

// Strings in hex format.
#include "mysql/strconv/formats/hex_format.h"

// Strings in escaped format.
#include "mysql/strconv/formats/escaped_format.h"

// Integers in variable-length binary format, strings as length+data.
#include "mysql/strconv/formats/binary_format.h"

// Integers in fixed-length binary format, strings as Binary_format.
#include "mysql/strconv/formats/fixint_binary_format.h"

// Strings of fixed length (decoder must know the size).
#include "mysql/strconv/formats/fixstr_binary_format.h"

// ==== Generic functionality to write to strings ====

// `out_str_*`, wrappers around output strings.
#include "mysql/strconv/encode/out_str.h"

// String target, the common base for String_writer and String_counter
#include "mysql/strconv/encode/string_target.h"

// String_counter, a String target that counts characters.
#include "mysql/strconv/encode/string_counter.h"

// String_writer, a String target that writes to a buffer.
#include "mysql/strconv/encode/string_writer.h"

// Helper function to write to an `out_str_*` through String_targets.
#include "mysql/strconv/encode/out_str_write.h"

// ==== Objects to hold parse options ====

// Repeat: represents the number of times a parsed object is repeated.
#include "mysql/strconv/decode/repeat.h"

// Helper for Checkers: invocables used to validate an object after parsing.
#include "mysql/strconv/decode/checker.h"

// Parse_options: uniform API to access parse options defined as Format, Repeat,
// Checker, or any combination of them.
#include "mysql/strconv/decode/parse_options.h"

// ==== Generic functionality to parse objects from strings ====

// Parse_position, holding the parsed string and the current parse position.
#include "mysql/strconv/decode/parse_position.h"

// enum Parse_status: the internal states of Parse_result.
#include "mysql/strconv/decode/parse_status.h"

// class Parse_result, holding the status, error message, and logic to propagate
// status from callee to caller.
#include "mysql/strconv/decode/parse_result.h"

// Parse_position + Parse_result + members to read sub-objects
#include "mysql/strconv/decode/parser.h"

// Fluent API, which allows us to write parsers with fewer error cases and more
// declarative syntax.
#include "mysql/strconv/decode/fluent_parser.h"

// ==== API functions parameterized by format ====

// End user API, which parses a given string using a Parser object.
#include "mysql/strconv/decode/decode.h"

// End user API, which parses a given string using a Parser object.
#include "mysql/strconv/encode/encode.h"

// ==== API functions to concatenate multiple objects ====

// Helper type used by concat.
#include "mysql/strconv/encode/concat_object.h"

// Concatenate multiple objects.
#include "mysql/strconv/encode/concat.h"

// ==== API functions for specific formats ====

// `encode_text` and `compute_encoded_length_text`
#include "mysql/strconv/encode/encode_text.h"

// `encode_debug` and `compute_encoded_length_debug`
#include "mysql/strconv/encode/encode_debug.h"

// `decode_text`
#include "mysql/strconv/decode/decode_text.h"

// ==== Formatters and parsers for integers and strings ====

// Parser for strings without the length encoded.
#include "mysql/strconv/conv/fixstr_binary_basic.h"

// Integers in ascii, strings copied verbatim.
#include "mysql/strconv/conv/text_basic.h"

// Strings in hex format.
#include "mysql/strconv/conv/hex_basic.h"

// Strings in escaped format.
#include "mysql/strconv/conv/escaped_basic.h"

// Integers in variable-length binary format, strings as length+data.
#include "mysql/strconv/conv/binary_basic.h"

// Integers in fixed-length binary format.
#include "mysql/strconv/conv/fixint_binary_basic.h"

// ==== Formatters for more specialized types ====

// Format a Parser object as a text containing an error message.
#include "mysql/strconv/conv/text_parser.h"

// Format Parse_status and Repeat objects, for debugging.
#include "mysql/strconv/conv/debug_parse_status.h"

// Format std::source_location objects, for debugging.
#include "mysql/strconv/conv/text_source_location.h"

// ==== Helpers to identify and skip whitespace ====

// Identify whitespace characters, and skip whitespace from a Parser_state
#include "mysql/strconv/decode/whitespace.h"

#endif  // ifndef MYSQL_STRCONV_STRCONV_H
