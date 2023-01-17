/*
  Copyright (c) 2022, 2023, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef ROUTING_SQL_LEXER_INPUT_STREAM_INCLUDED
#define ROUTING_SQL_LEXER_INPUT_STREAM_INCLUDED

#include <cassert>
#include <cstddef>
#include <cstring>  // memcpy

#include "lex_string.h"   // LEX_STRING
#include "m_ctype.h"      // my_charset_...
#include "my_inttypes.h"  // uchar, uint, ...
#include "sql/lexer_yystype.h"
#include "sql/sql_digest_stream.h"
#include "sql_chars.h"  // my_lex_states

#include "sql_lexer_error.h"  // warn_on_...

class THD;

/**
  The state of the lexical parser, when parsing comments.
 */
enum enum_comment_state {
  /**
    Not parsing comments.
  */
  NO_COMMENT,

  /**
    Parsing comments that need to be preserved.
    (Copy '/' '*' and '*' '/' sequences to the preprocessed buffer.)
    Typically, these are user comments '/' '*' ... '*' '/'.
  */
  PRESERVE_COMMENT,

  /**
    Parsing comments that need to be discarded.
    (Don't copy '/' '*' '!' and '*' '/' sequences to the preprocessed buffer.)
    Typically, these are special comments '/' '*' '!' ... '*' '/',
    or '/' '*' '!' 'M' 'M' 'm' 'm' 'm' ... '*' '/', where the comment
    markers should not be expanded.
  */
  DISCARD_COMMENT
};

class Lex_input_stream {
 public:
  /**
    Constructor

    @param grammar_selector_token_arg   See grammar_selector_token.
  */

  explicit Lex_input_stream(uint grammar_selector_token_arg)
      : grammar_selector_token(grammar_selector_token_arg) {}

  /**
     Object initializer. Must be called before usage.

     @retval false OK
     @retval true  Error
  */
  bool init(THD *thd, const char *buff, size_t length);

  void reset(const char *buff, size_t length);

  /**
    Set the echo mode.

    When echo is true, characters parsed from the raw input stream are
    preserved. When false, characters parsed are silently ignored.
    @param echo the echo mode.
  */
  void set_echo(bool echo) { m_echo = echo; }

  void save_in_comment_state() {
    m_echo_saved = m_echo;
    in_comment_saved = in_comment;
  }

  void restore_in_comment_state() {
    m_echo = m_echo_saved;
    in_comment = in_comment_saved;
  }

  /**
    Skip binary from the input stream.
    @param n number of bytes to accept.
  */
  void skip_binary(int n) {
    assert(m_ptr + n <= m_end_of_query);
    if (m_echo) {
      memcpy(m_cpp_ptr, m_ptr, n);
      m_cpp_ptr += n;
    }
    m_ptr += n;
  }

  /**
    Get a character, and advance in the stream.
    @return the next character to parse.
  */
  unsigned char yyGet() {
    assert(m_ptr <= m_end_of_query);
    char c = *m_ptr++;
    if (m_echo) *m_cpp_ptr++ = c;
    return c;
  }

  /**
    Get the last character accepted.
    @return the last character accepted.
  */
  unsigned char yyGetLast() const { return m_ptr[-1]; }

  /**
    Look at the next character to parse, but do not accept it.
  */
  unsigned char yyPeek() const {
    assert(m_ptr <= m_end_of_query);
    return m_ptr[0];
  }

  /**
    Look ahead at some character to parse.
    @param n offset of the character to look up
  */
  unsigned char yyPeekn(int n) const {
    assert(m_ptr + n <= m_end_of_query);
    return m_ptr[n];
  }

  /**
    Cancel the effect of the last yyGet() or yySkip().
    Note that the echo mode should not change between calls to yyGet / yySkip
    and yyUnget. The caller is responsible for ensuring that.
  */
  void yyUnget() {
    m_ptr--;
    if (m_echo) m_cpp_ptr--;
  }

  /**
    Accept a character, by advancing the input stream.
  */
  void yySkip() {
    assert(m_ptr <= m_end_of_query);
    if (m_echo)
      *m_cpp_ptr++ = *m_ptr++;
    else
      m_ptr++;
  }

  /**
    Accept multiple characters at once.
    @param n the number of characters to accept.
  */
  void yySkipn(int n) {
    assert(m_ptr + n <= m_end_of_query);
    if (m_echo) {
      memcpy(m_cpp_ptr, m_ptr, n);
      m_cpp_ptr += n;
    }
    m_ptr += n;
  }

  /**
    Puts a character back into the stream, canceling
    the effect of the last yyGet() or yySkip().
    Note that the echo mode should not change between calls
    to unput, get, or skip from the stream.
  */
  char *yyUnput(char ch) {
    *--m_ptr = ch;
    if (m_echo) m_cpp_ptr--;
    return m_ptr;
  }

