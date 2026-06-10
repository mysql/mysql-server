/*
 * Copyright (c) 2024, 2026, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
 *
 * This program is distributed in the hope that it will be useful,  but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 * the GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

%defines
%require "2.3"
%verbose
%define parse.error verbose
%define api.pure
%locations

%lex-param { routing_guidelines::Rules_parser* rp }
%parse-param { routing_guidelines::Rules_parser* rp }

// Make sure location is initialized the same way in all bison versions
%initial-action
{
  @$.first_column = 0;
  @$.last_column = 0;
};

%{
#include <cstring>
#include <cmath>
#include "rules_parser.h"

#ifdef _MSC_VER
#pragma warning(disable : 4065)
#endif

#define YYENABLE_NLS 0

#define NUMT routing_guidelines::rpn::Token::Type::NUM
#define STRT routing_guidelines::rpn::Token::Type::STR
#define BOOLT routing_guidelines::rpn::Token::Type::BOOL
#define NONET routing_guidelines::rpn::Token::Type::NONE
#define ROLET routing_guidelines::rpn::Token::Type::ROLE

#define MATH_OP(op, token, l, s1, s2, out)                        \
  auto& left = rp->rpn_.end()[-2];                                \
  if (left.is_num() && rp->rpn_.back().is_num()) {                \
    left.number() = left.number() op rp->rpn_.back().number();    \
    rp->rpn_.pop_back();                                          \
    out = {1, NUMT};                                              \
    rp->trace("Reducing with " #op);                              \
  } else if (rp->type_error(&l, #op " operator, left operand",    \
                            NUMT, s1.type) ||                     \
             rp->type_error(&l, #op " operator, right operand",   \
                            NUMT, s2.type)) {                     \
    YYABORT;                                                      \
  } else {                                                        \
    rp->emit(token, l);                                           \
    out = {s1.toks + s2.toks + 1, NUMT};                          \
  }

#define LOG_OP(op, token, l, s1, s2, out)                         \
  auto& left = rp->rpn_.end()[-2];                                \
  if (left.is_bool() && rp->rpn_.back().is_bool()) {              \
    left = Token(left.get_bool() op rp->rpn_.back().get_bool());  \
    rp->rpn_.pop_back();                                          \
    out = {1, BOOLT};                                             \
    rp->trace("Reducing with " #op);                              \
  } else {                                                        \
    rp->emit_log_operation(token, s2.toks, l);                    \
    out = {s1.toks + s2.toks + 2, BOOLT};                         \
  }

#define COMP_OP(op, token, l, s1, s2, out)                              \
  if (s1.type != NUMT && s1.type != STRT) {                             \
    const auto msg = "type error, " + to_string(s1.type) +              \
           " type arguments cannot be compared with " #op " operator";  \
    yyerror(&l, rp, msg.c_str());                                       \
    (void)yynerrs;                                                      \
    YYABORT;                                                            \
  } else if (rp->type_error(&l,                                         \
      #op " operator, the type of left operand does not match right" ,  \
                            s1.type, s2.type)) {                        \
    YYABORT;                                                            \
  } else if (rp->rpn_.end()[-2].type() != rp->rpn_.back().type()) {     \
    rp->emit(token, l);                                                 \
    out = {s1.toks + s2.toks + 1, BOOLT};                               \
  } else {                                                              \
    try {                                                               \
      bool res = rp->rpn_.end()[-2] op rp->rpn_.back();                 \
      rp->rpn_.pop_back();                                              \
      rp->rpn_.back() = Token(res);                                     \
      out = {1, BOOLT};                                                 \
      rp->trace("Reducing with " #op);                                  \
    } catch (...) {                                                     \
      rp->emit(token, l);                                               \
      out = {s1.toks + s2.toks + 1, BOOLT};                             \
    }                                                                   \
  }


#define EQ_OP(op, token, l, s1, s2, out)                                \
  if (s1.type != NONET && s2.type != NONET && rp->type_error(&l,        \
      strcmp(#op, "==") == 0 ?                                          \
        "= operator, the type of left operand does not match right" :   \
        "<> operator, the type of left operand does not match right",   \
                            s1.type, s2.type)) {                        \
    YYABORT;                                                            \
  } else if (rp->check_role_types(s1, s2, &l)) {                        \
    YYABORT;                                                            \
  } else if (rp->rpn_.end()[-2].type() != rp->rpn_.back().type()) {     \
    rp->emit(token, l);                                                 \
    out = {s1.toks + s2.toks + 1, BOOLT};                               \
  } else {                                                              \
    try {                                                               \
      bool res = rp->rpn_.end()[-2] op rp->rpn_.back();                 \
      rp->rpn_.pop_back();                                              \
      rp->rpn_.back() = Token(res);                                     \
      out = {1, BOOLT};                                                 \
      rp->trace("Reducing with " #op);                                  \
    } catch (...) {                                                     \
      rp->emit(token, l);                                               \
      out = {s1.toks + s2.toks + 1, BOOLT};                             \
    }                                                                   \
  }

using Token = routing_guidelines::rpn::Token;
using Rtype = routing_guidelines::rpn::Token::Type;

%}

%union {
  double num;
  std::string_view str{};
  routing_guidelines::Exp_info exp;
  routing_guidelines::List_info* list;
  const routing_guidelines::rpn::Function_definition* func;
}

%token
  T_END  0  "end of expression"
  T_ERROR   "error"
  T_EOL     "\n"
  T_DASH    "-"
  T_PLUS    "+"
  T_STAR    "*"
  T_SLASH   "/"
  T_PERCENT "%"
  T_LPAREN  "("
  T_RPAREN  ")"
  T_GT      ">"
  T_LT      "<"
  T_GE     ">="
  T_LE     "<="
  T_EQ     "="
  T_NE     "<>"
  T_IN
  T_LIKE
  T_NOT
  T_AND
  T_OR
  T_COMMA  ","
  T_TRUE
  T_FALSE
  T_NULL
;

%token <str> T_IDENTIFIER "identifier"
%token <str> T_VARREF     "variable reference"
%token <str> T_STRING     "string"
%token <str> T_ROLE       "role"

%token <num> T_NUMBER "number"

%nterm <exp> exp

%nterm <list> explist

%token <func> T_FUNCTION   "function name"

%destructor {
    if ($$) {
      delete $$;
      $$ = nullptr;
    }
} explist

%%

%start unit;

%left     T_OR;
%left     T_AND;
%nonassoc T_NOT;
%nonassoc ">" "<" ">=" "<=" "<>" "=" T_IN T_LIKE;
%left     "+" "-";
%left     "*" "/" "%";

unit: %empty
| exp
| unit T_ERROR          { YYABORT; }

exp:
  T_NUMBER          { rp->emit_num($1); $$ = {1, NUMT} ; }
| T_TRUE            { rp->emit_num(1, Rtype::BOOL); $$ = {1, BOOLT}; }
| T_FALSE           { rp->emit_num(0, Rtype::BOOL); $$ = {1, BOOLT}; }
| T_NULL            { rp->emit_null(); $$ = {1, NONET}; }
| T_STRING          { rp->emit_string($1); $$ = {1, STRT}; }
| T_IDENTIFIER      { rp->emit_string($1); $$ = {1, STRT}; }
| T_VARREF          { if (!rp->emit_reference($1, &@$, &$$)) YYABORT; }
| T_ROLE            { rp->emit_string($1, ROLET); $$ = {1, ROLET}; }
| "(" exp ")"       { $$ = $2; }

// Arithetic operations
| exp "+" exp       { MATH_OP(+, Rtype::ADD ,@$, $1, $3, $$); }
| exp "-" exp       { MATH_OP(-, Rtype::MIN ,@$, $1, $3, $$); }
| exp "*" exp       { MATH_OP(*, Rtype::MUL ,@$, $1, $3, $$); }
| exp "/" exp       { MATH_OP(/, Rtype::DIV ,@$, $1, $3, $$); }
| exp "%" exp       {
  auto& left = rp->rpn_.end()[-2];
  if (left.is_num() && rp->rpn_.back().is_num()) {
    left.number() = std::fmod(left.number(), rp->rpn_.back().number());
    rp->rpn_.pop_back();
    $$ = {1, NUMT};
    rp->trace("Reducing with %");
  } else if (rp->type_error(&@$, "% operator, left operand", NUMT, $1.type) ||
             rp->type_error(&@$, "% operator, right operand", NUMT, $3.type)) {
    YYABORT;
  } else {
    rp->emit(Rtype::MOD ,@$);
    $$ = {$1.toks + $3.toks + 1, NUMT};
  }
}
| "-" exp           {
  if (rp->rpn_.back().is_num()) {
    rp->rpn_.back().number() *= -1;
    $$ = {1, NUMT};
  } else if (rp->type_error(&@$, "- operator", NUMT, $2.type)) {
    YYABORT;
  } else {
    rp->emit(Rtype::NEG ,@$);
    $$ = {$2.toks + 1, NUMT};
  }
}

// IN operator
| exp T_IN "(" explist ")"  { if (!rp->emit_in_op($1, $4, &@$, &$$)) YYABORT; }
| exp T_NOT T_IN "(" explist ")" {
  if (!rp->emit_in_op($1, $5, &@$, &$$)) {
    YYABORT;
  } else {
    rp->emit(Rtype::NOT ,@2);
    $$.toks++;
  }
}

// LIKE operator
| exp T_LIKE exp  { if (!rp->emit_like_op($1, $3, &@$, &$$)) YYABORT; }
| exp T_NOT T_LIKE exp {
  if (!rp->emit_like_op($1, $4, &@$, &$$)) {
    YYABORT;
  } else {
    rp->emit(Rtype::NOT ,@2);
    $$.toks++;
  }
}

// Comparisons
| exp ">"  exp      { COMP_OP(>,  Rtype::GT ,@$, $1, $3, $$); }
| exp "<"  exp      { COMP_OP(<,  Rtype::LT ,@$, $1, $3, $$); }
| exp ">=" exp      { COMP_OP(>=, Rtype::GE ,@$, $1, $3, $$); }
| exp "<=" exp      { COMP_OP(<=, Rtype::LE ,@$, $1, $3, $$); }
| exp "="  exp      { EQ_OP(==, Rtype::EQ ,@$, $1, $3, $$); }
| exp "<>" exp      { EQ_OP(!=, Rtype::NE ,@$, $1, $3, $$); }

// Logical operations, no type checking required as everything casts to bool
| exp T_AND exp     { LOG_OP(&&, Rtype::AND ,@$, $1, $3, $$); }
| exp T_OR exp      { LOG_OP(||, Rtype::OR ,@$, $1, $3, $$); }
| T_NOT exp         {
  if (rp->rpn_.back().is_bool()) {
    rp->rpn_.back() = Token(!rp->rpn_.back().get_bool());
    rp->trace("Reducing with NOT");
    $$ = {1, BOOLT};
  } else {
    rp->emit(Rtype::NOT ,@$);
    $$ = {$2.toks + 1, BOOLT};
  }
}

// Functions
| T_FUNCTION "(" ")" {
  if (!rp->emit_function($1, nullptr, &@$, &$$)) YYABORT;
}
| T_FUNCTION "(" explist ")" {
  if (!rp->emit_function($1, $3, &@$, &$$)) YYABORT;
};

explist:
  exp             { $$ = new routing_guidelines::List_info{$1}; }
| explist "," exp { $1->emplace_back($3); $$ = $1; };

%%
