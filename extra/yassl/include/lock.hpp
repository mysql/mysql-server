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
#else
#include <pthread.h>
#endif
#endif

namespace yaSSL {


#ifdef MULTI_THREADED
    #ifdef _WIN32
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
