/* Copyright (C) 2008 MySQL AB

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

#ifndef NDB_RESTORE_PROXY_HPP
#define NDB_RESTORE_PROXY_HPP

#include <LocalProxy.hpp>

class RestoreProxy : public LocalProxy {
public:
  RestoreProxy(Block_context& ctx);
  virtual ~RestoreProxy();
  BLOCK_DEFINES(RestoreProxy);

protected:
  virtual SimulatedBlock* newWorker(Uint32 instanceNo);
};

#endif
