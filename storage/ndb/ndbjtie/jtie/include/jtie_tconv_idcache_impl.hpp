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
 * jtie_tconv_idcache_impl.hpp
 */

#ifndef jtie_tconv_idcache_impl_hpp
#define jtie_tconv_idcache_impl_hpp

#include <assert.h> // not using namespaces yet
#include <jni.h>

//#include "helpers.hpp"

// ---------------------------------------------------------------------------
// JNI field/method IDs access & caching
// ---------------------------------------------------------------------------

#if 0 // XXX ddefines not used at this time

// compilation flags
#define JTIE_NO_JNI_ID_CACHING 0
#define JTIE_WEAK_JNI_ID_CACHING 1
#define JTIE_STRONG_JNI_ID_CACHING 2

#if !defined(JTIE_JNI_ID_CACHING)
#  define JNI_ID_CACHING WEAK_CACHING
#elif (JTIE_JNI_ID_CACHING == JTIE_NO_JNI_ID_CACHING)
#  define JNI_ID_CACHING NO_CACHING
#elif (JTIE_JNI_ID_CACHING == JTIE_WEAK_JNI_ID_CACHING)
#  define JNI_ID_CACHING WEAK_CACHING
#elif (JTIE_JNI_ID_CACHING == JTIE_STRONG_JNI_ID_CACHING)
#  define JNI_ID_CACHING STRONG_CACHING
#else
#  error "Illegal value of JTIE_JNI_ID_CACHING"
#endif

#endif // XXX ddefines not used at this time

// ---------------------------------------------------------------------------

// JNI helper functions

template< typename T >
inline T
jniGetMemberID(JNIEnv * env,
               jclass cls,
               const char * name, const char * descriptor);

template<>
inline jmethodID
jniGetMemberID< jmethodID >(JNIEnv * env,
                            jclass cls,
                            const char * name, const char * descriptor) {
    return env->GetMethodID(cls, name, descriptor);
}

template<>
inline jfieldID
jniGetMemberID< jfieldID >(JNIEnv * env,
                           jclass cls,
                           const char * name, const char * descriptor) {
    return env->GetFieldID(cls, name, descriptor);
}

// ---------------------------------------------------------------------------

// XXX document these macros...

/**
 * Defines an info type describing a field member of a Java class.
 */
#define JTIE_DEFINE_FIELD_MEMBER_INFO( T )              \
    JTIE_DEFINE_CLASS_MEMBER_INFO( T, _jfieldID )

/**
 * Defines an info type describing a method member of a Java class.
 */
#define JTIE_DEFINE_METHOD_MEMBER_INFO( T )             \
    JTIE_DEFINE_CLASS_MEMBER_INFO( T, _jmethodID )

/**
 * Defines an info type describing a member of a Java class.
 */
#define JTIE_DEFINE_CLASS_MEMBER_INFO( T, IDT )                 \
    struct T {                                                  \
        static const char * const class_name;                   \
        static const char * const member_name;                  \
        static const char * const member_descriptor;            \
        typedef IDT * memberID_t;                               \
    };

/**
 * Instantiates an info type describing a member of a Java class.
 */
#define JTIE_INSTANTIATE_CLASS_MEMBER_INFO( T, CN, MN, MD )     \
    const char * const T::class_name = CN;                      \
    const char * const T::member_name = MN;                     \
    const char * const T::member_descriptor = MD;               \
    template struct MemberId< T >;                              \
    template struct MemberIdCache< T >;

// XXX document these classes...

template< typename C >
struct MemberId {
    typedef typename C::memberID_t ID_t;
    
    // number of JNI Get<Field|Method>ID() invocations
    static unsigned long nIdLookUps;

    static void setClass(JNIEnv * env, jclass cls) {
        (void)env; (void)cls;
    }

    static jclass getClass(JNIEnv * env) {
        return env->FindClass(C::class_name);
    }

    static ID_t getId(JNIEnv * env, jclass cls) {
        assert(cls != NULL);
        // multithreaded access ok, inaccurate if non-atomic increment
        nIdLookUps++;
        return jniGetMemberID< ID_t >(env, cls,
                                      C::member_name, C::member_descriptor);
    }
    
    static void releaseRef(JNIEnv * env, jclass cls) {
        env->DeleteLocalRef(cls);
    }
};

