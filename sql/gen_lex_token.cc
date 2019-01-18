/*
   Copyright (c) 2011, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* We only need the tokens here */
#define YYSTYPE_IS_DECLARED

#include "sql/lex.h"
#include "sql/lex_symbol.h"
#include "sql/sql_yacc.h"
#include "welcome_copyright_notice.h" /* ORACLE_WELCOME_COPYRIGHT_NOTICE */

/*
  MAINTAINER:

  Tokens printed in sql/lex_token.h do come from several sources:
  - tokens from sql_yacc.yy
  - tokens from sql_hints.yy
  - fake tokens for digests.

  All the token values are mapped in the same space,
  indexed by the token value directly.

  To account for enhancements and new tokens,
  gap are created, so that adding a token from one source
  does not change values of tokens from other sources.

  This is done to ensure stability in digest computed values.

  As of now (8.0.0), the mapping looks like this:
  - [0 .. 255] character terminal tokens.
  - [256 .. 907] non terminal tokens from sql_yacc.yy
  - [908 .. 999] reserved for sql_yacc.yy new tokens
  - [1000 .. 1017] non terminal tokens from sql_hints.yy
  - [1018 .. 1099] reserved for sql_hints.yy new tokens
  - [1100 .. 1111] non terminal tokens for digests

  Should gen_lex_token fail when tokens are exhausted
  (maybe you are reading this comment because of a fprintf(stderr) below),
  the options are as follows, by order of decreasing desirability:

  1) Reuse OBSOLETE_TOKEN_XXX instead of consuming new token values

  2) Consider if you really need to create a new token,
  instead of re using an existing one.

  Keep in mind that syntax sugar in the parser still adds
  to complexity, by making the parser tables bigger,
  so adding tokens all the time is not a good practice.

  3) Expand the values for
     - start_token_range_for_sql_hints
     - start_token_range_for_digests
  and record again all the MTR tests that print a DIGEST,
  because DIGEST values have now changed.

  While at it, because digests have changed anyway,
  please seriously consider to clean up and reorder:
  - all the tokens in sql/sql_yacc.yy in one nice list,
  ordered alphabetically, removing obsolete values if any.
  - likewise for sql/sql_hints.yy
*/

int start_token_range_for_sql_hints = 1000;
int start_token_range_for_digests = 1100;
/*
  This is a tool used during build only,
  so MY_MAX_TOKEN does not need to be exact,
  only big enough to hold:
  - 256 character terminal tokens
  - YYNTOKENS named terminal tokens from bison (sql_yacc.yy).
  - padding (see start_token_range_for_sql_hints)
  - YYNTOKENS named terminal tokens from bison (sql_hints.yy).
  - padding (see start_token_range_for_digests)
  - DIGEST special tokens.
  See also YYMAXUTOK.
*/
#define MY_MAX_TOKEN 1200
/** Generated token. */
struct gen_lex_token_string {
  const char *m_token_string;
  int m_token_length;
  bool m_append_space;
  bool m_start_expr;
};

gen_lex_token_string compiled_token_array[MY_MAX_TOKEN];
int max_token_seen = 0;
int max_token_seen_in_sql_yacc = 0;
int max_token_seen_in_sql_hints = 0;

char char_tokens[256];

int tok_generic_value = 0;
int tok_generic_value_list = 0;
int tok_row_single_value = 0;
int tok_row_single_value_list = 0;
int tok_row_multiple_value = 0;
int tok_row_multiple_value_list = 0;
int tok_in_generic_value_expression = 0;
int tok_ident = 0;
int tok_ident_at = 0;  ///< Fake token for the left part of table\@query_block.
int tok_hint_comment_open =
    0;  ///< Fake token value for "/*+" of hint comments.
int tok_hint_comment_close =
    0;  ///< Fake token value for "*/" of hint comments.
int tok_unused = 0;

/**
  Adjustment value to translate hint parser's internal token values to generally
  visible token values. This adjustment is necessary, since keyword token values
  of separate parsers may interfere.
*/
int tok_hint_adjust = 0;

static void set_token(int tok, const char *str) {
  if (tok <= 0) {
    fprintf(stderr, "Bad token found\n");
    exit(1);
  }

  if (tok > max_token_seen) {
    max_token_seen = tok;
  }

  if (max_token_seen >= MY_MAX_TOKEN) {
    fprintf(stderr, "Added that many new keywords ? Increase MY_MAX_TOKEN\n");
    exit(1);
  }

  compiled_token_array[tok].m_token_string = str;
  compiled_token_array[tok].m_token_length = strlen(str);
  compiled_token_array[tok].m_append_space = true;
  compiled_token_array[tok].m_start_expr = false;
}

