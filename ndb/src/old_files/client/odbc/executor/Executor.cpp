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
#include <common/CodeTree.hpp>
#include <common/StmtArea.hpp>
#include <codegen/Code_root.hpp>
#include "Executor.hpp"

void
Executor::execute(Ctx& ctx)
{
    Exec_base::Ctl ctl(0);
    Exec_root* execRoot = static_cast<Exec_root*>(m_stmtArea.m_execTree);
    ctx_assert(execRoot != 0);
    rebind(ctx);
    if (! ctx.ok())
	return;
    execRoot->execute(ctx, ctl);
    if (! ctx.ok())
	return;
    ctx_log2(("Executor: execute done"));
}

void
Executor::fetch(Ctx& ctx)
{
    Exec_base::Ctl ctl(0);
    Exec_root* execRoot = static_cast<Exec_root*>(m_stmtArea.m_execTree);
    ctx_assert(execRoot != 0);
    rebind(ctx);
    if (! ctx.ok())
	return;
    execRoot->fetch(ctx, ctl);
    if (! ctx.ok())
	return;
    ctx_log2(("Executor: fetch done"));
}

void
Executor::rebind(Ctx& ctx)
{
    Exec_root* execRoot = static_cast<Exec_root*>(m_stmtArea.m_execTree);
    ctx_assert(execRoot != 0);
    DescArea& apd = m_stmtArea.descArea(Desc_usage_APD);
    DescArea& ard = m_stmtArea.descArea(Desc_usage_ARD);
    if (! apd.isBound() || ! ard.isBound()) {
	ctx_log2(("Executor: rebind required"));
	execRoot->bind(ctx);
	if (! ctx.ok())
	    return;
	apd.setBound(true);
	ard.setBound(true);
    }
}
