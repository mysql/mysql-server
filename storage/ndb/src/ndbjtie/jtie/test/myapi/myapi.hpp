/*
 Copyright (c) 2010, 2024, Oracle and/or its affiliates.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2.0,
 as published by the Free Software Foundation.

 This program is designed to work with certain software (including
 but not limited to OpenSSL) that is licensed under separate terms,
 as designated in a particular file or component or in included license
 documentation.  The authors of MySQL hereby grant you an additional
 permission to link the program and your derivative works with the
 separately licensed software that they have either included with
 the program or referenced in the documentation.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/
/*
 * myapi.hpp
 */

#ifndef _myapi
#define _myapi

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "helpers.hpp"

// ----------------------------------------------------------------------
//  initializer and finalizer functions
// ----------------------------------------------------------------------

// initializer avoiding issues with static construction of objects
extern void myapi_init();
extern void myapi_finit();

// ----------------------------------------------------------------------
// void result/parameter types
// ----------------------------------------------------------------------

extern void f0();

// ----------------------------------------------------------------------
// [const] void/char * [const] result/parameter types
// ----------------------------------------------------------------------

// non-NULL-returning/accepting functions

extern const void *s010();
extern const char *s012();
extern void *s030();
extern char *s032();

extern void s110(const void *p0);
extern void s112(const char *p0);
extern void s130(void *p0);
extern void s132(char *p0);
extern void s150(const void *const p0);
extern void s152(const char *const p0);
extern void s170(void *const p0);
extern void s172(char *const p0);

// NULL-returning/accepting functions

extern const void *s210();
extern const char *s212();
extern void *s230();
extern char *s232();

extern void s310(const void *p0);
extern void s312(const char *p0);
extern void s330(void *p0);
extern void s332(char *p0);
extern void s350(const void *const p0);
extern void s352(const char *const p0);
extern void s370(void *const p0);
extern void s372(char *const p0);

// ----------------------------------------------------------------------
// all primitive result/parameter types
// ----------------------------------------------------------------------

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

// ----------------------------------------------------------------------
// all fixed-size primitive result/parameter types
// ----------------------------------------------------------------------

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

// ----------------------------------------------------------------------
// references of primitive result/parameter types
// ----------------------------------------------------------------------

extern const bool &f211();
extern const char &f212();
extern const int8_t &f213();
extern const uint8_t &f214();
extern const int16_t &f215();
extern const uint16_t &f216();
extern const int32_t &f217();
extern const uint32_t &f218();
extern const int64_t &f221();
extern const uint64_t &f222();
extern const float &f223();
extern const double &f224();

extern bool &f231();
extern char &f232();
extern int8_t &f233();
extern uint8_t &f234();
extern int16_t &f235();
extern uint16_t &f236();
extern int32_t &f237();
extern uint32_t &f238();
extern int64_t &f241();
extern uint64_t &f242();
extern float &f243();
extern double &f244();

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

// ----------------------------------------------------------------------
// pointers to primitive result/parameter types (array size == 1)
// ----------------------------------------------------------------------

extern const bool *f411();
extern const char *f412();
extern const int8_t *f413();
extern const uint8_t *f414();
extern const int16_t *f415();
extern const uint16_t *f416();
extern const int32_t *f417();
extern const uint32_t *f418();
extern const int64_t *f421();
extern const uint64_t *f422();
extern const float *f423();
extern const double *f424();

extern bool *f431();
extern char *f432();
extern int8_t *f433();
extern uint8_t *f434();
extern int16_t *f435();
extern uint16_t *f436();
extern int32_t *f437();
extern uint32_t *f438();
extern int64_t *f441();
extern uint64_t *f442();
extern float *f443();
extern double *f444();

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

extern void f551(const bool *const);
extern void f552(const char *const);
extern void f553(const int8_t *const);
extern void f554(const uint8_t *const);
extern void f555(const int16_t *const);
extern void f556(const uint16_t *const);
extern void f557(const int32_t *const);
extern void f558(const uint32_t *const);
extern void f561(const int64_t *const);
extern void f562(const uint64_t *const);
extern void f563(const float *const);
extern void f564(const double *const);

extern void f571(bool *const);
extern void f572(char *const);
extern void f573(int8_t *const);
extern void f574(uint8_t *const);
extern void f575(int16_t *const);
extern void f576(uint16_t *const);
extern void f577(int32_t *const);
extern void f578(uint32_t *const);
extern void f581(int64_t *const);
extern void f582(uint64_t *const);
extern void f583(float *const);
extern void f584(double *const);

