/*  Copyright (c) 2015, 2022, Oracle and/or its affiliates.

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
#ifndef RULE_INCLUDED
#define RULE_INCLUDED

#include "my_config.h"

#include <string>
#include <vector>

#include "my_dbug.h"
#include "my_inttypes.h"
#include "plugin/rewriter/persisted_rule.h"
#include "plugin/rewriter/services.h"

/// The results of an attempt to rewrite a query parse tree.
struct Rewrite_result {
  /**
    Means the query was successfully rewritten, and the new query is available
    in new_query.
  */
  bool was_rewritten;

  /// Means that at lest one matching digest was found in the hash table.
  bool digest_matched;

  /// If was_rewritten is true, a new query, otherwise an empty string.
  std::string new_query;

  Rewrite_result() : was_rewritten(false), digest_matched(false) {}
};

/**
  The in-memory representation of a pattern.
*/
class Pattern {
 public:
  enum Load_status { OK, PARSE_ERROR, NOT_SUPPORTED_STATEMENT, NO_DIGEST };

  int number_parameters;

  /// The pattern in normalized form.
  std::string normalized_pattern;

  /// The digest obtained from the pattern
  services::Digest digest;

  std::vector<std::string> literals;

  /**
    Loads the pattern. The pattern string is copied in deep-copy way. This is
    not done in the CTOR because of the memory allocation.

    This function does the following:
    - Parse the pattern string.
    - Print a normalized version.
    - Extract the position of the parameter markers.
    - Compute the digest

    @retval false Success.

    @retval true Either parse error, the pattern is not a select statement or
    out of memory.
  */
  Load_status load(MYSQL_THD thd, const Persisted_rule *diskrule);

  /**
    If any errors were raised during parsing, the first one is available here.
  */
  std::string parse_error_message() { return m_parse_error_message; }

 private:
  std::string m_parse_error_message;
};

class Replacement {
 public:
  /// The query string of the replacement
  std::string query_string;

  /**
    The number of parameters (and the size of m_param_slots and
    parameter_positions.)
  */
  int number_parameters;

  bool load(MYSQL_THD thd, const std::string replacement);

  /**
    If any errors were raised during parsing, the first one is available here.
  */
  std::string parse_error_message() { return m_parse_error_message; }

  std::vector<int> slots() const { return m_param_slots; }

 private:
  /// The positions in query_string of each parameter ('?')
  std::vector<int> m_param_slots;

  std::string m_parse_error_message;
};

/**
  Internal representation of a rewrite rule.
  A rewrite rule consists of a pattern and a replacement.
*/
class Rule {
 public:
  enum Load_status {
    OK,
    PATTERN_PARSE_ERROR,
    PATTERN_NOT_SUPPORTED_STATEMENT,
    PATTERN_GOT_NO_DIGEST,
    REPLACEMENT_PARSE_ERROR,
    REPLACEMENT_HAS_MORE_MARKERS
  };

  /// The digest buffer.
  const uchar *digest_buffer() const { return m_pattern.digest.c_ptr(); }

  /// The pattern in normalized form.
  std::string normalized_pattern() { return m_pattern.normalized_pattern; }

  /// Loads and parses the rule and replacement.
  Load_status load(MYSQL_THD thd, const Persisted_rule *diskrule) {
    switch (m_pattern.load(thd, diskrule)) {
      case Pattern::OK:
        break;
      case Pattern::PARSE_ERROR:
        return PATTERN_PARSE_ERROR;
      case Pattern::NOT_SUPPORTED_STATEMENT:
        return PATTERN_NOT_SUPPORTED_STATEMENT;
      case Pattern::NO_DIGEST:
        return PATTERN_GOT_NO_DIGEST;
    }

    if (m_replacement.load(thd, diskrule->replacement.value()))
      return REPLACEMENT_PARSE_ERROR;

    if (m_replacement.number_parameters > m_pattern.number_parameters)
      return REPLACEMENT_HAS_MORE_MARKERS;

    return OK;
  }

  /**
    Applies the rule on a query, thereby creating a new one. This is done by
    merging the replacement and literals from the query.

    @param thd Pointer to the query string.

    @retval false Everything worked, the new query is pointed to by 'query'.
    @retval true The query did not match the pattern, nothing is allocated.
  */
  Rewrite_result create_new_query(MYSQL_THD thd);

  /**
    Asks the parser service for the current query in normalized form and
    compares it to the normalized pattern. This is the equivalent of comparing
    the structure of two parse trees.

    @return True if the normalized pattern matches the current normalized
    query, otherwise false.
  */
  bool matches(MYSQL_THD thd) const;

  std::string pattern_parse_error_message() {
    return m_pattern.parse_error_message();
  }

  std::string replacement_parse_error_message() {
    return m_replacement.parse_error_message();
  }

 private:
  Pattern m_pattern;
  Replacement m_replacement;
};

#endif /* RULE_INCLUDED */
