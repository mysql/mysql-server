/*
 Copyright (c) 2010, 2012 Oracle and/or its affiliates. All rights reserved.

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
 * myapi.cpp
 */

#include <stdio.h> // not using namespaces yet
#include <string.h> // not using namespaces yet

#include "myapi.hpp"
#include "helpers.hpp"

// ---------------------------------------------------------------------------
// static initializations
// ---------------------------------------------------------------------------

int32_t B0::d0s = 20;
const int32_t B0::d0sc = -20;

int32_t B1::d0s = 30;
const int32_t B1::d0sc = -30;

A * A::a = NULL;
int32_t A::d0s = 10;
const int32_t A::d0sc = -10;

void B0::init() {
}

void B0::finit() {
}

void B1::init() {
}

void B1::finit() {
}

void A::init() {
    //printf("    XXX A::a = %p\n", A::a);
    assert(!A::a);
    A::a = new A();
    //printf("    YYY A::a = %p\n", A::a);
}

void A::finit() {
    //printf("    ZZZ A::a = %p\n", A::a);
    assert(A::a);
    delete A::a;
    A::a = NULL;
}

// ----------------------------------------

const C0 * C0::cc = NULL;
C0 * C0::c = NULL;

const C1 * C1::cc = NULL;
C1 * C1::c = NULL;

void C0::init() {
    //printf("    XXX C0::c = %p, C0::cc = %p\n", C0::c, C0::cc);
    //printf("    XXX C1::c = %p, C1::cc = %p\n", C1::c, C1::cc);
    assert(!C0::c);
    assert(!C0::cc);
    assert(C1::c);
    assert(C1::cc);
    C0::c = C1::c;
    C0::cc = C1::cc;
    //printf("    YYY C0::c = %p, C0::cc = %p\n", C0::c, C0::cc);
}

void C0::finit() {
    //printf("    ZZZ C0::c = %p, C0::cc = %p\n", C0::c, C0::cc);
    assert(C0::c);
    assert(C0::cc);
    C0::c = NULL;
    C0::cc = NULL;
}

void C1::init() {
    //printf("    XXX C1::c = %p, C1::cc = %p\n", C1::c, C1::cc);
    assert(!C1::c);
    assert(!C1::cc);
    C1::c = new C1();
    C1::cc = new C1();
    //printf("    YYY C1::c = %p, C1::cc = %p\n", C1::c, C1::cc);
}

void C1::finit() {
    //printf("    ZZZ C1::c = %p, C1::cc = %p\n", C1::c, C1::cc);
    assert(C1::c);
    assert(C1::cc);
    delete C1::c;
    delete C1::cc;
    C1::c = NULL;
    C1::cc = NULL;
}

// ----------------------------------------

D0 * D0::d = NULL;
D1 * D1::d = NULL;
D2 * D2::d = NULL;

void D0::init() {
    //printf("    XXX D0::d = %p\n", D0::d);
    assert(!D0::d);
    D0::d = new D0();
    //printf("    YYY D0::d = %p\n", D0::d);
}

void D0::finit() {
    //printf("    ZZZ D0::d = %p\n", D0::d);
    assert(D0::d);
    delete D0::d;
    D0::d = NULL;
}

void D1::init() {
    //printf("    XXX D1::d = %p\n", D1::d);
    assert(!D1::d);
    D1::d = new D1();
    //printf("    YYY D1::d = %p\n", D1::d);
}

void D1::finit() {
    //printf("    ZZZ D1::d = %p\n", D1::d);
    assert(D1::d);
    delete D1::d;
    D1::d = NULL;
}

void D2::init() {
    //printf("    XXX D2::d = %p\n", D2::d);
    assert(!D2::d);
    D2::d = new D2();
    //printf("    YYY D2::d = %p\n", D2::d);
}

void D2::finit() {
    //printf("    ZZZ D2::d = %p\n", D2::d);
    assert(D2::d);
    delete D2::d;
    D2::d = NULL;
}

// ----------------------------------------

void myapi_init() {
    // some order dependencies
    D2::init();
    D1::init();
    D0::init();
    C1::init();
    C0::init();
    B1::init();
    B0::init();
    A::init();
}

void myapi_finit() {
    A::finit();
    B0::finit();
    B1::finit();
    C0::finit();
    C1::finit();
    D0::finit();
    D1::finit();
    D2::finit();
}

// ---------------------------------------------------------------------------

void f0()
{
    TRACE("void f0()");
}

// ---------------------------------------------------------------------------

static const char * XYZ = "XYZ";
static char xyz[4] = { 'x', 'y', 'z', '\0' };

const void * s010()
{
    TRACE("const void * s010()");
    return XYZ;
}

const char * s012()
{
    TRACE("const char * s012()");
    return XYZ;
}

void * s030()
{
    TRACE("void * s030()");
    return xyz;
}

char * s032()
{
    TRACE("char * s032()");
    return xyz;
}

const void * const s050()
{
    TRACE("const void * const s050()");
    return XYZ;
}

const char * const s052()
{
    TRACE("const char * const s052()");
    return XYZ;
}

void * const s070()
{
    TRACE("void * const s070()");
    return xyz;
}

char * const s072()
{
    TRACE("char * const s072()");
    return xyz;
}

void s110(const void * p0)
{
    TRACE("void s110(const void *)");
    CHECK((strcmp((const char*)p0, xyz) != 0 && strcmp((const char*)p0, XYZ) != 0),
          "void s110(const void *)");
}

void s112(const char * p0)
{
    TRACE("void s112(const char *)");
    CHECK((strcmp(p0, xyz) != 0 && strcmp(p0, XYZ) != 0),
          "void s112(const char *)");
}

void s130(void * p0)
{
    TRACE("void s130(void *)");
    CHECK((strcmp((const char*)p0, xyz) != 0 && strcmp((const char*)p0, XYZ) != 0),
          "void s130(void *)");
}

void s132(char * p0)
{
    TRACE("void s132(char *)");
    CHECK((strcmp(p0, xyz) != 0 && strcmp(p0, XYZ) != 0),
          "void s132(char *)");
}

void s150(const void * const p0)
{
    TRACE("void s150(const void * const)");
    CHECK((strcmp((const char*)p0, xyz) != 0 && strcmp((const char*)p0, XYZ) != 0),
          "void s150(const void * const)");
}

void s152(const char * const p0)
{
    TRACE("void s152(const char * const)");
    CHECK((strcmp(p0, xyz) != 0 && strcmp(p0, XYZ) != 0),
          "void s152(const char * const)");
}

void s170(void * const p0)
{
    TRACE("void s170(void * const)");
    CHECK((strcmp((const char*)p0, xyz) != 0 && strcmp((const char*)p0, XYZ) != 0),
          "void s170(void * const)");
}

void s172(char * const p0)
{
    TRACE("void s172(char * const)");
    CHECK((strcmp(p0, xyz) != 0 && strcmp(p0, XYZ) != 0),
          "void s172(char * const)");
}

// ---------------------------------------------------------------------------

const void * s210()
{
    TRACE("const void * s210()");
    return NULL;
}

const char * s212()
{
    TRACE("const char * s212()");
    return NULL;
}

void * s230()
{
    TRACE("void * s230()");
    return NULL;
}

char * s232()
{
    TRACE("char * s232()");
    return NULL;
}

const void * const s250()
{
    TRACE("const void * const s250()");
    return NULL;
}

const char * const s252()
{
    TRACE("const char * const s252()");
    return NULL;
}

void * const s270()
{
    TRACE("void * const s270()");
    return NULL;
}

char * const s272()
{
    TRACE("char * const s272()");
    return NULL;
}

void s310(const void * p0)
{
    TRACE("void s310(const void *)");
    (void)p0;
}

void s312(const char * p0)
{
    TRACE("void s312(const char *)");
    (void)p0;
}

void s330(void * p0)
{
    TRACE("void s330(void *)");
    (void)p0;
}

void s332(char * p0)
{
    TRACE("void s332(char *)");
    (void)p0;
}

void s350(const void * const p0)
{
    TRACE("void s350(const void * const)");
    (void)p0;
}

void s352(const char * const p0)
{
    TRACE("void s352(const char * const)");
    (void)p0;
}

void s370(void * const p0)
{
    TRACE("void s370(void * const)");
    (void)p0;
}

void s372(char * const p0)
{
    TRACE("void s372(char * const)");
    (void)p0;
}

// ---------------------------------------------------------------------------

const bool f11(const bool p0)
{
    TRACE("const bool f11(const bool)");
    return p0;
}

const char f12(const char p0)
{
    TRACE("const char f12(const char)");
    return p0;
}

const signed char f13(const signed char p0)
{
    TRACE("const signed char f13(const signed char)");
    return p0;
}

