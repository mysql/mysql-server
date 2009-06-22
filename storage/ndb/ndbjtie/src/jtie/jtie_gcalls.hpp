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
 * jtie_gcalls.hpp
 */

#ifndef jtie_gcalls_hpp
#define jtie_gcalls_hpp

//#include <cstring>
//#include "helpers.hpp"
#include "jtie_ttrait.hpp"
#include "jtie_tconv_def.hpp"
#include "jtie_tconv_cobject.hpp"

// ---------------------------------------------------------------------------
// infrastructure code: generic wrapper function definitions
// ---------------------------------------------------------------------------

// XXX consider replacing or defaulting 'yyy::CA_t' to 'yyy::CA_t &'

// XXX we must not cast to the C formal type to avoid copy constructing
// cast< typename OT::CF_t, typename OT::CA_t >(cao)
// cast< typename P0T::CF_t, typename P0T::CA_t >(cap0)
// cast< typename P1T::CF_t, typename P1T::CA_t >(cap1)
// cast< typename P2T::CF_t, typename P2T::CA_t >(cap2)
// ...

// ---------------------------------------------------------------------------
// Non-Member Function Calls, No-Return
// ---------------------------------------------------------------------------

template< void F() >
inline
void
gcall(JNIEnv * env)
{
    TRACE("void gcall(JNIEnv *)");
    F();
}

template< typename P0T,
          void F(typename P0T::CF_t) >
inline
void
gcall(JNIEnv * env, typename P0T::JF_t jfp0)
{
    TRACE("void gcall(JNIEnv *, P0T::JF_t)");
    int s;
    typename P0T::JA_t jap0 = cast< typename P0T::JA_t, typename P0T::JF_t >(jfp0);
    typename P0T::CA_t cap0 = Param< typename P0T::JA_t, typename P0T::CA_t >::convert(s, jap0, env);
    if (s == 0) {
        F(
            (cap0)
            );
        Param< typename P0T::JA_t, typename P0T::CA_t >::release(cap0, jap0, env);
    }
}

template< typename P0T,
          typename P1T,
          void F(typename P0T::CF_t,typename P1T::CF_t) >
inline
void
gcall(JNIEnv * env, typename P0T::JF_t jfp0, typename P1T::JF_t jfp1)
{
    TRACE("void gcall(JNIEnv *, P0T::JF_t, P1T::JF_t)");
    int s;
    typename P0T::JA_t jap0 = cast< typename P0T::JA_t, typename P0T::JF_t >(jfp0);
    typename P0T::CA_t cap0 = Param< typename P0T::JA_t, typename P0T::CA_t >::convert(s, jap0, env);
    if (s == 0) {
        typename P1T::JA_t jap1 = cast< typename P1T::JA_t, typename P1T::JF_t >(jfp1);
        typename P1T::CA_t cap1 = Param< typename P1T::JA_t, typename P1T::CA_t >::convert(s, jap1, env);
        if (s == 0) {
            F(
                (cap0),
                (cap1)
                );
            Param< typename P1T::JA_t, typename P1T::CA_t >::release(cap1, jap1, env);
        }
        Param< typename P0T::JA_t, typename P0T::CA_t >::release(cap0, jap0, env);
    }
}


template< typename P0T,
          typename P1T,
          typename P2T,
          void F(typename P0T::CF_t,typename P1T::CF_t,typename P2T::CF_t) >
inline
void
gcall(JNIEnv * env, typename P0T::JF_t jfp0, typename P1T::JF_t jfp1, typename P2T::JF_t jfp2)
{
    TRACE("void gcall(JNIEnv *, P0T::JF_t, P1T::JF_t, P2T::JF_t)");
    int s;
    typename P0T::JA_t jap0 = cast< typename P0T::JA_t, typename P0T::JF_t >(jfp0);
    typename P0T::CA_t cap0 = Param< typename P0T::JA_t, typename P0T::CA_t >::convert(s, jap0, env);
    if (s == 0) {
        typename P1T::JA_t jap1 = cast< typename P1T::JA_t, typename P1T::JF_t >(jfp1);
        typename P1T::CA_t cap1 = Param< typename P1T::JA_t, typename P1T::CA_t >::convert(s, jap1, env);
        if (s == 0) {
            typename P2T::JA_t jap2 = cast< typename P2T::JA_t, typename P2T::JF_t >(jfp2);
            typename P2T::CA_t cap2 = Param< typename P2T::JA_t, typename P2T::CA_t >::convert(s, jap2, env);
            if (s == 0) {
                F(
                    (cap0),
                    (cap1),
                    (cap2)
                );
                Param< typename P2T::JA_t, typename P2T::CA_t >::release(cap2, jap2, env);
            }
            Param< typename P1T::JA_t, typename P1T::CA_t >::release(cap1, jap1, env);
        }
        Param< typename P0T::JA_t, typename P0T::CA_t >::release(cap0, jap0, env);
    }
}

// ---------------------------------------------------------------------------
// Non-Member Function Calls, Return
// ---------------------------------------------------------------------------

