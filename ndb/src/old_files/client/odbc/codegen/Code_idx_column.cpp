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

#include <NdbApi.hpp>
#include <common/StmtArea.hpp>
#include "Code_idx_column.hpp"
#include "Code_expr_conv.hpp"
#include "Code_root.hpp"

// Plan_idx_column

Plan_idx_column::~Plan_idx_column()
{
}

Plan_base*
Plan_idx_column::analyze(Ctx& ctx, Ctl& ctl)
{
    analyzeColumn(ctx, ctl);
    if (! ctx.ok())
	return 0;
    return this;
}

Exec_base*
Plan_idx_column::codegen(Ctx& ctx, Ctl& ctl)
{
    ctx_assert(false);
    return 0;
}

void
Plan_idx_column::print(Ctx& ctx)
{
    ctx.print(" [idx_column %s]", getPrintName());
}
