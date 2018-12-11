/* 
   Copyright (c) 2003, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <ndb_global.h>
#include <IPCConfig.hpp>

#include <TransporterRegistry.hpp>

#include <mgmapi.h>
#include <mgmapi_configuration.hpp>


/* Return true if node with "nodeId" is a MGM node */
static bool is_mgmd(Uint32 nodeId,
                    const struct ndb_mgm_configuration & config)
{
  ndb_mgm_configuration_iterator iter(config, CFG_SECTION_NODE);
  if (iter.find(CFG_NODE_ID, nodeId))
    abort();
  Uint32 type;
  if(iter.get(CFG_TYPE_OF_SECTION, &type))
    abort();

  return (type == NODE_TYPE_MGM);
}


bool
IPCConfig::configureTransporters(Uint32 nodeId,
                                 const struct ndb_mgm_configuration & config,
                                 class TransporterRegistry & tr,
                                 bool transporter_to_self)
{
  bool result= true;

  DBUG_ENTER("IPCConfig::configureTransporters");


  if (!is_mgmd(nodeId, config))
  {

    /**
     * Iterate over all MGM's and construct a connectstring
     * create mgm_handle and give it to the Transporter Registry
     */

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


  /* Remove transporter to nodes that does not exist anymore */
  for (int i= 1; i < MAX_NODES; i++)
  {
    ndb_mgm_configuration_iterator iter(config, CFG_SECTION_NODE);
    if (tr.get_transporter(i) && iter.find(CFG_NODE_ID, i))
    {
      // Transporter exist in TransporterRegistry but not
      // in configuration
      ndbout_c("The connection to node %d could not "
               "be removed at this time", i);
      result= false; // Need restart
    }
  }

  TransporterConfiguration conf;
  TransporterConfiguration loopback_conf;
  ndb_mgm_configuration_iterator iter(config, CFG_SECTION_CONNECTION);
  for(iter.first(); iter.valid(); iter.next()){
    
    bzero(&conf, sizeof(conf));
    Uint32 nodeId1, nodeId2, remoteNodeId;
    const char * remoteHostName= 0, * localHostName= 0;
    if(iter.get(CFG_CONNECTION_NODE_1, &nodeId1)) continue;
    if(iter.get(CFG_CONNECTION_NODE_2, &nodeId2)) continue;

    if(nodeId1 != nodeId && nodeId2 != nodeId) continue;
    remoteNodeId = (nodeId == nodeId1 ? nodeId2 : nodeId1);

    if (nodeId1 == nodeId && nodeId2 == nodeId)
    {
      transporter_to_self = false; // One already present..ignore extra arg
    }

    {
      const char * host1= 0, * host2= 0;
      iter.get(CFG_CONNECTION_HOSTNAME_1, &host1);
      iter.get(CFG_CONNECTION_HOSTNAME_2, &host2);
      localHostName  = (nodeId == nodeId1 ? host1 : host2);
      remoteHostName = (nodeId == nodeId1 ? host2 : host1);
    }

    Uint32 sendSignalId = 1;
    Uint32 checksum = 1;
    Uint32 preSendChecksum = 0;
    if(iter.get(CFG_CONNECTION_SEND_SIGNAL_ID, &sendSignalId)) continue;
    if(iter.get(CFG_CONNECTION_CHECKSUM, &checksum)) continue;
    iter.get(CFG_CONNECTION_PRESEND_CHECKSUM, &preSendChecksum);

    Uint32 type = ~0;
    if(iter.get(CFG_TYPE_OF_SECTION, &type)) continue;

    Uint32 server_port= 0;
    if(iter.get(CFG_CONNECTION_SERVER_PORT, &server_port)) break;
    
    Uint32 nodeIdServer= 0;
    if(iter.get(CFG_CONNECTION_NODE_ID_SERVER, &nodeIdServer)) break;

    if(is_mgmd(nodeId1, config) || is_mgmd(nodeId2, config))
    {
      // All connections with MGM uses the mgm port as server
      conf.isMgmConnection= true;
    }
    else
      conf.isMgmConnection= false;

    Uint32 bindInAddrAny = 0;
    iter.get(CFG_TCP_BIND_INADDR_ANY, &bindInAddrAny);

    if (nodeId == nodeIdServer && !conf.isMgmConnection) {
      tr.add_transporter_interface(remoteNodeId, 
				   !bindInAddrAny ? localHostName : "", 
				   server_port);
    }
    
    DBUG_PRINT("info", ("Transporter between this node %d and node %d using port %d, signalId %d, checksum %d,"
        "preSendChecksum %d",
               nodeId, remoteNodeId, server_port, sendSignalId, checksum, preSendChecksum));
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
    conf.preSendChecksum = preSendChecksum;
    conf.signalId       = sendSignalId;
    conf.s_port         = server_port;
    conf.localHostName  = localHostName;
    conf.remoteHostName = remoteHostName;
    conf.serverNodeId   = nodeIdServer;

    Uint32 spintime = 0;
    Uint32 shm_send_buffer_size = 2 * 1024 * 1024;
    switch(type){
    case CONNECTION_TYPE_SHM:
      if(iter.get(CFG_SHM_KEY, &conf.shm.shmKey)) break;
      if(iter.get(CFG_SHM_BUFFER_MEM, &conf.shm.shmSize)) break;

      iter.get(CFG_SHM_SPINTIME, &spintime);
      conf.shm.shmSpintime= spintime;
      iter.get(CFG_SHM_SEND_BUFFER_SIZE, &shm_send_buffer_size);
      conf.shm.sendBufferSize= shm_send_buffer_size;

      conf.type = tt_SHM_TRANSPORTER;

      if(!tr.configureTransporter(&conf)){
        DBUG_PRINT("error", ("Failed to configure SHM Transporter "
                             "from %d to %d",
	           conf.localNodeId, conf.remoteNodeId));
	ndbout_c("Failed to configure SHM Transporter to node %d",
                conf.remoteNodeId);
        result = false;
      }
#ifdef NDB_WIN32
      ndbout_c("Shared memory transporters not supported on Windows");
      result = false;
#else
      DBUG_PRINT("info", ("Configured SHM Transporter using shmkey %d, "
			  "buf size = %d", conf.shm.shmKey, conf.shm.shmSize));
#endif
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

      iter.get(CFG_TCP_SND_BUF_SIZE, &conf.tcp.tcpSndBufSize);
      iter.get(CFG_TCP_RCV_BUF_SIZE, &conf.tcp.tcpRcvBufSize);
      iter.get(CFG_TCP_MAXSEG_SIZE, &conf.tcp.tcpMaxsegSize);
      iter.get(CFG_CONNECTION_OVERLOAD, &conf.tcp.tcpOverloadLimit);

      conf.type = tt_TCP_TRANSPORTER;
      
      if(!tr.configureTransporter(&conf)){
	ndbout_c("Failed to configure TCP Transporter to node %d",
                 conf.remoteNodeId);
        result= false;
      }
      DBUG_PRINT("info", ("Configured TCP Transporter: sendBufferSize = %d, "
			  "maxReceiveSize = %d", conf.tcp.sendBufferSize,
			  conf.tcp.maxReceiveSize));
      loopback_conf = conf; // reuse it...
      break;
    default:
      ndbout << "Unknown transporter type from: " << nodeId << 
	" to: " << remoteNodeId << endl;
      break;
    } // switch
  } // for

  if (transporter_to_self)
  {
    loopback_conf.remoteNodeId = nodeId;
    loopback_conf.localNodeId = nodeId;
    loopback_conf.serverNodeId = 0; // always client
    loopback_conf.remoteHostName = "localhost";
    loopback_conf.localHostName = "localhost";
    loopback_conf.s_port = 1; // prevent asking ndb_mgmd for port...
    loopback_conf.type = tt_TCP_TRANSPORTER;
    loopback_conf.checksum = 0;
    loopback_conf.signalId = 0;
    loopback_conf.tcp.sendBufferSize = 1024*1024;
    loopback_conf.tcp.maxReceiveSize = 1024*1024;
    loopback_conf.tcp.tcpSndBufSize = 0;
    loopback_conf.tcp.tcpRcvBufSize = 0;
    loopback_conf.tcp.tcpMaxsegSize = 256*1024;
    loopback_conf.tcp.tcpOverloadLimit = 768*1024;
    if (!tr.configureTransporter(&loopback_conf))
    {
      ndbout_c("Failed to configure Loopback Transporter");
      result= false;
    }
  }

  DBUG_RETURN(result);
}
  
