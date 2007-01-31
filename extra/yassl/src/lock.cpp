/*
   Copyright (C) 2000-2007 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING. If not, write to the
   Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston,
   MA  02110-1301  USA.
*/

/*  Locking functions
 */

#include "runtime.hpp"
#include "lock.hpp"


namespace yaSSL {


#ifdef MULTI_THREADED
    #ifdef _WIN32
        
        Mutex::Mutex()
        {
            InitializeCriticalSection(&cs_);
        }


        Mutex::~Mutex()
        {
            DeleteCriticalSection(&cs_);
        }

            
        Mutex::Lock::Lock(Mutex& lm) : mutex_(lm)
        {
            EnterCriticalSection(&mutex_.cs_); 
        }


        Mutex::Lock::~Lock()
        {
            LeaveCriticalSection(&mutex_.cs_); 
        }
            
    #else  // _WIN32
        
        Mutex::Mutex()
        {
            pthread_mutex_init(&mutex_, 0);
        }


        Mutex::~Mutex()
        {
            pthread_mutex_destroy(&mutex_);
        }


        Mutex::Lock::Lock(Mutex& lm) : mutex_(lm)
        {
            pthread_mutex_lock(&mutex_.mutex_); 
        }


        Mutex::Lock::~Lock()
        {
            pthread_mutex_unlock(&mutex_.mutex_); 
        }
         

    #endif // _WIN32
#endif // MULTI_THREADED



} // namespace yaSSL

