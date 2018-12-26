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

#include "LogHandlerList.hpp"

#include <LogHandler.hpp>

//
// PUBLIC
//

LogHandlerList::LogHandlerList() :
  m_size(0),
  m_pHeadNode(NULL),
  m_pTailNode(NULL),
  m_pCurrNode(NULL)
{
}

LogHandlerList::~LogHandlerList()
{
  removeAll();
}

bool
LogHandlerList::add(LogHandler* pNewHandler)
{
  LogHandlerNode* pNode = new LogHandlerNode();
  if (!pNode)
    return false;

  if (m_pHeadNode == NULL) 
  {
    m_pHeadNode = pNode;
    pNode->pPrev = NULL;
  }
  else 
  {
    m_pTailNode->pNext = pNode;
    pNode->pPrev = m_pTailNode;
  }
  m_pTailNode = pNode;
  pNode->pNext = NULL;  
  pNode->pHandler = pNewHandler;

  m_size++;

  return true;
}

bool
LogHandlerList::remove(LogHandler* pRemoveHandler)
{
  LogHandlerNode* pNode = m_pHeadNode;
  bool removed = false;
  do
  {
    if (pNode->pHandler == pRemoveHandler)
    {
      removeNode(pNode);
      removed = true;
      break;
    }
  } while ( (pNode = next(pNode)) != NULL);

  return removed;
}

void 
LogHandlerList::removeAll()
{
  while (m_pHeadNode != NULL)
  {
    removeNode(m_pHeadNode);
  }
}

LogHandler* 
LogHandlerList::next()
{
  LogHandler* pHandler = NULL;
  if (m_pCurrNode == NULL)
  {
    m_pCurrNode = m_pHeadNode;
    if (m_pCurrNode != NULL)
    {
      pHandler = m_pCurrNode->pHandler;
    }
  }
  else
  {
    m_pCurrNode = next(m_pCurrNode); // Next node    
    if (m_pCurrNode != NULL)
    {
      pHandler = m_pCurrNode->pHandler;
    }
  }
 
  return pHandler;
}

int 
LogHandlerList::size() const
{
  return m_size;
}

//
// PRIVATE
//

LogHandlerList::LogHandlerNode* 
LogHandlerList::next(LogHandlerNode* pNode)
{
  LogHandlerNode* pCurr = pNode;
  if (pNode->pNext != NULL) 
  {
    pCurr = pNode->pNext;
  }
  else
  {
    // Tail
    pCurr = NULL;    
  }
  return pCurr;
}

LogHandlerList::LogHandlerNode* 
LogHandlerList::prev(LogHandlerNode* pNode)
{
  LogHandlerNode* pCurr = pNode;
  if (pNode->pPrev != NULL) // head
  {
    pCurr = pNode->pPrev;
  }
  else
  {
    // Head
    pCurr = NULL;
  }

  return pCurr;
}

void
LogHandlerList::removeNode(LogHandlerNode* pNode)
{
  if (pNode->pPrev == NULL) // If head
  {
    m_pHeadNode = pNode->pNext;
  }
  else 
  {
    pNode->pPrev->pNext = pNode->pNext;
  }

  if (pNode->pNext == NULL) // if tail
  {
    m_pTailNode = pNode->pPrev;
  }
  else
  {
    pNode->pNext->pPrev = pNode->pPrev;
  }

  pNode->pNext = NULL;
  pNode->pPrev = NULL;
  delete pNode->pHandler; // Delete log handler
  delete pNode; 

  m_size--;
}
