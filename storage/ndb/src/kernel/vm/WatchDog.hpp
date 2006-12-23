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

#ifndef WatchDog_H
#define WatchDog_H

#include <kernel_types.h>
#include <NdbThread.h>

extern "C" void* runWatchDog(void* w);

class WatchDog{
public:
  WatchDog(Uint32 interval = 3000);
  ~WatchDog();
 
  void doStart();
  void doStop();

  Uint32 setCheckInterval(Uint32 interval);
  
protected:
  /**
   * Thread function
   */
  friend void* runWatchDog(void* w);
  
  /**
   * Thread pointer 
   */
  NdbThread* theThreadPtr;
  
private:
  Uint32 theInterval;
  const Uint32 * theIPValue;
  
  bool theStop;
  
  void run();
  void shutdownSystem(const char *last_stuck_action);
};

#endif // WatchDog_H
