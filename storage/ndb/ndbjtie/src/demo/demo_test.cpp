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
 * demo_test.cpp
 */

#include <iostream>

#include "demo.hpp"
#include "helpers.hpp"

using std::cout;
using std::endl;

int
main(int argc, const char* argv[])
{
    cout << "--> main()" << endl;

    cout << endl << "calling simple(2.0) ..." << endl;
    simple(2.0);

    cout << endl << "calling A::print(NULL) ..." << endl;
    A::print(NULL);

    cout << endl << "calling A::print(\"...\") ..." << endl;
    A::print("this is a string literal");

    cout << endl << "calling A::getA() ..." << endl;
    A * a = A::getA();

    cout << endl << "calling a->print() ..." << endl;
    a->print();
    
    cout << endl;
    cout << "<-- main()" << endl;
    return 0;
}
