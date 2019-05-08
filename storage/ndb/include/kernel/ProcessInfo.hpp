/*
   Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NDB_PROCESSINFO_HPP
#define NDB_PROCESSINFO_HPP


/* Forward Declarations */
class ProcessInfoRep;

/* Class ProcessInfo */
class ProcessInfo {
  friend void getNameFromEnvironment();
  friend ProcessInfo * getOwnProcessInfo(Uint16);

public:
  ProcessInfo();
  ~ProcessInfo() {}

  static ProcessInfo * forNodeId(Uint16);
  static void release(ProcessInfo *);

  static bool isValidUri(const char *scheme, const char *path);

  bool isValid() const;
  void invalidate();

  void setUriPath(const char *);
  void setUriPath(Uint32 *signal_data);
  void setUriScheme(const char *);
  void setProcessName(const char *);
  void setHostAddress(const struct sockaddr *, socklen_t);
  void setHostAddress(const struct in_addr *);
  void setHostAddress(Uint32 *signal_data);
  void setHostAddress(const char *);
  void setPid();
  void setAngelPid(Uint32 pid);
  void setPort(Uint16);
  void setNodeId(Uint16);

  int getServiceUri(char * buffer, size_t length) const;

  const char * getUriPath() const        { return uri_path;          }
  const char * getUriScheme() const      { return uri_scheme;        }
  const char * getProcessName() const    { return process_name;      }
  const char * getHostAddress() const    { return host_address;      }
  int getPid() const;
  int getAngelPid() const                { return angel_process_id;  }
  int getPort() const                    { return application_port;  }
  int getNodeId() const                  { return node_id;           }

  STATIC_CONST( UriPathLength = 128 );
  STATIC_CONST( UriPathLengthInWords = 32 );
  STATIC_CONST( UriSchemeLength = 16);
  STATIC_CONST( ProcessNameLength = 48 );
  STATIC_CONST( AddressStringLength = 48 );  // Long enough for IPv6
  STATIC_CONST( AddressStringLengthInWords = 12);

  /* Interface for ClusterManager to create signal */
  void buildProcessInfoReport(ProcessInfoRep *);

  /* Interface for Qmgr to build ProcessInfo for remote node
     from received signal */
  void initializeFromProcessInfoRep(ProcessInfoRep *);


private:              /* Data Members */
  char uri_path[UriPathLength];
  char host_address[AddressStringLength];
  char process_name[ProcessNameLength];
  char uri_scheme[UriSchemeLength];
  Uint32 node_id;
  Uint32 process_id;
  Uint32 angel_process_id;
  Uint32 application_port;
};   // 256 bytes per node


inline bool ProcessInfo::isValid() const {
  return process_id;
}

#endif
