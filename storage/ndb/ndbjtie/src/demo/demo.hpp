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
 * demo.hpp
 */

#ifndef _demo
#define _demo

#include <cstdio>

#include "helpers.hpp"

extern double simple(double p0);

struct A {
    static A * a;
    
    static void print(const char * p0) {
        TRACE("void A::print(const char *)");
        printf("    p0 = %s\n", (p0 == NULL ? "NULL" : p0));
    };

    static A * getA() {
        TRACE("A * A::getA()");
        printf("    A * a = %p\n", a);
        return a;
    };

    virtual void print() const {
        TRACE("void A::print()");
        printf("    this = %p\n", this);
    };
};

#endif // _demo
