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

#ifndef NdbReceiver_H
#define NdbReceiver_H
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL  // Not part of public interface

#include <ndb_types.h>

class Ndb;
class NdbReceiver
{
public:
  enum ReceiverType	{ NDB_UNINITIALIZED,
			  NDB_OPERATION = 1,
			  NDB_SCANRECEIVER = 2,
			  NDB_INDEX_OPERATION = 3
  };
  
  NdbReceiver(Ndb *aNdb);
  void init(ReceiverType type, void* owner);
  ~NdbReceiver();
  
  Uint32 getId(){
    return m_id;
  }

  ReceiverType getType(){
    return m_type;
  }
  
  void* getOwner(){
    return m_owner;
  }
  
  bool checkMagicNumber() const;

private:
  Uint32 theMagicNumber;
  Ndb* m_ndb;
  Uint32 m_id;
  ReceiverType m_type;
  void* m_owner;
};

#ifdef NDB_NO_DROPPED_SIGNAL
#include <stdlib.h>
#endif

inline
bool 
NdbReceiver::checkMagicNumber() const {
  bool retVal = (theMagicNumber == 0x11223344);
#ifdef NDB_NO_DROPPED_SIGNAL
  if(!retVal){
    abort();
  }
#endif
  return retVal;
}

#endif
#endif
