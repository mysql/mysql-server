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

#ifndef CircularIndex_H
#define CircularIndex_H

//===========================================================================
//
// .DESCRIPTION
//              Building block for circular buffers. It increment as a normal index.
//              untill it it becomes the maximum size then it becomes zero. 
//
// .TYPICAL USE:
//              to implement a circular buffer.
//
// .EXAMPLE:
//              See MemoryChannel.C
//===========================================================================

///////////////////////////////////////////////////////////////////////////////
// CircularIndex(  int start= 0,int size=256 );
//   Constuctor
// Parameters:
//      start: where to start to index
//      size : range of the index, will be from 0 to size-1
///////////////////////////////////////////////////////////////////////////////
// operator int ();
//   returns the index
///////////////////////////////////////////////////////////////////////////////
// void operator ++ ();
//   increments the index with one, of size is reached it is set to zero
///////////////////////////////////////////////////////////////////////////////
// friend int full( const CircularIndex& write, const CircularIndex& read );
//   Taken the write index and the read index from a buffer it is calculated
//              if the buffer is full
// Parameters:
//      write: index used a write index for the buffer
//      read : index used a read index for the buffer
// return
//      0 : not full
//      1 : full
///////////////////////////////////////////////////////////////////////////////
// friend int empty( const CircularIndex& write, const CircularIndex& read );
//   Taken the write index and the read index from a buffer it is calculated
//              if the buffer is empty
// Parameters:
//      write: index used a write index for the buffer
//      read : index used a read index for the buffer
// return
//      0 : not empty
//      1 : empty
///////////////////////////////////////////////////////////////////////////////

class CircularIndex
{
public:
  inline CircularIndex(  int start= 0,int size=256 );
  operator int () const;
  CircularIndex& operator ++ ();
  friend int full( const CircularIndex& write, const CircularIndex& read );
  friend int empty( const CircularIndex& write, const CircularIndex& read );
private:
  int theSize;
  int theIndex;
};

inline CircularIndex::operator int () const
{
  return theIndex;
}

inline CircularIndex& CircularIndex::operator ++ ()
{
  ++theIndex;
  if( theIndex >= theSize ){
    theIndex= 0;
  }
  return *this;
}


inline int full( const CircularIndex& write, const CircularIndex& read )
{
  int readTmp= read.theIndex;

  if( read.theIndex < write.theIndex  )
    readTmp += read.theSize;

  return ( readTmp - write.theIndex) == 1;
}

inline int empty( const CircularIndex& write, const CircularIndex& read )
{
  return read.theIndex == write.theIndex;
}


inline CircularIndex::CircularIndex(  int start,int size ):
  theSize(size),
  theIndex(start)
{
}
#endif