template< typename RT,
          typename RT::CF_t F() >
inline
typename RT::JF_t
gcall(JNIEnv * env)
{
    TRACE("RT::JF_t gcall(JNIEnv *)");
    typename RT::JA_t jar = 0; // init return value to error
    typename RT::CA_t car = cast< typename RT::CA_t, typename RT::CF_t >(
        F());
    jar = Result< typename RT::JA_t, typename RT::CA_t >::convert(car, env);
    return cast< typename RT::JF_t, typename RT::JA_t >(jar);
}

template< typename RT,
          typename P0T,
          typename RT::CF_t F(typename P0T::CF_t) >
inline
typename RT::JF_t
gcall(JNIEnv * env, typename P0T::JF_t jfp0)
{
    TRACE("RT::JF_t gcall(JNIEnv *, P0T::JF_t)");
    typename RT::JA_t jar = 0; // init return value to error
    int s;
    typename P0T::JA_t jap0 = cast< typename P0T::JA_t, typename P0T::JF_t >(jfp0);
    typename P0T::CA_t cap0 = Param< typename P0T::JA_t, typename P0T::CA_t >::convert(s, jap0, env);
    if (s == 0) {
        typename RT::CA_t car = cast< typename RT::CA_t, typename RT::CF_t >(
            F(
                (cap0)
                ));
        jar = Result< typename RT::JA_t, typename RT::CA_t >::convert(car, env);
        Param< typename P0T::JA_t, typename P0T::CA_t >::release(cap0, jap0, env);
    }
    return cast< typename RT::JF_t, typename RT::JA_t >(jar);
}

template< typename RT,
          typename P0T,
          typename P1T,
          typename RT::CF_t F(typename P0T::CF_t,typename P1T::CF_t) >
inline
typename RT::JF_t
gcall(JNIEnv * env, typename P0T::JF_t jfp0, typename P1T::JF_t jfp1)
{
    TRACE("RT::JF_t gcall(JNIEnv *, P0T::JF_t, P1T::JF_t)");
    typename RT::JA_t jar = 0; // init return value to error
    int s;
    typename P0T::JA_t jap0 = cast< typename P0T::JA_t, typename P0T::JF_t >(jfp0);
    typename P0T::CA_t cap0 = Param< typename P0T::JA_t, typename P0T::CA_t >::convert(s, jap0, env);
    if (s == 0) {
        typename P1T::JA_t jap1 = cast< typename P1T::JA_t, typename P1T::JF_t >(jfp1);
        typename P1T::CA_t cap1 = Param< typename P1T::JA_t, typename P1T::CA_t >::convert(s, jap1, env);
        if (s == 0) {
            typename RT::CA_t car = cast< typename RT::CA_t, typename RT::CF_t >(
                F(
                    (cap0),
                    (cap1)
                    ));
            jar = Result< typename RT::JA_t, typename RT::CA_t >::convert(car, env);
            Param< typename P1T::JA_t, typename P1T::CA_t >::release(cap1, jap1, env);
        }
        Param< typename P0T::JA_t, typename P0T::CA_t >::release(cap0, jap0, env);
    }
    return cast< typename RT::JF_t, typename RT::JA_t >(jar);
}

template< typename RT,
          typename P0T,
          typename P1T,
          typename P2T,
          typename RT::CF_t F(typename P0T::CF_t,typename P1T::CF_t,typename P2T::CF_t) >
inline
typename RT::JF_t
gcall(JNIEnv * env, typename P0T::JF_t jfp0, typename P1T::JF_t jfp1, typename P2T::JF_t jfp2)
{
    TRACE("RT::JF_t gcall(JNIEnv *, P0T::JF_t, P1T::JF_t, P2T::JF_t)");
    typename RT::JA_t jar = 0; // init return value to error
    int s;
    typename P0T::JA_t jap0 = cast< typename P0T::JA_t, typename P0T::JF_t >(jfp0);
    typename P0T::CA_t cap0 = Param< typename P0T::JA_t, typename P0T::CA_t >::convert(s, jap0, env);
    if (s == 0) {
        typename P1T::JA_t jap1 = cast< typename P1T::JA_t, typename P1T::JF_t >(jfp1);
        typename P1T::CA_t cap1 = Param< typename P1T::JA_t, typename P1T::CA_t >::convert(s, jap1, env);
        if (s == 0) {
            typename P2T::JA_t jap2 = cast< typename P2T::JA_t, typename P2T::JF_t >(jfp2);
            typename P2T::CA_t cap2 = Param< typename P2T::JA_t, typename P2T::CA_t >::convert(s, jap2, env);
            if (s == 0) {
                typename RT::CA_t car = cast< typename RT::CA_t, typename RT::CF_t >(
                    F(
                        (cap0),
                        (cap1),
                        (cap2)
                        ));
                jar = Result< typename RT::JA_t, typename RT::CA_t >::convert(car, env);
                Param< typename P2T::JA_t, typename P2T::CA_t >::release(cap2, jap2, env);
            }
            Param< typename P1T::JA_t, typename P1T::CA_t >::release(cap1, jap1, env);
        }
        Param< typename P0T::JA_t, typename P0T::CA_t >::release(cap0, jap0, env);
    }
    return cast< typename RT::JF_t, typename RT::JA_t >(jar);
}

