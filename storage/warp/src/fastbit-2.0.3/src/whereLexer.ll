/* $Id$ -*- mode: c++ -*-

   Author: John Wu <John.Wu at acm.org>
   Lawrence Berkeley National Laboratory
   Copyright (c) 2007-2016 the Regents of the University of California
 */

%{ /* C++ declarations */
/** \file Defines the tokenlizer using Flex C++ template. */

#include "whereLexer.h"		// definition of YY_DECL
#include "whereParser.hh"	// class ibis::wherParser

typedef ibis::whereParser::token token;
typedef ibis::whereParser::token_type token_type;

#define yyterminate() return token::END
#define YY_USER_ACTION  yylloc->columns(yyleng);
%}

/* Flex declarations and options */
%option c++
%option stack
%option nounistd
 /*%option noyywrap*/
%option never-interactive
%option prefix="_whereLexer_"
 /*%option case-insensitive*/

/* regular expressions used to shorten the definitions 

the following definition of a name is somehow bad
DIGIT	[0-9]
ALPHA	[_a-zA-Z]
NAME	{ALPHA}((->)?[{DIGIT}{ALPHA}:.]+)*(\[[^\]]+\])?

this version works -- guess I can not use {ALPHA}
NAME	[_a-zA-Z]((->)?[_a-zA-Z0-9.:\[\]]+)*
*/
WS	[ \t\v\n\r]
SEP	[ \t\v\n\r,;]
STRSEP	{WS}*[,;]{WS}*
UNSIGNED	([0-9]+[.]?|[0-9]*[.][0-9]+)([eE][-+]?[0-9]+)?
NUMBER	[-+]?([0-9]+[.]?|[0-9]*[.][0-9]+)([eE][-+]?[0-9]+)?
QUOTED	\"([^\"\\]*(\\.[^\"\\]*)*)\"|\'([^\'\\]*(\\.[^\'\\]*)*)\'|\`([^\'\\]*(\\.[^\'\\]*)*)\'
NAME	[_a-zA-Z]((->)?[0-9A-Za-z_:.]+)*(\[[^\]]+\])?

%%
%{
    yylloc->step();
%}
		   /* section defining the tokens */
"!"   {return token::NOTOP;}
"~"   {return token::NOTOP;}
"<="  {return token::LEOP;}
"!="  {return token::NEQOP;}
"<>"  {return token::NEQOP;}
"<"   {return token::LTOP;}
">="  {return token::GEOP;}
">"   {return token::GTOP;}
"="   {return token::EQOP;}
"=="  {return token::EQOP;}
"||"  {return token::OROP;}
"&&"  {return token::ANDOP;}
"&!"  {return token::ANDNOTOP;}
"&~"  {return token::ANDNOTOP;}
"|"   {return token::BITOROP;}
"&"   {return token::BITANDOP;}
"-"   {return token::MINUSOP;}
"+"   {return token::ADDOP;}
"*"   {return token::MULTOP;}
"/"   {return token::DIVOP;}
"%"   {return token::REMOP;}
"^"   {return token::EXPOP;}
"**"  {return token::EXPOP;}
[nN][oO][tT] {return token::NOTOP;}
[nN][uU][lL][lL] {return token::NULLOP;}
[iI][nN] {return token::INOP;}
[oO][rR] {return token::OROP;}
[aA][nN][dD] {return token::ANDOP;}
[aA][nN][yY] {return token::ANYOP;}
[xX][oO][rR] {return token::XOROP;}
[lL][iI][kK][eE] {return token::LIKEOP;}
[mM][iI][nN][uU][sS] {return token::ANDNOTOP;}
[aA][nN][dD][nN][oO][tT] {return token::ANDNOTOP;}
[eE][xX][iI][sS][tT][sS] {return token::EXISTSOP;}
[bB][eE][tT][wW][eE][eE][nN] {return token::BETWEENOP;}
[cC][oO][nN][tT][aA][iI][nN][sS] {return token::CONTAINSOP;}
[iI][sS][oO]_[tT][oO]_[uU][nN][iI][xX][tT][iI][mM][eE]_[gG][mM][tT] {return token::ISO_TO_UNIXTIME_GMT;}
[iI][sS][oO]_[tT][oO]_[uU][nN][iI][xX][tT][iI][mM][eE]_[lL][oO][cC][aA][lL] {return token::ISO_TO_UNIXTIME_LOCAL;}
[tT][oO]_[uU][nN][iI][xX][tT][iI][mM][eE]_[gG][mM][tT] {return token::TO_UNIXTIME_GMT;}
[tT][oO]_[uU][nN][iI][xX][tT][iI][mM][eE]_[lL][oO][cC][aA][lL] {return token::TO_UNIXTIME_LOCAL;}
[sS][tT][rR][pP][tT][iI][mM][eE] {return token::FROM_UNIXTIME_LOCAL;}
[fF][rR][oO][mM]_[uU][nN][iI][xX][tT][iI][mM][eE]_[gG][mM][tT] {return token::FROM_UNIXTIME_GMT;}
[fF][rR][oO][mM]_[uU][nN][iI][xX][tT][iI][mM][eE]_[lL][oO][cC][aA][lL] {return token::FROM_UNIXTIME_LOCAL;}

