#ifndef MESSAGES_INCLUDED
#define MESSAGES_INCLUDED
/* Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

/**
  @file messages.h

  The messages that Rewriter outputs.

  @todo Move the messages in rewriter_plugin.cc and rewriter_udf.cc here.
*/

namespace rewriter_messages
{
  const char *
    PATTERN_PARSE_ERROR=            "Parse error in pattern";
  const char *
    PATTERN_NOT_A_SELECT_STATEMENT= "Pattern needs to be a a select statement.";
  const char *
    PATTERN_GOT_NO_DIGEST=          "Unable to get a digest for pattern.";
  const char *
    REPLACEMENT_PARSE_ERROR=        "Parse error in replacement";
  const char *
    REPLACEMENT_HAS_MORE_MARKERS=
    "Replacement has more parameter markers than pattern.";
}

#endif // MESSAGES_INCLUDED