// ----------------------------------------------------------------------
// pointers to primitive result/parameter types (array size == 0)
// ----------------------------------------------------------------------

extern const bool *f611();
extern const char *f612();
extern const int8_t *f613();
extern const uint8_t *f614();
extern const int16_t *f615();
extern const uint16_t *f616();
extern const int32_t *f617();
extern const uint32_t *f618();
extern const int64_t *f621();
extern const uint64_t *f622();
extern const float *f623();
extern const double *f624();

extern bool *f631();
extern char *f632();
extern int8_t *f633();
extern uint8_t *f634();
extern int16_t *f635();
extern uint16_t *f636();
extern int32_t *f637();
extern uint32_t *f638();
extern int64_t *f641();
extern uint64_t *f642();
extern float *f643();
extern double *f644();

extern void f711(const bool *);
extern void f712(const char *);
extern void f713(const int8_t *);
extern void f714(const uint8_t *);
extern void f715(const int16_t *);
extern void f716(const uint16_t *);
extern void f717(const int32_t *);
extern void f718(const uint32_t *);
extern void f721(const int64_t *);
extern void f722(const uint64_t *);
extern void f723(const float *);
extern void f724(const double *);

extern void f731(bool *);
extern void f732(char *);
extern void f733(int8_t *);
extern void f734(uint8_t *);
extern void f735(int16_t *);
extern void f736(uint16_t *);
extern void f737(int32_t *);
extern void f738(uint32_t *);
extern void f741(int64_t *);
extern void f742(uint64_t *);
extern void f743(float *);
extern void f744(double *);

extern void f751(const bool *const);
extern void f752(const char *const);
extern void f753(const int8_t *const);
extern void f754(const uint8_t *const);
extern void f755(const int16_t *const);
extern void f756(const uint16_t *const);
extern void f757(const int32_t *const);
extern void f758(const uint32_t *const);
extern void f761(const int64_t *const);
extern void f762(const uint64_t *const);
extern void f763(const float *const);
extern void f764(const double *const);

extern void f771(bool *const);
extern void f772(char *const);
extern void f773(int8_t *const);
extern void f774(uint8_t *const);
extern void f775(int16_t *const);
extern void f776(uint16_t *const);
extern void f777(int32_t *const);
extern void f778(uint32_t *const);
extern void f781(int64_t *const);
extern void f782(uint64_t *const);
extern void f783(float *const);
extern void f784(double *const);

// ----------------------------------------------------------------------
// object result/parameter types
// ----------------------------------------------------------------------

struct B0 {
  static int32_t d0s;
  static const int32_t d0sc;

  int32_t d0;
  const int32_t d0c;

  static void init();

  static void finit();

  B0() : d0(21), d0c(-21) { TRACE("B0()"); }

  B0(const B0 &b0) : d0(b0.d0), d0c(b0.d0c) {
    TRACE("B0(const B0 &)");
    ABORT_ERROR("!USE OF COPY CONSTRUCTOR!");
  }

  virtual ~B0() {}

  B0 &operator=(const B0 &p) {
    TRACE("B0 & operator=(const B0 &)");
    (void)p;
    ABORT_ERROR("!USE OF ASSIGNMENT OPERATOR!");
    return *this;
  }

  // ----------------------------------------------------------------------

  static int32_t f0s() {
    TRACE("int32_t B0::f0s()");
    return 20;
  }

  int32_t f0n() const {
    TRACE("int32_t B0::f0n()");
    return 21;
  }

  virtual int32_t f0v() const {
    TRACE("int32_t B0::f0v()");
    return 22;
  }
};

struct B1 : public B0 {
  static int32_t d0s;
  static const int32_t d0sc;

  int32_t d0;
  const int32_t d0c;

  static void init();

  static void finit();

  B1() : d0(31), d0c(-31) { TRACE("B1()"); }

  B1(const B1 &b1) : B0(b1), d0(b1.d0), d0c(b1.d0c) {
    TRACE("B1(const B1 &)");
    ABORT_ERROR("!USE OF COPY CONSTRUCTOR!");
  }

  ~B1() override {}

  B1 &operator=(const B1 &p) {
    TRACE("B1 & operator=(const B1 &)");
    (void)p;
    ABORT_ERROR("!USE OF ASSIGNMENT OPERATOR!");
    return *this;
  }

