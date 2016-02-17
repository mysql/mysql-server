/*
   Copyright (c) 2011, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include <my_global.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* We only need the tokens here */
#define YYSTYPE_IS_DECLARED
#include <lex.h>

#include <welcome_copyright_notice.h> /* ORACLE_WELCOME_COPYRIGHT_NOTICE */

/*
  This is a tool used during build only,
  so MY_MAX_TOKEN does not need to be exact,
  only big enough to hold:
  - 256 character terminal tokens
  - YYNTOKENS named terminal tokens
  from bison.
  See also YYMAXUTOK.
*/
#define MY_MAX_TOKEN 1000
/** Generated token. */
struct gen_lex_token_string
{
  const char *m_token_string;
  int m_token_length;
  bool m_append_space;
  bool m_start_expr;
};

gen_lex_token_string compiled_token_array[MY_MAX_TOKEN];
int max_token_seen= 0;

char char_tokens[256];

int tok_generic_value= 0;
int tok_generic_value_list= 0;
int tok_row_single_value= 0;
int tok_row_single_value_list= 0;
int tok_row_multiple_value= 0;
int tok_row_multiple_value_list= 0;
int tok_ident= 0;
int tok_ident_at= 0; ///< Fake token for the left part of table@query_block.
int tok_hint_comment_open= 0; ///< Fake token value for "/*+" of hint comments.
int tok_hint_comment_close= 0; ///< Fake token value for "*/" of hint comments.
int tok_unused= 0;

/**
  Adjustment value to translate hint parser's internal token values to generally
  visible token values. This adjustment is necessary, since keyword token values
  of separate parsers may interfere.
*/
int tok_hint_adjust= 0;

void set_token(int tok, const char *str)
{
  if (tok <= 0)
  {
    fprintf(stderr, "Bad token found\n");
    exit(1);
  }

  if (tok > max_token_seen)
  {
    max_token_seen= tok;
  }

  if (max_token_seen >= MY_MAX_TOKEN)
  {
    fprintf(stderr, "Added that many new keywords ? Increase MY_MAX_TOKEN\n");
    exit(1);
  }

  compiled_token_array[tok].m_token_string= str;
  compiled_token_array[tok].m_token_length= strlen(str);
  compiled_token_array[tok].m_append_space= true;
  compiled_token_array[tok].m_start_expr= false;
}

void set_start_expr_token(int tok)
{
  compiled_token_array[tok].m_start_expr= true;
}

