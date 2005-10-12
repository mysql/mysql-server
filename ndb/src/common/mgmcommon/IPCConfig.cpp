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

#include <ndb_global.h>
#include <ndb_opt_defaults.h>
#include <IPCConfig.hpp>
#include <NdbOut.hpp>
#include <NdbHost.h>

#include <TransporterDefinitions.hpp>
#include <TransporterRegistry.hpp>
#include <Properties.hpp>

#include <mgmapi_configuration.hpp>
#include <mgmapi_config_parameters.h>

#if defined DEBUG_TRANSPORTER
#define DEBUG(t) ndbout << __FILE__ << ":" << __LINE__ << ":" << t << endl;
#else
#define DEBUG(t)
#endif

IPCConfig::IPCConfig(Properties * p)
{
  theNoOfRemoteNodes = 0;
  the_ownId = 0;
  if(p != 0)
    props = new Properties(* p);
  else
    props = 0;
}


IPCConfig::~IPCConfig()
{
  if(props != 0){
    delete props;
  }
}

int
IPCConfig::init(){
  Uint32 nodeId;

  if(props == 0) return -1;
  if(!props->get("LocalNodeId", &nodeId)) {
    DEBUG( "Did not find local node id." );
    return -1;
  }
  the_ownId = nodeId;
  
  Uint32 noOfConnections;
  if(!props->get("NoOfConnections", &noOfConnections)) {
    DEBUG( "Did not find noOfConnections." );
    return -1;
  }
  
  for(Uint32 i = 0; i<noOfConnections; i++){
    const Properties * tmp;
    Uint32 node1, node2;

    if(!props->get("Connection", i, &tmp)) {
      DEBUG( "Did not find Connection." );
      return -1;
    }
    if(!tmp->get("NodeId1", &node1)) {
      DEBUG( "Did not find NodeId1." );
      return -1;
    }
    if(!tmp->get("NodeId2", &node2)) {
      DEBUG( "Did not find NodeId2." );
      return -1;
    }

    if(node1 == the_ownId && node2 != the_ownId)
      if(!addRemoteNodeId(node2)) {
	DEBUG( "addRemoteNodeId(node2) failed." );
	return -1;
      }

    if(node1 != the_ownId && node2 == the_ownId)
      if(!addRemoteNodeId(node1)) {
	DEBUG( "addRemoteNodeId(node2) failed." );
	return -1;
      }
  }
  return 0;
}

bool
IPCConfig::addRemoteNodeId(NodeId nodeId){
  for(int i = 0; i<theNoOfRemoteNodes; i++)
    if(theRemoteNodeIds[i] == nodeId)
      return false;
  theRemoteNodeIds[theNoOfRemoteNodes] = nodeId;
  theNoOfRemoteNodes++;
  return true;
}

/**
 * Supply a nodeId,
 *  and get next higher node id
 * Returns false if none found
 */
bool
IPCConfig::getNextRemoteNodeId(NodeId & nodeId) const {
  NodeId returnNode = MAX_NODES + 1;
  for(int i = 0; i<theNoOfRemoteNodes; i++)
    if(theRemoteNodeIds[i] > nodeId){
      if(theRemoteNodeIds[i] < returnNode){
	returnNode = theRemoteNodeIds[i];
      }
    }
  if(returnNode == (MAX_NODES + 1))
    return false;
  nodeId = returnNode;
  return true;
}


Uint32 
IPCConfig::getREPHBFrequency(NodeId id) const {
  const Properties * tmp;
  Uint32 out;

  /**
   *  Todo: Fix correct heartbeat
   */
  if (!props->get("Node", id, &tmp) || 
      !tmp->get("HeartbeatIntervalRepRep", &out)) {
    DEBUG("Illegal Node or HeartbeatIntervalRepRep in config.");    
    out = 10000;
  }
  
  return out;
}

const char* 
IPCConfig::getNodeType(NodeId id) const {
  const char * out;
  const Properties * tmp;

  if (!props->get("Node", id, &tmp) || !tmp->get("Type", &out)) {
    DEBUG("Illegal Node or NodeType in config.");
    out = "Unknown";
  }

  return out;
}