const unsigned char f14(const unsigned char p0)
{
    TRACE("const unsigned char f14(const unsigned char)");
    return p0;
}

const signed short f15(const signed short p0)
{
    TRACE("const signed short f15(const signed short)");
    return p0;
}

const unsigned short f16(const unsigned short p0)
{
    TRACE("const unsigned short f16(const unsigned short)");
    return p0;
}

const signed int f17(const signed int p0)
{
    TRACE("const signed int f17(const signed int)");
    return p0;
}

const unsigned int f18(const unsigned int p0)
{
    TRACE("const unsigned int f18(const unsigned int)");
    return p0;
}

const signed long f19(const signed long p0)
{
    TRACE("const signed long f19(const signed long)");
    return p0;
}

const unsigned long f20(const unsigned long p0)
{
    TRACE("const unsigned long f20(const unsigned long)");
    return p0;
}

const signed long long f21(const signed long long p0)
{
    TRACE("const signed long long f21(const signed long long)");
    return p0;
}

const unsigned long long f22(const unsigned long long p0)
{
    TRACE("const unsigned long long f22(const unsigned long long)");
    return p0;
}

const float f23(const float p0)
{
    TRACE("const float f23(const float)");
    return p0;
}

const double f24(const double p0)
{
    TRACE("const double f24(const double)");
    return p0;
}

const long double f25(const long double p0)
{
    TRACE("const long double f25(const long double)");
    return p0;
}

bool f31(bool p0)
{
    TRACE("bool f31(bool)");
    return p0;
}

char f32(char p0)
{
    TRACE("char f32(char)");
    return p0;
}

signed char f33(signed char p0)
{
    TRACE("signed char f33(signed char)");
    return p0;
}

unsigned char f34(unsigned char p0)
{
    TRACE("unsigned char f34(unsigned char)");
    return p0;
}

signed short f35(signed short p0)
{
    TRACE("signed short f35(signed short)");
    return p0;
}

unsigned short f36(unsigned short p0)
{
    TRACE("unsigned short f36(unsigned short)");
    return p0;
}

signed int f37(signed int p0)
{
    TRACE("signed int f37(signed int)");
    return p0;
}

unsigned int f38(unsigned int p0)
{
    TRACE("unsigned int f38(unsigned int)");
    return p0;
}

signed long f39(signed long p0)
{
    TRACE("signed long f39(signed long)");
    return p0;
}

unsigned long f40(unsigned long p0)
{
    TRACE("unsigned long f40(unsigned long)");
    return p0;
}

signed long long f41(signed long long p0)
{
    TRACE("signed long long f41(signed long long)");
    return p0;
}

unsigned long long f42(unsigned long long p0)
{
    TRACE("unsigned long long f42(unsigned long long)");
    return p0;
}

float f43(float p0)
{
    TRACE("float f43(float)");
    return p0;
}

double f44(double p0)
{
    TRACE("double f44(double)");
    return p0;
}

long double f45(long double p0)
{
    TRACE("long double f45(long double)");
    return p0;
}

// ---------------------------------------------------------------------------

const bool f011()
{
    TRACE("const bool f011()");
    static bool _f011 = 0;
    _f011 = !_f011;
    return _f011;
}

const char f012()
{
    TRACE("const char f012()");
    static char _f012 = 0;
    _f012++;
    return _f012;
}

const int8_t f013()
{
    TRACE("const int8_t f013()");
    static int8_t _f013 = 0;
    _f013++;
    return _f013;
}

const uint8_t f014()
{
    TRACE("const uint8_t f014()");
    static uint8_t _f014 = 0;
    _f014++;
    return _f014;
}

const int16_t f015()
{
    TRACE("const int16_t f015()");
    static int16_t _f015 = 0;
    _f015++;
    return _f015;
}

const uint16_t f016()
{
    TRACE("const uint16_t f016()");
    static uint16_t _f016 = 0;
    _f016++;
    return _f016;
}

const int32_t f017()
{
    TRACE("const int32_t f017()");
    static int32_t _f017 = 0;
    _f017++;
    return _f017;
}

const uint32_t f018()
{
    TRACE("const uint32_t f018()");
    static uint32_t _f018 = 0;
    _f018++;
    return _f018;
}

const int64_t f021()
{
    TRACE("const int64_t f021()");
    static int64_t _f021 = 0;
    _f021++;
    return _f021;
}

const uint64_t f022()
{
    TRACE("const uint64_t f022()");
    static uint64_t _f022 = 0;
    _f022++;
    return _f022;
}

const float f023()
{
    TRACE("const float f023()");
    static float _f023 = 0;
    _f023++;
    return _f023;
}

const double f024()
{
    TRACE("const double f024()");
    static double _f024 = 0;
    _f024++;
    return _f024;
}

bool f031()
{
    TRACE("bool f031()");
    static bool _f031 = 0;
    _f031 = !_f031;
    return _f031;
}

char f032()
{
    TRACE("char f032()");
    static char _f032 = 0;
    _f032++;
    return _f032;
}

int8_t f033()
{
    TRACE("int8_t f033()");
    static int8_t _f033 = 0;
    _f033++;
    return _f033;
}

uint8_t f034()
{
    TRACE("uint8_t f034()");
    static uint8_t _f034 = 0;
    _f034++;
    return _f034;
}

int16_t f035()
{
    TRACE("int16_t f035()");
    static int16_t _f035 = 0;
    _f035++;
    return _f035;
}

uint16_t f036()
{
    TRACE("uint16_t f036()");
    static uint16_t _f036 = 0;
    _f036++;
    return _f036;
}

int32_t f037()
{
    TRACE("int32_t f037()");
    static int32_t _f037 = 0;
    _f037++;
    return _f037;
}

uint32_t f038()
{
    TRACE("uint32_t f038()");
    static uint32_t _f038 = 0;
    _f038++;
    return _f038;
}

int64_t f041()
{
    TRACE("int64_t f041()");
    static int64_t _f041 = 0;
    _f041++;
    return _f041;
}

uint64_t f042()
{
    TRACE("uint64_t f042()");
    static uint64_t _f042 = 0;
    _f042++;
    return _f042;
}

float f043()
{
    TRACE("float f043()");
    static float _f043 = 0;
    _f043++;
    return _f043;
}

double f044()
{
    TRACE("double f044()");
    static double _f044 = 0;
    _f044++;
    return _f044;
}

// ---------------------------------------------------------------------------

void f111(const bool p0)
{
    TRACE("void f111(const bool)");
    static bool _f111 = 0;
    _f111 = !_f111;
    CHECK((p0 != _f111),
          "void f111(const bool)");
}

void f112(const char p0)
{
    TRACE("void f112(const char)");
    static char _f112 = 0;
    _f112++;
    CHECK((p0 != _f112),
          "void f112(const char)");
}

void f113(const int8_t p0)
{
    TRACE("void f113(const int8_t)");
    static int8_t _f113 = 0;
    _f113++;
    CHECK((p0 != _f113),
          "void f113(const int8_t)");
}

void f114(const uint8_t p0)
{
    TRACE("void f114(const uint8_t)");
    static uint8_t _f114 = 0;
    _f114++;
    CHECK((p0 != _f114),
          "void f114(const uint8_t)");
}

void f115(const int16_t p0)
{
    TRACE("void f115(const int16_t)");
    static int16_t _f115 = 0;
    _f115++;
    CHECK((p0 != _f115),
          "void f115(const int16_t)");
}

void f116(const uint16_t p0)
{
    TRACE("void f116(const uint16_t)");
    static uint16_t _f116 = 0;
    _f116++;
    CHECK((p0 != _f116),
          "void f116(const uint16_t)");
}

void f117(const int32_t p0)
{
    TRACE("void f117(const int32_t)");
    static int32_t _f117 = 0;
    _f117++;
    CHECK((p0 != _f117),
          "void f117(const int32_t)");
}

void f118(const uint32_t p0)
{
    TRACE("void f118(const uint32_t)");
    static uint32_t _f118 = 0;
    _f118++;
    CHECK((p0 != _f118),
          "void f118(const uint32_t)");
}

void f121(const int64_t p0)
{
    TRACE("void f121(const int64_t)");
    static int64_t _f121 = 0;
    _f121++;
    CHECK((p0 != _f121),
          "void f121(const int64_t)");
}

void f122(const uint64_t p0)
{
    TRACE("void f122(const uint64_t)");
    static uint64_t _f122 = 0;
    _f122++;
    CHECK((p0 != _f122),
          "void f122(const uint64_t)");
}

void f123(const float p0)
{
    TRACE("void f123(const float)");
    static float _f123 = 0;
    _f123++;
    CHECK((p0 != _f123),
          "void f123(const float)");
}

