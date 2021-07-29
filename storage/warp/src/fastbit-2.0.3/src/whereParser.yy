/* $Id$ -*- mode: c++ -*- */
// Author: John Wu <John.Wu at acm.org>
//      Lawrence Berkeley National Laboratory
// Copyright (c) 2007-2016 the Regents of the University of California

%code top {
/** \file Defines the parser for the where clause accepted by FastBit IBIS.
    The definitions are processed through bison.
*/

#include <iostream>
}
%code requires {
#include "whereClause.h"	// class whereClause
#include <time.h>		// strptime
}

/* bison declarations */
%require "2.7"
%debug
%error-verbose
%start START
%defines
%skeleton "lalr1.cc"
%define api.namespace {ibis}
%define parser_class_name {whereParser}
%locations
     /*%expect 1*/
%initial-action
{ // initialize location object
    @$.begin.filename = @$.end.filename = &(driver.clause_);
};

%parse-param {class ibis::whereClause& driver}

%union {
    int		 integerVal;
    int64_t	 int64Val;
    uint64_t	 uint64Val;
    double	 doubleVal;
    std::string *stringVal;
    ibis::qExpr *whereNode;
}

%token              END       0 "end of input"
%token <integerVal> NULLOP	"null"
%token <integerVal> NOTOP	"not"
%token <integerVal> LEOP	"<="
%token <integerVal> GEOP	">="
%token <integerVal> LTOP	"<"
%token <integerVal> GTOP	">"
%token <integerVal> EQOP	"=="
%token <integerVal> NEQOP	"!="
%token <integerVal> ANDOP	"and"
%token <integerVal> ANDNOTOP	"&!"
%token <integerVal> OROP	"or"
%token <integerVal> XOROP	"xor"
%token <integerVal> BETWEENOP	"between"
%token <integerVal> CONTAINSOP	"contains"
%token <integerVal> EXISTSOP	"exists"
%token <integerVal> INOP	"in"
%token <integerVal> LIKEOP	"like"
%token <integerVal> FROM_UNIXTIME_GMT	"FROM_UNIXTIME_GMT"
%token <integerVal> FROM_UNIXTIME_LOCAL	"FROM_UNIXTIME_LOCAL"
%token <integerVal> TO_UNIXTIME_GMT	"TO_UNIXTIME_GMT"
%token <integerVal> TO_UNIXTIME_LOCAL	"TO_UNIXTIME_LOCAL"
%token <integerVal> ISO_TO_UNIXTIME_GMT	"ISO_TO_UNIXTIME_GMT"
%token <integerVal> ISO_TO_UNIXTIME_LOCAL	"ISO_TO_UNIXTIME_LOCAL"
%token <integerVal> ANYOP	"any"
%token <integerVal> BITOROP	"|"
%token <integerVal> BITANDOP	"&"
%token <integerVal> ADDOP	"+"
%token <integerVal> MINUSOP	"-"
%token <integerVal> MULTOP	"*"
%token <integerVal> DIVOP	"/"
%token <integerVal> REMOP	"%"
%token <integerVal> EXPOP	"**"
%token <int64Val>   INT64	"integer value"
%token <uint64Val>  UINT64	"unsigned integer value"
%token <doubleVal>  NUMBER	"floating-point number"
%token <stringVal>  INTSEQ	"signed integer sequence"
%token <stringVal>  UINTSEQ	"unsigned integer sequence"
%token <stringVal>  NOUNSTR	"name string"
%token <stringVal>  NUMSEQ	"number sequence"
%token <stringVal>  STRSEQ	"string sequence"
%token <stringVal>  STRLIT	"string literal"

%left OROP
%left XOROP
%left ANDOP ANDNOTOP
%nonassoc ANYOP INOP LIKEOP CONSTAINSOP EXISTSOP
%nonassoc EQOP NEQOP
%left BITOROP
%left BITANDOP
%left ADDOP MINUSOP
%left MULTOP DIVOP REMOP
%right EXPOP
%right NOTOP

%type <whereNode> qexpr simpleRange compRange2 compRange3 mathExpr


%destructor { delete $$; } INTSEQ UINTSEQ NOUNSTR NUMSEQ STRSEQ STRLIT
%destructor { delete $$; } qexpr simpleRange compRange2 compRange3 mathExpr

%{
#include "whereLexer.h"

#undef yylex
#define yylex driver.lexer->lex
%}

%% /* Grammar rules */
qexpr:
qexpr OROP qexpr {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *$1
	<< " || " << *$3;
#endif
    $$ = new ibis::qExpr(ibis::qExpr::LOGICAL_OR);
    $$->setRight($3);
    $$->setLeft($1);
}
| qexpr XOROP qexpr {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *$1
	<< " ^ " << *$3;
#endif
    $$ = new ibis::qExpr(ibis::qExpr::LOGICAL_XOR);
    $$->setRight($3);
    $$->setLeft($1);
}
| qexpr ANDOP qexpr {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *$1
	<< " && " << *$3;
#endif
    $$ = new ibis::qExpr(ibis::qExpr::LOGICAL_AND);
    $$->setRight($3);
    $$->setLeft($1);
}
| qexpr ANDNOTOP qexpr {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *$1
	<< " &~ " << *$3;
#endif
    $$ = new ibis::qExpr(ibis::qExpr::LOGICAL_MINUS);
    $$->setRight($3);
    $$->setLeft($1);
}
| NOTOP qexpr {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- ! " << *$2;
#endif
    $$ = new ibis::qExpr(ibis::qExpr::LOGICAL_NOT);
    $$->setLeft($2);
}
| '(' qexpr ')' %prec NOTOP {
    $$ = $2;
}
| simpleRange
| compRange2
| compRange3
;

