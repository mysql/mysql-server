/*
   Copyright (c) 2000, 2012, Oracle and/or its affiliates. All rights reserved.

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


/* yaSSL log interface
 *
 */

#ifndef yaSSL_LOG_HPP
#define yaSSL_LOG_HPP

#include "socket_wrapper.hpp"

#ifdef YASSL_LOG
#include <stdio.h>
#endif

namespace yaSSL {

typedef unsigned int uint;


// Debug logger
class Log {
#ifdef YASSL_LOG
    FILE* log_;
#endif
public:
    explicit Log(const char* str = "yaSSL.log");
    ~Log();

    void Trace(const char*);
    void ShowTCP(socket_t, bool ended = false);
    void ShowData(uint, bool sent = false);
};


} // naemspace

#endif // yaSSL_LOG_HPP
