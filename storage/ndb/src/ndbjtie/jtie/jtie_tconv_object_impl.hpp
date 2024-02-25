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
 * jtie_tconv_object_impl.hpp
 */

#ifndef jtie_tconv_object_impl_hpp
#define jtie_tconv_object_impl_hpp

#include <assert.h> // not using namespaces yet
//#include <stdio.h> // not using namespaces yet
#include <jni.h>

#include "jtie_tconv_object.hpp"
#include "jtie_tconv_impl.hpp"
#include "jtie_tconv_idcache_impl.hpp"
#include "jtie_tconv_utils_impl.hpp"
#include "helpers.hpp"

// ---------------------------------------------------------------------------
// ObjectParam, Target, ObjectResult
// ---------------------------------------------------------------------------

// XXX cleanup, document

// Defines the field info type for Wrapper.cdelegate.
JTIE_DEFINE_FIELD_MEMBER_INFO(_Wrapper_cdelegate)

// Provides a (cached) access to field Id of Wrapper.cdelegate.
//typedef JniMemberId< NO_CACHING, _Wrapper_cdelegate > Wrapper_cdelegate;
typedef JniMemberId< WEAK_CACHING, _Wrapper_cdelegate > Wrapper_cdelegate;
//typedef JniMemberId< STRONG_CACHING, _Wrapper_cdelegate > Wrapper_cdelegate;

// ---------------------------------------------------------------------------

// XXX consider changing
//template< typename C > struct ObjectParam< _jtie_Object *, C * > {
// to
//template< typename J, typename C > struct ObjectParam< J *, C * > {
// same for Target, Result; or conversely
//template< typename J > struct ObjectResult< J *, void * > {

// Implements the mapping of jtie_Objects parameters.
template< typename J, typename C >
struct ObjectParam;

// Implements the mapping of jtie_Object invocation targets.
template< typename J, typename C >
struct Target;

// Implements the mapping of jtie_Object results.
template< typename J, typename C >
struct ObjectResult;

// Implements the mapping of jtie_Objects parameters to pointers.
template< typename C >
struct ObjectParam< _jtie_Object *, C * > {
    static C *
    convert(cstatus & s, _jtie_Object * j, JNIEnv * env) {
        TRACE("C * ObjectParam.convert(cstatus &, _jtie_Object *, JNIEnv *)");        
        // init status to error
        s = -1;
        C * c = NULL;

        if (j == NULL) {
            // ok
            s = 0;
        } else {
            // get a (local or global) class object reference
            jclass cls = Wrapper_cdelegate::getClass(env);
            if (cls == NULL) {
                // exception pending
            } else {
                // get the field ID valid along with the class reference
                jfieldID fid = Wrapper_cdelegate::getId(env, cls);
                if (fid == NULL) {
                    // exception pending
                } else {
                    // get the field's value
                    jlong p = env->GetLongField(j, fid);
                    //printf("    c = %lx\n", (unsigned long)p);
                    if (p == 0L) {
                        const char * m
                            = ("JTie: Java wrapper object must have a"
                               " non-zero delegate when used as target or"
                               " argument in a method call"
                               " (file: " __FILE__ ")");
                        registerException(env, "java/lang/AssertionError", m);
                    } else {
                        // convert jlong to an address via intptr_t (C99)
                        intptr_t ip =  static_cast< intptr_t >(p);
                        assert(static_cast< jlong >(ip) == p);
                        c = reinterpret_cast< C * >(ip);
                        //printf("    c = %p\n", c);

                        // ok
                        s = 0;
                    }
                }
                // release reference (if needed)
                Wrapper_cdelegate::releaseRef(env, cls);
            }
        }
        return c;
    }

    static void
    release(C * c, _jtie_Object * j, JNIEnv * env) {
        TRACE("void ObjectParam.release(C *, _jtie_Object *, JNIEnv *)");
        //printf("    c = %lx\n", (unsigned long)c);
        (void)c; (void)j; (void)env;
    }
};