simpleRange:
EXISTSOP NOUNSTR {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- EXISTS(" << *$2 << ')';
#endif
    $$ = new ibis::qExists($2->c_str());
    delete $2;
}
| EXISTSOP STRLIT {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- EXISTS(" << *$2 << ')';
#endif
    $$ = new ibis::qExists($2->c_str());
    delete $2;
}
| EXISTSOP '(' NOUNSTR ')' {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- EXISTS(" << *$3 << ')';
#endif
    $$ = new ibis::qExists($3->c_str());
    delete $3;
}
| EXISTSOP '(' STRLIT ')' {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- EXISTS(" << *$3 << ')';
#endif
    $$ = new ibis::qExists($3->c_str());
    delete $3;
}
| NOUNSTR INOP NUMSEQ {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *$1 << " IN ("
	<< *$3 << ")";
#endif
    $$ = new ibis::qDiscreteRange($1->c_str(), $3->c_str());
    delete $3;
    delete $1;
}
| NOUNSTR INOP '(' NUMBER ',' NUMBER ')' {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *$1 << " IN ("
	<< $4 << ", " << $6 << ")";
#endif
    std::vector<double> vals(2);
    vals[0] = $4;
    vals[1] = $6;
    $$ = new ibis::qDiscreteRange($1->c_str(), vals);
    delete $1;
}
| NOUNSTR INOP '(' NUMBER ')' {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *$1 << " IN ("
	<< $4 << ")";
#endif
    $$ = new ibis::qContinuousRange($1->c_str(), ibis::qExpr::OP_EQ, $4);
    delete $1;
}
| NOUNSTR NOTOP NULLOP {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *$1 << " NOT NULL";
#endif
    $$ = new ibis::qContinuousRange($1->c_str(), ibis::qExpr::OP_UNDEFINED, 0U);
}
| NOUNSTR NOTOP INOP NUMSEQ {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *$1 << " NOT IN ("
	<< *$4 << ")";
#endif
    $$ = new ibis::qExpr(ibis::qExpr::LOGICAL_NOT);
    $$->setLeft(new ibis::qDiscreteRange($1->c_str(), $4->c_str()));
    delete $4;
    delete $1;
}
| NOUNSTR NOTOP INOP '(' NUMBER ',' NUMBER ')' {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *$1 << " NOT IN ("
	<< $5 << ", " << $7 << ")";
#endif
    std::vector<double> vals(2);
    vals[0] = $5;
    vals[1] = $7;
    $$ = new ibis::qExpr(ibis::qExpr::LOGICAL_NOT);
    $$->setLeft(new ibis::qDiscreteRange($1->c_str(), vals));
    delete $1;
}
| NOUNSTR NOTOP INOP '(' NUMBER ')' {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *$1 << " NOT IN ("
	<< $5 << ")";
#endif
    $$ = new ibis::qExpr(ibis::qExpr::LOGICAL_NOT);
    $$->setLeft(new ibis::qContinuousRange($1->c_str(), ibis::qExpr::OP_EQ, $5));
    delete $1;
}
| NOUNSTR INOP STRSEQ {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *$1 << " IN ("
	<< *$3 << ")";
#endif
    $$ = new ibis::qAnyString($1->c_str(), $3->c_str());
    delete $3;
    delete $1;
}
| NOUNSTR INOP '(' NOUNSTR ',' NOUNSTR ')' {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *$1 << " IN ("
	<< *$4 << ", " << *$6 << ")";
#endif
    std::string val;
    val = '"'; /* add quote to keep strings intact */
    val += *$4;
    val += "\", \"";
    val += *$6;
    val += '"';
    $$ = new ibis::qAnyString($1->c_str(), val.c_str());
    delete $6;
    delete $4;
    delete $1;
}
| NOUNSTR INOP '(' STRLIT ',' NOUNSTR ')' {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *$1 << " IN ("
	<< *$4 << ", " << *$6 << ")";
#endif
    std::string val;
    val = '"'; /* add quote to keep strings intact */
    val += *$4;
    val += "\", \"";
    val += *$6;
    val += '"';
    $$ = new ibis::qAnyString($1->c_str(), val.c_str());
    delete $6;
    delete $4;
    delete $1;
}
| NOUNSTR INOP '(' NOUNSTR ',' STRLIT ')' {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *$1 << " IN ("
	<< *$4 << ", " << *$6 << ")";
#endif
    std::string val;
    val = '"'; /* add quote to keep strings intact */
    val += *$4;
    val += "\", \"";
    val += *$6;
    val += '"';
    $$ = new ibis::qAnyString($1->c_str(), val.c_str());
    delete $6;
    delete $4;
    delete $1;
}
| NOUNSTR INOP '(' STRLIT ',' STRLIT ')' {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *$1 << " IN ("
	<< *$4 << ", " << *$6 << ")";
#endif
    std::string val;
    val = '"'; /* add quote to keep strings intact */
    val += *$4;
    val += "\", \"";
    val += *$6;
    val += '"';
    $$ = new ibis::qAnyString($1->c_str(), val.c_str());
    delete $6;
    delete $4;
    delete $1;
}
| NOUNSTR INOP '(' NOUNSTR ')' {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *$1 << " IN ("
	<< *$4 << ")";
#endif
    std::string val;
    val = '"'; /* add quote to keep strings intact */
    val += *$4;
    val += '"';
    $$ = new ibis::qAnyString($1->c_str(), val.c_str());
    delete $4;
    delete $1;
}
| NOUNSTR INOP '(' STRLIT ')' {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *$1 << " IN ("
	<< *$4 << ")";
#endif
    std::string val;
    val = '"'; /* add quote to keep strings intact */
    val += *$4;
    val += '"';
    $$ = new ibis::qAnyString($1->c_str(), val.c_str());
    delete $4;
    delete $1;
}
| NOUNSTR LIKEOP NOUNSTR {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *$1 << " LIKE "
	<< *$3;
#endif
    $$ = new ibis::qLike($1->c_str(), $3->c_str());
    delete $3;
    delete $1;
}
| NOUNSTR LIKEOP STRLIT {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *$1 << " LIKE "
	<< *$3;
#endif
    $$ = new ibis::qLike($1->c_str(), $3->c_str());
    delete $3;
    delete $1;
}
| NOUNSTR NOTOP INOP STRSEQ {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *$1 << " NOT IN ("
	<< *$4 << ")";
#endif
    $$ = new ibis::qExpr(ibis::qExpr::LOGICAL_NOT);
    $$->setLeft(new ibis::qAnyString($1->c_str(), $4->c_str()));
    delete $4;
    delete $1;
}
| NOUNSTR NOTOP INOP '(' NOUNSTR ',' NOUNSTR ')' {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *$1 << " NOT IN ("
	<< *$5 << ", " << *$7 << ")";
#endif
    std::string val;
    val = '"'; /* add quote to keep strings intact */
    val += *$5;
    val += "\", \"";
    val += *$7;
    val += '"';
    $$ = new ibis::qExpr(ibis::qExpr::LOGICAL_NOT);
    $$->setLeft(new ibis::qAnyString($1->c_str(), val.c_str()));
    delete $7;
    delete $5;
    delete $1;
}
| NOUNSTR NOTOP INOP '(' STRLIT ',' NOUNSTR ')' {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *$1 << " NOT IN ("
	<< *$5 << ", " << *$7 << ")";
#endif
    std::string val;
    val = '"'; /* add quote to keep strings intact */
    val += *$5;
    val += "\", \"";
    val += *$7;
    val += '"';
    $$ = new ibis::qExpr(ibis::qExpr::LOGICAL_NOT);
    $$->setLeft(new ibis::qAnyString($1->c_str(), val.c_str()));
    delete $7;
    delete $5;
    delete $1;
}
| NOUNSTR NOTOP INOP '(' NOUNSTR ',' STRLIT ')' {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *$1 << " NOT IN ("
	<< *$5 << ", " << *$7 << ")";
#endif
    std::string val;
    val = '"'; /* add quote to keep strings intact */
    val += *$5;
    val += "\", \"";
    val += *$7;
    val += '"';
    $$ = new ibis::qExpr(ibis::qExpr::LOGICAL_NOT);
    $$->setLeft(new ibis::qAnyString($1->c_str(), val.c_str()));
    delete $7;
    delete $5;
    delete $1;
}
| NOUNSTR NOTOP INOP '(' STRLIT ',' STRLIT ')' {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *$1 << " NOT IN ("
	<< *$5 << ", " << *$7 << ")";
#endif
    std::string val;
    val = '"'; /* add quote to keep strings intact */
    val += *$5;
    val += "\", \"";
    val += *$7;
    val += '"';
    $$ = new ibis::qExpr(ibis::qExpr::LOGICAL_NOT);
    $$->setLeft(new ibis::qAnyString($1->c_str(), val.c_str()));
    delete $7;
    delete $5;
    delete $1;
}
| NOUNSTR NOTOP INOP '(' NOUNSTR ')' {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *$1 << " NOT IN ("
	<< *$5 << ")";
#endif
    std::string val;
    val = '"'; /* add quote to keep strings intact */
    val += *$5;
    val += '"';
    $$ = new ibis::qExpr(ibis::qExpr::LOGICAL_NOT);
    $$->setLeft(new ibis::qAnyString($1->c_str(), val.c_str()));
    delete $5;
    delete $1;
}
| NOUNSTR NOTOP INOP '(' STRLIT ')' {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *$1 << " NOT IN ("
	<< *$5 << ")";
#endif
    std::string val;
    val = '"'; /* add quote to keep strings intact */
    val += *$5;
    val += '"';
    $$ = new ibis::qExpr(ibis::qExpr::LOGICAL_NOT);
    $$->setLeft(new ibis::qAnyString($1->c_str(), val.c_str()));
    delete $5;
    delete $1;
}
| NOUNSTR INOP INTSEQ {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *$1 << " in ("
	<< *$3 << ")";
#endif
    $$ = new ibis::qIntHod($1->c_str(), $3->c_str());
    delete $3;
    delete $1;
}
| NOUNSTR NOTOP INOP INTSEQ {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *$1 << " not in ("
	<< *$4 << ")";
#endif
    $$ = new ibis::qExpr(ibis::qExpr::LOGICAL_NOT);
    $$->setLeft(new ibis::qIntHod($1->c_str(), $4->c_str()));
    delete $4;
    delete $1;
}
| NOUNSTR INOP UINTSEQ {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *$1 << " in ("
	<< *$3 << ")";
#endif
    $$ = new ibis::qUIntHod($1->c_str(), $3->c_str());
    delete $3;
    delete $1;
}
| NOUNSTR NOTOP INOP UINTSEQ {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *$1 << " not in ("
	<< *$4 << ")";
#endif
    $$ = new ibis::qExpr(ibis::qExpr::LOGICAL_NOT);
    $$->setLeft(new ibis::qUIntHod($1->c_str(), $4->c_str()));
    delete $4;
    delete $1;
}
| NOUNSTR CONTAINSOP NOUNSTR {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *$1
	<< " CONTAINS " << *$3;
#endif
    $$ = new ibis::qKeyword($1->c_str(), $3->c_str());
    delete $1;
    delete $3;
}
| NOUNSTR CONTAINSOP STRLIT {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << $1
	<< " CONTAINS " << *$3;
#endif
    $$ = new ibis::qKeyword($1->c_str(), $3->c_str());
    delete $3;
    delete $1;
}
| NOUNSTR CONTAINSOP '(' NOUNSTR ')' {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *$1
	<< " CONTAINS " << *$4;
#endif
    $$ = new ibis::qKeyword($1->c_str(), $4->c_str());
    delete $1;
    delete $4;
}
| NOUNSTR CONTAINSOP '(' STRLIT ')' {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << $1
	<< " CONTAINS " << *$4;
#endif
    $$ = new ibis::qKeyword($1->c_str(), $4->c_str());
    delete $4;
    delete $1;
}
| NOUNSTR CONTAINSOP '(' STRLIT ',' STRLIT ')' {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << $1
	<< " CONTAINS (" << *$4 << ", " << *$6 << ')';
#endif
    $$ = new ibis::qAllWords($1->c_str(), $4->c_str(), $6->c_str());
    delete $6;
    delete $4;
    delete $1;
}
| NOUNSTR CONTAINSOP '(' STRLIT ',' NOUNSTR ')' {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << $1
	<< " CONTAINS (" << *$4 << ", " << *$6 << ')';
#endif
    $$ = new ibis::qAllWords($1->c_str(), $4->c_str(), $6->c_str());
    delete $6;
    delete $4;
    delete $1;
}
| NOUNSTR CONTAINSOP '(' NOUNSTR ',' STRLIT ')' {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << $1
	<< " CONTAINS (" << *$4 << ", " << *$6 << ')';
#endif
    $$ = new ibis::qAllWords($1->c_str(), $4->c_str(), $6->c_str());
    delete $6;
    delete $4;
    delete $1;
}
| NOUNSTR CONTAINSOP '(' NOUNSTR ',' NOUNSTR ')' {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << $1
	<< " CONTAINS (" << *$4 << ", " << *$6 << ')';
#endif
    $$ = new ibis::qAllWords($1->c_str(), $4->c_str(), $6->c_str());
    delete $6;
    delete $4;
    delete $1;
}
| NOUNSTR CONTAINSOP STRSEQ {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << $1
	<< " CONTAINS (" << *$3 << ')';
#endif
    $$ = new ibis::qAllWords($1->c_str(), $3->c_str());
    delete $3;
    delete $1;
}
| ANYOP '(' NOUNSTR ')' EQOP NUMBER {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- ANY(" << *$3 << ") = "
	<< $6 << ")";
#endif
    $$ = new ibis::qAnyAny($3->c_str(), $6);
    delete $3;
}
| ANYOP '(' NOUNSTR ')' INOP NUMSEQ {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- ANY(" << *$3 << ") = "
	<< *$6 << ")";
#endif
    $$ = new ibis::qAnyAny($3->c_str(), $6->c_str());
    delete $6;
    delete $3;
}
| NOUNSTR EQOP INT64 {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *$1 << " = " << *$3;
#endif
    $$ = new ibis::qIntHod($1->c_str(), $3);
    delete $1;
}
| NOUNSTR NEQOP INT64 {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *$1 << " != " << *$3;
#endif
    $$ = new ibis::qExpr(ibis::qExpr::LOGICAL_NOT);
    $$->setLeft(new ibis::qIntHod($1->c_str(), $3));
    delete $1;
}
| NOUNSTR EQOP UINT64 {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *$1 << " = " << *$3;
#endif
    $$ = new ibis::qUIntHod($1->c_str(), $3);
    delete $1;
}
| NOUNSTR NEQOP UINT64 {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *$1 << " != " << *$3;
#endif
    $$ = new ibis::qExpr(ibis::qExpr::LOGICAL_NOT);
    $$->setLeft(new ibis::qUIntHod($1->c_str(), $3));
    delete $1;
}
| STRLIT EQOP NOUNSTR {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *$3 << " = "
	<< *$1;
#endif
    $$ = new ibis::qString($3->c_str(), $1->c_str());
    delete $3;
    delete $1;
}
| STRLIT NEQOP NOUNSTR {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *$3 << " = "
	<< *$1;
#endif
    $$ = new ibis::qExpr(ibis::qExpr::LOGICAL_NOT);
    $$->setLeft(new ibis::qString($3->c_str(), $1->c_str()));
    delete $3;
    delete $1;
}
| NOUNSTR EQOP STRLIT {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *$1 << " = "
	<< *$3;
#endif
    $$ = new ibis::qString($1->c_str(), $3->c_str());
    delete $3;
    delete $1;
}
| NOUNSTR NEQOP STRLIT {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *$1 << " != "
	<< *$3;
#endif
    $$ = new ibis::qExpr(ibis::qExpr::LOGICAL_NOT);
    $$->setLeft(new ibis::qString($1->c_str(), $3->c_str()));
    delete $3;
    delete $1;
}
| NOUNSTR EQOP mathExpr {
    ibis::math::term *me2 = static_cast<ibis::math::term*>($3);
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *$1 << " = "
	<< *me2;
#endif
    if (me2->termType() == ibis::math::NUMBER) {
	$$ = new ibis::qContinuousRange($1->c_str(), ibis::qExpr::OP_EQ, me2->eval());
	delete $3;
    }
    else {
	ibis::math::variable *me1 = new ibis::math::variable($1->c_str());
	$$ = new ibis::compRange(me1, ibis::qExpr::OP_EQ, me2);
    }
    delete $1;
}
| NOUNSTR NEQOP mathExpr {
    ibis::math::term *me2 = static_cast<ibis::math::term*>($3);
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *$1 << " = "
	<< *me2;
#endif
    ibis::qExpr*tmp = 0;
    if (me2->termType() == ibis::math::NUMBER) {
	tmp = new ibis::qContinuousRange($1->c_str(), ibis::qExpr::OP_EQ, me2->eval());
	delete $3;
    }
    else {
	ibis::math::variable *me1 = new ibis::math::variable($1->c_str());
	tmp = new ibis::compRange(me1, ibis::qExpr::OP_EQ, me2);
    }
    delete $1;
    $$ = new ibis::qExpr(ibis::qExpr::LOGICAL_NOT);
    $$->setLeft(tmp);
}
;

compRange2:
mathExpr EQOP mathExpr {
    ibis::math::term *me2 = static_cast<ibis::math::term*>($3);
    ibis::math::term *me1 = static_cast<ibis::math::term*>($1);
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *me1 << " = "
	<< *me2;
#endif
    $$ = new ibis::compRange(me1, ibis::qExpr::OP_EQ, me2);
}
| mathExpr NEQOP mathExpr {
    ibis::math::term *me2 = static_cast<ibis::math::term*>($3);
    ibis::math::term *me1 = static_cast<ibis::math::term*>($1);
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *me1 << " != "
	<< *me2;
#endif
    $$ = new ibis::qExpr(ibis::qExpr::LOGICAL_NOT);
    $$->setLeft(new ibis::compRange(me1, ibis::qExpr::OP_EQ, me2));
}
| mathExpr LTOP mathExpr {
    ibis::math::term *me2 = static_cast<ibis::math::term*>($3);
    ibis::math::term *me1 = static_cast<ibis::math::term*>($1);
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *me1 << " < "
	<< *me2;
#endif
    $$ = new ibis::compRange(me1, ibis::qExpr::OP_LT, me2);
}
| mathExpr LEOP mathExpr {
    ibis::math::term *me2 = static_cast<ibis::math::term*>($3);
    ibis::math::term *me1 = static_cast<ibis::math::term*>($1);
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *me1 << " <= "
	<< *me2;
#endif
    $$ = new ibis::compRange(me1, ibis::qExpr::OP_LE, me2);
}
| mathExpr GTOP mathExpr {
    ibis::math::term *me2 = static_cast<ibis::math::term*>($3);
    ibis::math::term *me1 = static_cast<ibis::math::term*>($1);
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *me1 << " > "
	<< *me2;
#endif
    $$ = new ibis::compRange(me1, ibis::qExpr::OP_GT, me2);
}
| mathExpr GEOP mathExpr {
    ibis::math::term *me2 = static_cast<ibis::math::term*>($3);
    ibis::math::term *me1 = static_cast<ibis::math::term*>($1);
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *me1 << " >= "
	<< *me2;
#endif
    $$ = new ibis::compRange(me1, ibis::qExpr::OP_GE, me2);
}
// | mathExpr EQOP STRLIT {
//     ibis::math::term *me1 = static_cast<ibis::math::term*>($1);
// #if defined(DEBUG) && DEBUG + 0 > 1
//     LOGGER(ibis::gVerbose >= 0)
// 	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *me1 << " = "
// 	<< *$3;
// #endif
//     ibis::math::variable *var = dynamic_cast<ibis::math::variable*>(me1);
//     if (var != 0) {
// 	$$ = new ibis::qString(var->variableName(), $3->c_str());
// 	delete $3;
// 	delete var;
//     }
//     else {
// 	LOGGER(ibis::gVerbose >= 0)
// 	    << "whereParser.yy: rule mathExpr == 'string literal' is a "
// 	    "kludge for Name == 'string literal'.  The mathExpr on the "
// 	    "left can only be variable name, currently " << *me1;
// 	delete $3;
// 	delete me1;
// 	throw "The rule on line 419 in whereParser.yy expects a simple "
// 	    "variable name on the left-hand side";
//     }
// }
// | mathExpr NEQOP STRLIT {
//     ibis::math::term *me1 = static_cast<ibis::math::term*>($1);
// #if defined(DEBUG) && DEBUG + 0 > 1
//     LOGGER(ibis::gVerbose >= 0)
// 	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *me1 << " != "
// 	<< *$3;
// #endif
//     ibis::math::variable *var = dynamic_cast<ibis::math::variable*>(me1);
//     if (var != 0) {
// 	$$ = new ibis::qExpr(ibis::qExpr::LOGICAL_NOT);
// 	$$->setLeft(new ibis::qString(var->variableName(), $3->c_str()));
// 	delete $3;
// 	delete var;
//     }
//     else {
// 	LOGGER(ibis::gVerbose >= 0)
// 	    << "whereParser.yy: rule mathExpr != 'string literal' is a "
// 	    "kludge for Name != 'string literal'.  The mathExpr on the "
// 	    "left can only be variable name, currently " << *me1;
// 	delete $3;
// 	delete me1;
// 	throw "The rule on line 419 in whereParser.yy expects a simple "
// 	    "variable name on the left-hand side";
//     }
// }
;

compRange3:
mathExpr LTOP mathExpr LTOP mathExpr {
    ibis::math::term *me3 = static_cast<ibis::math::term*>($5);
    ibis::math::term *me2 = static_cast<ibis::math::term*>($3);
    ibis::math::term *me1 = static_cast<ibis::math::term*>($1);
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *me1 << " < "
	<< *me2 << " < " << *me3;
#endif
    $$ = new ibis::compRange(me1, ibis::qExpr::OP_LT, me2,
			     ibis::qExpr::OP_LT, me3);
}
| mathExpr LTOP mathExpr LEOP mathExpr {
    ibis::math::term *me3 = static_cast<ibis::math::term*>($5);
    ibis::math::term *me2 = static_cast<ibis::math::term*>($3);
    ibis::math::term *me1 = static_cast<ibis::math::term*>($1);
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *me1 << " < "
	<< *me2 << " <= " << *me3;
#endif
    $$ = new ibis::compRange(me1, ibis::qExpr::OP_LT, me2,
			     ibis::qExpr::OP_LE, me3);
}
| mathExpr LEOP mathExpr LTOP mathExpr {
    ibis::math::term *me3 = static_cast<ibis::math::term*>($5);
    ibis::math::term *me2 = static_cast<ibis::math::term*>($3);
    ibis::math::term *me1 = static_cast<ibis::math::term*>($1);
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *me1 << " <= "
	<< *me2 << " < " << *me3;
#endif
    $$ = new ibis::compRange(me1, ibis::qExpr::OP_LE, me2,
			     ibis::qExpr::OP_LT, me3);
}
| mathExpr LEOP mathExpr LEOP mathExpr {
    ibis::math::term *me3 = static_cast<ibis::math::term*>($5);
    ibis::math::term *me2 = static_cast<ibis::math::term*>($3);
    ibis::math::term *me1 = static_cast<ibis::math::term*>($1);
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *me1 << " <= "
	<< *me2 << " <= " << *me3;
#endif
    $$ = new ibis::compRange(me1, ibis::qExpr::OP_LE, me2,
			     ibis::qExpr::OP_LE, me3);
}
| mathExpr GTOP mathExpr GTOP mathExpr {
    ibis::math::term *me3 = static_cast<ibis::math::term*>($5);
    ibis::math::term *me2 = static_cast<ibis::math::term*>($3);
    ibis::math::term *me1 = static_cast<ibis::math::term*>($1);
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *me1 << " > "
	<< *me2 << " > " << *me3;
#endif
    $$ = new ibis::compRange(me3, ibis::qExpr::OP_LT, me2,
			     ibis::qExpr::OP_LT, me1);
}
| mathExpr GTOP mathExpr GEOP mathExpr {
    ibis::math::term *me3 = static_cast<ibis::math::term*>($5);
    ibis::math::term *me2 = static_cast<ibis::math::term*>($3);
    ibis::math::term *me1 = static_cast<ibis::math::term*>($1);
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *me1 << " > "
	<< *me2 << " >= " << *me3;
#endif
    $$ = new ibis::compRange(me3, ibis::qExpr::OP_LE, me2,
			     ibis::qExpr::OP_LT, me1);
}
| mathExpr GEOP mathExpr GTOP mathExpr {
    ibis::math::term *me3 = static_cast<ibis::math::term*>($5);
    ibis::math::term *me2 = static_cast<ibis::math::term*>($3);
    ibis::math::term *me1 = static_cast<ibis::math::term*>($1);
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *me1 << " >= "
	<< *me2 << " > " << *me3;
#endif
    $$ = new ibis::compRange(me3, ibis::qExpr::OP_LT, me2,
			     ibis::qExpr::OP_LE, me1);
}
| mathExpr GEOP mathExpr GEOP mathExpr {
    ibis::math::term *me3 = static_cast<ibis::math::term*>($5);
    ibis::math::term *me2 = static_cast<ibis::math::term*>($3);
    ibis::math::term *me1 = static_cast<ibis::math::term*>($1);
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *me1 << " >= "
	<< *me2 << " >= " << *me3;
#endif
    $$ = new ibis::compRange(me3, ibis::qExpr::OP_LE, me2,
			     ibis::qExpr::OP_LE, me1);
}
| mathExpr BETWEENOP mathExpr ANDOP mathExpr {
    ibis::math::term *me3 = static_cast<ibis::math::term*>($5);
    ibis::math::term *me2 = static_cast<ibis::math::term*>($3);
    ibis::math::term *me1 = static_cast<ibis::math::term*>($1);
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *me1 << " BETWEEN "
	<< *me2 << " AND " << *me3;
#endif
    $$ = new ibis::compRange(me2, ibis::qExpr::OP_LE, me1,
			     ibis::qExpr::OP_LE, me3);
}
;

mathExpr:
mathExpr ADDOP mathExpr {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *$1
	<< " + " << *$3;
#endif
    ibis::math::bediener *opr =
	new ibis::math::bediener(ibis::math::PLUS);
    opr->setRight($3);
    opr->setLeft($1);
    $$ = static_cast<ibis::qExpr*>(opr);
}
| mathExpr MINUSOP mathExpr {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *$1
	<< " - " << *$3;
#endif
    ibis::math::bediener *opr =
	new ibis::math::bediener(ibis::math::MINUS);
    opr->setRight($3);
    opr->setLeft($1);
    $$ = static_cast<ibis::qExpr*>(opr);
}
| mathExpr MULTOP mathExpr {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *$1
	<< " * " << *$3;
#endif
    ibis::math::bediener *opr =
	new ibis::math::bediener(ibis::math::MULTIPLY);
    opr->setRight($3);
    opr->setLeft($1);
    $$ = static_cast<ibis::qExpr*>(opr);
}
| mathExpr DIVOP mathExpr {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *$1
	<< " / " << *$3;
#endif
    ibis::math::bediener *opr =
	new ibis::math::bediener(ibis::math::DIVIDE);
    opr->setRight($3);
    opr->setLeft($1);
    $$ = static_cast<ibis::qExpr*>(opr);
}
| mathExpr REMOP mathExpr {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *$1
	<< " % " << *$3;
#endif
    ibis::math::bediener *opr =
	new ibis::math::bediener(ibis::math::REMAINDER);
    opr->setRight($3);
    opr->setLeft($1);
    $$ = static_cast<ibis::qExpr*>(opr);
}
| mathExpr EXPOP mathExpr {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *$1
	<< " ^ " << *$3;
#endif
    ibis::math::bediener *opr =
	new ibis::math::bediener(ibis::math::POWER);
    opr->setRight($3);
    opr->setLeft($1);
    $$ = static_cast<ibis::qExpr*>(opr);
}
| mathExpr BITANDOP mathExpr {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *$1
	<< " & " << *$3;
#endif
    ibis::math::bediener *opr =
	new ibis::math::bediener(ibis::math::BITAND);
    opr->setRight($3);
    opr->setLeft($1);
    $$ = static_cast<ibis::qExpr*>(opr);
}
| mathExpr BITOROP mathExpr {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *$1
	<< " | " << *$3;
#endif
    ibis::math::bediener *opr =
	new ibis::math::bediener(ibis::math::BITOR);
    opr->setRight($3);
    opr->setLeft($1);
    $$ = static_cast<ibis::qExpr*>(opr);
}
| NOUNSTR '(' mathExpr ')' %prec INOP {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *$1 << "("
	<< *$3 << ")";
#endif
    ibis::math::stdFunction1 *fun =
	new ibis::math::stdFunction1($1->c_str());
    delete $1;
    fun->setLeft($3);
    $$ = static_cast<ibis::qExpr*>(fun);
}
| NOUNSTR '(' mathExpr ',' mathExpr ')' %prec INOP {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *$1 << "("
	<< *$3 << ", " << *$5 << ")";
#endif
    ibis::math::stdFunction2 *fun =
	new ibis::math::stdFunction2($1->c_str());
    fun->setRight($5);
    fun->setLeft($3);
    $$ = static_cast<ibis::qExpr*>(fun);
    delete $1;
}
| FROM_UNIXTIME_LOCAL '(' mathExpr ',' STRLIT ')' {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- FROM_UNIXTIME_LOCAL("
	<< *$3 << ", " << *$5 << ")";
#endif
    ibis::math::fromUnixTime fut($5->c_str());
    ibis::math::customFunction1 *fun =
	new ibis::math::customFunction1(fut);
    fun->setLeft($3);
    $$ = static_cast<ibis::qExpr*>(fun);
    delete $5;
}
| FROM_UNIXTIME_GMT '(' mathExpr ',' STRLIT ')' {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- FROM_UNIXTIME_GMT("
	<< *$3 << ", " << *$5 << ")";
#endif

    ibis::math::fromUnixTime fut($5->c_str(), "GMT");
    ibis::math::customFunction1 *fun =
	new ibis::math::customFunction1(fut);
    fun->setLeft($3);
    $$ = static_cast<ibis::qExpr*>(fun);
    delete $5;
}
| ISO_TO_UNIXTIME_LOCAL '(' mathExpr ')' {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- ISO_TO_UNIXTIME_LOCAL("
	<< *$3 << ")";
#endif

    ibis::math::toUnixTime fut;
    ibis::math::customFunction1 *fun =
	new ibis::math::customFunction1(fut);
    fun->setLeft($3);
    $$ = static_cast<ibis::qExpr*>(fun);
}
| ISO_TO_UNIXTIME_GMT '(' mathExpr ')' {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- ISO_TO_UNIXTIME_GMT("
	<< *$3 << ")";
#endif

    ibis::math::toUnixTime fut("GMT0");
    ibis::math::customFunction1 *fun =
	new ibis::math::customFunction1(fut);
    fun->setLeft($3);
    $$ = static_cast<ibis::qExpr*>(fun);
}
| TO_UNIXTIME_LOCAL '(' STRLIT ',' STRLIT ')' {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- TO_UNIXTIME_LOCAL("
	<< *$3 << ", " << *$5  << ")";
#endif
#if defined(HAVE_STRPTIME)
    struct tm mytm;
    memset(&mytm, 0, sizeof(mytm));
    const char *ret = strptime($3->c_str(), $5->c_str(), &mytm);
    if (ret != 0) {
        // A negative value for tm_isdst causes mktime() to attempt to
        // determine whether Daylight Saving Time is in effect for the
        // specified time.
        mytm.tm_isdst = -1;
        if (mytm.tm_mday == 0) {
            // This can happen if we are using a format without '%d'
            // e.g. "%Y%m" as tm_mday is day of the month (in the range 1
            // through 31).
            mytm.tm_mday = 1;
        }
        $$ = new ibis::math::number(mktime(&mytm));
    }
    delete $3;
    delete $5;

    if (ret == 0) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- " << __FILE__ << ':' << __LINE__
            << " failed to parse \"" << *$3 << "\" using format string \""
            << *$5 << "\", errno = " << errno;
        throw "Failed to parse string value in TO_UNIXTIME_LOCAL";
    }
#else
    LOGGER(ibis::gVerbose >= 0)
        << "Warning -- " << __FILE__ << ':' << __LINE__
        << " failed to parse \"" << *$3 << "\" using format string \""
        << *$5 << "\" because there is no strptime";
    throw "No strptime to parse string value in TO_UNIXTIME_LOCAL";
#endif
}
| TO_UNIXTIME_GMT '(' STRLIT ',' STRLIT ')' {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- TO_UNIXTIME_GMT("
	<< *$3 << ", " << *$5  << ")";
#endif
#if defined(HAVE_STRPTIME)
    struct tm mytm;
    memset(&mytm, 0, sizeof(mytm));
    const char *ret = strptime($3->c_str(), $5->c_str(), &mytm);
    if (ret != 0) {
        if (mytm.tm_mday == 0) {
            // This can happen if we are using a format without '%d'
            // e.g. "%Y%m" as tm_mday is day of the month (in the range 1
            // through 31).
            mytm.tm_mday = 1;
        }
        $$ = new ibis::math::number(timegm(&mytm));
    }
    delete $3;
    delete $5;

    if (ret == 0) {
        LOGGER(ibis::gVerbose >= 0)
            << "Warning -- " << __FILE__ << ':' << __LINE__
            << " failed to parse \"" << *$3 << "\" using format string \""
            << *$5 << "\", errno = " << errno;
        throw "Failed to parse string value in TO_UNIXTIME_GMT";
    }
#else
    LOGGER(ibis::gVerbose >= 0)
        << "Warning -- " << __FILE__ << ':' << __LINE__
        << " failed to parse \"" << *$3 << "\" using format string \""
        << *$5 << "\" because there is no strptime";
    throw "No strptime to parse string value in TO_UNIXTIME_GMT";
#endif
}
| MINUSOP mathExpr %prec NOTOP {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- - " << *$2;
#endif
    ibis::math::bediener *opr =
	new ibis::math::bediener(ibis::math::NEGATE);
    opr->setRight($2);
    $$ = static_cast<ibis::qExpr*>(opr);
}
| ADDOP mathExpr %prec NOTOP {
    $$ = $2;
}
| '(' mathExpr ')' {
    $$ = $2;
}
| NOUNSTR {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " got a variable name " << *$1;
#endif
    ibis::math::variable *var =
	new ibis::math::variable($1->c_str());
    $$ = static_cast<ibis::qExpr*>(var);
    delete $1;
}
| NUMBER {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " got a number " << $1;
#endif
    ibis::math::number *num = new ibis::math::number($1);
    $$ = static_cast<ibis::qExpr*>(num);
}
;

START : qexpr END { /* pass qexpr to the driver */
    driver.expr_ = $1;
}
| qexpr ';' { /* pass qexpr to the driver */
    driver.expr_ = $1;
}
;

%%
void ibis::whereParser::error(const ibis::whereParser::location_type& l,
			      const std::string& m) {
    LOGGER(ibis::gVerbose >= 0)
	<< "Warning -- ibis::whereParser encountered " << m
	<< " at location " << l;
}
