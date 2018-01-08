/*
   Copyright (c) 2000, 2012, Oracle and/or its affiliates. All rights reserved.

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

/* timer.cpp implements a high res and low res timer
 *
*/

#include "runtime.hpp"
#include "timer.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

namespace yaSSL {

#ifdef _WIN32

    timer_d timer()
    {
        static bool          init(false);
        static LARGE_INTEGER freq;
    
        if (!init) {
            QueryPerformanceFrequency(&freq);
            init = true;
        }

        LARGE_INTEGER count;
        QueryPerformanceCounter(&count);

        return static_cast<double>(count.QuadPart) / freq.QuadPart;
    }


    uint lowResTimer()
    {
        return static_cast<uint>(timer());
    }

#else // _WIN32

    timer_d timer()
    {
        struct timeval tv;
        gettimeofday(&tv, 0);

        return static_cast<double>(tv.tv_sec) 
             + static_cast<double>(tv.tv_usec) / 1000000;
    }


    uint lowResTimer()
    {
        struct timeval tv;
        gettimeofday(&tv, 0);

        return tv.tv_sec; 
    }


#endif // _WIN32
} // namespace yaSSL