  // ----------------------------------------------------------------------

  static int32_t f0s() {
    TRACE("int32_t B1::f0s()");
    return 30;
  }

  int32_t f0n() const {
    TRACE("int32_t B1::f0n()");
    return 31;
  }

  int32_t f0v() const override {
    TRACE("int32_t B1::f0v()");
    return 32;
  }
};

struct A {
  static A *a;
  static int32_t d0s;
  static const int32_t d0sc;

  int32_t d0;
  const int32_t d0c;

  static void init();

  static void finit();

  A() : d0(11), d0c(-11) { TRACE("A()"); }

  A(int i) : d0(11), d0c(-11) {
    TRACE("A(int)");
    (void)i;
  }

  A(const A &a) : d0(a.d0), d0c(a.d0c) {
    TRACE("A(const A &)");
    ABORT_ERROR("!USE OF COPY CONSTRUCTOR!");
  }

  virtual ~A() { TRACE("~A()"); }

  A &operator=(const A &p) {
    TRACE("A & operator=(const A &)");
    (void)p;
    ABORT_ERROR("!USE OF ASSIGNMENT OPERATOR!");
    return *this;
  }

  // ----------------------------------------------------------------------

  static A *deliver_ptr() {
    TRACE("A * A::deliver_ptr()");
    return A::a;
  }

  static A *deliver_null_ptr() {
    TRACE("A * A::deliver_null_ptr()");
    return NULL;
  }

  static A &deliver_ref() {
    TRACE("A & A::deliver_ref()");
    return *A::a;
  }

  static void take_ptr(A *o) {
    TRACE("void A::take_ptr(A *)");
    if (o != A::a) ABORT_ERROR("void A::take_ptr(A *)");
  }

  static void take_null_ptr(A *o) {
    TRACE("void A::take_null_ptr(A *)");
    if (o != NULL) ABORT_ERROR("void A::take_null_ptr(A *)");
  }

  static void take_ref(A &o) {
    TRACE("void A::take_ref(A &)");
    if (&o != A::a) ABORT_ERROR("void A::take_ref(A &)");
  }

  static void print(A *p0) {
    TRACE("void A::print(A *)");
    printf("    p0 = %p\n", (void *)p0);
    fflush(stdout);
  }

  // ----------------------------------------------------------------------

  B0 *newB0() const {
    TRACE("B0 A::newB0()");
    return new B0();
  }

  B1 *newB1() const {
    TRACE("B1 A::newB1()");
    return new B1();
  }

  static int32_t f0s() {
    TRACE("int32_t A::f0s()");
    return 10;
  }

  int32_t f0n() const {
    TRACE("int32_t A::f0n()");
    return 11;
  }

  virtual int32_t f0v() const {
    TRACE("int32_t A::f0v()");
    return 12;
  }

  void del(B0 &b) {
    TRACE("void A::del(B0 &)");
    delete &b;
  }

  void del(B1 &b) {
    TRACE("void A::del(B1 &)");
    delete &b;
  }

  // ----------------------------------------------------------------------
  // varying number of result/parameters
  // ----------------------------------------------------------------------

  void g0c() const { TRACE("void A::g0c()"); }

  void g1c(int8_t p0) const {
    TRACE("void A::g1c(int8_t)");
    if (p0 != 1) ABORT_ERROR("wrong arg value");
  }

  void g2c(int8_t p0, int16_t p1) const {
    TRACE("void A::g2c(int8_t, int16_t)");
    if (p0 != 1) ABORT_ERROR("wrong arg value");
    if (p1 != 2) ABORT_ERROR("wrong arg value");
  }

  void g3c(int8_t p0, int16_t p1, int32_t p2) const {
    TRACE("void A::g3c(int8_t, int16_t, int32_t)");
    if (p0 != 1) ABORT_ERROR("wrong arg value");
    if (p1 != 2) ABORT_ERROR("wrong arg value");
    if (p2 != 3) ABORT_ERROR("wrong arg value");
  }

  void g0() { TRACE("void A::g0()"); }

  void g1(int8_t p0) {
    TRACE("void A::g1(int8_t)");
    if (p0 != 1) ABORT_ERROR("wrong arg value");
  }

  void g2(int8_t p0, int16_t p1) {
    TRACE("void A::g2(int8_t, int16_t)");
    if (p0 != 1) ABORT_ERROR("wrong arg value");
    if (p1 != 2) ABORT_ERROR("wrong arg value");
  }

