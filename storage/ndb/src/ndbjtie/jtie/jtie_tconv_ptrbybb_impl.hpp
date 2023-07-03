/*
 Copyright (c) 2010, 2022, Oracle and/or its affiliates.
 Use is subject to license terms.

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
 * jtie_tconv_ptrbybb_impl.hpp
 */

#ifndef jtie_tconv_ptrbybb_impl_hpp
#define jtie_tconv_ptrbybb_impl_hpp

#include <assert.h> // not using namespaces yet
#include <string.h> // not using namespaces yet
#include <jni.h>

#include "jtie_tconv_ptrbybb.hpp"
#include "jtie_stdint.h"
#include "jtie_tconv_impl.hpp"
#include "jtie_tconv_idcache_impl.hpp"
#include "jtie_tconv_utils_impl.hpp"
#include "helpers.hpp"

// ---------------------------------------------------------------------------
// ByteBufferPtrParam, ByteBufferPtrResult
// ---------------------------------------------------------------------------

// Defines the method info type for ByteBuffer.isReadOnly().
JTIE_DEFINE_METHOD_MEMBER_INFO(_ByteBuffer_isReadOnly)

// Provides a (cached) access to method Id of ByteBuffer.isReadOnly().
//typedef JniMemberId< NO_CACHING, _ByteBuffer_isReadOnly >
typedef JniMemberId< WEAK_CACHING, _ByteBuffer_isReadOnly >
    ByteBuffer_isReadOnly;

// ---------------------------------------------------------------------------

// Defines the method info type for ByteBuffer.asReadOnlyBuffer().
JTIE_DEFINE_METHOD_MEMBER_INFO(_ByteBuffer_asReadOnlyBuffer)

// Provides a (cached) access to method Id of ByteBuffer.asReadOnlyBuffer().
//typedef JniMemberId< NO_CACHING, _ByteBuffer_asReadOnlyBuffer >
typedef JniMemberId< WEAK_CACHING, _ByteBuffer_asReadOnlyBuffer >
    ByteBuffer_asReadOnlyBuffer;

// ---------------------------------------------------------------------------

// Defines the method info type for ByteBuffer.remaining().
JTIE_DEFINE_METHOD_MEMBER_INFO(_ByteBuffer_remaining)

// Provides a (cached) access to method Id of ByteBuffer.remaining().
//typedef JniMemberId< NO_CACHING, _ByteBuffer_remaining >
typedef JniMemberId< WEAK_CACHING, _ByteBuffer_remaining >
    ByteBuffer_remaining;

// ---------------------------------------------------------------------------

// Defines the method info type for ByteBuffer.position().
JTIE_DEFINE_METHOD_MEMBER_INFO(_ByteBuffer_position)

// Provides a (cached) access to method Id of ByteBuffer.position().
//typedef JniMemberId< NO_CACHING, _ByteBuffer_position >
typedef JniMemberId< WEAK_CACHING, _ByteBuffer_position >
    ByteBuffer_position;

// ---------------------------------------------------------------------------

// helper functions

inline cstatus
ensureMutableBuffer(jtie_j_n_ByteBuffer jbb, JNIEnv * env);

template< jlong N >
inline cstatus
ensureMinBufferSize(jtie_j_n_ByteBuffer jbb, JNIEnv * env);

inline int32_t
getBufferPosition(jtie_j_n_ByteBuffer jbb, JNIEnv * env);

inline void *
getByteBufferAddress(jtie_j_n_ByteBuffer jbb, JNIEnv * env);

template< typename J >
inline J *
wrapAddressAsByteBuffer(const void * c, JNIEnv * env);

template< typename J >
inline J *
wrapByteBufferAsReadOnly(J * jbb, JNIEnv * env);

// ---------------------------------------------------------------------------

// Implements the mapping of ByteBuffers to pointer parameters.
template< typename J, typename C >
struct ByteBufferPtrParam {

