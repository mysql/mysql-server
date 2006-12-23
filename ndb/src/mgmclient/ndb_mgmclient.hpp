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

#ifndef Ndb_mgmclient_hpp
#define Ndb_mgmclient_hpp

class CommandInterpreter;
class Ndb_mgmclient
{
public:
  Ndb_mgmclient(const char*,int verbose=0);
  ~Ndb_mgmclient();
  int execute(const char *_line, int _try_reconnect=-1, bool interactive=1, int *error= 0);
  int execute(int argc, char** argv, int _try_reconnect=-1, bool interactive=1, int *error= 0);
  int disconnect();
private:
  CommandInterpreter *m_cmd;
};

#endif // Ndb_mgmclient_hpp
