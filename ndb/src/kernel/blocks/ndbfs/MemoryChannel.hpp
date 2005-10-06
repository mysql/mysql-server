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

#ifndef MemoryChannel_H
#define MemoryChannel_H

//===========================================================================
//
// .DESCRIPTION
//              Pointer based communication channel for communication between two 
//              thread. It does not copy any data in or out the channel so the 
//              item that is put in can not be used untill the other thread has 
//              given it back. There is no support for detecting the return of a 
//              item. The channel is half-duplex. 
//              For comminication between 1 writer and 1 reader use the MemoryChannel
//              class, for comminication between multiple writer and 1 reader use the
//              MemoryChannelMultipleWriter. There is no support for multiple readers.
//
// .TYPICAL USE:
//              to communicate between threads.
//
// .EXAMPLE:
//              See AsyncFile.C
//===========================================================================
//
//
// MemoryChannel( int size= 256);
//   Constuctor
// Parameters:
//      size : amount of pointer it can hold
//
// void operator ++ ();
//   increments the index with one, if size is reached it is set to zero
//
// virtual void write( T *t);
//   Puts the item in the channel if the channel is full an error is reported.
// Parameters:
//      t: pointer to item to put in the channel, after this the item
//                        is shared with the other thread.
// errors
//                      AFS_ERROR_CHANNALFULL, channel is full
//
// T* read();
//      Reads a itemn from the channel, if channel is empty it blocks untill
//              an item can be read.
// return
//                      T : item from the channel
//
// T* tryRead();
//      Reads a item from the channel, if channel is empty it returns zero.
// return
//                      T : item from the channel or zero if channel is empty.
//

#if defined NDB_OSE || defined NDB_SOFTOSE
#include "MemoryChannelOSE.hpp"
#else

#include "ErrorHandlingMacros.hpp"
#include "CircularIndex.hpp"
#include "NdbMutex.h"
#include "NdbCondition.h"
#include <NdbOut.hpp>


template <class T>
class MemoryChannel
{
public:
  MemoryChannel( int size= 256);
  virtual ~MemoryChannel( );

  void writeChannel( T *t);
  void writeChannelNoSignal( T *t);
  T* readChannel();
  T* tryReadChannel();

private:
  int theSize;
  T **theChannel;
  CircularIndex theWriteIndex;
  CircularIndex theReadIndex;
  NdbMutex* theMutexPtr;
  NdbCondition* theConditionPtr;

  template<class U>
  friend NdbOut& operator<<(NdbOut& out, const MemoryChannel<U> & chn);
};

template <class T>
NdbOut& operator<<(NdbOut& out, const MemoryChannel<T> & chn)
{
  NdbMutex_Lock(chn.theMutexPtr);
  out << "[ theSize: " << chn.theSize
      << " theReadIndex: " << (int)chn.theReadIndex 
      << " theWriteIndex: " << (int)chn.theWriteIndex << " ]";
  NdbMutex_Unlock(chn.theMutexPtr);
  return out;
}

template <class T> MemoryChannel<T>::MemoryChannel( int size):
        theSize(size),
        theChannel(new T*[size] ),
        theWriteIndex(0, size),
        theReadIndex(0, size)
{
  theMutexPtr = NdbMutex_Create();
  theConditionPtr = NdbCondition_Create();
}

template <class T> MemoryChannel<T>::~MemoryChannel( )
{
  NdbMutex_Destroy(theMutexPtr);
  NdbCondition_Destroy(theConditionPtr);
  delete [] theChannel;
}

template <class T> void MemoryChannel<T>::writeChannel( T *t)
{

  NdbMutex_Lock(theMutexPtr);
  if(full(theWriteIndex, theReadIndex) || theChannel == NULL) abort();
  theChannel[theWriteIndex]= t;
  ++theWriteIndex;
  NdbMutex_Unlock(theMutexPtr);
  NdbCondition_Signal(theConditionPtr);
}

template <class T> void MemoryChannel<T>::writeChannelNoSignal( T *t)
{

  NdbMutex_Lock(theMutexPtr);
  if(full(theWriteIndex, theReadIndex) || theChannel == NULL) abort();
  theChannel[theWriteIndex]= t;
  ++theWriteIndex;
  NdbMutex_Unlock(theMutexPtr);
}

template <class T> T* MemoryChannel<T>::readChannel()
{
  T* tmp;

  NdbMutex_Lock(theMutexPtr);
  while ( empty(theWriteIndex, theReadIndex) )
  {
    NdbCondition_Wait(theConditionPtr,
                        theMutexPtr);    
  }
        
  tmp= theChannel[theReadIndex];
  ++theReadIndex;
  NdbMutex_Unlock(theMutexPtr);
  return tmp;
}

template <class T> T* MemoryChannel<T>::tryReadChannel()
{
  T* tmp= 0;
  NdbMutex_Lock(theMutexPtr);
  if ( !empty(theWriteIndex, theReadIndex) )
  {     
    tmp= theChannel[theReadIndex];
    ++theReadIndex;
  }
  NdbMutex_Unlock(theMutexPtr);
  return tmp;
}

#endif

#endif // MemoryChannel_H