void f124(const double p0)
{
    TRACE("void f124(const double)");
    static double _f124 = 0;
    _f124++;
    CHECK((p0 != _f124),
          "void f124(const double)");
}

void f131(bool p0)
{
    TRACE("void f131(bool)");
    static bool _f131 = 0;
    _f131 = !_f131;
    CHECK((p0 != _f131),
          "void f131(bool)");
}

void f132(char p0)
{
    TRACE("void f132(char)");
    static char _f132 = 0;
    _f132++;
    CHECK((p0 != _f132),
          "void f132(char)");
}

void f133(int8_t p0)
{
    TRACE("void f133(int8_t)");
    static int8_t _f133 = 0;
    _f133++;
    CHECK((p0 != _f133),
          "void f133(int8_t)");
}

void f134(uint8_t p0)
{
    TRACE("void f134(uint8_t)");
    static uint8_t _f134 = 0;
    _f134++;
    CHECK((p0 != _f134),
          "void f134(uint8_t)");
}

void f135(int16_t p0)
{
    TRACE("void f135(int16_t)");
    static int16_t _f135 = 0;
    _f135++;
    CHECK((p0 != _f135),
          "void f135(int16_t)");
}

void f136(uint16_t p0)
{
    TRACE("void f136(uint16_t)");
    static uint16_t _f136 = 0;
    _f136++;
    CHECK((p0 != _f136),
          "void f136(uint16_t)");
}

void f137(int32_t p0)
{
    TRACE("void f137(int32_t)");
    static int32_t _f137 = 0;
    _f137++;
    CHECK((p0 != _f137),
          "void f137(int32_t)");
}

void f138(uint32_t p0)
{
    TRACE("void f138(uint32_t)");
    static uint32_t _f138 = 0;
    _f138++;
    CHECK((p0 != _f138),
          "void f138(uint32_t)");
}

void f141(int64_t p0)
{
    TRACE("void f141(int64_t)");
    static int64_t _f141 = 0;
    _f141++;
    CHECK((p0 != _f141),
          "void f141(int64_t)");
}

void f142(uint64_t p0)
{
    TRACE("void f142(uint64_t)");
    static uint64_t _f142 = 0;
    _f142++;
    CHECK((p0 != _f142),
          "void f142(uint64_t)");
}

void f143(float p0)
{
    TRACE("void f143(float)");
    static float _f143 = 0;
    _f143++;
    CHECK((p0 != _f143),
          "void f143(float)");
}

void f144(double p0)
{
    TRACE("void f144(double)");
    static double _f144 = 0;
    _f144++;
    CHECK((p0 != _f144),
          "void f144(double)");
}

// ---------------------------------------------------------------------------

const bool & f211()
{
    TRACE("const bool & f211()");
    static bool _f211 = 0;
    _f211 = !_f211;
    return _f211;
}

const char & f212()
{
    TRACE("const char & f212()");
    static char _f212 = 0;
    _f212++;
    return _f212;
}

const int8_t & f213()
{
    TRACE("const int8_t & f213()");
    static int8_t _f213 = 0;
    _f213++;
    return _f213;
}

const uint8_t & f214()
{
    TRACE("const uint8_t & f214()");
    static uint8_t _f214 = 0;
    _f214++;
    return _f214;
}

const int16_t & f215()
{
    TRACE("const int16_t & f215()");
    static int16_t _f215 = 0;
    _f215++;
    return _f215;
}

const uint16_t & f216()
{
    TRACE("const uint16_t & f216()");
    static uint16_t _f216 = 0;
    _f216++;
    return _f216;
}

const int32_t & f217()
{
    TRACE("const int32_t & f217()");
    static int32_t _f217 = 0;
    _f217++;
    return _f217;
}

const uint32_t & f218()
{
    TRACE("const uint32_t & f218()");
    static uint32_t _f218 = 0;
    _f218++;
    return _f218;
}

const int64_t & f221()
{
    TRACE("const int64_t & f221()");
    static int64_t _f221 = 0;
    _f221++;
    return _f221;
}

const uint64_t & f222()
{
    TRACE("const uint64_t & f222()");
    static uint64_t _f222 = 0;
    _f222++;
    return _f222;
}

const float & f223()
{
    TRACE("const & float f223()");
    static float _f223 = 0;
    _f223++;
    return _f223;
}

const double & f224()
{
    TRACE("const double & f224()");
    static double _f224 = 0;
    _f224++;
    return _f224;
}

bool & f231()
{
    TRACE("bool & f231()");
    static bool _f231 = 0;
    _f231 = !_f231;
    return _f231;
}

char & f232()
{
    TRACE("char & f232()");
    static char _f232 = 0;
    _f232++;
    return _f232;
}

int8_t & f233()
{
    TRACE("int8_t & f233()");
    static int8_t _f233 = 0;
    _f233++;
    return _f233;
}

uint8_t & f234()
{
    TRACE("uint8_t & f234()");
    static uint8_t _f234 = 0;
    _f234++;
    return _f234;
}

int16_t & f235()
{
    TRACE("int16_t & f235()");
    static int16_t _f235 = 0;
    _f235++;
    return _f235;
}

uint16_t & f236()
{
    TRACE("uint16_t & f236()");
    static uint16_t _f236 = 0;
    _f236++;
    return _f236;
}

int32_t & f237()
{
    TRACE("int32_t & f237()");
    static int32_t _f237 = 0;
    _f237++;
    return _f237;
}

uint32_t & f238()
{
    TRACE("uint32_t & f238()");
    static uint32_t _f238 = 0;
    _f238++;
    return _f238;
}

int64_t & f241()
{
    TRACE("int64_t & f241()");
    static int64_t _f241 = 0;
    _f241++;
    return _f241;
}

uint64_t & f242()
{
    TRACE("uint64_t & f242()");
    static uint64_t _f242 = 0;
    _f242++;
    return _f242;
}

float & f243()
{
    TRACE("float & f243()");
    static float _f243 = 0;
    _f243++;
    return _f243;
}

double & f244()
{
    TRACE("double & f244()");
    static double _f244 = 0;
    _f244++;
    return _f244;
}

// ---------------------------------------------------------------------------

void f311(const bool & p0)
{
    TRACE("void f311(const bool &)");
    static bool _f311 = 0;
    _f311 = !_f311;
    CHECK((p0 != _f311),
          "void f311(const bool &)");
}

void f312(const char & p0)
{
    TRACE("void f312(const char &)");
    static char _f312 = 0;
    _f312++;
    CHECK((p0 != _f312),
          "void f312(const char &)");
}

void f313(const int8_t & p0)
{
    TRACE("void f313(const int8_t &)");
    static int8_t _f313 = 0;
    _f313++;
    CHECK((p0 != _f313),
          "void f313(const int8_t &)");
}

void f314(const uint8_t & p0)
{
    TRACE("void f314(const uint8_t &)");
    static uint8_t _f314 = 0;
    _f314++;
    CHECK((p0 != _f314),
          "void f314(const uint8_t &)");
}

void f315(const int16_t & p0)
{
    TRACE("void f315(const int16_t &)");
    static int16_t _f315 = 0;
    _f315++;
    CHECK((p0 != _f315),
          "void f315(const int16_t &)");
}

void f316(const uint16_t & p0)
{
    TRACE("void f316(const uint16_t &)");
    static uint16_t _f316 = 0;
    _f316++;
    CHECK((p0 != _f316),
          "void f316(const uint16_t &)");
}

void f317(const int32_t & p0)
{
    TRACE("void f317(const int32_t &)");
    static int32_t _f317 = 0;
    _f317++;
    CHECK((p0 != _f317),
          "void f317(const int32_t &)");
}

void f318(const uint32_t & p0)
{
    TRACE("void f318(const uint32_t &)");
    static uint32_t _f318 = 0;
    _f318++;
    CHECK((p0 != _f318),
          "void f318(const uint32_t &)");
}

void f321(const int64_t & p0)
{
    TRACE("void f321(const int64_t &)");
    static int64_t _f321 = 0;
    _f321++;
    CHECK((p0 != _f321),
          "void f321(const int64_t &)");
}

void f322(const uint64_t & p0)
{
    TRACE("void f322(const uint64_t &)");
    static uint64_t _f322 = 0;
    _f322++;
    CHECK((p0 != _f322),
          "void f322(const uint64_t &)");
}

void f323(const float & p0)
{
    TRACE("void f323(const float &)");
    static float _f323 = 0;
    _f323++;
    CHECK((p0 != _f323),
          "void f323(const float &)");
}

void f324(const double & p0)
{
    TRACE("void f324(const double &)");
    static double _f324 = 0;
    _f324++;
    CHECK((p0 != _f324),
          "void f324(const double &)");
}