// ---------------------------------------------------------------------------
// Member Functions
// ---------------------------------------------------------------------------

// XXX depending upon how target object is hold (by reference or pointer):
// pointer-to-function-member applied on object:
//    ((cao).*F)()),
// pointer-to-function-member applied on pointer-to-object:
//    ((cao)->*F)()),

// XXX also, check if we can drop the casting of the target object:
//    jar = (cao.*F)();

// XXX need to duplicate for const/non-const member function arguments:
//          typename RT::CF_t (OT::CF_t::*F)() >
//    ...
//          typename RT::CF_t (OT::CF_t::*F)() const >

// ---------------------------------------------------------------------------
// Member Function Calls, Const, No-Return
// ---------------------------------------------------------------------------

template< typename OT,
          void (OT::CF_t::*F)() const >
inline
void
gcall(JNIEnv * env, typename OT::JF_t jfo)
{
    TRACE("void gcall(JNIEnv *, OT::JF_t)");
    int s;
    typename OT::JA_t jao = cast< typename OT::JA_t, typename OT::JF_t >(jfo);
    typename OT::CA_t cao = Param< typename OT::JA_t, typename OT::CA_t >::convert(s, jao, env);
    if (s == 0) {
        ((cao).*F)(
            );
        Param< typename OT::JA_t, typename OT::CA_t >::release(cao, jao, env);
    }
}

template< typename OT,
          typename P0T,
          void (OT::CF_t::*F)(typename P0T::CF_t) const >
inline
void
gcall(JNIEnv * env, typename OT::JF_t jfo, typename P0T::JF_t jfp0)
{
    TRACE("void gcall(JNIEnv *, OT::JF_t, P0T::JF_t)");
    int s;
    typename OT::JA_t jao = cast< typename OT::JA_t, typename OT::JF_t >(jfo);
    typename OT::CA_t cao = Param< typename OT::JA_t, typename OT::CA_t >::convert(s, jao, env);
    if (s == 0) {
        typename P0T::JA_t jap0 = cast< typename P0T::JA_t, typename P0T::JF_t >(jfp0);
        typename P0T::CA_t cap0 = Param< typename P0T::JA_t, typename P0T::CA_t >::convert(s, jap0, env);
        if (s == 0) {
            ((cao).*F)(
                (cap0)
                );
        Param< typename P0T::JA_t, typename P0T::CA_t >::release(cap0, jap0, env);
        }
        Param< typename OT::JA_t, typename OT::CA_t >::release(cao, jao, env);
    }
}

template< typename OT,
          typename P0T,
          typename P1T,
          void (OT::CF_t::*F)(typename P0T::CF_t,typename P1T::CF_t) const >
inline
void
gcall(JNIEnv * env, typename OT::JF_t jfo, typename P0T::JF_t jfp0, typename P1T::JF_t jfp1)
{
    TRACE("void gcall(JNIEnv *, OT::JF_t, P0T::JF_t, P1T::JF_t)");
    int s;
    typename OT::JA_t jao = cast< typename OT::JA_t, typename OT::JF_t >(jfo);
    typename OT::CA_t cao = Param< typename OT::JA_t, typename OT::CA_t >::convert(s, jao, env);
    if (s == 0) {
        typename P0T::JA_t jap0 = cast< typename P0T::JA_t, typename P0T::JF_t >(jfp0);
        typename P0T::CA_t cap0 = Param< typename P0T::JA_t, typename P0T::CA_t >::convert(s, jap0, env);
        if (s == 0) {
            typename P1T::JA_t jap1 = cast< typename P1T::JA_t, typename P1T::JF_t >(jfp1);
            typename P1T::CA_t cap1 = Param< typename P1T::JA_t, typename P1T::CA_t >::convert(s, jap1, env);
            if (s == 0) {
                ((cao).*F)(
                    (cap0),
                    (cap1)
                    );
                Param< typename P1T::JA_t, typename P1T::CA_t >::release(cap1, jap1, env);
            }
            Param< typename P0T::JA_t, typename P0T::CA_t >::release(cap0, jap0, env);
        }
        Param< typename OT::JA_t, typename OT::CA_t >::release(cao, jao, env);
    }
}

template< typename OT,
          typename P0T,
          typename P1T,
          typename P2T,
          void (OT::CF_t::*F)(typename P0T::CF_t,typename P1T::CF_t,typename P2T::CF_t) const >
