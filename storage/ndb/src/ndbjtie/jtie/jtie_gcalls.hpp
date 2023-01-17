/*
 Copyright (c) 2010, 2023, Oracle and/or its affiliates.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2.0,
 as published by the Free Software Foundation.

 This program is also distributed with certain software (including
 but not limited to OpenSSL) that is licensed under separate terms,
 as designated in a particular file or component or in included license
 documentation.  The authors of MySQL hereby grant you an additional
 permission to link the program and your derivative works with the
 separately licensed software that they have included with MySQL.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/
/*
 * jtie_gcalls.hpp
 */

#ifndef jtie_gcalls_hpp
#define jtie_gcalls_hpp

#include "jtie_stdint.h"
#include "jtie_tconv_impl.hpp"
#include "jtie_tconv_object_impl.hpp"
#include "helpers.hpp"

// ---------------------------------------------------------------------------
// generic wrapper function definitions
// ---------------------------------------------------------------------------

// XXX document workaround for MSVC's problems with template disambiguation:
//   gcall -> gcall_fr, gcall_fv, gcall_mfr, gcall_mfv

// XXX update comments below on alternate handling of const member functions

// Design and Implementation Notes:
//
// - The function templates (gcall<...>() et al) in this file implement
//   generically the delegation of Java method calls to C++ functions.
//
//   While the template definitions are schematic, they are quite numerous.
//   For example, to support up to 10-ary functions, 60 + 6 + 1 = 67 wrapper
//   template definitions need to written (plus 11 + 1 for c'tor, d'tor).
//
//   Therefore, preprocessor macros are used below for generating the
//   n-ary template definitions allowing for drastically cutting down the
//   code to the basic patterns -- at the expense of readability, however.
//
// - The templates' type parameters allow to abstract from the formal types
//   of parameters, result, and object invocation target.  The diverse data
//   conversions between the actual Java and C++ parameters are carried out
//   by the classes Param<>, Result<>, and Target<>, respectively.
//
// - In contrast, it's more difficult to abstract from the number of
//   parameters using templates for maximum compile-time computation.
//   Other options:
//   - variadic templates: not in the C++ standard yet
//   - using template tuple (or recursive list) types to assemble the call
//     arguments into a single compile-time data structure: poses code upon
//     the caller, especially, when having to specify the full signature
//     of the C function as template arguments
//
// - In addition, by the C++ rules on matching formal and actual template
//   parameters for function types, *six* separate, "overloaded" definitions
//   are needed for each n-ary wrapper function template:
//
//     Category:                        Template Parameter Signature:
//     1 global functions
//         1.1 w/o return:              void F(...)
//         1.2 w/ return:               RT::CF_t F(...)
//     2 static member functions
//         2.1 w/o return:              same as 1.1
//         2.2 w/ return:               same as 1.2
//     3 non-static member functions
//         3.1 non-const member
//             3.1.1 w/o return:        void (OT::CF_t::*F)(...)
//             3.1.2 w/ return:         RT::CF_t (OT::CF_t::*F)(...)
//         3.2 const member
//             3.2.1 w/o return:        void (OT::CF_t::*F)(...) const
//             3.2.2 w/ return:         RT::CF_t (OT::CF_t::*F)(...) const
//
//   Other options:
//   - 'void' can be used as argument for a formal template type parameter,
//     collapsing the const/non-const patterns: works only in simple cases,
//     not with code/return stmts for Java-C++ parameter/result conversions
//
//   - Introduce a C function call wrapper class templates that abstract from
//     the signature differences hiding them from the gcall<...>() functions:
//     poses code upon the caller to construct the C call wrapper type with
//     the list all the C function's return/parameter types.
//
// - Target objects are internally (OT::CA_t) hold by reference (they must
//   not be null, which Target<> checks during JA_t -> CA_t conversion).
//   Hence, the code below applies a pointer-to-function-member
//     (cao.*F)()   -- on object
//    [(cao->*F)()  -- on pointer-to-object, if held as pointer]
//
// - One must be careful not to trigger any copy-constructing at explicit
//   type casts between an object (result) from the C formal to the actual
//   type (e.g., A & -> A) using the cast<> function template, e.g.:
//     cast< typename OT::CF_t, typename OT::CA_t >(cao)
//     cast< typename P0T::CF_t, typename P0T::CA_t >(cap0)
//     ...
//
//   However, this issue is moot now, since all casts between the formal
//   and actual C types were removed under the requirement that these types
//   have to be assignment compatible.
//
//   An application is not affected when using the mapping as generated
//   by the pre-defined macro
//      define_class_mapping( J, C )
//   for user-defined classes.
//
// - Generic wrapper functions gcreate<>() and gdelete<>() are provided that
//   allow calling C++ constructors and destructors.  Unlike the gcall<>()
//   wrapper function template, the gcreate/gdelete do not take the name of
//   a C++ function as template parameter, since constructors/destructors
//   do not have a (member) name and cannot be passed as template arguments.
//
//   To be able to invoke and map to constructors/destructors through the
//   same framework used for ordinary functions, internal low-level wrapper
//   class templates ConstructorX/Destructor are defined with raw C++
//   arguments/results, to which gcreate<>() and gdelete<>() delegate.
//   (Cannot use function templates here, for need of partial specialization.)
//
//   The internal ConstructorX/Destructor wrappers provide the result or
//   parameter as both, reference or pointer types.
//
//   This way, the application can choose between a reference or pointer
//   type mapping of the result/parameter (reference conversion checking
//   for NULL and raising a proper Java exception).

