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
 * myapi.hpp
 */

#ifndef _myapi
#define _myapi

#include <stdint.h>
#include <cstdio>

#include "helpers.hpp"

// reference returns
//extern [const] int& f0(); -> ByteBuffer
//extern [const] int* f0(); -> ByteBuffer, but no way of knowing size
//extern [const] char* f0(); -> ByteBuffer, but no way of knowing size
//extern const char* f0(); -> String, assuming 0-terminated and copy-semantics

extern void f0();

extern const void * s010();
extern const char * s012();
extern void * s030();
extern char * s032();
extern void s110(const void * p0);
extern void s112(const char * p0);
extern void s130(void * p0);
extern void s132(char * p0);

extern const bool f11(const bool p0);
extern const char f12(const char p0);
extern const signed char f13(const signed char p0);
extern const unsigned char f14(const unsigned char p0);
extern const signed short f15(const signed short p0);
extern const unsigned short f16(const unsigned short p0);
extern const signed int f17(const signed int p0);
extern const unsigned int f18(const unsigned int p0);
extern const signed long f19(const signed long p0);
extern const unsigned long f20(const unsigned long p0);
extern const signed long long f21(const signed long long p0);
extern const unsigned long long f22(const unsigned long long p0);
extern const float f23(const float p0);
extern const double f24(const double p0);
extern const long double f25(const long double p0);

extern bool f31(bool p0);
extern char f32(char p0);
extern signed char f33(signed char p0);
extern unsigned char f34(unsigned char p0);
extern signed short f35(signed short p0);
extern unsigned short f36(unsigned short p0);
extern signed int f37(signed int p0);
extern unsigned int f38(unsigned int p0);
extern signed long f39(signed long p0);
extern unsigned long f40(unsigned long p0);
extern signed long long f41(signed long long p0);
extern unsigned long long f42(unsigned long long p0);
extern float f43(float p0);
extern double f44(double p0);
extern long double f45(long double p0);

extern const bool f011();
extern const char f012();
extern const int8_t f013();
extern const uint8_t f014();
extern const int16_t f015();
extern const uint16_t f016();
extern const int32_t f017();
extern const uint32_t f018();
extern const int64_t f021();
extern const uint64_t f022();
extern const float f023();
extern const double f024();

extern bool f031();
extern char f032();
extern int8_t f033();
extern uint8_t f034();
extern int16_t f035();
extern uint16_t f036();
extern int32_t f037();
extern uint32_t f038();
extern int64_t f041();
extern uint64_t f042();
extern float f043();
extern double f044();

extern void f111(const bool);
extern void f112(const char);
extern void f113(const int8_t);
extern void f114(const uint8_t);
extern void f115(const int16_t);
extern void f116(const uint16_t);
extern void f117(const int32_t);
extern void f118(const uint32_t);
extern void f121(const int64_t);
extern void f122(const uint64_t);
extern void f123(const float);
extern void f124(const double);

extern void f131(bool);
extern void f132(char);
extern void f133(int8_t);
extern void f134(uint8_t);
extern void f135(int16_t);
extern void f136(uint16_t);
extern void f137(int32_t);
extern void f138(uint32_t);
extern void f141(int64_t);
extern void f142(uint64_t);
extern void f143(float);
extern void f144(double);

extern const bool & f211();
extern const char & f212();
extern const int8_t & f213();
extern const uint8_t & f214();
extern const int16_t & f215();
extern const uint16_t & f216();
extern const int32_t & f217();
extern const uint32_t & f218();
extern const int64_t & f221();
extern const uint64_t & f222();
extern const float & f223();
extern const double & f224();

extern bool & f231();
extern char & f232();
extern int8_t & f233();
extern uint8_t & f234();
extern int16_t & f235();
extern uint16_t & f236();
extern int32_t & f237();
extern uint32_t & f238();
extern int64_t & f241();
extern uint64_t & f242();
extern float & f243();
extern double & f244();

