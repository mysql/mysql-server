/*
 Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

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
 * myapi_wrapper.hpp
 */

#ifndef myapi_wrapper_hpp
#define myapi_wrapper_hpp

// API to wrap
#include "myapi.hpp"

struct MyApiWrapper {

// ---------------------------------------------------------------------------

    static int32_t
    B0__f0s( )
    {
        return B0::f0s();
    }

    static int32_t
    B0__f0n( const B0 & obj )
    {
        return obj.f0n();
    }

    static int32_t
    B0__f0v( const B0 & obj )
    {
        return obj.f0v();
    }

// ---------------------------------------------------------------------------

    static int32_t
    B1__f0s( )
    {
        return B1::f0s();
    }

    static int32_t
    B1__f0n( const B1 & obj )
    {
        return obj.f0n();
    }

    static int32_t
    B1__f0v( const B1 & obj )
    {
        return obj.f0v();
    }

// ---------------------------------------------------------------------------

    static A *
    A__deliver_ptr( )
    {
        return A::deliver_ptr();
    }

    static A *
    A__deliver_null_ptr( )
    {
        return A::deliver_null_ptr();
    }

    static A &
    A__deliver_ref( )
    {
        return A::deliver_ref();
    }

    static A &
    A__deliver_null_ref( )
    {
        return A::deliver_null_ref();
    }

    static void
    A__take_ptr( A * o )
    {
        A::take_ptr(o);
    }

    static void
    A__take_null_ptr( A * o )
    {
        A::take_null_ptr(o);
    }

    static void
    A__take_ref( A & o )
    {
        A::take_ref(o);
    }

    static void
    A__take_null_ref( A & o )
    {
        A::take_null_ref(o);
    }

    static void
    A__print( A * p0 )
    {
        A::print(p0);
    }

    static B0 *
    A__newB0( const A & obj )
    {
        return obj.newB0();
    }

    static B1 *
    A__newB1( const A & obj )
    {
        return obj.newB1();
    }

    static int32_t
    A__f0s( )
    {
        return A::f0s();
    }

    static int32_t
    A__f0n( const A & obj )
    {
        return obj.f0n();
    }

    static int32_t
    A__f0v( const A & obj )
    {
        return obj.f0v();
    }

    static void
    A__del( A & obj, B0 & b )
    {
        obj.del(b);
    }

    static void
    A__del( A & obj, B1 & b )
    {
        obj.del(b);
    }

    static void
    A__g0c( const A & obj )
    {
        obj.g0c();
    }

    static void
    A__g1c( const A & obj, int8_t p0 )
    {
        obj.g1c(p0);
    }

    static void
    A__g2c( const A & obj, int8_t p0, int16_t p1 )
    {
        obj.g2c(p0, p1);
    }

    static void
    A__g3c( const A & obj, int8_t p0, int16_t p1, int32_t p2 )
    {
        obj.g3c(p0, p1, p2);
    }

    static void
    A__g0( A & obj )
    {
        obj.g0();
    }

    static void
    A__g1( A & obj, int8_t p0 )
    {
        obj.g1(p0);
    }

    static void
    A__g2( A & obj, int8_t p0, int16_t p1 )
    {
        obj.g2(p0, p1);
    }

    static void
    A__g3( A & obj, int8_t p0, int16_t p1, int32_t p2 )
    {
        obj.g3(p0, p1, p2);
    }

    static int32_t
    A__g0rc( const A & obj )
    {
        return obj.g0rc();
    }

    static int32_t
    A__g1rc( const A & obj, int8_t p0 )
    {
        return obj.g1rc(p0);
    }

    static int32_t
    A__g2rc( const A & obj, int8_t p0, int16_t p1 )
    {
        return obj.g2rc(p0, p1);
    }

    static int32_t
    A__g3rc( const A & obj, int8_t p0, int16_t p1, int32_t p2 )
    {
        return obj.g3rc(p0, p1, p2);
    }

    static int32_t
    A__g0r( A & obj )
    {
        return obj.g0r();
    }

    static int32_t
    A__g1r( A & obj, int8_t p0 )
    {
        return obj.g1r(p0);
    }

    static int32_t
    A__g2r( A & obj, int8_t p0, int16_t p1 )
    {
        return obj.g2r(p0, p1);
    }

    static int32_t
    A__g3r( A & obj, int8_t p0, int16_t p1, int32_t p2 )
    {
        return obj.g3r(p0, p1, p2);
    }

// ----------------------------------------------------------------------

    static void
    h0( )
    {
        ::h0();
    }

    static void
    h1( int8_t p0 )
    {
        ::h1(p0);
    }

    static void
    h2( int8_t p0, int16_t p1 )
    {
        ::h2(p0, p1);
    }

    static void
    h3( int8_t p0, int16_t p1, int32_t p2 )
    {
        ::h3(p0, p1, p2);
    }

    static int32_t
    h0r( )
    {
        return ::h0r();
    }

    static int32_t
    h1r( int8_t p0 )
    {
        return ::h1r(p0);
    }