inline
void
gcall(JNIEnv * env, typename OT::JF_t jfo, typename P0T::JF_t jfp0, typename P1T::JF_t jfp1, typename P2T::JF_t jfp2)
{
    TRACE("void gcall(JNIEnv *, OT::JF_t, P0T::JF_t, P2T::JF_t, P2T::JF_t)");
    int s;
    typename OT::JA_t jao = cast< typename OT::JA_t, typename OT::JF_t >(jfo);
    typename OT::CA_t cao = Param< typename OT::JA_t, typename OT::CA_t >::convert(s, jao, env);
    if (s == 0) {
        typename P0T::JA_t jap0 = cast< typename P0T::JA_t, typename P0T::JF_t >(jfp0);
        typename P0T::CA_t cap0 = Param< typename P0T::JA_t, typename P0T::CA_t >::convert(s, jap0, env);
        if (s == 0) {
            typename P1T::JA_t jap1 = cast< typename P1T::JA_t, typename P1T::JF_t >(jfp1);
            typename P1T::CA_t cap1 = Param< typename P1T::JA_t, typename P1T::CA_t >::convert(s, jap1, env);
            if (s == 0) {
                typename P2T::JA_t jap2 = cast< typename P2T::JA_t, typename P2T::JF_t >(jfp2);
                typename P2T::CA_t cap2 = Param< typename P2T::JA_t, typename P2T::CA_t >::convert(s, jap2, env);
                if (s == 0) {
                    ((cao).*F)(
                        (cap0),
                        (cap1),
                        (cap2)
                        );
                    Param< typename P2T::JA_t, typename P2T::CA_t >::release(cap2, jap2, env);
                }
                Param< typename P1T::JA_t, typename P1T::CA_t >::release(cap1, jap1, env);
            }
            Param< typename P0T::JA_t, typename P0T::CA_t >::release(cap0, jap0, env);
        }
        Param< typename OT::JA_t, typename OT::CA_t >::release(cao, jao, env);
    }
}

// ---------------------------------------------------------------------------
// Member Function Calls, Non-Const, No-Return
// ---------------------------------------------------------------------------

template< typename OT,
          void (OT::CF_t::*F)() >
inline
void
gcall(JNIEnv * env, typename OT::JF_t jfo)
{
    TRACE("void gcall(JNIEnv *, OT::JF_t)");
    int s;
    typename OT::JA_t jao = cast< typename OT::JA_t, typename OT::JF_t >(jfo);
    typename OT::CA_t cao = Param< typename OT::JA_t, typename OT::CA_t >::convert(s, jao, env);
    if (s == 0) {
        ((cao).*F)(
            );
        Param< typename OT::JA_t, typename OT::CA_t >::release(cao, jao, env);
    }
}

template< typename OT,
          typename P0T,
          void (OT::CF_t::*F)(typename P0T::CF_t) >
inline
void
gcall(JNIEnv * env, typename OT::JF_t jfo, typename P0T::JF_t jfp0)
{
    TRACE("void gcall(JNIEnv *, OT::JF_t, P0T::JF_t)");
    int s;
    typename OT::JA_t jao = cast< typename OT::JA_t, typename OT::JF_t >(jfo);
    typename OT::CA_t cao = Param< typename OT::JA_t, typename OT::CA_t >::convert(s, jao, env);
    if (s == 0) {
        typename P0T::JA_t jap0 = cast< typename P0T::JA_t, typename P0T::JF_t >(jfp0);
        typename P0T::CA_t cap0 = Param< typename P0T::JA_t, typename P0T::CA_t >::convert(s, jap0, env);
        if (s == 0) {
            ((cao).*F)(
                (cap0)
                );
            Param< typename P0T::JA_t, typename P0T::CA_t >::release(cap0, jap0, env);
        }
        Param< typename OT::JA_t, typename OT::CA_t >::release(cao, jao, env);
    }
}

template< typename OT,
          typename P0T,
          typename P1T,
          void (OT::CF_t::*F)(typename P0T::CF_t,typename P1T::CF_t) >
inline
void
gcall(JNIEnv * env, typename OT::JF_t jfo, typename P0T::JF_t jfp0, typename P1T::JF_t jfp1)
{
    TRACE("void gcall(JNIEnv *, OT::JF_t, P0T::JF_t, P1T::JF_t)");
    int s;
    typename OT::JA_t jao = cast< typename OT::JA_t, typename OT::JF_t >(jfo);
    typename OT::CA_t cao = Param< typename OT::JA_t, typename OT::CA_t >::convert(s, jao, env);
    if (s == 0) {
        typename P0T::JA_t jap0 = cast< typename P0T::JA_t, typename P0T::JF_t >(jfp0);
        typename P0T::CA_t cap0 = Param< typename P0T::JA_t, typename P0T::CA_t >::convert(s, jap0, env);
        if (s == 0) {
            typename P1T::JA_t jap1 = cast< typename P1T::JA_t, typename P1T::JF_t >(jfp1);
            typename P1T::CA_t cap1 = Param< typename P1T::JA_t, typename P1T::CA_t >::convert(s, jap1, env);
            if (s == 0) {
                ((cao).*F)(
                    (cap0),
                    (cap1)
                    );
                Param< typename P1T::JA_t, typename P1T::CA_t >::release(cap1, jap1, env);
            }
            Param< typename P0T::JA_t, typename P0T::CA_t >::release(cap0, jap0, env);
        }
        Param< typename OT::JA_t, typename OT::CA_t >::release(cao, jao, env);
    }
}

