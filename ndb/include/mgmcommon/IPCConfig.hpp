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

#ifndef IPCConfig_H
#define IPCConfig_H

#include <ndb_types.h>
#include <ndb_limits.h>
#include <kernel_types.h>
#include <Properties.hpp>

/**
 * @class IPCConfig
 * @brief Config transporters in TransporterRegistry using Properties config
 */
class IPCConfig 
{
public:
  IPCConfig(Properties * props);
  ~IPCConfig();

  /** @return 0 for OK */
  int init(); 
  
  NodeId ownId() const;
  
  /** @return No of transporters configured */
  int configureTransporters(class TransporterRegistry * theTransporterRegistry);

  /**
   * Supply a nodeId,
   *  and get next higher node id
   * @return false if none found, true otherwise
   *
   * getREPHBFrequency and getNodeType uses the last Id supplied to 
   * getNextRemoteNodeId.
   */
  bool getNextRemoteNodeId(NodeId & nodeId) const;
  Uint32 getREPHBFrequency(NodeId id) const;
  const char* getNodeType(NodeId id) const;
  
  NodeId getNoOfRemoteNodes() const {
    return theNoOfRemoteNodes;
  }

  void print() const { props->print(); }

  static Uint32 configureTransporters(Uint32 nodeId,
				      const struct ndb_mgm_configuration &,
				      class TransporterRegistry &);
  
private:
  NodeId        the_ownId;
  Properties *  props;
  
  bool    addRemoteNodeId(NodeId nodeId);
  NodeId  theNoOfRemoteNodes;
  NodeId  theRemoteNodeIds[MAX_NODES];
};

inline 
NodeId 
IPCConfig::ownId() const
{
  return the_ownId;
}



#endif // IPCConfig_H