void f331(bool & p0)
{
    TRACE("void f331(bool &)");
    static bool _f331 = 0;
    _f331 = !_f331;
    CHECK((p0 != _f331),
          "void f331(bool &)");
    p0 = !p0;
    _f331 = !_f331;
}

void f332(char & p0)
{
    TRACE("void f332(char &)");
    static char _f332 = 0;
    _f332++;
    CHECK((p0 != _f332),
          "void f332(char &)");
    p0++;
    _f332++;
}

void f333(int8_t & p0)
{
    TRACE("void f333(int8_t &)");
    static int8_t _f333 = 0;
    _f333++;
    CHECK((p0 != _f333),
          "void f333(int8_t &)");
    p0++;
    _f333++;
}

void f334(uint8_t & p0)
{
    TRACE("void f334(uint8_t &)");
    static uint8_t _f334 = 0;
    _f334++;
    CHECK((p0 != _f334),
          "void f334(uint8_t &)");
    p0++;
    _f334++;
}

void f335(int16_t & p0)
{
    TRACE("void f335(int16_t &)");
    static int16_t _f335 = 0;
    _f335++;
    CHECK((p0 != _f335),
          "void f335(int16_t &)");
    p0++;
    _f335++;
}

void f336(uint16_t & p0)
{
    TRACE("void f336(uint16_t &)");
    static uint16_t _f336 = 0;
    _f336++;
    CHECK((p0 != _f336),
          "void f336(uint16_t &)");
    p0++;
    _f336++;
}

void f337(int32_t & p0)
{
    TRACE("void f337(int32_t &)");
    static int32_t _f337 = 0;
    _f337++;
    CHECK((p0 != _f337),
          "void f337(int32_t &)");
    p0++;
    _f337++;
}

void f338(uint32_t & p0)
{
    TRACE("void f338(uint32_t &)");
    static uint32_t _f338 = 0;
    _f338++;
    CHECK((p0 != _f338),
          "void f338(uint32_t &)");
    p0++;
    _f338++;
}

void f341(int64_t & p0)
{
    TRACE("void f341(int64_t &)");
    static int64_t _f341 = 0;
    _f341++;
    CHECK((p0 != _f341),
          "void f341(int64_t &)");
    p0++;
    _f341++;
}

void f342(uint64_t & p0)
{
    TRACE("void f342(uint64_t &)");
    static uint64_t _f342 = 0;
    _f342++;
    CHECK((p0 != _f342),
          "void f342(uint64_t &)");
    p0++;
    _f342++;
}

void f343(float & p0)
{
    TRACE("void f343(float &)");
    static float _f343 = 0;
    _f343++;
    CHECK((p0 != _f343),
          "void f343(float &)");
    p0++;
    _f343++;
}

void f344(double & p0)
{
    TRACE("void f344(double &)");
    static double _f344 = 0;
    _f344++;
    CHECK((p0 != _f344),
          "void f344(double &)");
    p0++;
    _f344++;
}

// ---------------------------------------------------------------------------

const bool * f411()
{
    TRACE("const bool * f411()");
    static bool _f411 = 0;
    _f411 = !_f411;
    return &_f411;
}

const char * f412()
{
    TRACE("const char * f412()");
    static char _f412 = 0;
    _f412++;
    return &_f412;
}

const int8_t * f413()
{
    TRACE("const int8_t * f413()");
    static int8_t _f413 = 0;
    _f413++;
    return &_f413;
}

const uint8_t * f414()
{
    TRACE("const uint8_t * f414()");
    static uint8_t _f414 = 0;
    _f414++;
    return &_f414;
}

const int16_t * f415()
{
    TRACE("const int16_t * f415()");
    static int16_t _f415 = 0;
    _f415++;
    return &_f415;
}

const uint16_t * f416()
{
    TRACE("const uint16_t * f416()");
    static uint16_t _f416 = 0;
    _f416++;
    return &_f416;
}

const int32_t * f417()
{
    TRACE("const int32_t * f417()");
    static int32_t _f417 = 0;
    _f417++;
    return &_f417;
}

const uint32_t * f418()
{
    TRACE("const uint32_t * f418()");
    static uint32_t _f418 = 0;
    _f418++;
    return &_f418;
}

const int64_t * f421()
{
    TRACE("const int64_t * f421()");
    static int64_t _f421 = 0;
    _f421++;
    return &_f421;
}

const uint64_t * f422()
{
    TRACE("const uint64_t * f422()");
    static uint64_t _f422 = 0;
    _f422++;
    return &_f422;
}

const float * f423()
{
    TRACE("const * float f423()");
    static float _f423 = 0;
    _f423++;
    return &_f423;
}

const double * f424()
{
    TRACE("const double * f424()");
    static double _f424 = 0;
    _f424++;
    return &_f424;
}

bool * f431()
{
    TRACE("bool * f431()");
    static bool _f431 = 0;
    _f431 = !_f431;
    return &_f431;
}

char * f432()
{
    TRACE("char * f432()");
    static char _f432 = 0;
    _f432++;
    return &_f432;
}

int8_t * f433()
{
    TRACE("int8_t * f433()");
    static int8_t _f433 = 0;
    _f433++;
    return &_f433;
}

uint8_t * f434()
{
    TRACE("uint8_t * f434()");
    static uint8_t _f434 = 0;
    _f434++;
    return &_f434;
}

int16_t * f435()
{
    TRACE("int16_t * f435()");
    static int16_t _f435 = 0;
    _f435++;
    return &_f435;
}

uint16_t * f436()
{
    TRACE("uint16_t * f436()");
    static uint16_t _f436 = 0;
    _f436++;
    return &_f436;
}

int32_t * f437()
{
    TRACE("int32_t * f437()");
    static int32_t _f437 = 0;
    _f437++;
    return &_f437;
}

uint32_t * f438()
{
    TRACE("uint32_t * f438()");
    static uint32_t _f438 = 0;
    _f438++;
    return &_f438;
}

int64_t * f441()
{
    TRACE("int64_t * f441()");
    static int64_t _f441 = 0;
    _f441++;
    return &_f441;
}

uint64_t * f442()
{
    TRACE("uint64_t * f442()");
    static uint64_t _f442 = 0;
    _f442++;
    return &_f442;
}

float * f443()
{
    TRACE("float * f443()");
    static float _f443 = 0;
    _f443++;
    return &_f443;
}

double * f444()
{
    TRACE("double * f444()");
    static double _f444 = 0;
    _f444++;
    return &_f444;
}

const bool * const f451()
{
    TRACE("const bool * const f451()");
    static bool _f451 = 0;
    _f451 = !_f451;
    return &_f451;
}

const char * const f452()
{
    TRACE("const char * const f452()");
    static char _f452 = 0;
    _f452++;
    return &_f452;
}

const int8_t * const f453()
{
    TRACE("const int8_t * const f453()");
    static int8_t _f453 = 0;
    _f453++;
    return &_f453;
}

const uint8_t * const f454()
{
    TRACE("const uint8_t * const f454()");
    static uint8_t _f454 = 0;
    _f454++;
    return &_f454;
}

const int16_t * const f455()
{
    TRACE("const int16_t * const f455()");
    static int16_t _f455 = 0;
    _f455++;
    return &_f455;
}

const uint16_t * const f456()
{
    TRACE("const uint16_t * const f456()");
    static uint16_t _f456 = 0;
    _f456++;
    return &_f456;
}

const int32_t * const f457()
{
    TRACE("const int32_t * const f457()");
    static int32_t _f457 = 0;
    _f457++;
    return &_f457;
}

const uint32_t * const f458()
{
    TRACE("const uint32_t * const f458()");
    static uint32_t _f458 = 0;
    _f458++;
    return &_f458;
}

const int64_t * const f461()
{
    TRACE("const int64_t * const f461()");
    static int64_t _f461 = 0;
    _f461++;
    return &_f461;
}

const uint64_t * const f462()
{
    TRACE("const uint64_t * const f462()");
    static uint64_t _f462 = 0;
    _f462++;
    return &_f462;
}

const float * const f463()
{
    TRACE("const * float const f463()");
    static float _f463 = 0;
    _f463++;
    return &_f463;
}

const double * const f464()
{
    TRACE("const double * const f464()");
    static double _f464 = 0;
    _f464++;
    return &_f464;
}

bool * const f471()
{
    TRACE("bool * const f471()");
    static bool _f471 = 0;
    _f471 = !_f471;
    return &_f471;
}

char * const f472()
{
    TRACE("char * const f472()");
    static char _f472 = 0;
    _f472++;
    return &_f472;
}

int8_t * const f473()
{
    TRACE("int8_t * const f473()");
    static int8_t _f473 = 0;
    _f473++;
    return &_f473;
}

