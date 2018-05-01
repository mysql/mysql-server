#ifndef MESSAGES_INCLUDED
#define MESSAGES_INCLUDED
/* Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

/**
  @file messages.h

  The messages that Rewriter outputs.

  @todo Move the messages in rewriter_plugin.cc and rewriter_udf.cc here.
*/

namespace rewriter_messages {
const char *PATTERN_PARSE_ERROR = "Parse error in pattern";
const char *PATTERN_NOT_SUPPORTED_STATEMENT =
    "Statement type of pattern not supported.";
const char *PATTERN_GOT_NO_DIGEST = "Unable to get a digest for pattern.";
const char *REPLACEMENT_PARSE_ERROR = "Parse error in replacement";
const char *REPLACEMENT_HAS_MORE_MARKERS =
    "Replacement has more parameter markers than pattern.";
}  // namespace rewriter_messages

#endif  // MESSAGES_INCLUDED