static void set_start_expr_token(int tok) {
  compiled_token_array[tok].m_start_expr = true;
}

static void compute_tokens() {
  int tok;
  unsigned int i;
  char *str;

  /*
    Default value.
  */
  for (tok = 0; tok < MY_MAX_TOKEN; tok++) {
    compiled_token_array[tok].m_token_string = "(unknown)";
    compiled_token_array[tok].m_token_length = 9;
    compiled_token_array[tok].m_append_space = true;
    compiled_token_array[tok].m_start_expr = false;
  }

  /*
    Tokens made of just one terminal character
  */
  for (tok = 0; tok < 256; tok++) {
    str = &char_tokens[tok];
    str[0] = (char)tok;
    compiled_token_array[tok].m_token_string = str;
    compiled_token_array[tok].m_token_length = 1;
    compiled_token_array[tok].m_append_space = true;
  }

  max_token_seen = 255;

  /*
    String terminal tokens, used in sql_yacc.yy
  */
  set_token(NEG, "~");

  /*
    Tokens hard coded in sql_lex.cc
  */

  set_token(WITH_ROLLUP_SYM, "WITH ROLLUP");
  set_token(NOT2_SYM, "!");
  set_token(OR2_SYM, "|");
  set_token(PARAM_MARKER, "?");
  set_token(SET_VAR, ":=");
  set_token(UNDERSCORE_CHARSET, "(_charset)");
  set_token(END_OF_INPUT, "");
  set_token(JSON_SEPARATOR_SYM, "->");
  set_token(JSON_UNQUOTED_SEPARATOR_SYM, "->>");

  /*
    Values.
    These tokens are all normalized later,
    so this strings will never be displayed.
  */
  set_token(BIN_NUM, "(bin)");
  set_token(DECIMAL_NUM, "(decimal)");
  set_token(FLOAT_NUM, "(float)");
  set_token(HEX_NUM, "(hex)");
  set_token(LEX_HOSTNAME, "(hostname)");
  set_token(LONG_NUM, "(long)");
  set_token(NUM, "(num)");
  set_token(TEXT_STRING, "(text)");
  set_token(NCHAR_STRING, "(nchar)");
  set_token(ULONGLONG_NUM, "(ulonglong)");

  /*
    Identifiers.
  */
  set_token(IDENT, "(id)");
  set_token(IDENT_QUOTED, "(id_quoted)");

  /*
    See symbols[] in sql/lex.h
  */
  for (i = 0; i < sizeof(symbols) / sizeof(symbols[0]); i++) {
    if (!(symbols[i].group & SG_MAIN_PARSER)) continue;
    set_token(symbols[i].tok, symbols[i].name);
  }

  max_token_seen_in_sql_yacc = max_token_seen;

  if (max_token_seen_in_sql_yacc >= start_token_range_for_sql_hints) {
    fprintf(stderr,
            "sql/sql_yacc.yy token reserve exhausted.\n"
            "Please see MAINTAINER instructions in sql/gen_lex_token.cc\n");
    exit(1);
  }

  /*
    FAKE tokens to output "optimizer hint" keywords.

    Hint keyword token values may interfere with token values of the main SQL
    parser, so the tok_hint_adjust adjustment is needed to add them into
    compiled_token_array and lex_token_array.

    Also see the TOK_HINT_ADJUST() adjustment macro definition.
  */
  int tok_hint_min = INT_MAX;
  for (unsigned int i = 0; i < sizeof(symbols) / sizeof(symbols[0]); i++) {
    if (!(symbols[i].group & SG_HINTS)) continue;

    if (static_cast<int>(symbols[i].tok) < tok_hint_min)
      tok_hint_min = symbols[i].tok;  // Calculate the minimal hint token value.
  }

  tok_hint_adjust = start_token_range_for_sql_hints - tok_hint_min;

  for (unsigned int i = 0; i < sizeof(symbols) / sizeof(symbols[0]); i++) {
    if (!(symbols[i].group & SG_HINTS)) continue;
    set_token(symbols[i].tok + tok_hint_adjust, symbols[i].name);
  }

  max_token_seen_in_sql_hints = max_token_seen;

  if (max_token_seen_in_sql_hints >= start_token_range_for_digests) {
    fprintf(stderr,
            "sql/sql_hints.yy token reserve exhausted.\n"
            "Please see MAINTAINER instructions in sql/gen_lex_token.cc\n");
    exit(1);
  }

  max_token_seen = start_token_range_for_digests;

  /*
    Additional FAKE tokens,
    used internally to normalize a digest text.
  */

  /* Digest tokens in 5.7 */

  tok_generic_value = max_token_seen++;
  set_token(tok_generic_value, "?");

  tok_generic_value_list = max_token_seen++;
  set_token(tok_generic_value_list, "?, ...");

  tok_row_single_value = max_token_seen++;
  set_token(tok_row_single_value, "(?)");

  tok_row_single_value_list = max_token_seen++;
  set_token(tok_row_single_value_list, "(?) /* , ... */");

  tok_row_multiple_value = max_token_seen++;
  set_token(tok_row_multiple_value, "(...)");

  tok_row_multiple_value_list = max_token_seen++;
  set_token(tok_row_multiple_value_list, "(...) /* , ... */");

  tok_ident = max_token_seen++;
  set_token(tok_ident, "(tok_id)");

  tok_ident_at = max_token_seen++;
  set_token(tok_ident_at, "(tok_id_at)");

  tok_hint_comment_open = max_token_seen++;
  set_token(tok_hint_comment_open, HINT_COMMENT_STARTER);

  tok_hint_comment_close = max_token_seen++;
  set_token(tok_hint_comment_close, HINT_COMMENT_TERMINATOR);

  /* New in 8.0 */

  tok_in_generic_value_expression = max_token_seen++;
  set_token(tok_in_generic_value_expression, "IN (...)");

  /* Add new digest tokens here */

  tok_unused = max_token_seen++;
  set_token(tok_unused, "UNUSED");

  /*
    Fix whitespace for some special tokens.
  */

  /*
    The lexer parses "@@variable" as '@', '@', 'variable',
    returning a token for '@' alone.

    This is incorrect, '@' is not really a token,
    because the syntax "@ @ variable" (with spaces) is not accepted:
    The lexer keeps some internal state after the '@' fake token.

    To work around this, digest text are printed as "@@variable".
  */
  compiled_token_array[(int)'@'].m_append_space = false;

  /*
    Define additional properties for tokens.

    List all the token that are followed by an expression.
    This is needed to differentiate unary from binary
    '+' and '-' operators, because we want to:
    - reduce <unary +> <NUM> to <?>,
    - preserve <...> <binary +> <NUM> as is.
  */
  set_start_expr_token('(');
  set_start_expr_token(',');
  set_start_expr_token(EVERY_SYM);
  set_start_expr_token(AT_SYM);
  set_start_expr_token(STARTS_SYM);
  set_start_expr_token(ENDS_SYM);
  set_start_expr_token(DEFAULT_SYM);
  set_start_expr_token(RETURN_SYM);
  set_start_expr_token(IF);
  set_start_expr_token(ELSEIF_SYM);
  set_start_expr_token(CASE_SYM);
  set_start_expr_token(WHEN_SYM);
  set_start_expr_token(WHILE_SYM);
  set_start_expr_token(UNTIL_SYM);
  set_start_expr_token(SELECT_SYM);

  set_start_expr_token(OR_SYM);
  set_start_expr_token(OR2_SYM);
  set_start_expr_token(XOR);
  set_start_expr_token(AND_SYM);
  set_start_expr_token(AND_AND_SYM);
  set_start_expr_token(NOT_SYM);
  set_start_expr_token(BETWEEN_SYM);
  set_start_expr_token(LIKE);
  set_start_expr_token(REGEXP);

  set_start_expr_token('|');
  set_start_expr_token('&');
  set_start_expr_token(SHIFT_LEFT);
  set_start_expr_token(SHIFT_RIGHT);
  set_start_expr_token('+');
  set_start_expr_token('-');
  set_start_expr_token(INTERVAL_SYM);
  set_start_expr_token('*');
  set_start_expr_token('/');
  set_start_expr_token('%');
  set_start_expr_token(DIV_SYM);
  set_start_expr_token(MOD_SYM);
  set_start_expr_token('^');
}

