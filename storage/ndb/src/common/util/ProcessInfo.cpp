/*
   Copyright (c) 2016, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "ProcessInfo.hpp"
#include "BaseString.hpp"
#include "NdbHost.h"
#include "NdbTCP.h"
#include "OwnProcessInfo.hpp"
#include "ndb_global.h"
#include "ndb_socket.h"
#include "signaldata/ProcessInfoRep.hpp"

class ndb_sockaddr;

/* Utility Functions */

static inline bool isValidUriSchemeChar(char c) {
  return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || (c == '+') ||
         (c == '.') || (c == '-');
}

static bool valid_URI_scheme(const char *s) {
  while (*s) {
    if (!isValidUriSchemeChar(*s)) return false;
    s++;
  }
  return true;
}

static inline bool isUtf8CharMultibyte(
    char c) {  // is any part of multi-byte char
  return (c & 0x80);
}

static inline bool isUtf8CharInitial(
    char c) {  // is first byte of multi-byte char
  return ((c & 0xC0) == 0xC0);
}

static size_t truncateUtf8(const char *string, size_t max_len) {
  size_t len = 0;
  if (string) {
    len = strnlen(string, max_len);
    if (len == max_len) {
      char c = string[len];
      if (isUtf8CharMultibyte(c)) {
        while (!isUtf8CharInitial(c)) {
          c = string[--len];
        }
        len--;
      }
    }
  }
  return len;
}

/* Class ProcessInfo */

ProcessInfo::ProcessInfo() { invalidate(); }

void ProcessInfo::invalidate() {
  memset(uri_path, 0, UriPathLength);
  memset(host_address, 0, AddressStringLength);
  memset(process_name, 0, ProcessNameLength);
  strcpy(uri_scheme, "ndb");
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
ProcessInfo *ProcessInfo::forNodeId(Uint16 nodeId) {
  ProcessInfo *process = getOwnProcessInfo(nodeId);
  if (process->node_id == nodeId) return process;
  /* Make a copy */
  ProcessInfo *self = new ProcessInfo();
  self->node_id = nodeId;  // do not copy node id
  strncpy(self->process_name, process->process_name, ProcessNameLength);
  self->process_id = process->process_id;
  self->angel_process_id = process->angel_process_id;
  /* Do not copy any of the fields that will be set from set_service_uri() */
  return self;
}

/* Delete a ProcessInfo only if it was new-allocated in ProcessInfo::forNodeId()
 */
void ProcessInfo::release(ProcessInfo *self) {
  if ((self != nullptr) && (self != getOwnProcessInfo(0))) delete self;
}

/* Check URI components for syntactic validity
 */
bool ProcessInfo::isValidUri(const char *scheme, const char *path) {
  if (path && path[0] == '/' && path[1] == '/') return false;
  return valid_URI_scheme(scheme);
}

void ProcessInfo::setProcessName(const char *name) {
  size_t len = 0;
  if (name != nullptr) {
    len = truncateUtf8(name, ProcessNameLength);
    strncpy(process_name, name, len);
  }
  process_name[len] = 0;
}

void ProcessInfo::setPid() { process_id = NdbHost_GetProcessId(); }

int ProcessInfo::getPid() const {
  assert(isValid());
  return process_id;
}

void ProcessInfo::setUriPath(const char *path) {
  size_t len = 0;
  if (path != nullptr) {
    len = truncateUtf8(path, UriPathLength);
    strncpy(uri_path, path, len);
  }
  uri_path[len] = 0;
}

void ProcessInfo::setUriPath(Uint32 *signal_data) {
  memcpy(uri_path, signal_data, UriPathLength);
}

void ProcessInfo::setUriScheme(const char *scheme) {
  if (scheme && scheme[0] && valid_URI_scheme(scheme)) {
    strncpy(uri_scheme, scheme, UriSchemeLength);
    uri_scheme[UriSchemeLength - 1] = '\0';
  }
}

void ProcessInfo::setHostAddress(const char *address_string) {
  if (address_string) {
    strncpy(host_address, address_string, AddressStringLength);
    host_address[AddressStringLength - 1] = '\0';
  }
}

void ProcessInfo::setHostAddress(Uint32 *signal_data) {
  setHostAddress((const char *)signal_data);
}

void ProcessInfo::setHostAddress(const ndb_sockaddr *addr) {
  /* If address passed in is a wildcard address, do not use it. */
  if (!addr->is_unspecified())
    Ndb_inet_ntop(addr, host_address, AddressStringLength);
}

void ProcessInfo::setAngelPid(Uint32 pid) { angel_process_id = pid; }

void ProcessInfo::setPort(Uint16 port) { application_port = port; }

void ProcessInfo::setNodeId(Uint16 nodeId) { node_id = nodeId; }

void ProcessInfo::initializeFromProcessInfoRep(ProcessInfoRep *signal) {
  if (isValid()) invalidate();
  setProcessName((char *)signal->process_name);
  setUriScheme((char *)signal->uri_scheme);
  process_id = signal->process_id;
  angel_process_id = signal->angel_process_id;
  application_port = signal->application_port;
  node_id = signal->node_id;
}

void ProcessInfo::buildProcessInfoReport(ProcessInfoRep *signal) {
  memcpy(signal->process_name, process_name, ProcessNameLength);
  memcpy(signal->uri_scheme, uri_scheme, UriSchemeLength);
  signal->node_id = node_id;
  signal->process_id = process_id;
  signal->angel_process_id = angel_process_id;
  signal->application_port = application_port;
}

int ProcessInfo::getServiceUri(char *buffer, size_t buf_len) const {
  int len;

  /* Path must begin with a single slash if authority was present. */
  const char *path_prefix = "";
  if (uri_path[0] != '\0' && uri_path[0] != '/') {
    path_prefix = "/";
  }

  char buf[512];
  if (application_port > 0) {
    char *sockaddr_string = Ndb_combine_address_port(
        buf, sizeof(buf), host_address, application_port);
    len = BaseString::snprintf(buffer, buf_len, "%s://%s%s%s", uri_scheme,
                               sockaddr_string, path_prefix, uri_path);
  } else {
    if (strchr(host_address, ':') == nullptr) {
      len = BaseString::snprintf(buffer, buf_len, "%s://%s%s%s", uri_scheme,
                                 host_address, path_prefix, uri_path);
    } else {
      len = BaseString::snprintf(buffer, buf_len, "%s://[%s]%s%s", uri_scheme,
                                 host_address, path_prefix, uri_path);
    }
  }
  return len;
}
