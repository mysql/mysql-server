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

#include "Code_dml_row.hpp"
#include "Code_dml_column.hpp"

Plan_dml_row::~Plan_dml_row()
{
}

Plan_base*
Plan_dml_row::analyze(Ctx& ctx, Ctl& ctl)
{
    unsigned size = getSize();
    // analyze the columns
    for (unsigned i = 1; i <= size; i++) {
	Plan_dml_column* column = getColumn(i);
	column->analyze(ctx, ctl);
	if (! ctx.ok())
	    return 0;
    }
    // node was not replaced
    return this;
}

Exec_base*
Plan_dml_row::codegen(Ctx& ctx, Ctl& ctl)
{
    ctx_assert(false);
    return 0;
}

void
Plan_dml_row::print(Ctx& ctx)
{
    unsigned size = getSize();
    ctx.print(" [dml_row");
    for (unsigned i = 1; i <= size; i++) {
	Plan_base* a = m_columnList[i];
	a == 0 ? ctx.print(" -") : a->print(ctx);
    }
    ctx.print("]");
}
