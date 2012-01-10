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

#ifndef TRPMAN_H
#define TRPMAN_H

#include <pc.hpp>
#include <SimulatedBlock.hpp>
#include <LocalProxy.hpp>

class Trpman : public SimulatedBlock
{
public:
  Trpman(Block_context& ctx, Uint32 instanceNumber = 0);
  virtual ~Trpman();
  BLOCK_DEFINES(Trpman);

  void execCLOSE_COMREQ(Signal *signal);
  void execCLOSE_COMCONF(Signal * signal);
  void execOPEN_COMORD(Signal *signal);
  void execENABLE_COMREQ(Signal *signal);
  void execDISCONNECT_REP(Signal *signal);
  void execCONNECT_REP(Signal *signal);
  void execROUTE_ORD(Signal* signal);

  void execDBINFO_SCANREQ(Signal*);

  void execNDB_TAMPER(Signal*);
  void execDUMP_STATE_ORD(Signal*);
protected:

};

class TrpmanProxy : public LocalProxy
{
public:
  TrpmanProxy(Block_context& ctx);
  virtual ~TrpmanProxy();
  BLOCK_DEFINES(TrpmanProxy);

  void execCLOSE_COMREQ(Signal *signal);
  void execOPEN_COMORD(Signal *signal);
  void execENABLE_COMREQ(Signal *signal);
  void execROUTE_ORD(Signal* signal);

  void execNDB_TAMPER(Signal*);
  void execDUMP_STATE_ORD(Signal*);
protected:
  virtual SimulatedBlock* newWorker(Uint32 instanceNo);
};
#endif
