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

/*****************************************************************************
 * Name:          NdbCursorOperation.cpp
 * Include:
 * Link:
 * Author:        UABMASD Martin Sköld INN/V Alzato
 * Date:          2002-04-01
 * Version:       0.1
 * Description:   Cursor support
 * Documentation:
 * Adjust:  2002-04-01  UABMASD   First version.
 ****************************************************************************/

#include <NdbCursorOperation.hpp>
#include <NdbResultSet.hpp>

NdbCursorOperation::NdbCursorOperation(Ndb* aNdb) :
{
}

NdbCursorOperation::~NdbCursorOperation()
{
  if (m_resultSet)
    delete m_resultSet;
}

void NdbCursorOperation::cursInit()
{
  // Initialize result set
}

NdbResultSet* NdbCursorOperation::getResultSet()
{
}


