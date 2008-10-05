/* Copyright (C) 2007 MySQL AB

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

#ifndef DBINFO_H
#define DBINFO_H

#include <SimulatedBlock.hpp>

#include <NodeBitmask.hpp>
#include <SLList.hpp>


class Dbinfo : public SimulatedBlock
{
public:
  Dbinfo(Block_context& ctx);
  virtual ~Dbinfo();
  BLOCK_DEFINES(Dbinfo);

protected:
  struct Node {
    Uint32 nodeId;
    Uint32 alive;
    Uint32 nextList;
    union { Uint32 prevList; Uint32 nextPool; };
  };
  typedef Ptr<Node> NodePtr;

  NodeId c_masterNodeId;
  ArrayPool<Node> c_nodePool;
  SLList<Node> c_nodes;
  NdbNodeBitmask c_aliveNodes;


  void execSTTOR(Signal* signal);
  void sendSTTORRY(Signal*);
  void execREAD_CONFIG_REQ(Signal*);
  void execDUMP_STATE_ORD(Signal* signal);

  void execDBINFO_SCANREQ(Signal *signal);
  void execDBINFO_TRANSID_AI(Signal* signal);
  void execDBINFO_SCANCONF(Signal *signal);

  /* for maintaining c_aliveNodes */
  void execREAD_NODESCONF(Signal* signal);
  void execINCL_NODEREQ(Signal* signal);
  void execNODE_FAILREP(Signal* signal);
};

#endif
