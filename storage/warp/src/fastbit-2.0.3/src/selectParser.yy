/* $Id$ -*- mode: c++ -*- */
// Author: John Wu <John.Wu at acm.org>
//      Lawrence Berkeley National Laboratory
// Copyright (c) 2007-2016 the Regents of the University of California

%code top {
/** \file Defines the parser for the select clause accepted by FastBit
    IBIS.  The definitions are processed through bison.
*/
#include <iostream>
}
%code requires {
#include "selectClause.h"	// class selectClause
}

/* bison declarations */
%require "2.7"
%debug
%error-verbose
 /*%start START*/
%defines
%skeleton "lalr1.cc"
%define api.namespace {ibis}
%define parser_class_name {selectParser}
%locations
     /*%expect 1*/
%initial-action
{ // initialize location object
    @$.begin.filename = @$.end.filename = &(driver.clause_);
};

%parse-param {class ibis::selectClause& driver}

%union {
    int			integerVal;
    double		doubleVal;
    std::string 	*stringVal;
    ibis::math::term	*selectNode;
};

%token              END       0 "end of input"
%token <integerVal> ASOP	"as"
%token <integerVal> BITOROP	"|"
%token <integerVal> BITANDOP	"&"
%token <integerVal> ADDOP	"+"
%token <integerVal> MINUSOP	"-"
%token <integerVal> MULTOP	"*"
%token <integerVal> DIVOP	"/"
%token <integerVal> REMOP	"%"
%token <integerVal> EXPOP	"**"
%token <doubleVal>  NUMBER	"numerical value"
%token <stringVal>  NOUNSTR	"name"
%token <stringVal>  STRLIT	"string literal"
%token <integerVal> FORMAT_UNIXTIME_GMT   "FORMAT_UNIXTIME_GMT"
%token <integerVal> FORMAT_UNIXTIME_LOCAL "FORMAT_UNIXTIME_LOCAL"

%nonassoc ASOP
%left BITOROP
%left BITANDOP
%left ADDOP MINUSOP
%left MULTOP DIVOP REMOP
%right EXPOP

%type <selectNode> mathExpr

%destructor { delete $$; } STRLIT
%destructor { delete $$; } NOUNSTR
%destructor { delete $$; } mathExpr

%{
#include "selectLexer.h"

#undef yylex
#define yylex driver.lexer->lex
%}