// ---------------------------------------------------------------------------
// List Generation Macros
// ---------------------------------------------------------------------------

// a macro used as element in a list; the argument is expanded, e.g.
//    #define MY_MACRO(n) a##n
//    LE(MY_MACRO(3))  -->  a3
#define LE(x) x

// a macro generating a blank-separated list
//
// usage: pass the name of a macro taking a number argument, e.g.
//    #define MY_MACRO(n) a##n
//    BSL0(MY_MACRO)  -->
//    BSL1(MY_MACRO)  -->  a1
//    BSL2(MY_MACRO)  -->  a1 a2
#define BSL0(m)
#define BSL1(m)           LE(m(1))
#define BSL2(m)  BSL1(m)  LE(m(2))
#define BSL3(m)  BSL2(m)  LE(m(3))
#define BSL4(m)  BSL3(m)  LE(m(4))
#define BSL5(m)  BSL4(m)  LE(m(5))
#define BSL6(m)  BSL5(m)  LE(m(6))
#define BSL7(m)  BSL6(m)  LE(m(7))
#define BSL8(m)  BSL7(m)  LE(m(8))
#define BSL9(m)  BSL8(m)  LE(m(9))
#define BSL10(m) BSL9(m)  LE(m(10))
#define BSL11(m) BSL10(m) LE(m(11))
#define BSL12(m) BSL11(m) LE(m(12))
#define BSL13(m) BSL12(m) LE(m(13))
#define BSL14(m) BSL13(m) LE(m(14))
#define BSL15(m) BSL14(m) LE(m(15))
#define BSL16(m) BSL15(m) LE(m(16))
#define BSL17(m) BSL16(m) LE(m(17))
#define BSL18(m) BSL17(m) LE(m(18))
#define BSL19(m) BSL18(m) LE(m(19))

// a macro generating a blank-separated list in reverse order
//
// usage: pass the name of a macro taking a number argument, e.g.
//    #define MY_MACRO(n) a##n
//    RBSL0(MY_MACRO)  -->
//    RBSL1(MY_MACRO)  -->  a1
//    RBSL2(MY_MACRO)  -->  a2 a1
#define RBSL0(m)
#define RBSL1(m)  LE(m(1))         
#define RBSL2(m)  LE(m(2))  RBSL1(m)
#define RBSL3(m)  LE(m(3))  RBSL2(m)
#define RBSL4(m)  LE(m(4))  RBSL3(m)
#define RBSL5(m)  LE(m(5))  RBSL4(m)
#define RBSL6(m)  LE(m(6))  RBSL5(m)
#define RBSL7(m)  LE(m(7))  RBSL6(m)
#define RBSL8(m)  LE(m(8))  RBSL7(m)
#define RBSL9(m)  LE(m(9))  RBSL8(m)
#define RBSL10(m) LE(m(10)) RBSL9(m)
#define RBSL11(m) LE(m(11)) RBSL10(m)
#define RBSL12(m) LE(m(12)) RBSL11(m)
#define RBSL13(m) LE(m(13)) RBSL12(m)
#define RBSL14(m) LE(m(14)) RBSL13(m)
#define RBSL15(m) LE(m(15)) RBSL14(m)
#define RBSL16(m) LE(m(16)) RBSL15(m)
#define RBSL17(m) LE(m(17)) RBSL16(m)
#define RBSL18(m) LE(m(18)) RBSL17(m)
#define RBSL19(m) LE(m(19)) RBSL18(m)

