/*
   Copyright (C) 2003-2006 MySQL AB
    All rights reserved. Use is subject to license terms.

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

#ifndef NDBTIMER_H
#define NDBTIMER_H

#include <NdbTick.h>
#include <NdbOut.hpp>

// 
// Class used for measuring time and priting the results
// 
// Currently measures time in milliseconds
// 

class NdbTimer
{
public:

  NdbTimer();
  ~NdbTimer() {};

  void doStart();
  void doStop();
  void doReset();
  NDB_TICKS elapsedTime();
  void printTransactionStatistics(const char* text, 
				  int numTransactions, 
				  int numOperations);
  void printTestTimer(int numLoops, 
		      int numRecords);
  void printTotalTime(void);
private:
  NDB_TICKS startTime;
  NDB_TICKS stopTime;
};

inline NdbTimer::NdbTimer(){
  doReset();
}

inline void NdbTimer::doReset(void){
  startTime = 0;
  stopTime = 0;
}

inline void NdbTimer::doStart(void){
  startTime = NdbTick_CurrentMillisecond();
}

inline void NdbTimer::doStop(void){
  stopTime = NdbTick_CurrentMillisecond();
}

inline NDB_TICKS NdbTimer::elapsedTime(void){
  return (stopTime - startTime); 
}

inline void NdbTimer::printTransactionStatistics(const char* text, 
						 int numTransactions, 
						 int numOperations){

  // Convert to Uint32 in order to be able to print it to screen
  Uint32 lapTime = (Uint32)elapsedTime();
  ndbout_c("%i transactions, %i %s total time = %d ms\nAverage %f ms/transaction, %f ms/%s.\n%f transactions/second, %f %ss/second.\n",
	 numTransactions, numTransactions*numOperations, text, lapTime,
         ((double)lapTime/numTransactions), ((double)lapTime/(numTransactions*numOperations)), text, 
         1000.0/((double)lapTime/numOperations), 1000.0/((double)lapTime/(numTransactions*numOperations)), text);
}



inline void NdbTimer::printTestTimer(int numLoops, 
				     int numRecords){
  // Convert to Uint32 in order to be able to print it to screen
  Uint32 lapTime = (Uint32)elapsedTime();
  ndbout_c("%i loop * %i records, total time = %d ms\nAverage %f ms/loop, %f ms/record.\n%f looop/second, %f records/second.\n",
	   numLoops, numRecords, lapTime,
	   ((double)lapTime/numLoops), ((double)lapTime/(numLoops*numRecords)),
	   1000.0/((double)lapTime/numLoops), 1000.0/((double)lapTime/(numLoops*numRecords)));
}


inline void NdbTimer::printTotalTime(void){
  // Convert to Uint32 in order to be able to print it to screen
  Uint32 lapTime = (Uint32)elapsedTime();
  Uint32 secTime = lapTime/1000;
  ndbout_c("Total time : %d seconds (%d ms)\n", secTime, lapTime);
}






#endif
