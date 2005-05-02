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

#ifndef MemoryChannelOSE_H
#define MemoryChannelOSE_H

//===========================================================================
//
// .DESCRIPTION
//              Pointer based communication channel for communication between two 
//              thread. It sends the pointer to the other signal via an OSE signal 
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
//      size : is ignored in OSE version
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

#include <ose.h>
#include "ErrorHandlingMacros.hpp"
#include "Error.hpp"
#include "NdbMutex.h"
#include "NdbCondition.h"





template <class T>
class MemoryChannel
{
public:
  MemoryChannel( int size= 256);
  virtual ~MemoryChannel( );

  virtual void writeChannel( T *t);
  T* readChannel();
  T* tryReadChannel();

private:
  PROCESS theReceiverPid;
};

template <class T> class MemoryChannelMultipleWriter:public MemoryChannel<T>
{
public:
  MemoryChannelMultipleWriter( int size= 256);
  ~MemoryChannelMultipleWriter( );
  void writeChannel( T *t);

private:
};


#define MEMCHANNEL_SIGBASE  5643
   
#define MEMCHANNEL_SIGNAL           (MEMCHANNEL_SIGBASE + 1)  /* !-SIGNO(struct MemChannelSignal)-! */  


struct MemChannelSignal
{
  SIGSELECT sigNo;
  void* ptr;
};

union SIGNAL 
{
  SIGSELECT sigNo;
  struct MemChannelSignal          memChanSig;
};

template <class T> MemoryChannel<T>::MemoryChannel( int size )
{
  // Default receiver for this channel is the creating process
  theReceiverPid = current_process();
}

template <class T> MemoryChannel<T>::~MemoryChannel( )
{
}

template <class T> void MemoryChannel<T>::writeChannel( T *t)
{
  union SIGNAL* sig;

  sig = alloc(sizeof(struct MemChannelSignal), MEMCHANNEL_SIGNAL);
  ((struct MemChannelSignal*)sig)->ptr = t;
  send(&sig, theReceiverPid);
}


template <class T> T* MemoryChannel<T>::readChannel()
{
  T* tmp;

  static const SIGSELECT sel_mem[] = {1, MEMCHANNEL_SIGNAL};
  union SIGNAL* sig;

  tmp = NULL; /* Default value */

  sig = receive((SIGSELECT*)sel_mem);
  if (sig != NIL){
    if (sig->sigNo == MEMCHANNEL_SIGNAL){
      tmp = (T*)(((struct MemChannelSignal*)sig)->ptr);
    }else{
      assert(1==0);
    }
    free_buf(&sig);    
  }

  return tmp;
}

template <class T> T* MemoryChannel<T>::tryReadChannel()
{
  T* tmp;

  static const SIGSELECT sel_mem[] = {1, MEMCHANNEL_SIGNAL};
  union SIGNAL* sig;

  tmp = NULL; /* Default value */

  sig = receive_w_tmo(0, (SIGSELECT*)sel_mem);
  if (sig != NIL){
    if (sig->sigNo == MEMCHANNEL_SIGNAL){
      tmp = (T*)(((struct MemChannelSignal*)sig)->ptr);
    }else{
      assert(1==0);
    }
    free_buf(&sig);    
  }

  return tmp;
}


#endif // MemoryChannel_H
























