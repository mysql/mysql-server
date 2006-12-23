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


#include <Parser.hpp>
#include <NdbOut.hpp>
#include <Properties.hpp>
#include "SocketService.hpp"

SocketService::SocketService() {

}

SocketService::~SocketService() {

}

int
SocketService::runSession(NDB_SOCKET_TYPE socket, SocketService & ss){
  InputStream *m_input = new SocketInputStream(socket);
  char buf[255];

  m_input->gets(buf,255);
  ndbout_c("SocketService:received: %s\n", buf);
  ndbout_c("This should now be parsed\n");
  ndbout_c("and put in a property object.\n");
  ndbout_c("The propery is then accessible from the ClientInterface.\n");
  ndbout_c("by getPropertyObject.\n");
  ndbout_c("At least this is the idea.");
    /*Parser_t *m_parser = 
      new Parser<SocketService>(commands, *m_input, true, true, true);
      */
  /** to do
   * add a proprty object to which the parser will put its result.
   */
  
  return 1 ; //succesful
  //return 0; //unsuccesful

}

void
SocketService::getPropertyObject() {
  ndbout << "get property object. return to front end or something" << endl;
}


