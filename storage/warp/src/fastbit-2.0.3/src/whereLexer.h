// $Id$
// Author: John Wu <John.Wu at acm.org>
//      Lawrence Berkeley National Laboratory
// Copyright (c) 2007-2016 the Regents of the University of California
#ifndef IBIS_WHERELEXER_H
#define IBIS_WHERELEXER_H
/** \file
    Declares the name ibis::whereLexer.  Defines the tokenizer with two
    arguments to satisfy the reentrant parser defined in whereParser.yy.
 */
#ifndef YY_DECL
// the new lex function to satisfy the reentrant parser requirement
#define YY_DECL ibis::whereParser::token_type ibis::whereLexer::lex \
    (ibis::whereParser::semantic_type* yylval, \
     ibis::whereParser::location_type* yylloc)
#endif
#include "whereParser.hh"	// class whereParser

// rename yyFlexLexer to _wLexer
#undef yyFlexLexer
#define yyFlexLexer _wLexer
#include <FlexLexer.h>
//#undef yyFlexLexer

namespace ibis {
    /// Defines a new class with the desired lex function for C++ output of
    /// bison.
    ///
    /// @note This version of the lexer converts hexadecimal numbers to
    /// double precision floating-point numbers, which is not suitble for
    /// handling long integers.
    /// @note This version of the lexer does not distinguish between quoted
    /// strings and unquoted strings.  In cases where a string literal is
    /// needed such as for string matches, the evaluation engine will take
    /// one string as the column name and the other as a string literal.
    /// To ensure a single string is treated as a string literal, use the
    /// expression in the form of
    /// @code
    /// column_name IN ( string_literal )
    /// @endcode
    class whereLexer : public ::_wLexer {
    public:
	whereLexer(std::istream* in=0, std::ostream* out=0);
	virtual ~whereLexer();

	// The new lex function.  It carries the value of token and its type.
	// The value of the token is returned as the first argument and the
	// corresponding type is the return value of this function.
	virtual whereParser::token_type
	lex(whereParser::semantic_type*, whereParser::location_type*);

	void set_debug(bool);
    }; // ibis::whereLexer
} // namespace ibis
#endif
