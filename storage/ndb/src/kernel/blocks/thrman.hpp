/*
   Copyright (c) 2011, 2014, Oracle and/or its affiliates. All rights reserved.

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
#include <NdbGetRUsage.h>
#include <NdbTick.h>

#define JAM_FILE_ID 340

//#define DEBUG_CPU_USAGE 1
class Thrman : public SimulatedBlock
{
public:
  Thrman(Block_context& ctx, Uint32 instanceNumber = 0);
  virtual ~Thrman();
  BLOCK_DEFINES(Thrman);

  void execDBINFO_SCANREQ(Signal*);
  void execCONTINUEB(Signal*);
  void execGET_CPU_USAGE_REQ(Signal*);
  void execREAD_CONFIG_REQ(Signal*);
  void execSTTOR(Signal*);
protected:

private:
  /* Private methods */
  void sendSTTORRY(Signal*);
  void sendNextCONTINUEB(Signal*);
  void measure_cpu_usage(void);

  /* Private variables */
  struct ndb_rusage last_rusage;
  Uint32 current_cpu_load;

  NDB_TICKS prev_cpu_usage_check;

  static const Uint32 ZCONTINUEB_MEASURE_CPU_USAGE = 1;
  static const Uint32 default_cpu_load = 90;
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


#undef JAM_FILE_ID

#endif
