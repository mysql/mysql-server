/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef NDB_LOCAL_PROXY_HPP
#define NDB_LOCAL_PROXY_HPP

#include <pc.hpp>
#include <SimulatedBlock.hpp>
#include <signaldata/ReadConfig.hpp>

/*
 * Proxy blocks for MT LQH.
 *
 * The LQH proxy is the LQH block seen by other nodes and blocks,
 * unless by-passed for efficiency.  Real LQH instances (workers)
 * run behind it.
 *
 * There are also ACC,TUP,TUX,BACKUP,RESTORE proxies and workers.
 * All proxy classes are subclasses of LocalProxy.
 */

class LocalProxy : public SimulatedBlock {
public:
  LocalProxy(BlockNumber blockNumber, Block_context& ctx);
  virtual ~LocalProxy();
  BLOCK_DEFINES(LocalProxy);

protected:
  enum { MaxWorkers = MAX_NDBMT_WORKERS };
  Uint32 c_workers;
  Uint32 c_threads;
  SimulatedBlock* c_worker[MaxWorkers];

  virtual SimulatedBlock* newWorker(Uint32 instanceNo) = 0;

  // worker index to worker ref
  BlockReference workerRef(Uint32 i) {
    return numberToRef(number(), 1 + i, getOwnNodeId());
  }

  // GSN_READ_CONFIG_REQ
  struct Ss_READ_CONFIG_REQ {
    bool m_active;
    Uint32 m_worker;
    ReadConfigReq m_readConfigReq;
    Ss_READ_CONFIG_REQ() :
      m_active(false)
    {}
  };
  Ss_READ_CONFIG_REQ c_ss_READ_CONFIG_REQ;
  void execREAD_CONFIG_REQ(Signal*);
  void sendREAD_CONFIG_REQ(Signal*, Uint32 i);
  void execREAD_CONFIG_CONF(Signal*);
  void sendREAD_CONFIG_CONF(Signal*);
};

#endif
