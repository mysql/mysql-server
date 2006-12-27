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


#include <ndb_global.h>
#include <NdbOut.hpp>
#include <Properties.hpp>
#include <socket_io.h>
#include <NdbTick.h>
#include <NdbMain.h>
#include <NdbSleep.h>
#include "SocketService.hpp"
#include "SocketRegistry.hpp"
#include "SocketClient.hpp"
#include "ClientInterface.hpp"

#include <InputStream.hpp>

#include <Parser.hpp>

NDB_MAIN(socketclient) {
  
  
  if(argc<3) {
    printf("wrong args: socketclient <hostname> <port>\n");
    return 0;
  }
  const char * remotehost = argv[1];
  const int port   = atoi(argv[2]);
  
  
  ClientInterface * ci = new ClientInterface(2);
  ci->connectCPCDdaemon(remotehost,port);

  /*ci->listProcesses(remotehost);
 
  ci->startProcess(remotehost, "1247"); 

  ci->stopProcess(remotehost, "1247");*/

  ci->defineProcess(remotehost, "ndb", "ndb-cluster1", "envirnm", "/ndb/bin", 
		    "-i", "permanent", "/ndb/ndb.2", "team");

  ci->startProcess(remotehost, "1247"); 

  ci->listProcesses(remotehost);

  //ci->undefineProcess(remotehost, "1247");

  ci->disconnectCPCDdaemon(remotehost);
}
