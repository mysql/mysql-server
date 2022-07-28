/*
   Copyright (c) 2008, 2022, Oracle and/or its affiliates.

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

#ifndef ATRT_CLIENT_HPP
#define ATRT_CLIENT_HPP

#include <DbUtil.hpp>

class AtrtClient: public DbUtil {
public:

  enum AtrtCommandType {
    ATCT_CHANGE_VERSION= 1,
    ATCT_RESET_PROC= 2,
    ATCT_START_PROCESS= 3,
    ATCT_STOP_PROCESS= 4,
    ATCT_SWITCH_CONFIG = 5
  };

  AtrtClient(const char* _suffix= ".1.atrt");
  AtrtClient(MYSQL*);
  ~AtrtClient();


  // Command functions
  bool changeVersion(int process_id, const char* process_args);
  bool switchConfig(int process_id, const char* process_args);
  bool stopProcess(int process_id);
  bool startProcess(int process_id);
  bool resetProc(int process_id);

  // Query functions
  bool getConnectString(int cluster_id, SqlResultSet& result);
  bool getClusters(SqlResultSet& result);
  bool getMgmds(int cluster_id, SqlResultSet& result);
  bool getNdbds(int cluster_id, SqlResultSet& result);
  int getOwnProcessId();

private:
  int writeCommand(AtrtCommandType _type,
                   const Properties& args);
  bool readCommand(uint command_id,
                   SqlResultSet& result);

  bool doCommand(AtrtCommandType _type,
                 const Properties& args);
};

#endif
