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

#ifndef NODELOGLEVEL_H
#define NODELOGLEVEL_H

#include <ndb_global.h>

#include <signaldata/SetLogLevelOrd.hpp>

/**
 * Holds a DB node's log level settings for both local and event log levels.
 * It only holds one log level setting even though SetLogLevelOrd can handle
 * multiple log levels at once, it is not used in that way in the managment 
 * server.
 *
 * @version #@ $Id: NodeLogLevel.hpp,v 1.2 2003/07/05 17:40:22 elathal Exp $
 */
class NodeLogLevel
{ 
public:
  NodeLogLevel(int nodeId, const SetLogLevelOrd& ll);
  ~NodeLogLevel();

  int getNodeId() const;
  Uint32 getCategory() const;
  int getLevel() const;
  void setLevel(int level);
  SetLogLevelOrd getLogLevelOrd() const;

private:
  NodeLogLevel();
  NodeLogLevel(const NodeLogLevel&);
  bool operator == (const NodeLogLevel&);
  NodeLogLevel operator = (const NodeLogLevel&);
  
  int m_nodeId;
  SetLogLevelOrd m_logLevel;
};

#endif