  void g3(int8_t p0, int16_t p1, int32_t p2) {
    TRACE("void A::g3(int8_t, int16_t, int32_t)");
    if (p0 != 1) ABORT_ERROR("wrong arg value");
    if (p1 != 2) ABORT_ERROR("wrong arg value");
    if (p2 != 3) ABORT_ERROR("wrong arg value");
  }

  // ----------------------------------------------------------------------

  int32_t g0rc() const {
    TRACE("int32_t A::g0rc()");
    return 0;
  }

  int32_t g1rc(int8_t p0) const {
    TRACE("int32_t A::g1rc(int8_t)");
    if (p0 != 1) ABORT_ERROR("wrong arg value");
    return p0;
  }

  int32_t g2rc(int8_t p0, int16_t p1) const {
    TRACE("int32_t A::g2rc(int8_t, int16_t)");
    if (p0 != 1) ABORT_ERROR("wrong arg value");
    if (p1 != 2) ABORT_ERROR("wrong arg value");
    return p0 + p1;
  }

  int32_t g3rc(int8_t p0, int16_t p1, int32_t p2) const {
    TRACE("int32_t A::g3rc(int8_t, int16_t, int32_t)");
    if (p0 != 1) ABORT_ERROR("wrong arg value");
    if (p1 != 2) ABORT_ERROR("wrong arg value");
    if (p2 != 3) ABORT_ERROR("wrong arg value");
    return p0 + p1 + p2;
  }

  int32_t g0r() {
    TRACE("int32_t A::g0r()");
    return 0;
  }

  int32_t g1r(int8_t p0) {
    TRACE("int32_t A::g1r(int8_t)");
    if (p0 != 1) ABORT_ERROR("wrong arg value");
    return p0;
  }

  int32_t g2r(int8_t p0, int16_t p1) {
    TRACE("int32_t A::g2r(int8_t, int16_t)");
    if (p0 != 1) ABORT_ERROR("wrong arg value");
    if (p1 != 2) ABORT_ERROR("wrong arg value");
    return p0 + p1;
  }

  int32_t g3r(int8_t p0, int16_t p1, int32_t p2) {
    TRACE("int32_t A::g3r(int8_t, int16_t, int32_t)");
    if (p0 != 1) ABORT_ERROR("wrong arg value");
    if (p1 != 2) ABORT_ERROR("wrong arg value");
    if (p2 != 3) ABORT_ERROR("wrong arg value");
    return p0 + p1 + p2;
  }
};

// ----------------------------------------------------------------------

inline void h0() { TRACE("void h0()"); }

inline void h1(int8_t p0) {
  TRACE("void h1(int8_t)");
  if (p0 != 1) ABORT_ERROR("wrong arg value");
}

inline void h2(int8_t p0, int16_t p1) {
  TRACE("void h2(int8_t, int16_t)");
  if (p0 != 1) ABORT_ERROR("wrong arg value");
  if (p1 != 2) ABORT_ERROR("wrong arg value");
}

inline void h3(int8_t p0, int16_t p1, int32_t p2) {
  TRACE("void h3(int8_t, int16_t, int32_t)");
  if (p0 != 1) ABORT_ERROR("wrong arg value");
  if (p1 != 2) ABORT_ERROR("wrong arg value");
  if (p2 != 3) ABORT_ERROR("wrong arg value");
}

inline int32_t h0r() {
  TRACE("int32_t h0r()");
  return 0;
}

inline int32_t h1r(int8_t p0) {
  TRACE("int32_t h1r(int8_t)");
  if (p0 != 1) ABORT_ERROR("wrong arg value");
  return p0;
}

inline int32_t h2r(int8_t p0, int16_t p1) {
  TRACE("int32_t h2r(int8_t, int16_t)");
  if (p0 != 1) ABORT_ERROR("wrong arg value");
  if (p1 != 2) ABORT_ERROR("wrong arg value");
  return p0 + p1;
}

inline int32_t h3r(int8_t p0, int16_t p1, int32_t p2) {
  TRACE("int32_t h3r(int8_t, int16_t, int32_t)");
  if (p0 != 1) ABORT_ERROR("wrong arg value");
  if (p1 != 2) ABORT_ERROR("wrong arg value");
  if (p2 != 3) ABORT_ERROR("wrong arg value");
  return p0 + p1 + p2;
}

// ----------------------------------------------------------------------
// [non-]const member functions and object[-array] result/parameter types
// ----------------------------------------------------------------------