template< typename C >
struct MemberIdCache : MemberId< C > {
    typedef typename C::memberID_t ID_t;

    static ID_t getId(JNIEnv * env, jclass cls) {
        assert (cls != NULL);
        (void)env; (void)cls;
        return mid;
    }

protected:
    static jclass gClassRef;
    static ID_t mid;
};

template< typename C >
struct MemberIdWeakCache : MemberIdCache< C > {
    typedef MemberId< C > A;
    typedef MemberIdCache< C > Base;

    static void setClass(JNIEnv * env, jclass cls) {
        assert (cls != NULL);

        // multithreaded access ok, sets same class/member object
        Base::gClassRef = static_cast<jclass>(env->NewWeakGlobalRef(cls));
        Base::mid = A::getId(env, cls);
    }

    using Base::getId;
    
    static jclass getClass(JNIEnv * env) {
        jclass cls = static_cast<jclass>(env->NewLocalRef(Base::gClassRef));
        if (cls == NULL) {
            cls = A::getClass(env);
            setClass(env, cls);
        }
        return cls;
    }
    
    static void releaseRef(JNIEnv * env, jclass cls) {
        env->DeleteLocalRef(cls);
    }
};

template< typename C >
struct MemberIdStrongCache : MemberIdCache< C > {
    typedef MemberId< C > A;
    typedef MemberIdCache< C > Base;
    
    static void setClass(JNIEnv * env, jclass cls) {
        assert (cls != NULL);

        // multithreaded access ok, sets same class/member object
        Base::gClassRef = static_cast<jclass>(env->NewGlobalRef(cls));
        Base::mid = A::getId(env, cls);
    }

    using Base::getId;
    
    static jclass getClass(JNIEnv * env) {
        jclass cls = Base::gClassRef;
        if (cls == NULL) {
            cls = A::getClass(env);
            setClass(env, cls);
        }
        return cls;
    }
    
    static void releaseRef(JNIEnv * env, jclass cls) {
    }
};

template< typename C >
struct MemberIdPreloadedWeakCache : MemberIdWeakCache< C > {
    typedef MemberIdWeakCache< C > Base;
    
    using Base::setClass;

    using Base::getId;
    
    static jclass getClass(JNIEnv * env) {
        jclass cls = Base::gClassRef;
        assert (cls != NULL);
        return cls;
    }
    
    static void releaseRef(JNIEnv * env, jclass cls) {
    }
};

template< typename C >
struct MemberIdPreloadedStrongCache : MemberIdStrongCache< C > {
    typedef MemberIdStrongCache< C > Base;
    
    using Base::setClass;

    using Base::getId;
    
    static jclass getClass(JNIEnv * env) {
        jclass cls = Base::gClassRef;
        assert (cls != NULL);
        return cls;
    }    

    using Base::releaseRef;
};

// XXX test with multiple compilation units <-> jtie_lib.hpp

template< typename C > unsigned long MemberId< C >
    ::nIdLookUps = 0;

template< typename C > jclass MemberIdCache< C >
    ::gClassRef = NULL;

template< typename C > typename C::memberID_t MemberIdCache< C >
    ::mid = NULL;

// XXX document

enum JniMemberIdCaching {
    NO_CACHING,
    WEAK_CACHING,
    STRONG_CACHING,
    WEAK_CACHING_PRELOAD,
    STRONG_CACHING_PRELOAD
};

template< JniMemberIdCaching M, typename C >
struct JniMemberId;

template< typename C >
struct JniMemberId< NO_CACHING, C >
    : MemberId< C > {};

template< typename C >
struct JniMemberId< WEAK_CACHING, C >
    : MemberIdWeakCache< C > {};

template< typename C >
struct JniMemberId< STRONG_CACHING, C >
    : MemberIdStrongCache< C > {};

template< typename C >
struct JniMemberId< WEAK_CACHING_PRELOAD, C >
    : MemberIdPreloadedWeakCache< C > {};

template< typename C >
struct JniMemberId< STRONG_CACHING_PRELOAD, C >
    : MemberIdPreloadedStrongCache< C > {};

// ---------------------------------------------------------------------------

#endif // jtie_tconv_idcache_impl_hpp