    static C *
    convert(cstatus & s, jtie_j_n_ByteBuffer j, JNIEnv * env) {
        TRACE("C * ByteBufferPtrParam.convert(cstatus &, jtie_j_n_ByteBuffer, JNIEnv *)");

        // init return value and status to error
        s = -1;
        C * c = NULL;

        if (j == NULL) {
            // ok
            s = 0;
        } else {
            if (TypeInfo< C >::isMutable()
                && (ensureMutableBuffer(j, env) != 0)) {
                // exception pending
            } else {
                if (ensureMinBufferSize< J::capacity >(j, env) != 0) {
                    // exception pending
                } else {
                    assert(env->GetDirectBufferCapacity(j) >= J::capacity);
                    void * a = getByteBufferAddress(j, env);
                    if (a == NULL) {
                        // exception pending
                    } else {
                        // ok
                        s = 0;
                        c = static_cast< C * >(a);
                    }
                }
            }
        }
        return c;
    }

    static void
    release(C * c, jtie_j_n_ByteBuffer j, JNIEnv * env) {
        TRACE("void ByteBufferPtrParam.release(C *, jtie_j_n_ByteBuffer, JNIEnv *)");
        (void)c; (void)j; (void)env;
    }
};

// Implements the mapping of ByteBuffers to pointer results.
template< typename J, typename C >
struct ByteBufferPtrResult {
    static J *
    convert(C * c, JNIEnv * env) {
        TRACE("J * ByteBufferPtrResult.convert(C *, JNIEnv *)");

        // init return value to error
        J * j = NULL;

        if (c == NULL) {
            // ok
        } else {
            J * jbb = wrapAddressAsByteBuffer< J >(c, env);        
            if (jbb == NULL) {
                // exception pending
            } else {
                assert(env->GetDirectBufferCapacity(jbb) == J::capacity);
                if (TypeInfo< C >::isMutable()) {
                    // ok
                    j = jbb;
                } else {
                    J * jrobb = wrapByteBufferAsReadOnly(jbb, env);
                    if (jrobb == NULL) {
                        // exception pending
                    } else {
                        // ok
                        j = jrobb;
                    }
                    env->DeleteLocalRef(jbb);
                }
            }
        }
        return j;
    }
};

// ---------------------------------------------------------------------------
// Helper functions
// ---------------------------------------------------------------------------

// Returns zero if a buffer is read-only; otherwise, an exception is pending.
inline cstatus
ensureMutableBuffer(jtie_j_n_ByteBuffer jbb, JNIEnv * env) {
    // init return value to error
    cstatus s = -1;
    
    // get a (local or global) class object reference
    jclass cls = ByteBuffer_isReadOnly::getClass(env);
    if (cls == NULL) {
        // exception pending
    } else {
        // get the method ID valid along with the class object reference
        jmethodID mid = ByteBuffer_isReadOnly::getId(env, cls);
        if (mid == NULL) {
            // exception pending
        } else {
            jboolean ro = env->CallBooleanMethod(jbb, mid);
            if (env->ExceptionCheck() != JNI_OK) {
                // exception pending
            } else {
                if (ro) {
                    const char * c = "java/nio/ReadOnlyBufferException";
                    // this exception's c'tor does not take messages
                    const char * m = NULL;
                    //const char * m = ("JTie: java.nio.ByteBuffer must"
                    //                  " not be read-only when mapped to a"
                    //                  "  non-const object reference type");
                    registerException(env, c, m);
                } else {
                    // ok
                    s = 0;
                }
            }
        }
        // release reference (if needed)
        ByteBuffer_isReadOnly::releaseRef(env, cls);
    }
    return s;
}