// Implements the mapping of jtie_Objects parameters to references.
template< typename C >
struct ObjectParam< _jtie_Object *, C & > {
    static C &
    convert(cstatus & s, _jtie_Object * j, JNIEnv * env) {
        TRACE("C & ObjectParam.convert(cstatus &, _jtie_Object *, JNIEnv *)");

        // init return value and status to error
        s = -1;
        C * c = NULL;

        if (j == NULL) {
            const char * msg
                = ("JTie: Java argument must not be null when mapped"
                   " to a C reference (file: " __FILE__ ")");
            registerException(env, "java/lang/IllegalArgumentException", msg);
        } else {
            c = ObjectParam< _jtie_Object *, C * >::convert(s, j, env);
            assert(s != 0 || c != NULL);
        }

        // never actually dereferenced if status indicates an error
        return *c;
    }

    static void
    release(C & c, _jtie_Object * j, JNIEnv * env) {
        TRACE("void ObjectParam.release(C &, _jtie_Object *, JNIEnv *)");
        ObjectParam< _jtie_Object *, C * >::release(&c, j, env);
    }
};

// Implements the mapping of jtie_Object invocation targets.
template< typename C >
struct Target< _jtie_Object *, C > {
    static C &
    convert(cstatus & s, _jtie_Object * j, JNIEnv * env) {
        TRACE("C & Target.convert(cstatus &, _jtie_Object *, JNIEnv *)");

        // init return value and status to error
        s = -1;
        C * c = NULL;

        if (j == NULL) {
            const char * msg = ("JTie: Java target object of a method call"
                                " must not be null (file: " __FILE__ ")");
            registerException(env, "java/lang/NullPointerException", msg);
        } else {
            // to avoid template instantiation clutter and ambiguities
            // specialize/delegate to either ObjectParam< _jtie_Object *, C * >
            // (preferred for smaller type space) or Param< J *, C * > 
            c = ObjectParam< _jtie_Object *, C * >::convert(s, j, env);
            assert(s != 0 || c != NULL);
        }

        // never actually dereferenced if status indicates an error
        return *c;
    }

    static void
    release(C & c, _jtie_Object * j, JNIEnv * env) {
        TRACE("void Target.release(C &, _jtie_Object *, JNIEnv *)");
        // match delegation in convert()
        ObjectParam< _jtie_Object *, C * >::release(&c, j, env);
    }
};

// Implements the mapping of jtie_Object results to pointers.
template< typename J, typename C >
struct ObjectResult< J *, C * > {
    // Provides a (cached) access to the method Id of the constructor of J.
    //typedef JniMemberId< NO_CACHING, typename J::ctor > J_ctor;
    typedef JniMemberId< WEAK_CACHING, typename J::ctor > J_ctor;
    //typedef JniMemberId< STRONG_CACHING, typename J::ctor > J_ctor;

    static J *
    convert(C * c, JNIEnv * env) {
        TRACE("J * ObjectResult.convert(JNIEnv *, C *)");
        J * j = NULL;

        if (c == NULL) {
            // ok
        } else {
            // get a (local or global) class object reference
            jclass cls = J_ctor::getClass(env);
            if (cls == NULL) {
                // exception pending
            } else {
                // get the method ID valid along with the class reference
                jmethodID cid = J_ctor::getId(env, cls);
                if (cid == NULL) {
                    // exception pending
                } else {
                    J * jo = wrapAsJavaObject(cls, cid, c, env);
                    if (jo == NULL) {
                        // exception pending
                    } else {
                        // ok
                        j = jo;
                    }
                }
                // release reference (if needed)
                J_ctor::releaseRef(env, cls);
            }
        }
        return j;
    }

private:
    // Constructs a Wrapper object of user-defined type.
    static J *
    wrapAsJavaObject(jclass cls, jmethodID cid, C * c, JNIEnv * env);
};

// Implements the mapping of jtie_Object results to references.
template< typename J , typename C >
struct ObjectResult< J *, C & > {
    static J *
    convert(C & c, JNIEnv * env) {
        TRACE("J * ObjectResult.convert(JNIEnv *, C &)");        
        J * j = NULL; // init to error
        C * p = &c;

        if (p == NULL) {
            const char * msg
                = ("JTie: returned C reference must not be null"
                   " (e.g., check if memory allocation has failed without"
                   " raising an exception, as can happen with older C++"
                   " compilers?) (file: " __FILE__ ")");
            registerException(env, "java/lang/AssertionError", msg);
        } else {
            // ok
            j = ObjectResult< J *, C * >::convert(p, env);
        }
        return j;
    }
};

// ---------------------------------------------------------------------------
// Helper functions
// ---------------------------------------------------------------------------