%% /* Grammar rules */
slist: sterm | sterm slist;
sterm: mathExpr ',' {
    driver.addTerm($1, 0);
}
| mathExpr END {
    driver.addTerm($1, 0);
}
| mathExpr NOUNSTR ',' {
    driver.addTerm($1, $2);
    delete $2;
}
| mathExpr NOUNSTR END {
    driver.addTerm($1, $2);
    delete $2;
}
| mathExpr ASOP NOUNSTR ',' {
    driver.addTerm($1, $3);
    delete $3;
}
| mathExpr ASOP NOUNSTR END {
    driver.addTerm($1, $3);
    delete $3;
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
    $$ = opr;
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
    $$ = opr;
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
    $$ = opr;
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
    $$ = opr;
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
    $$ = opr;
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
    $$ = opr;
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
    $$ = opr;
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
    $$ = opr;
}
| NOUNSTR '(' MULTOP ')' {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *$1 << "(*)";
#endif
    ibis::math::term *fun = 0;
    if (stricmp($1->c_str(), "count") == 0) { // aggregation count
	ibis::math::variable *var = new ibis::math::variable("*");
	fun = driver.addAgregado(ibis::selectClause::CNT, var);
    }
    else {
	LOGGER(ibis::gVerbose >= 0)
	    << "Warning -- only operator COUNT supports * as the argument, "
	    "but received " << *$1;
	throw "invalid use of (*)";
    }
    delete $1;
    $$ = fun;
}
| NOUNSTR '(' mathExpr ')' {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *$1 << "("
	<< *$3 << ")";
#endif
    ibis::math::term *fun = 0;
    if (stricmp($1->c_str(), "count") == 0) { // aggregation count
	delete $3; // drop the expression, replace it with "*"
	ibis::math::variable *var = new ibis::math::variable("*");
	fun = driver.addAgregado(ibis::selectClause::CNT, var);
    }
    else if (stricmp($1->c_str(), "max") == 0) { // aggregation max
	fun = driver.addAgregado(ibis::selectClause::MAX, $3);
    }
    else if (stricmp($1->c_str(), "min") == 0) { // aggregation min
	fun = driver.addAgregado(ibis::selectClause::MIN, $3);
    }
    else if (stricmp($1->c_str(), "sum") == 0) { // aggregation sum
	fun = driver.addAgregado(ibis::selectClause::SUM, $3);
    }
    else if (stricmp($1->c_str(), "median") == 0) { // aggregation median
	fun = driver.addAgregado(ibis::selectClause::MEDIAN, $3);
    }
    else if (stricmp($1->c_str(), "countd") == 0 ||
	     stricmp($1->c_str(), "countdistinct") == 0) {
	// count distinct values
	fun = driver.addAgregado(ibis::selectClause::DISTINCT, $3);
    }
    else if (stricmp($1->c_str(), "concat") == 0 ||
	     stricmp($1->c_str(), "group_concat") == 0) {
	// concatenate all values as ASCII strings
	fun = driver.addAgregado(ibis::selectClause::CONCAT, $3);
    }
    else if (stricmp($1->c_str(), "avg") == 0) { // aggregation avg
	ibis::math::term *numer =
	    driver.addAgregado(ibis::selectClause::SUM, $3);
	ibis::math::variable *var = new ibis::math::variable("*");
	ibis::math::term *denom =
	    driver.addAgregado(ibis::selectClause::CNT, var);
	ibis::math::bediener *opr =
	    new ibis::math::bediener(ibis::math::DIVIDE);
	opr->setRight(denom);
	opr->setLeft(numer);
	fun = opr;
    }
    else if (stricmp($1->c_str(), "varp") == 0 ||
	     stricmp($1->c_str(), "varpop") == 0) {
	// population variance is computed as
	// fabs(sum (x^2) / count(*) - (sum (x) / count(*))^2)
	ibis::math::term *x = $3;
	ibis::math::number *two = new ibis::math::number(2.0);
	ibis::math::variable *star = new ibis::math::variable("*");
	ibis::math::term *t11 = new ibis::math::bediener(ibis::math::POWER);
	t11->setLeft(x);
	t11->setRight(two);
	t11 = driver.addAgregado(ibis::selectClause::SUM, t11);
	ibis::math::term *t12 =
	    driver.addAgregado(ibis::selectClause::CNT, star);
	ibis::math::term *t13 = new ibis::math::bediener(ibis::math::DIVIDE);
	t13->setLeft(t11);
	t13->setRight(t12);
	ibis::math::term *t21 =
	    driver.addAgregado(ibis::selectClause::SUM, x->dup());
	ibis::math::term *t23 = new ibis::math::bediener(ibis::math::DIVIDE);
	t23->setLeft(t21);
	t23->setRight(t12->dup());
	ibis::math::term *t24 = new ibis::math::bediener(ibis::math::POWER);
	t24->setLeft(t23);
	t24->setRight(two->dup());
        ibis::math::term *t0 = new ibis::math::bediener(ibis::math::MINUS);
	t0->setLeft(t13);
	t0->setRight(t24);
        fun = new ibis::math::stdFunction1("fabs");
        fun->setLeft(t0);
	//fun = driver.addAgregado(ibis::selectClause::VARPOP, $3);
    }
    else if (stricmp($1->c_str(), "var") == 0 ||
	     stricmp($1->c_str(), "varsamp") == 0 ||
	     stricmp($1->c_str(), "variance") == 0) {
	// sample variance is computed as
	// fabs((sum (x^2) / count(*) - (sum (x) / count(*))^2) * (count(*) / (count(*)-1)))
	ibis::math::term *x = $3;
	ibis::math::number *two = new ibis::math::number(2.0);
	ibis::math::variable *star = new ibis::math::variable("*");
	ibis::math::term *t11 = new ibis::math::bediener(ibis::math::POWER);
	t11->setLeft(x);
	t11->setRight(two);
	t11 = driver.addAgregado(ibis::selectClause::SUM, t11);
	ibis::math::term *t12 =
	    driver.addAgregado(ibis::selectClause::CNT, star);
	ibis::math::term *t13 = new ibis::math::bediener(ibis::math::DIVIDE);
	t13->setLeft(t11);
	t13->setRight(t12);
	ibis::math::term *t21 =
	    driver.addAgregado(ibis::selectClause::SUM, x->dup());
	ibis::math::term *t23 = new ibis::math::bediener(ibis::math::DIVIDE);
	t23->setLeft(t21);
	t23->setRight(t12->dup());
	ibis::math::term *t24 = new ibis::math::bediener(ibis::math::POWER);
	t24->setLeft(t23);
	t24->setRight(two->dup());
	ibis::math::term *t31 = new ibis::math::bediener(ibis::math::MINUS);
	t31->setLeft(t13);
	t31->setRight(t24);
	ibis::math::term *t32 = new ibis::math::bediener(ibis::math::MINUS);
	ibis::math::number *one = new ibis::math::number(1.0);
	t32->setLeft(t12->dup());
	t32->setRight(one);
	ibis::math::term *t33 = new ibis::math::bediener(ibis::math::DIVIDE);
	t33->setLeft(t12->dup());
	t33->setRight(t32);
        ibis::math::term *t0 = new ibis::math::bediener(ibis::math::MULTIPLY);
	t0->setLeft(t31);
	t0->setRight(t33);
        fun = new ibis::math::stdFunction1("fabs");
        fun->setLeft(t0);
	//fun = driver.addAgregado(ibis::selectClause::VARSAMP, $3);
    }
    else if (stricmp($1->c_str(), "stdevp") == 0 ||
	     stricmp($1->c_str(), "stdpop") == 0) {
	// population standard deviation is computed as
	// sqrt(fabs(sum (x^2) / count(*) - (sum (x) / count(*))^2))
	ibis::math::term *x = $3;
	ibis::math::number *two = new ibis::math::number(2.0);
	ibis::math::variable *star = new ibis::math::variable("*");
	ibis::math::term *t11 = new ibis::math::bediener(ibis::math::POWER);
	t11->setLeft(x);
	t11->setRight(two);
	t11 = driver.addAgregado(ibis::selectClause::SUM, t11);
	ibis::math::term *t12 =
	    driver.addAgregado(ibis::selectClause::CNT, star);
	ibis::math::term *t13 = new ibis::math::bediener(ibis::math::DIVIDE);
	t13->setLeft(t11);
	t13->setRight(t12);
	ibis::math::term *t21 =
	    driver.addAgregado(ibis::selectClause::SUM, x->dup());
	ibis::math::term *t23 = new ibis::math::bediener(ibis::math::DIVIDE);
	t23->setLeft(t21);
	t23->setRight(t12->dup());
	ibis::math::term *t24 = new ibis::math::bediener(ibis::math::POWER);
	t24->setLeft(t23);
	t24->setRight(two->dup());
	ibis::math::term *t31 = new ibis::math::bediener(ibis::math::MINUS);
	t31->setLeft(t13);
	t31->setRight(t24);
        ibis::math::term *t0 = new ibis::math::stdFunction1("fabs");
        t0->setLeft(t31);
	fun = new ibis::math::stdFunction1("sqrt");
	fun->setLeft(t0);
	//fun = driver.addAgregado(ibis::selectClause::STDPOP, $3);
    }
    else if (stricmp($1->c_str(), "std") == 0 ||
	     stricmp($1->c_str(), "stdev") == 0 ||
	     stricmp($1->c_str(), "stddev") == 0 ||
	     stricmp($1->c_str(), "stdsamp") == 0) {
	// sample standard deviation is computed as
	// sqrt(fabs(sum (x^2) / count(*) - (sum (x) / count(*))^2) * (count(*) / (count(*)-1))))
	ibis::math::term *x = $3;
	ibis::math::number *two = new ibis::math::number(2.0);
	ibis::math::variable *star = new ibis::math::variable("*");
	ibis::math::term *t11 = new ibis::math::bediener(ibis::math::POWER);
	t11->setLeft(x);
	t11->setRight(two);
	t11 = driver.addAgregado(ibis::selectClause::SUM, t11);
	ibis::math::term *t12 =
	    driver.addAgregado(ibis::selectClause::CNT, star);
	ibis::math::term *t13 = new ibis::math::bediener(ibis::math::DIVIDE);
	t13->setLeft(t11);
	t13->setRight(t12);
	ibis::math::term *t21 =
	    driver.addAgregado(ibis::selectClause::SUM, x->dup());
	ibis::math::term *t23 = new ibis::math::bediener(ibis::math::DIVIDE);
	t23->setLeft(t21);
	t23->setRight(t12->dup());
	ibis::math::term *t24 = new ibis::math::bediener(ibis::math::POWER);
	t24->setLeft(t23);
	t24->setRight(two->dup());
	ibis::math::term *t31 = new ibis::math::bediener(ibis::math::MINUS);
	t31->setLeft(t13);
	t31->setRight(t24);
	ibis::math::term *t32 = new ibis::math::bediener(ibis::math::MINUS);
	ibis::math::number *one = new ibis::math::number(1.0);
	t32->setLeft(t12->dup());
	t32->setRight(one);
	ibis::math::term *t33 = new ibis::math::bediener(ibis::math::DIVIDE);
	t33->setLeft(t12->dup());
	t33->setRight(t32);
	ibis::math::term *t34 = new ibis::math::bediener(ibis::math::MULTIPLY);
	t34->setLeft(t31);
	t34->setRight(t33);
        ibis::math::term *t0 = new ibis::math::stdFunction1("fabs");
        t0->setLeft(t34);
	fun = new ibis::math::stdFunction1("sqrt");
	fun->setLeft(t0);
	// fun = driver.addAgregado(ibis::selectClause::STDSAMP, $3);
    }
    else { // assume it is a standard math function
	fun = new ibis::math::stdFunction1($1->c_str());
	fun->setLeft($3);
    }
    delete $1;
    $$ = fun;
}
| FORMAT_UNIXTIME_GMT '(' mathExpr ',' NOUNSTR ')' {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- FORMAT_UNIXTIME_GMT("
	<< *$3 << ", " << *$5 << ")";
#endif
    ibis::math::formatUnixTime fut($5->c_str(), "GMT");
    ibis::math::stringFunction1 *fun = new ibis::math::stringFunction1(fut);
    fun->setLeft($3);
    $$ = fun;
    delete $5;
}
| FORMAT_UNIXTIME_GMT '(' mathExpr ',' STRLIT ')' {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- FORMAT_UNIXTIME_GMT("
	<< *$3 << ", " << *$5 << ")";
#endif
    ibis::math::formatUnixTime fut($5->c_str(), "GMT");
    ibis::math::stringFunction1 *fun = new ibis::math::stringFunction1(fut);
    fun->setLeft($3);
    $$ = fun;
    delete $5;
}
| FORMAT_UNIXTIME_LOCAL '(' mathExpr ',' NOUNSTR ')' {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- FORMAT_UNIXTIME_LOCAL("
	<< *$3 << ", " << *$5 << ")";
#endif
    ibis::math::formatUnixTime fut($5->c_str());
    ibis::math::stringFunction1 *fun = new ibis::math::stringFunction1(fut);
    fun->setLeft($3);
    $$ = fun;
    delete $5;
}
| FORMAT_UNIXTIME_LOCAL '(' mathExpr ',' STRLIT ')' {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- FORMAT_UNIXTIME_LOCAL("
	<< *$3 << ", " << *$5 << ")";
#endif
    ibis::math::formatUnixTime fut($5->c_str());
    ibis::math::stringFunction1 *fun = new ibis::math::stringFunction1(fut);
    fun->setLeft($3);
    $$ = fun;
    delete $5;
}
| NOUNSTR '(' mathExpr ',' mathExpr ')' {
    /* two-arugment math functions */
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- " << *$1 << "("
	<< *$3 << ", " << *$5 << ")";
#endif
    ibis::math::stdFunction2 *fun =
	new ibis::math::stdFunction2($1->c_str());
    fun->setRight($5);
    fun->setLeft($3);
    $$ = fun;
    delete $1;
}
| MINUSOP mathExpr %prec EXPOP {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " parsing -- - " << *$2;
#endif
    ibis::math::bediener *opr =
	new ibis::math::bediener(ibis::math::NEGATE);
    opr->setRight($2);
    $$ = opr;
}
| ADDOP mathExpr %prec EXPOP {
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
    $$ = new ibis::math::variable($1->c_str());
    delete $1;
}
| STRLIT {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " got a string literal " << *$1;
#endif
    $$ = new ibis::math::literal($1->c_str());
    delete $1;
}
| NUMBER {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ":" << __LINE__ << " got a number " << $1;
#endif
    $$ = new ibis::math::number($1);
}
;

%%
void ibis::selectParser::error(const ibis::selectParser::location_type& l,
			       const std::string& m) {
    LOGGER(ibis::gVerbose >= 0)
	<< "Warning -- ibis::selectParser encountered " << m << " at location "
	<< l;
}