// Returns zero if a buffer has a min size; otherwise, an exception is pending.
template< jlong N >
inline cstatus
ensureMinBufferSize(jtie_j_n_ByteBuffer jbb, JNIEnv * env) {
    // init return value to error
    cstatus s = -1;

    // check the ByteBuffer's capacity
    jlong bc = env->GetDirectBufferCapacity(jbb);
    if (bc < N) {
        // crashes with gcc & operator<<(ostream &, jlong/jint)
        char m[256];
        const long long n = N;
        const long long BC = bc;
        if (bc < 0) {
            sprintf(m, "JTie: failed to retrieve java.nio.ByteBuffer's"
                    " capacity (perhaps, a direct buffer or an unaligned"
                    " view buffer)");
        } else {
            sprintf(m, "JTie: java.nio.ByteBuffer's capacity is too small"
                    "  for the mapped parameter;"
                    " required: %lld, found: %lld.", n, BC);
        }
        const char * c = "java/lang/IllegalArgumentException";
        registerException(env, c, m);
    } else {
#ifndef NDEBUG
        // get a (local or global) class object reference
        jclass cls = ByteBuffer_remaining::getClass(env);
        if (cls == NULL) {
            // exception pending
        } else {
            // get the method ID valid along with the class object reference
            jmethodID mid = ByteBuffer_remaining::getId(env, cls);
            if (mid == NULL) {
                // exception pending
            } else {
                jint r = env->CallIntMethod(jbb, mid);
                if (env->ExceptionCheck() != JNI_OK) {
                    // exception pending
                } else {
                    if (r < N) {
                        // crashes with gcc & operator<<(ostream &, jlong/jint)
                        char m[256];
                        const long long n = N;
                        const long long R = r;
                        sprintf(m, "JTie: too few remaining elements of"
                                " java.nio.ByteBuffer for mapped parameter;"
                                " required: %lld, found: %lld", n, R);
                        const char * c = "java/lang/IllegalArgumentException";
                        registerException(env, c, m);
                    } else {
                        // ok
                        s = 0;
                    }
                }
            }
            // release reference (if needed)
            ByteBuffer_remaining::releaseRef(env, cls);
        }
#else
        // ok
        s = 0;
#endif // NDEBUG
    }    
    return s;
}

// Returns the buffer's position; if the position cannot be accessed for any
// reason, a negative value is returned and an exception is pending.
inline int32_t
getBufferPosition(jtie_j_n_ByteBuffer jbb, JNIEnv * env) {
    // init return value to error
    jint pos = -1;

    // get a (local or global) class object reference
    jclass cls = ByteBuffer_position::getClass(env);
    if (cls == NULL) {
        // exception pending
    } else {
        // get the method ID valid along with the class object reference
        jmethodID mid = ByteBuffer_position::getId(env, cls);
        if (mid == NULL) {
            // exception pending
        } else {
            jint p = env->CallIntMethod(jbb, mid);
            if (env->ExceptionCheck() != JNI_OK) {
                // exception pending
            } else {
                // ok
                pos = p;
            }
        }
        // release reference (if needed)
        ByteBuffer_position::releaseRef(env, cls);
    }
    return pos;
}


// Returns the buffer address of a direct ByteBuffer; if the address cannot
// be accessed for any reason, NULL is returned and an exception is pending.
inline void *
getByteBufferAddress(jtie_j_n_ByteBuffer jbb, JNIEnv * env) {
    // get the internal buffer address of direct ByteBuffer
    char * a = static_cast< char * >(env->GetDirectBufferAddress(jbb));
    if (a == NULL) {
#ifndef JTIE_BYTEBUFFER_NO_ZERO_CAPACITY_MAPPING
        // check for direct ByteBuffer of zero-capacity
        if (env->GetDirectBufferCapacity(jbb) != 0) {
#endif // JTIE_BYTEBUFFER_NO_ZERO_CAPACITY_MAPPING
            // raise exception
            const char * m = ("JTie: cannot get the java.nio.ByteBuffer's"
                              " internal address (perhaps, not a direct buffer"
                              " or its memory region is undefined)");
            const char * c = "java/lang/IllegalArgumentException";
            registerException(env, c, m);
#ifndef JTIE_BYTEBUFFER_NO_ZERO_CAPACITY_MAPPING
        } else {
            // ok
            assert(a == NULL);
        }
#endif // JTIE_BYTEBUFFER_NO_ZERO_CAPACITY_MAPPING
    } else {
#ifndef JTIE_BYTEBUFFER_MAPS_TO_BASE_ADDRESS
        int32_t p = getBufferPosition(jbb, env);
        if (p < 0) {
            // exception pending
        } else {
            // ok
            a += p;
        }
#endif // JTIE_BYTEBUFFER_MAPS_TO_BASE_ADDRESS
    }
    return a;
}