    static int32_t
    h2r( int8_t p0, int16_t p1 )
    {
        return ::h2r(p0, p1);
    }

    static int32_t
    h3r( int8_t p0, int16_t p1, int32_t p2 )
    {
        return ::h3r(p0, p1, p2);
    }

// ---------------------------------------------------------------------------

    static C0 *
    C0__pass__0( C0 * c0 ) // disambiguate overloaded function for MSVC
    {
        return C0::pass(c0);
    }

    static const C0 *
    C0__pass__1( const C0 * c0 ) // disambiguate overloaded function for MSVC
    {
        return C0::pass(c0);
    }

    static int64_t
    C0__hash( const C0 * c0, int32_t n )
    {
        return C0::hash(c0, n);
    }

    static void
    C0__check( const C0 & obj, int64_t p0 )
    {
        obj.check(p0);
    }

    static void
    C0__print( const C0 & obj )
    {
        obj.print();
    }

    static const C0 *
    C0__deliver_C0Cp( const C0 & obj )
    {
        return obj.deliver_C0Cp();
    }

    static const C0 &
    C0__deliver_C0Cr( const C0 & obj )
    {
        return obj.deliver_C0Cr();
    }

    static void
    C0__take_C0Cp( const C0 & obj, const C0 * cp )
    {
        obj.take_C0Cp(cp);
    }

    static void
    C0__take_C0Cr( const C0 & obj, const C0 & cp )
    {
        obj.take_C0Cr(cp);
    }

    static C0 *
    C0__deliver_C0p( C0 & obj )
    {
        return obj.deliver_C0p();
    }

    static C0 &
    C0__deliver_C0r( C0 & obj )
    {
        return obj.deliver_C0r();
    }

    static void
    C0__take_C0p( C0 & obj, C0 * p )
    {
        obj.take_C0p(p);
    }

    static void
    C0__take_C0r( C0 & obj, C0 & p )
    {
        obj.take_C0r(p);
    }

// ---------------------------------------------------------------------------

    static C1 *
    C1__pass( C1 * c1 )
    {
        return C1::pass(c1);
    }

    static const C1 *
    C1__pass( const C1 * c1 )
    {
        return C1::pass(c1);
    }

    static int64_t
    C1__hash( const C1 * c1, int32_t n )
    {
        return C1::hash(c1, n);
    }

    static const C1 *
    C1__deliver_C1Cp( const C1 & obj )
    {
        return obj.deliver_C1Cp();
    }

    static const C1 &
    C1__deliver_C1Cr( const C1 & obj )
    {
        return obj.deliver_C1Cr();
    }

    static void
    C1__take_C1Cp( const C1 & obj, const C1 * cp )
    {
        obj.take_C1Cp(cp);
    }

    static void
    C1__take_C1Cr( const C1 & obj, const C1 & cp )
    {
        obj.take_C1Cr(cp);
    }

    static C1 *
    C1__deliver_C1p( C1 & obj )
    {
        return obj.deliver_C1p();
    }

    static C1 &
    C1__deliver_C1r( C1 & obj )
    {
        return obj.deliver_C1r();
    }

    static void
    C1__take_C1p( C1 & obj, C1 * p )
    {
        obj.take_C1p(p);
    }

    static void
    C1__take_C1r( C1 & obj, C1 & p )
    {
        obj.take_C1r(p);
    }

// ---------------------------------------------------------------------------

    static int
    D0__f_d0( D0 & obj )
    {
        return obj.f_d0();
    }

    static int
    D0__f_nv( D0 & obj )
    {
        return obj.f_nv();
    }

    static int
    D0__f_v( D0 & obj )
    {
        return obj.f_v();
    }

    static D1 *
    D0__sub( )
    {
        return D0::sub();
    }

// ---------------------------------------------------------------------------

    static int
    D1__f_d1( D1 & obj )
    {
        return obj.f_d1();
    }

    static int
    D1__f_nv( D1 & obj )
    {
        return obj.f_nv();
    }

    static int
    D1__f_v( D1 & obj )
    {
        return obj.f_v();
    }

    static D1 *
    D1__sub( )
    {
        return D1::sub();
    }

// ---------------------------------------------------------------------------

    static int
    D2__f_d2( D2 & obj )
    {
        return obj.f_d2();
    }

    static int
    D2__f_nv( D2 & obj )
    {
        return obj.f_nv();
    }

    static int
    D2__f_v( D2 & obj )
    {
        return obj.f_v();
    }

    static D1 *
    D2__sub( )
    {
        return D2::sub();
    }

// ---------------------------------------------------------------------------

    static E::EE
    E__deliver_EE1( )
    {
        return E::deliver_EE1();
    }

    static void
    E__take_EE1( E::EE e )
    {
        E::take_EE1(e);
    }

    static const E::EE
    E__deliver_EE1c( )
    {
        return E::deliver_EE1c();
    }

    static void
    E__take_EE1c( const E::EE e )
    {
        E::take_EE1c(e);
    }
};

#endif // myapi_wrapper_hpp