extern void f311(const bool &);
extern void f312(const char &);
extern void f313(const int8_t &);
extern void f314(const uint8_t &);
extern void f315(const int16_t &);
extern void f316(const uint16_t &);
extern void f317(const int32_t &);
extern void f318(const uint32_t &);
extern void f321(const int64_t &);
extern void f322(const uint64_t &);
extern void f323(const float &);
extern void f324(const double &);

extern void f331(bool &);
extern void f332(char &);
extern void f333(int8_t &);
extern void f334(uint8_t &);
extern void f335(int16_t &);
extern void f336(uint16_t &);
extern void f337(int32_t &);
extern void f338(uint32_t &);
extern void f341(int64_t &);
extern void f342(uint64_t &);
extern void f343(float &);
extern void f344(double &);

extern const bool * f411();
extern const char * f412();
extern const int8_t * f413();
extern const uint8_t * f414();
extern const int16_t * f415();
extern const uint16_t * f416();
extern const int32_t * f417();
extern const uint32_t * f418();
extern const int64_t * f421();
extern const uint64_t * f422();
extern const float * f423();
extern const double * f424();

extern bool * f431();
extern char * f432();
extern int8_t * f433();
extern uint8_t * f434();
extern int16_t * f435();
extern uint16_t * f436();
extern int32_t * f437();
extern uint32_t * f438();
extern int64_t * f441();
extern uint64_t * f442();
extern float * f443();
extern double * f444();

extern void f511(const bool *);
extern void f512(const char *);
extern void f513(const int8_t *);
extern void f514(const uint8_t *);
extern void f515(const int16_t *);
extern void f516(const uint16_t *);
extern void f517(const int32_t *);
extern void f518(const uint32_t *);
extern void f521(const int64_t *);
extern void f522(const uint64_t *);
extern void f523(const float *);
extern void f524(const double *);

extern void f531(bool *);
extern void f532(char *);
extern void f533(int8_t *);
extern void f534(uint8_t *);
extern void f535(int16_t *);
extern void f536(uint16_t *);
extern void f537(int32_t *);
extern void f538(uint32_t *);
extern void f541(int64_t *);
extern void f542(uint64_t *);
extern void f543(float *);
extern void f544(double *);

struct B0 {
    B0() {
        TRACE("B0()");
    };

    B0(const B0 & b0) {
        TRACE("B0(const B0 &)");
        ABORT_ERROR("!USE OF COPY CONSTRUCTOR!");
    };

    virtual ~B0() {
    };

    B0 & operator=(const B0 & p) {
        TRACE("B0 & operator=(const B0 &)");
        ABORT_ERROR("!USE OF ASSIGNMENT OPERATOR!");
        return *this;
    }

    static int32_t f0s() {
        TRACE("int32_t B0::f0s()");
        return 20;
    };

    int32_t f0n() const {
        TRACE("int32_t B0::f0n()");
        return 21;
    };

    virtual int32_t f0v() const {
        TRACE("int32_t B0::f0v()");
        return 22;
    };

};

struct B1 : public B0 {
    B1() {
        TRACE("B1()");
    };

    B1(const B1 & b1) {
        TRACE("B1(const B1 &)");
        ABORT_ERROR("!USE OF COPY CONSTRUCTOR!");
    };

    virtual ~B1() {
    };

    B1 & operator=(const B1 & p) {
        TRACE("B1 & operator=(const B1 &)");
        ABORT_ERROR("!USE OF ASSIGNMENT OPERATOR!");
        return *this;
    }

    static int32_t f0s() {
        TRACE("int32_t B1::f0s()");
        return 30;
    };

    int32_t f0n() const {
        TRACE("int32_t B1::f0n()");
        return 31;
    };

    virtual int32_t f0v() const {
        TRACE("int32_t B1::f0v()");
        return 32;
    };
};

struct A {
    static A * a;
    
    A() {
        TRACE("A()");
    };
    
    A(const A & a) {
        TRACE("A(const A &)");
        ABORT_ERROR("!USE OF COPY CONSTRUCTOR!");
    };

