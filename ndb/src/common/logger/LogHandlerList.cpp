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

void 
LogHandlerList::add(LogHandler* pNewHandler)
{
  LogHandlerNode* pNode = new LogHandlerNode();

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
