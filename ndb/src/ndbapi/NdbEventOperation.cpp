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
 * Name:          NdbEventOperation.cpp
 * Include:
 * Link:
 * Author:        Tomas Ulin MySQL AB
 * Date:          2003-11-21
 * Version:       0.1
 * Description:   Event support
 * Documentation:
 * Adjust:  2003-11-21  Tomas Ulin   First version.
 ****************************************************************************/

#include <Ndb.hpp>
#include <signaldata/SumaImpl.hpp>
#include <portlib/NdbMem.h>
#include <transporter/TransporterDefinitions.hpp>
#include <NdbEventOperation.hpp>
#include "NdbEventOperationImpl.hpp"
#include "NdbDictionaryImpl.hpp"

NdbEventOperation::NdbEventOperation(Ndb *theNdb, 
				     const char* eventName, 
				     int bufferLength) 
  : m_impl(* new NdbEventOperationImpl(*this,theNdb, 
				       eventName, 
				       bufferLength))
{
}

NdbEventOperation::~NdbEventOperation()
{
  NdbEventOperationImpl * tmp = &m_impl;
  if (this != tmp)
    delete tmp;
}

NdbEventOperation::State NdbEventOperation::getState()
{
  return m_impl.getState();
}

NdbRecAttr *
NdbEventOperation::getValue(const char *colName, char *aValue)
{
  return m_impl.getValue(colName, aValue, 0);
}

NdbRecAttr *
NdbEventOperation::getPreValue(const char *colName, char *aValue)
{
  return m_impl.getValue(colName, aValue, 1);
}

int
NdbEventOperation::execute()
{
  return m_impl.execute();
}

int
NdbEventOperation::next(int *pOverrun)
{
  return m_impl.next(pOverrun);
}

bool
NdbEventOperation::isConsistent()
{
  return m_impl.isConsistent();
}

Uint32
NdbEventOperation::getGCI()
{
  return m_impl.getGCI();
}

Uint32
NdbEventOperation::getLatestGCI()
{
  return m_impl.getLatestGCI();
}

NdbDictionary::Event::TableEvent
NdbEventOperation::getEventType()
{
  return m_impl.getEventType();
}

void
NdbEventOperation::print()
{
  m_impl.print();
}

/*
 * Private members
 */

int
NdbEventOperation::wait(void *p, int aMillisecondNumber)
{
  return NdbEventOperationImpl::wait(p, aMillisecondNumber);
}

NdbEventOperation::NdbEventOperation(NdbEventOperationImpl& impl) 
  : m_impl(impl) {};

