/*
   Copyright (c) 2003, 2022, Oracle and/or its affiliates.

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

#ifndef LOGHANDLERLIST_H
#define LOGHANDLERLIST_H

class LogHandler;
#include <ndb_global.h>

/**
 * Provides a simple linked list of log handlers.
 *
 * @see LogHandler
 * @version #@ $Id: LogHandlerList.hpp,v 1.2 2002/03/14 13:07:21 eyualex Exp $
 */
class LogHandlerList
{
public:
  /**
   * Default Constructor.
   */
  LogHandlerList();

  /**
   * Destructor.
   */
  ~LogHandlerList();

  /**
   * Adds a new log handler.
   *
   * @param pNewHandler log handler.
   */
  bool add(LogHandler* pNewHandler);

  /**
   * Removes a log handler from the list and call its destructor.
   *
   * @param pRemoveHandler the handler to remove
   */
  bool remove(LogHandler* pRemoveHandler);

  /**
   * Removes all log handlers.
   */
  void removeAll();

  /**
   * Returns the next log handler in the list. 
   * returns a log handler or NULL.
   */
  LogHandler* next();

  /**
   * Returns the size of the list.
   */ 
  int size() const;
private:
  /** List node */
  struct LogHandlerNode
  {
    LogHandlerNode* pPrev;
    LogHandlerNode* pNext;    
    LogHandler* pHandler;
  };

  LogHandlerNode* next(LogHandlerNode* pNode);
  LogHandlerNode* prev(LogHandlerNode* pNode);

  void removeNode(LogHandlerNode* pNode);

  int m_size;

  LogHandlerNode* m_pHeadNode;
  LogHandlerNode* m_pTailNode;
  LogHandlerNode* m_pCurrNode;
};

#endif