template< typename OT,
          typename P0T,
          typename P1T,
          typename P2T,
          void (OT::CF_t::*F)(typename P0T::CF_t,typename P1T::CF_t,typename P2T::CF_t) >
inline
void
gcall(JNIEnv * env, typename OT::JF_t jfo, typename P0T::JF_t jfp0, typename P1T::JF_t jfp1, typename P2T::JF_t jfp2)
{
    TRACE("void gcall(JNIEnv *, OT::JF_t, P0T::JF_t, P2T::JF_t, P2T::JF_t)");
    int s;
    typename OT::JA_t jao = cast< typename OT::JA_t, typename OT::JF_t >(jfo);
    typename OT::CA_t cao = Param< typename OT::JA_t, typename OT::CA_t >::convert(s, jao, env);
    if (s == 0) {
        typename P0T::JA_t jap0 = cast< typename P0T::JA_t, typename P0T::JF_t >(jfp0);
        typename P0T::CA_t cap0 = Param< typename P0T::JA_t, typename P0T::CA_t >::convert(s, jap0, env);
        if (s == 0) {
            typename P1T::JA_t jap1 = cast< typename P1T::JA_t, typename P1T::JF_t >(jfp1);
            typename P1T::CA_t cap1 = Param< typename P1T::JA_t, typename P1T::CA_t >::convert(s, jap1, env);
            if (s == 0) {
                typename P2T::JA_t jap2 = cast< typename P2T::JA_t, typename P2T::JF_t >(jfp2);
                typename P2T::CA_t cap2 = Param< typename P2T::JA_t, typename P2T::CA_t >::convert(s, jap2, env);
                if (s == 0) {
                    ((cao).*F)(
                        (cap0),
                        (cap1),
                        (cap2)
                        );
                    Param< typename P2T::JA_t, typename P2T::CA_t >::release(cap2, jap2, env);
                }
                Param< typename P1T::JA_t, typename P1T::CA_t >::release(cap1, jap1, env);
            }
            Param< typename P0T::JA_t, typename P0T::CA_t >::release(cap0, jap0, env);
        }
        Param< typename OT::JA_t, typename OT::CA_t >::release(cao, jao, env);
    }
}

// ---------------------------------------------------------------------------
// Member Function Calls, Const, Return
// ---------------------------------------------------------------------------

template< typename OT,
          typename RT,
          typename RT::CF_t (OT::CF_t::*F)() const >
inline
typename RT::JF_t
gcall(JNIEnv * env, typename OT::JF_t jfo)
{
    TRACE("RT::JF_t gcall(JNIEnv *, OT::JF_t)");
    typename RT::JA_t jar = 0; // init return value to error
    int s;
    typename OT::JA_t jao = cast< typename OT::JA_t, typename OT::JF_t >(jfo);
    typename OT::CA_t cao = Param< typename OT::JA_t, typename OT::CA_t >::convert(s, jao, env);
    if (s == 0) {
        jar = Result< typename RT::JA_t, typename RT::CA_t >::convert(
            cast< typename RT::CA_t, typename RT::CF_t >(
                ((cao).*F)(
                    )),
            env);
        Param< typename OT::JA_t, typename OT::CA_t >::release(cao, jao, env);
    }
    return cast< typename RT::JF_t, typename RT::JA_t >(jar);
}

template< typename OT,
          typename RT,
          typename P0T,
          typename RT::CF_t (OT::CF_t::*F)(typename P0T::CF_t) const >
inline
typename RT::JF_t
gcall(JNIEnv * env, typename OT::JF_t jfo, typename P0T::JF_t jfp0)
{
    TRACE("RT::JF_t gcall(JNIEnv *, OT::JF_t, P0T::JF_t)");
    typename RT::JA_t jar = 0; // init return value to error
    int s;
    typename OT::JA_t jao = cast< typename OT::JA_t, typename OT::JF_t >(jfo);
    typename OT::CA_t cao = Param< typename OT::JA_t, typename OT::CA_t >::convert(s, jao, env);
    if (s == 0) {
        typename P0T::JA_t jap0 = cast< typename P0T::JA_t, typename P0T::JF_t >(jfp0);
        typename P0T::CA_t cap0 = Param< typename P0T::JA_t, typename P0T::CA_t >::convert(s, jap0, env);
        if (s == 0) {
            jar = Result< typename RT::JA_t, typename RT::CA_t >::convert(
                cast< typename RT::CA_t, typename RT::CF_t >(
                    ((cao).*F)(
                        (cap0)
                        )),
                env);
            Param< typename P0T::JA_t, typename P0T::CA_t >::release(cap0, jap0, env);
        }
        Param< typename OT::JA_t, typename OT::CA_t >::release(cao, jao, env);
    }
    return cast< typename RT::JF_t, typename RT::JA_t >(jar);
}

template< typename OT,
          typename RT,
          typename P0T,
          typename P1T,
          typename RT::CF_t (OT::CF_t::*F)(typename P0T::CF_t,typename P1T::CF_t) const >
