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

#include "NodeLogLevel.hpp"
// TODO_RONM: Clearly getCategory and getLevel is not correctly coded. Must be taken care of.

NodeLogLevel::NodeLogLevel(int nodeId, const SetLogLevelOrd& ll)
{
  m_nodeId   = nodeId;
  m_logLevel = ll;
}

NodeLogLevel::~NodeLogLevel() 
{
}

int 
NodeLogLevel::getNodeId() const 
{
  return m_nodeId;
}

Uint32 
NodeLogLevel::getCategory() const
{
  for (Uint32 i = 0; i < m_logLevel.noOfEntries; i++)
  {
    return m_logLevel.theCategories[i];
  }
  return 0;
}

int 
NodeLogLevel::getLevel() const
{
  for (Uint32 i = 0; i < m_logLevel.noOfEntries; i++)
  {
    return m_logLevel.theLevels[i];
  }
  return 0;
}

void
NodeLogLevel::setLevel(int level)  
{
  for (Uint32 i = 0; i < m_logLevel.noOfEntries; i++)
  {
    m_logLevel.theLevels[i] = level;
  }

}

SetLogLevelOrd 
NodeLogLevel::getLogLevelOrd() const
{
  return m_logLevel;
}
