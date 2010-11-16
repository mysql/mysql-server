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
 * ndbjtie_classes.cpp
 */

#include <stdint.h>
#include <jni.h>
#include "helpers.hpp"

#include <cstdio>

#include "jtie_ttrait.hpp"
#include "jtie_tconv.hpp"
#include "jtie_tconv_cvalue.hpp"
#include "jtie_tconv_cvalue_ext.hpp"
#include "jtie_tconv_carray.hpp"
#include "jtie_tconv_refbybb.hpp"
//#include "jtie_tconv_refbyval.hpp"
#include "jtie_tconv_cobject.hpp"
#include "jtie_tconv_cstring.hpp"
#include "jtie_gcalls.hpp"

#include "ndbjtie_Ndb_cluster_connection.h"
#include "ndbjtie_Ndb.h"
#include "ndbjtie_NdbError.h"
#include "ndbjtie_NdbDictionary.h"
#include "ndbjtie_NdbDictionary_Object.h"
#include "ndbjtie_NdbDictionary_Table.h"
#include "ndbjtie_NdbTransaction.h"
#include <NdbApi.hpp>
#include <NdbError.hpp>

// ---------------------------------------------------------------------------
// generatable, application-dependent code: NDB JTie function stubs
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Type definitions and conversions for Ndb_cluster_connection
// ---------------------------------------------------------------------------

// internal type for Ndb_cluster_connection
struct _c_s_m_n_Ndb_cluster_connection {
    static const char * const java_internal_class_name;
};
const char * const _c_s_m_n_Ndb_cluster_connection::java_internal_class_name = "ndbjtie/Ndb_cluster_connection";
typedef _c_s_m_n_Ndb_cluster_connection * c_s_m_n_Ndb_cluster_connection;

// formal <-> actual result type cast
template<>
inline jobject
cast< jobject, c_s_m_n_Ndb_cluster_connection >(c_s_m_n_Ndb_cluster_connection s) {
    TRACE("jobject cast(c_s_m_n_Ndb_cluster_connection)");
    return reinterpret_cast< jobject >(s);
}

// formal <-> actual parameter type cast
template<>
inline c_s_m_n_Ndb_cluster_connection
cast< c_s_m_n_Ndb_cluster_connection, jobject >(jobject s) {
    TRACE("c_s_m_n_Ndb_cluster_connection cast(jobject)");
    return reinterpret_cast< c_s_m_n_Ndb_cluster_connection >(s);
}

// type mapping aliases
typedef ttrait< jobject, Ndb_cluster_connection, jobject, Ndb_cluster_connection & > ttrait_Ndb_cluster_connection_target;
typedef ttrait< jobject, Ndb_cluster_connection *, c_s_m_n_Ndb_cluster_connection, Ndb_cluster_connection * > ttrait_Ndb_cluster_connection_ptr_return;
typedef ttrait< jobject, Ndb_cluster_connection &, c_s_m_n_Ndb_cluster_connection, Ndb_cluster_connection & > ttrait_Ndb_cluster_connection_ref_return;
typedef ttrait< jobject, Ndb_cluster_connection * > ttrait_Ndb_cluster_connection_ptr_arg;
typedef ttrait< jobject, Ndb_cluster_connection & > ttrait_Ndb_cluster_connection_ref_arg;

// constructor wrapper (return a reference for automatic Java exceptions)
Ndb_cluster_connection &
Ndb_cluster_connection_create(const char * p0) {
    TRACE("Ndb_cluster_connection & Ndb_cluster_connection_create(const char *)");
    Ndb_cluster_connection * r = new Ndb_cluster_connection(p0);
    printf("    r = %p\n", r);
    return *r;
};

// destructor wrapper (take a reference for automatic Java exceptions)
void
Ndb_cluster_connection_delete(Ndb_cluster_connection & p0) {
    TRACE("void Ndb_cluster_connection_delete(Ndb_cluster_connection &)");
    printf("    p0 = %p\n", &p0);
    delete &p0;
};

// ---------------------------------------------------------------------------
// Type definitions and conversions for Ndb
// ---------------------------------------------------------------------------

// internal type for Ndb
struct _c_s_m_n_Ndb {
    static const char * const java_internal_class_name;
};
const char * const _c_s_m_n_Ndb::java_internal_class_name = "ndbjtie/Ndb";
typedef _c_s_m_n_Ndb * c_s_m_n_Ndb;