// a macro generating a comma-separated list
//
// usage: pass the name of a macro taking a number argument, e.g.
//    #define MY_MACRO(n) a##n
//    CSL0(MY_MACRO)  -->
//    CSL1(MY_MACRO)  -->  a1
//    CSL2(MY_MACRO)  -->  a1, a2
#define CSL0(m)
#define CSL1(m)            LE(m(1))
#define CSL2(m)  CSL1(m),  LE(m(2))
#define CSL3(m)  CSL2(m),  LE(m(3))
#define CSL4(m)  CSL3(m),  LE(m(4))
#define CSL5(m)  CSL4(m),  LE(m(5))
#define CSL6(m)  CSL5(m),  LE(m(6))
#define CSL7(m)  CSL6(m),  LE(m(7))
#define CSL8(m)  CSL7(m),  LE(m(8))
#define CSL9(m)  CSL8(m),  LE(m(9))
#define CSL10(m) CSL9(m),  LE(m(10))
#define CSL11(m) CSL10(m), LE(m(11))
#define CSL12(m) CSL11(m), LE(m(12))
#define CSL13(m) CSL12(m), LE(m(13))
#define CSL14(m) CSL13(m), LE(m(14))
#define CSL15(m) CSL14(m), LE(m(15))
#define CSL16(m) CSL15(m), LE(m(16))
#define CSL17(m) CSL16(m), LE(m(17))
#define CSL18(m) CSL17(m), LE(m(18))
#define CSL19(m) CSL18(m), LE(m(19))

// a macro generating a comma-preceded list
//
// usage: pass the name of a macro taking a number argument, e.g.
//    #define MY_MACRO(n) a##n
//    CPL0(MY_MACRO)  -->
//    CPL1(MY_MACRO)  -->  ,a1
//    CPL2(MY_MACRO)  -->  ,a1 ,a2
#define CPL0(m)
#define CPL1(m)           ,LE(m(1))
#define CPL2(m)  CPL1(m)  ,LE(m(2))
#define CPL3(m)  CPL2(m)  ,LE(m(3))
#define CPL4(m)  CPL3(m)  ,LE(m(4))
#define CPL5(m)  CPL4(m)  ,LE(m(5))
#define CPL6(m)  CPL5(m)  ,LE(m(6))
#define CPL7(m)  CPL6(m)  ,LE(m(7))
#define CPL8(m)  CPL7(m)  ,LE(m(8))
#define CPL9(m)  CPL8(m)  ,LE(m(9))
#define CPL10(m) CPL9(m)  ,LE(m(10))
#define CPL11(m) CPL10(m) ,LE(m(11))
#define CPL12(m) CPL11(m) ,LE(m(12))
#define CPL13(m) CPL12(m) ,LE(m(13))
#define CPL14(m) CPL13(m) ,LE(m(14))
#define CPL15(m) CPL14(m) ,LE(m(15))
#define CPL16(m) CPL15(m) ,LE(m(16))
#define CPL17(m) CPL16(m) ,LE(m(17))
#define CPL18(m) CPL17(m) ,LE(m(18))
#define CPL19(m) CPL18(m) ,LE(m(19))

// a macro generating a comma-terminated list
//
// usage: pass the name of a macro taking a number argument, e.g.
//    #define MY_MACRO(n) a##n
//    CTL0(MY_MACRO)  -->
//    CTL1(MY_MACRO)  -->  a1,
//    CTL2(MY_MACRO)  -->  a1, a2,
#define CTL0(m)
#define CTL1(m)           LE(m(1)),
#define CTL2(m)  CTL1(m)  LE(m(2)),
#define CTL3(m)  CTL2(m)  LE(m(3)),
#define CTL4(m)  CTL3(m)  LE(m(4)),
#define CTL5(m)  CTL4(m)  LE(m(5)),
#define CTL6(m)  CTL5(m)  LE(m(6)),
#define CTL7(m)  CTL6(m)  LE(m(7)),
#define CTL8(m)  CTL7(m)  LE(m(8)),
#define CTL9(m)  CTL8(m)  LE(m(9)),
#define CTL10(m) CTL9(m)  LE(m(10)),
#define CTL11(m) CTL10(m) LE(m(11)),
#define CTL12(m) CTL11(m) LE(m(12)),
#define CTL13(m) CTL12(m) LE(m(13)),
#define CTL14(m) CTL13(m) LE(m(14)),
#define CTL15(m) CTL14(m) LE(m(15)),
#define CTL16(m) CTL15(m) LE(m(16)),
#define CTL17(m) CTL16(m) LE(m(17)),
#define CTL18(m) CTL17(m) LE(m(18)),
#define CTL19(m) CTL18(m) LE(m(19)),

// ---------------------------------------------------------------------------
// Stringification Macros
// ---------------------------------------------------------------------------

// macro to stringify arguments, which are not expanded, e.g.
//    #define A B
//    STRING_NE(A)  -->  "A"
#define STRING_NE(x) #x

// 2nd level macro to stringify arguments, which are expanded, e.g.
//    #define A B
//    STRING(A)  -->  "B"
#define STRING(m) STRING_NE(m)

