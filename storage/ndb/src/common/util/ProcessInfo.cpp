/*
   Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

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


#include "ndb_global.h"
#include "NdbHost.h"
#include "ProcessInfo.hpp"
#include "OwnProcessInfo.hpp"
#include "signaldata/ProcessInfoRep.hpp"
#include "EventLogger.hpp"
#include "ndb_net.h"
#include "ndb_socket.h"
#include "NdbTCP.h"

extern EventLogger * g_eventLogger;

/* Utility Functions */

static inline bool isUtf8CharMultibyte(char c) {  // is any part of multi-byte char
  return (c & 0x80);
}

static inline bool isUtf8CharInitial(char c) {   // is first byte of multi-byte char
  return ((c & 0xC0) == 0xC0);
}

static size_t truncateUtf8(const char * string, size_t max_len) {
  size_t len = 0;
  if(string) {
    len = strnlen(string, max_len);
    if(len == max_len) {
      char c = string[len];
      if(isUtf8CharMultibyte(c)) {
        while(! isUtf8CharInitial(c)) {
          c = string[--len];
        }
        len--;
      }
    }
  }
  return len;
}


/* Class ProcessInfo */


ProcessInfo::ProcessInfo()
{
  invalidate();
}

void ProcessInfo::invalidate()
{
  memset(connection_name, 0, ConnectionNameLength);
  memset(host_address, 0, AddressStringLength);
  memset(process_name, 0, ProcessNameLength);
  node_id = 0;
  process_id = 0;
  angel_process_id = 0;
  application_port = 0;
}

/* There is one bootstrap OwnProcessInfo per process,
   but API nodes need one ProcessInfo per Ndb_cluster_connection.
   This named constructor returns OnwProcessInfo for the first 
   requested node id. For subsequent node ids, it creates a
   new ProcessInfo initialized by copying OwnProcessInfo.
*/
ProcessInfo * ProcessInfo::forNodeId(Uint16 nodeId)
{
  ProcessInfo * process = getOwnProcessInfo(nodeId);
  if(process->node_id == nodeId)
    return process;
  /* Make a copy */
  ProcessInfo * self = new ProcessInfo();
  strncpy(self->host_address, process->host_address, AddressStringLength);
  strncpy(self->process_name, process->process_name, ProcessNameLength);
  self->node_id = nodeId;   // do not copy node id or connection name
  self->process_id = process->process_id;
  self->angel_process_id = process->angel_process_id;
  self->application_port = process->application_port;
  return self;
}

/* Delete a ProcessInfo only if it was new-allocated in ProcessInfo::forNodeId()
*/
void ProcessInfo::release(ProcessInfo *self)
{
  if((self != 0) && (self != getOwnProcessInfo(0)))
    delete self;
}

void ProcessInfo::setProcessName(const char * name) {
  size_t len = truncateUtf8(name, ProcessNameLength);
  strncpy(process_name, name, len);
  process_name[len] = 0;
}

void ProcessInfo::setPid() {
  process_id = NdbHost_GetProcessId();
}

int ProcessInfo::getPid() const {
  assert(isValid());
  return process_id;
}

void ProcessInfo::setConnectionName(const char * name) {
  size_t len = truncateUtf8(name, ConnectionNameLength);
  strncpy(connection_name, name, len);
  connection_name[len] = 0;
  g_eventLogger->info("ProcessInfo set connection name: %s", connection_name);
}

void ProcessInfo::setConnectionName(Uint32 * signal_data) {
  memcpy(connection_name, signal_data, ConnectionNameLength);
}

void ProcessInfo::setHostAddress(const char * address_string) {
  if(address_string) {
    strncpy(host_address, address_string, AddressStringLength);
  }
}

void ProcessInfo::setHostAddress(Uint32 * signal_data) {
  setHostAddress((const char *) signal_data);
}

void ProcessInfo::setHostAddress(const struct sockaddr * addr, socklen_t len) {
  getnameinfo(addr, len, host_address, AddressStringLength, 0, 0, NI_NUMERICHOST);
}

void ProcessInfo::setHostAddress(const struct in_addr * addr) {
    Ndb_inet_ntop(AF_INET, addr, host_address, AddressStringLength);
}

void ProcessInfo::setAngelPid(Uint32 pid) {
  angel_process_id = pid;
}

void ProcessInfo::setPort(Uint16 port) {
  application_port = port;
  g_eventLogger->info("ProcessInfo set port: %d", application_port);
}

void ProcessInfo::setNodeId(Uint16 nodeId) {
  node_id = nodeId;
}

void ProcessInfo::initializeFromProcessInfoRep(ProcessInfoRep * signal) {
  g_eventLogger->info("Received ProcessInfoRep. "
    "Node: %d, Port: %d, Name: %s, Pid: %d",
    signal->node_id, signal->application_port,
    signal->process_name, signal->process_id);
  if(isValid()) invalidate();
  setProcessName( (char *) signal->process_name);
  process_id = signal->process_id;
  angel_process_id = signal->angel_process_id;
  application_port = signal->application_port;
  node_id = signal->node_id;
}

void ProcessInfo::buildProcessInfoReport(ProcessInfoRep *signal) {
  memcpy(signal->process_name, process_name, ProcessNameLength);
  signal->node_id = node_id;
  signal->process_id = process_id;
  signal->angel_process_id = angel_process_id;
  signal->application_port = application_port;

  g_eventLogger->info("Created ProcessInfoRep. "
    "Node: %d, Port: %d, Name: %s, Pid: %d",
    signal->node_id, signal->application_port,
    signal->process_name, signal->process_id);
}