// Nullifies a Wrapper object's stored address of the native delegate.
inline void
detachWrapper(_jtie_Object * jo, JNIEnv * env) {
    // get a (local or global) class object reference
    // as a precaution, do not use env->GetObjectClass(jobject), for we
    // never want to access a field from a subclass that hides the delegate
    // field in Wrapper
    jclass cls = Wrapper_cdelegate::getClass(env);
    if (cls == NULL) {
        // exception pending
    } else {
        // get the field ID valid along with the class reference
        jfieldID fid = Wrapper_cdelegate::getId(env, cls);
        if (fid == NULL) {
            // exception pending
        } else {
            // convert address to a jlong via intptr_t (C99)
            //printf("    p = %p\n", c);
            intptr_t ip = 0; //reinterpret_cast< intptr_t >((void*)NULL);
            jlong p = static_cast< jlong >(ip);
            assert(static_cast< intptr_t >(p) == ip);

            // set the field's value
            env->SetLongField(jo, fid, p);
        }
        // release reference (if needed)
        Wrapper_cdelegate::releaseRef(env, cls);
    }
}

// Constructs a Wrapper object of user-defined type.
template< typename J, typename C >
inline J *
ObjectResult< J *, C * >::
wrapAsJavaObject(jclass cls, jmethodID cid, C * c, JNIEnv * env) {
    J * j = NULL;

    // get a (local or global) class object reference
    // as a precaution, do not use parameter cls, for we never want to access
    // a field from a subclass that hides the delegate field in Wrapper
    jclass cls0 = Wrapper_cdelegate::getClass(env);
    if (cls0 == NULL) {
        // exception pending
    } else {
        // get the field ID valid along with the class reference
        jfieldID fid = Wrapper_cdelegate::getId(env, cls0);
        if (fid == NULL) {
            // exception pending
        } else {
            // construct a Wrapper object
            jobject jo = env->NewObject(cls, cid);
            if (jo == NULL) {
                // exception pending
            } else {
                // convert address to a jlong via intptr_t (C99)
                //printf("    p = %p\n", c);
                intptr_t ip = reinterpret_cast< intptr_t >(c);
                jlong p = static_cast< jlong >(ip);
                assert(static_cast< intptr_t >(p) == ip);

                // set the field's value
                env->SetLongField(jo, fid, p);

                // ok
                j = static_cast< J * >(jo);
            }
        }
        // release reference (if needed)
        Wrapper_cdelegate::releaseRef(env, cls0);
    }
    return j;
}

// ---------------------------------------------------------------------------
// Specializations for Object type conversions
// ---------------------------------------------------------------------------

// extend jtie_Object specializations to const pointers
template< typename J, typename C >
struct Param< _jtie_ObjectMapper< J > *, C * const >
    : Param< _jtie_ObjectMapper< J > *, C * > {};
template< typename J, typename C >
struct Result< _jtie_ObjectMapper< J > *, C * const >
    :  Result< _jtie_ObjectMapper< J > *, C * > {};

// Sufficient to specialize Param<> and Target<> over jtie_Object.

// specialize Target (note usage of C, which is a direct type)
template< typename J, typename C >
struct Target< _jtie_ObjectMapper< J > *, C >
    : Target< _jtie_Object *, C > {};

// specialize Param for pointers (note usage of C)
template< typename J, typename C >
struct Param< _jtie_ObjectMapper< J > *, C * >
    : ObjectParam< _jtie_Object *, C * > {};

// specialize Param for references (note usage of C)
template< typename J, typename C >
struct Param< _jtie_ObjectMapper< J > *, C & >
    : ObjectParam< _jtie_Object *, C & > {};

// Result<> needs to be specialized over ObjectMapper, which carries
// additional information needed for instantiating Java wrapper objects.

// specialize Result for pointers (note usage of C)
template< typename J, typename C >
struct Result< _jtie_ObjectMapper< J > *, C * >
    : ObjectResult< _jtie_ObjectMapper< J > *, C * > {};

// specialize Result for references (note usage of C)
template< typename J, typename C >
struct Result< _jtie_ObjectMapper< J > *, C & >
    : ObjectResult< _jtie_ObjectMapper< J > *, C & > {};

// ---------------------------------------------------------------------------

#endif // jtie_tconv_object_impl_hpp
