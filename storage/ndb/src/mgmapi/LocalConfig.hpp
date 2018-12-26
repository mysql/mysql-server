/*
   Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

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

#ifndef LocalConfig_H
#define LocalConfig_H

#include <ndb_global.h>
#include <BaseString.hpp>

//****************************************************************************
// Description: The class LocalConfig corresponds to the information possible
// to give in the local configuration file.
//*****************************************************************************

enum MgmtSrvrId_Type {
  MgmId_TCP  = 0,
  MgmId_File = 1
};

struct MgmtSrvrId {
  MgmtSrvrId_Type type;
  BaseString name;
  unsigned int port;
  BaseString bind_address;
  unsigned int bind_address_port;
};

struct LocalConfig {

  int _ownNodeId;
  Vector<MgmtSrvrId> ids;
  
  int error_line;
  char error_msg[256];

  BaseString bind_address;
  unsigned int bind_address_port;

  LocalConfig();
  ~LocalConfig();
  bool init(const char *connectString = 0,
	    const char *fileName = 0);

  void setError(int lineNumber, const char * _msg);
  bool readConnectString(const char *, const char *info);
  bool readFile(const char * file, bool &fopenError);
  bool parseLine(char * line, int lineNumber);
  
  bool parseNodeId(const char *buf);
  bool parseHostName(const char *buf);
  bool parseBindAddress(const char *buf);
  bool parseFileName(const char *buf);
  bool parseString(const char *buf, BaseString &err);
  char * makeConnectString(char *buf, int sz);
};

#endif // LocalConfig_H

