# Copyright (c) 2024, 2026, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is designed to work with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms, as
# designated in a particular file or component or in included license
# documentation. The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have either included with
# the program or referenced in the documentation.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See
# the GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
SET(TARGET_HEADERS
  conv/binary_basic.h
  conv/debug_parse_status.h
  conv/debug_repeat.h
  conv/escaped_basic.h
  conv/fixint_binary_basic.h
  conv/fixstr_binary_basic.h
  conv/hex_basic.h
  conv/text_basic.h
  conv/text_parser.h
  conv/text_source_location.h
  decode/checker.h
  decode/decode.h
  decode/decode_text.h
  decode/fluent_parser.h
  decode/parse_options.h
  decode/parse_position.h
  decode/parse_result.h
  decode/parse_status.h
  decode/parser.h
  decode/repeat.h
  decode/whitespace.h
  encode/concat.h
  encode/concat_object.h
  encode/encode.h
  encode/encode_debug.h
  encode/encode_text.h
  encode/out_str.h
  encode/out_str_write.h
  encode/string_counter.h
  encode/string_target.h
  encode/string_writer.h
  formats/binary_format.h
  formats/debug_format.h
  formats/escaped_format.h
  formats/fixint_binary_format.h
  formats/fixstr_binary_format.h
  formats/format.h
  formats/hex_format.h
  formats/resolve_format.h
  formats/text_format.h
  strconv.h
)

SET(TARGET_SRCS
)
