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
 * myapi.cpp
 */

#include <cstdio>
#include <cstring>

#include "myapi.hpp"
#include "helpers.hpp"

void f0()
{
    TRACE("void f0()");
}

// ---------------------------------------------------------------------------

A * A::a = new A();

// ---------------------------------------------------------------------------

const void * s010();

const char * s012()
{
    TRACE("const char * s012()");
    static const char * _s012 = "abc";
    return _s012;
}

void * s030();

char * s032()
{
    TRACE("char * s032()");
    static char _s032[5] = { 's', '0', '3', '2', '\0' };
    return _s032;
}

void s110(const void * p0);

void s112(const char * p0)
{
    TRACE("void f112(const char *)");
    static const char * _f112 = "abc";
    if (strcmp(p0, _f112) != 0) ABORT_ERROR("void f112(const char *)");
}

void s130(void * p0);

void s132(char * p0);

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

const signed char f013()
{
    TRACE("const signed char f013()");
    static signed char _f013 = 0;
    _f013++;
    return _f013;
}

const unsigned char f014()
{
    TRACE("const unsigned char f014()");
    static unsigned char _f014 = 0;
    _f014++;
    return _f014;
}

const signed short f015()
{
    TRACE("const signed short f015()");
    static signed short _f015 = 0;
    _f015++;
    return _f015;
}

const unsigned short f016()
{
    TRACE("const unsigned short f016()");
    static unsigned short _f016 = 0;
    _f016++;
    return _f016;
}

const signed int f017()
{
    TRACE("const signed int f017()");
    static signed int _f017 = 0;
    _f017++;
    return _f017;
}

const unsigned int f018()
{
    TRACE("const unsigned int f018()");
    static unsigned int _f018 = 0;
    _f018++;
    return _f018;
}

const signed long long f021()
{
    TRACE("const signed long long f021()");
    static signed long long _f021 = 0;
    _f021++;
    return _f021;
}

