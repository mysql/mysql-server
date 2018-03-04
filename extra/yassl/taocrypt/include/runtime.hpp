/*
   Copyright (c) 2005, 2014, Oracle and/or its affiliates. All rights reserved.

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

/* runtime.hpp provides C++ runtime support functions when building a pure C
 * version of yaSSL, user must define YASSL_PURE_C
*/



#ifndef yaSSL_NEW_HPP
#define yaSSL_NEW_HPP

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef YASSL_PURE_C

#ifdef __sun
 

// Handler for pure virtual functions
namespace __Crun {
    void pure_error(void);
} // namespace __Crun

#endif // __sun


#if defined(__GNUC__) && !(defined(__ICC) || defined(__INTEL_COMPILER))

#if __GNUC__ > 2

extern "C" {
#if defined(DO_TAOCRYPT_KERNEL_MODE)
    #include "kernelc.hpp"
#endif
    int __cxa_pure_virtual () __attribute__ ((weak));
} // extern "C"

#endif // __GNUC__ > 2
#endif // compiler check
#endif // YASSL_PURE_C
#endif // yaSSL_NEW_HPP

