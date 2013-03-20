/* Copyright (C) 2008 MySQL AB, 2009 Sun Microsystems, Inc.
    All rights reserved. Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#include "DbaccProxy.hpp"
#include "Dbacc.hpp"

DbaccProxy::DbaccProxy(Block_context& ctx) :
  LocalProxy(DBACC, ctx)
{
}

DbaccProxy::~DbaccProxy()
{
}

SimulatedBlock*
DbaccProxy::newWorker(Uint32 instanceNo)
{
  return new Dbacc(m_ctx, instanceNo);
}

BLOCK_FUNCTIONS(DbaccProxy)