struct C0 {
  static C0 *c;
  static const C0 *cc;

  const int64_t id;

  static void init();

  static void finit();

  C0() : id((int64_t)this) { TRACE("C0()"); }

  C0(const C0 &o) : id(o.id) {
    TRACE("C0(const C0 &)");
    (void)o;
    ABORT_ERROR("!USE OF COPY CONSTRUCTOR!");
  }

  virtual ~C0() { TRACE("~C0()"); }

  C0 &operator=(const C0 &o) {
    TRACE("C0 & operator=(const C0 &)");
    (void)o;
    ABORT_ERROR("!USE OF ASSIGNMENT OPERATOR!");
    return *this;
  }

  // ----------------------------------------------------------------------
  // static (on purpose) array functions
  // ----------------------------------------------------------------------

  static C0 *pass(C0 *c0) { return c0; }

  static const C0 *pass(const C0 *c0) { return c0; }

  static int64_t hash(const C0 *c0, int32_t n) {
    TRACE("int64_t C0::hash(const C0 *, int32_t)");
    if (c0 == NULL) ABORT_ERROR("c0 == NULL");
    if (n < 0) ABORT_ERROR("n < 0");

    int64_t r = 0;
    for (int i = 0; i < n; i++) {
      r ^= c0[i].id;
    }
    return r;
  }

  // ----------------------------------------------------------------------
  // (non-virtual) instance (on purpose) array functions
  // ----------------------------------------------------------------------

  void check(int64_t id) const {
    TRACE("void check(int64_t) const");
    if (id != this->id) ABORT_ERROR("id != this->id");
  }

  void print() const {
    TRACE("void C0::print() const");
    printf("    this->id = %llx\n", (long long unsigned int)id);
    fflush(stdout);
  }

  const C0 *deliver_C0Cp() const {
    TRACE("const C0 * C0::deliver_C0Cp() const");
    return cc;
  }

  const C0 &deliver_C0Cr() const {
    TRACE("const C0 & C0::deliver_C0Cr() const");
    return *cc;
  }

  void take_C0Cp(const C0 *cp) const {
    TRACE("void C0::take_C0Cp(const C0 *) const");
    if (cp != C0::c && cp != C0::cc) ABORT_ERROR("cp != C0::c && cp != C0::cc");
  }

  void take_C0Cr(const C0 &cp) const {
    TRACE("void C0::take_C0Cr(const C0 &) const");
    if (&cp != C0::c && &cp != C0::cc)
      ABORT_ERROR("&cp != C0::c && &cp != C0::cc");
  }

  C0 *deliver_C0p() {
    TRACE("C0 * C0::deliver_C0p()");
    return c;
  }

  C0 &deliver_C0r() {
    TRACE("C0 & C0::deliver_C0r()");
    return *c;
  }

  void take_C0p(C0 *p) {
    TRACE("void C0::take_C0p(C0 *)");
    if (p != C0::c) ABORT_ERROR("p != C0::c");
  }

  void take_C0r(C0 &p) {
    TRACE("void C0::take_C0r(C0 &)");
    if (&p != C0::c) ABORT_ERROR("&p != C0::c");
  }
};

struct C1 : public C0 {
  static C1 *c;
  static const C1 *cc;

  static void init();

  static void finit();

  C1() { TRACE("C1()"); }

  C1(const C1 &o) : C0(o) {
    TRACE("C1(const C1 &)");
    (void)o;
    ABORT_ERROR("!USE OF COPY CONSTRUCTOR!");
  }

  ~C1() override { TRACE("~C1()"); }

  C1 &operator=(const C1 &p) {
    TRACE("C1 & operator=(const C1 &)");
    (void)p;
    ABORT_ERROR("!USE OF ASSIGNMENT OPERATOR!");
    return *this;
  }

  // ----------------------------------------------------------------------
  // static (on purpose) array functions
  // ----------------------------------------------------------------------

  static C1 *pass(C1 *c1) { return c1; }

  static const C1 *pass(const C1 *c1) { return c1; }

  static int64_t hash(const C1 *c1, int32_t n) {
    TRACE("int64_t C1::hash(const C1 *, int32_t)");
    if (c1 == NULL) ABORT_ERROR("c1 == NULL");
    if (n < 0) ABORT_ERROR("n < 0");

    int64_t r = 0;
    for (int i = 0; i < n; i++) {
      r ^= c1[i].id;
    }
    return r;
  }

