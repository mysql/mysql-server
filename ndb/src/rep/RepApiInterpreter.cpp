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

#include "RepApiInterpreter.hpp"
#include <signaldata/GrepImpl.hpp>

RepApiInterpreter::RepApiInterpreter(RepComponents * comps, int port)
{
  m_repComponents = comps;
  m_repState = comps->getRepState();
  m_port = port;
  ss = new SocketServer();
  serv = new RepApiService(*this);
}


RepApiInterpreter::~RepApiInterpreter()
{
}

void
RepApiInterpreter::startInterpreter() 
{
  if(!ss->setup(serv, m_port)){
    sleep(1);
    delete ss;
    delete serv;
  }
  ss->startServer();
}


void
RepApiInterpreter::stopInterpreter() 
{
  delete ss;
}


Properties *
RepApiInterpreter::execCommand(const Properties & props)
{
  Properties * result = new Properties();
  Uint32 req = 0;
  Uint32 epoch = 0;
  props.get("request", &req);
  props.get("epoch", &epoch);
  GrepError::Code err = m_repState->protectedRequest((GrepReq::Request)req, 
						     epoch);
  result->put("err", err);
  return result;
}

Properties *
RepApiInterpreter::getStatus()
{

  return m_repState->getStatus();
}


Properties * 
RepApiInterpreter::query(Uint32 counter, Uint32 replicationId)
{
  return m_repState->query((QueryCounter)counter, replicationId);
}