const unsigned long long f022()
{
    TRACE("const unsigned long long f022()");
    static unsigned long long _f022 = 0;
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

const long double f025()
{
    TRACE("const long double f025()");
    static long double _f025 = 0;
    _f025++;
    return _f025;
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

signed char f033()
{
    TRACE("signed char f033()");
    static signed char _f033 = 0;
    _f033++;
    return _f033;
}

unsigned char f034()
{
    TRACE("unsigned char f034()");
    static unsigned char _f034 = 0;
    _f034++;
    return _f034;
}

signed short f035()
{
    TRACE("signed short f035()");
    static signed short _f035 = 0;
    _f035++;
    return _f035;
}

unsigned short f036()
{
    TRACE("unsigned short f036()");
    static unsigned short _f036 = 0;
    _f036++;
    return _f036;
}

signed int f037()
{
    TRACE("signed int f037()");
    static signed int _f037 = 0;
    _f037++;
    return _f037;
}

unsigned int f038()
{
    TRACE("unsigned int f038()");
    static unsigned int _f038 = 0;
    _f038++;
    return _f038;
}

signed long long f041()
{
    TRACE("signed long long f041()");
    static signed long long _f041 = 0;
    _f041++;
    return _f041;
}

unsigned long long f042()
{
    TRACE("unsigned long long f042()");
    static unsigned long long _f042 = 0;
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

long double f045()
{
    TRACE("long double f045()");
    static long double _f045 = 0;
    _f045++;
    return _f045;
}

// ---------------------------------------------------------------------------

void f111(const bool p0)
{
    TRACE("void f111(const bool)");
    static bool _f111 = 0;
    _f111 = !_f111;
    if (p0 != _f111) ABORT_ERROR("void f111(const bool)");
}

void f112(const char p0)
{
    TRACE("void f112(const char)");
    static char _f112 = 0;
    _f112++;
    if (p0 != _f112) ABORT_ERROR("void f112(const char)");
}

void f113(const signed char p0)
{
    TRACE("void f113(const signed char)");
    static signed char _f113 = 0;
    _f113++;
    if (p0 != _f113) ABORT_ERROR("void f113(const signed char)");
}

void f114(const unsigned char p0)
{
    TRACE("void f114(const unsigned char)");
    static unsigned char _f114 = 0;
    _f114++;
    if (p0 != _f114) ABORT_ERROR("void f114(const unsigned char)");
}

void f115(const signed short p0)
{
    TRACE("void f115(const signed short)");
    static signed short _f115 = 0;
    _f115++;
    if (p0 != _f115) ABORT_ERROR("void f115(const signed short)");
}

void f116(const unsigned short p0)
{
    TRACE("void f116(const unsigned short)");
    static unsigned short _f116 = 0;
    _f116++;
    if (p0 != _f116) ABORT_ERROR("void f116(const unsigned short)");
}

void f117(const signed int p0)
{
    TRACE("void f117(const signed int)");
    static signed int _f117 = 0;
    _f117++;
    if (p0 != _f117) ABORT_ERROR("void f117(const signed int)");
}

void f118(const unsigned int p0)
{
    TRACE("void f118(const unsigned int)");
    static unsigned int _f118 = 0;
    _f118++;
    if (p0 != _f118) ABORT_ERROR("void f118(const unsigned int)");
}

void f121(const signed long long p0)
{
    TRACE("void f121(const signed long long)");
    static signed long long _f121 = 0;
    _f121++;
    if (p0 != _f121) ABORT_ERROR("void f121(const signed long long)");
}

void f122(const unsigned long long p0)
{
    TRACE("void f122(const unsigned long long)");
    static unsigned long long _f122 = 0;
    _f122++;
    if (p0 != _f122) ABORT_ERROR("void f122(const unsigned long long)");
}

void f123(const float p0)
{
    TRACE("void f123(const float)");
    static float _f123 = 0;
    _f123++;
    if (p0 != _f123) ABORT_ERROR("void f123(const float)");
}

void f124(const double p0)
{
    TRACE("void f124(const double)");
    static double _f124 = 0;
    _f124++;
    if (p0 != _f124) ABORT_ERROR("void f124(const double)");
}

void f125(const long double p0)
{
    TRACE("void f125(const long double)");
    static long double _f125 = 0;
    _f125++;
    if (p0 != _f125) ABORT_ERROR("void f125(const long double)");
}

void f131(bool p0)
{
    TRACE("void f131(bool)");
    static bool _f131 = 0;
    _f131 = !_f131;
    if (p0 != _f131) ABORT_ERROR("void f131(bool)");
}

void f132(char p0)
{
    TRACE("void f132(char)");
    static char _f132 = 0;
    _f132++;
    if (p0 != _f132) ABORT_ERROR("void f132(char)");
}

void f133(signed char p0)
{
    TRACE("void f133(signed char)");
    static signed char _f133 = 0;
    _f133++;
    if (p0 != _f133) ABORT_ERROR("void f133(signed char)");
}

void f134(unsigned char p0)
{
    TRACE("void f134(unsigned char)");
    static unsigned char _f134 = 0;
    _f134++;
    if (p0 != _f134) ABORT_ERROR("void f134(unsigned char)");
}

void f135(signed short p0)
{
    TRACE("void f135(signed short)");
    static signed short _f135 = 0;
    _f135++;
    if (p0 != _f135) ABORT_ERROR("void f135(signed short)");
}

void f136(unsigned short p0)
{
    TRACE("void f136(unsigned short)");
    static unsigned short _f136 = 0;
    _f136++;
    if (p0 != _f136) ABORT_ERROR("void f136(unsigned short)");
}

void f137(signed int p0)
{
    TRACE("void f137(signed int)");
    static signed int _f137 = 0;
    _f137++;
    if (p0 != _f137) ABORT_ERROR("void f137(signed int)");
}

void f138(unsigned int p0)
{
    TRACE("void f138(unsigned int)");
    static unsigned int _f138 = 0;
    _f138++;
    if (p0 != _f138) ABORT_ERROR("void f138(unsigned int)");
}

void f141(signed long long p0)
{
    TRACE("void f141(signed long long)");
    static signed long long _f141 = 0;
    _f141++;
    if (p0 != _f141) ABORT_ERROR("void f141(signed long long)");
}

void f142(unsigned long long p0)
{
    TRACE("void f142(unsigned long long)");
    static unsigned long long _f142 = 0;
    _f142++;
    if (p0 != _f142) ABORT_ERROR("void f142(unsigned long long)");
}

void f143(float p0)
{
    TRACE("void f143(float)");
    static float _f143 = 0;
    _f143++;
    if (p0 != _f143) ABORT_ERROR("void f143(float)");
}

void f144(double p0)
{
    TRACE("void f144(double)");
    static double _f144 = 0;
    _f144++;
    if (p0 != _f144) ABORT_ERROR("void f144(double)");
}

void f145(long double p0)
{
    TRACE("void f145(long double)");
    static long double _f145 = 0;
    _f145++;
    if (p0 != _f145) ABORT_ERROR("void f145(long double)");
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

const signed char & f213()
{
    TRACE("const signed char & f213()");
    static signed char _f213 = 0;
    _f213++;
    return _f213;
}

const unsigned char & f214()
{
    TRACE("const unsigned char & f214()");
    static unsigned char _f214 = 0;
    _f214++;
    return _f214;
}

const signed short & f215()
{
    TRACE("const signed short & f215()");
    static signed short _f215 = 0;
    _f215++;
    return _f215;
}

const unsigned short & f216()
{
    TRACE("const unsigned short & f216()");
    static unsigned short _f216 = 0;
    _f216++;
    return _f216;
}

const signed int & f217()
{
    TRACE("const signed int & f217()");
    static signed int _f217 = 0;
    _f217++;
    return _f217;
}

const unsigned int & f218()
{
    TRACE("const unsigned int & f218()");
    static unsigned int _f218 = 0;
    _f218++;
    return _f218;
}

const signed long long & f221()
{
    TRACE("const signed long long & f221()");
    static signed long long _f221 = 0;
    _f221++;
    return _f221;
}

const unsigned long long & f222()
{
    TRACE("const unsigned long long & f222()");
    static unsigned long long _f222 = 0;
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

const long double & f225()
{
    TRACE("const long double & f225()");
    static long double _f225 = 0;
    _f225++;
    return _f225;
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

signed char & f233()
{
    TRACE("signed char & f233()");
    static signed char _f233 = 0;
    _f233++;
    return _f233;
}

unsigned char & f234()
{
    TRACE("unsigned char & f234()");
    static unsigned char _f234 = 0;
    _f234++;
    return _f234;
}

signed short & f235()
{
    TRACE("signed short & f235()");
    static signed short _f235 = 0;
    _f235++;
    return _f235;
}

unsigned short & f236()
{
    TRACE("unsigned short & f236()");
    static unsigned short _f236 = 0;
    _f236++;
    return _f236;
}

signed int & f237()
{
    TRACE("signed int & f237()");
    static signed int _f237 = 0;
    _f237++;
    return _f237;
}

unsigned int & f238()
{
    TRACE("unsigned int & f238()");
    static unsigned int _f238 = 0;
    _f238++;
    return _f238;
}

signed long long & f241()
{
    TRACE("signed long long & f241()");
    static signed long long _f241 = 0;
    _f241++;
    return _f241;
}

unsigned long long & f242()
{
    TRACE("unsigned long long & f242()");
    static unsigned long long _f242 = 0;
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

long double & f245()
{
    TRACE("long double & f245()");
    static long double _f245 = 0;
    _f245++;
    return _f245;
}

// ---------------------------------------------------------------------------

void f311(const bool & p0)
{
    TRACE("void f311(const bool &)");
    static bool _f311 = 0;
    _f311 = !_f311;
    if (p0 != _f311) ABORT_ERROR("void f311(const bool &)");
}

void f312(const char & p0)
{
    TRACE("void f312(const char &)");
    static char _f312 = 0;
    _f312++;
    if (p0 != _f312) ABORT_ERROR("void f312(const char &)");
}

void f313(const signed char & p0)
{
    TRACE("void f313(const signed char &)");
    static signed char _f313 = 0;
    _f313++;
    if (p0 != _f313) ABORT_ERROR("void f313(const signed char &)");
}

void f314(const unsigned char & p0)
{
    TRACE("void f314(const unsigned char &)");
    static unsigned char _f314 = 0;
    _f314++;
    if (p0 != _f314) ABORT_ERROR("void f314(const unsigned char &)");
}

void f315(const signed short & p0)
{
    TRACE("void f315(const signed short &)");
    static signed short _f315 = 0;
    _f315++;
    if (p0 != _f315) ABORT_ERROR("void f315(const signed short &)");
}

void f316(const unsigned short & p0)
{
    TRACE("void f316(const unsigned short &)");
    static unsigned short _f316 = 0;
    _f316++;
    if (p0 != _f316) ABORT_ERROR("void f316(const unsigned short &)");
}

void f317(const signed int & p0)
{
    TRACE("void f317(const signed int &)");
    static signed int _f317 = 0;
    _f317++;
    if (p0 != _f317) ABORT_ERROR("void f317(const signed int &)");
}

void f318(const unsigned int & p0)
{
    TRACE("void f318(const unsigned int &)");
    static unsigned int _f318 = 0;
    _f318++;
    if (p0 != _f318) ABORT_ERROR("void f318(const unsigned int &)");
}

void f321(const signed long long & p0)
{
    TRACE("void f321(const signed long long &)");
    static signed long long _f321 = 0;
    _f321++;
    if (p0 != _f321) ABORT_ERROR("void f321(const signed long long &)");
}

void f322(const unsigned long long & p0)
{
    TRACE("void f322(const unsigned long long &)");
    static unsigned long long _f322 = 0;
    _f322++;
    if (p0 != _f322) ABORT_ERROR("void f322(const unsigned long long &)");
}

void f323(const float & p0)
{
    TRACE("void f323(const float &)");
    static float _f323 = 0;
    _f323++;
    if (p0 != _f323) ABORT_ERROR("void f323(const float &)");
}

void f324(const double & p0)
{
    TRACE("void f324(const double &)");
    static double _f324 = 0;
    _f324++;
    if (p0 != _f324) ABORT_ERROR("void f324(const double &)");
}

void f325(const long double & p0)
{
    TRACE("void f325(const long double &)");
    static long double _f325 = 0;
    _f325++;
    if (p0 != _f325) ABORT_ERROR("void f325(const long double &)");
}

void f331(bool & p0)
{
    TRACE("void f331(bool &)");
    static bool _f331 = 0;
    _f331 = !_f331;
    if (p0 != _f331) ABORT_ERROR("void f331(bool &)");
    p0 = !p0;
    _f331 = !_f331;
}

void f332(char & p0)
{
    TRACE("void f332(char &)");
    static char _f332 = 0;
    _f332++;
    if (p0 != _f332) ABORT_ERROR("void f332(char &)");
    p0++;
    _f332++;
}

void f333(signed char & p0)
{
    TRACE("void f333(signed char &)");
    static signed char _f333 = 0;
    _f333++;
    if (p0 != _f333) ABORT_ERROR("void f333(signed char &)");
    p0++;
    _f333++;
}

void f334(unsigned char & p0)
{
    TRACE("void f334(unsigned char &)");
    static unsigned char _f334 = 0;
    _f334++;
    if (p0 != _f334) ABORT_ERROR("void f334(unsigned char &)");
    p0++;
    _f334++;
}

void f335(signed short & p0)
{
    TRACE("void f335(signed short &)");
    static signed short _f335 = 0;
    _f335++;
    if (p0 != _f335) ABORT_ERROR("void f335(signed short &)");
    p0++;
    _f335++;
}

void f336(unsigned short & p0)
{
    TRACE("void f336(unsigned short &)");
    static unsigned short _f336 = 0;
    _f336++;
    if (p0 != _f336) ABORT_ERROR("void f336(unsigned short &)");
    p0++;
    _f336++;
}

void f337(signed int & p0)
{
    TRACE("void f337(signed int &)");
    static signed int _f337 = 0;
    _f337++;
    if (p0 != _f337) ABORT_ERROR("void f337(signed int &)");
    p0++;
    _f337++;
}

void f338(unsigned int & p0)
{
    TRACE("void f338(unsigned int &)");
    static unsigned int _f338 = 0;
    _f338++;
    if (p0 != _f338) ABORT_ERROR("void f338(unsigned int &)");
    p0++;
    _f338++;
}

void f341(signed long long & p0)
{
    TRACE("void f341(signed long long &)");
    static signed long long _f341 = 0;
    _f341++;
    if (p0 != _f341) ABORT_ERROR("void f341(signed long long &)");
    p0++;
    _f341++;
}

void f342(unsigned long long & p0)
{
    TRACE("void f342(unsigned long long &)");
    static unsigned long long _f342 = 0;
    _f342++;
    if (p0 != _f342) ABORT_ERROR("void f342(unsigned long long &)");
    p0++;
    _f342++;
}

void f343(float & p0)
{
    TRACE("void f343(float &)");
    static float _f343 = 0;
    _f343++;
    if (p0 != _f343) ABORT_ERROR("void f343(float &)");
    p0++;
    _f343++;
}

void f344(double & p0)
{
    TRACE("void f344(double &)");
    static double _f344 = 0;
    _f344++;
    if (p0 != _f344) ABORT_ERROR("void f344(double &)");
    p0++;
    _f344++;
}

void f345(long double & p0)
{
    TRACE("void f345(long double &)");
    static long double _f345 = 0;
    _f345++;
    if (p0 != _f345) ABORT_ERROR("void f345(long double &)");
    p0++;
    _f345++;
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

const signed char * f413()
{
    TRACE("const signed char * f413()");
    static signed char _f413 = 0;
    _f413++;
    return &_f413;
}

const unsigned char * f414()
{
    TRACE("const unsigned char * f414()");
    static unsigned char _f414 = 0;
    _f414++;
    return &_f414;
}

const signed short * f415()
{
    TRACE("const signed short * f415()");
    static signed short _f415 = 0;
    _f415++;
    return &_f415;
}

const unsigned short * f416()
{
    TRACE("const unsigned short * f416()");
    static unsigned short _f416 = 0;
    _f416++;
    return &_f416;
}

const signed int * f417()
{
    TRACE("const signed int * f417()");
    static signed int _f417 = 0;
    _f417++;
    return &_f417;
}

const unsigned int * f418()
{
    TRACE("const unsigned int * f418()");
    static unsigned int _f418 = 0;
    _f418++;
    return &_f418;
}

const signed long long * f421()
{
    TRACE("const signed long long * f421()");
    static signed long long _f421 = 0;
    _f421++;
    return &_f421;
}

const unsigned long long * f422()
{
    TRACE("const unsigned long long * f422()");
    static unsigned long long _f422 = 0;
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

const long double * f425()
{
    TRACE("const long double * f425()");
    static long double _f425 = 0;
    _f425++;
    return &_f425;
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

signed char * f433()
{
    TRACE("signed char * f433()");
    static signed char _f433 = 0;
    _f433++;
    return &_f433;
}

unsigned char * f434()
{
    TRACE("unsigned char * f434()");
    static unsigned char _f434 = 0;
    _f434++;
    return &_f434;
}

signed short * f435()
{
    TRACE("signed short * f435()");
    static signed short _f435 = 0;
    _f435++;
    return &_f435;
}

unsigned short * f436()
{
    TRACE("unsigned short * f436()");
    static unsigned short _f436 = 0;
    _f436++;
    return &_f436;
}

signed int * f437()
{
    TRACE("signed int * f437()");
    static signed int _f437 = 0;
    _f437++;
    return &_f437;
}

unsigned int * f438()
{
    TRACE("unsigned int * f438()");
    static unsigned int _f438 = 0;
    _f438++;
    return &_f438;
}

signed long long * f441()
{
    TRACE("signed long long * f441()");
    static signed long long _f441 = 0;
    _f441++;
    return &_f441;
}

unsigned long long * f442()
{
    TRACE("unsigned long long * f442()");
    static unsigned long long _f442 = 0;
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

long double * f445()
{
    TRACE("long double * f445()");
    static long double _f445 = 0;
    _f445++;
    return &_f445;
}

// ---------------------------------------------------------------------------

void f511(const bool * p0)
{
    TRACE("void f511(const bool *)");
    static bool _f511 = 0;
    _f511 = !_f511;
    if (*p0 != _f511) ABORT_ERROR("void f511(const bool *)");
}

void f512(const char * p0)
{
    TRACE("void f512(const char *)");
    static char _f512 = 0;
    _f512++;
    if (*p0 != _f512) ABORT_ERROR("void f512(const char *)");
}

void f513(const signed char * p0)
{
    TRACE("void f513(const signed char *)");
    static signed char _f513 = 0;
    _f513++;
    if (*p0 != _f513) ABORT_ERROR("void f513(const signed char *)");
}

void f514(const unsigned char * p0)
{
    TRACE("void f514(const unsigned char *)");
    static unsigned char _f514 = 0;
    _f514++;
    if (*p0 != _f514) ABORT_ERROR("void f514(const unsigned char *)");
}

void f515(const signed short * p0)
{
    TRACE("void f515(const signed short *)");
    static signed short _f515 = 0;
    _f515++;
    if (*p0 != _f515) ABORT_ERROR("void f515(const signed short *)");
}

void f516(const unsigned short * p0)
{
    TRACE("void f516(const unsigned short *)");
    static unsigned short _f516 = 0;
    _f516++;
    if (*p0 != _f516) ABORT_ERROR("void f516(const unsigned short *)");
}

void f517(const signed int * p0)
{
    TRACE("void f517(const signed int *)");
    static signed int _f517 = 0;
    _f517++;
    if (*p0 != _f517) ABORT_ERROR("void f517(const signed int *)");
}

void f518(const unsigned int * p0)
{
    TRACE("void f518(const unsigned int *)");
    static unsigned int _f518 = 0;
    _f518++;
    if (*p0 != _f518) ABORT_ERROR("void f518(const unsigned int *)");
}

void f521(const signed long long * p0)
{
    TRACE("void f521(const signed long long *)");
    static signed long long _f521 = 0;
    _f521++;
    if (*p0 != _f521) ABORT_ERROR("void f521(const signed long long *)");
}

void f522(const unsigned long long * p0)
{
    TRACE("void f522(const unsigned long long *)");
    static unsigned long long _f522 = 0;
    _f522++;
    if (*p0 != _f522) ABORT_ERROR("void f522(const unsigned long long *)");
}

void f523(const float * p0)
{
    TRACE("void f523(const float *)");
    static float _f523 = 0;
    _f523++;
    if (*p0 != _f523) ABORT_ERROR("void f523(const float *)");
}

void f524(const double * p0)
{
    TRACE("void f524(const double *)");
    static double _f524 = 0;
    _f524++;
    if (*p0 != _f524) ABORT_ERROR("void f524(const double *)");
}

void f525(const long double * p0)
{
    TRACE("void f525(const long double *)");
    static long double _f525 = 0;
    _f525++;
    if (*p0 != _f525) ABORT_ERROR("void f525(const long double *)");
}

void f531(bool * p0)
{
    TRACE("void f531(bool *)");
    static bool _f531 = 0;
    _f531 = !_f531;
    if (*p0 != _f531) ABORT_ERROR("void f531(bool *)");
    *p0 = !*p0;
    _f531 = !_f531;
}

void f532(char * p0)
{
    TRACE("void f532(char *)");
    static char _f532 = 0;
    _f532++;
    if (*p0 != _f532) ABORT_ERROR("void f532(char *)");
    (*p0)++;
    _f532++;
}

void f533(signed char * p0)
{
    TRACE("void f533(signed char *)");
    static signed char _f533 = 0;
    _f533++;
    if (*p0 != _f533) ABORT_ERROR("void f533(signed char *)");
    (*p0)++;
    _f533++;
}

void f534(unsigned char * p0)
{
    TRACE("void f534(unsigned char *)");
    static unsigned char _f534 = 0;
    _f534++;
    if (*p0 != _f534) ABORT_ERROR("void f534(unsigned char *)");
    (*p0)++;
    _f534++;
}

void f535(signed short * p0)
{
    TRACE("void f535(signed short *)");
    static signed short _f535 = 0;
    _f535++;
    if (*p0 != _f535) ABORT_ERROR("void f535(signed short *)");
    (*p0)++;
    _f535++;
}

void f536(unsigned short * p0)
{
    TRACE("void f536(unsigned short *)");
    static unsigned short _f536 = 0;
    _f536++;
    if (*p0 != _f536) ABORT_ERROR("void f536(unsigned short *)");
    (*p0)++;
    _f536++;
}

void f537(signed int * p0)
{
    TRACE("void f537(signed int *)");
    static signed int _f537 = 0;
    _f537++;
    if (*p0 != _f537) ABORT_ERROR("void f537(signed int *)");
    (*p0)++;
    _f537++;
}

void f538(unsigned int * p0)
{
    TRACE("void f538(unsigned int *)");
    static unsigned int _f538 = 0;
    _f538++;
    if (*p0 != _f538) ABORT_ERROR("void f538(unsigned int *)");
    (*p0)++;
    _f538++;
}

void f541(signed long long * p0)
{
    TRACE("void f541(signed long long *)");
    static signed long long _f541 = 0;
    _f541++;
    if (*p0 != _f541) ABORT_ERROR("void f541(signed long long *)");
    (*p0)++;
    _f541++;
}

void f542(unsigned long long * p0)
{
    TRACE("void f542(unsigned long long *)");
    static unsigned long long _f542 = 0;
    _f542++;
    if (*p0 != _f542) ABORT_ERROR("void f542(unsigned long long *)");
    (*p0)++;
    _f542++;
}

void f543(float * p0)
{
    TRACE("void f543(float *)");
    static float _f543 = 0;
    _f543++;
    if (*p0 != _f543) ABORT_ERROR("void f543(float *)");
    (*p0)++;
    _f543++;
}

void f544(double * p0)
{
    TRACE("void f544(double *)");
    static double _f544 = 0;
    _f544++;
    if (*p0 != _f544) ABORT_ERROR("void f544(double *)");
    (*p0)++;
    _f544++;
}

void f545(long double * p0)
{
    TRACE("void f545(long double *)");
    static long double _f545 = 0;
    _f545++;
    if (*p0 != _f545) ABORT_ERROR("void f545(long double *)");
    (*p0)++;
    _f545++;
}

// ---------------------------------------------------------------------------
