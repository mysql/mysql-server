/* $Id$ -*- mode: c++ -*-

   Author: John Wu <John.Wu at acm.org>
   Lawrence Berkeley National Laboratory
   Copyright (c) 2009-2016 the Regents of the University of California
 */

%{ /* C++ declarations */
/** \file Defines the tokenlizer using Flex C++ template. */

#include "fromLexer.h"		// definition of YY_DECL
#include "fromParser.hh"	// class ibis::fromParser

typedef ibis::fromParser::token token;
typedef ibis::fromParser::token_type token_type;

#define yyterminate() return token::END
#define YY_USER_ACTION  yylloc->columns(yyleng);
%}

/* Flex declarations and options */
%option c++
%option stack
%option nounistd
 /*%option noyywrap*/
%option never-interactive
%option prefix="_fromLexer_"

/* regular expressions used to shorten the definitions 
*/
WS	[ \t\v\n]
SEP	[ \t\v\n,;]
NAME	[_a-zA-Z]((->)?[0-9A-Za-z_:.]+)*(\[[^\]]+\])?
NUMBER	([0-9]+[.]?|[0-9]*[.][0-9]+)([eE][-+]?[0-9]+)?

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
[oO][nN] {return token::ONOP;}
[jJ][oO][iI][nN] {return token::JOINOP;}
[uU][sS][iI][nN][gG] {return token::USINGOP;}
[bB][eE][tT][wW][eE][eE][nN] {return token::BETWEENOP;}
[aA][nN][dD] {return token::ANDOP;}

{NAME} { /* a name, unquoted string */
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
 	<< __FILE__ << ":" << __LINE__ << " got a name: " << yytext;
#endif
    yylval->stringVal = new std::string(yytext, yyleng);
    return token::NOUNSTR;
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
/* additional c++ code to complete the definition of class fromLexer */
ibis::fromLexer::fromLexer(std::istream* in, std::ostream* out)
    : ::_fLexer(in, out) {
#if defined(DEBUG) && DEBUG + 0 > 1
    yy_flex_debug = true;
#endif
}

ibis::fromLexer::~fromLexer() {
}

/* function needed by the super-class of ibis::fromLexer */
#ifdef yylex
#undef yylex
#endif

int ::_fLexer::yylex() {
    return 0;
} // ::_fLexer::yylex

int ::_fLexer::yywrap() {
    return 1;
} // ::_fLexer::yywrap