// formal <-> actual result type cast
template<>
inline jobject
cast< jobject, c_s_m_n_Ndb >(c_s_m_n_Ndb s) {
    TRACE("jobject cast(c_s_m_n_Ndb)");
    return reinterpret_cast< jobject >(s);
}

// formal <-> actual parameter type cast
template<>
inline c_s_m_n_Ndb
cast< c_s_m_n_Ndb, jobject >(jobject s) {
    TRACE("c_s_m_n_Ndb cast(jobject)");
    return reinterpret_cast< c_s_m_n_Ndb >(s);
}

// type mapping aliases
typedef ttrait< jobject, Ndb, jobject, Ndb & > ttrait_Ndb_target;
typedef ttrait< jobject, Ndb *, c_s_m_n_Ndb, Ndb * > ttrait_Ndb_ptr_return;
typedef ttrait< jobject, Ndb &, c_s_m_n_Ndb, Ndb & > ttrait_Ndb_ref_return;
typedef ttrait< jobject, Ndb * > ttrait_Ndb_ptr_arg;
typedef ttrait< jobject, Ndb & > ttrait_Ndb_ref_arg;

// constructor wrapper (return a reference for automatic Java exceptions)
Ndb &
Ndb_create(Ndb_cluster_connection * p0, const char * p1, const char * p2) {
    TRACE("Ndb & Ndb_create(Ndb_cluster_connection *, const char *, const char *)");
    Ndb * r = new Ndb(p0, p1, p2);
    printf("    r = %p\n", r);
    return *r;
};

// destructor wrapper (take a reference for automatic Java exceptions)
void
Ndb_delete(Ndb & p0) {
    TRACE("void Ndb_delete(Ndb &)");
    printf("    p0 = %p\n", &p0);
    delete &p0;
};

// ---------------------------------------------------------------------------
// Type definitions and conversions for NdbError
// ---------------------------------------------------------------------------

// internal type for NdbError
struct _c_s_m_n_NdbError {
    static const char * const java_internal_class_name;
};
const char * const _c_s_m_n_NdbError::java_internal_class_name = "ndbjtie/NdbError";
typedef _c_s_m_n_NdbError * c_s_m_n_NdbError;

// formal <-> actual result type cast
template<>
inline jobject
cast< jobject, c_s_m_n_NdbError >(c_s_m_n_NdbError s) {
    TRACE("jobject cast(c_s_m_n_NdbError)");
    return reinterpret_cast< jobject >(s);
}

// formal <-> actual parameter type cast
template<>
inline c_s_m_n_NdbError
cast< c_s_m_n_NdbError, jobject >(jobject s) {
    TRACE("c_s_m_n_NdbError cast(jobject)");
    return reinterpret_cast< c_s_m_n_NdbError >(s);
}

// type mapping aliases
typedef ttrait< jobject, NdbError, jobject, NdbError & > ttrait_NdbError_target;
typedef ttrait< jobject, NdbError *, c_s_m_n_NdbError, NdbError * > ttrait_NdbError_ptr_return;
typedef ttrait< jobject, NdbError &, c_s_m_n_NdbError, NdbError & > ttrait_NdbError_ref_return;
typedef ttrait< jobject, NdbError * > ttrait_NdbError_ptr_arg;
typedef ttrait< jobject, NdbError & > ttrait_NdbError_ref_arg;

// new:
typedef ttrait< jobject, const NdbError &, c_s_m_n_NdbError, const NdbError & > ttrait_NdbError_cref_return;
// leads to ambuguities between
//#include "jtie_tconv_refbyval.hpp"
//#include "jtie_tconv_cobject.hpp"
//
//../jtie/jtie_gcalls.hpp:487: error: ambiguous class template instantiation for 'struct Result<_c_s_m_n_NdbError*, const NdbError&>'
//../jtie/jtie_tconv_refbyval.hpp:25: error: candidates are: struct Result<J, const C&>
//../jtie/jtie_tconv_refbyval.hpp:38: error:                 struct Result<J, C&>
//../jtie/jtie_tconv_cobject.hpp:157: error:                 struct Result<J*, C&>
//../jtie/jtie_gcalls.hpp:487: error: incomplete type 'Result<_c_s_m_n_NdbError*, const NdbError&>' used in nested name specifier


// ---------------------------------------------------------------------------
// Library Load and Unload Handlers
// ---------------------------------------------------------------------------

// useful for Threads to obtain their local JNIEnv
static JavaVM * cached_jvm;

