/* $Id$ -*- mode: c++ -*-

   Author: John Wu <John.Wu at acm.org>
   Lawrence Berkeley National Laboratory
   Copyright (c) 2007-2016 the Regents of the University of California
 */

%{ /* C++ declarations */
/** \file Defines the tokenlizer using Flex C++ template. */

#include "selectLexer.h"	// definition of YY_DECL
#include "selectParser.hh"	// class ibis::selectParser

typedef ibis::selectParser::token token;
typedef ibis::selectParser::token_type token_type;

#define yyterminate() return token::END
#define YY_USER_ACTION  yylloc->columns(yyleng);
%}

/* Flex declarations and options */
%option c++
%option stack
%option nounistd
 /*%option noyywrap*/
%option never-interactive
%option prefix="_selectLexer_"

/* regular expressions used to shorten the definitions 
*/
WS	[ \t\v\n]
SEP	[ \t\v\n,;]
NAME	[_a-zA-Z]((->)?[0-9A-Za-z_:.]+)*(\[[^\]]+\])?
NUMBER	([0-9]+[.]?|[0-9]*[.][0-9]+)([eE][-+]?[0-9]+)?
QUOTED	\"([^\"\\]*(\\.[^\"\\]*)*)\"|\'([^\'\\]*(\\.[^\'\\]*)*)\'|\`([^\'\\]*(\\.[^\'\\]*)*)\'

%%
%{
    yylloc->step();
%}
		   /* section defining the tokens */
"|"   {return token::BITOROP;}
"&"   {return token::BITANDOP;}
"-"   {return token::MINUSOP;}
"^"   {return token::EXPOP;}
"+"   {return token::ADDOP;}
"*"   {return token::MULTOP;}
"/"   {return token::DIVOP;}
"%"   {return token::REMOP;}
"**"  {return token::EXPOP;}
[aA][sS] {return token::ASOP;}
[fF][oO][rR][mM][aA][tT]_[uU][nN][iI][xX][tT][iI][mM][eE]_[gG][mM][tT] {return token::FORMAT_UNIXTIME_GMT;}
[fF][oO][rR][mM][aA][tT]_[uU][nN][iI][xX][tT][iI][mM][eE]_[lL][oO][cC][aA][lL] {return token::FORMAT_UNIXTIME_LOCAL;}
[fF][oO][rR][mM][aA][tT]_[uU][nN][iI][xX][tT][iI][mM][eE] {return token::FORMAT_UNIXTIME_LOCAL;}

{NAME} { /* a name, unquoted string */
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
 	<< __FILE__ << ":" << __LINE__ << " got a name: " << yytext;
#endif
    yylval->stringVal = new std::string(yytext, yyleng);
    return token::NOUNSTR;
}

{QUOTED} { /* a quoted string */
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
 	<< __FILE__ << ":" << __LINE__ << " got a quoted string: " << yytext;
#endif
    yylval->stringVal = new std::string(yytext+1, yyleng-2);
    return token::STRLIT;
}

{NUMBER} { /* a number (let parser deal with the sign) */
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
 	<< __FILE__ << ":" << __LINE__ << " got a number: " << yytext;
#endif
    yylval->doubleVal = atof(yytext);
    return token::NUMBER;
}

0[xX][0-9a-fA-F]+ { /* a hexidacimal string */
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
 	<< __FILE__ << ":" << __LINE__ << " got a hexadecimal number: " << yytext;
#endif
    (void) sscanf(yytext+2, "%x", &(yylval->integerVal));
    return token::NUMBER;
}

{WS}+ ; /* do nothing for blank space */

. { /* pass the character to the parser as a token */
    return static_cast<token_type>(*yytext);
}

%%
/* additional c++ code to complete the definition of class selectLexer */
ibis::selectLexer::selectLexer(std::istream* in, std::ostream* out)
    : ::_sLexer(in, out) {
#if defined(DEBUG) && DEBUG + 0 > 1
    yy_flex_debug = true;
#endif
}

ibis::selectLexer::~selectLexer() {
}

/* function needed by the super-class of ibis::selectLexer */
#ifdef yylex
#undef yylex
#endif

int ::_sLexer::yylex() {
    return 0;
} // ::_sLexer::yylex

int ::_sLexer::yywrap() {
    return 1;
} // ::_sLexer::yywrap
