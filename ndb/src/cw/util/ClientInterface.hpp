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

#ifndef CLIENT_IF_HPP
#define CLIENT_IF_HPP
#include <ndb_global.h>
#include <Parser.hpp>
#include <InputStream.hpp>
#include <Parser.hpp>
#include <NdbOut.hpp>
#include <Properties.hpp>
#include "SocketRegistry.hpp"
#include "SocketService.hpp"

class ClientInterface {
private:
  SocketService * ss;
  SocketRegistry<SocketService> * sr;
  
public:
  ClientInterface(Uint32 maxNoOfCPC);
  ~ClientInterface();
  void startProcess(const char * remotehost, char * id);
  void stopProcess(const char * remotehost, char * id);
  void defineProcess(const char * remotehost,  char * name, char * group, 
		     char * env, char * path, char * args, char * type,
		     char * cwd, char * owner);
  void undefineProcess(const char * remotehost, char * id);
  void listProcesses(const char * remotehost);
  void showProcess(const char * remotehost, char * id);
  void connectCPCDdaemon(const char * remotehost, Uint16 port);
  void disconnectCPCDdaemon(const char * remotehost);
  void removeCPCDdaemon(const char * remotehost);
  
};
#endif
