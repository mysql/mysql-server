// $Id$
// Author: John Wu <John.Wu at acm.org>
//      Lawrence Berkeley National Laboratory
// Copyright (c) 2007-2016 the Regents of the University of California
#ifndef IBIS_SELECTLEXER_H
#define IBIS_SELECTLEXER_H
/** \file
    Declares the name ibis::selectLexer.  Defines the tokenizer with two
    arguments to satisfy the reentrant parser defined in selectParser.yy.
 */
#ifndef YY_DECL
// the new lex function to satisfy the reentrant parser requirement
#define YY_DECL ibis::selectParser::token_type ibis::selectLexer::lex \
    (ibis::selectParser::semantic_type* yylval, \
     ibis::selectParser::location_type* yylloc)
#endif
#include "selectParser.hh"	// class selectParser

// rename yyFlexLexer to _sLexer
#undef yyFlexLexer
#define yyFlexLexer _sLexer
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
    class selectLexer : public ::_sLexer {
    public:
	selectLexer(std::istream* in=0, std::ostream* out=0);
	virtual ~selectLexer();

	// The new lex function.  It carries the value of token and its type.
	// The value of the token is returned as the first argument and the
	// corresponding type is the return value of this function.
	virtual selectParser::token_type
	lex(selectParser::semantic_type*, selectParser::location_type*);

	void set_debug(bool);
    }; // ibis::selectLexer
} // namespace ibis
#endif