// cache classes in a weak global ref to allow for them to be unloaded
//static jclass cls_jtie_Wrapper;
static jweak cls_jtie_Wrapper;

// cached method and field IDs
// XXX need to be recomputed after reloading of class?
static jmethodID mid_jtie_Wrapper_ctor;
static jmethodID mid_jtie_Wrapper_detach;
static jfieldID fid_jtie_Wrapper_cdelegate;

// initialize the NDB interface and cache method and field ids;
// called when the native library is loaded; returns the JNI version
// needed by the native library.
JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM * jvm, void * reserved)
{
    TRACE("jint JNI_OnLoad(JavaVM *, void *)");
    cout << "    loading the NDB JTie library..." << endl;

    // cache the JavaVM pointer
    cached_jvm = jvm;

    // get the JNI environment
    JNIEnv * env;
    if (jvm->GetEnv((void**)&env, JNI_VERSION_1_2) != JNI_OK) {
        return JNI_ERR; // unsupported version or thread not attached to VM
    }

    // find class
    jclass cls = env->FindClass("jtie/Wrapper");
    if (cls == NULL) {
        return JNI_ERR; // class not found or resource problem, error pending
    }

    // store class in a weak global ref to allow for class to be unloaded
    cls_jtie_Wrapper = env->NewWeakGlobalRef(cls);
    if (cls_jtie_Wrapper == NULL) {
        return JNI_ERR; // OutOfMemoryError pending
    }
    
    // retrieve and cache the method and field IDs
    mid_jtie_Wrapper_ctor = env->GetMethodID(cls, "<init>", "(J)V");
    if (mid_jtie_Wrapper_ctor == NULL) {
        return JNI_ERR; // method found or resource problem, error pending
    }
    mid_jtie_Wrapper_detach = env->GetMethodID(cls, "detach", "()V");
    if (mid_jtie_Wrapper_detach == NULL) {
        return JNI_ERR; // method found or resource problem, error pending
    }
    fid_jtie_Wrapper_cdelegate = env->GetFieldID(cls, "cdelegate", "J");
    if (fid_jtie_Wrapper_cdelegate == NULL) {
        return JNI_ERR; // method found or resource problem, error pending
    }

    // XXX better call ndb_init() from main()?
    cout << "    initializing NDBAPI..." << endl;
    int stat = ndb_init();
    if (stat != 0) {
        //ABORT_ERROR("ndb_init() returned: " << stat);
        cout << endl << "!!! error in " << __FILE__ << ", line: " << __LINE__
             << ", msg: ndb_init() returned: " << stat;
        return JNI_ERR;
    }
    cout << "    [ok]" << endl;

    cout << "    [loaded]" << endl;

    // as a precaution, we assume using JNI functions introduced in JDK 1.4
    //return JNI_VERSION_1_2;
    return JNI_VERSION_1_4;
}

// called when the class loader containing the native library is garbage
// collected; called in an unknown context (such as from a finalizer):
// be conservative, and refrain from arbitrary Java call-backs.
JNIEXPORT void JNICALL
JNI_OnUnload(JavaVM * jvm, void * reserved)
{
    TRACE("void JNI_OnUnload(JavaVM *, void *)");
    cout << "    unloading the NDB JTie library..." << endl;

    // XXX better call ndb_end() from main()?
    cout << "    closing NDBAPI ...   " << endl;
    ndb_end(0);
    cout << "    [ok]" << endl;

    // get the JNI environment
    JNIEnv * env;
    if (jvm->GetEnv((void**)&env, JNI_VERSION_1_2) != JNI_OK) {
        return; // unsupported version or thread not attached to VM
    }

    // delete cached resources
    fid_jtie_Wrapper_cdelegate = NULL;
    mid_jtie_Wrapper_detach = NULL;
    mid_jtie_Wrapper_ctor = NULL;
    env->NewWeakGlobalRef(cls_jtie_Wrapper);
    cached_jvm = NULL; // probably, not needed

    cout << "    [unloaded]" << endl;
}


// ---------------------------------------------------------------------------
// Function Stubs for Ndb_cluster_connection
// ---------------------------------------------------------------------------

JNIEXPORT jobject JNICALL
Java_ndbjtie_Ndb_1cluster_1connection_create(JNIEnv * env, jclass cls, jstring p0)
{
    TRACE("jobject Java_ndbjtie_Ndb_1cluster_1connection_create(JNIEnv *, jclass, jstring)");
    return gcreate< ttrait_Ndb_cluster_connection_ref_return, ttrait_cstring, Ndb_cluster_connection_create >(env, p0);
}