uint8_t * const f474()
{
    TRACE("uint8_t * const f474()");
    static uint8_t _f474 = 0;
    _f474++;
    return &_f474;
}

int16_t * const f475()
{
    TRACE("int16_t * const f475()");
    static int16_t _f475 = 0;
    _f475++;
    return &_f475;
}

uint16_t * const f476()
{
    TRACE("uint16_t * const f476()");
    static uint16_t _f476 = 0;
    _f476++;
    return &_f476;
}

int32_t * const f477()
{
    TRACE("int32_t * const f477()");
    static int32_t _f477 = 0;
    _f477++;
    return &_f477;
}

uint32_t * const f478()
{
    TRACE("uint32_t * const f478()");
    static uint32_t _f478 = 0;
    _f478++;
    return &_f478;
}

int64_t * const f481()
{
    TRACE("int64_t * const f481()");
    static int64_t _f481 = 0;
    _f481++;
    return &_f481;
}

uint64_t * const f482()
{
    TRACE("uint64_t * const f482()");
    static uint64_t _f482 = 0;
    _f482++;
    return &_f482;
}

float * const f483()
{
    TRACE("float * const f483()");
    static float _f483 = 0;
    _f483++;
    return &_f483;
}

double * const f484()
{
    TRACE("double * const f484()");
    static double _f484 = 0;
    _f484++;
    return &_f484;
}

// ---------------------------------------------------------------------------

void f511(const bool * p0)
{
    TRACE("void f511(const bool *)");
    static bool _f511 = 0;
    _f511 = !_f511;
    CHECK((*p0 != _f511),
          "void f511(const bool *)");
}

void f512(const char * p0)
{
    TRACE("void f512(const char *)");
    static char _f512 = 0;
    _f512++;
    CHECK((*p0 != _f512),
          "void f512(const char *)");
}

void f513(const int8_t * p0)
{
    TRACE("void f513(const int8_t *)");
    static int8_t _f513 = 0;
    _f513++;
    CHECK((*p0 != _f513),
          "void f513(const int8_t *)");
}

void f514(const uint8_t * p0)
{
    TRACE("void f514(const uint8_t *)");
    static uint8_t _f514 = 0;
    _f514++;
    CHECK((*p0 != _f514),
          "void f514(const uint8_t *)");
}

void f515(const int16_t * p0)
{
    TRACE("void f515(const int16_t *)");
    static int16_t _f515 = 0;
    _f515++;
    CHECK((*p0 != _f515),
          "void f515(const int16_t *)");
}

void f516(const uint16_t * p0)
{
    TRACE("void f516(const uint16_t *)");
    static uint16_t _f516 = 0;
    _f516++;
    CHECK((*p0 != _f516),
          "void f516(const uint16_t *)");
}

void f517(const int32_t * p0)
{
    TRACE("void f517(const int32_t *)");
    static int32_t _f517 = 0;
    _f517++;
    CHECK((*p0 != _f517),
          "void f517(const int32_t *)");
}

void f518(const uint32_t * p0)
{
    TRACE("void f518(const uint32_t *)");
    static uint32_t _f518 = 0;
    _f518++;
    CHECK((*p0 != _f518),
          "void f518(const uint32_t *)");
}

void f521(const int64_t * p0)
{
    TRACE("void f521(const int64_t *)");
    static int64_t _f521 = 0;
    _f521++;
    CHECK((*p0 != _f521),
          "void f521(const int64_t *)");
}

void f522(const uint64_t * p0)
{
    TRACE("void f522(const uint64_t *)");
    static uint64_t _f522 = 0;
    _f522++;
    CHECK((*p0 != _f522),
          "void f522(const uint64_t *)");
}

void f523(const float * p0)
{
    TRACE("void f523(const float *)");
    static float _f523 = 0;
    _f523++;
    CHECK((*p0 != _f523),
          "void f523(const float *)");
}

void f524(const double * p0)
{
    TRACE("void f524(const double *)");
    static double _f524 = 0;
    _f524++;
    CHECK((*p0 != _f524),
          "void f524(const double *)");
}

void f531(bool * p0)
{
    TRACE("void f531(bool *)");
    static bool _f531 = 0;
    _f531 = !_f531;
    CHECK((*p0 != _f531),
          "void f531(bool *)");
    *p0 = !*p0;
    _f531 = !_f531;
}

void f532(char * p0)
{
    TRACE("void f532(char *)");
    static char _f532 = 0;
    _f532++;
    CHECK((*p0 != _f532),
          "void f532(char *)");
    (*p0)++;
    _f532++;
}

void f533(int8_t * p0)
{
    TRACE("void f533(int8_t *)");
    static int8_t _f533 = 0;
    _f533++;
    CHECK((*p0 != _f533),
          "void f533(int8_t *)");
    (*p0)++;
    _f533++;
}

void f534(uint8_t * p0)
{
    TRACE("void f534(uint8_t *)");
    static uint8_t _f534 = 0;
    _f534++;
    CHECK((*p0 != _f534),
          "void f534(uint8_t *)");
    (*p0)++;
    _f534++;
}

void f535(int16_t * p0)
{
    TRACE("void f535(int16_t *)");
    static int16_t _f535 = 0;
    _f535++;
    CHECK((*p0 != _f535),
          "void f535(int16_t *)");
    (*p0)++;
    _f535++;
}

void f536(uint16_t * p0)
{
    TRACE("void f536(uint16_t *)");
    static uint16_t _f536 = 0;
    _f536++;
    CHECK((*p0 != _f536),
          "void f536(uint16_t *)");
    (*p0)++;
    _f536++;
}

void f537(int32_t * p0)
{
    TRACE("void f537(int32_t *)");
    static int32_t _f537 = 0;
    _f537++;
    CHECK((*p0 != _f537),
          "void f537(int32_t *)");
    (*p0)++;
    _f537++;
}

void f538(uint32_t * p0)
{
    TRACE("void f538(uint32_t *)");
    static uint32_t _f538 = 0;
    _f538++;
    CHECK((*p0 != _f538),
          "void f538(uint32_t *)");
    (*p0)++;
    _f538++;
}

void f541(int64_t * p0)
{
    TRACE("void f541(int64_t *)");
    static int64_t _f541 = 0;
    _f541++;
    CHECK((*p0 != _f541),
          "void f541(int64_t *)");
    (*p0)++;
    _f541++;
}

void f542(uint64_t * p0)
{
    TRACE("void f542(uint64_t *)");
    static uint64_t _f542 = 0;
    _f542++;
    CHECK((*p0 != _f542),
          "void f542(uint64_t *)");
    (*p0)++;
    _f542++;
}

void f543(float * p0)
{
    TRACE("void f543(float *)");
    static float _f543 = 0;
    _f543++;
    CHECK((*p0 != _f543),
          "void f543(float *)");
    (*p0)++;
    _f543++;
}

void f544(double * p0)
{
    TRACE("void f544(double *)");
    static double _f544 = 0;
    _f544++;
    CHECK((*p0 != _f544),
          "void f544(double *)");
    (*p0)++;
    _f544++;
}

void f551(const bool * const p0)
{
    TRACE("void f551(const bool * const)");
    static bool _f551 = 0;
    _f551 = !_f551;
    CHECK((*p0 != _f551),
          "void f551(const bool * const)");
}

void f552(const char * const p0)
{
    TRACE("void f552(const char * const)");
    static char _f552 = 0;
    _f552++;
    CHECK((*p0 != _f552),
          "void f552(const char * const)");
}

void f553(const int8_t * const p0)
{
    TRACE("void f553(const int8_t * const)");
    static int8_t _f553 = 0;
    _f553++;
    CHECK((*p0 != _f553),
          "void f553(const int8_t * const)");
}

void f554(const uint8_t * const p0)
{
    TRACE("void f554(const uint8_t * const)");
    static uint8_t _f554 = 0;
    _f554++;
    CHECK((*p0 != _f554),
          "void f554(const uint8_t * const)");
}

void f555(const int16_t * const p0)
{
    TRACE("void f555(const int16_t * const)");
    static int16_t _f555 = 0;
    _f555++;
    CHECK((*p0 != _f555),
          "void f555(const int16_t * const)");
}

void f556(const uint16_t * const p0)
{
    TRACE("void f556(const uint16_t * const)");
    static uint16_t _f556 = 0;
    _f556++;
    CHECK((*p0 != _f556),
          "void f556(const uint16_t * const)");
}

void f557(const int32_t * const p0)
{
    TRACE("void f557(const int32_t * const)");
    static int32_t _f557 = 0;
    _f557++;
    CHECK((*p0 != _f557),
          "void f557(const int32_t * const)");
}

void f558(const uint32_t * const p0)
{
    TRACE("void f558(const uint32_t * const)");
    static uint32_t _f558 = 0;
    _f558++;
    CHECK((*p0 != _f558),
          "void f558(const uint32_t * const)");
}

