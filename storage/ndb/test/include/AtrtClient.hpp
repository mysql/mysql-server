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

#ifndef ATRT_CLIENT_HPP
#define ATRT_CLIENT_HPP

#include <DbUtil.hpp>

class AtrtClient: public DbUtil {
public:

  enum AtrtCommandType {
    ATCT_CHANGE_VERSION= 1,
    ATCT_RESET_PROC= 2
  };

  AtrtClient(const char* _user= "root",
             const char* _password= "",
             const char* _suffix= ".1.atrt");
  AtrtClient(MYSQL*);
  ~AtrtClient();


  // Command functions
  bool changeVersion(int process_id, const char* process_args);
  bool resetProc(int process_id);

  // Query functions
  bool getConnectString(int cluster_id, SqlResultSet& result);
  bool getClusters(SqlResultSet& result);
  bool getMgmds(int cluster_id, SqlResultSet& result);
  bool getNdbds(int cluster_id, SqlResultSet& result);

private:
  int writeCommand(AtrtCommandType _type,
                   const Properties& args);
  bool readCommand(uint command_id,
                   SqlResultSet& result);

  bool doCommand(AtrtCommandType _type,
                 const Properties& args);
};

#endif