  // ----------------------------------------------------------------------
  // (non-virtual) instance (on purpose) array functions
  // ----------------------------------------------------------------------

  const C1 *deliver_C1Cp() const {
    TRACE("const C1 * C1::deliver_C1Cp() const");
    return cc;
  }

  const C1 &deliver_C1Cr() const {
    TRACE("const C1 & C1::deliver_C1Cr() const");
    return *cc;
  }

  void take_C1Cp(const C1 *cp) const {
    TRACE("void C1::take_C1Cp(const C1 *) const");
    if (cp != C1::c && cp != C1::cc) ABORT_ERROR("cp != C1::c && cp != C1::cc");
  }

  void take_C1Cr(const C1 &cp) const {
    TRACE("void C1::take_C1Cr(const C1 &) const");
    if (&cp != C1::c && &cp != C1::cc)
      ABORT_ERROR("&cp != C1::c && &cp != C1::cc");
  }

  C1 *deliver_C1p() {
    TRACE("C1 * C1::deliver_C1p()");
    return c;
  }

  C1 &deliver_C1r() {
    TRACE("C1 & C1::deliver_C1r()");
    return *c;
  }

  void take_C1p(C1 *p) {
    TRACE("void C1::take_C1p(C1 *)");
    if (p != C1::c) ABORT_ERROR("p != C1::c");
  }

  void take_C1r(C1 &p) {
    TRACE("void C1::take_C1r(C1 &)");
    if (&p != C1::c) ABORT_ERROR("&p != C1::c");
  }
};

// ----------------------------------------------------------------------
// overriding and virtual/non-virtual functions
// ----------------------------------------------------------------------

struct D1;

struct D0 {
  static D0 *d;
  static void init();
  static void finit();
  virtual ~D0() {}

  int f_d0() {
    TRACE("D0::f_d0()");
    return 20;
  }
  int f_nv() {
    TRACE("D0::f_nv()");
    return 21;
  }
  virtual int f_v() {
    TRACE("D0::f_v()");
    return 22;
  }
  static D1 *sub();
};

struct D1 : D0 {
  static D1 *d;
  static void init();
  static void finit();
  ~D1() override {}

  int f_d1() {
    TRACE("D0::f_d1()");
    return 30;
  }
  int f_nv() {
    TRACE("D1::f_nv()");
    return 31;
  }
  int f_v() override {
    TRACE("D1::f_v()");
    return 32;
  }
  static D1 *sub();
};

struct D2 : D1 {
  static D2 *d;
  static void init();
  static void finit();
  ~D2() override {}

  int f_d2() {
    TRACE("D2::f_d2()");
    return 40;
  }
  int f_nv() {
    TRACE("D2::f_nv()");
    return 41;
  }
  int f_v() override {
    TRACE("D2::f_v()");
    return 42;
  }
  static D1 *sub();
};

// d1class instance returns (casts unnecessary but for attention)
inline D1 *D0::sub() {
  TRACE("D1 * D0::sub()");
  return ((D1 *)D1::d);
}  // D1
inline D1 *D1::sub() {
  TRACE("D1 * D1::sub()");
  return ((D1 *)D2::d);
}  // D2
inline D1 *D2::sub() {
  TRACE("D1 * D2::sub()");
  return NULL;
}  // --

// ----------------------------------------------------------------------
// enums
// ----------------------------------------------------------------------

struct E {
  enum EE { EE0, EE1 };

  static EE deliver_EE1() {
    TRACE("E::EE E::deliver_EE1()");
    return EE1;
  }

  static void take_EE1(EE e) {
    TRACE("void E::take_EE1(E::EE)");
    if (e != EE1) ABORT_ERROR("e != EE1");
  }

  static void take_EE1c(const EE e) {
    TRACE("void E::take_EE1c(const E::EE)");
    if (e != EE1) ABORT_ERROR("e != EE1");
  }

 private:
  // no need to instantiate
  E() { TRACE("E()"); }

  E(const E &o) {
    TRACE("E(const E &)");
    (void)o;
    ABORT_ERROR("!USE OF COPY CONSTRUCTOR!");
  }

  virtual ~E() { TRACE("~E()"); }

  E &operator=(const E &o) {
    TRACE("E & operator=(const E &)");
    (void)o;
    ABORT_ERROR("!USE OF ASSIGNMENT OPERATOR!");
    return *this;
  }
};

// ----------------------------------------------------------------------

#endif  // _myapi