#include <mgmapi.h>
Uint32
IPCConfig::configureTransporters(Uint32 nodeId,
				 const class ndb_mgm_configuration & config,
				 class TransporterRegistry & tr){
  TransporterConfiguration conf;

  DBUG_ENTER("IPCConfig::configureTransporters");

  /**
   * Iterate over all MGM's an construct a connectstring
   * create mgm_handle and give it to the Transporter Registry
   */
  {
    const char *separator= "";
    BaseString connect_string;
    ndb_mgm_configuration_iterator iter(config, CFG_SECTION_NODE);
    for(iter.first(); iter.valid(); iter.next())
    {
      Uint32 type;
      if(iter.get(CFG_TYPE_OF_SECTION, &type)) continue;
      if(type != NODE_TYPE_MGM) continue;
      const char* hostname;
      Uint32 port;
      if(iter.get(CFG_NODE_HOST, &hostname)) continue;
      if( strlen(hostname) == 0 ) continue;
      if(iter.get(CFG_MGM_PORT, &port)) continue;
      connect_string.appfmt("%s%s:%u",separator,hostname,port);
      separator= ",";
    }
    NdbMgmHandle h= ndb_mgm_create_handle();
    if ( h && connect_string.length() > 0 )
    {
      ndb_mgm_set_connectstring(h,connect_string.c_str());
      tr.set_mgm_handle(h);
    }
  }

  Uint32 noOfTransportersCreated= 0;
  ndb_mgm_configuration_iterator iter(config, CFG_SECTION_CONNECTION);
  
  for(iter.first(); iter.valid(); iter.next()){
    
    Uint32 nodeId1, nodeId2, remoteNodeId;
    const char * remoteHostName= 0, * localHostName= 0;
    if(iter.get(CFG_CONNECTION_NODE_1, &nodeId1)) continue;
    if(iter.get(CFG_CONNECTION_NODE_2, &nodeId2)) continue;

    if(nodeId1 != nodeId && nodeId2 != nodeId) continue;
    remoteNodeId = (nodeId == nodeId1 ? nodeId2 : nodeId1);

    {
      const char * host1= 0, * host2= 0;
      iter.get(CFG_CONNECTION_HOSTNAME_1, &host1);
      iter.get(CFG_CONNECTION_HOSTNAME_2, &host2);
      localHostName  = (nodeId == nodeId1 ? host1 : host2);
      remoteHostName = (nodeId == nodeId1 ? host2 : host1);
    }

    Uint32 sendSignalId = 1;
    Uint32 checksum = 1;
    if(iter.get(CFG_CONNECTION_SEND_SIGNAL_ID, &sendSignalId)) continue;
    if(iter.get(CFG_CONNECTION_CHECKSUM, &checksum)) continue;

    Uint32 type = ~0;
    if(iter.get(CFG_TYPE_OF_SECTION, &type)) continue;

    Uint32 server_port= 0;
    if(iter.get(CFG_CONNECTION_SERVER_PORT, &server_port)) break;
    
    Uint32 nodeIdServer= 0;
    if(iter.get(CFG_CONNECTION_NODE_ID_SERVER, &nodeIdServer)) break;

    /*
      We check the node type.
    */
    Uint32 node1type, node2type;
    ndb_mgm_configuration_iterator node1iter(config, CFG_SECTION_NODE);
    ndb_mgm_configuration_iterator node2iter(config, CFG_SECTION_NODE);
    node1iter.find(CFG_NODE_ID,nodeId1);
    node2iter.find(CFG_NODE_ID,nodeId2);
    node1iter.get(CFG_TYPE_OF_SECTION,&node1type);
    node2iter.get(CFG_TYPE_OF_SECTION,&node2type);

    if(node1type==NODE_TYPE_MGM || node2type==NODE_TYPE_MGM)
      conf.isMgmConnection= true;
    else
      conf.isMgmConnection= false;

    if (nodeId == nodeIdServer && !conf.isMgmConnection) {
      tr.add_transporter_interface(remoteNodeId, localHostName, server_port);
    }

    DBUG_PRINT("info", ("Transporter between this node %d and node %d using port %d, signalId %d, checksum %d",
               nodeId, remoteNodeId, server_port, sendSignalId, checksum));
    /*
      This may be a dynamic port. It depends on when we're getting
      our configuration. If we've been restarted, we'll be getting
      a configuration with our old dynamic port in it, hence the number
      here is negative (and we try the old port number first).

      On a first-run, server_port will be zero (with dynamic ports)

      If we're not using dynamic ports, we don't do anything.
    */

    conf.localNodeId    = nodeId;
    conf.remoteNodeId   = remoteNodeId;
    conf.checksum       = checksum;
    conf.signalId       = sendSignalId;
    conf.s_port         = server_port;
    conf.localHostName  = localHostName;
    conf.remoteHostName = remoteHostName;
    conf.serverNodeId   = nodeIdServer;

    switch(type){
    case CONNECTION_TYPE_SHM:
      if(iter.get(CFG_SHM_KEY, &conf.shm.shmKey)) break;
      if(iter.get(CFG_SHM_BUFFER_MEM, &conf.shm.shmSize)) break;

      Uint32 tmp;
      if(iter.get(CFG_SHM_SIGNUM, &tmp)) break;
      conf.shm.signum= tmp;

      if(!tr.createSHMTransporter(&conf)){
        DBUG_PRINT("error", ("Failed to create SHM Transporter from %d to %d",
	           conf.localNodeId, conf.remoteNodeId));
	ndbout << "Failed to create SHM Transporter from: " 
	       << conf.localNodeId << " to: " << conf.remoteNodeId << endl;
      } else {
	noOfTransportersCreated++;
      }
      DBUG_PRINT("info", ("Created SHM Transporter using shmkey %d, "
			  "buf size = %d", conf.shm.shmKey, conf.shm.shmSize));

      break;

    case CONNECTION_TYPE_SCI:
      if(iter.get(CFG_SCI_SEND_LIMIT, &conf.sci.sendLimit)) break;
      if(iter.get(CFG_SCI_BUFFER_MEM, &conf.sci.bufferSize)) break;
      if (nodeId == nodeId1) {
        if(iter.get(CFG_SCI_HOST2_ID_0, &conf.sci.remoteSciNodeId0)) break;
        if(iter.get(CFG_SCI_HOST2_ID_1, &conf.sci.remoteSciNodeId1)) break;
      } else {
        if(iter.get(CFG_SCI_HOST1_ID_0, &conf.sci.remoteSciNodeId0)) break;
        if(iter.get(CFG_SCI_HOST1_ID_1, &conf.sci.remoteSciNodeId1)) break;
      }
      if (conf.sci.remoteSciNodeId1 == 0) {
        conf.sci.nLocalAdapters = 1;
      } else {
        conf.sci.nLocalAdapters = 2;
      }
     if(!tr.createSCITransporter(&conf)){
        DBUG_PRINT("error", ("Failed to create SCI Transporter from %d to %d",
	           conf.localNodeId, conf.remoteNodeId));
	ndbout << "Failed to create SCI Transporter from: " 
	       << conf.localNodeId << " to: " << conf.remoteNodeId << endl;
      } else {
        DBUG_PRINT("info", ("Created SCI Transporter: Adapters = %d, "
			    "remote SCI node id %d",
                   conf.sci.nLocalAdapters, conf.sci.remoteSciNodeId0));
        DBUG_PRINT("info", ("Host 1 = %s, Host 2 = %s, sendLimit = %d, "
			    "buf size = %d", conf.localHostName,
			    conf.remoteHostName, conf.sci.sendLimit,
			    conf.sci.bufferSize));
        if (conf.sci.nLocalAdapters > 1) {
          DBUG_PRINT("info", ("Fault-tolerant with 2 Remote Adapters, "
			      "second remote SCI node id = %d",
			      conf.sci.remoteSciNodeId1)); 
        }
	noOfTransportersCreated++;
	continue;
      }
     break;

    case CONNECTION_TYPE_TCP:
      if(iter.get(CFG_TCP_SEND_BUFFER_SIZE, &conf.tcp.sendBufferSize)) break;
      if(iter.get(CFG_TCP_RECEIVE_BUFFER_SIZE, &conf.tcp.maxReceiveSize)) break;
      
      const char * proxy;
      if (!iter.get(CFG_TCP_PROXY, &proxy)) {
	if (strlen(proxy) > 0 && nodeId2 == nodeId) {
	  // TODO handle host:port
	  conf.s_port = atoi(proxy);
	}
      }
      
      if(!tr.createTCPTransporter(&conf)){
	ndbout << "Failed to create TCP Transporter from: " 
	       << nodeId << " to: " << remoteNodeId << endl;
      } else {
	noOfTransportersCreated++;
      }
      DBUG_PRINT("info", ("Created TCP Transporter: sendBufferSize = %d, "
			  "maxReceiveSize = %d", conf.tcp.sendBufferSize,
			  conf.tcp.maxReceiveSize));
      break;
    case CONNECTION_TYPE_OSE:
      if(iter.get(CFG_OSE_PRIO_A_SIZE, &conf.ose.prioASignalSize)) break;
      if(iter.get(CFG_OSE_PRIO_B_SIZE, &conf.ose.prioBSignalSize)) break;
      
      if(!tr.createOSETransporter(&conf)){
	ndbout << "Failed to create OSE Transporter from: " 
	       << nodeId << " to: " << remoteNodeId << endl;
      } else {
	noOfTransportersCreated++;
      }
      break;

    default:
      ndbout << "Unknown transporter type from: " << nodeId << 
	" to: " << remoteNodeId << endl;
      break;
    } // switch
  } // for

  DBUG_RETURN(noOfTransportersCreated);
}
  