    virtual ~A() {
        TRACE("~A()");
    };

    A & operator=(const A & p) {
        TRACE("A & operator=(const A &)");
        ABORT_ERROR("!USE OF ASSIGNMENT OPERATOR!");
        return *this;
    }

    static A * return_ptr() {
        TRACE("A * A::return_ptr()");
        return a;
    };

    static A * return_null_ptr() {
        TRACE("A * A::return_null_ptr()");
        return NULL;
    };

    static A & return_ref() {
        TRACE("A & A::return_ref()");
        return *a;
    };

    static A & return_null_ref() {
        TRACE("A & A::return_null_ref()");
        return *((A *)NULL);
    };

    static void take_ptr(A * a) {
        TRACE("void A::take_ptr(A * a)");
        if (a != A::a) ABORT_ERROR("void A::take_ptr(A * a)");
    };

    static void take_null_ptr(A * a) {
        TRACE("void A::take_null_ptr(A * a)");
        if (a != NULL) ABORT_ERROR("void A::take_null_ptr(A * a)");
    };

    static void take_ref(A & a) {
        TRACE("void A::take_ref(A & a)");
        if (&a != A::a) ABORT_ERROR("void A::take_ref(A & a)");
    };

    static void take_null_ref(A & a) {
        TRACE("void A::take_null_ref(A & a)");
        if (&a != NULL) ABORT_ERROR("void A::take_null_ref(A & a)");
    };

    static void print(A * p0) {
        TRACE("void A::print(A *)");
        printf("    p0 = %lx\n", (unsigned long)p0);
    };

    // XXX also test non-const methods, references...

    B0 * getB0() const {
        TRACE("B0 A::getB0()");
        return new B0();
    };

    B1 * getB1() const {
        TRACE("B1 A::getB1()");
        return new B1();
    };

    static int32_t f0s() {
        TRACE("int32_t A::f0s()");
        return 10;
    };

    int32_t f0n() const {
        TRACE("int32_t A::f0n()");
        return 11;
    };

    virtual int32_t f0v() const {
        TRACE("int32_t A::f0v()");
        return 12;
    };

    // ----------------------------------------------------------------------

    void g0c() const {
        TRACE("void A::g0c()");
    };

    void g1c(int8_t p0) const {
        TRACE("void A::g1c(int8_t)");
        if (p0 != 1) ABORT_ERROR("wrong arg value");
    };

    void g2c(int8_t p0, int16_t p1) const {
        TRACE("void A::g2c(int8_t, int16_t)");
        if (p0 != 1) ABORT_ERROR("wrong arg value");
        if (p1 != 2) ABORT_ERROR("wrong arg value");
    };

    void g3c(int8_t p0, int16_t p1, int32_t p2) const {
        TRACE("void A::g3c(int8_t, int16_t, int32_t)");
        if (p0 != 1) ABORT_ERROR("wrong arg value");
        if (p1 != 2) ABORT_ERROR("wrong arg value");
        if (p2 != 3) ABORT_ERROR("wrong arg value");
    };

    void g0() {
        TRACE("void A::g0()");
    };

    void g1(int8_t p0) {
        TRACE("void A::g1(int8_t)");
        if (p0 != 1) ABORT_ERROR("wrong arg value");
    };

    void g2(int8_t p0, int16_t p1) {
        TRACE("void A::g2(int8_t, int16_t)");
        if (p0 != 1) ABORT_ERROR("wrong arg value");
        if (p1 != 2) ABORT_ERROR("wrong arg value");
    };

    void g3(int8_t p0, int16_t p1, int32_t p2) {
        TRACE("void A::g3(int8_t, int16_t, int32_t)");
        if (p0 != 1) ABORT_ERROR("wrong arg value");
        if (p1 != 2) ABORT_ERROR("wrong arg value");
        if (p2 != 3) ABORT_ERROR("wrong arg value");
    };

    // ----------------------------------------------------------------------

    int32_t g0rc() const {
        TRACE("int32_t A::g0rc()");
        return 0;
    };