// Issues with generating stringified lists:
//
// The argument to STRING cannot contain commas, e.g.
//    #define CSL a1, a2
//    STRING(CSL) --> error: macro "STRING" passed 2 arguments...
//
// Grouping the arguments with (), makes them part of the string, e.g.
//    #define STRINGP(x) STRING( (x) )
//    STRINGP(CSL) --> "(a1, a2)"
//
// Workaround: stringify the elements individually, which are then
// concatenated into one string by the compiler, e.g., generate
//       "a1" ", " "a2"

// macro generating a comma-separated, stringified list
//
// usage: pass the name of a macro taking a number argument, e.g.
//    #define MY_MACRO(n) a##n
//    SCSL0(MY_MACRO)  -->
//    SCSL1(MY_MACRO)  -->  "a1"
//    SCSL2(MY_MACRO)  -->  "a1" ", " "a2"
//    ...
#define SCSL0(m)
#define SCSL1(m)                 STRING(m(1))
#define SCSL2(m)  SCSL1(m)  ", " STRING(m(2))
#define SCSL3(m)  SCSL2(m)  ", " STRING(m(3))
#define SCSL4(m)  SCSL3(m)  ", " STRING(m(4))
#define SCSL5(m)  SCSL4(m)  ", " STRING(m(5))
#define SCSL6(m)  SCSL5(m)  ", " STRING(m(6))
#define SCSL7(m)  SCSL6(m)  ", " STRING(m(7))
#define SCSL8(m)  SCSL7(m)  ", " STRING(m(8))
#define SCSL9(m)  SCSL8(m)  ", " STRING(m(9))
#define SCSL10(m) SCSL9(m)  ", " STRING(m(10))
#define SCSL11(m) SCSL10(m) ", " STRING(m(11))
#define SCSL12(m) SCSL11(m) ", " STRING(m(12))
#define SCSL13(m) SCSL12(m) ", " STRING(m(13))
#define SCSL14(m) SCSL13(m) ", " STRING(m(14))
#define SCSL15(m) SCSL14(m) ", " STRING(m(15))
#define SCSL16(m) SCSL15(m) ", " STRING(m(16))
#define SCSL17(m) SCSL16(m) ", " STRING(m(17))
#define SCSL18(m) SCSL17(m) ", " STRING(m(18))
#define SCSL19(m) SCSL18(m) ", " STRING(m(19))

// macro generating a comma-preceded, stringified list
//
// usage: pass the name of a macro taking a number argument, e.g.
//    #define MY_MACRO(n) a##n
//    SCPL0(MY_MACRO)  -->
//    SCPL1(MY_MACRO)  -->  ", " "a1"
//    SCPL2(MY_MACRO)  -->  ", " "a1" ", " "a2"
//    ...
#define SCPL0(m)
#define SCPL1(m)            ", " STRING(m(1))
#define SCPL2(m)  SCPL1(m)  ", " STRING(m(2))
#define SCPL3(m)  SCPL2(m)  ", " STRING(m(3))
#define SCPL4(m)  SCPL3(m)  ", " STRING(m(4))
#define SCPL5(m)  SCPL4(m)  ", " STRING(m(5))
#define SCPL6(m)  SCPL5(m)  ", " STRING(m(6))
#define SCPL7(m)  SCPL6(m)  ", " STRING(m(7))
#define SCPL8(m)  SCPL7(m)  ", " STRING(m(8))
#define SCPL9(m)  SCPL8(m)  ", " STRING(m(9))
#define SCPL10(m) SCPL9(m)  ", " STRING(m(10))
#define SCPL11(m) SCPL10(m) ", " STRING(m(11))
#define SCPL12(m) SCPL11(m) ", " STRING(m(12))
#define SCPL13(m) SCPL12(m) ", " STRING(m(13))
#define SCPL14(m) SCPL13(m) ", " STRING(m(14))
#define SCPL15(m) SCPL14(m) ", " STRING(m(15))
#define SCPL16(m) SCPL15(m) ", " STRING(m(16))
#define SCPL17(m) SCPL16(m) ", " STRING(m(17))
#define SCPL18(m) SCPL17(m) ", " STRING(m(18))
#define SCPL19(m) SCPL18(m) ", " STRING(m(19))

// ---------------------------------------------------------------------------
// Name Definitions used in Wrapper Function Templates
// ---------------------------------------------------------------------------

// JNI environment parameter declaration
#define JEPD JNIEnv * env

// Stringified JNI environment type
#define SJET "JNIEnv *"

// ---------------------------------------------------------------------------

// JNI Java class parameter declaration
#define JCPD jclass cls

// Stringified JNI Java class type
#define SJCT "jclass"

// ---------------------------------------------------------------------------

// Template formal result type declaration
#define TFRTD typename RT

// C formal result type
#define CFRT typename RT::CF_t

// Java formal result type
#define JFRT typename RT::JF_t

// Java actual result type
#define JART typename RT::JA_t

// C actual result type
#define CART typename RT::CA_t

