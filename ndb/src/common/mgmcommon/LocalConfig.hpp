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

#ifndef LocalConfig_H
#define LocalConfig_H

#include <ndb_global.h>
#include <NdbOut.hpp>

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
  union {
    struct {
      char * remoteHost;
      unsigned int port;
    } tcp;
    struct {
      char * filename;
    } file;
  } data;
};

struct LocalConfig {

  int _ownNodeId;

  int size;
  int items;
  MgmtSrvrId ** ids;

  int error_line;
  char error_msg[256];

  LocalConfig();
  ~LocalConfig();
  bool init(bool onlyNodeId = false,
	    const char *connectString = 0,
	    const char *fileName = 0,
	    const char *defaultConnectString = 0);

  void add(MgmtSrvrId *i);

  void printError() const;
  void printUsage() const;

  void setError(int lineNumber, const char * _msg);
  bool readConnectString(const char * connectString, bool onlyNodeId = false);  
  bool readFile(const char * filename, bool &fopenError, bool onlyNodeId = false);
  bool parseLine(char * line, int lineNumber);

  bool parseNodeId(const char *buf);
  bool parseHostName(const char *buf);
  bool parseFileName(const char *buf);
  bool parseString(const char *buf, bool onlyNodeId, char *line);
};

#endif // LocalConfig_H

