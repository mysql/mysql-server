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

#ifndef NEWTON_BP_HPP
#define NEWTON_BP_HPP

#include "dba_internal.hpp"

#include <NdbThread.h>
#include <NdbMutex.h>
#include <NdbSleep.h>

extern "C" void* runNDB_C(void * nbp);

/**
 * This class implements the NewtonBatchProcess
 */
class NewtonBatchProcess {
  friend void* runNDB_C(void * nbp);
public:
  NewtonBatchProcess(Ndb &, NdbMutex &);
  ~NewtonBatchProcess();
  
  void doStart();
  void doStop(bool wait);
  
  bool isRunning() const ;
  bool isStopping() const ;
  
private:
  void run();

  bool _running;
  bool _stop;
  
  Ndb & theNdb;
  NdbMutex & theMutex;
  
  NdbThread * theThread;
  NdbMutex * startStopMutex;
};

#endif
