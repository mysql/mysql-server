#!/bin/sh
# Copyright (c) 2024, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is designed to work with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have either included with
# the program or referenced in the documentation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

if [ $# = 0 ] ; then
    cat <<EOF
$0 FILE

Remove C/C++ comments from the input.
EOF
    exit
fi

# This regex is a partial lexer for C++. The purpose is to identify comments,
# i.e., /*...*/ and //...\n. For this, it needs to tokenize the comment
# delimiters, i.e., '/*', '*/', '//', and '\n'.
#
# It also needs to tokenize string literals, so that it does not treat '/*' and
# '*/' as comment delimiters when they occur in strings. So it also tokenizes
# double quotes.
#
# To parse string literals, it also needs to tokenize character literals, so it
# does not treat " as a string literal delimiter when it occurs in a character
# literal.
#
# To parse character literals,  it also needs to parse numbers with thousands
# separators, so that it does not treat ' as a character literal when it is part
# of a number.
#
# So we match all these types of tokens.  We capture all non-comments in capture
# group $2, and capture the newline that ends a C++-style comment (//) in
# capture group $1.
c_comment='/\*[^\*]*(?:\*[^/][^\*]*)*\*/'
cpp_comment='//[^\r\\]*(?:\\.[^\r\\]*)*(\r)'
string='"[^"\\]*(?:\\.[^"\\]*)*"'
character="'\\\\?.'"
thousands_separator="[0-9]'"
regex="$c_comment|$cpp_comment|($string|$character|$thousands_separator)"

#echo "regex=$regex"

# rg --replace --multiline is buggy
# So instead we convert each file to a single line.
cat $1 |
  tr '\n' '\r' |
  rg --replace '$1$2' "$regex" - |
  tr '\r' '\n'
