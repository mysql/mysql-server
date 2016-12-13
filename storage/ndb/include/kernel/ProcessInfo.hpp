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
  ~ProcessInfo() {};

  static ProcessInfo * forNodeId(Uint16);
  static void release(ProcessInfo *);

  bool isValid() const;
  void invalidate();

  void setConnectionName(const char *);
  void setConnectionName(Uint32 *signal_data);
  void setProcessName(const char *);
  void setHostAddress(const struct sockaddr *, socklen_t);
  void setHostAddress(const struct in_addr *);
  void setHostAddress(Uint32 *signal_data);
  void setHostAddress(const char *);
  void setPid();
  void setAngelPid(Uint32 pid);
  void setPort(Uint16);
  void setNodeId(Uint16);
  const char * getConnectionName() const { return connection_name;   };
  const char * getProcessName() const    { return process_name;      };
  const char * getHostAddress() const    { return host_address;      };
  int getPid() const;
  int getAngelPid() const                { return angel_process_id;  };
  int getPort() const                    { return application_port;  };
  int getNodeId() const                  { return node_id;           };


  /* Interface for Qmgr to build ProcessInfo for remote node
     from received signal */
  void initializeFromProcessInfoRep(ProcessInfoRep *);

  STATIC_CONST( ConnectionNameLength = 128 );
  STATIC_CONST( ConnectionNameLengthInWords = 32);
  STATIC_CONST( ProcessNameLength = 48 );
  STATIC_CONST( AddressStringLength = 48 );  // Long enough for IPv6
  STATIC_CONST( AddressStringLengthInWords = 12);

  /* Interface for ClusterManager to create signal */
  void buildProcessInfoReport(ProcessInfoRep *);


private:              /* Data Members */
  char connection_name[ConnectionNameLength];
  char host_address[AddressStringLength];
  char process_name[ProcessNameLength];
  Uint32 node_id;
  Uint32 process_id;
  Uint32 angel_process_id;
  Uint32 application_port;
};   // 240 bytes per node


inline bool ProcessInfo::isValid() const {
  return process_id;
}

#endif