void f561(const int64_t * const p0)
{
    TRACE("void f561(const int64_t * const)");
    static int64_t _f561 = 0;
    _f561++;
    CHECK((*p0 != _f561),
          "void f561(const int64_t * const)");
}

void f562(const uint64_t * const p0)
{
    TRACE("void f562(const uint64_t * const)");
    static uint64_t _f562 = 0;
    _f562++;
    CHECK((*p0 != _f562),
          "void f562(const uint64_t * const)");
}

void f563(const float * const p0)
{
    TRACE("void f563(const float * const)");
    static float _f563 = 0;
    _f563++;
    CHECK((*p0 != _f563),
          "void f563(const float * const)");
}

void f564(const double * const p0)
{
    TRACE("void f564(const double * const)");
    static double _f564 = 0;
    _f564++;
    CHECK((*p0 != _f564),
          "void f564(const double * const)");
}

void f571(bool * const p0)
{
    TRACE("void f571(bool * const)");
    static bool _f571 = 0;
    _f571 = !_f571;
    CHECK((*p0 != _f571),
          "void f571(bool * const)");
    *p0 = !*p0;
    _f571 = !_f571;
}

void f572(char * const p0)
{
    TRACE("void f572(char * const)");
    static char _f572 = 0;
    _f572++;
    CHECK((*p0 != _f572),
          "void f572(char * const)");
    (*p0)++;
    _f572++;
}

void f573(int8_t * const p0)
{
    TRACE("void f573(int8_t * const)");
    static int8_t _f573 = 0;
    _f573++;
    CHECK((*p0 != _f573),
          "void f573(int8_t * const)");
    (*p0)++;
    _f573++;
}

void f574(uint8_t * const p0)
{
    TRACE("void f574(uint8_t * const)");
    static uint8_t _f574 = 0;
    _f574++;
    CHECK((*p0 != _f574),
          "void f574(uint8_t * const)");
    (*p0)++;
    _f574++;
}

void f575(int16_t * const p0)
{
    TRACE("void f575(int16_t * const)");
    static int16_t _f575 = 0;
    _f575++;
    CHECK((*p0 != _f575),
          "void f575(int16_t * const)");
    (*p0)++;
    _f575++;
}

void f576(uint16_t * const p0)
{
    TRACE("void f576(uint16_t * const)");
    static uint16_t _f576 = 0;
    _f576++;
    CHECK((*p0 != _f576),
          "void f576(uint16_t * const)");
    (*p0)++;
    _f576++;
}

void f577(int32_t * const p0)
{
    TRACE("void f577(int32_t * const)");
    static int32_t _f577 = 0;
    _f577++;
    CHECK((*p0 != _f577),
          "void f577(int32_t * const)");
    (*p0)++;
    _f577++;
}

void f578(uint32_t * const p0)
{
    TRACE("void f578(uint32_t * const)");
    static uint32_t _f578 = 0;
    _f578++;
    CHECK((*p0 != _f578),
          "void f578(uint32_t * const)");
    (*p0)++;
    _f578++;
}

void f581(int64_t * const p0)
{
    TRACE("void f581(int64_t * const)");
    static int64_t _f581 = 0;
    _f581++;
    CHECK((*p0 != _f581),
          "void f581(int64_t * const)");
    (*p0)++;
    _f581++;
}

void f582(uint64_t * const p0)
{
    TRACE("void f582(uint64_t * const)");
    static uint64_t _f582 = 0;
    _f582++;
    CHECK((*p0 != _f582),
          "void f582(uint64_t * const)");
    (*p0)++;
    _f582++;
}

void f583(float * const p0)
{
    TRACE("void f583(float * const)");
    static float _f583 = 0;
    _f583++;
    CHECK((*p0 != _f583),
          "void f583(float * const)");
    (*p0)++;
    _f583++;
}

void f584(double * const p0)
{
    TRACE("void f584(double * const)");
    static double _f584 = 0;
    _f584++;
    CHECK((*p0 != _f584),
          "void f584(double * const)");
    (*p0)++;
    _f584++;
}

// ---------------------------------------------------------------------------

const bool * f611()
{
    TRACE("const bool * f611()");
    static bool _f611 = 1;
    return (((_f611 = !_f611) == 0) ? NULL : &_f611);
}

const char * f612()
{
    TRACE("const char * f612()");
    static char _f612 = 1;
    return (((_f612 = (char)~_f612) != 1) ? NULL : &_f612);
}

const int8_t * f613()
{
    TRACE("const int8_t * f613()");
    static int8_t _f613 = 1;
    return (((_f613 = (int8_t)~_f613) != 1) ? NULL : &_f613);
}

const uint8_t * f614()
{
    TRACE("const uint8_t * f614()");
    static uint8_t _f614 = 1;
    return (((_f614 = (uint8_t)~_f614) != 1) ? NULL : &_f614);
}

const int16_t * f615()
{
    TRACE("const int16_t * f615()");
    static int16_t _f615 = 1;
    return (((_f615 = (int16_t)~_f615) != 1) ? NULL : &_f615);
}

const uint16_t * f616()
{
    TRACE("const uint16_t * f616()");
    static uint16_t _f616 = 1;
    return (((_f616 = (uint16_t)~_f616) != 1) ? NULL : &_f616);
}

const int32_t * f617()
{
    TRACE("const int32_t * f617()");
    static int32_t _f617 = 1;
    return (((_f617 = (int32_t)~_f617) != 1) ? NULL : &_f617);
}

const uint32_t * f618()
{
    TRACE("const uint32_t * f618()");
    static uint32_t _f618 = 1;
    return (((_f618 = (uint32_t)~_f618) != 1) ? NULL : &_f618);
}

const int64_t * f621()
{
    TRACE("const int64_t * f621()");
    static int64_t _f621 = 1;
    return (((_f621 = (int64_t)~_f621) != 1) ? NULL : &_f621);
}

const uint64_t * f622()
{
    TRACE("const uint64_t * f622()");
    static uint64_t _f622 = 1;
    return (((_f622 = (uint64_t)~_f622) != 1) ? NULL : &_f622);
}

const float * f623()
{
    TRACE("const * float f623()");
    static float _f623 = 1;
    return (((_f623 = (float)-_f623) != 1) ? NULL : &_f623);
}

const double * f624()
{
    TRACE("const double * f624()");
    static double _f624 = 1;
    return (((_f624 = (double)-_f624) != 1) ? NULL : &_f624);
}

bool * f631()
{
    TRACE("bool * f631()");
    static bool _f631 = 1;
    return (((_f631 = !_f631) == 0) ? NULL : &_f631);
}

char * f632()
{
    TRACE("char * f632()");
    static char _f632 = 1;
    return (((_f632 = (char)~_f632) != 1) ? NULL : &_f632);
}

int8_t * f633()
{
    TRACE("int8_t * f633()");
    static int8_t _f633 = 1;
    return (((_f633 = (int8_t)~_f633) != 1) ? NULL : &_f633);
}

uint8_t * f634()
{
    TRACE("uint8_t * f634()");
    static uint8_t _f634 = 1;
    return (((_f634 = (uint8_t)~_f634) != 1) ? NULL : &_f634);
}

int16_t * f635()
{
    TRACE("int16_t * f635()");
    static int16_t _f635 = 1;
    return (((_f635 = (int16_t)~_f635) != 1) ? NULL : &_f635);
}

uint16_t * f636()
{
    TRACE("uint16_t * f636()");
    static uint16_t _f636 = 1;
    return (((_f636 = (uint16_t)~_f636) != 1) ? NULL : &_f636);
}

int32_t * f637()
{
    TRACE("int32_t * f637()");
    static int32_t _f637 = 1;
    return (((_f637 = (int32_t)~_f637) != 1) ? NULL : &_f637);
}

uint32_t * f638()
{
    TRACE("uint32_t * f638()");
    static uint32_t _f638 = 1;
    return (((_f638 = (uint32_t)~_f638) != 1) ? NULL : &_f638);
}

int64_t * f641()
{
    TRACE("int64_t * f641()");
    static int64_t _f641 = 1;
    return (((_f641 = (int64_t)~_f641) != 1) ? NULL : &_f641);
}

uint64_t * f642()
{
    TRACE("uint64_t * f642()");
    static uint64_t _f642 = 1;
    return (((_f642 = (uint64_t)~_f642) != 1) ? NULL : &_f642);
}

float * f643()
{
    TRACE("float * f643()");
    static float _f643 = 1;
    return (((_f643 = (float)-_f643) != 1) ? NULL : &_f643);
}

