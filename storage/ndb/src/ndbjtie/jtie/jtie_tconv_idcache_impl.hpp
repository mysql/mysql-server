/*
 Copyright 2010 Sun Microsystems, Inc.
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

// Local JNI helper functions

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
#define JTIE_INSTANTIATE_CLASS_MEMBER_INFO( T, CN, MN, MD )             \
    const char * const T::class_name = CN;                              \
    const char * const T::member_name = MN;                             \
    const char * const T::member_descriptor = MD;                       \
    template struct MemberId< T >;                                      \
    template struct MemberIdCache< T >;

/**
 * Provides uniform access to the JNI Field/Method ID of a Java class member
 * as described by the member info type 'C'.
 *
 * This base class does not cache the member ID and the class object, but
 * it retrieves the member ID from JNI upon each access; different caching
 * strategies are provided by derived classes.
 *
 * This class (and its derived classes) impose a strict usage pattern.
 * For example, given definitions...
 *
 *    // Defines the field info type for MyClass.myField.
 *    JTIE_DEFINE_FIELD_MEMBER_INFO(_MyClass_myField)
 *
 *    // Provides a (cached) access to field Id of MyClass.myField.
 *    typedef JniMemberId< _MyClass_myField > MyClass_myField;
 *
 * any use of a member ID must be bracketed by getClass() and releaseRef():
 *
 *    // obtain a class reference
 *    jclass cls = MyClass_myField::getClass(env);
 *    if (cls == NULL) {
 *        // exception pending
 *    } else {
 *        // get the field ID valid along with the class reference
 *        jfieldID fid = MyClass_myField::getId(env, cls);
 *        if (fid == NULL) {
 *            // exception pending
 *        } else {
 *            // OK to access field using 'fid'
 *        }
 *        // allow for releasing the class reference
 *        MyClass_myField::releaseRef(env, cls);
 *    }
 *
 * Derived classes implement any caching underneath this usage pattern.
 */
template< typename C >
struct MemberId {
    typedef typename C::memberID_t ID_t;

    // number of JNI Get<Field|Method>ID() invocations for statistics
    static unsigned long nIdLookUps;

    /**
     * Allows for storing a (global) class reference.
     *
     * Usually only called from getClass(), but enables "cache preloading"
     * from a (native, static) function called at class initialization.
     *
     * Pre condition:
     * - this thread has no pending JNI exception (!env->ExceptionCheck())
     * - a valid local or global class reference: assert(cls != NULL)
     */
    static void setClass(JNIEnv * env, jclass cls) {
        assert(cls != NULL);
        (void)env; (void)cls;
    }

    /**
     * Returns a JNI Reference to the class declaring the member specified
     * by info type 'C'.
     *
     * Depending upon the underlying caching strategy, a returned reference
     * may be local or global, weak or strong; the scope of its use must be
     * demarcated by releaseRef().
     *
     * Pre condition:
     * - this thread has no pending JNI exception (!env->ExceptionCheck())
     *
     * Post condition:
     * - return value is
     *   NULL:
     *     - this thread has a pending JNI exception (env->ExceptionCheck())
     *   otherwise:
     *     - this thread has no pending JNI exception (!env->ExceptionCheck())
     *     - the returned reference is valid (at least) until releaseRef()
     */
    static jclass getClass(JNIEnv * env) {
        assert(env->ExceptionCheck() == JNI_OK);
        jclass cls = env->FindClass(C::class_name);
        if (cls == NULL) { // break out for better diagnostics
            assert(env->ExceptionCheck() != JNI_OK); // exception pending

//#ifndef NDEBUG // XXX for debugging
            // print error diagnostics
            char m[256];
#ifndef _WIN32
            snprintf(m, 256, "JTie: failed to find Java class '%s'\n",
#else
            _snprintf(m, 256, "JTie: failed to find Java class '%s'\n",
#endif
                     (C::class_name == NULL ? "NULL" : C::class_name));
            fprintf(stderr, m);
            env->ExceptionDescribe();
            env->FatalError(m); // XXX for debugging
//#endif // NDEBUG
        } else {
            assert(env->ExceptionCheck() == JNI_OK); // ok
        }
        return cls;
    }

    /**
     * Returns the JNI Field/Method ID of a Java class member.
     *
     * The member ID is only valid along with a class object obtained by
     * getClass() and before releaseRef().
     *
     * Pre condition:
     * - this thread has no pending JNI exception (!env->ExceptionCheck())
     * - a valid class reference obtained by getClass(): assert(cls != NULL)
     *
     * Post condition:
     * - return value is
     *   NULL:
     *     - this thread has a pending JNI exception (env->ExceptionCheck())
     *   otherwise:
     *     - this thread has no pending JNI exception (!env->ExceptionCheck())
     *     - the returned member ID is valid (at least) until releaseRef()
     */
    static ID_t getId(JNIEnv * env, jclass cls) {
        assert(cls != NULL);
        // multithreaded access ok, inaccurate if non-atomic increment
        nIdLookUps++;
        return jniGetMemberID< ID_t >(env, cls,
                                      C::member_name, C::member_descriptor);
    }

    /**
     * Allows for a class reference to be released along with any member IDs.
     *
     * Pre condition:
     * - a valid class reference obtained by getClass(): assert(cls != NULL)
     * - this thread may have a pending JNI exception (env->ExceptionCheck())
     */
    static void releaseRef(JNIEnv * env, jclass cls) {
        assert(cls != NULL);
        env->DeleteLocalRef(cls);
    }
};

/**
 * Base class for caching of JNI Field/Method IDs.
 */
template< typename C >
struct MemberIdCache : MemberId< C > {
    typedef typename C::memberID_t ID_t;