JNIEXPORT void JNICALL
Java_ndbjtie_Ndb_1cluster_1connection_delete(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_ndbjtie_Ndb_1cluster_1connection_delete(JNIEnv *, jclass, jobject)");
    gdelete< ttrait_Ndb_cluster_connection_ref_arg, Ndb_cluster_connection_delete >(env, p0);
}

JNIEXPORT jint JNICALL
Java_ndbjtie_Ndb_1cluster_1connection_connect(JNIEnv * env, jobject obj, jint p0, jint p1, jint p2)
{
    TRACE("jint Java_ndbjtie_Ndb_1cluster_1connection_connect(JNIEnv *, jobject, jint, jint, jint)");
    return gcall<  ttrait_Ndb_cluster_connection_target, ttrait_int, ttrait_int, ttrait_int, ttrait_int, &Ndb_cluster_connection::connect >(env, obj, p0, p1, p2);
}

JNIEXPORT jint JNICALL
Java_ndbjtie_Ndb_1cluster_1connection_wait_1until_1ready(JNIEnv * env, jobject obj, jint p0, jint p1)
{
    TRACE("jint Java_ndbjtie_Ndb_1cluster_1connection_wait_1until_1ready(JNIEnv *, jobject, jint, jint)");
    return gcall< ttrait_Ndb_cluster_connection_target, ttrait_int, ttrait_int, ttrait_int, &Ndb_cluster_connection::wait_until_ready >(env, obj, p0, p1);
}

// ---------------------------------------------------------------------------
// Function Stubs for Ndb
// ---------------------------------------------------------------------------

JNIEXPORT jobject JNICALL
Java_ndbjtie_Ndb_create(JNIEnv * env, jclass cls, jobject p0, jstring p1, jstring p2)
{
    TRACE("jobject Java_ndbjtie_Ndb_create(JNIEnv *, jclass, jstring)");
    return gcreate< ttrait_Ndb_ref_return, ttrait_Ndb_cluster_connection_ptr_arg, ttrait_cstring, ttrait_cstring, Ndb_create >(env, p0, p1, p2);
}

JNIEXPORT void JNICALL
Java_ndbjtie_Ndb_delete(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_ndbjtie_Ndb_delete(JNIEnv *, jclass, jobject)");
    gdelete< ttrait_Ndb_ref_arg, Ndb_delete >(env, p0);
}

JNIEXPORT jint JNICALL
Java_ndbjtie_Ndb_init(JNIEnv * env, jclass cls, jobject obj, jint p0)
{
    TRACE("jint Java_ndbjtie_Ndb_init(JNIEnv *, jclass, jobject, jint)");
    return gcall< ttrait_Ndb_target, ttrait_int, ttrait_int, &Ndb::init >(env, obj, p0);
}

JNIEXPORT jobject JNICALL
Java_ndbjtie_Ndb_getNdbError__Lndbjtie_Ndb_2(JNIEnv * env, jclass cls, jobject obj)
{
    TRACE("jobject Java_ndbjtie_Ndb_getNdbError__Lndbjtie_Ndb_2(JNIEnv *, jclass, jobject)");
    return gcall< ttrait_Ndb_target, ttrait_NdbError_cref_return, &Ndb::getNdbError >(env, obj);
}

JNIEXPORT jobject JNICALL
Java_ndbjtie_Ndb_getNdbError__Lndbjtie_Ndb_2I(JNIEnv * env, jclass cls, jobject obj, jint p0)
{
    TRACE("jobject Java_ndbjtie_Ndb_getNdbError__Lndbjtie_Ndb_2I(JNIEnv *, jclass, jobject, jint)");
    return gcall< ttrait_Ndb_target, ttrait_NdbError_cref_return, ttrait_int, &Ndb::getNdbError >(env, obj, p0);
}

JNIEXPORT jobject JNICALL
Java_ndbjtie_Ndb_startTransaction__Lndbjtie_NdbDictionary_Table_2CI
  (JNIEnv *, jobject, jobject, jchar, jint);

JNIEXPORT jobject JNICALL
Java_ndbjtie_Ndb_startTransaction__Lndbjtie_NdbDictionary_Table_2I
  (JNIEnv *, jobject, jobject, jint);

JNIEXPORT void JNICALL
Java_ndbjtie_Ndb_closeTransaction
  (JNIEnv *, jobject, jobject);

// ---------------------------------------------------------------------------
// Function Stubs for NdbDictionary
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
