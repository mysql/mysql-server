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

#include "NdbImpl.hpp"
#include <NdbReceiver.hpp>

NdbReceiver::NdbReceiver(Ndb *aNdb) :
  theMagicNumber(0),
  m_ndb(aNdb),
  m_id(NdbObjectIdMap::InvalidId),
  m_type(NDB_UNINITIALIZED),
  m_owner(0)
{
}
 
void
NdbReceiver::init(ReceiverType type, void* owner)
{
  theMagicNumber = 0x11223344;
  m_type = type;
  m_owner = owner;
  if (m_id == NdbObjectIdMap::InvalidId) {
    if (m_ndb)
      m_id = m_ndb->theNdbObjectIdMap->map(this);
  }
}
  
NdbReceiver::~NdbReceiver()
{
  if (m_id != NdbObjectIdMap::InvalidId) {
    m_ndb->theNdbObjectIdMap->unmap(m_id, this);
  }
}