    static ID_t getId(JNIEnv * env, jclass cls) {
        assert(cls != NULL);
        // the cached member id is only valid along with global class ref
        assert(env->IsSameObject(gClassRef, NULL) == JNI_FALSE);
        (void)env; (void)cls;
        return mid;
    }

protected:
    // the cached global (weak or strong) class ref
    static jclass gClassRef;

    // the cached member id (only valid along with global class ref)
    static ID_t mid;
};

/**
 * Provides caching of JNI Field/Method IDs using weak class references,
 * allowing classes to be unloaded when no longer used by Java code.
 */
template< typename C >
struct MemberIdWeakCache : MemberIdCache< C > {
    typedef MemberId< C > A;
    typedef MemberIdCache< C > Base;

    static void setClass(JNIEnv * env, jclass cls) {
        assert(cls != NULL);

        // multithreaded access ok, sets same class/member object
        Base::gClassRef = static_cast< jclass >(env->NewWeakGlobalRef(cls));
        Base::mid = A::getId(env, cls);
    }

    using Base::getId; // use as inherited (some compiler wanted this)

    static jclass getClass(JNIEnv * env) {
        // a weak global class ref may refer to a freed object at any time
        //   (i.e.: env->IsSameObject(Base::gClassRef, NULL))
        // unless we've obtained a strong (local or global) non-NULL class ref
        jclass cls = static_cast< jclass >(env->NewLocalRef(Base::gClassRef));
        if (cls == NULL) {
            // global class ref was NULL or referencing a freed object
            cls = A::getClass(env);
            if (cls == NULL) {
                // exception pending
            } else {
                setClass(env, cls);
            }
        }
        return cls;
    }

    static void releaseRef(JNIEnv * env, jclass cls) {
        assert(cls != NULL);
        env->DeleteLocalRef(cls);
    }
};

/**
 * Provides caching of JNI Field/Method IDs using strong class references,
 * preventing classes from being unloaded even if no longer used by Java code.
 */
template< typename C >
struct MemberIdStrongCache : MemberIdCache< C > {
    typedef MemberId< C > A;
    typedef MemberIdCache< C > Base;

    static void setClass(JNIEnv * env, jclass cls) {
        assert(cls != NULL);

        // multithreaded access ok, sets same class/member object
        Base::gClassRef = static_cast< jclass >(env->NewGlobalRef(cls));
        Base::mid = A::getId(env, cls);
    }

    using Base::getId; // use as inherited (some compiler wanted this)

    static jclass getClass(JNIEnv * env) {
        jclass cls = Base::gClassRef;
        if (cls == NULL) {
            cls = A::getClass(env);
            if (cls == NULL) {
                // exception pending
            } else {
                setClass(env, cls);
            }
        }
        return cls;
    }

    static void releaseRef(JNIEnv * env, jclass cls) {
        assert(cls != NULL);
        (void)env; (void)cls;
    }
};

/**
 * Provides caching of JNI Field/Method IDs using weak class references
 * with preloading (at class initialization) -- VERY TRICKY, NOT SUPPORTED.
 */
template< typename C >
struct MemberIdPreloadedWeakCache : MemberIdWeakCache< C > {
    typedef MemberIdWeakCache< C > Base;

    using Base::setClass; // use as inherited (some compiler wanted this)

    using Base::getId; // use as inherited (some compiler wanted this)

    static jclass getClass(JNIEnv * env) {
        // weak global class ref is assumed to be preloaded and valid
        jclass cls = Base::gClassRef;
        assert(env->IsSameObject(cls, NULL) == JNI_FALSE);
        return cls;
    }

    static void releaseRef(JNIEnv * env, jclass cls) {
        assert(cls != NULL);
        (void)env; (void)cls;
    }
};

/**
 * Provides caching of JNI Field/Method IDs using strong class references
 * with preloading (at class initialization) -- VERY TRICKY, NOT SUPPORTED.
 */
template< typename C >
struct MemberIdPreloadedStrongCache : MemberIdStrongCache< C > {
    typedef MemberIdStrongCache< C > Base;

    using Base::setClass; // use as inherited (some compiler wanted this)

    using Base::getId; // use as inherited (some compiler wanted this)

    static jclass getClass(JNIEnv * env) {
        // strong global class ref is assumed to be preloaded and valid
        jclass cls = Base::gClassRef;
        assert(env->IsSameObject(cls, NULL) == JNI_FALSE);
        return cls;
    }

    using Base::releaseRef; // use as inherited (some compiler wanted this)
};

// XXX static initialization <-> multiple compilation units <-> jtie_lib.hpp
template< typename C > unsigned long MemberId< C >
    ::nIdLookUps = 0;

template< typename C > jclass MemberIdCache< C >
    ::gClassRef = NULL;

template< typename C > typename C::memberID_t MemberIdCache< C >
    ::mid = NULL;

// XXX document

/**
 * The supported caching strategies for member IDs and class references.
 */
enum JniMemberIdCaching {
    NO_CACHING
    ,WEAK_CACHING
    ,STRONG_CACHING
#if 0 // preloaded caching very tricky, not supported at this time
    ,WEAK_CACHING_PRELOAD
    ,STRONG_CACHING_PRELOAD
#endif // preloaded caching very tricky, not supported at this time
};

/**
 * Generic class for member ID access with selection of caching strategy.
 */
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

#if 0 // preloaded caching very tricky, not supported at this time
template< typename C >
struct JniMemberId< WEAK_CACHING_PRELOAD, C >
    : MemberIdPreloadedWeakCache< C > {};

template< typename C >
struct JniMemberId< STRONG_CACHING_PRELOAD, C >
    : MemberIdPreloadedStrongCache< C > {};
#endif // preloaded caching very tricky, not supported at this time

// ---------------------------------------------------------------------------

#endif // jtie_tconv_idcache_impl_hpp