// Stringified Java formal result type
#define SJFRT "RT::JF_t"

// C actual result declaration
#define CARD CART car

// Java actual result declaration
#define JARD JART jar = 0

// ---------------------------------------------------------------------------

// Template formal object type declaration
#define TFOT typename OT

// Short C formal object type (not preceded by 'typename')
#define SCFOT OT::CF_t

// Java actual object type
#define JAOT typename OT::JA_t

// C actual object type
#define CAOT typename OT::CA_t

// Java formal object parameter declaration
#define JFOPD typename OT::JF_t jfo

// Stringified Java formal object type
#define SJFOT "OT::JF_t"

// ---------------------------------------------------------------------------

// Template formal parameter type
#define TFPT(n) P##n##T

// Template formal parameter type declaration
#define TFPTD(n) typename P##n##T

// C formal parameter type
#define CFPT(n) typename P##n##T::CF_t

// Java formal parameter type
#define JFPT(n) typename P##n##T::JF_t

// Java actual parameter type
#define JAPT(n) typename P##n##T::JA_t

// C actual parameter type
#define CAPT(n) typename P##n##T::CA_t

// Java formal parameter declaration
#define JFPD(n) JFPT(n) jfp##n

// Java formal parameter
#define JFP(n) jfp##n

// C actual parameter
#define CAP(n) cap##n

// Short Java formal parameter type
#define SJFPT(n) P##n##T::JF_t

// ---------------------------------------------------------------------------

// status flag declaration
#define SFD int s = 1; (void)s;

