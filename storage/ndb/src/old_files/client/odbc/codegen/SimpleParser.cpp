/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <common/common.hpp>
#include <NdbMutex.h>
#include <common/StmtArea.hpp>
#include <FlexLexer.h>
#include "SimpleParser.hpp"

SimpleParser::~SimpleParser()
{
}

#ifdef NDB_WIN32
static NdbMutex & parse_mutex = * NdbMutex_Create();
#else
static NdbMutex parse_mutex = NDB_MUTEX_INITIALIZER;
#endif

void
SimpleParser::yyparse()
{
    Ctx& ctx = this->ctx();
    NdbMutex_Lock(&parse_mutex);
    ctx_log2(("parse: %s", stmtArea().sqlText().c_str()));
#if YYDEBUG
    SimpleParser_yydebug = (m_ctx.logLevel() >= 5);
#endif
    SimpleParser_yyparse((void*)this);
    NdbMutex_Unlock(&parse_mutex);
}

void
SimpleParser::pushState(int sc)
{
    yy_push_state(sc);
    m_stacksize++;
}

void
SimpleParser::popState()
{
    ctx_assert(m_stacksize > 0);
    yy_pop_state();
    m_stacksize--;
}

void
SimpleParser::parseError(const char* msg)
{
    ctx().pushStatus(Sqlstate::_42000, Error::Gen, "%s at '%*s' position %u", msg, yyleng, yytext, m_parsePos - yyleng);
}

int
SimpleParser::LexerInput(char* buf, int max_size)
{
    const BaseString& text = stmtArea().sqlText();
    int n = 0;
    const char* const t = text.c_str();
    const unsigned m = text.length();
    while (n < max_size && m_textPos < m) {
	buf[n++] = t[m_textPos++];
	m_parsePos++;		// XXX simple hack
	break;
    }
    return n;
}

// XXX just a catch-all (scanner should match all input)
void
SimpleParser::LexerOutput(const char* buf, int size)
{
    if (! ctx().ok())
	return;
    const char* msg = "unrecognized input";
    ctx().pushStatus(Sqlstate::_42000, Error::Gen, "%s at '%*s' position %u", msg, size, buf, m_parsePos);
}

void
SimpleParser::LexerError(const char* msg)
{
    ctx().pushStatus(Sqlstate::_42000, Error::Gen, "%s at '%*s' position %u", msg, yyleng, yytext, m_parsePos);
}