inline
typename RT::JF_t
gcall(JNIEnv * env, typename OT::JF_t jfo, typename P0T::JF_t jfp0, typename P1T::JF_t jfp1)
{
    TRACE("RT::JF_t gcall(JNIEnv *, OT::JF_t, P0T::JF_t, P1T::JF_t)");
    typename RT::JA_t jar = 0; // init return value to error
    int s;
    typename OT::JA_t jao = cast< typename OT::JA_t, typename OT::JF_t >(jfo);
    typename OT::CA_t cao = Param< typename OT::JA_t, typename OT::CA_t >::convert(s, jao, env);
    if (s == 0) {
        typename P0T::JA_t jap0 = cast< typename P0T::JA_t, typename P0T::JF_t >(jfp0);
        typename P0T::CA_t cap0 = Param< typename P0T::JA_t, typename P0T::CA_t >::convert(s, jap0, env);
        if (s == 0) {
            typename P1T::JA_t jap1 = cast< typename P1T::JA_t, typename P1T::JF_t >(jfp1);
            typename P1T::CA_t cap1 = Param< typename P1T::JA_t, typename P1T::CA_t >::convert(s, jap1, env);
            if (s == 0) {
                jar = Result< typename RT::JA_t, typename RT::CA_t >::convert(
                    cast< typename RT::CA_t, typename RT::CF_t >(
                        ((cao).*F)(
                            (cap0),
                            (cap1)
                            )),
                    env);
                Param< typename P1T::JA_t, typename P1T::CA_t >::release(cap1, jap1, env);
            }
            Param< typename P0T::JA_t, typename P0T::CA_t >::release(cap0, jap0, env);
        }
        Param< typename OT::JA_t, typename OT::CA_t >::release(cao, jao, env);
    }
    return cast< typename RT::JF_t, typename RT::JA_t >(jar);
}

template< typename OT,
          typename RT,
          typename P0T,
          typename P1T,
          typename P2T,
          typename RT::CF_t (OT::CF_t::*F)(typename P0T::CF_t,typename P1T::CF_t,typename P2T::CF_t) const >
inline
typename RT::JF_t
gcall(JNIEnv * env, typename OT::JF_t jfo, typename P0T::JF_t jfp0, typename P1T::JF_t jfp1, typename P2T::JF_t jfp2)
{
    TRACE("RT::JF_t gcall(JNIEnv *, OT::JF_t, P0T::JF_t, P2T::JF_t, P2T::JF_t)");
    typename RT::JA_t jar = 0; // init return value to error
    int s;
    typename OT::JA_t jao = cast< typename OT::JA_t, typename OT::JF_t >(jfo);
    typename OT::CA_t cao = Param< typename OT::JA_t, typename OT::CA_t >::convert(s, jao, env);
    if (s == 0) {
        typename P0T::JA_t jap0 = cast< typename P0T::JA_t, typename P0T::JF_t >(jfp0);
        typename P0T::CA_t cap0 = Param< typename P0T::JA_t, typename P0T::CA_t >::convert(s, jap0, env);
        if (s == 0) {
            typename P1T::JA_t jap1 = cast< typename P1T::JA_t, typename P1T::JF_t >(jfp1);
            typename P1T::CA_t cap1 = Param< typename P1T::JA_t, typename P1T::CA_t >::convert(s, jap1, env);
            if (s == 0) {
                typename P2T::JA_t jap2 = cast< typename P2T::JA_t, typename P2T::JF_t >(jfp2);
                typename P2T::CA_t cap2 = Param< typename P2T::JA_t, typename P2T::CA_t >::convert(s, jap2, env);
                if (s == 0) {
                    jar = Result< typename RT::JA_t, typename RT::CA_t >::convert(
                        cast< typename RT::CA_t, typename RT::CF_t >(
                            ((cao).*F)(
                                (cap0),
                                (cap1),
                                (cap2)
                                )),
                        env);
                    Param< typename P2T::JA_t, typename P2T::CA_t >::release(cap2, jap2, env);
                }
                Param< typename P1T::JA_t, typename P1T::CA_t >::release(cap1, jap1, env);
            }
            Param< typename P0T::JA_t, typename P0T::CA_t >::release(cap0, jap0, env);
        }
        Param< typename OT::JA_t, typename OT::CA_t >::release(cao, jao, env);
    }
    return cast< typename RT::JF_t, typename RT::JA_t >(jar);
}

// ---------------------------------------------------------------------------
// Member Function Calls, Non-Const, Return
// ---------------------------------------------------------------------------

template< typename OT,
          typename RT,
          typename RT::CF_t (OT::CF_t::*F)() >