static void print_tokens() {
  int tok;

  printf("#ifdef LEX_TOKEN_WITH_DEFINITION\n");
  printf("lex_token_string lex_token_array[]=\n");
  printf("{\n");
  printf("/* PART 1: character tokens. */\n");

  for (tok = 0; tok < 256; tok++) {
    printf("/* %03d */  { \"\\x%02x\", 1, %s, %s},\n", tok, tok,
           compiled_token_array[tok].m_append_space ? "true" : "false",
           compiled_token_array[tok].m_start_expr ? "true" : "false");
  }

  printf("/* PART 2: named tokens from sql/sql_yacc.yy. */\n");

  for (tok = 256; tok <= max_token_seen_in_sql_yacc; tok++) {
    printf("/* %03d */  { \"%s\", %d, %s, %s},\n", tok,
           compiled_token_array[tok].m_token_string,
           compiled_token_array[tok].m_token_length,
           compiled_token_array[tok].m_append_space ? "true" : "false",
           compiled_token_array[tok].m_start_expr ? "true" : "false");
  }

  printf("/* PART 3: padding reserved for sql/sql_yacc.yy extensions. */\n");

  for (tok = max_token_seen_in_sql_yacc + 1;
       tok < start_token_range_for_sql_hints; tok++) {
    printf(
        "/* reserved %03d for sql/sql_yacc.yy */  { \"\", 0, false, false},\n",
        tok);
  }

  printf("/* PART 4: named tokens from sql/sql_hints.yy. */\n");

  for (tok = start_token_range_for_sql_hints;
       tok <= max_token_seen_in_sql_hints; tok++) {
    printf("/* %03d */  { \"%s\", %d, %s, %s},\n", tok,
           compiled_token_array[tok].m_token_string,
           compiled_token_array[tok].m_token_length,
           compiled_token_array[tok].m_append_space ? "true" : "false",
           compiled_token_array[tok].m_start_expr ? "true" : "false");
  }

  printf("/* PART 5: padding reserved for sql/sql_hints.yy extensions. */\n");

  for (tok = max_token_seen_in_sql_hints + 1;
       tok < start_token_range_for_digests; tok++) {
    printf(
        "/* reserved %03d for sql/sql_hints.yy */  { \"\", 0, false, false},\n",
        tok);
  }

  printf("/* PART 6: Digest special tokens. */\n");

  for (tok = start_token_range_for_digests; tok < max_token_seen; tok++) {
    printf("/* %03d */  { \"%s\", %d, %s, %s},\n", tok,
           compiled_token_array[tok].m_token_string,
           compiled_token_array[tok].m_token_length,
           compiled_token_array[tok].m_append_space ? "true" : "false",
           compiled_token_array[tok].m_start_expr ? "true" : "false");
  }

  printf("/* PART 7: End of token list. */\n");

  printf("/* DUMMY */ { \"\", 0, false, false}\n");
  printf("};\n");
  printf("#endif /* LEX_TOKEN_WITH_DEFINITION */\n");

  printf("/* DIGEST specific tokens. */\n");
  printf("#define TOK_GENERIC_VALUE %d\n", tok_generic_value);
  printf("#define TOK_GENERIC_VALUE_LIST %d\n", tok_generic_value_list);
  printf("#define TOK_ROW_SINGLE_VALUE %d\n", tok_row_single_value);
  printf("#define TOK_ROW_SINGLE_VALUE_LIST %d\n", tok_row_single_value_list);
  printf("#define TOK_ROW_MULTIPLE_VALUE %d\n", tok_row_multiple_value);
  printf("#define TOK_ROW_MULTIPLE_VALUE_LIST %d\n",
         tok_row_multiple_value_list);
  printf("#define TOK_IDENT %d\n", tok_ident);
  printf("#define TOK_IDENT_AT %d\n", tok_ident_at);
  printf("#define TOK_HINT_COMMENT_OPEN %d\n", tok_hint_comment_open);
  printf("#define TOK_HINT_COMMENT_CLOSE %d\n", tok_hint_comment_close);
  printf("#define TOK_IN_GENERIC_VALUE_EXPRESSION %d\n",
         tok_in_generic_value_expression);
  printf("#define TOK_HINT_ADJUST(x) ((x) + %d)\n", tok_hint_adjust);
  printf("#define TOK_UNUSED %d\n", tok_unused);
}