#define PARAM_CONV_BEGIN(n)                                              \
    JAPT(n) jap##n = cast< JAPT(n), JFPT(n) >(jfp##n);                   \
    CAPT(n) cap##n = Param< JAPT(n), CAPT(n) >::convert(s, jap##n, env); \
    if (s == 0) {

#define PARAM_CONV_END(n)                                    \
    Param< JAPT(n), CAPT(n) >::release(cap##n, jap##n, env); \
    }

#define TARGET_CONV_BEGIN                                    \
    JAOT jao = cast< JAOT, TFOT::JF_t >(jfo);                \
    CAOT & cao = Target< JAOT, CAOT >::convert(s, jao, env); \
    if (s == 0) {

#define TARGET_CONV_END                             \
    Target< JAOT, CAOT >::release(cao, jao, env);   \
    }

#define RESULT_CONV                                     \
    jar = Result< JART, CART >::convert(car, env);

#define RESULT_CAST                             \
    cast< JFRT, JART >(jar);

// ---------------------------------------------------------------------------
// Data Member Access
// ---------------------------------------------------------------------------

// non-member field or static field read access
template< TFRTD,
          CFRT & D >
inline
JFRT
gget(JEPD, JCPD)
{
    TRACE(SJFRT " gget(" SJET ", " SJCT ")");
    (void)cls;
    JARD;
    CARD = D;
    RESULT_CONV;
    return RESULT_CAST;
}

// member field read access
template< TFOT,
          TFRTD,
          CFRT SCFOT::*D >
inline JFRT
gget(JEPD, JFOPD)
{
    TRACE(SJFRT " gget(" SJET ", " SJFOT ")");
    JARD;
    SFD;
    TARGET_CONV_BEGIN;
    CARD = (cao).*D;
    RESULT_CONV;
    TARGET_CONV_END;
    return RESULT_CAST;
}

// non-member field or static field write access
template< TFPTD(1),
          CFPT(1) & D >
inline
void
gset(JEPD, JCPD, JFPD(1))
{
    TRACE("void" " gset(" SJET ", " SJCT ", " STRING(SJFPT(1)) ")");
    (void)cls;
    SFD;
    PARAM_CONV_BEGIN(1);
    D = CAP(1);
    PARAM_CONV_END(1);
}

// member field write access
template< TFOT,
          TFPTD(1),
          CFPT(1) SCFOT::*D >
inline void
gset(JEPD, JFOPD CPL1(JFPD))
{
    TRACE("void" " gset(" SJET ", " SJFOT ", " STRING(SJFPT(1)) ")");
    SFD;
    TARGET_CONV_BEGIN;
    PARAM_CONV_BEGIN(1);
    (cao).*D = CAP(1);
    PARAM_CONV_END(1);
    TARGET_CONV_END;
}

// ---------------------------------------------------------------------------
// Non-Member and Static Member Function Calls, No-Return
// ---------------------------------------------------------------------------

// parameters: n = n-ary function
#define TFD_F(n)                                                        \
    template< CTL##n(TFPTD)                                             \
              void F(CSL##n(CFPT)) >                                    \
    inline void                                                         \
    gcall_fv(JEPD, JCPD CPL##n(JFPD))                                   \
    {                                                                   \
        TRACE("void" " gcall_fv(" SJET ", " SJCT SCPL##n(SJFPT) ")");   \
        (void)env; (void)cls;                                           \
        SFD;                                                            \
        BSL##n(PARAM_CONV_BEGIN);                                       \
        F( CSL##n(CAP) );                                               \
        RBSL##n(PARAM_CONV_END);                                        \
    }

// generate the function templates (separate lines for proper error messages)
TFD_F(0)
TFD_F(1)
TFD_F(2)
TFD_F(3)
TFD_F(4)
TFD_F(5)
TFD_F(6)
TFD_F(7)
TFD_F(8)
TFD_F(9)
TFD_F(10)
TFD_F(11)
TFD_F(12)
TFD_F(13)
TFD_F(14)
TFD_F(15)
TFD_F(16)
TFD_F(17)
TFD_F(18)
TFD_F(19)

// ---------------------------------------------------------------------------
// Non-Member and Static Member Function Calls, Return
// ---------------------------------------------------------------------------

// parameters: n = n-ary function
#define TFD_FR(n)                                                       \
    template< TFRTD,                                                    \
              CTL##n(TFPTD)                                             \
              CFRT F(CSL##n(CFPT)) >                                    \
    inline JFRT                                                         \
    gcall_fr(JEPD, JCPD CPL##n(JFPD))                                   \
    {                                                                   \
        TRACE(SJFRT " gcall_fr(" SJET ", " SJCT SCPL##n(SJFPT) ")");    \
        (void)cls;                                                      \
        JARD;                                                           \
        SFD;                                                            \
        BSL##n(PARAM_CONV_BEGIN);                                       \
        CARD = F( CSL##n(CAP) );                                        \
        RESULT_CONV;                                                    \
        RBSL##n(PARAM_CONV_END);                                        \
        return RESULT_CAST;                                             \
    }

// generate the function templates (separate lines help error messages)
TFD_FR(0)
TFD_FR(1)
TFD_FR(2)
TFD_FR(3)
TFD_FR(4)
TFD_FR(5)
TFD_FR(6)
TFD_FR(7)
TFD_FR(8)
TFD_FR(9)
TFD_FR(10)
TFD_FR(11)
TFD_FR(12)
TFD_FR(13)
TFD_FR(14)
TFD_FR(15)
TFD_FR(16)
TFD_FR(17)
TFD_FR(18)
TFD_FR(19)

// ---------------------------------------------------------------------------
// Non-Static Const/Non-Const Member Function Calls, No-Return
// ---------------------------------------------------------------------------

// parameters: n = n-ary function
//
// we do not generate a separate set of templates for const members anymore:
//   cm = empty or 'const'
//   #define TFD_MF(n,cm)
//          ... void (SCFOT::*F)(CSL##n(CFPT)) cm >
// for
// - leads to template ambiguities with const member function ptr type
// - empty macro arguments are undefined in ISO C90 and ISO C++98
//
#define TFD_MF(n)                                                       \
    template< TFOT,                                                     \
              CTL##n(TFPTD)                                             \
              void (SCFOT::*F)(CSL##n(CFPT)) >                          \
    inline void                                                         \
    gcall_mfv(JEPD, JFOPD CPL##n(JFPD))                                 \
    {                                                                   \
        TRACE("void" " gcall_mfv(" SJET ", " SJFOT SCPL##n(SJFPT) ")"); \
        SFD;                                                            \
        TARGET_CONV_BEGIN;                                              \
        BSL##n(PARAM_CONV_BEGIN);                                       \
        ((cao).*F)( CSL##n(CAP) );                                      \
        RBSL##n(PARAM_CONV_END);                                        \
        TARGET_CONV_END;                                                \
    }

// generate the function templates (separate lines help error messages)
TFD_MF(0)
TFD_MF(1)
TFD_MF(2)
TFD_MF(3)
TFD_MF(4)
TFD_MF(5)
TFD_MF(6)
TFD_MF(7)
TFD_MF(8)
TFD_MF(9)
TFD_MF(10)
TFD_MF(11)
TFD_MF(12)
TFD_MF(13)
TFD_MF(14)
TFD_MF(15)
TFD_MF(16)
TFD_MF(17)
TFD_MF(18)
TFD_MF(19)

// ---------------------------------------------------------------------------
// Non-Static Const/Non-Const Member Function Calls, Return
// ---------------------------------------------------------------------------

// parameters: n = n-ary, cm = const member function qualifier
//
// we do not generate a separate set of templates for const members anymore:
//   cm = empty or 'const'
//   #define TFD_MFR(n,cm)
//          ... CFRT (SCFOT::*F)(CSL##n(CFPT)) cm >
// for
// - leads to template ambiguities with const member function ptr type
// - empty macro arguments are undefined in ISO C90 and ISO C++98
//
#define TFD_MFR(n)                                                      \
    template< TFOT,                                                     \
              TFRTD,                                                    \
              CTL##n(TFPTD)                                             \
              CFRT (SCFOT::*F)(CSL##n(CFPT)) >                          \
    inline JFRT                                                         \
    gcall_mfr(JEPD, JFOPD CPL##n(JFPD))                                 \
    {                                                                   \
        TRACE(SJFRT " gcall_mfr(" SJET ", " SJFOT SCPL##n(SJFPT) ")");  \
        JARD;                                                           \
        SFD;                                                            \
        TARGET_CONV_BEGIN;                                              \
        BSL##n(PARAM_CONV_BEGIN);                                       \
        CARD = ((cao).*F)( CSL##n(CAP) );                               \
        RESULT_CONV;                                                    \
        RBSL##n(PARAM_CONV_END);                                        \
        TARGET_CONV_END;                                                \
        return RESULT_CAST;                                             \
    }

// generate the function templates (separate lines help error messages)
TFD_MFR(0)
TFD_MFR(1)
TFD_MFR(2)
TFD_MFR(3)
TFD_MFR(4)
TFD_MFR(5)
TFD_MFR(6)
TFD_MFR(7)
TFD_MFR(8)
TFD_MFR(9)
TFD_MFR(10)
TFD_MFR(11)
TFD_MFR(12)
TFD_MFR(13)
TFD_MFR(14)
TFD_MFR(15)
TFD_MFR(16)
TFD_MFR(17)
TFD_MFR(18)
TFD_MFR(19)

// ---------------------------------------------------------------------------
// Internal C++ Constructor/Destructor/Index Access Wrappers
// ---------------------------------------------------------------------------

// parameters: n = n-ary

// class template calling the array destructor
template< typename C > struct ArrayHelper;

template< typename C >
struct ArrayHelper< C * > {
    static void
    cdelete(C * p0) {
        TRACE("void ArrayHelper::cdelete(C *)");
        delete[] p0;
    }

    static C *
    ccreate(int32_t p0) {
        TRACE("C * ArrayHelper::ccreate(int32_t)");
        if (p0 < 0) throw std::bad_array_new_length();
        if constexpr (INT32_MAX > SIZE_MAX / sizeof(C))
        {
            if (uint32(p0) > SIZE_MAX / sizeof(C))
                throw std::bad_array_new_length();
        }
        // ISO C++: 'new' throws std::bad_alloc if unsuccessful
        return new C[p0];
    }

    static C *
    cat(C * p0, int32_t i) {
        TRACE("C * ArrayHelper::cat(C *)");
        return (p0 + i);
    }
};

template< typename C >
struct ArrayHelper< C & > {
    static void
    cdelete(C & p0) {
        TRACE("void ArrayHelper::cdelete(C &)");
        ArrayHelper< C * >::cdelete(&p0);
    }

    static C &
    ccreate(int32_t p0) {
        TRACE("C & ArrayHelper::ccreate(int32_t)");
        return *ArrayHelper< C * >::ccreate(p0);
    }

    static C &
    cat(C & p0, int32_t i) {
        TRACE("C & ArrayHelper::cat(C &)");
        return *ArrayHelper< C * >::cat(&p0, i);
    }
};

// ---------------------------------------------------------------------------

// class template calling the destructor
template< typename C > struct Destructor;

template< typename C >
struct Destructor< C * > {
    static void
    cdelete(C * p0) {
        TRACE("void Destructor::cdelete(C *)");
        delete p0;
    }
};

template< typename C >
struct Destructor< C & > {
    static void
    cdelete(C & p0) {
        TRACE("void Destructor::cdelete(C &)");
        Destructor< C * >::cdelete(&p0);
    }
};

// Template formal parameter type (redefine)
#define CC_TFPT(n) P##n##_CF_t

// Template formal parameter type declaration (redefine)
#define CC_TFPTD(n) typename CC_TFPT(n)

// C formal parameter type (redefine)
#define CC_CFPT(n) CC_TFPT(n)

// C formal parameter
#define CC_CFP(n) cfp##n

// C formal parameter declaration
#define CC_CFPD(n) CC_CFPT(n) CC_CFP(n)

// n-ary class templates calling constructors
#define TFD_CC(n)                                                       \
    template< typename C CPL##n(CC_TFPTD) > struct Constructor##n;      \
                                                                        \
    template< typename C CPL##n(CC_TFPTD) >                             \
    struct Constructor##n< C * CPL##n(CC_TFPT) > {                      \
        static C *                                                      \
        ccreate(CSL##n(CC_CFPD)) {                                      \
            TRACE("C * ccreate(" SCSL##n(CC_TFPT) ")");                 \
            return new C(CSL##n(CC_CFP));                               \
        }                                                               \
    };                                                                  \
                                                                        \
    template< typename C CPL##n(CC_TFPTD) >                             \
    struct Constructor##n< C & CPL##n(CC_TFPT) > {                      \
        static C &                                                      \
        ccreate(CSL##n(CC_CFPD)) {                                      \
            TRACE("C & ccreate(" SCSL##n(CC_TFPT) ")");                 \
            return *Constructor##n< C * CPL##n(CC_TFPT) >               \
                ::ccreate(CSL##n(CC_CFP));                              \
        }                                                               \
    };

// generate the class templates (separate lines help error messages)
TFD_CC(0)
TFD_CC(1)
TFD_CC(2)
TFD_CC(3)
TFD_CC(4)
TFD_CC(5)
TFD_CC(6)
TFD_CC(7)
TFD_CC(8)
TFD_CC(9)
TFD_CC(10)
TFD_CC(11)
TFD_CC(12)
TFD_CC(13)
TFD_CC(14)
TFD_CC(15)
TFD_CC(16)
TFD_CC(17)
TFD_CC(18)
TFD_CC(19)

// ---------------------------------------------------------------------------
// Constructor, Destructor, and Index Access Calls
// ---------------------------------------------------------------------------

// array delete template function definition
template< TFPTD(1) >
inline
void
gdeleteArray(JEPD, JCPD, JFPD(1))
{
    TRACE("void gdeleteArray(" SJET ", " SJCT ", " STRING(SJFPT(1)) ")");
    (void)cls;
    // not using gcall_fv<...>(...) due to call to detachWrapper()
    SFD;
    PARAM_CONV_BEGIN(1);
#ifdef JTIE_OBJECT_CLEAR_ADDRESS_UPON_DELETE
    detachWrapper(jap1, env);
#endif // JTIE_OBJECT_CLEAR_ADDRESS_UPON_DELETE
    ArrayHelper< CFPT(1) >::cdelete(CAP(1));
    PARAM_CONV_END(1);
}

// array create template function definition
template< TFRTD, TFPTD(1) >
inline
JFRT
gcreateArray(JEPD, JCPD, JFPD(1))
{
    TRACE(SJFRT " gcreateArray(" SJET ", " SJCT ", " STRING(SJFPT(1)) ")");
    return gcall_fr< RT, TFPT(1),
        &ArrayHelper< CFRT >::ccreate
        >(env, cls, JFP(1));
}

// array index access template function definition
template< TFRTD, TFPTD(1), TFPTD(2) >
inline
JFRT
gat(JEPD, JCPD, JFPD(1), JFPD(2))
{
    TRACE(SJFRT " gat(" SJET ", " SJCT ", " STRING(SJFPT(1)) ", " STRING(SJFPT(2)) ")");
    return gcall_fr< RT, TFPT(1), TFPT(2),
        &ArrayHelper< CFRT >::cat
        >(env, cls, JFP(1), JFP(2));
}

// ---------------------------------------------------------------------------

// destructor template function definition
template< TFPTD(1) >
inline
void
gdelete(JEPD, JCPD, JFPD(1))
{
    TRACE("void gdelete(" SJET ", " SJCT ", " STRING(SJFPT(1)) ")");
    (void)cls;
    // not using gcall_fv<...>(...) due to call to detachWrapper()
    SFD;
    PARAM_CONV_BEGIN(1);
#ifdef JTIE_OBJECT_CLEAR_ADDRESS_UPON_DELETE
    detachWrapper(jap1, env);
#endif // JTIE_OBJECT_CLEAR_ADDRESS_UPON_DELETE
    Destructor< CFPT(1) >::cdelete(CAP(1));
    PARAM_CONV_END(1);
}

// n-ary constructor template function definition
#define TFD_C(n)                                                        \
    template< TFRTD CPL##n(TFPTD) >                                     \
    inline JFRT                                                         \
    gcreate(JEPD, JCPD CPL##n(JFPD))                                    \
    {                                                                   \
        TRACE(SJFRT " gcreate(" SJET ", " SJCT SCSL##n(SJFPT) ")");     \
        return gcall_fr< RT, CTL##n(TFPT)                               \
            &Constructor##n< CFRT CPL##n(CFPT) >::ccreate               \
            >(env, cls CPL##n(JFP));                                    \
    }

// generate the function templates (separate lines help error messages)
TFD_C(0)
TFD_C(1)
TFD_C(2)
TFD_C(3)
TFD_C(4)
TFD_C(5)
TFD_C(6)
TFD_C(7)
TFD_C(8)
TFD_C(9)
TFD_C(10)
TFD_C(11)
TFD_C(12)
TFD_C(13)
TFD_C(14)
TFD_C(15)
TFD_C(16)
TFD_C(17)
TFD_C(18)
TFD_C(19)

// ---------------------------------------------------------------------------

#endif // jtie_gcalls_hpp