inline
typename RT::JF_t
gcall(JNIEnv * env, typename OT::JF_t jfo)
{
    TRACE("RT::JF_t gcall(JNIEnv *, OT::JF_t)");
    typename RT::JA_t jar = 0; // init return value to error
    int s;
    typename OT::JA_t jao = cast< typename OT::JA_t, typename OT::JF_t >(jfo);
    typename OT::CA_t cao = Param< typename OT::JA_t, typename OT::CA_t >::convert(s, jao, env);
    if (s == 0) {
        jar = Result< typename RT::JA_t, typename RT::CA_t >::convert(
            cast< typename RT::CA_t, typename RT::CF_t >(
                //((cao).*F)(
                ((cao).*F)(
                    )),
            env);
        Param< typename OT::JA_t, typename OT::CA_t >::release(cao, jao, env);
    }
    return cast< typename RT::JF_t, typename RT::JA_t >(jar);
}

template< typename OT,
          typename RT,
          typename P0T,
          typename RT::CF_t (OT::CF_t::*F)(typename P0T::CF_t) >
inline
typename RT::JF_t
gcall(JNIEnv * env, typename OT::JF_t jfo, typename P0T::JF_t jfp0)
{
    TRACE("RT::JF_t gcall(JNIEnv *, OT::JF_t, P0T::JF_t)");
    typename RT::JA_t jar = 0; // init return value to error
    int s;
    typename OT::JA_t jao = cast< typename OT::JA_t, typename OT::JF_t >(jfo);
    typename OT::CA_t cao = Param< typename OT::JA_t, typename OT::CA_t >::convert(s, jao, env);
    if (s == 0) {
        typename P0T::JA_t jap0 = cast< typename P0T::JA_t, typename P0T::JF_t >(jfp0);
        typename P0T::CA_t cap0 = Param< typename P0T::JA_t, typename P0T::CA_t >::convert(s, jap0, env);
        if (s == 0) {
            jar = Result< typename RT::JA_t, typename RT::CA_t >::convert(
                cast< typename RT::CA_t, typename RT::CF_t >(
                    ((cao).*F)(
                        (cap0)
                        )),
                env);
            Param< typename P0T::JA_t, typename P0T::CA_t >::release(cap0, jap0, env);
        }
        Param< typename OT::JA_t, typename OT::CA_t >::release(cao, jao, env);
    }
    return cast< typename RT::JF_t, typename RT::JA_t >(jar);
}

template< typename OT,
          typename RT,
          typename P0T,
          typename P1T,
          typename RT::CF_t (OT::CF_t::*F)(typename P0T::CF_t,typename P1T::CF_t) >
inline
typename RT::JF_t
gcall(JNIEnv * env, typename OT::JF_t jfo, typename P0T::JF_t jfp0, typename P1T::JF_t jfp1)
{
    TRACE("RT::JF_t gcall(JNIEnv *, OT::JF_t, P0T::JF_t, P1T::JF_t)");
    typename RT::JA_t jar = 0; // init return value to error
    int s;
    typename OT::JA_t jao = cast< typename OT::JA_t, typename OT::JF_t >(jfo);
    typename OT::CA_t cao = Param< typename OT::JA_t, typename OT::CA_t >::convert(s, jao, env);
    if (s == 0) {
        typename P0T::JA_t jap0 = cast< typename P0T::JA_t, typename P0T::JF_t >(jfp0);
        typename P0T::CA_t cap0 = Param< typename P0T::JA_t, typename P0T::CA_t >::convert(s, jap0, env);
        if (s == 0) {
            typename P1T::JA_t jap1 = cast< typename P1T::JA_t, typename P1T::JF_t >(jfp1);
            typename P1T::CA_t cap1 = Param< typename P1T::JA_t, typename P1T::CA_t >::convert(s, jap1, env);
            if (s == 0) {
                jar = Result< typename RT::JA_t, typename RT::CA_t >::convert(
                    cast< typename RT::CA_t, typename RT::CF_t >(
                        ((cao).*F)(
                            (cap0),
                            (cap1)
                            )),
                    env);
                Param< typename P1T::JA_t, typename P1T::CA_t >::release(cap1, jap1, env);
            }
            Param< typename P0T::JA_t, typename P0T::CA_t >::release(cap0, jap0, env);
        }
        Param< typename OT::JA_t, typename OT::CA_t >::release(cao, jao, env);
    }
    return cast< typename RT::JF_t, typename RT::JA_t >(jar);
}

template< typename OT,
          typename RT,
          typename P0T,
          typename P1T,
          typename P2T,
          typename RT::CF_t (OT::CF_t::*F)(typename P0T::CF_t,typename P1T::CF_t,typename P2T::CF_t) >