{NAME} { /* a name, unquoted string */
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
 	<< __FILE__ << ':' << __LINE__ << " got a name: " << yytext;
#endif
    yylval->stringVal = new std::string(yytext, yyleng);
    return token::NOUNSTR;
}

{UNSIGNED} { /* an integer or a floating-point number (without a sign) */
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
 	<< __FILE__ << ':' << __LINE__ << " got a floating-point number: " << yytext;
#endif
    yylval->doubleVal = atof(yytext);
    return token::NUMBER;
}

0[xX][0-9a-fA-F]+ { /* a hexidacimal string */
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
 	<< __FILE__ << ':' << __LINE__ << " got a hexadecimal integer: " << yytext;
#endif
    const char *tmp = yytext;
    int ierr = ibis::util::readUInt(yylval->uint64Val, tmp);
    if (ierr != 0) {
	throw "failed to parse a hexadecimal integer";
    }
    return token::UINT64;
}

[0-9]+[uU][lL]* {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ':' << __LINE__ << " got a unsigned integer: " << yytext;
#endif
    const char *tmp = yytext;
    int ierr = ibis::util::readUInt(yylval->uint64Val, tmp);
    if (ierr != 0)
	throw "failed to parse a unsigned integer";
    return token::UINT64;
}

[-+]?[0-9]+[lL][lL]? {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
	<< __FILE__ << ':' << __LINE__ << " got a 64-bit integer: " << yytext;
#endif
    const char *tmp = yytext;
    int ierr = ibis::util::readInt(yylval->int64Val, tmp);
    if (ierr != 0)
	throw "failed to parse a long integer";
    return token::INT64;
}

\({WS}*[-+]?[0-9]+[lL][lL]?({SEP}+[-+]?[0-9]+[lL]?[lL]?)*{WS}*\) {/*\(\)*/
    /* a series of long integers */ /*  */
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
 	<< __FILE__ << ':' << __LINE__ << " got a signed integer sequence: " << yytext;
#endif
    yylval->stringVal = new std::string(yytext+1, yyleng-2);
    return token::INTSEQ;
}

\({WS}*(0[xX][0-9a-fA-F]+|[0-9]+[uU][lL]?[lL]?)({SEP}+(0[xX][0-9a-fA-F]+|[0-9]+[uU][lL]?[lL]?))*{WS}*\) {
    /* a series of unsigned long integers */
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
 	<< __FILE__ << ':' << __LINE__ << " got a unsigned integer sequence: " << yytext;
#endif
    yylval->stringVal = new std::string(yytext+1, yyleng-2);
    return token::UINTSEQ;
}

{QUOTED} { /* a quoted string literal */
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
 	<< __FILE__ << ':' << __LINE__ << " got a quoted string: " << yytext;
#endif
    yylval->stringVal = new std::string(yytext+1, yyleng-2);
    return token::STRLIT;
}

\({WS}*{NUMBER}{SEP}+{NUMBER}({SEP}+{NUMBER})+{WS}*\) { /* a number series */
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
 	<< __FILE__ << ':' << __LINE__ << " got a number sequence: " << yytext;
#endif
    yylval->stringVal = new std::string(yytext+1, yyleng-2);
    return token::NUMSEQ;
}

\({WS}*({QUOTED}|{NAME}){STRSEP}({QUOTED}|{NAME})({STRSEP}({QUOTED}|{NAME}))+{WS}*\) {
#if defined(DEBUG) && DEBUG + 0 > 1
    LOGGER(ibis::gVerbose >= 0)
 	<< __FILE__ << ':' << __LINE__ << " got a string sequence: " << yytext;
#endif
    yylval->stringVal = new std::string(yytext+1, yyleng-2);
    return token::STRSEQ;
}

{WS}+ ; /* do nothing for blank space */

. { /* pass the character to the parser as a token */
    return static_cast<token_type>(*yytext);
}

%%
/* additional c++ code to complete the definition of class whereLexer */
ibis::whereLexer::whereLexer(std::istream* in, std::ostream* out)
    : ::_wLexer(in, out) {
#if defined(DEBUG) && DEBUG + 0 > 1
    yy_flex_debug = true;
#endif
}

ibis::whereLexer::~whereLexer() {
}

/* function needed by the super-class of ibis::whereLexer */
#ifdef yylex
#undef yylex
#endif

int ::_wLexer::yylex() {
    return 0;
} // ::_wLexer::yylex

int ::_wLexer::yywrap() {
    return 1;
} // ::_wLexer::yywrap