/*
  ZEROFILL_SYM is the last token in the MySQL 5.7 token list,
  see sql/sql_yacc.yy
  The token value is frozen and should not change,
  to avoid changing query digest values.
*/
static const int zerofill_expected_value = 906;

int main(int, char **) {
  if (ZEROFILL_SYM < zerofill_expected_value) {
    fprintf(stderr,
            "Token deleted.\n"
            "Please read MAINTAINER instructions in sql/sql_yacc.yy\n");
    return 1;
  }

  if (ZEROFILL_SYM > zerofill_expected_value) {
    fprintf(stderr,
            "Token added in the wrong place.\n"
            "Please read MAINTAINER instructions in sql/sql_yacc.yy\n");
    return 1;
  }

  puts(ORACLE_GPL_COPYRIGHT_NOTICE("2016"));

  printf("/*\n");
  printf("  This file is generated, do not edit.\n");
  printf("  See file sql/gen_lex_token.cc.\n");
  printf("*/\n");
  printf("struct lex_token_string\n");
  printf("{\n");
  printf("  const char *m_token_string;\n");
  printf("  int m_token_length;\n");
  printf("  bool m_append_space;\n");
  printf("  bool m_start_expr;\n");
  printf("};\n");
  printf("typedef struct lex_token_string lex_token_string;\n");

  compute_tokens();
  print_tokens();

  return 0;
}