  /**
    Inject a character into the pre-processed stream.

    Note, this function is used to inject a space instead of multi-character
    C-comment. Thus there is no boundary checks here (basically, we replace
    N-chars by 1-char here).
  */
  char *cpp_inject(char ch) {
    *m_cpp_ptr = ch;
    return ++m_cpp_ptr;
  }

  /**
    End of file indicator for the query text to parse.
    @return true if there are no more characters to parse
  */
  bool eof() const { return (m_ptr >= m_end_of_query); }

  /**
    End of file indicator for the query text to parse.
    @param n number of characters expected
    @return true if there are less than n characters to parse
  */
  bool eof(int n) const { return ((m_ptr + n) >= m_end_of_query); }

  /** Get the raw query buffer. */
  const char *get_buf() const { return m_buf; }

  /** Get the pre-processed query buffer. */
  const char *get_cpp_buf() const { return m_cpp_buf; }

  /** Get the end of the raw query buffer. */
  const char *get_end_of_query() const { return m_end_of_query; }

  /** Mark the stream position as the start of a new token. */
  void start_token() {
    m_tok_start = m_ptr;
    m_tok_end = m_ptr;

    m_cpp_tok_start = m_cpp_ptr;
    m_cpp_tok_end = m_cpp_ptr;
  }

  /**
    Adjust the starting position of the current token.
    This is used to compensate for starting whitespace.
  */
  void restart_token() {
    m_tok_start = m_ptr;
    m_cpp_tok_start = m_cpp_ptr;
  }

  /** Get the token start position, in the raw buffer. */
  const char *get_tok_start() const { return m_tok_start; }

  /** Get the token start position, in the pre-processed buffer. */
  const char *get_cpp_tok_start() const { return m_cpp_tok_start; }

  /** Get the token end position, in the raw buffer. */
  const char *get_tok_end() const { return m_tok_end; }

  /** Get the token end position, in the pre-processed buffer. */
  const char *get_cpp_tok_end() const { return m_cpp_tok_end; }

  /** Get the current stream pointer, in the raw buffer. */
  const char *get_ptr() const { return m_ptr; }

  /** Get the current stream pointer, in the pre-processed buffer. */
  const char *get_cpp_ptr() const { return m_cpp_ptr; }

  /** Get the length of the current token, in the raw buffer. */
  uint yyLength() const {
    /*
      The assumption is that the lexical analyser is always 1 character ahead,
      which the -1 account for.
    */
    assert(m_ptr > m_tok_start);
    return (uint)((m_ptr - m_tok_start) - 1);
  }

  /** Get the utf8-body string. */
  const char *get_body_utf8_str() const { return m_body_utf8; }

  /** Get the utf8-body length. */
  uint get_body_utf8_length() const {
    return (uint)(m_body_utf8_ptr - m_body_utf8);
  }

  void body_utf8_start(THD *thd, const char *begin_ptr);
  void body_utf8_append(const char *ptr);
  void body_utf8_append(const char *ptr, const char *end_ptr);
  void body_utf8_append_literal(THD *thd, const LEX_STRING *txt,
                                const CHARSET_INFO *txt_cs,
                                const char *end_ptr);

  uint get_lineno(const char *raw_ptr) const;

  /** Current thread. */
  THD *m_thd;

  /** Current line number. */
  uint yylineno;

  /** Length of the last token parsed. */
  uint yytoklen;

  /** Interface with bison, value of the last token parsed. */
  Lexer_yystype *yylval;

  /**
    LALR(2) resolution, look ahead token.
    Value of the next token to return, if any,
    or -1, if no token was parsed in advance.
    Note: 0 is a legal token, and represents YYEOF.
  */
  int lookahead_token;

  /** LALR(2) resolution, value of the look ahead token.*/
  Lexer_yystype *lookahead_yylval;

  /// Skip adding of the current token's digest since it is already added
  ///
  /// Usually we calculate a digest token by token at the top-level function
  /// of the lexer: MYSQLlex(). However, some complex ("hintable") tokens break
  /// that data flow: for example, the `SELECT /*+ HINT(t) */` is the single
  /// token from the main parser's point of view, and we add the "SELECT"
  /// keyword to the digest buffer right after the lex_one_token() call,
  /// but the "/*+ HINT(t) */" is a sequence of separate tokens from the hint
  /// parser's point of view, and we add those tokens to the digest buffer
  /// *inside* the lex_one_token() call. Thus, the usual data flow adds
  /// tokens from the "/*+ HINT(t) */" string first, and only than it appends
  /// the "SELECT" keyword token to that stream: "/*+ HINT(t) */ SELECT".
  /// This is not acceptable, since we use the digest buffer to restore
  /// query strings in their normalized forms, so the order of added tokens is
  /// important. Thus, we add tokens of "hintable" keywords to a digest buffer
  /// right in the hint parser and skip adding of them at the caller with the
  /// help of skip_digest flag.
  bool skip_digest;

  void add_digest_token(uint token, Lexer_yystype *yylval);

