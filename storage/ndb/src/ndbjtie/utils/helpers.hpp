/*
 Copyright (c) 2011, 2014, Oracle and/or its affiliates. All rights
 reserved.

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
 * helpers.hpp
 */

#ifndef helpers_hpp
#define helpers_hpp

#include <my_config.h>
#include <stdio.h> // not using namespaces yet
#include <stdlib.h> // not using namespaces yet

/************************************************************
 * Helper Macros & Functions
 ************************************************************/

// need two levels of macro substitution
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#define CHECK(cond, message)                    \
    if (cond) ABORT_ERROR(message);

// gcc: beware crashes when printing source code line number '<< __LINE__'
// C99's __func__ not supported by some C++ compilers yet (Solaris)
#define PRINT_ERROR(message)                                            \
    do {                                                                \
        fflush(stdout);                                                 \
        fprintf(stderr, "\n!!! error, file: %s, line: %s, msg: %s.\n",  \
                (__FILE__), TOSTRING(__LINE__), (message));             \
        fflush(stderr);                                                 \
    } while (0)

#define PRINT_ERROR_CODE(message, code)                                 \
    do {                                                                \
        fflush(stdout);                                                 \
        fprintf(stderr, "\n!!! error, file: %s, line: %s, msg: %s, "    \
                "code %d.\n",                                           \
                (__FILE__), TOSTRING(__LINE__),                         \
                (message), (code));                                     \
        fflush(stderr);                                                 \
    } while (0)

#define ABORT_ERROR(message)                                            \
    do {                                                                \
        PRINT_ERROR(message);                                           \
        exit(-1);                                                       \
    } while (0)

// macro for printing verbose message
#if JTIE_VERBOSE
#  define VERBOSE(msg) fflush(stdout); printf("    %s\n", (msg));
#else
#  define VERBOSE(msg)
#endif

// macros for tracing
#ifdef JTIE_TRACE
#  define ENTER(name) fflush(stdout); printf("--> %s\n", (name));
#  define LEAVE(name) printf("<-- %s\n", (name)); fflush(stdout);
#  define TRACE(name) JTieTracer _jtie_tracer(name);
#else
#  define ENTER(name)
#  define LEAVE(name)
#  define TRACE(name)
#endif // JTIE_TRACE

// use as:
// myfunction() {
//   TRACE("myfunction()");
//   ...
// }
class JTieTracer
{
    const char* const name;
public:
    JTieTracer(const char* fname) : name(fname) {
        ENTER(name);
    }

    ~JTieTracer() {
        LEAVE(name);
    }
};

#endif // helpers_hpp