double * f644()
{
    TRACE("double * f644()");
    static double _f644 = 1;
    return (((_f644 = (double)-_f644) != 1) ? NULL : &_f644);
}

const bool * const f651()
{
    TRACE("const bool * const f651()");
    static bool _f651 = 1;
    return (((_f651 = !_f651) == 0) ? NULL : &_f651);
}

const char * const f652()
{
    TRACE("const char * const f652()");
    static char _f652 = 1;
    return (((_f652 = (char)~_f652) != 1) ? NULL : &_f652);
}

const int8_t * const f653()
{
    TRACE("const int8_t * const f653()");
    static int8_t _f653 = 1;
    return (((_f653 = (int8_t)~_f653) != 1) ? NULL : &_f653);
}

const uint8_t * const f654()
{
    TRACE("const uint8_t * const f654()");
    static uint8_t _f654 = 1;
    return (((_f654 = (uint8_t)~_f654) != 1) ? NULL : &_f654);
}

const int16_t * const f655()
{
    TRACE("const int16_t * const f655()");
    static int16_t _f655 = 1;
    return (((_f655 = (int16_t)~_f655) != 1) ? NULL : &_f655);
}

const uint16_t * const f656()
{
    TRACE("const uint16_t * const f656()");
    static uint16_t _f656 = 1;
    return (((_f656 = (uint16_t)~_f656) != 1) ? NULL : &_f656);
}

const int32_t * const f657()
{
    TRACE("const int32_t * const f657()");
    static int32_t _f657 = 1;
    return (((_f657 = (int32_t)~_f657) != 1) ? NULL : &_f657);
}

const uint32_t * const f658()
{
    TRACE("const uint32_t * const f658()");
    static uint32_t _f658 = 1;
    return (((_f658 = (uint32_t)~_f658) != 1) ? NULL : &_f658);
}

const int64_t * const f661()
{
    TRACE("const int64_t * const f661()");
    static int64_t _f661 = 1;
    return (((_f661 = (int64_t)~_f661) != 1) ? NULL : &_f661);
}

const uint64_t * const f662()
{
    TRACE("const uint64_t * const f662()");
    static uint64_t _f662 = 1;
    return (((_f662 = (uint64_t)~_f662) != 1) ? NULL : &_f662);
}

const float * const f663()
{
    TRACE("const * float const f663()");
    static float _f663 = 1;
    return (((_f663 = (float)-_f663) != 1) ? NULL : &_f663);
}

const double * const f664()
{
    TRACE("const double * const f664()");
    static double _f664 = 1;
    return (((_f664 = (double)-_f664) != 1) ? NULL : &_f664);
}

bool * const f671()
{
    TRACE("bool * const f671()");
    static bool _f671 = 1;
    return (((_f671 = !_f671) == 0) ? NULL : &_f671);
}

char * const f672()
{
    TRACE("char * const f672()");
    static char _f672 = 1;
    return (((_f672 = (char)~_f672) != 1) ? NULL : &_f672);
}

int8_t * const f673()
{
    TRACE("int8_t * const f673()");
    static int8_t _f673 = 1;
    return (((_f673 = (int8_t)~_f673) != 1) ? NULL : &_f673);
}

uint8_t * const f674()
{
    TRACE("uint8_t * const f674()");
    static uint8_t _f674 = 1;
    return (((_f674 = (uint8_t)~_f674) != 1) ? NULL : &_f674);
}

int16_t * const f675()
{
    TRACE("int16_t * const f675()");
    static int16_t _f675 = 1;
    return (((_f675 = (int16_t)~_f675) != 1) ? NULL : &_f675);
}

uint16_t * const f676()
{
    TRACE("uint16_t * const f676()");
    static uint16_t _f676 = 1;
    return (((_f676 = (uint16_t)~_f676) != 1) ? NULL : &_f676);
}

int32_t * const f677()
{
    TRACE("int32_t * const f677()");
    static int32_t _f677 = 1;
    return (((_f677 = (int32_t)~_f677) != 1) ? NULL : &_f677);
}

uint32_t * const f678()
{
    TRACE("uint32_t * const f678()");
    static uint32_t _f678 = 1;
    return (((_f678 = (uint32_t)~_f678) != 1) ? NULL : &_f678);
}

int64_t * const f681()
{
    TRACE("int64_t * const f681()");
    static int64_t _f681 = 1;
    return (((_f681 = (int64_t)~_f681) != 1) ? NULL : &_f681);
}

uint64_t * const f682()
{
    TRACE("uint64_t * const f682()");
    static uint64_t _f682 = 1;
    return (((_f682 = (uint64_t)~_f682) != 1) ? NULL : &_f682);
}

float * const f683()
{
    TRACE("float * const f683()");
    static float _f683 = 1;
    return (((_f683 = (float)-_f683) != 1) ? NULL : &_f683);
}

double * const f684()
{
    TRACE("double * const f684()");
    static double _f684 = 1;
    return (((_f684 = (double)-_f684) != 1) ? NULL : &_f684);
}

// ---------------------------------------------------------------------------

void f711(const bool * p0)
{
    TRACE("void f711(const bool *)");
    static bool _f711 = 1;
    CHECK((((_f711 = !_f711) == 0) ^ (p0 == NULL)),
          "void f711(const bool *)");
}

void f712(const char * p0)
{
    TRACE("void f712(const char *)");
    static char _f712 = 1;
    CHECK((((_f712 = (char)~_f712) != 1) ^ (p0 == NULL)),
          "void f712(const char *)");
}

void f713(const int8_t * p0)
{
    TRACE("void f713(const int8_t *)");
    static int8_t _f713 = 1;
    CHECK((((_f713 = (int8_t)~_f713) != 1) ^ (p0 == NULL)),
          "void f713(const int8_t *)");
}

void f714(const uint8_t * p0)
{
    TRACE("void f714(const uint8_t *)");
    static uint8_t _f714 = 1;
    CHECK((((_f714 = (uint8_t)~_f714) != 1) ^ (p0 == NULL)),
          "void f714(const uint8_t *)");
}

void f715(const int16_t * p0)
{
    TRACE("void f715(const int16_t *)");
    static int16_t _f715 = 1;
    CHECK((((_f715 = (int16_t)~_f715) != 1) ^ (p0 == NULL)),
          "void f715(const int16_t *)");
}

void f716(const uint16_t * p0)
{
    TRACE("void f716(const uint16_t *)");
    static uint16_t _f716 = 1;
    CHECK((((_f716 = (uint16_t)~_f716) != 1) ^ (p0 == NULL)),
          "void f716(const uint16_t *)");
}

void f717(const int32_t * p0)
{
    TRACE("void f717(const int32_t *)");
    static int32_t _f717 = 1;
    CHECK((((_f717 = (int32_t)~_f717) != 1) ^ (p0 == NULL)),
          "void f717(const int32_t *)");
}

void f718(const uint32_t * p0)
{
    TRACE("void f718(const uint32_t *)");
    static uint32_t _f718 = 1;
    CHECK((((_f718 = (uint32_t)~_f718) != 1) ^ (p0 == NULL)),
          "void f718(const uint32_t *)");
}

void f721(const int64_t * p0)
{
    TRACE("void f721(const int64_t *)");
    static int64_t _f721 = 1;
    CHECK((((_f721 = (int64_t)~_f721) != 1) ^ (p0 == NULL)),
          "void f721(const int64_t *)");
}

void f722(const uint64_t * p0)
{
    TRACE("void f722(const uint64_t *)");
    static uint64_t _f722 = 1;
    CHECK((((_f722 = (uint64_t)~_f722) != 1) ^ (p0 == NULL)),
          "void f722(const uint64_t *)");
}

void f723(const float * p0)
{
    TRACE("void f723(const float *)");
    static float _f723 = 1;
    CHECK((((_f723 = (float)-_f723) != 1) ^ (p0 == NULL)),
          "void f723(const float *)");
}

void f724(const double * p0)
{
    TRACE("void f724(const double *)");
    static double _f724 = 1;
    CHECK((((_f724 = (double)-_f724) != 1) ^ (p0 == NULL)),
          "void f724(const double *)");
}

void f731(bool * p0)
{
    TRACE("void f731(bool *)");
    static bool _f731 = 1;
    CHECK((((_f731 = !_f731) == 0) ^ (p0 == NULL)),
          "void f731(bool *)");
}

void f732(char * p0)
{
    TRACE("void f732(char *)");
    static char _f732 = 1;
    CHECK((((_f732 = (char)~_f732) != 1) ^ (p0 == NULL)),
          "void f732(char *)");
}

void f733(int8_t * p0)
{
    TRACE("void f733(int8_t *)");
    static int8_t _f733 = 1;
    CHECK((((_f733 = (int8_t)~_f733) != 1) ^ (p0 == NULL)),
          "void f733(int8_t *)");
}

