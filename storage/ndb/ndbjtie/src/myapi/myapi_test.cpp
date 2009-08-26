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
 * myapi_test.cpp
 */

#include <iostream>
#include <cassert>
#include <cstring>

#include "myapi.hpp"
#include "helpers.hpp"

using std::cout;
using std::endl;
//using std::string;
//using std::stringbuf;

/*
template< typename T, T F() >
void
call(T v) 
{
    T r = F();
    //cout << "r = " << (int)r << ", v = " << (int)v << endl;
    assert(r == v);
}

// primitive types, value, out
void
test0()
{
    cout << "testing basic functions: f1xx(f0xx()) ..." << endl;
    
    f010();
    call< const bool, f11 >(true);
    call< const bool, f11 >(false);
    call< bool, f31 >(true);
    call< bool, f31 >(false);
    for (int i = 1; i < 3; i++) {
        call< const char, f12 >(i);
    }
}
*/

void
test0()
{
    cout << endl << "testing basic function: f0() ..." << endl;

    f0();
}

// primitive types, by value in, out
void
test1()
{
    cout << endl << "testing primitive type functions: fxx(0) ..." << endl;

    f11(0);
    f12(0);
    f13(0);
    f14(0);
    f15(0);
    f16(0);
    f17(0);
    f18(0);
    f19(0);
    f20(0);
    f21(0);
    f22(0);
    f23(0);
    f24(0);
    f25(0);

    f31(0);
    f32(0);
    f33(0);
    f34(0);
    f35(0);
    f36(0);
    f37(0);
    f38(0);
    f39(0);
    f40(0);
    f41(0);
    f42(0);
    f43(0);
    f44(0);
    f45(0);
}

void
test2()
{
    cout << endl << "testing basic functions: f1xx(f0xx()) ..." << endl;

    for (int i = 0; i < 2; i++) {
        f111(f011());
        f112(f012());
        f113(f013());
        f114(f014());
        f115(f015());
        f116(f016());
        f117(f017());
        f118(f018());
        f121(f021());
        f122(f022());
        f123(f023());
        f124(f024());

        f131(f031());
        f132(f032());
        f133(f033());
        f134(f034());
        f135(f035());
        f136(f036());
        f137(f037());
        f138(f038());
        f141(f041());
        f142(f042());
        f143(f043());
        f144(f044());
    }
}

void
test3()
{
    cout << endl << "testing basic functions: f3xx(f2xx()) ..." << endl;

    for (int i = 0; i < 2; i++) {
        f311(f211());
        f312(f212());
        f313(f213());
        f314(f214());
        f315(f215());
        f316(f216());
        f317(f217());
        f318(f218());
        f321(f221());
        f322(f222());
        f323(f223());
        f324(f224());

        f331(f231());
        f332(f232());
        f333(f233());
        f334(f234());
        f335(f235());
        f336(f236());
        f337(f237());
        f338(f238());
        f341(f241());
        f342(f242());
        f343(f243());
        f344(f244());
    }
}

void
test4()
{
    cout << endl << "testing basic functions: f5xx(f4xx()) ..." << endl;

    for (int i = 0; i < 2; i++) {
        f511(f411());
        f512(f412());
        f513(f413());
        f514(f414());
        f515(f415());
        f516(f416());
        f517(f417());
        f518(f418());
        f521(f421());
        f522(f422());
        f523(f423());
        f524(f424());

        f531(f431());
        f532(f432());
        f533(f433());
        f534(f434());
        f535(f435());
        f536(f436());
        f537(f437());
        f538(f438());
        f541(f441());
        f542(f442());
        f543(f443());
        f544(f444());
    }
}

void
test5()
{
    cout << endl << "testing instance wrappers: ..." << endl;
    int n;
    
    cout << endl << "calling A..." << endl;
    A * a = new A();
    n = A::f0s();
    cout << "... A::f0s() = " << n << endl;
    assert (n == 10);
    n = a->f0s();
    cout << "... a->f0s() = " << n << endl;
    assert (n == 10);
    n = a->f0n();
    cout << "... a->f0n() = " << n << endl;
    assert (n == 11);
    n = a->f0v();
    cout << "... a->f0v() = " << n << endl;
    assert (n == 12);

    cout << endl << "A::take_ptr(A::return_ptr())..." << endl;
    A::take_ptr(A::return_ptr());

    cout << endl << "A::take_null_ptr(A::return_null_ptr())..." << endl;
    A::take_null_ptr(A::return_null_ptr());

    cout << endl << "A::take_ref(A::return_ref())..." << endl;
    A::take_ref(A::return_ref());

    cout << endl << "A::take_null_ref(A::return_null_ref())..." << endl;
    A::take_null_ref(A::return_null_ref());

    cout << endl << "A::print(A *)..." << endl;
    A::print(a);
    
    cout << endl << "calling B0..." << endl;
    B0 & b0b0 = *a->getB0();
    n = B0::f0s();    
    cout << "... B0::f0s() = " << n << endl;
    assert (n == 20);
    n = b0b0.f0s();    
    cout << "... b0b0.f0s() = " << n << endl;
    assert (n == 20);
    n = b0b0.f0n();
    cout << "... b0b0.f0n() = " << n << endl;
    assert (n == 21);
    n = b0b0.f0v();
    cout << "... b0b0.f0v() = " << n << endl;
    assert (n == 22);
    
    cout << endl << "calling B1..." << endl;
    B0 & b0b1 = *a->getB1();
    n = B1::f0s();    
    cout << "... B1::f0s() = " << n << endl;
    assert (n == 30);
    n = b0b1.f0s();    
    cout << "... b0b1.f0s() = " << n << endl;
    assert (n == 20);
    n = b0b1.f0n();
    cout << "... b0b1.f0n() = " << n << endl;
    assert (n == 21);
    n = b0b1.f0v();
    cout << "... b0b1.f0v() = " << n << endl;
    assert (n == 32);
};

void
test6()
{
    cout << endl << "testing string/byte array functions: sxxx(sxxx) ..." << endl;

    s112(s012());
}

void
test7()
{
    cout << endl << "testing n-ary array functions: g(), h() ..." << endl;
    int32_t n;

    cout << endl << "creating A..." << endl;
    A * a = new A();
    const A * ac = a;

    h0();

    h1(1);

    h2(1, 2);

    h3(1, 2, 3);

    n = h0r();
    assert(n == 0);
    
    n = h1r(1);
    assert(n == 1);
    
    n = h2r(1, 2);
    assert(n == 3);
    
    n = h3r(1, 2, 3);
    assert(n == 6);
    
    ac->g0c();

    ac->g1c(1);

    ac->g2c(1, 2);

    ac->g3c(1, 2, 3);

    a->g0();

    a->g1(1);

    a->g2(1, 2);

    a->g3(1, 2, 3);

    n = ac->g0rc();
    assert(n == 0);
    
    n = ac->g1rc(1);
    assert(n == 1);
    
    n = ac->g2rc(1, 2);
    assert(n == 3);
    
    n = ac->g3rc(1, 2, 3);
    assert(n == 6);
    
    n = a->g0r();
    assert(n == 0);
    
    n = a->g1r(1);
    assert(n == 1);
    
    n = a->g2r(1, 2);
    assert(n == 3);
    
    n = a->g3r(1, 2, 3);
    assert(n == 6);
}

int
main(int argc, const char* argv[])
{
    cout << "--> main()" << endl;

    test0();
    test1();
    test2();
    test3();
    test4();
    test5();
    test6();
    test7();

    cout << endl;
    cout << "<-- main()" << endl;
    return 0;
}
