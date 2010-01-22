/*
 Copyright (C) 2009 Sun Microsystems, Inc.
 All rights reserved. Use is subject to license terms.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; version 2 of the License.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/
/*
 * gccsup_lib.hpp
 */

#ifndef gccsup_lib_hpp
#define gccsup_lib_hpp

#ifdef __GNUG__

#include "helpers.hpp"

/*
 * This file is a hack: It provides definitions for gcc/g++ symbols.
 * 
 * Modern versions of g++ generate references to a function
 *    __cxa_pure_virtual()
 * whenever the code contains pure virtual functions.  This function
 * is an error handler for catching a call to a virtual function while
 * an object is still being constructed.  It should never get called.
 *
 * There is a default implementation in gcc's libsupc++ (suplemental),
 * which is also included in libstdc++.
 *
 * Unfortunately, and for reasons unknown to the author (MZ), the
 * mysql/ndb build system chooses
 * - not to link against libstdc++ by means of forcing the use of gcc
 *   (instead of using option -nodefaultlibs, or -nostdlib -lgcc etc)
 * - not to specify option -lsupc++ when linking with gcc,
 * - not to provide a symbol definition in libndbclient (which seems
 *   to be the only MySQL Cluster library referencing it).
 *
 * So, unless any of the above is changed, libndbjtie must provide a
 * definition of any missing g++ symbol (or dynamic loading may fail).
 * Conversely, when any of the above gets changed, it is undetermined
 * whether we'll get some sort of duplicate symbol definition errors.
 */

// Unlike this function's definition in mysys/my_new.cc,
// it seems that the return type is void and not int; see
//   http://gcc.gnu.org/ml/libstdc++/2009-04/msg00120.html
extern "C"
void __cxa_pure_virtual()
{
    ABORT_ERROR("Error: pure virtual method called; aborting program.");
    for (;;);    
}

#endif // __GNUG__

#endif // gccsup_lib_hpp
