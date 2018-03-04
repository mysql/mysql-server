/*
   Copyright (c) 2005, 2012, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.
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

