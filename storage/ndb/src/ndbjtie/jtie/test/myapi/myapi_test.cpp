/*
 Copyright (c) 2010, 2014, Oracle and/or its affiliates. All rights reserved.

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

#include <my_config.h>
#include <assert.h> // not using namespaces yet
#include <stdio.h> // not using namespaces yet
#include <stdlib.h> // not using namespaces yet

#include "myapi.hpp"
#include "helpers.hpp"

void
test0()
{
    printf("\ntesting basic function: f0() ...\n");

    f0();
}

// primitive types, by value in, out
void
test1()
{
    printf("\ntesting primitive type functions: fxx(0) ...\n");


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
    printf("\ntesting basic functions: f1xx(f0xx()) ...\n");

    for (int i = 0; i < 2; i++) {

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
    printf("\ntesting basic functions: f3xx(f2xx()) ...\n");

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
    printf("\ntesting basic functions: f5xx(f4xx()) ...\n");

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
    printf("\ntesting basic functions: f7xx(f6xx()) ...\n");

    for (int i = 0; i < 2; i++) {
        f711(f611());
        f712(f612());
        f713(f613());
        f714(f614());
        f715(f615());
        f716(f616());
        f717(f617());
        f718(f618());
        f721(f621());
        f722(f622());
        f723(f623());
        f724(f624());

        f731(f631());
        f732(f632());
        f733(f633());
        f734(f634());
        f735(f635());
        f736(f636());
        f737(f637());
        f738(f638());
        f741(f641());
        f742(f642());
        f743(f643());
        f744(f644());


    }
}

void
test6()
{
    printf("\ntesting instance wrappers: ...\n");
    int n;

    printf("\ncalling A...\n");
    A * a = new A();
    printf("... new A() = %p\n", &a);
    delete new A(5);
    printf("... delete new A(int)\n");
    n = A::f0s();
    printf("... A::f0s() = %d\n", n);
    assert(n == 10);
    n = a->f0s();
    printf("... a->f0s() = %d\n", n);
    assert(n == 10);
    n = a->f0n();
    printf("... a->f0n() = %d\n", n);
    assert(n == 11);
    n = a->f0v();
    printf("... a->f0v() = %d\n", n);
    assert(n == 12);

    printf("\nA::take_ptr(A::deliver_ptr())...\n");
    A::take_ptr(A::deliver_ptr());

    printf("\nA::take_null_ptr(A::deliver_null_ptr())...\n");
    A::take_null_ptr(A::deliver_null_ptr());

    printf("\nA::take_ref(A::deliver_ref())...\n");
    A::take_ref(A::deliver_ref());

    printf("\nA::take_null_ref(A::deliver_null_ref())...\n");
    A::take_null_ref(A::deliver_null_ref());

    printf("\nA::print(A *)...\n");
    A::print(a);


    printf("\naccessing A...\n");
    n = ++A::d0s;
    printf("... ++A::d0s = %d\n", n);
    assert(n == 11);
    n = A::d0sc;
    printf("... A::d0sc = %d\n", n);
    assert(n == -10);
    n = ++a->d0s;
    printf("... ++a->d0s = %d\n", n);
    assert(n == 12);
    n = a->d0sc;
    printf("... a->d0sc = %d\n", n);
    assert(n == -10);
    n = ++a->d0;
    printf("... ++a->d0 = %d\n", n);
    assert(n == 12);
    n = a->d0c;
    printf("... a->d0c = %d\n", n);
    assert(n == -11);

    printf("\ncalling B0...\n");
    B0 & b0b0 = *a->newB0();
    printf("... a->newB0() = %p\n", &b0b0);
    n = B0::f0s();
    printf("... B0::f0s() = %d\n", n);
    assert(n == 20);
    n = b0b0.f0s();
    printf("... b0b0.f0s() = %d\n", n);
    assert(n == 20);
    n = b0b0.f0n();
    printf("... b0b0.f0n() = %d\n", n);
    assert(n == 21);
    n = b0b0.f0v();
    printf("... b0b0.f0v() = %d\n", n);
    assert(n == 22);
    a->del(b0b0);
    printf("... a->del(b0b0)\n");

    printf("\naccessing B0...\n");
    B0 & b0 = *a->newB0();
    printf("... a->newB0() = %p\n", &b0);
    n = ++B0::d0s;
    printf("... ++B0::d0s = %d\n", n);
    assert(n == 21);
    n = B0::d0sc;
    printf("... B0::d0sc = %d\n", n);
    assert(n == -20);
    n = ++b0.d0s;
    printf("... ++b0.d0s = %d\n", n);
    assert(n == 22);
    n = b0.d0sc;
    printf("... b0.d0sc = %d\n", n);
    assert(n == -20);
    n = ++b0.d0;
    printf("... ++b0.d0 = %d\n", n);
    assert(n == 22);
    n = b0.d0c;
    printf("... b0.d0c = %d\n", n);
    assert(n == -21);
    a->del(b0);
    printf("... a->del(b0)\n");

    printf("\ncalling B1...\n");
    B1 & b1b1 = *a->newB1();
    B0 & b0b1 = b1b1;
    printf("... a->newB1() = %p\n", &b0b1);
    n = B1::f0s();
    printf("... B1::f0s() = %d\n", n);
    assert(n == 30);
    n = b0b1.f0s();
    printf("... b0b1.f0s() = %d\n", n);
    assert(n == 20);
    n = b0b1.f0n();
    printf("... b0b1.f0n() = %d\n", n);
    assert(n == 21);
    n = b0b1.f0v();
    printf("... b0b1.f0v() = %d\n", n);
    assert(n == 32);
    a->del(b1b1);
    printf("... a->del(b1b1)\n");

    printf("\naccessing B1...\n");
    B1 & b1 = *a->newB1();
    printf("... a->newB1() = %p\n", &b1);
    n = ++B1::d0s;
    printf("... ++B1::d0s = %d\n", n);
    assert(n == 31);
    n = B1::d0sc;
    printf("... B1::d0sc = %d\n", n);
    assert(n == -30);
    n = ++b1.d0s;
    printf("... ++b1.d0s = %d\n", n);
    assert(n == 32);
    n = b1.d0sc;
    printf("... b1.d0sc = %d\n", n);
    assert(n == -30);
    n = ++b1.d0;
    printf("... ++b1.d0 = %d\n", n);
    assert(n == 32);
    n = b1.d0c;
    printf("... b1.d0c = %d\n", n);
    assert(n == -31);
    a->del(b1);
    printf("... a->del(b1)\n");

    printf("\ndelete A...\n");
    delete a;
};

void
test7()
{
    printf("\ntesting string/byte array functions: sxxx(sxxx) ...\n");

    s110(s010());
    s110(s012());
    s112(s012());

    s110(s030());
    s110(s032());
    s112(s032());

    s130(s030());
    s130(s032());
    s132(s032());




    s310(s210());
    s310(s212());
    s312(s212());

    s310(s230());
    s310(s232());
    s312(s232());

    s330(s230());
    s330(s232());
    s332(s232());



}

void
test8()
{
    printf("\ntesting n-ary array functions: g(), h() ...\n");
    int32_t n;

    printf("\ncreating A...\n");
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

    printf("delete A...\n");
    delete a;
}

void
test9()
{
    printf("\ntesting const & inheritance: ...\n");

    C0 & c0 = *C0::c;
    const C0 & c0c = *C0::cc;
    C1 & c1 = *C1::c;
    const C1 & c1c = *C1::cc;

    // for debugging
    if (false) {
        printf("\nc0.print()... c0c.print()...\n");
        c0.print();
        c1.print();

        printf("\nc1.print()... c1c.print()...\n");
        c0c.print();
        c1c.print();
    }

    printf("\nc0.check(c0.id);\n");
    c0.check(c0.id);

    printf("\nc0c.check(c0c.id);\n");
    c0c.check(c0c.id);

    printf("\nc1.check(c1.id);\n");
    c1.check(c1.id);

    printf("\nc1c.check(c1c.id);\n");
    c1c.check(c1c.id);

    // C0 -> C0
    printf("\nc0c.take_C0Cp(c0c.deliver_C0Cp())...\n");
    c0c.take_C0Cp(c0c.deliver_C0Cp());

    printf("\nc0c.take_C0Cr(c0c.deliver_C0Cr())...\n");
    c0c.take_C0Cr(c0c.deliver_C0Cr());

    printf("\nc0c.take_C0Cp(c0.deliver_C0p())...\n");
    c0c.take_C0Cp(c0.deliver_C0p());

    printf("\nc0c.take_C0Cr(c0.deliver_C0r())...\n");
    c0c.take_C0Cr(c0.deliver_C0r());

    printf("\nc0.take_C0p(c0.deliver_C0p())...\n");
    c0.take_C0p(c0.deliver_C0p());

    printf("\nc0.take_C0r(c0.deliver_C0r())...\n");
    c0.take_C0r(c0.deliver_C0r());

    // C1 -> C0
    printf("\nc0c.take_C0Cp(c1c.deliver_C1Cp())...\n");
    c0c.take_C0Cp(c1c.deliver_C1Cp());

    printf("\nc0c.take_C0Cr(c1c.deliver_C1Cr())...\n");
    c0c.take_C0Cr(c1c.deliver_C1Cr());

    printf("\nc0c.take_C0Cp(c1.deliver_C1p())...\n");
    c0c.take_C0Cp(c1.deliver_C1p());

    printf("\nc0c.take_C0Cr(c1.deliver_C1r())...\n");
    c0c.take_C0Cr(c1.deliver_C1r());

    printf("\nc0.take_C0p(c1.deliver_C1p())...\n");
    c0.take_C0p(c1.deliver_C1p());

    printf("\nc0.take_C0r(c1.deliver_C1r())...\n");
    c0.take_C0r(c1.deliver_C1r());

    // C0 -> C1
    printf("\nc1c.take_C1Cp(c1c.deliver_C1Cp())...\n");
    c1c.take_C1Cp(c1c.deliver_C1Cp());

    printf("\nc1c.take_C1Cr(c1c.deliver_C1Cr())...\n");
    c1c.take_C1Cr(c1c.deliver_C1Cr());

    printf("\nc1c.take_C1Cp(c1.deliver_C1p())...\n");
    c1c.take_C1Cp(c1.deliver_C1p());

    printf("\nc1c.take_C1Cr(c1.deliver_C1r())...\n");
    c1c.take_C1Cr(c1.deliver_C1r());

    printf("\nc1.take_C1p(c1.deliver_C1p())...\n");
    c1.take_C1p(c1.deliver_C1p());

    printf("\nc1.take_C1r(c1.deliver_C1r())...\n");
    c1.take_C1r(c1.deliver_C1r());
}

void
test10()
{
    printf("\ntesting object array functions ...\n\n");

    printf("\ndelete[] (new C0[0])\n");
    C0 * c0a0 = new C0[0];
#if __GNUC__ == 4 && __GNUC_MINOR__ == 7 && __GNUC_PATCHLEVEL__ >= 0
    // workaround for GCC 4.7.x bug
    // no initialization of var for const zero-length obj array expr;
    // compile warning: "'c0a0' is used uninitialized in this function"
    // see http://gcc.gnu.org/bugzilla/show_bug.cgi?id=53330
    c0a0 = new C0[atoi("0")];
#endif

    // check that zero-length array new worked
    assert(c0a0);
    delete[] c0a0;

    const int n = 3;
    C0 * c0a = new C0[n];
    const C0 * c0ca = c0a;
    C1 * c1a = new C1[n];
    const C1 * c1ca = c1a;

    for (int i = 0; i < 0; i++) {
        printf("\nc0a[i].print()\n");
        c0a[i].print();

        printf("\nc0ca[i].print()\n");
        c0ca[i].print();

        printf("\nc1a[i].print()\n");
        c1a[i].print();

        printf("\nc1ca[i].print()\n");
        c1ca[i].print();
    }

    for (int i = 0; i < n; i++) {
        printf("\nc0a[i].check(c0a[i].id)\n");
        c0a[i].check(c0a[i].id);

        printf("\nc0ca[i].check(c0ca[i].id)\n");
        c0ca[i].check(c0ca[i].id);

        printf("\nc1a[i].check(c1a[i].id)\n");
        c1a[i].check(c1a[i].id);

        printf("\nc1ca[i].check(c1ca[i].id)\n");
        c1ca[i].check(c1ca[i].id);
    }

    printf("\nC0::hash(c0a, n) == C0::hash(C0::pass(c0a), n)...\n");
    assert(C0::hash(c0a, n) == C0::hash(C0::pass(c0a), n));

    printf("\nC0::hash(c0ca, n) == C0::hash(C0::pass(c0ca), n)...\n");
    assert(C0::hash(c0ca, n) == C0::hash(C0::pass(c0ca), n));

    printf("\nC1::hash(c1a, n) == C1::hash(C1::pass(c1a), n)...\n");
    assert(C1::hash(c1a, n) == C1::hash(C1::pass(c1a), n));

    printf("\nC1::hash(c1ca, n) == C1::hash(C1::pass(c1ca), n)...\n");
    assert(C1::hash(c1ca, n) == C1::hash(C1::pass(c1ca), n));

    delete[] c1a;
    delete[] c0a;
}

void
test11()
{
    printf("\ntesting function dispatch ...\n\n");

    assert(D0::sub()->f_d0() == 20);
    assert(D0::sub()->f_nv() == 31);
    assert(D0::sub()->f_v() == 32);

    assert(D1::sub()->f_d0() == 20);
    assert(D1::sub()->f_d1() == 30);
    assert(D1::sub()->f_nv() == 31);
    assert(D1::sub()->f_v() == 42);

    assert(D2::sub() == NULL);
}

template< typename E, void (F)(E) >
inline void call(E e)
{
    (F)(e);
}

template< typename E, E (F)() >
inline E call()
{
    return (F)();
}

void
test12()
{
    printf("\ntesting enums: ...\n");

    // EE enums
    printf("\nE::take_EE1(E::deliver_EE1())...\n");
    E::take_EE1(E::deliver_EE1());

    printf("\ncall< E::EE, E::deliver_EE1 >()...\n");
    E::EE e = call< E::EE, E::deliver_EE1 >();
    assert(e == E::EE1);

    printf("\ncall< E::EE, E::take_EE1 >(e)...\n");
    call< E::EE, E::take_EE1 >(e);

    printf("\nE::take_EE1c(E::deliver_EE1())...\n");
    E::take_EE1c(E::deliver_EE1());

    printf("\ncall< E::EE, E::deliver_EE1 >()...\n");
    const E::EE ec = call< E::EE, E::deliver_EE1 >();
    assert(ec == E::EE1);

    printf("\ncall< E::EE, E::take_EE1c >(e)...\n");
    call< const E::EE, E::take_EE1c >(ec);
}

int
main(int argc, const char* argv[])
{
    printf("\n--> main()\n");
    (void)argc; (void)argv;

    myapi_init();

    if (true) {
        test0();
        test1();
        test2();
        test3();
        test4();
        test5();
        test6();
        test7();
        test8();
        test9();
        test10();
        test11();
        test12();
    } else {
        test12();
    }

    myapi_finit();

    printf("\n<-- main()\n");
    return 0;
}