void compute_tokens()
{
  int tok;
  unsigned int i;
  char *str;

  /*
    Default value.
  */
  for (tok= 0; tok < MY_MAX_TOKEN; tok++)
  {
    compiled_token_array[tok].m_token_string= "(unknown)";
    compiled_token_array[tok].m_token_length= 9;
    compiled_token_array[tok].m_append_space= true;
    compiled_token_array[tok].m_start_expr= false;
  }

  /*
    Tokens made of just one terminal character
  */
  for (tok=0; tok < 256; tok++)
  {
    str= & char_tokens[tok];
    str[0]= (char) tok;
    compiled_token_array[tok].m_token_string= str;
    compiled_token_array[tok].m_token_length= 1;
    compiled_token_array[tok].m_append_space= true;
  }

  max_token_seen= 255;

  /*
    String terminal tokens, used in sql_yacc.yy
  */
  set_token(NEG, "~");
  set_token(TABLE_REF_PRIORITY, "TABLE_REF_PRIORITY");

  /*
    Tokens hard coded in sql_lex.cc
  */

  set_token(WITH_CUBE_SYM, "WITH CUBE");
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
    Unused tokens
  */
  set_token(LOCATOR_SYM, "LOCATOR");
  set_token(SERVER_OPTIONS, "SERVER_OPTIONS");
  set_token(UDF_RETURNS_SYM, "UDF_RETURNS");

  /*
    See symbols[] in sql/lex.h
  */
  for (i= 0; i< sizeof(symbols)/sizeof(symbols[0]); i++)
  {
    if (!(symbols[i].group & SG_MAIN_PARSER))
      continue;
    set_token(symbols[i].tok, symbols[i].name);
  }

  /*
    FAKE tokens to output "optimizer hint" keywords.

    Hint keyword token values may interfere with token values of the main SQL
    parser, so the tok_hint_adjust adjustment is needed to add them into
    compiled_token_array and lex_token_array.

    Also see the TOK_HINT_ADJUST() adjustment macro definition.
  */
  int tok_hint_min= INT_MAX;
  for (unsigned int i= 0; i < sizeof(symbols)/sizeof(symbols[0]); i++)
  {
    if ((symbols[i].group & SG_HINTS) &&
        static_cast<int>(symbols[i].tok) < tok_hint_min)
      tok_hint_min= symbols[i].tok; // Calculate the minimal hint token value.
  }
  tok_hint_adjust= max_token_seen + 1 - tok_hint_min;
  for (unsigned int i= 0; i < sizeof(symbols)/sizeof(symbols[0]); i++)
  {
    if (!(symbols[i].group & SG_HINTS))
      continue;
    set_token(symbols[i].tok + tok_hint_adjust, symbols[i].name);
  }

  /*
    Additional FAKE tokens,
    used internally to normalize a digest text.
  */

  max_token_seen++;
  tok_generic_value= max_token_seen;
  set_token(tok_generic_value, "?");

  max_token_seen++;
  tok_generic_value_list= max_token_seen;
  set_token(tok_generic_value_list, "?, ...");

  max_token_seen++;
  tok_row_single_value= max_token_seen;
  set_token(tok_row_single_value, "(?)");

  max_token_seen++;
  tok_row_single_value_list= max_token_seen;
  set_token(tok_row_single_value_list, "(?) /* , ... */");

  max_token_seen++;
  tok_row_multiple_value= max_token_seen;
  set_token(tok_row_multiple_value, "(...)");

  max_token_seen++;
  tok_row_multiple_value_list= max_token_seen;
  set_token(tok_row_multiple_value_list, "(...) /* , ... */");

  max_token_seen++;
  tok_ident= max_token_seen;
  set_token(tok_ident, "(tok_id)");

  max_token_seen++;
  tok_ident_at= max_token_seen;
  set_token(tok_ident_at, "(tok_id_at)");

  max_token_seen++;
  tok_hint_comment_open= max_token_seen;
  set_token(tok_hint_comment_open, HINT_COMMENT_STARTER);

  max_token_seen++;
  tok_hint_comment_close= max_token_seen;
  set_token(tok_hint_comment_close, HINT_COMMENT_TERMINATOR);

  max_token_seen++;
  tok_unused= max_token_seen;
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
  compiled_token_array[(int) '@'].m_append_space= false;

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
  set_start_expr_token(DEFAULT);
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

void print_tokens()
{
  int tok;

  printf("#ifdef LEX_TOKEN_WITH_DEFINITION\n");
  printf("lex_token_string lex_token_array[]=\n");
  printf("{\n");
  printf("/* PART 1: character tokens. */\n");

  for (tok= 0; tok<256; tok++)
  {
    printf("/* %03d */  { \"\\x%02x\", 1, %s, %s},\n",
           tok,
           tok,
           compiled_token_array[tok].m_append_space ? "true" : "false",
           compiled_token_array[tok].m_start_expr ? "true" : "false");
  }

  printf("/* PART 2: named tokens. */\n");

  for (tok= 256; tok<= max_token_seen; tok++)
  {
    printf("/* %03d */  { \"%s\", %d, %s, %s},\n",
           tok,
           compiled_token_array[tok].m_token_string,
           compiled_token_array[tok].m_token_length,
           compiled_token_array[tok].m_append_space ? "true" : "false",
           compiled_token_array[tok].m_start_expr ? "true" : "false");
  }

  printf("/* DUMMY */ { \"\", 0, false, false}\n");
  printf("};\n");
  printf("#endif /* LEX_TOKEN_WITH_DEFINITION */\n");

  printf("/* DIGEST specific tokens. */\n");
  printf("#define TOK_GENERIC_VALUE %d\n", tok_generic_value);
  printf("#define TOK_GENERIC_VALUE_LIST %d\n", tok_generic_value_list);
  printf("#define TOK_ROW_SINGLE_VALUE %d\n", tok_row_single_value);
  printf("#define TOK_ROW_SINGLE_VALUE_LIST %d\n", tok_row_single_value_list);
  printf("#define TOK_ROW_MULTIPLE_VALUE %d\n", tok_row_multiple_value);
  printf("#define TOK_ROW_MULTIPLE_VALUE_LIST %d\n", tok_row_multiple_value_list);
  printf("#define TOK_IDENT %d\n", tok_ident);
  printf("#define TOK_IDENT_AT %d\n", tok_ident_at);
  printf("#define TOK_HINT_COMMENT_OPEN %d\n", tok_hint_comment_open);
  printf("#define TOK_HINT_COMMENT_CLOSE %d\n", tok_hint_comment_close);
  printf("#define TOK_HINT_ADJUST(x) ((x) + %d)\n", tok_hint_adjust);
  printf("#define TOK_UNUSED %d\n", tok_unused);
}

int main(int argc,char **argv)
{
  puts("/*");
  puts(ORACLE_WELCOME_COPYRIGHT_NOTICE("2011"));
  puts("*/");

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

