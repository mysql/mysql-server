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

#include "ClientInterface.hpp"



ClientInterface::ClientInterface(Uint32 maxNoOfCPCD) {
  sr = new SocketRegistry<SocketService>(maxNoOfCPCD);
  ss = new SocketService();
}


ClientInterface::~ClientInterface() {
  delete sr;
  delete ss;

}


void ClientInterface::connectCPCDdaemon(const char * remotehost, Uint16 port)
{
  sr->createSocketClient(remotehost, port);
}

void ClientInterface::disconnectCPCDdaemon(const char * remotehost)
{
  sr->removeSocketClient(remotehost);
}

void ClientInterface::removeCPCDdaemon(const char * remotehost)
{
  sr->removeSocketClient(remotehost);
}

void ClientInterface::startProcess(const char * remotehost, char * id) {
  char buf[255] = "start process ";
  char str[80];
  char line[10];

  strcpy(line, id);
  strcpy(str, "id:"); 
  strcat(str, line);
  strcat(str, "\n\n");
  strcat(buf, str);
  printf("Request: %s\n", buf); 
 
  sr->performSend(buf,255,remotehost);
  sr->syncPerformReceive(remotehost, *ss, 0);
  ss->getPropertyObject();
}

void ClientInterface::stopProcess(const char * remotehost, char * id) {
  char buf[255] = "stop process ";
  char str[80];  
  char line[10];

  strcpy(line, id);
  strcpy(str, "id:"); 
  strcat(str, line);
  strcat(str, "\n\n");
  strcat(buf, str);
  printf("Request: %s\n", buf); 

  sr->performSend(buf,255,remotehost);
  sr->syncPerformReceive(remotehost, *ss, 0);
  ss->getPropertyObject();
}

void ClientInterface::defineProcess(const char * remotehost, char * name, 
				    char * group, char * env, char * path, 
				    char * args, char * type, char * cwd, char * owner){
  char buf[255] = "define process ";
  char str[80];
  char line[10];
 
  strcpy(line, name);
  strcpy(str, "name:");
  strcat(str, line);
  strcat(buf, str);
  strcat(buf, " \n");

  strcpy(line, group);
  strcpy(str, "group:");
  strcat(str, line);
  strcat(buf, str);
  strcat(buf, " \n");

  strcpy(line, env);
  strcpy(str, "env:");
  strcat(str, line);
  strcat(buf, str);
  strcat(buf, " \n");

  strcpy(line, path);
  strcpy(str, "path:"); 
  strcat(str, line);
  strcat(buf, str);
  strcat(buf, " \n");

  strcpy(line, args);
  strcpy(str, "args:");
  strcat(str, line);
  strcat(buf, str);
  strcat(buf, " \n");

  strcpy(line, type);
  strcpy(str, "type:");
  strcat(str, line);
  strcat(buf, str);
  strcat(buf, " \n");
  
  strcpy(line, cwd);
  strcpy(str, "cwd:");
  strcat(str, line);
  strcat(buf, str);
  strcat(buf, " \n");

  strcpy(line, owner);
  strcpy(str, "owner:");
  strcat(str, line);
  strcat(buf, str); 
  strcat(buf, "\n\n");

  printf("Request: %s\n", buf); 

  sr->performSend(buf,255,remotehost);
  sr->syncPerformReceive(remotehost, *ss, 0);
  ss->getPropertyObject();
}

void ClientInterface::undefineProcess(const char * remotehost, char * id){
  char buf[255] = "undefine process ";
  char str[80];
  char line[10];

  strcpy(line, id);
  strcpy(str, "id:"); 
  strcat(str, line);
  strcat(str, "\n\n");
  strcat(buf, str);
  printf("Request: %s\n", buf); 

  sr->performSend(buf,255,remotehost);
  sr->syncPerformReceive(remotehost, *ss, 0);
  ss->getPropertyObject();
}

void ClientInterface::listProcesses(const char * remotehost) {
  char buf[255]="list processes\n\n"; 
  printf("Request: %s\n", buf);
  sr->performSend(buf,255,remotehost);
  sr->syncPerformReceive(remotehost, *ss, 0);
  ss->getPropertyObject();
}

void ClientInterface::showProcess(const char * remotehost, char * id) {
  char buf[255] = "show process ";
  char str[80];
  char line[10];

  strcpy(line, id);
  strcpy(str, "id:"); 
  strcat(str, line);
  strcat(str, "\n\n");
  strcat(buf, str);
  printf("Request: %s\n", buf); 
 
  sr->performSend(buf,255,remotehost);
  sr->syncPerformReceive(remotehost, *ss, 0);
  ss->getPropertyObject();
}
