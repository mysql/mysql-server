/*
   Copyright (c) 2005, 2012, Oracle and/or its affiliates. All rights reserved.

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

/* lock.hpp provides an os specific Lock, locks mutex on entry and unlocks
 * automatically upon exit, no-ops provided for Single Threaded
*/

#ifndef yaSSL_LOCK_HPP
#define yaSSL_LOCK_HPP

/*
  Visual Studio Source Annotations header (sourceannotations.h) fails
  to compile if outside of the global namespace.
*/
#ifdef MULTI_THREADED
#ifdef _WIN32
#include <windows.h>
#endif
#endif

namespace yaSSL {


#ifdef MULTI_THREADED
    #ifdef _WIN32
        #include <windows.h>

        class Mutex {
            CRITICAL_SECTION cs_;
        public:
            Mutex();
            ~Mutex();

            class Lock;
            friend class Lock;
    
            class Lock {
                Mutex& mutex_;
            public:
                explicit Lock(Mutex& lm);
                ~Lock();
            };
        };
    #else  // _WIN32
        #include <pthread.h>

        class Mutex {
            pthread_mutex_t mutex_;
        public:

            Mutex();
            ~Mutex();

            class Lock;
            friend class Lock;

            class Lock {
                Mutex& mutex_;
            public:
                explicit Lock(Mutex& lm);
                ~Lock();
            };
        };

    #endif // _WIN32
#else  // MULTI_THREADED (WE'RE SINGLE)

    class Mutex {
    public:
        class Lock {
        public:
            explicit Lock(Mutex&) {}
        };
    };

#endif // MULTI_THREADED



} // namespace
#endif // yaSSL_LOCK_HPP
