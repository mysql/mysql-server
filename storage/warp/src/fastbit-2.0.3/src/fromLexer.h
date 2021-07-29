// $Id$
// Author: John Wu <John.Wu at acm.org>
//      Lawrence Berkeley National Laboratory
// Copyright (c) 2009-2016 the Regents of the University of California
#ifndef IBIS_FROMLEXER_H
#define IBIS_FROMLEXER_H
/** \file
    Declares the name ibis::fromLexer.  Defines the tokenizer with two
    arguments to satisfy the reentrant parser defined in selectParser.yy.
 */
#ifndef YY_DECL
// the new lex function to satisfy the reentrant parser requirement
#define YY_DECL ibis::fromParser::token_type ibis::fromLexer::lex \
    (ibis::fromParser::semantic_type* yylval, \
     ibis::fromParser::location_type* yylloc)
#endif
#include "fromParser.hh"	// class fromParser

// rename yyFlexLexer to _fLexer
#undef yyFlexLexer
#define yyFlexLexer _fLexer
#include <FlexLexer.h>
//#undef yyFlexLexer

namespace ibis {
    /// Defines a new class with the desired lex function for C++ output of
    /// bison.
    ///
    /// @note All names must start with an alphabet or the underscore (_).
    /// @note This version of the lexer converts hexadecimal numbers to
    /// double precision floating-point numbers, which is not suitble for
    /// handling long integers.
    class fromLexer : public ::_fLexer {
    public:
	fromLexer(std::istream* in=0, std::ostream* out=0);
	virtual ~fromLexer();

	// The new lex function.  It carries the value of token and its type.
	// The value of the token is returned as the first argument and the
	// corresponding type is the return value of this function.
	virtual fromParser::token_type
	lex(fromParser::semantic_type*, fromParser::location_type*);

	void set_debug(bool);
    }; // ibis::fromLexer
} // namespace ibis
#endif
