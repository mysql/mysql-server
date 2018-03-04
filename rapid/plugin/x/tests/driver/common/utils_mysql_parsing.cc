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
//--------------------------------------------------------------------------------------------------
#include "plugin/x/tests/driver/common/utils_mysql_parsing.h"

#include "plugin/x/tests/driver/common/utils_string_parsing.h"

namespace shcore {
namespace mysql {
namespace splitter {
//--------------------------------------------------------------------------------------------------

const unsigned char *skip_leading_whitespace(const unsigned char *head,
                                             const unsigned char *tail) {
  while (head < tail && *head <= ' ') head++;
  return head;
}

//--------------------------------------------------------------------------------------------------

bool is_line_break(const unsigned char *head, const unsigned char *line_break) {
  if (*line_break == '\0') return false;

  while (*head != '\0' && *line_break != '\0' && *head == *line_break) {
    head++;
    line_break++;
  }
  return *line_break == '\0';
}

//--------------------------------------------------------------------------------------------------

/**
 * A statement splitter to take a list of sql statements and split them into
 *individual statements,
 * return their position and length in the original string (instead the copied
 *strings).
 *
 * A tweak was added to the function to return the number of complete statements
 *found, where
 * complete means the ending delimiter was found.
 */
size_t determineStatementRanges(const char *sql, size_t length,
                                std::string &delimiter,
                                std::vector<std::pair<size_t, size_t> > &ranges,
                                const std::string &line_break,
                                std::stack<std::string> &input_context_stack) {
  int full_statement_count = 0;
  const unsigned char *delimiter_head = (unsigned char *)delimiter.c_str();

  const unsigned char keyword[] = "delimiter";

  const unsigned char *head = (unsigned char *)sql;
  const unsigned char *tail = head;
  const unsigned char *end = head + length;
  const unsigned char *new_line = (unsigned char *)line_break.c_str();
  bool have_content = false;  // Set when anything else but comments were found
                              // for the current statement.

  ranges.clear();

  while (tail < end) {
    switch (*tail) {
      case '*':  // Comes from a multiline comment and comment is done
        if (*(tail + 1) == '/' && (!input_context_stack.empty() &&
                                   input_context_stack.top() == "/*")) {
          if (!input_context_stack.empty()) input_context_stack.pop();

          tail += 2;
          head = tail;  // Skip over the comment.
        }
        break;
      case '/':  // Possible multi line comment or hidden (conditional) command.
        if (*(tail + 1) == '*') {
          tail += 2;
          bool is_hidden_command = (*tail == '!');
          while (true) {
            while (tail < end && *tail != '*') tail++;
            if (tail == end)  // Unfinished comment.
            {
              input_context_stack.push("/*");
              break;
            } else {
              if (*++tail == '/') {
                tail++;  // Skip the slash too.
                break;
              }
            }
          }

          if (!is_hidden_command && !have_content)
            head = tail;  // Skip over the comment.
        }
        break;

      case '-':  // Possible single line comment.
      {
        const unsigned char *end_char = tail + 2;
        if (*(tail + 1) == '-' &&
            (*end_char == ' ' || *end_char == '\t' ||
             is_line_break(end_char, new_line) || length == 2)) {
          // Skip everything until the end of the line.
          tail += 2;
          while (tail < end && !is_line_break(tail, new_line)) tail++;
          if (!have_content) head = tail;
        }
        break;
      }

      case '#':  // MySQL single line comment.
        while (tail < end && !is_line_break(tail, new_line)) tail++;
        if (!have_content) head = tail;
        break;

      case '"':
      case '\'':
      case '`': {
        have_content = true;
        char quote = *tail++;

        if (input_context_stack.empty() || input_context_stack.top() == "-") {
          // Quoted string/id. Skip this in a local loop if is opening quote.
          while (tail < end && *tail != quote) {
            // Skip any escaped character too.
            if (*tail == '\\') tail++;
            tail++;
          }
          if (*tail == quote)
            tail++;  // Skip trailing quote char to if one was there.
          else {
            std::string q;
            q.assign(&quote, 1);
            input_context_stack.push(
                q);  // Sets multiline opening quote to continue processing
          }
        } else  // Closing quote, clears the multiline flag
          input_context_stack.pop();

        break;
      }

      case 'd':
      case 'D': {
        have_content = true;

        // Possible start of the keyword DELIMITER. Must be at the start of the
        // text or a character,
        // which is not part of a regular MySQL identifier (0-9, A-Z, a-z, _, $,
        // \u0080-\uffff).
        unsigned char previous = tail > (unsigned char *)sql ? *(tail - 1) : 0;
        bool is_identifier_char =
            previous >= 0x80 || (previous >= '0' && previous <= '9') ||
            ((previous | 0x20) >= 'a' && (previous | 0x20) <= 'z') ||
            previous == '$' || previous == '_';
        if (tail == (unsigned char *)sql || !is_identifier_char) {
          const unsigned char *run = tail + 1;
          const unsigned char *kw = keyword + 1;
          int count = 9;
          while (count-- > 1 && (*run++ | 0x20) == *kw++)
            ;
          if (count == 0 && *run == ' ') {
            // Delimiter keyword found. Get the new delimiter (everything until
            // the end of the line).
            tail = run++;
            while (run < end && !is_line_break(run, new_line)) run++;

            delimiter = std::string((char *)tail, run - tail);
            aux::trim(delimiter);

            delimiter_head = (unsigned char *)delimiter.c_str();

            // Skip over the delimiter statement and any following line breaks.
            while (is_line_break(run, new_line)) run++;
            tail = run;
            head = tail;
          }
        }
        break;
      }
    }

    if (*tail == *delimiter_head) {
      // Found possible start of the delimiter. Check if it really is.
      size_t count = delimiter.size();
      if (count == 1) {
        // Most common case. Trim the statement and check if it is not empty
        // before adding the range.
        head = skip_leading_whitespace(head, tail);
        if (head < tail || (!input_context_stack.empty() &&
                            input_context_stack.top() == "-")) {
          full_statement_count++;

          if (!input_context_stack.empty()) input_context_stack.pop();

          if (head < tail)
            ranges.push_back(std::make_pair<size_t, size_t>(
                head - (unsigned char *)sql, tail - head));
        }
        head = ++tail;
        have_content = false;
      } else {
        const unsigned char *run = tail + 1;
        const unsigned char *del = delimiter_head + 1;
        while (count-- > 1 && (*run++ == *del++))
          ;

        if (count == 0) {
          // Multi char delimiter is complete. Tail still points to the start of
          // the delimiter.
          // Run points to the first character after the delimiter.
          head = skip_leading_whitespace(head, tail);
          if (head < tail || (!input_context_stack.empty() &&
                              input_context_stack.top() == "-")) {
            full_statement_count++;

            if (!input_context_stack.empty()) input_context_stack.pop();

            if (head < tail)
              ranges.push_back(std::make_pair<size_t, size_t>(
                  head - (unsigned char *)sql, tail - head));
          }

          tail = run;
          head = run;
          have_content = false;
        }
      }
    }

    // Multiline comments are ignored, everything else is not
    if (*tail > ' ' &&
        (input_context_stack.empty() || input_context_stack.top() != "/*"))
      have_content = true;
    tail++;
  }

  // Add remaining text to the range list but ignores it when it is a multiline
  // comment
  head = skip_leading_whitespace(head, tail);
  if (head < tail &&
      (input_context_stack.empty() || input_context_stack.top() != "/*")) {
    ranges.push_back(std::make_pair<size_t, size_t>(head - (unsigned char *)sql,
                                                    tail - head));

    // If not a multiline string then sets the flag to multiline statement (not
    // terminated)
    if (input_context_stack.empty()) input_context_stack.push("-");
  }

  return full_statement_count;
}
}
}
}
