/* runtime.hpp                                
 *
 * Copyright (C) 2003 Sawtooth Consulting Ltd.
 *
 * This file is part of yaSSL.
 *
 * yaSSL is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * yaSSL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

/* runtime.hpp provides C++ runtime support functions when building a pure C
 * version of yaSSL, user must define YASSL_PURE_C
*/



#if !defined(yaSSL_NEW_HPP) && defined(DEFINE_CXA_PURE_VIRTUAL)

#define yaSSL_NEW_HPP


#include <cstdlib>


static void* operator new (size_t sz)
{
    return malloc (sz ? sz : 1);
}

static void* operator new[](size_t sz)
{
    return malloc (sz ? sz : 1);
}

static void operator delete (void* ptr)
{
    if (ptr) free(ptr);
}

static void operator delete[] (void* ptr)
{
    if (ptr) free(ptr);
}


#ifdef __GNUC__

extern "C" {

static int __cxa_pure_virtual()
{
    // oops, pure virtual called!
    return 0;
}

// simple guards for now that aren't perfect
// does yaSSL need full locking for two Integer statics,
// the Factory, and Session list?
// could leak ~ 8 bytes if two threads try to initialize at same time
// gcc didn't implement until 3.4


typedef long long __guard;


static int __cxa_guard_acquire(__guard* g)
{
    return !*(char*)g;
}

static void __cxa_guard_release(__guard* g)
{
    *(char*)g = 1;
}


} // extern "C"
#endif // __GNUC__

#endif // yaSSL_NEW_HPP
