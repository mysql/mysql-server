/*
   Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef THRMAN_H
#define THRMAN_H

#include <SimulatedBlock.hpp>
#include <LocalProxy.hpp>

class Thrman : public SimulatedBlock
{
public:
  Thrman(Block_context& ctx, Uint32 instanceNumber = 0);
  virtual ~Thrman();
  BLOCK_DEFINES(Thrman);

  void execDBINFO_SCANREQ(Signal*);
protected:

};

class ThrmanProxy : public LocalProxy
{
public:
  ThrmanProxy(Block_context& ctx);
  virtual ~ThrmanProxy();
  BLOCK_DEFINES(ThrmanProxy);

protected:
  virtual SimulatedBlock* newWorker(Uint32 instanceNo);

};

#endif