    int32_t g1rc(int8_t p0) const {
        TRACE("int32_t A::g1rc(int8_t)");
        if (p0 != 1) ABORT_ERROR("wrong arg value");
        return p0;
    };

    int32_t g2rc(int8_t p0, int16_t p1) const {
        TRACE("int32_t A::g2rc(int8_t, int16_t)");
        if (p0 != 1) ABORT_ERROR("wrong arg value");
        if (p1 != 2) ABORT_ERROR("wrong arg value");
        return p0 + p1;
    };

    int32_t g3rc(int8_t p0, int16_t p1, int32_t p2) const {
        TRACE("int32_t A::g3rc(int8_t, int16_t, int32_t)");
        if (p0 != 1) ABORT_ERROR("wrong arg value");
        if (p1 != 2) ABORT_ERROR("wrong arg value");
        if (p2 != 3) ABORT_ERROR("wrong arg value");
        return p0 + p1 + p2;
    };

    int32_t g0r() {
        TRACE("int32_t A::g0r()");
        return 0;
    };

    int32_t g1r(int8_t p0) {
        TRACE("int32_t A::g1r(int8_t)");
        if (p0 != 1) ABORT_ERROR("wrong arg value");
        return p0;
    };

    int32_t g2r(int8_t p0, int16_t p1) {
        TRACE("int32_t A::g2r(int8_t, int16_t)");
        if (p0 != 1) ABORT_ERROR("wrong arg value");
        if (p1 != 2) ABORT_ERROR("wrong arg value");
        return p0 + p1;
    };

    int32_t g3r(int8_t p0, int16_t p1, int32_t p2) {
        TRACE("int32_t A::g3r(int8_t, int16_t, int32_t)");
        if (p0 != 1) ABORT_ERROR("wrong arg value");
        if (p1 != 2) ABORT_ERROR("wrong arg value");
        if (p2 != 3) ABORT_ERROR("wrong arg value");
        return p0 + p1 + p2;
    };
};

// ----------------------------------------------------------------------

inline void h0() {
    TRACE("void h0()");
};

inline void h1(int8_t p0) {
    TRACE("void h1(int8_t)");
    if (p0 != 1) ABORT_ERROR("wrong arg value");
};

inline void h2(int8_t p0, int16_t p1) {
    TRACE("void h2(int8_t, int16_t)");
    if (p0 != 1) ABORT_ERROR("wrong arg value");
    if (p1 != 2) ABORT_ERROR("wrong arg value");
};

inline void h3(int8_t p0, int16_t p1, int32_t p2) {
    TRACE("void h3(int8_t, int16_t, int32_t)");
    if (p0 != 1) ABORT_ERROR("wrong arg value");
    if (p1 != 2) ABORT_ERROR("wrong arg value");
    if (p2 != 3) ABORT_ERROR("wrong arg value");
};

inline int32_t h0r() {
    TRACE("int32_t h0r()");
    return 0;
};

inline int32_t h1r(int8_t p0) {
    TRACE("int32_t h1r(int8_t)");
    if (p0 != 1) ABORT_ERROR("wrong arg value");
    return p0;
};

inline int32_t h2r(int8_t p0, int16_t p1) {
    TRACE("int32_t h2r(int8_t, int16_t)");
    if (p0 != 1) ABORT_ERROR("wrong arg value");
    if (p1 != 2) ABORT_ERROR("wrong arg value");
    return p0 + p1;
};

inline int32_t h3r(int8_t p0, int16_t p1, int32_t p2) {
    TRACE("int32_t h3r(int8_t, int16_t, int32_t)");
    if (p0 != 1) ABORT_ERROR("wrong arg value");
    if (p1 != 2) ABORT_ERROR("wrong arg value");
    if (p2 != 3) ABORT_ERROR("wrong arg value");
    return  p0 + p1 + p2;
};

//    printf("p0=%d, p1=%d, p2=%d", p0, p1, p2);

#endif // _myapi
