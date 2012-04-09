/* -*- mode: java; c-basic-offset: 4; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=4:tabstop=4:smarttab:
 *
 *  Copyright (c) 2010, 2011, Oracle and/or its affiliates. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <jni.h>
#include <cassert>

#include "helpers.hpp"

#include "com_mysql_cluster_crund_NdbApiLoad.h"

#include "CrundNdbApiOperations.hpp"

// ----------------------------------------------------------------------

// provides the benchmark's basic database operations
static CrundNdbApiOperations* ops = NULL;

// ----------------------------------------------------------------------

JNIEXPORT jint JNICALL
Java_com_mysql_cluster_crund_NdbApiLoad_ndbinit(JNIEnv* env,
                                                jclass cls,
                                                jstring mgmd_jstr)
{
    TRACE("ndbinit()");
    assert(mgmd_jstr);

    // location of cluster management server (ndb_mgmd)
    // get a native string from the Java string
    const char* mgmd_cstr = env->GetStringUTFChars(mgmd_jstr, NULL);

    // initialize the benchmark's resources
    ops = new CrundNdbApiOperations();
    ops->init(mgmd_cstr);

    // release the native and Java strings
    env->ReleaseStringUTFChars(mgmd_jstr, mgmd_cstr);

    return 0;
}

JNIEXPORT jint JNICALL
Java_com_mysql_cluster_crund_NdbApiLoad_ndbclose(JNIEnv* env,
                                                 jclass cls)
{
    TRACE("ndbclose()");

    // release the benchmark's resources
    ops->close();
    ops = NULL;

    return 0;
}

JNIEXPORT void JNICALL
Java_com_mysql_cluster_crund_NdbApiLoad_initConnection(JNIEnv* env,
                                                       jobject obj,
                                                       jstring catalog_jstr,
                                                       jstring schema_jstr,
                                                       jint defaultLockMode)
{
    TRACE("initConnection()");
    assert(catalog_jstr);
    assert(schema_jstr);

    // get native strings from the Java strings
    const char* catalog_cstr = env->GetStringUTFChars(catalog_jstr, NULL);
    const char* schema_cstr = env->GetStringUTFChars(schema_jstr, NULL);
    NdbOperation::LockMode dlm = (NdbOperation::LockMode)defaultLockMode;

    ops->initConnection(catalog_cstr, schema_cstr, dlm);

    // release the native and Java strings
    env->ReleaseStringUTFChars(schema_jstr, schema_cstr);
    env->ReleaseStringUTFChars(catalog_jstr, catalog_cstr);
}

JNIEXPORT void JNICALL
Java_com_mysql_cluster_crund_NdbApiLoad_closeConnection(JNIEnv* env,
                                                        jobject obj)
{
    TRACE("closeConnection()");
    ops->closeConnection();
}

JNIEXPORT void JNICALL
Java_com_mysql_cluster_crund_NdbApiLoad_clearData(JNIEnv* env,
                                                  jobject obj)
{
    TRACE("clearData()");
    ops->clearData();
}

JNIEXPORT void JNICALL
Java_com_mysql_cluster_crund_NdbApiLoad_delAllA(JNIEnv* env,
                                                jobject obj,
                                                jint nOps,
                                                jboolean batch)
{
    TRACE("delAllA()");
    int count;
    ops->delByScan(ops->model->table_A, count, batch == JNI_TRUE);
    assert(count == nOps);
}

JNIEXPORT void JNICALL
Java_com_mysql_cluster_crund_NdbApiLoad_delAllB0(JNIEnv* env,
                                                 jobject obj,
                                                 jint nOps,
                                                 jboolean batch)
{
    TRACE("delAllB0()");
    int count;
    ops->delByScan(ops->model->table_B0, count, batch == JNI_TRUE);
    assert(count == nOps);
}

JNIEXPORT void JNICALL
Java_com_mysql_cluster_crund_NdbApiLoad_insA(JNIEnv* env,
                                             jobject obj,
                                             jint nOps,
                                             jboolean setAttrs,
                                             jboolean batch)
{
    TRACE("insA()");
    ops->ins(ops->model->table_A, 1, nOps,
             setAttrs == JNI_TRUE, batch == JNI_TRUE);
}

JNIEXPORT void JNICALL
Java_com_mysql_cluster_crund_NdbApiLoad_insB0(JNIEnv* env,
                                              jobject obj,
                                              jint nOps,
                                              jboolean setAttrs,
                                              jboolean batch)
{
    TRACE("insB0()");
    ops->ins(ops->model->table_B0, 1, nOps,
             setAttrs == JNI_TRUE, batch == JNI_TRUE);
}

JNIEXPORT void JNICALL
Java_com_mysql_cluster_crund_NdbApiLoad_delAByPK(JNIEnv* env,
                                                 jobject obj,
                                                 jint nOps,
                                                 jboolean batch)
{
    TRACE("delAByPK()");
    ops->delByPK(ops->model->table_A, 1, nOps, batch == JNI_TRUE);
}

JNIEXPORT void JNICALL
Java_com_mysql_cluster_crund_NdbApiLoad_delB0ByPK(JNIEnv* env,
                                                  jobject obj,
                                                  jint nOps,
                                                  jboolean batch)
{
    TRACE("delB0ByPK()");
    ops->delByPK(ops->model->table_B0, 1, nOps, batch == JNI_TRUE);
}

JNIEXPORT void JNICALL
Java_com_mysql_cluster_crund_NdbApiLoad_setAByPK(JNIEnv* env,
                                                 jobject obj,
                                                 jint nOps,
                                                 jboolean batch)
{
    TRACE("setAByPK()");
    ops->setByPK(ops->model->table_A, 1, nOps, batch == JNI_TRUE);
}

JNIEXPORT void JNICALL
Java_com_mysql_cluster_crund_NdbApiLoad_setB0ByPK(JNIEnv* env,
                                                  jobject obj,
                                                  jint nOps,
                                                  jboolean batch)
{
    TRACE("setB0ByPK()");
    ops->setByPK(ops->model->table_B0, 1, nOps, batch == JNI_TRUE);
}

JNIEXPORT void JNICALL
Java_com_mysql_cluster_crund_NdbApiLoad_getAByPK_1bb(JNIEnv* env,
                                                     jobject obj,
                                                     jint nOps,
                                                     jboolean batch)
{
    TRACE("getAByPK_bb()");
    ops->getByPK_bb(ops->model->table_A, 1, nOps, batch == JNI_TRUE);
}

JNIEXPORT void JNICALL
Java_com_mysql_cluster_crund_NdbApiLoad_getB0ByPK_1bb(JNIEnv* env,
                                                      jobject obj,
                                                      jint nOps,
                                                      jboolean batch)
{
    TRACE("getB0ByPK_bb()");
    ops->getByPK_bb(ops->model->table_B0, 1, nOps, batch == JNI_TRUE);
}

JNIEXPORT void JNICALL
Java_com_mysql_cluster_crund_NdbApiLoad_getAByPK_1ar(JNIEnv* env,
                                                     jobject obj,
                                                     jint nOps,
                                                     jboolean batch)
{
    TRACE("getAByPK_ar()");
    ops->getByPK_ar(ops->model->table_A, 1, nOps, batch == JNI_TRUE);
}

JNIEXPORT void JNICALL
Java_com_mysql_cluster_crund_NdbApiLoad_getB0ByPK_1ar(JNIEnv* env,
                                                      jobject obj,
                                                      jint nOps,
                                                      jboolean batch)
{
    TRACE("getB0ByPK_ar()");
    ops->getByPK_ar(ops->model->table_B0, 1, nOps, batch == JNI_TRUE);
}

JNIEXPORT void JNICALL
Java_com_mysql_cluster_crund_NdbApiLoad_setVarbinary(JNIEnv* env,
                                                     jobject obj,
                                                     jint nOps,
                                                     jboolean batch,
                                                     jint length)
{
    TRACE("setVarbinary()");
    ops->setVarbinary(ops->model->table_B0,
                      1, nOps, batch == JNI_TRUE, length);
}

JNIEXPORT void JNICALL
Java_com_mysql_cluster_crund_NdbApiLoad_getVarbinary(JNIEnv* env,
                                                     jobject obj,
                                                     jint nOps,
                                                     jboolean batch,
                                                     jint length)
{
    TRACE("getVarbinary()");
    ops->getVarbinary(ops->model->table_B0,
                      1, nOps, batch == JNI_TRUE, length);
}

JNIEXPORT void JNICALL
Java_com_mysql_cluster_crund_NdbApiLoad_setVarchar(JNIEnv* env,
                                                   jobject obj,
                                                   jint nOps,
                                                   jboolean batch,
                                                   jint length)
{
    TRACE("setVarchar()");
    ops->setVarchar(ops->model->table_B0,
                    1, nOps, batch == JNI_TRUE, length);
}

JNIEXPORT void JNICALL
Java_com_mysql_cluster_crund_NdbApiLoad_getVarchar(JNIEnv* env,
                                                   jobject obj,
                                                   jint nOps,
                                                   jboolean batch,
                                                   jint length)
{
    TRACE("getVarchar()");
    ops->getVarchar(ops->model->table_B0,
                    1, nOps, batch == JNI_TRUE, length);
}

JNIEXPORT void JNICALL
Java_com_mysql_cluster_crund_NdbApiLoad_setB0ToA(JNIEnv* env,
                                                 jobject obj,
                                                 jint nOps,
                                                 jboolean batch)
{
    TRACE("setB0ToA()");
    ops->setB0ToA(nOps, batch == JNI_TRUE);
}

JNIEXPORT void JNICALL
Java_com_mysql_cluster_crund_NdbApiLoad_navB0ToA(JNIEnv* env,
                                                 jobject obj,
                                                 jint nOps,
                                                 jboolean batch)
{
    TRACE("navB0ToA()");
    ops->navB0ToA(nOps, batch == JNI_TRUE);
}

JNIEXPORT void JNICALL
Java_com_mysql_cluster_crund_NdbApiLoad_navB0ToAalt(JNIEnv* env,
                                                    jobject obj,
                                                    jint nOps,
                                                    jboolean batch)
{
    TRACE("navB0ToAalt()");
    ops->navB0ToAalt(nOps, batch == JNI_TRUE);
}

JNIEXPORT void JNICALL
Java_com_mysql_cluster_crund_NdbApiLoad_navAToB0(JNIEnv* env,
                                                 jobject obj,
                                                 jint nOps,
                                                 jboolean forceSend)
{
    TRACE("navAToB0()");
    ops->navAToB0(nOps, forceSend == JNI_TRUE);
}

JNIEXPORT void JNICALL
Java_com_mysql_cluster_crund_NdbApiLoad_navAToB0alt(JNIEnv* env,
                                                    jobject obj,
                                                    jint nOps,
                                                    jboolean forceSend)
{
    TRACE("navAToB0alt()");
    ops->navAToB0alt(nOps, forceSend == JNI_TRUE);
}

JNIEXPORT void JNICALL
Java_com_mysql_cluster_crund_NdbApiLoad_nullB0ToA(JNIEnv* env,
                                                  jobject obj,
                                                  jint nOps,
                                                  jboolean batch)
{
    TRACE("nullB0ToA()");
    ops->nullB0ToA(nOps, batch == JNI_TRUE);
}

// ----------------------------------------------------------------------
