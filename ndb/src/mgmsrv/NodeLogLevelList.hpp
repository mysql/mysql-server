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

#ifndef NODELOGLEVELLIST_H
#define NODELOGLEVELLIST_H

class NodeLogLevel;

/**
 * Provides a simple linked list of NodeLogLevel.
 *
 * @see NodeLogLevel
 * @version #@ $Id: NodeLogLevelList.hpp,v 1.1 2002/08/09 12:53:50 eyualex Exp $
 */
class NodeLogLevelList
{
public:
  /**
   * Default Constructor.
   */
  NodeLogLevelList();

  /**
   * Destructor.
   */
  ~NodeLogLevelList();

  /**
   * Adds a new node.
   *
   * @param pNewHandler a new NodeLogLevel.
   */
  void add(NodeLogLevel* pNewNode);

  /**
   * Removes a NodeLogLevel from the list and call its destructor.
   *
   * @param pRemoveHandler the NodeLogLevel to remove
   */
  bool remove(NodeLogLevel* pRemoveNode);

  /**
   * Removes all items.
   */
  void removeAll();

  /**
   * Returns the next node in the list. 
   * returns a node or NULL.
   */
  NodeLogLevel* next();

  /**
   * Returns the size of the list.
   */ 
  int size() const;
private:
  /** List node */
  struct NodeLogLevelNode
  {
    NodeLogLevelNode* pPrev;
    NodeLogLevelNode* pNext;    
    NodeLogLevel* pHandler;
  };

  NodeLogLevelNode* next(NodeLogLevelNode* pNode);
  NodeLogLevelNode* prev(NodeLogLevelNode* pNode);

  void removeNode(NodeLogLevelNode* pNode);

  int m_size;

  NodeLogLevelNode* m_pHeadNode;
  NodeLogLevelNode* m_pTailNode;
  NodeLogLevelNode* m_pCurrNode;
};

#endif