  void reduce_digest_token(uint token_left, uint token_right);

  /**
    True if this scanner tokenizes a partial query (partition expression,
    generated column expression etc.)

    @return true if parsing a partial query, otherwise false.
  */
  bool is_partial_parser() const { return grammar_selector_token >= 0; }

  /**
    Outputs warnings on deprecated charsets in complete SQL statements

    @param [in] cs    The character set/collation to check for a deprecation.
    @param [in] alias The name/alias of @p cs.
  */
  void warn_on_deprecated_charset(const CHARSET_INFO *cs,
                                  const char *alias) const {
    if (!is_partial_parser()) {
      ::warn_on_deprecated_charset(m_thd, cs, alias);
    }
  }

  /**
    Outputs warnings on deprecated collations in complete SQL statements

    @param [in] collation     The collation to check for a deprecation.
  */
  void warn_on_deprecated_collation(const CHARSET_INFO *collation) const {
    if (!is_partial_parser()) {
      ::warn_on_deprecated_collation(m_thd, collation);
    }
  }

  const CHARSET_INFO *query_charset;

 private:
  /** Pointer to the current position in the raw input stream. */
  char *m_ptr;

  /** Starting position of the last token parsed, in the raw buffer. */
  const char *m_tok_start;

  /** Ending position of the previous token parsed, in the raw buffer. */
  const char *m_tok_end;

  /** End of the query text in the input stream, in the raw buffer. */
  const char *m_end_of_query;

  /** Begining of the query text in the input stream, in the raw buffer. */
  const char *m_buf;

  /** Length of the raw buffer. */
  size_t m_buf_length;

  /** Echo the parsed stream to the pre-processed buffer. */
  bool m_echo;
  bool m_echo_saved;

  /** Pre-processed buffer. */
  char *m_cpp_buf;

  /** Pointer to the current position in the pre-processed input stream. */
  char *m_cpp_ptr;

  /**
    Starting position of the last token parsed,
    in the pre-processed buffer.
  */
  const char *m_cpp_tok_start;

  /**
    Ending position of the previous token parsed,
    in the pre-processed buffer.
  */
  const char *m_cpp_tok_end;

  /** UTF8-body buffer created during parsing. */
  char *m_body_utf8;

  /** Pointer to the current position in the UTF8-body buffer. */
  char *m_body_utf8_ptr;

  /**
    Position in the pre-processed buffer. The query from m_cpp_buf to
    m_cpp_utf_processed_ptr is converted to UTF8-body.
  */
  const char *m_cpp_utf8_processed_ptr;

 public:
  /** Current state of the lexical analyser. */
  enum my_lex_states next_state;

  /**
    Position of ';' in the stream, to delimit multiple queries.
    This delimiter is in the raw buffer.
  */
  const char *found_semicolon;

  /** Token character bitmaps, to detect 7bit strings. */
  uchar tok_bitmap;

  /** SQL_MODE = IGNORE_SPACE. */
  bool ignore_space;

  /**
    true if we're parsing a prepared statement: in this mode
    we should allow placeholders.
  */
  bool stmt_prepare_mode;
  /**
    true if we should allow multi-statements.
  */
  bool multi_statements;

  /** State of the lexical analyser for comments. */
  enum_comment_state in_comment;
  enum_comment_state in_comment_saved;

  /**
    Starting position of the TEXT_STRING or IDENT in the pre-processed
    buffer.

    NOTE: this member must be used within MYSQLlex() function only.
  */
  const char *m_cpp_text_start;

  /**
    Ending position of the TEXT_STRING or IDENT in the pre-processed
    buffer.

    NOTE: this member must be used within MYSQLlex() function only.
    */
  const char *m_cpp_text_end;

  /**
    Character set specified by the character-set-introducer.

    NOTE: this member must be used within MYSQLlex() function only.
  */
  const CHARSET_INFO *m_underscore_cs;

  /**
    Current statement digest instrumentation.
  */
  sql_digest_state *m_digest{nullptr};

  /**
    The synthetic 1st token to prepend token stream with.

    This token value tricks parser to simulate multiple %start-ing points.
    Currently the grammar is aware of 4 such synthetic tokens:
    1. GRAMMAR_SELECTOR_PART for partitioning stuff from DD,
    2. GRAMMAR_SELECTOR_GCOL for generated column stuff from DD,
    3. GRAMMAR_SELECTOR_EXPR for generic single expressions from DD/.frm.
    4. GRAMMAR_SELECTOR_CTE for generic subquery expressions from CTEs.
    5. -1 when parsing with the main grammar (no grammar selector available).

    @note yylex() is expected to return the value of type int:
          0 is for EOF and everything else for real token numbers.
          Bison, in its turn, generates positive token numbers.
          So, the negative grammar_selector_token means "not a token".
          In other words, -1 is "empty value".
  */
  const int grammar_selector_token;

  bool text_string_is_7bit() const { return !(tok_bitmap & 0x80); }
};

#endif