// Constructs a fixed-length, direct ByteBuffer wrapping an address.
template< typename J >
inline J *
wrapAddressAsByteBuffer(const void * c, JNIEnv * env) {
    // ok to strip const here, will be wrapped as read-only buffer then
    void * mc = const_cast< void * >(c);
    jobject jo = env->NewDirectByteBuffer(mc, J::capacity);
    return static_cast< J * >(jo);
}

// Constructs a read-only ByteBuffer wrapping a buffer.
template< typename J >
inline J *
wrapByteBufferAsReadOnly(J * jbb, JNIEnv * env) {
    // init return value to error
    J * j = NULL;
    
    // get a (local or global) class object reference
    jclass cls = ByteBuffer_asReadOnlyBuffer::getClass(env);
    if (cls == NULL) {
        // exception pending
    } else {
        // get the method ID valid along with the class object reference
        jmethodID mid = ByteBuffer_asReadOnlyBuffer::getId(env, cls);
        if (mid == NULL) {
            // exception pending
        } else {
            // get a read-only copy from the ByteBuffer object
            jobject jo = env->CallObjectMethod(jbb, mid);
            if (env->ExceptionCheck() != JNI_OK) {
                // exception pending
            } else {
                if (jo == NULL) {
                    const char * m
                        = ("JTie: invalid NULL return from"
                           " java.nio.ByteBuffer.asReadOnlyBuffer()");
                    const char * c = "java/lang/AssertionError";
                    registerException(env, c, m);
                } else {
                    j = static_cast< J * >(jo);
                }
            }
        }
        // release reference (if needed)
        ByteBuffer_asReadOnlyBuffer::releaseRef(env, cls);
    }
    return j;
}

// ---------------------------------------------------------------------------
// Specializations for ByteBuffer type conversions
// ---------------------------------------------------------------------------

// extend ByteBuffer specializations to const pointers
template< typename C >
struct Param< jtie_j_n_ByteBuffer, C * const > 
    : Param< jtie_j_n_ByteBuffer, C * > {};
template< typename C >
struct Result< jtie_j_n_ByteBuffer, C * const >
    : Result< jtie_j_n_ByteBuffer, C * > {};
template< typename J, typename C >
struct Param< _jtie_j_n_ByteBufferMapper< J > *, C * const >
    : Param< _jtie_j_n_ByteBufferMapper< J > *, C * > {};
template< typename J, typename C >
struct Result< _jtie_j_n_ByteBufferMapper< J > *, C * const >
    :  Result< _jtie_j_n_ByteBufferMapper< J > *, C * > {};

// specialize BoundedByteBuffers mapped to pointers/arrays:
// - params: require a minimum buffer capacity given by the
//   BoundedByteBuffer's static data member
// - results: allocate buffer with a capacity given by the
//   BoundedByteBuffer's static data member
template< typename J, typename C >
struct Param< _jtie_j_n_ByteBufferMapper< J > *, C * >
    : ByteBufferPtrParam< _jtie_j_n_ByteBufferMapper< J >, C > {};
template< typename J, typename C >
struct Result< _jtie_j_n_ByteBufferMapper< J > *, C * >
    : ByteBufferPtrResult< _jtie_j_n_ByteBufferMapper< J >, C > {};

// specialize ByteBuffers mapped to pointers/arrays:
// - params: do not require a minimum buffer capacity, for size may be zero
//   when just passing an address
// - results: allocate buffer with a capacity of zero, since the size is
//    unknown (i.e., just returning an address)
template< typename C >
struct Param< jtie_j_n_ByteBuffer, C * >
    : ByteBufferPtrParam< _jtie_j_n_BoundedByteBuffer< 0 >, C > {};
template< typename C >
struct Result< jtie_j_n_ByteBuffer, C * >
    : ByteBufferPtrResult< _jtie_j_n_BoundedByteBuffer< 0 >, C > {};

// ---------------------------------------------------------------------------

#endif // jtie_tconv_ptrbybb_impl_hpp