inline
typename RT::JF_t
gcall(JNIEnv * env, typename OT::JF_t jfo, typename P0T::JF_t jfp0, typename P1T::JF_t jfp1, typename P2T::JF_t jfp2)
{
    TRACE("RT::JF_t gcall(JNIEnv *, OT::JF_t, P0T::JF_t, P2T::JF_t, P2T::JF_t)");
    typename RT::JA_t jar = 0; // init return value to error
    int s;
    typename OT::JA_t jao = cast< typename OT::JA_t, typename OT::JF_t >(jfo);
    typename OT::CA_t cao = Param< typename OT::JA_t, typename OT::CA_t >::convert(s, jao, env);
    if (s == 0) {
        typename P0T::JA_t jap0 = cast< typename P0T::JA_t, typename P0T::JF_t >(jfp0);
        typename P0T::CA_t cap0 = Param< typename P0T::JA_t, typename P0T::CA_t >::convert(s, jap0, env);
        if (s == 0) {
            typename P1T::JA_t jap1 = cast< typename P1T::JA_t, typename P1T::JF_t >(jfp1);
            typename P1T::CA_t cap1 = Param< typename P1T::JA_t, typename P1T::CA_t >::convert(s, jap1, env);
            if (s == 0) {
                typename P2T::JA_t jap2 = cast< typename P2T::JA_t, typename P2T::JF_t >(jfp2);
                typename P2T::CA_t cap2 = Param< typename P2T::JA_t, typename P2T::CA_t >::convert(s, jap2, env);
                if (s == 0) {
                    jar = Result< typename RT::JA_t, typename RT::CA_t >::convert(
                        cast< typename RT::CA_t, typename RT::CF_t >(
                            ((cao).*F)(
                                (cap0),
                                (cap1),
                                (cap2)
                                )),
                        env);
                    Param< typename P2T::JA_t, typename P2T::CA_t >::release(cap2, jap2, env);
                }
                Param< typename P1T::JA_t, typename P1T::CA_t >::release(cap1, jap1, env);
            }
            Param< typename P0T::JA_t, typename P0T::CA_t >::release(cap0, jap0, env);
        }
        Param< typename OT::JA_t, typename OT::CA_t >::release(cao, jao, env);
    }
    return cast< typename RT::JF_t, typename RT::JA_t >(jar);
}

// ---------------------------------------------------------------------------
// Constructor.Destructor Calls
// ---------------------------------------------------------------------------

template< typename P0T,
          void F(typename P0T::CF_t) >
inline
void
gdelete(JNIEnv * env, typename P0T::JF_t jfp0)
{
    TRACE("void gdelete(JNIEnv *, P0T::JF_t)");
    int s;
    typename P0T::JA_t jap0 = cast< typename P0T::JA_t, typename P0T::JF_t >(jfp0);
    typename P0T::CA_t cap0 = Param< typename P0T::JA_t, typename P0T::CA_t >::convert(s, jap0, env);
    if (s == 0) {
        F(
            (cap0)
            );
        Param< typename P0T::JA_t, typename P0T::CA_t >::release(cap0, jap0, env);
        detachWrapper(jfp0, env);
    }
}

template< typename RT,
          typename RT::CF_t F() >
inline
typename RT::JF_t
gcreate(JNIEnv * env)
{
    TRACE("RT::JF_t gcreate(JNIEnv *)");
    return gcall< RT, F >(env);
}

template< typename RT,
          typename P0T,
          typename RT::CF_t F(typename P0T::CF_t) >
inline
typename RT::JF_t
gcreate(JNIEnv * env, typename P0T::JF_t jfp0)
{
    TRACE("RT::JF_t gcreate(JNIEnv *, P0T::JF_t)");
    return gcall< RT, P0T, F >(env, jfp0);
}

template< typename RT,
          typename P0T,
          typename P1T,
          typename RT::CF_t F(typename P0T::CF_t,typename P1T::CF_t) >
inline
typename RT::JF_t
gcreate(JNIEnv * env, typename P0T::JF_t jfp0, typename P1T::JF_t jfp1)
{
    TRACE("RT::JF_t gcreate(JNIEnv *, P0T::JF_t, P1T::JF_t)");
    return gcall< RT, P0T, P1T, F >(env, jfp0, jfp1);
}

template< typename RT,
          typename P0T,
          typename P1T,
          typename P2T,
          typename RT::CF_t F(typename P0T::CF_t,typename P1T::CF_t,typename P2T::CF_t) >
inline
typename RT::JF_t
gcreate(JNIEnv * env, typename P0T::JF_t jfp0, typename P1T::JF_t jfp1, typename P2T::JF_t jfp2)
{
    TRACE("RT::JF_t gcreate(JNIEnv *, P0T::JF_t, P1T::JF_t, P2T::JF_t)");
    return gcall< RT, P0T, P1T, P2T, F >(env, jfp0, jfp1, jfp2);
}

// ---------------------------------------------------------------------------

/*
template< typename JP0, typename CP0,
          typename JP1, typename CP1,
          void F(CP0, CP1) >
inline
void
gcall(JNIEnv * env, JP0 jp0, JP1 jp1)
{
    TRACE("void gcall(JNIEnv *, JP0, JP1)");
    CP0 cp0;
    if (Param< JP0, CP0 >::convert(env, cp0, jp0) == 0) {
        CP1 cp1;
        if (Param< JP1, CP1 >::convert(env, cp1, jp1) == 0) {
            F(cp0, cp1);
            Param< JP1, CP1 >::release(env, cp1, jp1);
        }
        Param< JP0, CP0 >::release(env, cp0, jp0);
    }
}
*/

#endif // jtie_gcalls_hpp
