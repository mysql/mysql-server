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

#include "Code_query_repeat.hpp"
#include "Code_root.hpp"

// Plan_query_repeat

Plan_query_repeat::~Plan_query_repeat()
{
}

Plan_base*
Plan_query_repeat::analyze(Ctx& ctx, Ctl& ctl)
{
    return this;
}

Exec_base*
Plan_query_repeat::codegen(Ctx& ctx, Ctl& ctl)
{
    Exec_query_repeat* exec = new Exec_query_repeat(ctl.m_execRoot);
    ctl.m_execRoot->saveNode(exec);
    // SqlSpecs is empty
    const SqlSpecs sqlSpecs(0);
    Exec_query_repeat::Code& code = *new Exec_query_repeat::Code(sqlSpecs, m_forever, m_maxcount);
    exec->setCode(code);
    return exec;
}

void
Plan_query_repeat::print(Ctx& ctx)
{
    ctx.print(" [query_repeat");
    if (! m_forever)
	ctx.print(" %ld", (long)m_maxcount);
    ctx.print("]");
}

// Exec_query_repeat

Exec_query_repeat::Code::~Code()
{
}

Exec_query_repeat::Data::~Data()
{
}

Exec_query_repeat::~Exec_query_repeat()
{
}

void
Exec_query_repeat::alloc(Ctx& ctx, Ctl& ctl)
{
    const Code& code = getCode();
    // SqlRow is empty
    Data& data = *new Data(this, code.sqlSpecs());
    setData(data);
}

void
Exec_query_repeat::execImpl(Ctx& ctx, Ctl& ctl)
{
    Data& data = getData();
    data.m_count = 0;
}

bool
Exec_query_repeat::fetchImpl(Ctx& ctx, Ctl& ctl)
{
    const Code& code = getCode();
    Data& data = getData();
    // fetch until count is up
    if (code.m_forever || data.m_count < code.m_maxcount) {
	data.m_count++;
	return true;
    }
    return false;
}

void
Exec_query_repeat::close(Ctx& ctx)
{
}

void
Exec_query_repeat::print(Ctx& ctx)
{
    const Code& code = getCode();
    ctx.print(" [query_repeat");
    if (! code.m_forever)
	ctx.print(" %ld", (long)code.m_maxcount);
    ctx.print("]");
}
