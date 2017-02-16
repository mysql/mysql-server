/*
   Copyright (c) 2005, 2014, Oracle and/or its affiliates

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

/* runtime.hpp provides C++ runtime support functions when building a pure C
 * version of yaSSL, user must define YASSL_PURE_C
*/



#ifndef yaSSL_NEW_HPP
#define yaSSL_NEW_HPP

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef __sun
 

// Handler for pure virtual functions
namespace __Crun {
    static void pure_error(void)
    {
       // "Pure virtual method called, Aborted", GCC 4.2 str cmp fix
    }
} // namespace __Crun

#endif // __sun


#if defined(__GNUC__) && !(defined(__ICC) || defined(__INTEL_COMPILER))

#if __GNUC__ > 2

extern "C" {
#if defined(DO_TAOCRYPT_KERNEL_MODE)
    #include "kernelc.hpp"
#endif

/* Disallow inline __cxa_pure_virtual() */
static int __cxa_pure_virtual() __attribute__((noinline, used));
static int __cxa_pure_virtual()
{
    // oops, pure virtual called!
    return 0;
}

} // extern "C"

#endif // __GNUC__ > 2
#endif // compiler check
#endif // yaSSL_NEW_HPP

