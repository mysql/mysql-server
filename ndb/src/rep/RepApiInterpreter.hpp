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

#ifndef REP_API_INTERPRETER_HPP
#define REP_API_INTERPRETER_HPP

#include <editline/editline.h>

#include <rep/RepComponents.hpp>
#include <rep/state/RepState.hpp>
#include <rep/RepApiService.hpp>
#include <signaldata/GrepImpl.hpp>
#include <Properties.hpp>

/**
 * @class RepCommandInterpreter
 * @brief
 */

class RepApiInterpreter {
public:
  RepApiInterpreter(class RepComponents * comps, int port); 
  ~RepApiInterpreter();
  void startInterpreter();
  void stopInterpreter();
  Properties * execCommand(const Properties & props);
  Properties * getStatus();
  Properties * query(Uint32 counter, Uint32 replicationId);
  bool readAndExecute();

private:
  char *  readline_gets() const; 
  void    request(Uint32 request);
  int   m_port;
  class RepComponents *  m_repComponents;
  class RepState *       m_repState;
  SocketServer * ss;  
  RepApiService * serv;
};

#endif