void f734(uint8_t * p0)
{
    TRACE("void f734(uint8_t *)");
    static uint8_t _f734 = 1;
    CHECK((((_f734 = (uint8_t)~_f734) != 1) ^ (p0 == NULL)),
          "void f734(uint8_t *)");
}

void f735(int16_t * p0)
{
    TRACE("void f735(int16_t *)");
    static int16_t _f735 = 1;
    CHECK((((_f735 = (int16_t)~_f735) != 1) ^ (p0 == NULL)),
          "void f735(int16_t *)");
}

void f736(uint16_t * p0)
{
    TRACE("void f736(uint16_t *)");
    static uint16_t _f736 = 1;
    CHECK((((_f736 = (uint16_t)~_f736) != 1) ^ (p0 == NULL)),
          "void f736(uint16_t *)");
}

void f737(int32_t * p0)
{
    TRACE("void f737(int32_t *)");
    static int32_t _f737 = 1;
    CHECK((((_f737 = (int32_t)~_f737) != 1) ^ (p0 == NULL)),
          "void f737(int32_t *)");
}

void f738(uint32_t * p0)
{
    TRACE("void f738(uint32_t *)");
    static uint32_t _f738 = 1;
    CHECK((((_f738 = (uint32_t)~_f738) != 1) ^ (p0 == NULL)),
          "void f738(uint32_t *)");
}

void f741(int64_t * p0)
{
    TRACE("void f741(int64_t *)");
    static int64_t _f741 = 1;
    CHECK((((_f741 = (int64_t)~_f741) != 1) ^ (p0 == NULL)),
          "void f741(int64_t *)");
}

void f742(uint64_t * p0)
{
    TRACE("void f742(uint64_t *)");
    static uint64_t _f742 = 1;
    CHECK((((_f742 = (uint64_t)~_f742) != 1) ^ (p0 == NULL)),
          "void f742(uint64_t *)");
}

void f743(float * p0)
{
    TRACE("void f743(float *)");
    static float _f743 = 1;
    CHECK((((_f743 = (float)-_f743) != 1) ^ (p0 == NULL)),
          "void f743(float *)");
}

void f744(double * p0)
{
    TRACE("void f744(double *)");
    static double _f744 = 1;
    CHECK((((_f744 = (double)-_f744) != 1) ^ (p0 == NULL)),
          "void f744(double *)");
}

void f751(const bool * const p0)
{
    TRACE("void f751(const bool * const)");
    static bool _f751 = 1;
    CHECK((((_f751 = !_f751) == 0) ^ (p0 == NULL)),
          "void f751(const bool * const)");
}

void f752(const char * const p0)
{
    TRACE("void f752(const char * const)");
    static char _f752 = 1;
    CHECK((((_f752 = (char)~_f752) != 1) ^ (p0 == NULL)),
          "void f752(const char * const)");
}

void f753(const int8_t * const p0)
{
    TRACE("void f753(const int8_t * const)");
    static int8_t _f753 = 1;
    CHECK((((_f753 = (int8_t)~_f753) != 1) ^ (p0 == NULL)),
          "void f753(const int8_t * const)");
}

void f754(const uint8_t * const p0)
{
    TRACE("void f754(const uint8_t * const)");
    static uint8_t _f754 = 1;
    CHECK((((_f754 = (uint8_t)~_f754) != 1) ^ (p0 == NULL)),
          "void f754(const uint8_t * const)");
}

void f755(const int16_t * const p0)
{
    TRACE("void f755(const int16_t * const)");
    static int16_t _f755 = 1;
    CHECK((((_f755 = (int16_t)~_f755) != 1) ^ (p0 == NULL)),
          "void f755(const int16_t * const)");
}

void f756(const uint16_t * const p0)
{
    TRACE("void f756(const uint16_t * const)");
    static uint16_t _f756 = 1;
    CHECK((((_f756 = (uint16_t)~_f756) != 1) ^ (p0 == NULL)),
          "void f756(const uint16_t * const)");
}

void f757(const int32_t * const p0)
{
    TRACE("void f757(const int32_t * const)");
    static int32_t _f757 = 1;
    CHECK((((_f757 = (int32_t)~_f757) != 1) ^ (p0 == NULL)),
          "void f757(const int32_t * const)");
}

void f758(const uint32_t * const p0)
{
    TRACE("void f758(const uint32_t * const)");
    static uint32_t _f758 = 1;
    CHECK((((_f758 = (uint32_t)~_f758) != 1) ^ (p0 == NULL)),
          "void f758(const uint32_t * const)");
}

void f761(const int64_t * const p0)
{
    TRACE("void f761(const int64_t * const)");
    static int64_t _f761 = 1;
    CHECK((((_f761 = (int64_t)~_f761) != 1) ^ (p0 == NULL)),
          "void f761(const int64_t * const)");
}

void f762(const uint64_t * const p0)
{
    TRACE("void f762(const uint64_t * const)");
    static uint64_t _f762 = 1;
    CHECK((((_f762 = (uint64_t)~_f762) != 1) ^ (p0 == NULL)),
          "void f762(const uint64_t * const)");
}

void f763(const float * const p0)
{
    TRACE("void f763(const float * const)");
    static float _f763 = 1;
    CHECK((((_f763 = (float)-_f763) != 1) ^ (p0 == NULL)),
          "void f763(const float * const)");
}

void f764(const double * const p0)
{
    TRACE("void f764(const double * const)");
    static double _f764 = 1;
    CHECK((((_f764 = (double)-_f764) != 1) ^ (p0 == NULL)),
          "void f764(const double * const)");
}

void f771(bool * const p0)
{
    TRACE("void f771(bool * const)");
    static bool _f771 = 1;
    CHECK((((_f771 = !_f771) == 0) ^ (p0 == NULL)),
          "void f771(bool * const)");
}

void f772(char * const p0)
{
    TRACE("void f772(char * const)");
    static char _f772 = 1;
    CHECK((((_f772 = (char)~_f772) != 1) ^ (p0 == NULL)),
          "void f772(char * const)");
}

void f773(int8_t * const p0)
{
    TRACE("void f773(int8_t * const)");
    static int8_t _f773 = 1;
    CHECK((((_f773 = (int8_t)~_f773) != 1) ^ (p0 == NULL)),
          "void f773(int8_t * const)");
}

void f774(uint8_t * const p0)
{
    TRACE("void f774(uint8_t * const)");
    static uint8_t _f774 = 1;
    CHECK((((_f774 = (uint8_t)~_f774) != 1) ^ (p0 == NULL)),
          "void f774(uint8_t * const)");
}

void f775(int16_t * const p0)
{
    TRACE("void f775(int16_t * const)");
    static int16_t _f775 = 1;
    CHECK((((_f775 = (int16_t)~_f775) != 1) ^ (p0 == NULL)),
          "void f775(int16_t * const)");
}

void f776(uint16_t * const p0)
{
    TRACE("void f776(uint16_t * const)");
    static uint16_t _f776 = 1;
    CHECK((((_f776 = (uint16_t)~_f776) != 1) ^ (p0 == NULL)),
          "void f776(uint16_t * const)");
}

void f777(int32_t * const p0)
{
    TRACE("void f777(int32_t * const)");
    static int32_t _f777 = 1;
    CHECK((((_f777 = (int32_t)~_f777) != 1) ^ (p0 == NULL)),
          "void f777(int32_t * const)");
}

void f778(uint32_t * const p0)
{
    TRACE("void f778(uint32_t * const)");
    static uint32_t _f778 = 1;
    CHECK((((_f778 = (uint32_t)~_f778) != 1) ^ (p0 == NULL)),
          "void f778(uint32_t * const)");
}

void f781(int64_t * const p0)
{
    TRACE("void f781(int64_t * const)");
    static int64_t _f781 = 1;
    CHECK((((_f781 = (int64_t)~_f781) != 1) ^ (p0 == NULL)),
          "void f781(int64_t * const)");
}

void f782(uint64_t * const p0)
{
    TRACE("void f782(uint64_t * const)");
    static uint64_t _f782 = 1;
    CHECK((((_f782 = (uint64_t)~_f782) != 1) ^ (p0 == NULL)),
          "void f782(uint64_t * const)");
}

void f783(float * const p0)
{
    TRACE("void f783(float * const)");
    static float _f783 = 1;
    CHECK((((_f783 = (float)-_f783) != 1) ^ (p0 == NULL)),
          "void f783(float * const)");
}

void f784(double * const p0)
{
    TRACE("void f784(double * const)");
    static double _f784 = 1;
    CHECK((((_f784 = (double)-_f784) != 1) ^ (p0 == NULL)),
          "void f784(double * const)");
}

// ---------------------------------------------------------------------------
