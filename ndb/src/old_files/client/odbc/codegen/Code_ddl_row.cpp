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

#include "Code_ddl_row.hpp"
#include "Code_ddl_column.hpp"
#include "Code_ddl_constr.hpp"

Plan_ddl_row::~Plan_ddl_row()
{
}

Plan_base*
Plan_ddl_row::analyze(Ctx& ctx, Ctl& ctl)
{
    // analyze the columns
    for (unsigned i = 1, n = countColumn(); i <= n; i++) {
	Plan_ddl_column* column = getColumn(i);
	column->analyze(ctx, ctl);
	if (! ctx.ok())
	    return 0;
    }
    // node was not replaced
    return this;
}

Exec_base*
Plan_ddl_row::codegen(Ctx& ctx, Ctl& ctl)
{
    ctx_assert(false);
    return 0;
}

void
Plan_ddl_row::print(Ctx& ctx)
{
    ctx.print(" [ddl_row");
    for (unsigned i = 1, n = countColumn(); i <= n; i++) {
	Plan_base* a = m_columnList[i];
	printList(ctx, &a, 1);
    }
}
