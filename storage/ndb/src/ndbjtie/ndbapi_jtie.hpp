/*
 Copyright (c) 2010, 2022, Oracle and/or its affiliates.

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
 * ndbapi_jtie.hpp
 */

#ifndef ndbapi_jtie_hpp
#define ndbapi_jtie_hpp

// API to implement against
#include "NdbApi.hpp"
#include "NdbError.hpp"
#include "NdbApiWrapper.hpp"

// libraries
#include "ndbjtie_defs.hpp"
#include "helpers.hpp"
#include "jtie.hpp"

// ---------------------------------------------------------------------------
// NDBAPI JTie Type Definitions
// ---------------------------------------------------------------------------

JTIE_DEFINE_PEER_CLASS_MAPPING(Ndb,
                               c_m_n_n_Ndb)
JTIE_DEFINE_PEER_CLASS_MAPPING(NdbBlob,
                               c_m_n_n_NdbBlob)
JTIE_DEFINE_PEER_CLASS_MAPPING(NdbDictionary,
                               c_m_n_n_NdbDictionary)
JTIE_DEFINE_PEER_CLASS_MAPPING(NdbDictionary::AutoGrowSpecification,
                               c_m_n_n_NdbDictionary_AutoGrowSpecification)
JTIE_DEFINE_PEER_CLASS_MAPPING(NdbDictionary::Column,
                               c_m_n_n_NdbDictionary_Column)
JTIE_DEFINE_PEER_CLASS_MAPPING(NdbDictionary::Datafile,
                               c_m_n_n_NdbDictionary_Datafile)
JTIE_DEFINE_PEER_CLASS_MAPPING(NdbDictionary::Dictionary,
                               c_m_n_n_NdbDictionary_Dictionary)
JTIE_DEFINE_PEER_CLASS_MAPPING(NdbDictionary::Dictionary::List,
                               c_m_n_n_NdbDictionary_DictionaryConst_List)
JTIE_DEFINE_PEER_CLASS_MAPPING(NdbDictionary::Dictionary::List::Element,
                               c_m_n_n_NdbDictionary_DictionaryConst_ListConst_Element)
JTIE_DEFINE_PEER_CLASS_MAPPING(NdbDictionary::Dictionary::List::Element,
                               c_m_n_n_NdbDictionary_DictionaryConst_ListConst_ElementArray)
JTIE_DEFINE_PEER_CLASS_MAPPING(NdbDictionary::Event,
                               c_m_n_n_NdbDictionary_Event)
JTIE_DEFINE_PEER_CLASS_MAPPING(NdbDictionary::Index,
                               c_m_n_n_NdbDictionary_Index)
JTIE_DEFINE_PEER_CLASS_MAPPING(NdbDictionary::LogfileGroup,
                               c_m_n_n_NdbDictionary_LogfileGroup)
JTIE_DEFINE_PEER_CLASS_MAPPING(NdbDictionary::Object,
                               c_m_n_n_NdbDictionary_Object)
JTIE_DEFINE_PEER_CLASS_MAPPING(NdbDictionary::ObjectId,
                               c_m_n_n_NdbDictionary_ObjectId)
JTIE_DEFINE_PEER_CLASS_MAPPING(NdbDictionary::OptimizeIndexHandle,
                               c_m_n_n_NdbDictionary_OptimizeIndexHandle)
JTIE_DEFINE_PEER_CLASS_MAPPING(NdbDictionary::OptimizeTableHandle,
                               c_m_n_n_NdbDictionary_OptimizeTableHandle)
JTIE_DEFINE_PEER_CLASS_MAPPING(NdbDictionary::RecordSpecification,
                               c_m_n_n_NdbDictionary_RecordSpecification)
JTIE_DEFINE_PEER_CLASS_MAPPING(NdbDictionary::RecordSpecification,
                               c_m_n_n_NdbDictionary_RecordSpecificationArray)
JTIE_DEFINE_PEER_CLASS_MAPPING(NdbDictionary::Table,
                               c_m_n_n_NdbDictionary_Table)
JTIE_DEFINE_PEER_CLASS_MAPPING(NdbDictionary::Tablespace,
                               c_m_n_n_NdbDictionary_Tablespace)
JTIE_DEFINE_PEER_CLASS_MAPPING(NdbDictionary::Undofile,
                               c_m_n_n_NdbDictionary_Undofile)
JTIE_DEFINE_PEER_CLASS_MAPPING(NdbError,
                               c_m_n_n_NdbError)
JTIE_DEFINE_PEER_CLASS_MAPPING(NdbEventOperation,
                               c_m_n_n_NdbEventOperation)
JTIE_DEFINE_PEER_CLASS_MAPPING(NdbIndexOperation,
                               c_m_n_n_NdbIndexOperation)
JTIE_DEFINE_PEER_CLASS_MAPPING(NdbIndexScanOperation,
                               c_m_n_n_NdbIndexScanOperation)
JTIE_DEFINE_PEER_CLASS_MAPPING(NdbIndexScanOperation::IndexBound,
                               c_m_n_n_NdbIndexScanOperation_IndexBound)
JTIE_DEFINE_PEER_CLASS_MAPPING(NdbInterpretedCode,
                               c_m_n_n_NdbInterpretedCode)
JTIE_DEFINE_PEER_CLASS_MAPPING(NdbLockHandle,
                               c_m_n_n_NdbLockHandle)
JTIE_DEFINE_PEER_CLASS_MAPPING(NdbOperation,
                               c_m_n_n_NdbOperation)
JTIE_DEFINE_PEER_CLASS_MAPPING(NdbOperation::GetValueSpec,
                               c_m_n_n_NdbOperation_GetValueSpec)
JTIE_DEFINE_PEER_CLASS_MAPPING(NdbOperation::GetValueSpec,
                               c_m_n_n_NdbOperation_GetValueSpecArray)
JTIE_DEFINE_PEER_CLASS_MAPPING(NdbOperation::OperationOptions,
                               c_m_n_n_NdbOperation_OperationOptions)
JTIE_DEFINE_PEER_CLASS_MAPPING(NdbOperation::SetValueSpec,
                               c_m_n_n_NdbOperation_SetValueSpec)
JTIE_DEFINE_PEER_CLASS_MAPPING(NdbOperation::SetValueSpec,
                               c_m_n_n_NdbOperation_SetValueSpecArray)
JTIE_DEFINE_PEER_CLASS_MAPPING(NdbRecAttr,
                               c_m_n_n_NdbRecAttr)
JTIE_DEFINE_PEER_CLASS_MAPPING(NdbRecord,
                               c_m_n_n_NdbRecord)
JTIE_DEFINE_PEER_CLASS_MAPPING(NdbScanFilter,
                               c_m_n_n_NdbScanFilter)
JTIE_DEFINE_PEER_CLASS_MAPPING(NdbScanOperation,
                               c_m_n_n_NdbScanOperation)
JTIE_DEFINE_PEER_CLASS_MAPPING(NdbScanOperation::ScanOptions,
                               c_m_n_n_NdbScanOperation_ScanOptions)
JTIE_DEFINE_PEER_CLASS_MAPPING(NdbTransaction,
                               c_m_n_n_NdbTransaction)
JTIE_DEFINE_PEER_CLASS_MAPPING(Ndb::Key_part_ptr,
                               c_m_n_n_Ndb_Key_part_ptr)
JTIE_DEFINE_PEER_CLASS_MAPPING(Ndb::Key_part_ptr,
                               c_m_n_n_Ndb_Key_part_ptrArray)
JTIE_DEFINE_PEER_CLASS_MAPPING(Ndb::PartitionSpec,
                               c_m_n_n_Ndb_PartitionSpec)
JTIE_DEFINE_PEER_CLASS_MAPPING(Ndb_cluster_connection,
                               c_m_n_n_Ndb_cluster_connection)

// ---------------------------------------------------------------------------

JTIE_DEFINE_JINT_ENUM_TYPE_MAPPING(NdbBlob::State,
                                   c_m_n_n_NdbBlob_State)
JTIE_DEFINE_JINT_ENUM_TYPE_MAPPING(NdbDictionary::Object::Status,
                                   c_m_n_n_NdbDictionary_Object_Status)
JTIE_DEFINE_JINT_ENUM_TYPE_MAPPING(NdbDictionary::Object::Type,
                                   c_m_n_n_NdbDictionary_Object_Type)
JTIE_DEFINE_JINT_ENUM_TYPE_MAPPING(NdbDictionary::Object::State,
                                   c_m_n_n_NdbDictionary_Object_State)
JTIE_DEFINE_JINT_ENUM_TYPE_MAPPING(NdbDictionary::Object::Store,
                                   c_m_n_n_NdbDictionary_Object_Store)
JTIE_DEFINE_JINT_ENUM_TYPE_MAPPING(NdbDictionary::Object::FragmentType,
                                   c_m_n_n_NdbDictionary_Object_FragmentType)
JTIE_DEFINE_JINT_ENUM_TYPE_MAPPING(NdbDictionary::Column::Type,
                                   c_m_n_n_NdbDictionary_Column_Type)
JTIE_DEFINE_JINT_ENUM_TYPE_MAPPING(NdbDictionary::Column::ArrayType,
                                   c_m_n_n_NdbDictionary_Column_ArrayType)
JTIE_DEFINE_JINT_ENUM_TYPE_MAPPING(NdbDictionary::Column::StorageType,
                                   c_m_n_n_NdbDictionary_Column_StorageType)
JTIE_DEFINE_JINT_ENUM_TYPE_MAPPING(NdbDictionary::Table::SingleUserMode,
                                   c_m_n_n_NdbDictionary_Table_SingleUserMode)
JTIE_DEFINE_JINT_ENUM_TYPE_MAPPING(NdbDictionary::Index::Type,
                                   c_m_n_n_NdbDictionary_Index_Type)
JTIE_DEFINE_JINT_ENUM_TYPE_MAPPING(NdbDictionary::Event::TableEvent,
                                   c_m_n_n_NdbDictionary_Event_TableEvent)
JTIE_DEFINE_JINT_ENUM_TYPE_MAPPING(NdbDictionary::Event::EventDurability,
                                   c_m_n_n_NdbDictionary_Event_EventDurability)
JTIE_DEFINE_JINT_ENUM_TYPE_MAPPING(NdbDictionary::Event::EventReport,
                                   c_m_n_n_NdbDictionary_Event_EventReport)
JTIE_DEFINE_JINT_ENUM_TYPE_MAPPING(NdbDictionary::NdbRecordFlags,
                                   c_m_n_n_NdbDictionary_NdbRecordFlags)
JTIE_DEFINE_JINT_ENUM_TYPE_MAPPING(NdbDictionary::RecordType,
                                   c_m_n_n_NdbDictionary_RecordType)
JTIE_DEFINE_JINT_ENUM_TYPE_MAPPING(NdbError::Status,
                                   c_m_n_n_NdbError_Status)
JTIE_DEFINE_JINT_ENUM_TYPE_MAPPING(NdbError::Classification,
                                   c_m_n_n_NdbError_Classification)
JTIE_DEFINE_JINT_ENUM_TYPE_MAPPING(NdbEventOperation::State,
                                   c_m_n_n_NdbEventOperation_State)
JTIE_DEFINE_JINT_ENUM_TYPE_MAPPING(NdbIndexScanOperation::BoundType,
                                   c_m_n_n_NdbIndexScanOperation_BoundType)
JTIE_DEFINE_JINT_ENUM_TYPE_MAPPING(NdbOperation::Type,
                                   c_m_n_n_NdbOperation_Type)
JTIE_DEFINE_JINT_ENUM_TYPE_MAPPING(NdbOperation::LockMode,
                                   c_m_n_n_NdbOperation_LockMode)
JTIE_DEFINE_JINT_ENUM_TYPE_MAPPING(NdbOperation::AbortOption,
                                   c_m_n_n_NdbOperation_AbortOption)
JTIE_DEFINE_JINT_ENUM_TYPE_MAPPING(NdbOperation::OperationOptions::Flags,
                                   c_m_n_n_NdbOperation_OperationOptions_Flags)
JTIE_DEFINE_JINT_ENUM_TYPE_MAPPING(NdbScanFilter::Group,
                                   c_m_n_n_NdbScanFilter_Group)
JTIE_DEFINE_JINT_ENUM_TYPE_MAPPING(NdbScanFilter::BinaryCondition,
                                   c_m_n_n_NdbScanFilter_BinaryCondition)
JTIE_DEFINE_JINT_ENUM_TYPE_MAPPING(NdbScanFilter::Error,
                                   c_m_n_n_NdbScanFilter_Error)
JTIE_DEFINE_JINT_ENUM_TYPE_MAPPING(NdbScanOperation::ScanFlag,
                                   c_m_n_n_NdbScanOperation_ScanFlag)
JTIE_DEFINE_JINT_ENUM_TYPE_MAPPING(NdbScanOperation::ScanOptions::Type,
                                   c_m_n_n_NdbScanOperation_ScanOptions_Type)
JTIE_DEFINE_JINT_ENUM_TYPE_MAPPING(NdbTransaction::ExecType,
                                   c_m_n_n_NdbTransaction_ExecType)
JTIE_DEFINE_JINT_ENUM_TYPE_MAPPING(NdbTransaction::CommitStatusType,
                                   c_m_n_n_NdbTransaction_CommitStatusType)

// ---------------------------------------------------------------------------
// NDBAPI JTie Function Stubs
// ---------------------------------------------------------------------------

// The API stub functions in this file have mangled names that adhere
// to the JVM specification.  It is not necessary to include the
// function prototypes generated by the javah tool from the Java source,
// if they are declared to receive "C" linkage here.
extern "C" {

// A javah bug in JDK 5
//   http://forums.sun.com/thread.jspa?threadID=5115982&tstart=1499
// generates a wrong name for native methods in static nested classes:
//
// JDK 6 has this bug only partially fixed (nested classes as invocation
// targets but not as parameters).
//
// Outer$Inner is to be mangled as unicode escape: Outer_00024Inner, see:
//   http://java.sun.com/javase/6/docs/technotes/guides/jni/spec/design.html#wp615]Resolving%20Native%20Method%20Names

// ---------------------------------------------------------------------------

//#include "com_mysql_ndbjtie_ndbapi_NDBAPI.h"

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NDBAPI
 * Method:    create_instance
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/Ndb_cluster_connection;III)Z
 */
JNIEXPORT jboolean JNICALL
Java_com_mysql_ndbjtie_ndbapi_NDBAPI_create_1instance(JNIEnv * env, jclass cls, jobject p0, jint p1, jint p2, jint p3)
{
    TRACE("jboolean Java_com_mysql_ndbjtie_ndbapi_NDBAPI_create_1instance(JNIEnv *, jclass, jobject, jint, jint, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_fr< ttrait_bool, ttrait_c_m_n_n_Ndb_cluster_connection_p, ttrait_Uint32, ttrait_Uint32, ttrait_Uint32, &::create_instance >(env, cls, p0, p1, p2, p3);
#else
    return gcall_fr< ttrait_bool, ttrait_c_m_n_n_Ndb_cluster_connection_p, ttrait_Uint32, ttrait_Uint32, ttrait_Uint32, &NdbApiWrapper::create_instance >(env, cls, p0, p1, p2, p3);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NDBAPI
 * Method:    drop_instance
 * Signature: ()V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NDBAPI_drop_1instance(JNIEnv * env, jclass cls)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NDBAPI_drop_1instance(JNIEnv *, jclass)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_fv< &::drop_instance >(env, cls);
#else
    gcall_fv< &NdbApiWrapper::drop_instance >(env, cls);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NDBAPI
 * Method:    get_ndb_object
 * Signature: ([ILjava/lang/String;Ljava/lang/String;)Lcom/mysql/ndbjtie/ndbapi/Ndb;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NDBAPI_get_1ndb_1object(JNIEnv * env, jclass cls, jintArray p0, jstring p1, jstring p2)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NDBAPI_get_1ndb_1object(JNIEnv *, jclass, jintArray, jstring, jstring)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_fr< ttrait_c_m_n_n_Ndb_p, ttrait_Uint32_r_a, ttrait_char_cp_jutf8null, ttrait_char_cp_jutf8null, &::get_ndb_object >(env, cls, p0, p1, p2);
#else
    return gcall_fr< ttrait_c_m_n_n_Ndb_p, ttrait_Uint32_r_a, ttrait_char_cp_jutf8null, ttrait_char_cp_jutf8null, &NdbApiWrapper::get_ndb_object >(env, cls, p0, p1, p2);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NDBAPI
 * Method:    return_ndb_object
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/Ndb;I)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NDBAPI_return_1ndb_1object(JNIEnv * env, jclass cls, jobject p0, jint p1)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NDBAPI_return_1ndb_1object(JNIEnv *, jclass, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_fv< ttrait_c_m_n_n_Ndb_p, ttrait_Uint32, &::return_ndb_object >(env, cls, p0, p1);
#else
    gcall_fv< ttrait_c_m_n_n_Ndb_p, ttrait_Uint32, &NdbApiWrapper::return_ndb_object >(env, cls, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

// ---------------------------------------------------------------------------

//#include "com_mysql_ndbjtie_ndbapi_Ndb.h"

/*
 * Class:     com_mysql_ndbjtie_ndbapi_Ndb
 * Method:    getAutoIncrementValue
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/TableConst;[JIJJ)I
 */
JNIEXPORT jint JNICALL Java_com_mysql_ndbjtie_ndbapi_Ndb_getAutoIncrementValue
  (JNIEnv * env, jobject obj, jobject table, jlongArray ret, jint batch, jlong step, jlong start)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_Ndb_getAutoIncrementValue__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024TableConst_2L__2I(JNIEnv *, jobject, jobject, jlongarray, jint, jlong, jlong)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_Ndb_t, ttrait_int, ttrait_c_m_n_n_NdbDictionary_Table_cp, ttrait_Uint64_r_a, ttrait_Uint32, ttrait_Uint64, ttrait_Uint64, &Ndb::getAutoIncrementValue >(env, obj, table, ret, batch, step, start);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_Ndb_r, ttrait_c_m_n_n_NdbDictionary_Table_cp, ttrait_Uint64_r_a, ttrait_Uint32, ttrait_Uint64, ttrait_Uint64, &NdbApiWrapper::Ndb__getAutoIncrementValue >(env, NULL, obj, table, ret, batch, step, start);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}


/*
 * Class:     com_mysql_ndbjtie_ndbapi_Ndb
 * Method:    getDatabaseName
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL
Java_com_mysql_ndbjtie_ndbapi_Ndb_getDatabaseName(JNIEnv * env, jobject obj)
{
    TRACE("jstring Java_com_mysql_ndbjtie_ndbapi_Ndb_getDatabaseName(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_Ndb_ct, ttrait_char_cp_jutf8null, &Ndb::getDatabaseName >(env, obj);
#else
    return gcall_fr< ttrait_char_cp_jutf8null, ttrait_c_m_n_n_Ndb_cr, &NdbApiWrapper::Ndb__getDatabaseName >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_Ndb
 * Method:    getDatabaseSchemaName
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL
Java_com_mysql_ndbjtie_ndbapi_Ndb_getDatabaseSchemaName(JNIEnv * env, jobject obj)
{
    TRACE("jstring Java_com_mysql_ndbjtie_ndbapi_Ndb_getDatabaseSchemaName(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_Ndb_ct, ttrait_char_cp_jutf8null, &Ndb::getDatabaseSchemaName >(env, obj);
#else
    return gcall_fr< ttrait_char_cp_jutf8null, ttrait_c_m_n_n_Ndb_cr, &NdbApiWrapper::Ndb__getDatabaseSchemaName >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_Ndb
 * Method:    getDictionary
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/Dictionary;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_Ndb_getDictionary(JNIEnv * env, jobject obj)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_Ndb_getDictionary(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_Ndb_ct, ttrait_c_m_n_n_NdbDictionary_Dictionary_p, &Ndb::getDictionary >(env, obj);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbDictionary_Dictionary_p, ttrait_c_m_n_n_Ndb_cr, &NdbApiWrapper::Ndb__getDictionary >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_Ndb
 * Method:    getNdbError
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbErrorConst;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_Ndb_getNdbError__(JNIEnv * env, jobject obj)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_Ndb_getNdbError__(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_Ndb_ct, ttrait_c_m_n_n_NdbError_cr, &Ndb::getNdbError >(env, obj);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbError_cr, ttrait_c_m_n_n_Ndb_cr, &NdbApiWrapper::Ndb__getNdbError__0 >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_Ndb
 * Method:    getNdbErrorDetail
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbErrorConst;Ljava/nio/ByteBuffer;I)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL
Java_com_mysql_ndbjtie_ndbapi_Ndb_getNdbErrorDetail(JNIEnv * env, jobject obj, jobject p0, jobject p1, jint p2)
{
    TRACE("Java_com_mysql_ndbjtie_ndbapi_Ndb_getNdbErrorDetail(JNIEnv *, jobject, jobject, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_Ndb_ct, ttrait_char_cp_jutf8null, ttrait_c_m_n_n_NdbError_cr, ttrait_char_0p_bb, ttrait_Uint32, &Ndb::getNdbErrorDetail >(env, obj, p0, p1, p2);
#else
    return gcall_fr< ttrait_char_cp_jutf8null, ttrait_c_m_n_n_Ndb_cr, ttrait_c_m_n_n_NdbError_cr, ttrait_char_0p_bb, ttrait_Uint32, &NdbApiWrapper::Ndb__getNdbErrorDetail >(env, NULL, obj, p0, p1, p2);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_Ndb
 * Method:    create
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/Ndb_cluster_connection;Ljava/lang/String;Ljava/lang/String;)Lcom/mysql/ndbjtie/ndbapi/Ndb;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_Ndb_create(JNIEnv * env, jclass cls, jobject p0, jstring p1, jstring p2)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_Ndb_create(JNIEnv *, jclass, jobject, jstring, jstring)");
    return gcreate< ttrait_c_m_n_n_Ndb_r, ttrait_c_m_n_n_Ndb_cluster_connection_p, ttrait_char_cp_jutf8null, ttrait_char_cp_jutf8null >(env, cls, p0, p1, p2);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_Ndb
 * Method:    delete
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/Ndb;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_Ndb_delete(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_Ndb_delete(JNIEnv *, jclass, jobject)");
    gdelete< ttrait_c_m_n_n_Ndb_r >(env, cls, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_Ndb
 * Method:    setDatabaseName
 * Signature: (Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_Ndb_setDatabaseName(JNIEnv * env, jobject obj, jstring p0)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_Ndb_setDatabaseName(JNIEnv *, jobject, jstring)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_Ndb_t, ttrait_int, ttrait_char_cp_jutf8null, &Ndb::setDatabaseName >(env, obj, p0);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_Ndb_r, ttrait_char_cp_jutf8null, &NdbApiWrapper::Ndb__setDatabaseName >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_Ndb
 * Method:    setDatabaseSchemaName
 * Signature: (Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_Ndb_setDatabaseSchemaName(JNIEnv * env, jobject obj, jstring p0)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_Ndb_setDatabaseSchemaName(JNIEnv *, jobject, jstring)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_Ndb_t, ttrait_int, ttrait_char_cp_jutf8null, &Ndb::setDatabaseSchemaName >(env, obj, p0);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_Ndb_r, ttrait_char_cp_jutf8null, &NdbApiWrapper::Ndb__setDatabaseSchemaName >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_Ndb
 * Method:    init
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_Ndb_init(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_Ndb_init(JNIEnv *, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_Ndb_t, ttrait_int, ttrait_int, &Ndb::init >(env, obj, p0);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_Ndb_r, ttrait_int, &NdbApiWrapper::Ndb__init >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_Ndb
 * Method:    createEventOperation
 * Signature: (Ljava/lang/String;)Lcom/mysql/ndbjtie/ndbapi/NdbEventOperation;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_Ndb_createEventOperation(JNIEnv * env, jobject obj, jstring p0)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_Ndb_createEventOperation(JNIEnv *, jobject, jstring)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_Ndb_t, ttrait_c_m_n_n_NdbEventOperation_p, ttrait_char_cp_jutf8null, &Ndb::createEventOperation >(env, obj, p0);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbEventOperation_p, ttrait_c_m_n_n_Ndb_r, ttrait_char_cp_jutf8null, &NdbApiWrapper::Ndb__createEventOperation >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_Ndb
 * Method:    dropEventOperation
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbEventOperation;)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_Ndb_dropEventOperation(JNIEnv * env, jobject obj, jobject p0)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_Ndb_dropEventOperation(JNIEnv *, jobject, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_Ndb_t, ttrait_int, ttrait_c_m_n_n_NdbEventOperation_p, &Ndb::dropEventOperation >(env, obj, p0);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_Ndb_r, ttrait_c_m_n_n_NdbEventOperation_p, &NdbApiWrapper::Ndb__dropEventOperation >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_Ndb
 * Method:    pollEvents
 * Signature: (I[J)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_Ndb_pollEvents(JNIEnv * env, jobject obj, jint p0, jlongArray p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_Ndb_pollEvents(JNIEnv *, jobject, jint, jlongArray)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_Ndb_t, ttrait_int, ttrait_int, ttrait_Uint64_0p_a, &Ndb::pollEvents >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_Ndb_r, ttrait_int, ttrait_Uint64_0p_a, &NdbApiWrapper::Ndb__pollEvents >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_Ndb
 * Method:    nextEvent
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbEventOperation;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_Ndb_nextEvent(JNIEnv * env, jobject obj)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_Ndb_nextEvent(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_Ndb_t, ttrait_c_m_n_n_NdbEventOperation_p, &Ndb::nextEvent >(env, obj);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbEventOperation_p, ttrait_c_m_n_n_Ndb_r, &NdbApiWrapper::Ndb__nextEvent >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_Ndb
 * Method:    isConsistent
 * Signature: ([J)Z
 */
JNIEXPORT jboolean JNICALL
Java_com_mysql_ndbjtie_ndbapi_Ndb_isConsistent(JNIEnv * env, jobject obj, jlongArray p0)
{
    TRACE("jboolean Java_com_mysql_ndbjtie_ndbapi_Ndb_isConsistent(JNIEnv *, jobject, jlongArray)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_Ndb_t, ttrait_bool, ttrait_Uint64_r_a, &Ndb::isConsistent >(env, obj, p0);
#else
    return gcall_fr< ttrait_bool, ttrait_c_m_n_n_Ndb_r, ttrait_Uint64_r_a, &NdbApiWrapper::Ndb__isConsistent >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_Ndb
 * Method:    isConsistentGCI
 * Signature: (J)Z
 */
JNIEXPORT jboolean JNICALL
Java_com_mysql_ndbjtie_ndbapi_Ndb_isConsistentGCI(JNIEnv * env, jobject obj, jlong p0)
{
    TRACE("jboolean Java_com_mysql_ndbjtie_ndbapi_Ndb_isConsistentGCI(JNIEnv *, jobject, jlong)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_Ndb_t, ttrait_bool, ttrait_Uint64, &Ndb::isConsistentGCI >(env, obj, p0);
#else
    return gcall_fr< ttrait_bool, ttrait_c_m_n_n_Ndb_r, ttrait_Uint64, &NdbApiWrapper::Ndb__isConsistentGCI >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_Ndb
 * Method:    getGCIEventOperations
 * Signature: ([I[I)Lcom/mysql/ndbjtie/ndbapi/NdbEventOperationConst;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_Ndb_getGCIEventOperations(JNIEnv * env, jobject obj, jintArray p0, jintArray p1)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_Ndb_getGCIEventOperations(JNIEnv *, jobject, jintArray, jintArray)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_Ndb_t, ttrait_c_m_n_n_NdbEventOperation_cp, ttrait_Uint32_0p_a, ttrait_Uint32_0p_a, &Ndb::getGCIEventOperations >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbEventOperation_cp, ttrait_c_m_n_n_Ndb_r, ttrait_Uint32_0p_a, ttrait_Uint32_0p_a, &NdbApiWrapper::Ndb__getGCIEventOperations >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_Ndb
 * Method:    startTransaction
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/TableConst;Ljava/nio/ByteBuffer;I)Lcom/mysql/ndbjtie/ndbapi/NdbTransaction;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_Ndb_startTransaction__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024TableConst_2Ljava_nio_ByteBuffer_2I(JNIEnv * env, jobject obj, jobject p0, jobject p1, jint p2)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_Ndb_startTransaction__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024TableConst_2Ljava_nio_ByteBuffer_2I(JNIEnv *, jobject, jobject, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_Ndb_t, ttrait_c_m_n_n_NdbTransaction_p, ttrait_c_m_n_n_NdbDictionary_Table_cp, ttrait_char_0cp_bb, ttrait_Uint32, &Ndb::startTransaction >(env, obj, p0, p1, p2);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbTransaction_p, ttrait_c_m_n_n_Ndb_r, ttrait_c_m_n_n_NdbDictionary_Table_cp, ttrait_char_0cp_bb, ttrait_Uint32, &NdbApiWrapper::Ndb__startTransaction__0 >(env, NULL, obj, p0, p1, p2);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_Ndb
 * Method:    startTransaction
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/TableConst;Lcom/mysql/ndbjtie/ndbapi/Ndb/Key_part_ptrConstArray;Ljava/nio/ByteBuffer;I)Lcom/mysql/ndbjtie/ndbapi/NdbTransaction;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_Ndb_startTransaction__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024TableConst_2Lcom_mysql_ndbjtie_ndbapi_Ndb_00024Key_1part_1ptrConstArray_2Ljava_nio_ByteBuffer_2I(JNIEnv * env, jobject obj, jobject p0, jobject p1, jobject p2, jint p3)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_Ndb_startTransaction__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024TableConst_2Lcom_mysql_ndbjtie_ndbapi_Ndb_00024Key_1part_1ptrConstArray_2Ljava_nio_ByteBuffer_2I(JNIEnv *, jobject, jobject, jobject, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_Ndb_t, ttrait_c_m_n_n_NdbTransaction_p, ttrait_c_m_n_n_NdbDictionary_Table_cp, ttrait_c_m_n_n_Ndb_Key_part_ptrArray_cp, ttrait_void_1p_bb, ttrait_Uint32, &Ndb::startTransaction >(env, obj, p0, p1, p2, p3);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbTransaction_p, ttrait_c_m_n_n_Ndb_r, ttrait_c_m_n_n_NdbDictionary_Table_cp, ttrait_c_m_n_n_Ndb_Key_part_ptrArray_cp, ttrait_void_1p_bb, ttrait_Uint32, &NdbApiWrapper::Ndb__startTransaction__1 >(env, NULL, obj, p0, p1, p2, p3);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_Ndb
 * Method:    startTransaction
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/TableConst;I)Lcom/mysql/ndbjtie/ndbapi/NdbTransaction;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_Ndb_startTransaction__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024TableConst_2I(JNIEnv * env, jobject obj, jobject p0, jint p1)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_Ndb_startTransaction__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024TableConst_2I(JNIEnv *, jobject, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_Ndb_t, ttrait_c_m_n_n_NdbTransaction_p, ttrait_c_m_n_n_NdbDictionary_Table_cp, ttrait_Uint32, &Ndb::startTransaction >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbTransaction_p, ttrait_c_m_n_n_Ndb_r, ttrait_c_m_n_n_NdbDictionary_Table_cp, ttrait_Uint32, &NdbApiWrapper::Ndb__startTransaction >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_Ndb
 * Method:    computeHash
 * Signature: ([ILcom/mysql/ndbjtie/ndbapi/NdbDictionary/TableConst;[Lcom/mysql/ndbjtie/ndbapi/Ndb/Key_part_ptrConst;Ljava/nio/ByteBuffer;I)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_Ndb_computeHash(JNIEnv * env, jclass cls, jintArray p0, jobject p1, jobjectArray p2, jobject p3, jint p4)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_Ndb_computeHash(JNIEnv *, jclass, jintArray, jobject, jobjectArray, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_fr< ttrait_int, ttrait_Uint32_0p_a, ttrait_c_m_n_n_NdbDictionary_Table_cp, ttrait_c_m_n_n_Ndb_Key_part_ptrArray_cp, ttrait_void_1p_bb, ttrait_Uint32, &Ndb::computeHash >(env, cls, p0, p1, p2, p3, p4);
#else
    return gcall_fr< ttrait_int, ttrait_Uint32_0p_a, ttrait_c_m_n_n_NdbDictionary_Table_cp, ttrait_c_m_n_n_Ndb_Key_part_ptrArray_cp, ttrait_void_1p_bb, ttrait_Uint32, &NdbApiWrapper::Ndb__computeHash >(env, cls, p0, p1, p2, p3, p4);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_Ndb
 * Method:    closeTransaction
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbTransaction;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_Ndb_closeTransaction(JNIEnv * env, jobject obj, jobject p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_Ndb_closeTransaction(JNIEnv *, jobject, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_Ndb_t, ttrait_c_m_n_n_NdbTransaction_p, &Ndb::closeTransaction >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_Ndb_r, ttrait_c_m_n_n_NdbTransaction_p, &NdbApiWrapper::Ndb__closeTransaction >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_Ndb
 * Method:    getNdbError
 * Signature: (I)Lcom/mysql/ndbjtie/ndbapi/NdbErrorConst;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_Ndb_getNdbError__I(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_Ndb_getNdbError__I(JNIEnv *, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_Ndb_t, ttrait_c_m_n_n_NdbError_cr, ttrait_int, &Ndb::getNdbError >(env, obj, p0);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbError_cr, ttrait_c_m_n_n_Ndb_r, ttrait_int, &NdbApiWrapper::Ndb__getNdbError__1 >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

// ---------------------------------------------------------------------------

//#include "com_mysql_ndbjtie_ndbapi_NdbBlob.h"

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbBlob
 * Method:    getNdbError
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbErrorConst;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbBlob_getNdbError(JNIEnv * env, jobject obj)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbBlob_getNdbError(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbBlob_ct, ttrait_c_m_n_n_NdbError_cr, &NdbBlob::getNdbError >(env, obj);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbError_cr, ttrait_c_m_n_n_NdbBlob_cr, &NdbApiWrapper::NdbBlob__getNdbError >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbBlob
 * Method:    getNdbOperation
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbOperationConst;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbBlob_getNdbOperation(JNIEnv * env, jobject obj)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbBlob_getNdbOperation(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbBlob_ct, ttrait_c_m_n_n_NdbOperation_cp, &NdbBlob::getNdbOperation >(env, obj);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbOperation_cp, ttrait_c_m_n_n_NdbBlob_cr, &NdbApiWrapper::NdbBlob__getNdbOperation >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbBlob
 * Method:    getState
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbBlob_getState(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbBlob_getState(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbBlob_t, ttrait_c_m_n_n_NdbBlob_State_iv/*_enum_*/, &NdbBlob::getState >(env, obj);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbBlob_State_iv/*_enum_*/, ttrait_c_m_n_n_NdbBlob_r, &NdbApiWrapper::NdbBlob__getState >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

#if 0 // MMM! support variable-width type non-const references
/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbBlob
 * Method:    getVersion
 * Signature: ([I)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbBlob_getVersion(JNIEnv * env, jobject obj, jintArray p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbBlob_getVersion(JNIEnv *, jobject, jintArray)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbBlob_t, ttrait_int_r_a, &NdbBlob::getVersion >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_NdbBlob_r, ttrait_int_r_a, &NdbApiWrapper::NdbBlob__getVersion >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}
#endif // MMM! support variable-width type non-const references

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbBlob
 * Method:    getValue
 * Signature: (Ljava/nio/ByteBuffer;I)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbBlob_getValue(JNIEnv * env, jobject obj, jobject p0, jint p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbBlob_getValue(JNIEnv *, jobject, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbBlob_t, ttrait_int, ttrait_void_1p_bb, ttrait_Uint32, &NdbBlob::getValue >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbBlob_r, ttrait_void_1p_bb, ttrait_Uint32, &NdbApiWrapper::NdbBlob__getValue >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbBlob
 * Method:    setValue
 * Signature: (Ljava/nio/ByteBuffer;I)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbBlob_setValue(JNIEnv * env, jobject obj, jobject p0, jint p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbBlob_setValue(JNIEnv *, jobject, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbBlob_t, ttrait_int, ttrait_void_1cp_bb, ttrait_Uint32, &NdbBlob::setValue >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbBlob_r, ttrait_void_1cp_bb, ttrait_Uint32, &NdbApiWrapper::NdbBlob__setValue >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

#if 0 // MMM! support variable-width type non-const references
/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbBlob
 * Method:    getNull
 * Signature: ([I)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbBlob_getNull(JNIEnv * env, jobject obj, jintArray p0)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbBlob_getNull(JNIEnv *, jobject, jintArray)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbBlob_t, ttrait_int, ttrait_int_r_a, &NdbBlob::getNull >(env, obj, p0);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbBlob_r, ttrait_int_r_a, &NdbApiWrapper::NdbBlob__getNull >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}
#endif // MMM! support variable-width type non-const references

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbBlob
 * Method:    setNull
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbBlob_setNull(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbBlob_setNull(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbBlob_t, ttrait_int, &NdbBlob::setNull >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbBlob_r, &NdbApiWrapper::NdbBlob__setNull >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbBlob
 * Method:    getLength
 * Signature: ([J)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbBlob_getLength(JNIEnv * env, jobject obj, jlongArray p0)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbBlob_getLength(JNIEnv *, jobject, jlongArray)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbBlob_t, ttrait_int, ttrait_Uint64_r_a, &NdbBlob::getLength >(env, obj, p0);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbBlob_r, ttrait_Uint64_r_a, &NdbApiWrapper::NdbBlob__getLength >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbBlob
 * Method:    truncate
 * Signature: (J)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbBlob_truncate(JNIEnv * env, jobject obj, jlong p0)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbBlob_truncate(JNIEnv *, jobject, jlong)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbBlob_t, ttrait_int, ttrait_Uint64, &NdbBlob::truncate >(env, obj, p0);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbBlob_r, ttrait_Uint64, &NdbApiWrapper::NdbBlob__truncate >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbBlob
 * Method:    getPos
 * Signature: ([J)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbBlob_getPos(JNIEnv * env, jobject obj, jlongArray p0)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbBlob_getPos(JNIEnv *, jobject, jlongArray)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbBlob_t, ttrait_int, ttrait_Uint64_r_a, &NdbBlob::getPos >(env, obj, p0);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbBlob_r, ttrait_Uint64_r_a, &NdbApiWrapper::NdbBlob__getPos >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbBlob
 * Method:    setPos
 * Signature: (J)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbBlob_setPos(JNIEnv * env, jobject obj, jlong p0)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbBlob_setPos(JNIEnv *, jobject, jlong)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbBlob_t, ttrait_int, ttrait_Uint64, &NdbBlob::setPos >(env, obj, p0);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbBlob_r, ttrait_Uint64, &NdbApiWrapper::NdbBlob__setPos >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbBlob
 * Method:    readData
 * Signature: (Ljava/nio/ByteBuffer;[I)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbBlob_readData(JNIEnv * env, jobject obj, jobject p0, jintArray p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbBlob_readData(JNIEnv *, jobject, jobject, jintArray)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbBlob_t, ttrait_int, ttrait_void_0p_bb, ttrait_Uint32_r_a, &NdbBlob::readData >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbBlob_r, ttrait_void_0p_bb, ttrait_Uint32_r_a, &NdbApiWrapper::NdbBlob__readData >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbBlob
 * Method:    writeData
 * Signature: (Ljava/nio/ByteBuffer;I)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbBlob_writeData(JNIEnv * env, jobject obj, jobject p0, jint p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbBlob_writeData(JNIEnv *, jobject, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbBlob_t, ttrait_int, ttrait_void_0cp_bb, ttrait_Uint32, &NdbBlob::writeData >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbBlob_r, ttrait_void_0cp_bb, ttrait_Uint32, &NdbApiWrapper::NdbBlob__writeData >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbBlob
 * Method:    getColumn
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/ColumnConst;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbBlob_getColumn(JNIEnv * env, jobject obj)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbBlob_getColumn(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbBlob_t, ttrait_c_m_n_n_NdbDictionary_Column_cp, &NdbBlob::getColumn >(env, obj);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbDictionary_Column_cp, ttrait_c_m_n_n_NdbBlob_r, &NdbApiWrapper::NdbBlob__getColumn >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbBlob
 * Method:    getBlobTableName
 * Signature: (Ljava/nio/ByteBuffer;Lcom/mysql/ndbjtie/ndbapi/Ndb;Ljava/lang/String;Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbBlob_getBlobTableName(JNIEnv * env, jclass cls, jobject p0, jobject p1, jstring p2, jstring p3)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbBlob_getBlobTableName(JNIEnv *, jclass, jobject, jobject, jstring, jstring)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_OVERLOADED_FUNCTION
    return gcall_fr< ttrait_int, ttrait_char_1p_bb, ttrait_c_m_n_n_Ndb_p, ttrait_char_cp_jutf8null, ttrait_char_cp_jutf8null, &NdbBlob::getBlobTableName >(env, cls, p0, p1, p2, p3);
#else
    return gcall_fr< ttrait_int, ttrait_char_1p_bb, ttrait_c_m_n_n_Ndb_p, ttrait_char_cp_jutf8null, ttrait_char_cp_jutf8null, &NdbApiWrapper::NdbBlob__getBlobTableName >(env, cls, p0, p1, p2, p3);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_OVERLOADED_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbBlob
 * Method:    getBlobEventName
 * Signature: (Ljava/nio/ByteBuffer;Lcom/mysql/ndbjtie/ndbapi/Ndb;Ljava/lang/String;Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbBlob_getBlobEventName(JNIEnv * env, jclass cls, jobject p0, jobject p1, jstring p2, jstring p3)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbBlob_getBlobEventName(JNIEnv *, jclass, jobject, jobject, jstring, jstring)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_OVERLOADED_FUNCTION
    return gcall_fr< ttrait_int, ttrait_char_1p_bb, ttrait_c_m_n_n_Ndb_p, ttrait_char_cp_jutf8null, ttrait_char_cp_jutf8null, &NdbBlob::getBlobEventName >(env, cls, p0, p1, p2, p3);
#else
    return gcall_fr< ttrait_int, ttrait_char_1p_bb, ttrait_c_m_n_n_Ndb_p, ttrait_char_cp_jutf8null, ttrait_char_cp_jutf8null, &NdbApiWrapper::NdbBlob__getBlobEventName >(env, cls, p0, p1, p2, p3);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_OVERLOADED_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbBlob
 * Method:    blobsFirstBlob
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbBlob;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbBlob_blobsFirstBlob(JNIEnv * env, jobject obj)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbBlob_blobsFirstBlob(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbBlob_t, ttrait_c_m_n_n_NdbBlob_p, &NdbBlob::blobsFirstBlob >(env, obj);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbBlob_p, ttrait_c_m_n_n_NdbBlob_r, &NdbApiWrapper::NdbBlob__blobsFirstBlob >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbBlob
 * Method:    blobsNextBlob
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbBlob;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbBlob_blobsNextBlob(JNIEnv * env, jobject obj)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbBlob_blobsNextBlob(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbBlob_t, ttrait_c_m_n_n_NdbBlob_p, &NdbBlob::blobsNextBlob >(env, obj);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbBlob_p, ttrait_c_m_n_n_NdbBlob_r, &NdbApiWrapper::NdbBlob__blobsNextBlob >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbBlob
 * Method:    close
 * Signature: (Z)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbBlob_close(JNIEnv * env, jobject obj, jboolean p0)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbBlob_close(JNIEnv *, jobject, jboolean)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbBlob_t, ttrait_int, ttrait_bool, &NdbBlob::close >(env, obj, p0);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbBlob_r, ttrait_bool, &NdbApiWrapper::NdbBlob__close >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

// ---------------------------------------------------------------------------

//#include "com_mysql_ndbjtie_ndbapi_NdbDictionary.h"

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary
 * Method:    create
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbDictionary;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_create(JNIEnv * env, jclass cls)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_create(JNIEnv *, jclass)");
    return gcreate< ttrait_c_m_n_n_NdbDictionary_r >(env, cls);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary
 * Method:    delete
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_delete(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_delete(JNIEnv *, jclass, jobject)");
    gdelete< ttrait_c_m_n_n_NdbDictionary_r >(env, cls, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary
 * Method:    getRecordType
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbRecordConst;)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_getRecordType(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_getRecordType(JNIEnv *, jclass, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_fr< ttrait_c_m_n_n_NdbDictionary_RecordType_iv/*_enum_*/, ttrait_c_m_n_n_NdbRecord_cp, &NdbDictionary::getRecordType >(env, cls, p0);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbDictionary_RecordType_iv/*_enum_*/, ttrait_c_m_n_n_NdbRecord_cp, &NdbApiWrapper::NdbDictionary__getRecordType >(env, cls, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary
 * Method:    getRecordTableName
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbRecordConst;)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_getRecordTableName(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("jstring Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_getRecordTableName(JNIEnv *, jclass, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_fr< ttrait_char_cp_jutf8null, ttrait_c_m_n_n_NdbRecord_cp, &NdbDictionary::getRecordTableName >(env, cls, p0);
#else
    return gcall_fr< ttrait_char_cp_jutf8null, ttrait_c_m_n_n_NdbRecord_cp, &NdbApiWrapper::NdbDictionary__getRecordTableName >(env, cls, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary
 * Method:    getRecordIndexName
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbRecordConst;)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_getRecordIndexName(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("jstring Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_getRecordIndexName(JNIEnv *, jclass, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_fr< ttrait_char_cp_jutf8null, ttrait_c_m_n_n_NdbRecord_cp, &NdbDictionary::getRecordIndexName >(env, cls, p0);
#else
    return gcall_fr< ttrait_char_cp_jutf8null, ttrait_c_m_n_n_NdbRecord_cp, &NdbApiWrapper::NdbDictionary__getRecordIndexName >(env, cls, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary
 * Method:    getFirstAttrId
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbRecordConst;[I)Z
 */
JNIEXPORT jboolean JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_getFirstAttrId(JNIEnv * env, jclass cls, jobject p0, jintArray p1)
{
    TRACE("jboolean Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_getFirstAttrId(JNIEnv *, jclass, jobject, jintArray)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_fr< ttrait_bool, ttrait_c_m_n_n_NdbRecord_cp, ttrait_Uint32_r_a, &NdbDictionary::getFirstAttrId >(env, cls, p0, p1);
#else
    return gcall_fr< ttrait_bool, ttrait_c_m_n_n_NdbRecord_cp, ttrait_Uint32_r_a, &NdbApiWrapper::NdbDictionary__getFirstAttrId >(env, cls, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary
 * Method:    getNextAttrId
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbRecordConst;[I)Z
 */
JNIEXPORT jboolean JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_getNextAttrId(JNIEnv * env, jclass cls, jobject p0, jintArray p1)
{
    TRACE("jboolean Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_getNextAttrId(JNIEnv *, jclass, jobject, jintArray)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_fr< ttrait_bool, ttrait_c_m_n_n_NdbRecord_cp, ttrait_Uint32_r_a, &NdbDictionary::getNextAttrId >(env, cls, p0, p1);
#else
    return gcall_fr< ttrait_bool, ttrait_c_m_n_n_NdbRecord_cp, ttrait_Uint32_r_a, &NdbApiWrapper::NdbDictionary__getNextAttrId >(env, cls, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary
 * Method:    getOffset
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbRecordConst;I[I)Z
 */
JNIEXPORT jboolean JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_getOffset(JNIEnv * env, jclass cls, jobject p0, jint p1, jintArray p2)
{
    TRACE("jboolean Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_getOffset(JNIEnv *, jclass, jobject, jint, jintArray)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_fr< ttrait_bool, ttrait_c_m_n_n_NdbRecord_cp, ttrait_Uint32, ttrait_Uint32_r_a, &NdbDictionary::getOffset >(env, cls, p0, p1, p2);
#else
    return gcall_fr< ttrait_bool, ttrait_c_m_n_n_NdbRecord_cp, ttrait_Uint32, ttrait_Uint32_r_a, &NdbApiWrapper::NdbDictionary__getOffset >(env, cls, p0, p1, p2);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary
 * Method:    getNullBitOffset
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbRecordConst;I[I[I)Z
 */
JNIEXPORT jboolean JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_getNullBitOffset(JNIEnv * env, jclass cls, jobject p0, jint p1, jintArray p2, jintArray p3)
{
    TRACE("jboolean Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_getNullBitOffset(JNIEnv *, jclass, jobject, jint, jintArray, jintArray)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_fr< ttrait_bool, ttrait_c_m_n_n_NdbRecord_cp, ttrait_Uint32, ttrait_Uint32_r_a, ttrait_Uint32_r_a, &NdbDictionary::getNullBitOffset >(env, cls, p0, p1, p2, p3);
#else
    return gcall_fr< ttrait_bool, ttrait_c_m_n_n_NdbRecord_cp, ttrait_Uint32, ttrait_Uint32_r_a, ttrait_Uint32_r_a, &NdbApiWrapper::NdbDictionary__getNullBitOffset >(env, cls, p0, p1, p2, p3);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary
 * Method:    getValuePtr
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbRecordConst;Ljava/lang/String;I)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_getValuePtr(JNIEnv * env, jclass cls, jobject p0, jstring p1, jint p2)
{
    TRACE("jstring Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_getValuePtr(JNIEnv *, jclass, jobject, jstring, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_OVERLOADED_FUNCTION
    return gcall_fr< ttrait_char_cp_jutf8null, ttrait_c_m_n_n_NdbRecord_cp, ttrait_char_cp_jutf8null, ttrait_Uint32, &NdbDictionary::getValuePtr >(env, cls, p0, p1, p2);
#else
    return gcall_fr< ttrait_char_cp_jutf8null, ttrait_c_m_n_n_NdbRecord_cp, ttrait_char_cp_jutf8null, ttrait_Uint32, &NdbApiWrapper::NdbDictionary__getValuePtr >(env, cls, p0, p1, p2);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_OVERLOADED_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary
 * Method:    isNull
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbRecordConst;Ljava/lang/String;I)Z
 */
JNIEXPORT jboolean JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_isNull(JNIEnv * env, jclass cls, jobject p0, jstring p1, jint p2)
{
    TRACE("jboolean Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_isNull(JNIEnv *, jclass, jobject, jstring, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_fr< ttrait_bool, ttrait_c_m_n_n_NdbRecord_cp, ttrait_char_cp_jutf8null, ttrait_Uint32, &NdbDictionary::isNull >(env, cls, p0, p1, p2);
#else
    return gcall_fr< ttrait_bool, ttrait_c_m_n_n_NdbRecord_cp, ttrait_char_cp_jutf8null, ttrait_Uint32, &NdbApiWrapper::NdbDictionary__isNull >(env, cls, p0, p1, p2);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary
 * Method:    setNull
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbRecordConst;Ljava/nio/ByteBuffer;IZ)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_setNull(JNIEnv * env, jclass cls, jobject p0, jobject p1, jint p2, jboolean p3)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_setNull(JNIEnv *, jclass, jobject, jobject, jint, jboolean)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbRecord_cp, ttrait_char_1p_bb, ttrait_Uint32, ttrait_bool, &NdbDictionary::setNull >(env, cls, p0, p1, p2, p3);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbRecord_cp, ttrait_char_1p_bb, ttrait_Uint32, ttrait_bool, &NdbApiWrapper::NdbDictionary__setNull >(env, cls, p0, p1, p2, p3);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary
 * Method:    getRecordRowLength
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbRecordConst;)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_getRecordRowLength(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_getRecordRowLength(JNIEnv *, jclass, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_fr< ttrait_Uint32, ttrait_c_m_n_n_NdbRecord_cp, &NdbDictionary::getRecordRowLength >(env, cls, p0);
#else
    return gcall_fr< ttrait_Uint32, ttrait_c_m_n_n_NdbRecord_cp, &NdbApiWrapper::NdbDictionary__getRecordRowLength >(env, cls, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

// ---------------------------------------------------------------------------

//#include "com_mysql_ndbjtie_ndbapi_NdbDictionary_AutoGrowSpecification.h"

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_AutoGrowSpecification
 * Method:    min_free
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024AutoGrowSpecification_min_1free__(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024AutoGrowSpecification_min_1free__(JNIEnv *, jobject)");
    return gget< ttrait_c_m_n_n_NdbDictionary_AutoGrowSpecification_t, ttrait_Uint32, &NdbDictionary::AutoGrowSpecification::min_free >(env, obj);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_AutoGrowSpecification
 * Method:    max_size
 * Signature: ()J
 */
JNIEXPORT jlong JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024AutoGrowSpecification_max_1size__(JNIEnv * env, jobject obj)
{
    TRACE("jlong Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024AutoGrowSpecification_max_1size__(JNIEnv *, jobject)");
    return gget< ttrait_c_m_n_n_NdbDictionary_AutoGrowSpecification_t, ttrait_Uint64, &NdbDictionary::AutoGrowSpecification::max_size >(env, obj);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_AutoGrowSpecification
 * Method:    file_size
 * Signature: ()J
 */
JNIEXPORT jlong JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024AutoGrowSpecification_file_1size__(JNIEnv * env, jobject obj)
{
    TRACE("jlong Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024AutoGrowSpecification_file_1size__(JNIEnv *, jobject)");
    return gget< ttrait_c_m_n_n_NdbDictionary_AutoGrowSpecification_t, ttrait_Uint64, &NdbDictionary::AutoGrowSpecification::file_size >(env, obj);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_AutoGrowSpecification
 * Method:    filename_pattern
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024AutoGrowSpecification_filename_1pattern__(JNIEnv * env, jobject obj)
{
    TRACE("jstring Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024AutoGrowSpecification_filename_1pattern__(JNIEnv *, jobject)");
    return gget< ttrait_c_m_n_n_NdbDictionary_AutoGrowSpecification_t, ttrait_char_cp_jutf8null, &NdbDictionary::AutoGrowSpecification::filename_pattern >(env, obj);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_AutoGrowSpecification
 * Method:    min_free
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024AutoGrowSpecification_min_1free__I(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024AutoGrowSpecification_min_1free__I(JNIEnv *, jobject, jint)");
    gset< ttrait_c_m_n_n_NdbDictionary_AutoGrowSpecification_t, ttrait_Uint32, &NdbDictionary::AutoGrowSpecification::min_free >(env, obj, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_AutoGrowSpecification
 * Method:    max_size
 * Signature: (J)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024AutoGrowSpecification_max_1size__J(JNIEnv * env, jobject obj, jlong p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024AutoGrowSpecification_max_1size__J(JNIEnv *, jobject, jlong)");
    gset< ttrait_c_m_n_n_NdbDictionary_AutoGrowSpecification_t, ttrait_Uint64, &NdbDictionary::AutoGrowSpecification::max_size >(env, obj, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_AutoGrowSpecification
 * Method:    file_size
 * Signature: (J)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024AutoGrowSpecification_file_1size__J(JNIEnv * env, jobject obj, jlong p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024AutoGrowSpecification_file_1size__J(JNIEnv *, jobject, jlong)");
    gset< ttrait_c_m_n_n_NdbDictionary_AutoGrowSpecification_t, ttrait_Uint64, &NdbDictionary::AutoGrowSpecification::file_size >(env, obj, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_AutoGrowSpecification
 * Method:    filename_pattern
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024AutoGrowSpecification_filename_1pattern__Ljava_lang_String_2(JNIEnv * env, jobject obj, jstring p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024AutoGrowSpecification_filename_1pattern__Ljava_lang_String_2(JNIEnv *, jobject, jstring)");
    gset< ttrait_c_m_n_n_NdbDictionary_AutoGrowSpecification_t, ttrait_char_cp_jutf8null, &NdbDictionary::AutoGrowSpecification::filename_pattern >(env, obj, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_AutoGrowSpecification
 * Method:    create
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/AutoGrowSpecification;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024AutoGrowSpecification_create(JNIEnv * env, jclass cls)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024AutoGrowSpecification_create(JNIEnv *, jclass)");
    return gcreate< ttrait_c_m_n_n_NdbDictionary_AutoGrowSpecification_r >(env, cls);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_AutoGrowSpecification
 * Method:    delete
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/AutoGrowSpecification;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024AutoGrowSpecification_delete(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024AutoGrowSpecification_delete(JNIEnv *, jclass, jobject)");
    gdelete< ttrait_c_m_n_n_NdbDictionary_AutoGrowSpecification_r >(env, cls, p0);
}

// ---------------------------------------------------------------------------

//#include "com_mysql_ndbjtie_ndbapi_NdbDictionary_Column.h"

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Column
 * Method:    getAutoIncrement
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_getAutoIncrement(JNIEnv * env, jobject obj)
{
    TRACE("jboolean Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_getAutoIncrement(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Column_ct, ttrait_bool, &NdbDictionary::Column::getAutoIncrement >(env, obj);
#else
    return gcall_fr< ttrait_bool, ttrait_c_m_n_n_NdbDictionary_Column_cr, &NdbApiWrapper::NdbDictionary__Column__getAutoIncrement >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Column
 * Method:    getName
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_getName(JNIEnv * env, jobject obj)
{
    TRACE("jstring Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_getName(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Column_ct, ttrait_char_cp_jutf8null, &NdbDictionary::Column::getName >(env, obj);
#else
    return gcall_fr< ttrait_char_cp_jutf8null, ttrait_c_m_n_n_NdbDictionary_Column_cr, &NdbApiWrapper::NdbDictionary__Column__getName >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Column
 * Method:    getNullable
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_getNullable(JNIEnv * env, jobject obj)
{
    TRACE("jboolean Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_getNullable(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Column_ct, ttrait_bool, &NdbDictionary::Column::getNullable >(env, obj);
#else
    return gcall_fr< ttrait_bool, ttrait_c_m_n_n_NdbDictionary_Column_cr, &NdbApiWrapper::NdbDictionary__Column__getNullable >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Column
 * Method:    getPrimaryKey
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_getPrimaryKey(JNIEnv * env, jobject obj)
{
    TRACE("jboolean Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_getPrimaryKey(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Column_ct, ttrait_bool, &NdbDictionary::Column::getPrimaryKey >(env, obj);
#else
    return gcall_fr< ttrait_bool, ttrait_c_m_n_n_NdbDictionary_Column_cr, &NdbApiWrapper::NdbDictionary__Column__getPrimaryKey >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Column
 * Method:    getColumnNo
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_getColumnNo(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_getColumnNo(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Column_ct, ttrait_int, &NdbDictionary::Column::getColumnNo >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Column_cr, &NdbApiWrapper::NdbDictionary__Column__getColumnNo >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Column
 * Method:    equal
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/ColumnConst;)Z
 */
JNIEXPORT jboolean JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_equal(JNIEnv * env, jobject obj, jobject p0)
{
    TRACE("jboolean Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_equal(JNIEnv *, jobject, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Column_ct, ttrait_bool, ttrait_c_m_n_n_NdbDictionary_Column_cr, &NdbDictionary::Column::equal >(env, obj, p0);
#else
    return gcall_fr< ttrait_bool, ttrait_c_m_n_n_NdbDictionary_Column_cr, ttrait_c_m_n_n_NdbDictionary_Column_cr, &NdbApiWrapper::NdbDictionary__Column__equal >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Column
 * Method:    getType
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_getType(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_getType(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Column_ct, ttrait_c_m_n_n_NdbDictionary_Column_Type_iv/*_enum_*/, &NdbDictionary::Column::getType >(env, obj);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbDictionary_Column_Type_iv/*_enum_*/, ttrait_c_m_n_n_NdbDictionary_Column_cr, &NdbApiWrapper::NdbDictionary__Column__getType >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Column
 * Method:    getPrecision
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_getPrecision(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_getPrecision(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Column_ct, ttrait_int, &NdbDictionary::Column::getPrecision >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Column_cr, &NdbApiWrapper::NdbDictionary__Column__getPrecision >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Column
 * Method:    getScale
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_getScale(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_getScale(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Column_ct, ttrait_int, &NdbDictionary::Column::getScale >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Column_cr, &NdbApiWrapper::NdbDictionary__Column__getScale >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Column
 * Method:    getLength
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_getLength(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_getLength(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Column_ct, ttrait_int, &NdbDictionary::Column::getLength >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Column_cr, &NdbApiWrapper::NdbDictionary__Column__getLength >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Column
 * Method:    getCharsetNumber
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_getCharsetNumber(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_getCharsetNumber(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Column_ct, ttrait_int, &NdbDictionary::Column::getCharsetNumber >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Column_cr, &NdbApiWrapper::NdbDictionary__Column__getCharsetNumber >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Column
 * Method:    getInlineSize
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_getInlineSize(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_getInlineSize(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Column_ct, ttrait_int, &NdbDictionary::Column::getInlineSize >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Column_cr, &NdbApiWrapper::NdbDictionary__Column__getInlineSize >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Column
 * Method:    getPartSize
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_getPartSize(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_getPartSize(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Column_ct, ttrait_int, &NdbDictionary::Column::getPartSize >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Column_cr, &NdbApiWrapper::NdbDictionary__Column__getPartSize >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Column
 * Method:    getStripeSize
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_getStripeSize(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_getStripeSize(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Column_ct, ttrait_int, &NdbDictionary::Column::getStripeSize >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Column_cr, &NdbApiWrapper::NdbDictionary__Column__getStripeSize >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Column
 * Method:    getSize
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_getSize(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_getSize(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Column_ct, ttrait_int, &NdbDictionary::Column::getSize >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Column_cr, &NdbApiWrapper::NdbDictionary__Column__getSize >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Column
 * Method:    getPartitionKey
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_getPartitionKey(JNIEnv * env, jobject obj)
{
    TRACE("jboolean Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_getPartitionKey(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Column_ct, ttrait_bool, &NdbDictionary::Column::getPartitionKey >(env, obj);
#else
    return gcall_fr< ttrait_bool, ttrait_c_m_n_n_NdbDictionary_Column_cr, &NdbApiWrapper::NdbDictionary__Column__getPartitionKey >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Column
 * Method:    getArrayType
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_getArrayType(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_getArrayType(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Column_ct, ttrait_c_m_n_n_NdbDictionary_Column_ArrayType_iv/*_enum_*/, &NdbDictionary::Column::getArrayType >(env, obj);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbDictionary_Column_ArrayType_iv/*_enum_*/, ttrait_c_m_n_n_NdbDictionary_Column_cr, &NdbApiWrapper::NdbDictionary__Column__getArrayType >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Column
 * Method:    getStorageType
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_getStorageType(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_getStorageType(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Column_ct, ttrait_c_m_n_n_NdbDictionary_Column_StorageType_iv/*_enum_*/, &NdbDictionary::Column::getStorageType >(env, obj);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbDictionary_Column_StorageType_iv/*_enum_*/, ttrait_c_m_n_n_NdbDictionary_Column_cr, &NdbApiWrapper::NdbDictionary__Column__getStorageType >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Column
 * Method:    getDynamic
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_getDynamic(JNIEnv * env, jobject obj)
{
    TRACE("jboolean Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_getDynamic(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Column_ct, ttrait_bool, &NdbDictionary::Column::getDynamic >(env, obj);
#else
    return gcall_fr< ttrait_bool, ttrait_c_m_n_n_NdbDictionary_Column_cr, &NdbApiWrapper::NdbDictionary__Column__getDynamic >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Column
 * Method:    getIndexSourced
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_getIndexSourced(JNIEnv * env, jobject obj)
{
    TRACE("jboolean Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_getIndexSourced(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Column_ct, ttrait_bool, &NdbDictionary::Column::getIndexSourced >(env, obj);
#else
    return gcall_fr< ttrait_bool, ttrait_c_m_n_n_NdbDictionary_Column_cr, &NdbApiWrapper::NdbDictionary__Column__getIndexSourced >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Column
 * Method:    create
 * Signature: (Ljava/lang/String;)Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/Column;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_create__Ljava_lang_String_2(JNIEnv * env, jclass cls, jstring p0)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_create__Ljava_lang_String_2(JNIEnv *, jclass, jstring)");
    return gcreate< ttrait_c_m_n_n_NdbDictionary_Column_r, ttrait_char_cp_jutf8null >(env, cls, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Column
 * Method:    create
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/ColumnConst;)Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/Column;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_create__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024ColumnConst_2(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_create__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024ColumnConst_2(JNIEnv *, jclass, jobject)");
    return gcreate< ttrait_c_m_n_n_NdbDictionary_Column_r, ttrait_c_m_n_n_NdbDictionary_Column_cr >(env, cls, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Column
 * Method:    delete
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/Column;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_delete(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_delete(JNIEnv *, jclass, jobject)");
    gdelete< ttrait_c_m_n_n_NdbDictionary_Column_r >(env, cls, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Column
 * Method:    setName
 * Signature: (Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_setName(JNIEnv * env, jobject obj, jstring p0)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_setName(JNIEnv *, jobject, jstring)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Column_t, ttrait_int, ttrait_char_cp_jutf8null, &NdbDictionary::Column::setName >(env, obj, p0);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Column_r, ttrait_char_cp_jutf8null, &NdbApiWrapper::NdbDictionary__Column__setName >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Column
 * Method:    setNullable
 * Signature: (Z)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_setNullable(JNIEnv * env, jobject obj, jboolean p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_setNullable(JNIEnv *, jobject, jboolean)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Column_t, ttrait_bool, &NdbDictionary::Column::setNullable >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_Column_r, ttrait_bool, &NdbApiWrapper::NdbDictionary__Column__setNullable >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Column
 * Method:    setPrimaryKey
 * Signature: (Z)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_setPrimaryKey(JNIEnv * env, jobject obj, jboolean p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_setPrimaryKey(JNIEnv *, jobject, jboolean)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Column_t, ttrait_bool, &NdbDictionary::Column::setPrimaryKey >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_Column_r, ttrait_bool, &NdbApiWrapper::NdbDictionary__Column__setPrimaryKey >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Column
 * Method:    setType
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_setType(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_setType(JNIEnv *, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Column_t, ttrait_c_m_n_n_NdbDictionary_Column_Type_iv/*_enum_*/, &NdbDictionary::Column::setType >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_Column_r, ttrait_c_m_n_n_NdbDictionary_Column_Type_iv/*_enum_*/, &NdbApiWrapper::NdbDictionary__Column__setType >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Column
 * Method:    setPrecision
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_setPrecision(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_setPrecision(JNIEnv *, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Column_t, ttrait_int, &NdbDictionary::Column::setPrecision >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_Column_r, ttrait_int, &NdbApiWrapper::NdbDictionary__Column__setPrecision >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Column
 * Method:    setScale
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_setScale(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_setScale(JNIEnv *, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Column_t, ttrait_int, &NdbDictionary::Column::setScale >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_Column_r, ttrait_int, &NdbApiWrapper::NdbDictionary__Column__setScale >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Column
 * Method:    setLength
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_setLength(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_setLength(JNIEnv *, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Column_t, ttrait_int, &NdbDictionary::Column::setLength >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_Column_r, ttrait_int, &NdbApiWrapper::NdbDictionary__Column__setLength >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Column
 * Method:    setInlineSize
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_setInlineSize(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_setInlineSize(JNIEnv *, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Column_t, ttrait_int, &NdbDictionary::Column::setInlineSize >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_Column_r, ttrait_int, &NdbApiWrapper::NdbDictionary__Column__setInlineSize >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Column
 * Method:    setPartSize
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_setPartSize(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_setPartSize(JNIEnv *, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Column_t, ttrait_int, &NdbDictionary::Column::setPartSize >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_Column_r, ttrait_int, &NdbApiWrapper::NdbDictionary__Column__setPartSize >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Column
 * Method:    setStripeSize
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_setStripeSize(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_setStripeSize(JNIEnv *, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Column_t, ttrait_int, &NdbDictionary::Column::setStripeSize >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_Column_r, ttrait_int, &NdbApiWrapper::NdbDictionary__Column__setStripeSize >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Column
 * Method:    setPartitionKey
 * Signature: (Z)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_setPartitionKey(JNIEnv * env, jobject obj, jboolean p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_setPartitionKey(JNIEnv *, jobject, jboolean)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Column_t, ttrait_bool, &NdbDictionary::Column::setPartitionKey >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_Column_r, ttrait_bool, &NdbApiWrapper::NdbDictionary__Column__setPartitionKey >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Column
 * Method:    setArrayType
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_setArrayType(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_setArrayType(JNIEnv *, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Column_t, ttrait_c_m_n_n_NdbDictionary_Column_ArrayType_iv/*_enum_*/, &NdbDictionary::Column::setArrayType >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_Column_r, ttrait_c_m_n_n_NdbDictionary_Column_ArrayType_iv/*_enum_*/, &NdbApiWrapper::NdbDictionary__Column__setArrayType >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Column
 * Method:    setStorageType
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_setStorageType(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_setStorageType(JNIEnv *, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Column_t, ttrait_c_m_n_n_NdbDictionary_Column_StorageType_iv/*_enum_*/, &NdbDictionary::Column::setStorageType >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_Column_r, ttrait_c_m_n_n_NdbDictionary_Column_StorageType_iv/*_enum_*/, &NdbApiWrapper::NdbDictionary__Column__setStorageType >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Column
 * Method:    setDynamic
 * Signature: (Z)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_setDynamic(JNIEnv * env, jobject obj, jboolean p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Column_setDynamic(JNIEnv *, jobject, jboolean)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Column_t, ttrait_bool, &NdbDictionary::Column::setDynamic >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_Column_r, ttrait_bool, &NdbApiWrapper::NdbDictionary__Column__setDynamic >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

// ---------------------------------------------------------------------------

//#include "com_mysql_ndbjtie_ndbapi_NdbDictionary_Datafile.h"

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Datafile
 * Method:    getPath
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Datafile_getPath(JNIEnv * env, jobject obj)
{
    TRACE("jstring Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Datafile_getPath(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Datafile_ct, ttrait_char_cp_jutf8null, &NdbDictionary::Datafile::getPath >(env, obj);
#else
    return gcall_fr< ttrait_char_cp_jutf8null, ttrait_c_m_n_n_NdbDictionary_Datafile_cr, &NdbApiWrapper::NdbDictionary__Datafile__getPath >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Datafile
 * Method:    getSize
 * Signature: ()J
 */
JNIEXPORT jlong JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Datafile_getSize(JNIEnv * env, jobject obj)
{
    TRACE("jlong Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Datafile_getSize(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Datafile_ct, ttrait_Uint64, &NdbDictionary::Datafile::getSize >(env, obj);
#else
    return gcall_fr< ttrait_Uint64, ttrait_c_m_n_n_NdbDictionary_Datafile_cr, &NdbApiWrapper::NdbDictionary__Datafile__getSize >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Datafile
 * Method:    getFree
 * Signature: ()J
 */
JNIEXPORT jlong JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Datafile_getFree(JNIEnv * env, jobject obj)
{
    TRACE("jlong Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Datafile_getFree(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Datafile_ct, ttrait_Uint64, &NdbDictionary::Datafile::getFree >(env, obj);
#else
    return gcall_fr< ttrait_Uint64, ttrait_c_m_n_n_NdbDictionary_Datafile_cr, &NdbApiWrapper::NdbDictionary__Datafile__getFree >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Datafile
 * Method:    getTablespace
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Datafile_getTablespace(JNIEnv * env, jobject obj)
{
    TRACE("jstring Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Datafile_getTablespace(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Datafile_ct, ttrait_char_cp_jutf8null, &NdbDictionary::Datafile::getTablespace >(env, obj);
#else
    return gcall_fr< ttrait_char_cp_jutf8null, ttrait_c_m_n_n_NdbDictionary_Datafile_cr, &NdbApiWrapper::NdbDictionary__Datafile__getTablespace >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Datafile
 * Method:    getTablespaceId
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/ObjectId;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Datafile_getTablespaceId(JNIEnv * env, jobject obj, jobject p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Datafile_getTablespaceId(JNIEnv *, jobject, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Datafile_ct, ttrait_c_m_n_n_NdbDictionary_ObjectId_p, &NdbDictionary::Datafile::getTablespaceId >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_Datafile_cr, ttrait_c_m_n_n_NdbDictionary_ObjectId_p, &NdbApiWrapper::NdbDictionary__Datafile__getTablespaceId >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Datafile
 * Method:    getObjectStatus
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Datafile_getObjectStatus(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Datafile_getObjectStatus(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Datafile_ct, ttrait_c_m_n_n_NdbDictionary_Object_Status_iv/*_enum_*/, &NdbDictionary::Datafile::getObjectStatus >(env, obj);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbDictionary_Object_Status_iv/*_enum_*/, ttrait_c_m_n_n_NdbDictionary_Datafile_cr, &NdbApiWrapper::NdbDictionary__Datafile__getObjectStatus >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Datafile
 * Method:    getObjectVersion
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Datafile_getObjectVersion(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Datafile_getObjectVersion(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Datafile_ct, ttrait_int, &NdbDictionary::Datafile::getObjectVersion >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Datafile_cr, &NdbApiWrapper::NdbDictionary__Datafile__getObjectVersion >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Datafile
 * Method:    getObjectId
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Datafile_getObjectId(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Datafile_getObjectId(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Datafile_ct, ttrait_int, &NdbDictionary::Datafile::getObjectId >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Datafile_cr, &NdbApiWrapper::NdbDictionary__Datafile__getObjectId >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Datafile
 * Method:    create
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/Datafile;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Datafile_create__(JNIEnv * env, jclass cls)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Datafile_create__(JNIEnv *, jclass)");
    return gcreate< ttrait_c_m_n_n_NdbDictionary_Datafile_r >(env, cls);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Datafile
 * Method:    create
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/DatafileConst;)Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/Datafile;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Datafile_create__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024DatafileConst_2(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Datafile_create__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024DatafileConst_2(JNIEnv *, jclass, jobject)");
    return gcreate< ttrait_c_m_n_n_NdbDictionary_Datafile_r, ttrait_c_m_n_n_NdbDictionary_Datafile_cr >(env, cls, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Datafile
 * Method:    delete
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/Datafile;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Datafile_delete(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Datafile_delete(JNIEnv *, jclass, jobject)");
    gdelete< ttrait_c_m_n_n_NdbDictionary_Datafile_r >(env, cls, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Datafile
 * Method:    setPath
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Datafile_setPath(JNIEnv * env, jobject obj, jstring p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Datafile_setPath(JNIEnv *, jobject, jstring)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Datafile_t, ttrait_char_cp_jutf8null, &NdbDictionary::Datafile::setPath >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_Datafile_r, ttrait_char_cp_jutf8null, &NdbApiWrapper::NdbDictionary__Datafile__setPath >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Datafile
 * Method:    setSize
 * Signature: (J)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Datafile_setSize(JNIEnv * env, jobject obj, jlong p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Datafile_setSize(JNIEnv *, jobject, jlong)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Datafile_t, ttrait_Uint64, &NdbDictionary::Datafile::setSize >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_Datafile_r, ttrait_Uint64, &NdbApiWrapper::NdbDictionary__Datafile__setSize >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Datafile
 * Method:    setTablespace
 * Signature: (Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Datafile_setTablespace__Ljava_lang_String_2(JNIEnv * env, jobject obj, jstring p0)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Datafile_setTablespace__Ljava_lang_String_2(JNIEnv *, jobject, jstring)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Datafile_t, ttrait_int, ttrait_char_cp_jutf8null, &NdbDictionary::Datafile::setTablespace >(env, obj, p0);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Datafile_r, ttrait_char_cp_jutf8null, &NdbApiWrapper::NdbDictionary__Datafile__setTablespace__0 >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Datafile
 * Method:    setTablespace
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/TablespaceConst;)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Datafile_setTablespace__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024TablespaceConst_2(JNIEnv * env, jobject obj, jobject p0)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Datafile_setTablespace__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024TablespaceConst_2(JNIEnv *, jobject, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Datafile_t, ttrait_int, ttrait_c_m_n_n_NdbDictionary_Tablespace_cr, &NdbDictionary::Datafile::setTablespace >(env, obj, p0);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Datafile_r, ttrait_c_m_n_n_NdbDictionary_Tablespace_cr, &NdbApiWrapper::NdbDictionary__Datafile__setTablespace__1 >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

// ---------------------------------------------------------------------------

//#include "com_mysql_ndbjtie_ndbapi_NdbDictionary_Dictionary.h"

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Dictionary
 * Method:    listObjects
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/DictionaryConst/List;I)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_listObjects(JNIEnv * env, jobject obj, jobject p0, jint p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_listObjects(JNIEnv *, jobject, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_CONST_OVERLOADED_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Dictionary_ct, ttrait_int, ttrait_c_m_n_n_NdbDictionary_DictionaryConst_List_r, ttrait_c_m_n_n_NdbDictionary_Object_Type_iv/*_enum_*/, &NdbDictionary::Dictionary::listObjects >(env, obj, p0, p1); // call of overloaded const/non-const method
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Dictionary_cr, ttrait_c_m_n_n_NdbDictionary_DictionaryConst_List_r, ttrait_c_m_n_n_NdbDictionary_Object_Type_iv/*_enum_*/, &NdbApiWrapper::NdbDictionary__Dictionary__listObjects__0 >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_CONST_OVERLOADED_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Dictionary
 * Method:    getNdbError
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbErrorConst;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_getNdbError(JNIEnv * env, jobject obj)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_getNdbError(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Dictionary_ct, ttrait_c_m_n_n_NdbError_cr, &NdbDictionary::Dictionary::getNdbError >(env, obj);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbError_cr, ttrait_c_m_n_n_NdbDictionary_Dictionary_cr, &NdbApiWrapper::NdbDictionary__Dictionary__getNdbError >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Dictionary
 * Method:    getTable
 * Signature: (Ljava/lang/String;)Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/TableConst;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_getTable(JNIEnv * env, jobject obj, jstring p0)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_getTable(JNIEnv *, jobject, jstring)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Dictionary_ct, ttrait_c_m_n_n_NdbDictionary_Table_cp, ttrait_char_cp_jutf8null, &NdbDictionary::Dictionary::getTable >(env, obj, p0);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbDictionary_Table_cp, ttrait_c_m_n_n_NdbDictionary_Dictionary_cr, ttrait_char_cp_jutf8null, &NdbApiWrapper::NdbDictionary__Dictionary__getTable >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Dictionary
 * Method:    getIndex
 * Signature: (Ljava/lang/String;Ljava/lang/String;)Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/IndexConst;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_getIndex(JNIEnv * env, jobject obj, jstring p0, jstring p1)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_getIndex(JNIEnv *, jobject, jstring, jstring)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Dictionary_ct, ttrait_c_m_n_n_NdbDictionary_Index_cp, ttrait_char_cp_jutf8null, ttrait_char_cp_jutf8null, &NdbDictionary::Dictionary::getIndex >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbDictionary_Index_cp, ttrait_c_m_n_n_NdbDictionary_Dictionary_cr, ttrait_char_cp_jutf8null, ttrait_char_cp_jutf8null, &NdbApiWrapper::NdbDictionary__Dictionary__getIndex >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Dictionary
 * Method:    listIndexes
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/DictionaryConst/List;Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_listIndexes(JNIEnv * env, jobject obj, jobject p0, jstring p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_listIndexes(JNIEnv *, jobject, jobject, jstring)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_CONST_OVERLOADED_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Dictionary_ct, ttrait_int, ttrait_c_m_n_n_NdbDictionary_DictionaryConst_List_r, ttrait_char_cp_jutf8null, &NdbDictionary::Dictionary::listIndexes >(env, obj, p0, p1); // call of overloaded const/non-const method
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Dictionary_cr, ttrait_c_m_n_n_NdbDictionary_DictionaryConst_List_r, ttrait_char_cp_jutf8null, &NdbApiWrapper::NdbDictionary__Dictionary__listIndexes__0 >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_CONST_OVERLOADED_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Dictionary
 * Method:    listEvents
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/DictionaryConst/List;)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_listEvents(JNIEnv * env, jobject obj, jobject p0)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_listEvents(JNIEnv *, jobject, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_CONST_OVERLOADED_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Dictionary_ct, ttrait_int, ttrait_c_m_n_n_NdbDictionary_DictionaryConst_List_r, &NdbDictionary::Dictionary::listEvents >(env, obj, p0); // call of overloaded const/non-const method
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Dictionary_cr, ttrait_c_m_n_n_NdbDictionary_DictionaryConst_List_r, &NdbApiWrapper::NdbDictionary__Dictionary__listEvents__0 >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_CONST_OVERLOADED_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Dictionary
 * Method:    createEvent
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/EventConst;)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_createEvent(JNIEnv * env, jobject obj, jobject p0)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_createEvent(JNIEnv *, jobject, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Dictionary_t, ttrait_int, ttrait_c_m_n_n_NdbDictionary_Event_cr, &NdbDictionary::Dictionary::createEvent >(env, obj, p0);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Dictionary_r, ttrait_c_m_n_n_NdbDictionary_Event_cr, &NdbApiWrapper::NdbDictionary__Dictionary__createEvent >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Dictionary
 * Method:    dropEvent
 * Signature: (Ljava/lang/String;I)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_dropEvent(JNIEnv * env, jobject obj, jstring p0, jint p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_dropEvent(JNIEnv *, jobject, jstring, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Dictionary_t, ttrait_int, ttrait_char_cp_jutf8null, ttrait_int, &NdbDictionary::Dictionary::dropEvent >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Dictionary_r, ttrait_char_cp_jutf8null, ttrait_int, &NdbApiWrapper::NdbDictionary__Dictionary__dropEvent >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Dictionary
 * Method:    getEvent
 * Signature: (Ljava/lang/String;)Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/EventConst;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_getEvent(JNIEnv * env, jobject obj, jstring p0)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_getEvent(JNIEnv *, jobject, jstring)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Dictionary_t, ttrait_c_m_n_n_NdbDictionary_Event_cp, ttrait_char_cp_jutf8null, &NdbDictionary::Dictionary::getEvent >(env, obj, p0);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbDictionary_Event_cp, ttrait_c_m_n_n_NdbDictionary_Dictionary_r, ttrait_char_cp_jutf8null, &NdbApiWrapper::NdbDictionary__Dictionary__getEvent >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Dictionary
 * Method:    createTable
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/TableConst;)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_createTable(JNIEnv * env, jobject obj, jobject p0)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_createTable(JNIEnv *, jobject, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Dictionary_t, ttrait_int, ttrait_c_m_n_n_NdbDictionary_Table_cr, &NdbDictionary::Dictionary::createTable >(env, obj, p0);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Dictionary_r, ttrait_c_m_n_n_NdbDictionary_Table_cr, &NdbApiWrapper::NdbDictionary__Dictionary__createTable >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Dictionary
 * Method:    optimizeTable
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/TableConst;Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/OptimizeTableHandle;)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_optimizeTable(JNIEnv * env, jobject obj, jobject p0, jobject p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_optimizeTable(JNIEnv *, jobject, jobject, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Dictionary_t, ttrait_int, ttrait_c_m_n_n_NdbDictionary_Table_cr, ttrait_c_m_n_n_NdbDictionary_OptimizeTableHandle_r, &NdbDictionary::Dictionary::optimizeTable >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Dictionary_r, ttrait_c_m_n_n_NdbDictionary_Table_cr, ttrait_c_m_n_n_NdbDictionary_OptimizeTableHandle_r, &NdbApiWrapper::NdbDictionary__Dictionary__optimizeTable >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Dictionary
 * Method:    optimizeIndex
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/IndexConst;Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/OptimizeIndexHandle;)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_optimizeIndex(JNIEnv * env, jobject obj, jobject p0, jobject p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_optimizeIndex(JNIEnv *, jobject, jobject, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Dictionary_t, ttrait_int, ttrait_c_m_n_n_NdbDictionary_Index_cr, ttrait_c_m_n_n_NdbDictionary_OptimizeIndexHandle_r, &NdbDictionary::Dictionary::optimizeIndex >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Dictionary_r, ttrait_c_m_n_n_NdbDictionary_Index_cr, ttrait_c_m_n_n_NdbDictionary_OptimizeIndexHandle_r, &NdbApiWrapper::NdbDictionary__Dictionary__optimizeIndex >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Dictionary
 * Method:    dropTable
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/Table;)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_dropTable__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_2(JNIEnv * env, jobject obj, jobject p0)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_dropTable__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_2(JNIEnv *, jobject, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Dictionary_t, ttrait_int, ttrait_c_m_n_n_NdbDictionary_Table_r, &NdbDictionary::Dictionary::dropTable >(env, obj, p0);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Dictionary_r, ttrait_c_m_n_n_NdbDictionary_Table_r, &NdbApiWrapper::NdbDictionary__Dictionary__dropTable__0 >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Dictionary
 * Method:    dropTable
 * Signature: (Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_dropTable__Ljava_lang_String_2(JNIEnv * env, jobject obj, jstring p0)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_dropTable__Ljava_lang_String_2(JNIEnv *, jobject, jstring)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Dictionary_t, ttrait_int, ttrait_char_cp_jutf8null, &NdbDictionary::Dictionary::dropTable >(env, obj, p0);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Dictionary_r, ttrait_char_cp_jutf8null, &NdbApiWrapper::NdbDictionary__Dictionary__dropTable__1 >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Dictionary
 * Method:    supportedAlterTable
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/TableConst;Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/TableConst;)Z
 */
JNIEXPORT jboolean JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_supportedAlterTable(JNIEnv * env, jobject obj, jobject p0, jobject p1)
{
    TRACE("jboolean Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_supportedAlterTable(JNIEnv *, jobject, jobject, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Dictionary_t, ttrait_bool, ttrait_c_m_n_n_NdbDictionary_Table_cr, ttrait_c_m_n_n_NdbDictionary_Table_cr, &NdbDictionary::Dictionary::supportedAlterTable >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_bool, ttrait_c_m_n_n_NdbDictionary_Dictionary_r, ttrait_c_m_n_n_NdbDictionary_Table_cr, ttrait_c_m_n_n_NdbDictionary_Table_cr, &NdbApiWrapper::NdbDictionary__Dictionary__supportedAlterTable >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Dictionary
 * Method:    removeCachedTable
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_removeCachedTable(JNIEnv * env, jobject obj, jstring p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_removeCachedTable(JNIEnv *, jobject, jstring)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Dictionary_t, ttrait_char_cp_jutf8null, &NdbDictionary::Dictionary::removeCachedTable >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_Dictionary_r, ttrait_char_cp_jutf8null, &NdbApiWrapper::NdbDictionary__Dictionary__removeCachedTable__0 >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Dictionary
 * Method:    removeCachedIndex
 * Signature: (Ljava/lang/String;Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_removeCachedIndex(JNIEnv * env, jobject obj, jstring p0, jstring p1)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_removeCachedIndex(JNIEnv *, jobject, jstring, jstring)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Dictionary_t, ttrait_char_cp_jutf8null, ttrait_char_cp_jutf8null, &NdbDictionary::Dictionary::removeCachedIndex >(env, obj, p0, p1);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_Dictionary_r, ttrait_char_cp_jutf8null, ttrait_char_cp_jutf8null, &NdbApiWrapper::NdbDictionary__Dictionary__removeCachedIndex__1 >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Dictionary
 * Method:    invalidateTable
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_invalidateTable(JNIEnv * env, jobject obj, jstring p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_invalidateTable(JNIEnv *, jobject, jstring)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Dictionary_t, ttrait_char_cp_jutf8null, &NdbDictionary::Dictionary::invalidateTable >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_Dictionary_r, ttrait_char_cp_jutf8null, &NdbApiWrapper::NdbDictionary__Dictionary__invalidateTable__0 >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Dictionary
 * Method:    invalidateIndex
 * Signature: (Ljava/lang/String;Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_invalidateIndex(JNIEnv * env, jobject obj, jstring p0, jstring p1)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_invalidateIndex(JNIEnv *, jobject, jstring, jstring)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Dictionary_t, ttrait_char_cp_jutf8null, ttrait_char_cp_jutf8null, &NdbDictionary::Dictionary::invalidateIndex >(env, obj, p0, p1);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_Dictionary_r, ttrait_char_cp_jutf8null, ttrait_char_cp_jutf8null, &NdbApiWrapper::NdbDictionary__Dictionary__invalidateIndex__1 >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Dictionary
 * Method:    createIndex
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/IndexConst;Z)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_createIndex__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024IndexConst_2Z(JNIEnv * env, jobject obj, jobject p0, jboolean p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_createIndex__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024IndexConst_2Z(JNIEnv *, jobject, jobject, jboolean)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Dictionary_t, ttrait_int, ttrait_c_m_n_n_NdbDictionary_Index_cr, ttrait_bool, &NdbDictionary::Dictionary::createIndex >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Dictionary_r, ttrait_c_m_n_n_NdbDictionary_Index_cr, ttrait_bool, &NdbApiWrapper::NdbDictionary__Dictionary__createIndex__0 >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Dictionary
 * Method:    createIndex
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/IndexConst;Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/TableConst;Z)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_createIndex__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024IndexConst_2Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024TableConst_2Z(JNIEnv * env, jobject obj, jobject p0, jobject p1, jboolean p2)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_createIndex__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024IndexConst_2Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024TableConst_2Z(JNIEnv *, jobject, jobject, jobject, jboolean)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Dictionary_t, ttrait_int, ttrait_c_m_n_n_NdbDictionary_Index_cr, ttrait_c_m_n_n_NdbDictionary_Table_cr, ttrait_bool, &NdbDictionary::Dictionary::createIndex >(env, obj, p0, p1, p2);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Dictionary_r, ttrait_c_m_n_n_NdbDictionary_Index_cr, ttrait_c_m_n_n_NdbDictionary_Table_cr, ttrait_bool, &NdbApiWrapper::NdbDictionary__Dictionary__createIndex__1 >(env, NULL, obj, p0, p1, p2);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Dictionary
 * Method:    dropIndex
 * Signature: (Ljava/lang/String;Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_dropIndex(JNIEnv * env, jobject obj, jstring p0, jstring p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_dropIndex(JNIEnv *, jobject, jstring, jstring)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Dictionary_t, ttrait_int, ttrait_char_cp_jutf8null, ttrait_char_cp_jutf8null, &NdbDictionary::Dictionary::dropIndex >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Dictionary_r, ttrait_char_cp_jutf8null, ttrait_char_cp_jutf8null, &NdbApiWrapper::NdbDictionary__Dictionary__dropIndex >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Dictionary
 * Method:    createLogfileGroup
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/LogfileGroupConst;Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/ObjectId;)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_createLogfileGroup(JNIEnv * env, jobject obj, jobject p0, jobject p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_createLogfileGroup(JNIEnv *, jobject, jobject, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Dictionary_t, ttrait_int, ttrait_c_m_n_n_NdbDictionary_LogfileGroup_cr, ttrait_c_m_n_n_NdbDictionary_ObjectId_p, &NdbDictionary::Dictionary::createLogfileGroup >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Dictionary_r, ttrait_c_m_n_n_NdbDictionary_LogfileGroup_cr, ttrait_c_m_n_n_NdbDictionary_ObjectId_p, &NdbApiWrapper::NdbDictionary__Dictionary__createLogfileGroup >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Dictionary
 * Method:    dropLogfileGroup
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/LogfileGroupConst;)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_dropLogfileGroup(JNIEnv * env, jobject obj, jobject p0)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_dropLogfileGroup(JNIEnv *, jobject, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Dictionary_t, ttrait_int, ttrait_c_m_n_n_NdbDictionary_LogfileGroup_cr, &NdbDictionary::Dictionary::dropLogfileGroup >(env, obj, p0);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Dictionary_r, ttrait_c_m_n_n_NdbDictionary_LogfileGroup_cr, &NdbApiWrapper::NdbDictionary__Dictionary__dropLogfileGroup >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Dictionary
 * Method:    createTablespace
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/TablespaceConst;Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/ObjectId;)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_createTablespace(JNIEnv * env, jobject obj, jobject p0, jobject p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_createTablespace(JNIEnv *, jobject, jobject, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Dictionary_t, ttrait_int, ttrait_c_m_n_n_NdbDictionary_Tablespace_cr, ttrait_c_m_n_n_NdbDictionary_ObjectId_p, &NdbDictionary::Dictionary::createTablespace >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Dictionary_r, ttrait_c_m_n_n_NdbDictionary_Tablespace_cr, ttrait_c_m_n_n_NdbDictionary_ObjectId_p, &NdbApiWrapper::NdbDictionary__Dictionary__createTablespace >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Dictionary
 * Method:    dropTablespace
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/TablespaceConst;)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_dropTablespace(JNIEnv * env, jobject obj, jobject p0)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_dropTablespace(JNIEnv *, jobject, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Dictionary_t, ttrait_int, ttrait_c_m_n_n_NdbDictionary_Tablespace_cr, &NdbDictionary::Dictionary::dropTablespace >(env, obj, p0);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Dictionary_r, ttrait_c_m_n_n_NdbDictionary_Tablespace_cr, &NdbApiWrapper::NdbDictionary__Dictionary__dropTablespace >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Dictionary
 * Method:    createDatafile
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/DatafileConst;ZLcom/mysql/ndbjtie/ndbapi/NdbDictionary/ObjectId;)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_createDatafile(JNIEnv * env, jobject obj, jobject p0, jboolean p1, jobject p2)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_createDatafile(JNIEnv *, jobject, jobject, jboolean, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Dictionary_t, ttrait_int, ttrait_c_m_n_n_NdbDictionary_Datafile_cr, ttrait_bool, ttrait_c_m_n_n_NdbDictionary_ObjectId_p, &NdbDictionary::Dictionary::createDatafile >(env, obj, p0, p1, p2);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Dictionary_r, ttrait_c_m_n_n_NdbDictionary_Datafile_cr, ttrait_bool, ttrait_c_m_n_n_NdbDictionary_ObjectId_p, &NdbApiWrapper::NdbDictionary__Dictionary__createDatafile >(env, NULL, obj, p0, p1, p2);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Dictionary
 * Method:    dropDatafile
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/DatafileConst;)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_dropDatafile(JNIEnv * env, jobject obj, jobject p0)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_dropDatafile(JNIEnv *, jobject, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Dictionary_t, ttrait_int, ttrait_c_m_n_n_NdbDictionary_Datafile_cr, &NdbDictionary::Dictionary::dropDatafile >(env, obj, p0);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Dictionary_r, ttrait_c_m_n_n_NdbDictionary_Datafile_cr, &NdbApiWrapper::NdbDictionary__Dictionary__dropDatafile >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Dictionary
 * Method:    createUndofile
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/UndofileConst;ZLcom/mysql/ndbjtie/ndbapi/NdbDictionary/ObjectId;)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_createUndofile(JNIEnv * env, jobject obj, jobject p0, jboolean p1, jobject p2)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_createUndofile(JNIEnv *, jobject, jobject, jboolean, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Dictionary_t, ttrait_int, ttrait_c_m_n_n_NdbDictionary_Undofile_cr, ttrait_bool, ttrait_c_m_n_n_NdbDictionary_ObjectId_p, &NdbDictionary::Dictionary::createUndofile >(env, obj, p0, p1, p2);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Dictionary_r, ttrait_c_m_n_n_NdbDictionary_Undofile_cr, ttrait_bool, ttrait_c_m_n_n_NdbDictionary_ObjectId_p, &NdbApiWrapper::NdbDictionary__Dictionary__createUndofile >(env, NULL, obj, p0, p1, p2);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Dictionary
 * Method:    dropUndofile
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/UndofileConst;)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_dropUndofile(JNIEnv * env, jobject obj, jobject p0)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_dropUndofile(JNIEnv *, jobject, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Dictionary_t, ttrait_int, ttrait_c_m_n_n_NdbDictionary_Undofile_cr, &NdbDictionary::Dictionary::dropUndofile >(env, obj, p0);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Dictionary_r, ttrait_c_m_n_n_NdbDictionary_Undofile_cr, &NdbApiWrapper::NdbDictionary__Dictionary__dropUndofile >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Dictionary
 * Method:    createRecord
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/TableConst;Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/RecordSpecificationConstArray;III)Lcom/mysql/ndbjtie/ndbapi/NdbRecord;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_createRecord__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024TableConst_2Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024RecordSpecificationConstArray_2III(JNIEnv * env, jobject obj, jobject p0, jobject p1, jint p2, jint p3, jint p4)
{
    TRACE("Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_createRecord__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024TableConst_2Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024RecordSpecificationConstArray_2III(JNIEnv *, jobject, jobject, jobject, jint, jint, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Dictionary_t, ttrait_c_m_n_n_NdbRecord_p, ttrait_c_m_n_n_NdbDictionary_Table_cp, ttrait_c_m_n_n_NdbDictionary_RecordSpecificationArray_cp, ttrait_Uint32, ttrait_Uint32, ttrait_Uint32, &NdbDictionary::Dictionary::createRecord >(env, obj, p0, p1, p2, p3, p4);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbRecord_p, ttrait_c_m_n_n_NdbDictionary_Dictionary_r, ttrait_c_m_n_n_NdbDictionary_Table_cp, ttrait_c_m_n_n_NdbDictionary_RecordSpecificationArray_cp, ttrait_Uint32, ttrait_Uint32, ttrait_Uint32, &NdbApiWrapper::NdbDictionary__Dictionary__createRecord__0 >(env, NULL, obj, p0, p1, p2, p3, p4);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Dictionary
 * Method:    createRecord
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/IndexConst;Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/TableConst;Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/RecordSpecificationConstArray;III)Lcom/mysql/ndbjtie/ndbapi/NdbRecord;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_createRecord__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024IndexConst_2Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024TableConst_2Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024RecordSpecificationConstArray_2III(JNIEnv * env, jobject obj, jobject p0, jobject p1, jobject p2, jint p3, jint p4, jint p5)
{
    TRACE("Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_createRecord__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024IndexConst_2Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024TableConst_2Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024RecordSpecificationConstArray_2III(JNIEnv *, jobject, jobject, jobject, jobject, jint, jint, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Dictionary_t, ttrait_c_m_n_n_NdbRecord_p, ttrait_c_m_n_n_NdbDictionary_Index_cp, ttrait_c_m_n_n_NdbDictionary_Table_cp, ttrait_c_m_n_n_NdbDictionary_RecordSpecificationArray_cp, ttrait_Uint32, ttrait_Uint32, ttrait_Uint32, &NdbDictionary::Dictionary::createRecord >(env, obj, p0, p1, p2, p3, p4, p5);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbRecord_p, ttrait_c_m_n_n_NdbDictionary_Dictionary_r, ttrait_c_m_n_n_NdbDictionary_Index_cp, ttrait_c_m_n_n_NdbDictionary_Table_cp, ttrait_c_m_n_n_NdbDictionary_RecordSpecificationArray_cp, ttrait_Uint32, ttrait_Uint32, ttrait_Uint32, &NdbApiWrapper::NdbDictionary__Dictionary__createRecord__1 >(env, NULL, obj, p0, p1, p2, p3, p4, p5);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Dictionary
 * Method:    createRecord
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/IndexConst;Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/RecordSpecificationConstArray;III)Lcom/mysql/ndbjtie/ndbapi/NdbRecord;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_createRecord__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024IndexConst_2Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024RecordSpecificationConstArray_2III(JNIEnv * env, jobject obj, jobject p0, jobject p1, jint p2, jint p3, jint p4)
{
    TRACE("Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_createRecord__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024IndexConst_2Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024RecordSpecificationConstArray_2III(JNIEnv *, jobject, jobject, jobject, jint, jint, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Dictionary_t, ttrait_c_m_n_n_NdbRecord_p, ttrait_c_m_n_n_NdbDictionary_Index_cp, ttrait_c_m_n_n_NdbDictionary_RecordSpecificationArray_cp, ttrait_Uint32, ttrait_Uint32, ttrait_Uint32, &NdbDictionary::Dictionary::createRecord >(env, obj, p0, p1, p2, p3, p4);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbRecord_p, ttrait_c_m_n_n_NdbDictionary_Dictionary_r, ttrait_c_m_n_n_NdbDictionary_Index_cp, ttrait_c_m_n_n_NdbDictionary_RecordSpecificationArray_cp, ttrait_Uint32, ttrait_Uint32, ttrait_Uint32, &NdbApiWrapper::NdbDictionary__Dictionary__createRecord__2 >(env, NULL, obj, p0, p1, p2, p3, p4);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Dictionary
 * Method:    releaseRecord
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbRecord;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_releaseRecord(JNIEnv * env, jobject obj, jobject p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_releaseRecord(JNIEnv *, jobject, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Dictionary_t, ttrait_c_m_n_n_NdbRecord_p, &NdbDictionary::Dictionary::releaseRecord >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_Dictionary_r, ttrait_c_m_n_n_NdbRecord_p, &NdbApiWrapper::NdbDictionary__Dictionary__releaseRecord >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

// ---------------------------------------------------------------------------

//#include "com_mysql_ndbjtie_ndbapi_NdbDictionary_DictionaryConst_List.h"

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_DictionaryConst_List
 * Method:    count
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024DictionaryConst_00024List_count__(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024DictionaryConst_00024List_count__(JNIEnv *, jobject)");
    return gget< ttrait_c_m_n_n_NdbDictionary_DictionaryConst_List_t, ttrait_uint, &NdbDictionary::Dictionary::List::count >(env, obj);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_DictionaryConst_List
 * Method:    elements
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/DictionaryConst/ListConst/ElementArray;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024DictionaryConst_00024List_elements__(JNIEnv * env, jobject obj)
{
    TRACE("Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024DictionaryConst_00024List_elements__(JNIEnv *, jobject)");
    return gget< ttrait_c_m_n_n_NdbDictionary_DictionaryConst_List_t, ttrait_c_m_n_n_NdbDictionary_DictionaryConst_ListConst_ElementArray_p, &NdbDictionary::Dictionary::List::elements >(env, obj);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_DictionaryConst_List
 * Method:    count
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024DictionaryConst_00024List_count__I(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024DictionaryConst_00024List_count__I(JNIEnv *, jobject, jint)");
    gset< ttrait_c_m_n_n_NdbDictionary_DictionaryConst_List_t, ttrait_uint, &NdbDictionary::Dictionary::List::count >(env, obj, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_DictionaryConst_List
 * Method:    elements
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/DictionaryConst/ListConst/ElementArray;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024DictionaryConst_00024List_elements__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024DictionaryConst_00024ListConst_00024ElementArray_2(JNIEnv * env, jobject obj, jobject p0)
{
    TRACE("Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024DictionaryConst_00024List_elements__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024DictionaryConst_00024ListConst_00024ElementArray_2(JNIEnv *, jobject, jobject)");
    gset< ttrait_c_m_n_n_NdbDictionary_DictionaryConst_List_t, ttrait_c_m_n_n_NdbDictionary_DictionaryConst_ListConst_ElementArray_p, &NdbDictionary::Dictionary::List::elements >(env, obj, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_DictionaryConst_List
 * Method:    create
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/DictionaryConst/List;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024DictionaryConst_00024List_create(JNIEnv * env, jclass cls)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024DictionaryConst_00024List_create(JNIEnv *, jclass)");
    return gcreate< ttrait_c_m_n_n_NdbDictionary_DictionaryConst_List_r >(env, cls);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_DictionaryConst_List
 * Method:    delete
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/DictionaryConst/List;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024DictionaryConst_00024List_delete(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024DictionaryConst_00024List_delete(JNIEnv *, jclass, jobject)");
    gdelete< ttrait_c_m_n_n_NdbDictionary_DictionaryConst_List_r >(env, cls, p0);
}

// ---------------------------------------------------------------------------

//#include "com_mysql_ndbjtie_ndbapi_NdbDictionary_DictionaryConst_ListConst_Element.h"

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_DictionaryConst_ListConst_Element
 * Method:    id
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024DictionaryConst_00024ListConst_00024Element_id__(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024DictionaryConst_00024ListConst_00024Element_id__(JNIEnv *, jobject)");
    return gget< ttrait_c_m_n_n_NdbDictionary_DictionaryConst_ListConst_Element_t, ttrait_uint, &NdbDictionary::Dictionary::List::Element::id >(env, obj);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_DictionaryConst_ListConst_Element
 * Method:    type
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024DictionaryConst_00024ListConst_00024Element_type__(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024DictionaryConst_00024ListConst_00024Element_type__(JNIEnv *, jobject)");
    return gget< ttrait_c_m_n_n_NdbDictionary_DictionaryConst_ListConst_Element_t, ttrait_c_m_n_n_NdbDictionary_Object_Type_iv/*_enum_*/, &NdbDictionary::Dictionary::List::Element::type >(env, obj);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_DictionaryConst_ListConst_Element
 * Method:    state
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024DictionaryConst_00024ListConst_00024Element_state__(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024DictionaryConst_00024ListConst_00024Element_state__(JNIEnv *, jobject)");
    return gget< ttrait_c_m_n_n_NdbDictionary_DictionaryConst_ListConst_Element_t, ttrait_c_m_n_n_NdbDictionary_Object_State_iv/*_enum_*/, &NdbDictionary::Dictionary::List::Element::state >(env, obj);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_DictionaryConst_ListConst_Element
 * Method:    store
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024DictionaryConst_00024ListConst_00024Element_store__(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024DictionaryConst_00024ListConst_00024Element_store__(JNIEnv *, jobject)");
    return gget< ttrait_c_m_n_n_NdbDictionary_DictionaryConst_ListConst_Element_t, ttrait_c_m_n_n_NdbDictionary_Object_Store_iv/*_enum_*/, &NdbDictionary::Dictionary::List::Element::store >(env, obj);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_DictionaryConst_ListConst_Element
 * Method:    temp
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024DictionaryConst_00024ListConst_00024Element_temp__(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024DictionaryConst_00024ListConst_00024Element_temp__(JNIEnv *, jobject)");
    return gget< ttrait_c_m_n_n_NdbDictionary_DictionaryConst_ListConst_Element_t, ttrait_Uint32, &NdbDictionary::Dictionary::List::Element::temp >(env, obj);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_DictionaryConst_ListConst_Element
 * Method:    database
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024DictionaryConst_00024ListConst_00024Element_database__(JNIEnv * env, jobject obj)
{
    TRACE("jstring Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024DictionaryConst_00024ListConst_00024Element_database__(JNIEnv *, jobject)");
    return gget< ttrait_c_m_n_n_NdbDictionary_DictionaryConst_ListConst_Element_t, ttrait_char_p_jutf8null, &NdbDictionary::Dictionary::List::Element::database >(env, obj);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_DictionaryConst_ListConst_Element
 * Method:    schema
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024DictionaryConst_00024ListConst_00024Element_schema__(JNIEnv * env, jobject obj)
{
    TRACE("jstring Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024DictionaryConst_00024ListConst_00024Element_schema__(JNIEnv *, jobject)");
    return gget< ttrait_c_m_n_n_NdbDictionary_DictionaryConst_ListConst_Element_t, ttrait_char_p_jutf8null, &NdbDictionary::Dictionary::List::Element::schema >(env, obj);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_DictionaryConst_ListConst_Element
 * Method:    name
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024DictionaryConst_00024ListConst_00024Element_name__(JNIEnv * env, jobject obj)
{
    TRACE("jstring Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024DictionaryConst_00024ListConst_00024Element_name__(JNIEnv *, jobject)");
    return gget< ttrait_c_m_n_n_NdbDictionary_DictionaryConst_ListConst_Element_t, ttrait_char_p_jutf8null, &NdbDictionary::Dictionary::List::Element::name >(env, obj);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_DictionaryConst_ListConst_Element
 * Method:    id
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024DictionaryConst_00024ListConst_00024Element_id__I(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024DictionaryConst_00024ListConst_00024Element_id__I(JNIEnv *, jobject, jint)");
    gset< ttrait_c_m_n_n_NdbDictionary_DictionaryConst_ListConst_Element_t, ttrait_uint, &NdbDictionary::Dictionary::List::Element::id >(env, obj, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_DictionaryConst_ListConst_Element
 * Method:    type
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024DictionaryConst_00024ListConst_00024Element_type__I(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024DictionaryConst_00024ListConst_00024Element_type__I(JNIEnv *, jobject, jint)");
    gset< ttrait_c_m_n_n_NdbDictionary_DictionaryConst_ListConst_Element_t, ttrait_c_m_n_n_NdbDictionary_Object_Type_iv, &NdbDictionary::Dictionary::List::Element::type >(env, obj, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_DictionaryConst_ListConst_Element
 * Method:    state
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024DictionaryConst_00024ListConst_00024Element_state__I(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024DictionaryConst_00024ListConst_00024Element_state__I(JNIEnv *, jobject, jint)");
    gset< ttrait_c_m_n_n_NdbDictionary_DictionaryConst_ListConst_Element_t, ttrait_c_m_n_n_NdbDictionary_Object_State_iv/*_enum_*/, &NdbDictionary::Dictionary::List::Element::state >(env, obj, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_DictionaryConst_ListConst_Element
 * Method:    store
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024DictionaryConst_00024ListConst_00024Element_store__I(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024DictionaryConst_00024ListConst_00024Element_store__I(JNIEnv *, jobject, jint)");
    gset< ttrait_c_m_n_n_NdbDictionary_DictionaryConst_ListConst_Element_t, ttrait_c_m_n_n_NdbDictionary_Object_Store_iv/*_enum_*/, &NdbDictionary::Dictionary::List::Element::store >(env, obj, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_DictionaryConst_ListConst_Element
 * Method:    temp
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024DictionaryConst_00024ListConst_00024Element_temp__I(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024DictionaryConst_00024ListConst_00024Element_temp__I(JNIEnv *, jobject, jint)");
    gset< ttrait_c_m_n_n_NdbDictionary_DictionaryConst_ListConst_Element_t, ttrait_Uint32, &NdbDictionary::Dictionary::List::Element::temp >(env, obj, p0);
}

#if 0 // MMM unsupported mapping <in:String->char*> (and questionable NDBAPI usage)
/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_DictionaryConst_ListConst_Element
 * Method:    database
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024DictionaryConst_00024ListConst_00024Element_database__Ljava_lang_String_2(JNIEnv * env, jobject obj, jstring p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024DictionaryConst_00024ListConst_00024Element_database__Ljava_lang_String_2(JNIEnv *, jobject, jstring)");
    gset< ttrait_c_m_n_n_NdbDictionary_DictionaryConst_ListConst_Element_t, ttrait_utf8string, &NdbDictionary::Dictionary::List::Element::database >(env, obj, p0);
}
#endif // MMM unsupported mapping <in:String->char*> (and questionable NDBAPI usage)

#if 0 // MMM unsupported mapping <in:String->char*> (and questionable NDBAPI usage)
/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_DictionaryConst_ListConst_Element
 * Method:    schema
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024DictionaryConst_00024ListConst_00024Element_schema__Ljava_lang_String_2(JNIEnv * env, jobject obj, jstring p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024DictionaryConst_00024ListConst_00024Element_schema__Ljava_lang_String_2(JNIEnv *, jobject, jstring)");
    gset< ttrait_c_m_n_n_NdbDictionary_DictionaryConst_ListConst_Element_t, ttrait_utf8string, &NdbDictionary::Dictionary::List::Element::schema >(env, obj, p0);
}
#endif // MMM unsupported mapping <in:String->char*> (and questionable NDBAPI usage)

#if 0 // MMM unsupported mapping <in:String->char*> (and questionable NDBAPI usage)
/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_DictionaryConst_ListConst_Element
 * Method:    name
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024DictionaryConst_00024ListConst_00024Element_name__Ljava_lang_String_2(JNIEnv * env, jobject obj, jstring p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024DictionaryConst_00024ListConst_00024Element_name__Ljava_lang_String_2(JNIEnv *, jobject, jstring)");
    gset< ttrait_c_m_n_n_NdbDictionary_DictionaryConst_ListConst_Element_t, ttrait_utf8string, &NdbDictionary::Dictionary::List::Element::name >(env, obj, p0);
}
#endif // MMM unsupported mapping <in:String->char*> (and questionable NDBAPI usage)

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_DictionaryConst_ListConst_Element
 * Method:    create
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/DictionaryConst/ListConst/Element;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024DictionaryConst_00024ListConst_00024Element_create(JNIEnv * env, jclass cls)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024DictionaryConst_00024ListConst_00024Element_create(JNIEnv *, jclass)");
    return gcreate< ttrait_c_m_n_n_NdbDictionary_DictionaryConst_ListConst_Element_r >(env, cls);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_DictionaryConst_ListConst_Element
 * Method:    delete
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/DictionaryConst/ListConst/Element;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024DictionaryConst_00024ListConst_00024Element_delete(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024DictionaryConst_00024ListConst_00024Element_delete(JNIEnv *, jclass, jobject)");
    gdelete< ttrait_c_m_n_n_NdbDictionary_DictionaryConst_ListConst_Element_r >(env, cls, p0);
}

// ---------------------------------------------------------------------------

//#include "com_mysql_ndbjtie_ndbapi_NdbDictionary_DictionaryConst_ListConst_ElementArray.h"

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_DictionaryConst_ListConst_ElementArray
 * Method:    create
 * Signature: (I)Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/DictionaryConst/ListConst/ElementArray;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024DictionaryConst_00024ListConst_00024ElementArray_create(JNIEnv * env, jclass cls, jint p0)
{
    TRACE("Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024DictionaryConst_00024ListConst_00024ElementArray_create(JNIEnv *, jclass, jint)");
    return gcreateArray< ttrait_c_m_n_n_NdbDictionary_DictionaryConst_ListConst_ElementArray_r, ttrait_int32 >(env, cls, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_DictionaryConst_ListConst_ElementArray
 * Method:    delete
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/DictionaryConst/ListConst/ElementArray;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024DictionaryConst_00024ListConst_00024ElementArray_delete(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024DictionaryConst_00024ListConst_00024ElementArray_delete(JNIEnv *, jclass, jobject)");
    gdeleteArray< ttrait_c_m_n_n_NdbDictionary_DictionaryConst_ListConst_ElementArray_r >(env, cls, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_DictionaryConst_ListConst_ElementArray
 * Method:    at
 * Signature: (I)Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/DictionaryConst/ListConst/Element;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024DictionaryConst_00024ListConst_00024ElementArray_at(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024DictionaryConst_00024ListConst_00024ElementArray_at(JNIEnv *, jobject, jint)");
    return gat< ttrait_c_m_n_n_NdbDictionary_DictionaryConst_ListConst_Element_r, ttrait_c_m_n_n_NdbDictionary_DictionaryConst_ListConst_ElementArray_r, ttrait_int32 >(env, NULL, obj, p0);
}

// ---------------------------------------------------------------------------

//#include "com_mysql_ndbjtie_ndbapi_NdbDictionary_Event.h"

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Event
 * Method:    getName
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Event_getName(JNIEnv * env, jobject obj)
{
    TRACE("jstring Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Event_getName(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Event_ct, ttrait_char_cp_jutf8null, &NdbDictionary::Event::getName >(env, obj);
#else
    return gcall_fr< ttrait_char_cp_jutf8null, ttrait_c_m_n_n_NdbDictionary_Event_cr, &NdbApiWrapper::NdbDictionary__Event__getName >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Event
 * Method:    getTable
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/TableConst;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Event_getTable(JNIEnv * env, jobject obj)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Event_getTable(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Event_ct, ttrait_c_m_n_n_NdbDictionary_Table_cp, &NdbDictionary::Event::getTable >(env, obj);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbDictionary_Table_cp, ttrait_c_m_n_n_NdbDictionary_Event_cr, &NdbApiWrapper::NdbDictionary__Event__getTable >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Event
 * Method:    getTableName
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Event_getTableName(JNIEnv * env, jobject obj)
{
    TRACE("jstring Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Event_getTableName(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Event_ct, ttrait_char_cp_jutf8null, &NdbDictionary::Event::getTableName >(env, obj);
#else
    return gcall_fr< ttrait_char_cp_jutf8null, ttrait_c_m_n_n_NdbDictionary_Event_cr, &NdbApiWrapper::NdbDictionary__Event__getTableName >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Event
 * Method:    getTableEvent
 * Signature: (I)Z
 */
JNIEXPORT jboolean JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Event_getTableEvent(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("jboolean Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Event_getTableEvent(JNIEnv *, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Event_ct, ttrait_bool, ttrait_c_m_n_n_NdbDictionary_Event_TableEvent_iv/*_enum_*/, &NdbDictionary::Event::getTableEvent >(env, obj, p0);
#else
    return gcall_fr< ttrait_bool, ttrait_c_m_n_n_NdbDictionary_Event_cr, ttrait_c_m_n_n_NdbDictionary_Event_TableEvent_iv/*_enum_*/, &NdbApiWrapper::NdbDictionary__Event__getTableEvent >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Event
 * Method:    getDurability
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Event_getDurability(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Event_getDurability(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Event_ct, ttrait_c_m_n_n_NdbDictionary_Event_EventDurability_iv/*_enum_*/, &NdbDictionary::Event::getDurability >(env, obj);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbDictionary_Event_EventDurability_iv/*_enum_*/, ttrait_c_m_n_n_NdbDictionary_Event_cr, &NdbApiWrapper::NdbDictionary__Event__getDurability >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Event
 * Method:    getReport
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Event_getReport(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Event_getReport(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Event_ct, ttrait_c_m_n_n_NdbDictionary_Event_EventReport_iv/*_enum_*/, &NdbDictionary::Event::getReport >(env, obj);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbDictionary_Event_EventReport_iv/*_enum_*/, ttrait_c_m_n_n_NdbDictionary_Event_cr, &NdbApiWrapper::NdbDictionary__Event__getReport >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Event
 * Method:    getNoOfEventColumns
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Event_getNoOfEventColumns(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Event_getNoOfEventColumns(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Event_ct, ttrait_int, &NdbDictionary::Event::getNoOfEventColumns >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Event_cr, &NdbApiWrapper::NdbDictionary__Event__getNoOfEventColumns >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Event
 * Method:    getEventColumn
 * Signature: (I)Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/ColumnConst;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Event_getEventColumn(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Event_getEventColumn(JNIEnv *, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Event_ct, ttrait_c_m_n_n_NdbDictionary_Column_cp, ttrait_uint, &NdbDictionary::Event::getEventColumn >(env, obj, p0);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbDictionary_Column_cp, ttrait_c_m_n_n_NdbDictionary_Event_cr, ttrait_uint, &NdbApiWrapper::NdbDictionary__Event__getEventColumn >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Event
 * Method:    getObjectStatus
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Event_getObjectStatus(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Event_getObjectStatus(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Event_ct, ttrait_c_m_n_n_NdbDictionary_Object_Status_iv/*_enum_*/, &NdbDictionary::Event::getObjectStatus >(env, obj);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbDictionary_Object_Status_iv/*_enum_*/, ttrait_c_m_n_n_NdbDictionary_Event_cr, &NdbApiWrapper::NdbDictionary__Event__getObjectStatus >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Event
 * Method:    getObjectVersion
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Event_getObjectVersion(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Event_getObjectVersion(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Event_ct, ttrait_int, &NdbDictionary::Event::getObjectVersion >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Event_cr, &NdbApiWrapper::NdbDictionary__Event__getObjectVersion >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Event
 * Method:    getObjectId
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Event_getObjectId(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Event_getObjectId(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Event_ct, ttrait_int, &NdbDictionary::Event::getObjectId >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Event_cr, &NdbApiWrapper::NdbDictionary__Event__getObjectId >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Event
 * Method:    create
 * Signature: (Ljava/lang/String;)Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/Event;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Event_create__Ljava_lang_String_2(JNIEnv * env, jclass cls, jstring p0)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Event_create__Ljava_lang_String_2(JNIEnv *, jclass, jstring)");
    return gcreate< ttrait_c_m_n_n_NdbDictionary_Event_r, ttrait_char_cp_jutf8null >(env, cls, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Event
 * Method:    create
 * Signature: (Ljava/lang/String;Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/TableConst;)Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/Event;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Event_create__Ljava_lang_String_2Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024TableConst_2(JNIEnv * env, jclass cls, jstring p0, jobject p1)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Event_create__Ljava_lang_String_2Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024TableConst_2(JNIEnv *, jclass, jstring, jobject)");
    return gcreate< ttrait_c_m_n_n_NdbDictionary_Event_r, ttrait_char_cp_jutf8null, ttrait_c_m_n_n_NdbDictionary_Table_cr >(env, cls, p0, p1);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Event
 * Method:    delete
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/Event;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Event_delete(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Event_delete(JNIEnv *, jclass, jobject)");
    gdelete< ttrait_c_m_n_n_NdbDictionary_Event_r >(env, cls, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Event
 * Method:    setName
 * Signature: (Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Event_setName(JNIEnv * env, jobject obj, jstring p0)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Event_setName(JNIEnv *, jobject, jstring)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Event_t, ttrait_int, ttrait_char_cp_jutf8null, &NdbDictionary::Event::setName >(env, obj, p0);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Event_r, ttrait_char_cp_jutf8null, &NdbApiWrapper::NdbDictionary__Event__setName >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Event
 * Method:    setTable
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/TableConst;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Event_setTable__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024TableConst_2(JNIEnv * env, jobject obj, jobject p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Event_setTable__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024TableConst_2(JNIEnv *, jobject, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Event_t, ttrait_c_m_n_n_NdbDictionary_Table_cr, &NdbDictionary::Event::setTable >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_Event_r, ttrait_c_m_n_n_NdbDictionary_Table_cr, &NdbApiWrapper::NdbDictionary__Event__setTable__0 >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Event
 * Method:    setTable
 * Signature: (Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Event_setTable__Ljava_lang_String_2(JNIEnv * env, jobject obj, jstring p0)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Event_setTable__Ljava_lang_String_2(JNIEnv *, jobject, jstring)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Event_t, ttrait_int, ttrait_char_cp_jutf8null, &NdbDictionary::Event::setTable >(env, obj, p0);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Event_r, ttrait_char_cp_jutf8null, &NdbApiWrapper::NdbDictionary__Event__setTable__1 >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Event
 * Method:    addTableEvent
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Event_addTableEvent(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Event_addTableEvent(JNIEnv *, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Event_t, ttrait_c_m_n_n_NdbDictionary_Event_TableEvent_iv/*_enum_*/, &NdbDictionary::Event::addTableEvent >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_Event_r, ttrait_c_m_n_n_NdbDictionary_Event_TableEvent_iv/*_enum_*/, &NdbApiWrapper::NdbDictionary__Event__addTableEvent >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Event
 * Method:    setDurability
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Event_setDurability(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Event_setDurability(JNIEnv *, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Event_t, ttrait_c_m_n_n_NdbDictionary_Event_EventDurability_iv/*_enum_*/, &NdbDictionary::Event::setDurability >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_Event_r, ttrait_c_m_n_n_NdbDictionary_Event_EventDurability_iv/*_enum_*/, &NdbApiWrapper::NdbDictionary__Event__setDurability >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Event
 * Method:    setReport
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Event_setReport(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Event_setReport(JNIEnv *, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Event_t, ttrait_c_m_n_n_NdbDictionary_Event_EventReport_iv/*_enum_*/, &NdbDictionary::Event::setReport >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_Event_r, ttrait_c_m_n_n_NdbDictionary_Event_EventReport_iv/*_enum_*/, &NdbApiWrapper::NdbDictionary__Event__setReport >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Event
 * Method:    addEventColumn
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Event_addEventColumn__I(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Event_addEventColumn__I(JNIEnv *, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Event_t, ttrait_uint, &NdbDictionary::Event::addEventColumn >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_Event_r, ttrait_uint, &NdbApiWrapper::NdbDictionary__Event__addEventColumn__0 >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Event
 * Method:    addEventColumn
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Event_addEventColumn__Ljava_lang_String_2(JNIEnv * env, jobject obj, jstring p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Event_addEventColumn__Ljava_lang_String_2(JNIEnv *, jobject, jstring)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Event_t, ttrait_char_cp_jutf8null, &NdbDictionary::Event::addEventColumn >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_Event_r, ttrait_char_cp_jutf8null, &NdbApiWrapper::NdbDictionary__Event__addEventColumn__1 >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

#if 0 // MMM! support <in:String[]>, error: parse error in template argument list
/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Event
 * Method:    addEventColumns
 * Signature: (I[Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Event_addEventColumns(JNIEnv * env, jobject obj, jint p0, jobjectArray p1)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Event_addEventColumns(JNIEnv *, jobject, jint, jobjectArray)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Event_t, ttrait_int, String[]/*_const char * *_*/, &NdbDictionary::Event::addEventColumns >(env, obj, p0, p1);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_Event_r, ttrait_int, String[]/*_const char * *_*/, &NdbApiWrapper::NdbDictionary__Event__addEventColumns >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}
#endif // MMM! support <in:String[]>, error: parse error in template argument list

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Event
 * Method:    mergeEvents
 * Signature: (Z)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Event_mergeEvents(JNIEnv * env, jobject obj, jboolean p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Event_mergeEvents(JNIEnv *, jobject, jboolean)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Event_t, ttrait_bool, &NdbDictionary::Event::mergeEvents >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_Event_r, ttrait_bool, &NdbApiWrapper::NdbDictionary__Event__mergeEvents >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

// ---------------------------------------------------------------------------

//#include "com_mysql_ndbjtie_ndbapi_NdbDictionary_Index.h"

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Index
 * Method:    getName
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Index_getName(JNIEnv * env, jobject obj)
{
    TRACE("jstring Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Index_getName(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Index_ct, ttrait_char_cp_jutf8null, &NdbDictionary::Index::getName >(env, obj);
#else
    return gcall_fr< ttrait_char_cp_jutf8null, ttrait_c_m_n_n_NdbDictionary_Index_cr, &NdbApiWrapper::NdbDictionary__Index__getName >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Index
 * Method:    getTable
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Index_getTable(JNIEnv * env, jobject obj)
{
    TRACE("jstring Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Index_getTable(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Index_ct, ttrait_char_cp_jutf8null, &NdbDictionary::Index::getTable >(env, obj);
#else
    return gcall_fr< ttrait_char_cp_jutf8null, ttrait_c_m_n_n_NdbDictionary_Index_cr, &NdbApiWrapper::NdbDictionary__Index__getTable >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Index
 * Method:    getNoOfColumns
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Index_getNoOfColumns(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Index_getNoOfColumns(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Index_ct, ttrait_uint, &NdbDictionary::Index::getNoOfColumns >(env, obj);
#else
    return gcall_fr< ttrait_uint, ttrait_c_m_n_n_NdbDictionary_Index_cr, &NdbApiWrapper::NdbDictionary__Index__getNoOfColumns >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Index
 * Method:    getColumn
 * Signature: (I)Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/ColumnConst;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Index_getColumn(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Index_getColumn(JNIEnv *, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Index_ct, ttrait_c_m_n_n_NdbDictionary_Column_cp, ttrait_uint, &NdbDictionary::Index::getColumn >(env, obj, p0);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbDictionary_Column_cp, ttrait_c_m_n_n_NdbDictionary_Index_cr, ttrait_uint, &NdbApiWrapper::NdbDictionary__Index__getColumn >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Index
 * Method:    getType
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Index_getType(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Index_getType(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Index_ct, ttrait_c_m_n_n_NdbDictionary_Index_Type_iv/*_enum_*/, &NdbDictionary::Index::getType >(env, obj);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbDictionary_Index_Type_iv/*_enum_*/, ttrait_c_m_n_n_NdbDictionary_Index_cr, &NdbApiWrapper::NdbDictionary__Index__getType >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Index
 * Method:    getLogging
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Index_getLogging(JNIEnv * env, jobject obj)
{
    TRACE("jboolean Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Index_getLogging(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Index_ct, ttrait_bool, &NdbDictionary::Index::getLogging >(env, obj);
#else
    return gcall_fr< ttrait_bool, ttrait_c_m_n_n_NdbDictionary_Index_cr, &NdbApiWrapper::NdbDictionary__Index__getLogging >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Index
 * Method:    getObjectStatus
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Index_getObjectStatus(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Index_getObjectStatus(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Index_ct, ttrait_c_m_n_n_NdbDictionary_Object_Status_iv/*_enum_*/, &NdbDictionary::Index::getObjectStatus >(env, obj);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbDictionary_Object_Status_iv/*_enum_*/, ttrait_c_m_n_n_NdbDictionary_Index_cr, &NdbApiWrapper::NdbDictionary__Index__getObjectStatus >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Index
 * Method:    getObjectVersion
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Index_getObjectVersion(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Index_getObjectVersion(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Index_ct, ttrait_int, &NdbDictionary::Index::getObjectVersion >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Index_cr, &NdbApiWrapper::NdbDictionary__Index__getObjectVersion >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Index
 * Method:    getObjectId
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Index_getObjectId(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Index_getObjectId(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Index_ct, ttrait_int, &NdbDictionary::Index::getObjectId >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Index_cr, &NdbApiWrapper::NdbDictionary__Index__getObjectId >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Index
 * Method:    getDefaultRecord
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbRecordConst;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Index_getDefaultRecord(JNIEnv * env, jobject obj)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Index_getDefaultRecord(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Index_ct, ttrait_c_m_n_n_NdbRecord_cp, &NdbDictionary::Index::getDefaultRecord >(env, obj);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbRecord_cp, ttrait_c_m_n_n_NdbDictionary_Index_cr, &NdbApiWrapper::NdbDictionary__Index__getDefaultRecord >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Index
 * Method:    create
 * Signature: (Ljava/lang/String;)Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/Index;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Index_create(JNIEnv * env, jclass cls, jstring p0)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Index_create(JNIEnv *, jclass, jstring)");
    return gcreate< ttrait_c_m_n_n_NdbDictionary_Index_r, ttrait_char_cp_jutf8null >(env, cls, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Index
 * Method:    delete
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/Index;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Index_delete(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Index_delete(JNIEnv *, jclass, jobject)");
    gdelete< ttrait_c_m_n_n_NdbDictionary_Index_r >(env, cls, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Index
 * Method:    setName
 * Signature: (Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Index_setName(JNIEnv * env, jobject obj, jstring p0)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Index_setName(JNIEnv *, jobject, jstring)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Index_t, ttrait_int, ttrait_char_cp_jutf8null, &NdbDictionary::Index::setName >(env, obj, p0);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Index_r, ttrait_char_cp_jutf8null, &NdbApiWrapper::NdbDictionary__Index__setName >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Index
 * Method:    setTable
 * Signature: (Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Index_setTable(JNIEnv * env, jobject obj, jstring p0)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Index_setTable(JNIEnv *, jobject, jstring)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Index_t, ttrait_int, ttrait_char_cp_jutf8null, &NdbDictionary::Index::setTable >(env, obj, p0);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Index_r, ttrait_char_cp_jutf8null, &NdbApiWrapper::NdbDictionary__Index__setTable >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Index
 * Method:    addColumn
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/ColumnConst;)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Index_addColumn(JNIEnv * env, jobject obj, jobject p0)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Index_addColumn(JNIEnv *, jobject, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Index_t, ttrait_int, ttrait_c_m_n_n_NdbDictionary_Column_cr, &NdbDictionary::Index::addColumn >(env, obj, p0);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Index_r, ttrait_c_m_n_n_NdbDictionary_Column_cr, &NdbApiWrapper::NdbDictionary__Index__addColumn >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Index
 * Method:    addColumnName
 * Signature: (Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Index_addColumnName(JNIEnv * env, jobject obj, jstring p0)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Index_addColumnName(JNIEnv *, jobject, jstring)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Index_t, ttrait_int, ttrait_char_cp_jutf8null, &NdbDictionary::Index::addColumnName >(env, obj, p0);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Index_r, ttrait_char_cp_jutf8null, &NdbApiWrapper::NdbDictionary__Index__addColumnName >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

#if 0 // MMM! support <in:String[]>, error: parse error in template argument list
/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Index
 * Method:    addColumnNames
 * Signature: (I[Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Index_addColumnNames(JNIEnv * env, jobject obj, jint p0, jobjectArray p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Index_addColumnNames(JNIEnv *, jobject, jint, jobjectArray)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Index_t, ttrait_int, ttrait_uint, String[]/*_const char * *_*/, &NdbDictionary::Index::addColumnNames >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Index_r, ttrait_uint, String[]/*_const char * *_*/, &NdbApiWrapper::NdbDictionary__Index__addColumnNames >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}
#endif // MMM! support <in:String[]>, error: parse error in template argument list

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Index
 * Method:    setType
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Index_setType(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Index_setType(JNIEnv *, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Index_t, ttrait_c_m_n_n_NdbDictionary_Index_Type_iv/*_enum_*/, &NdbDictionary::Index::setType >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_Index_r, ttrait_c_m_n_n_NdbDictionary_Index_Type_iv/*_enum_*/, &NdbApiWrapper::NdbDictionary__Index__setType >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Index
 * Method:    setLogging
 * Signature: (Z)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Index_setLogging(JNIEnv * env, jobject obj, jboolean p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Index_setLogging(JNIEnv *, jobject, jboolean)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Index_t, ttrait_bool, &NdbDictionary::Index::setLogging >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_Index_r, ttrait_bool, &NdbApiWrapper::NdbDictionary__Index__setLogging >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

// ---------------------------------------------------------------------------

//#include "com_mysql_ndbjtie_ndbapi_NdbDictionary_LogfileGroup.h"

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_LogfileGroup
 * Method:    getName
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024LogfileGroup_getName(JNIEnv * env, jobject obj)
{
    TRACE("jstring Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024LogfileGroup_getName(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_LogfileGroup_ct, ttrait_char_cp_jutf8null, &NdbDictionary::LogfileGroup::getName >(env, obj);
#else
    return gcall_fr< ttrait_char_cp_jutf8null, ttrait_c_m_n_n_NdbDictionary_LogfileGroup_cr, &NdbApiWrapper::NdbDictionary__LogfileGroup__getName >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_LogfileGroup
 * Method:    getUndoBufferSize
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024LogfileGroup_getUndoBufferSize(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024LogfileGroup_getUndoBufferSize(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_LogfileGroup_ct, ttrait_Uint32, &NdbDictionary::LogfileGroup::getUndoBufferSize >(env, obj);
#else
    return gcall_fr< ttrait_Uint32, ttrait_c_m_n_n_NdbDictionary_LogfileGroup_cr, &NdbApiWrapper::NdbDictionary__LogfileGroup__getUndoBufferSize >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_LogfileGroup
 * Method:    getAutoGrowSpecification
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/AutoGrowSpecificationConst;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024LogfileGroup_getAutoGrowSpecification(JNIEnv * env, jobject obj)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024LogfileGroup_getAutoGrowSpecification(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_LogfileGroup_ct, ttrait_c_m_n_n_NdbDictionary_AutoGrowSpecification_cr, &NdbDictionary::LogfileGroup::getAutoGrowSpecification >(env, obj);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbDictionary_AutoGrowSpecification_cr, ttrait_c_m_n_n_NdbDictionary_LogfileGroup_cr, &NdbApiWrapper::NdbDictionary__LogfileGroup__getAutoGrowSpecification >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_LogfileGroup
 * Method:    getUndoFreeWords
 * Signature: ()J
 */
JNIEXPORT jlong JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024LogfileGroup_getUndoFreeWords(JNIEnv * env, jobject obj)
{
    TRACE("jlong Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024LogfileGroup_getUndoFreeWords(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_LogfileGroup_ct, ttrait_Uint64, &NdbDictionary::LogfileGroup::getUndoFreeWords >(env, obj);
#else
    return gcall_fr< ttrait_Uint64, ttrait_c_m_n_n_NdbDictionary_LogfileGroup_cr, &NdbApiWrapper::NdbDictionary__LogfileGroup__getUndoFreeWords >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_LogfileGroup
 * Method:    getObjectStatus
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024LogfileGroup_getObjectStatus(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024LogfileGroup_getObjectStatus(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_LogfileGroup_ct, ttrait_c_m_n_n_NdbDictionary_Object_Status_iv/*_enum_*/, &NdbDictionary::LogfileGroup::getObjectStatus >(env, obj);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbDictionary_Object_Status_iv/*_enum_*/, ttrait_c_m_n_n_NdbDictionary_LogfileGroup_cr, &NdbApiWrapper::NdbDictionary__LogfileGroup__getObjectStatus >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_LogfileGroup
 * Method:    getObjectVersion
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024LogfileGroup_getObjectVersion(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024LogfileGroup_getObjectVersion(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_LogfileGroup_ct, ttrait_int, &NdbDictionary::LogfileGroup::getObjectVersion >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_LogfileGroup_cr, &NdbApiWrapper::NdbDictionary__LogfileGroup__getObjectVersion >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_LogfileGroup
 * Method:    getObjectId
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024LogfileGroup_getObjectId(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024LogfileGroup_getObjectId(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_LogfileGroup_ct, ttrait_int, &NdbDictionary::LogfileGroup::getObjectId >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_LogfileGroup_cr, &NdbApiWrapper::NdbDictionary__LogfileGroup__getObjectId >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_LogfileGroup
 * Method:    create
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/LogfileGroup;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024LogfileGroup_create__(JNIEnv * env, jclass cls)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024LogfileGroup_create__(JNIEnv *, jclass)");
    return gcreate< ttrait_c_m_n_n_NdbDictionary_LogfileGroup_r >(env, cls);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_LogfileGroup
 * Method:    create
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/LogfileGroupConst;)Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/LogfileGroup;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024LogfileGroup_create__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024LogfileGroupConst_2(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024LogfileGroup_create__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024LogfileGroupConst_2(JNIEnv *, jclass, jobject)");
    return gcreate< ttrait_c_m_n_n_NdbDictionary_LogfileGroup_r, ttrait_c_m_n_n_NdbDictionary_LogfileGroup_cr >(env, cls, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_LogfileGroup
 * Method:    delete
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/LogfileGroup;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024LogfileGroup_delete(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024LogfileGroup_delete(JNIEnv *, jclass, jobject)");
    gdelete< ttrait_c_m_n_n_NdbDictionary_LogfileGroup_r >(env, cls, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_LogfileGroup
 * Method:    setName
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024LogfileGroup_setName(JNIEnv * env, jobject obj, jstring p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024LogfileGroup_setName(JNIEnv *, jobject, jstring)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_LogfileGroup_t, ttrait_char_cp_jutf8null, &NdbDictionary::LogfileGroup::setName >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_LogfileGroup_r, ttrait_char_cp_jutf8null, &NdbApiWrapper::NdbDictionary__LogfileGroup__setName >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_LogfileGroup
 * Method:    setUndoBufferSize
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024LogfileGroup_setUndoBufferSize(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024LogfileGroup_setUndoBufferSize(JNIEnv *, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_LogfileGroup_t, ttrait_Uint32, &NdbDictionary::LogfileGroup::setUndoBufferSize >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_LogfileGroup_r, ttrait_Uint32, &NdbApiWrapper::NdbDictionary__LogfileGroup__setUndoBufferSize >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_LogfileGroup
 * Method:    setAutoGrowSpecification
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/AutoGrowSpecificationConst;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024LogfileGroup_setAutoGrowSpecification(JNIEnv * env, jobject obj, jobject p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024LogfileGroup_setAutoGrowSpecification(JNIEnv *, jobject, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_LogfileGroup_t, ttrait_c_m_n_n_NdbDictionary_AutoGrowSpecification_cr, &NdbDictionary::LogfileGroup::setAutoGrowSpecification >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_LogfileGroup_r, ttrait_c_m_n_n_NdbDictionary_AutoGrowSpecification_cr, &NdbApiWrapper::NdbDictionary__LogfileGroup__setAutoGrowSpecification >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

// ---------------------------------------------------------------------------

//#include "com_mysql_ndbjtie_ndbapi_NdbDictionary_Object.h"

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Object
 * Method:    delete
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/Object;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Object_delete(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Object_delete(JNIEnv *, jclass, jobject)");
    gdelete< ttrait_c_m_n_n_NdbDictionary_Object_r >(env, cls, p0);
}

// ---------------------------------------------------------------------------

//#include "com_mysql_ndbjtie_ndbapi_NdbDictionary_ObjectId.h"

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_ObjectId
 * Method:    getObjectStatus
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024ObjectId_getObjectStatus(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024ObjectId_getObjectStatus(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_ObjectId_ct, ttrait_c_m_n_n_NdbDictionary_Object_Status_iv/*_enum_*/, &NdbDictionary::ObjectId::getObjectStatus >(env, obj);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbDictionary_Object_Status_iv/*_enum_*/, ttrait_c_m_n_n_NdbDictionary_ObjectId_cr, &NdbApiWrapper::NdbDictionary__ObjectId__getObjectStatus >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_ObjectId
 * Method:    getObjectVersion
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024ObjectId_getObjectVersion(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024ObjectId_getObjectVersion(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_ObjectId_ct, ttrait_int, &NdbDictionary::ObjectId::getObjectVersion >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_ObjectId_cr, &NdbApiWrapper::NdbDictionary__ObjectId__getObjectVersion >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_ObjectId
 * Method:    getObjectId
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024ObjectId_getObjectId(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024ObjectId_getObjectId(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_ObjectId_ct, ttrait_int, &NdbDictionary::ObjectId::getObjectId >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_ObjectId_cr, &NdbApiWrapper::NdbDictionary__ObjectId__getObjectId >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_ObjectId
 * Method:    create
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/ObjectId;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024ObjectId_create(JNIEnv * env, jclass cls)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024ObjectId_create(JNIEnv *, jclass)");
    return gcreate< ttrait_c_m_n_n_NdbDictionary_ObjectId_r >(env, cls);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_ObjectId
 * Method:    delete
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/ObjectId;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024ObjectId_delete(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024ObjectId_delete(JNIEnv *, jclass, jobject)");
    gdelete< ttrait_c_m_n_n_NdbDictionary_ObjectId_r >(env, cls, p0);
}

// ---------------------------------------------------------------------------

//#include "com_mysql_ndbjtie_ndbapi_NdbDictionary_OptimizeIndexHandle.h"

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_OptimizeIndexHandle
 * Method:    create
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/OptimizeIndexHandle;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024OptimizeIndexHandle_create(JNIEnv * env, jclass cls)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024OptimizeIndexHandle_create(JNIEnv *, jclass)");
    return gcreate< ttrait_c_m_n_n_NdbDictionary_OptimizeIndexHandle_r >(env, cls);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_OptimizeIndexHandle
 * Method:    delete
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/OptimizeIndexHandle;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024OptimizeIndexHandle_delete(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024OptimizeIndexHandle_delete(JNIEnv *, jclass, jobject)");
    gdelete< ttrait_c_m_n_n_NdbDictionary_OptimizeIndexHandle_r >(env, cls, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_OptimizeIndexHandle
 * Method:    next
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024OptimizeIndexHandle_next(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024OptimizeIndexHandle_next(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_OptimizeIndexHandle_t, ttrait_int, &NdbDictionary::OptimizeIndexHandle::next >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_OptimizeIndexHandle_r, &NdbApiWrapper::NdbDictionary__OptimizeIndexHandle__next >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_OptimizeIndexHandle
 * Method:    close
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024OptimizeIndexHandle_close(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024OptimizeIndexHandle_close(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_OptimizeIndexHandle_t, ttrait_int, &NdbDictionary::OptimizeIndexHandle::close >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_OptimizeIndexHandle_r, &NdbApiWrapper::NdbDictionary__OptimizeIndexHandle__close >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

// ---------------------------------------------------------------------------

//#include "com_mysql_ndbjtie_ndbapi_NdbDictionary_OptimizeTableHandle.h"

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_OptimizeTableHandle
 * Method:    create
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/OptimizeTableHandle;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024OptimizeTableHandle_create(JNIEnv * env, jclass cls)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024OptimizeTableHandle_create(JNIEnv *, jclass)");
    return gcreate< ttrait_c_m_n_n_NdbDictionary_OptimizeTableHandle_r >(env, cls);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_OptimizeTableHandle
 * Method:    delete
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/OptimizeTableHandle;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024OptimizeTableHandle_delete(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024OptimizeTableHandle_delete(JNIEnv *, jclass, jobject)");
    gdelete< ttrait_c_m_n_n_NdbDictionary_OptimizeTableHandle_r >(env, cls, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_OptimizeTableHandle
 * Method:    next
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024OptimizeTableHandle_next(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024OptimizeTableHandle_next(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_OptimizeTableHandle_t, ttrait_int, &NdbDictionary::OptimizeTableHandle::next >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_OptimizeTableHandle_r, &NdbApiWrapper::NdbDictionary__OptimizeTableHandle__next >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_OptimizeTableHandle
 * Method:    close
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024OptimizeTableHandle_close(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024OptimizeTableHandle_close(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_OptimizeTableHandle_t, ttrait_int, &NdbDictionary::OptimizeTableHandle::close >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_OptimizeTableHandle_r, &NdbApiWrapper::NdbDictionary__OptimizeTableHandle__close >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

// ---------------------------------------------------------------------------

//#include "com_mysql_ndbjtie_ndbapi_NdbDictionary_RecordSpecification.h"

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_RecordSpecification
 * Method:    size
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024RecordSpecification_size(JNIEnv * env, jclass cls)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024RecordSpecification_size(JNIEnv *, jclass)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_fr< ttrait_Uint32, &NdbDictionary::RecordSpecification::size >(env, cls);
#else
    return gcall_fr< ttrait_Uint32, &NdbApiWrapper::NdbDictionary__RecordSpecification__size >(env, cls);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_RecordSpecification
 * Method:    column
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/ColumnConst;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024RecordSpecification_column__(JNIEnv * env, jobject obj)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024RecordSpecification_column__(JNIEnv *, jobject)");
    return gget< ttrait_c_m_n_n_NdbDictionary_RecordSpecification_t, ttrait_c_m_n_n_NdbDictionary_Column_cp, &NdbDictionary::RecordSpecification::column >(env, obj);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_RecordSpecification
 * Method:    offset
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024RecordSpecification_offset__(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024RecordSpecification_offset__(JNIEnv *, jobject)");
    return gget< ttrait_c_m_n_n_NdbDictionary_RecordSpecification_t, ttrait_Uint32, &NdbDictionary::RecordSpecification::offset >(env, obj);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_RecordSpecification
 * Method:    nullbit_byte_offset
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024RecordSpecification_nullbit_1byte_1offset__(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024RecordSpecification_nullbit_1byte_1offset__(JNIEnv *, jobject)");
    return gget< ttrait_c_m_n_n_NdbDictionary_RecordSpecification_t, ttrait_Uint32, &NdbDictionary::RecordSpecification::nullbit_byte_offset >(env, obj);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_RecordSpecification
 * Method:    nullbit_bit_in_byte
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024RecordSpecification_nullbit_1bit_1in_1byte__(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024RecordSpecification_nullbit_1bit_1in_1byte__(JNIEnv *, jobject)");
    return gget< ttrait_c_m_n_n_NdbDictionary_RecordSpecification_t, ttrait_Uint32, &NdbDictionary::RecordSpecification::nullbit_bit_in_byte >(env, obj);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_RecordSpecification
 * Method:    column
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/ColumnConst;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024RecordSpecification_column__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024ColumnConst_2(JNIEnv * env, jobject obj, jobject p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024RecordSpecification_column__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024ColumnConst_2(JNIEnv *, jobject, jobject)");
    gset< ttrait_c_m_n_n_NdbDictionary_RecordSpecification_t, ttrait_c_m_n_n_NdbDictionary_Column_cp, &NdbDictionary::RecordSpecification::column >(env, obj, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_RecordSpecification
 * Method:    offset
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024RecordSpecification_offset__I(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024RecordSpecification_offset__I(JNIEnv *, jobject, jint)");
    gset< ttrait_c_m_n_n_NdbDictionary_RecordSpecification_t, ttrait_Uint32, &NdbDictionary::RecordSpecification::offset >(env, obj, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_RecordSpecification
 * Method:    nullbit_byte_offset
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024RecordSpecification_nullbit_1byte_1offset__I(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024RecordSpecification_nullbit_1byte_1offset__I(JNIEnv *, jobject, jint)");
    gset< ttrait_c_m_n_n_NdbDictionary_RecordSpecification_t, ttrait_Uint32, &NdbDictionary::RecordSpecification::nullbit_byte_offset >(env, obj, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_RecordSpecification
 * Method:    nullbit_bit_in_byte
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024RecordSpecification_nullbit_1bit_1in_1byte__I(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024RecordSpecification_nullbit_1bit_1in_1byte__I(JNIEnv *, jobject, jint)");
    gset< ttrait_c_m_n_n_NdbDictionary_RecordSpecification_t, ttrait_Uint32, &NdbDictionary::RecordSpecification::nullbit_bit_in_byte >(env, obj, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_RecordSpecification
 * Method:    create
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/RecordSpecification;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024RecordSpecification_create(JNIEnv * env, jclass cls)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024RecordSpecification_create(JNIEnv *, jclass)");
    return gcreate< ttrait_c_m_n_n_NdbDictionary_RecordSpecification_r >(env, cls);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_RecordSpecification
 * Method:    delete
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/RecordSpecification;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024RecordSpecification_delete(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024RecordSpecification_delete(JNIEnv *, jclass, jobject)");
    gdelete< ttrait_c_m_n_n_NdbDictionary_RecordSpecification_r >(env, cls, p0);
}

// ---------------------------------------------------------------------------

//#include "com_mysql_ndbjtie_ndbapi_NdbDictionary_RecordSpecificationArray.h"

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_RecordSpecificationArray
 * Method:    create
 * Signature: (I)Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/RecordSpecificationArray;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024RecordSpecificationArray_create(JNIEnv * env, jclass cls, jint p0)
{
    TRACE("Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024RecordSpecificationArray_create(JNIEnv *, jclass, jint)");
    return gcreateArray< ttrait_c_m_n_n_NdbDictionary_RecordSpecificationArray_r, ttrait_int32 >(env, cls, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_RecordSpecificationArray
 * Method:    delete
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/RecordSpecificationArray;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024RecordSpecificationArray_delete(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024RecordSpecificationArray_delete(JNIEnv *, jclass, jobject)");
    gdeleteArray< ttrait_c_m_n_n_NdbDictionary_RecordSpecificationArray_r >(env, cls, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_RecordSpecificationArray
 * Method:    at
 * Signature: (I)Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/RecordSpecification;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024RecordSpecificationArray_at(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024RecordSpecificationArray_at(JNIEnv *, jobject, jint)");
    return gat< ttrait_c_m_n_n_NdbDictionary_RecordSpecification_r, ttrait_c_m_n_n_NdbDictionary_RecordSpecificationArray_r, ttrait_int32 >(env, NULL, obj, p0);
}

// ---------------------------------------------------------------------------

//#include "com_mysql_ndbjtie_ndbapi_NdbDictionary_Table.h"

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    getName
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getName(JNIEnv * env, jobject obj)
{
    TRACE("jstring Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getName(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Table_ct, ttrait_char_cp_jutf8null, &NdbDictionary::Table::getName >(env, obj);
#else
    return gcall_fr< ttrait_char_cp_jutf8null, ttrait_c_m_n_n_NdbDictionary_Table_cr, &NdbApiWrapper::NdbDictionary__Table__getName >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    getTableId
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getTableId(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getTableId(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Table_ct, ttrait_int, &NdbDictionary::Table::getTableId >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Table_cr, &NdbApiWrapper::NdbDictionary__Table__getTableId >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    getColumn
 * Signature: (Ljava/lang/String;)Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/ColumnConst;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getColumn__Ljava_lang_String_2(JNIEnv * env, jobject obj, jstring p0)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getColumn__Ljava_lang_String_2(JNIEnv *, jobject, jstring)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_CONST_OVERLOADED_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Table_ct, ttrait_c_m_n_n_NdbDictionary_Column_cp, ttrait_char_cp_jutf8null, &NdbDictionary::Table::getColumn >(env, obj, p0); // call of overloaded const/non-const method
#else
    return gcall_fr< ttrait_c_m_n_n_NdbDictionary_Column_cp, ttrait_c_m_n_n_NdbDictionary_Table_cr, ttrait_char_cp_jutf8null, &NdbApiWrapper::NdbDictionary__Table__getColumn__0 >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_CONST_OVERLOADED_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    getColumn
 * Signature: (I)Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/ColumnConst;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getColumn__I(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getColumn__I(JNIEnv *, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_CONST_OVERLOADED_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Table_ct, ttrait_c_m_n_n_NdbDictionary_Column_cp, ttrait_int, &NdbDictionary::Table::getColumn >(env, obj, p0); // call of overloaded const/non-const method
#else
    return gcall_fr< ttrait_c_m_n_n_NdbDictionary_Column_cp, ttrait_c_m_n_n_NdbDictionary_Table_cr, ttrait_int, &NdbApiWrapper::NdbDictionary__Table__getColumn__1 >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_CONST_OVERLOADED_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    getLogging
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getLogging(JNIEnv * env, jobject obj)
{
    TRACE("jboolean Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getLogging(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Table_ct, ttrait_bool, &NdbDictionary::Table::getLogging >(env, obj);
#else
    return gcall_fr< ttrait_bool, ttrait_c_m_n_n_NdbDictionary_Table_cr, &NdbApiWrapper::NdbDictionary__Table__getLogging >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    getFragmentType
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getFragmentType(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getFragmentType(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Table_ct, ttrait_c_m_n_n_NdbDictionary_Object_FragmentType_iv/*_enum_*/, &NdbDictionary::Table::getFragmentType >(env, obj);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbDictionary_Object_FragmentType_iv/*_enum_*/, ttrait_c_m_n_n_NdbDictionary_Table_cr, &NdbApiWrapper::NdbDictionary__Table__getFragmentType >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    getKValue
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getKValue(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getKValue(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Table_ct, ttrait_int, &NdbDictionary::Table::getKValue >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Table_cr, &NdbApiWrapper::NdbDictionary__Table__getKValue >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    getMinLoadFactor
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getMinLoadFactor(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getMinLoadFactor(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Table_ct, ttrait_int, &NdbDictionary::Table::getMinLoadFactor >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Table_cr, &NdbApiWrapper::NdbDictionary__Table__getMinLoadFactor >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    getMaxLoadFactor
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getMaxLoadFactor(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getMaxLoadFactor(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Table_ct, ttrait_int, &NdbDictionary::Table::getMaxLoadFactor >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Table_cr, &NdbApiWrapper::NdbDictionary__Table__getMaxLoadFactor >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    getNoOfColumns
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getNoOfColumns(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getNoOfColumns(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Table_ct, ttrait_int, &NdbDictionary::Table::getNoOfColumns >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Table_cr, &NdbApiWrapper::NdbDictionary__Table__getNoOfColumns >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    getNoOfPrimaryKeys
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getNoOfPrimaryKeys(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getNoOfPrimaryKeys(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Table_ct, ttrait_int, &NdbDictionary::Table::getNoOfPrimaryKeys >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Table_cr, &NdbApiWrapper::NdbDictionary__Table__getNoOfPrimaryKeys >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    getPrimaryKey
 * Signature: (I)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getPrimaryKey(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("jstring Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getPrimaryKey(JNIEnv *, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Table_ct, ttrait_char_cp_jutf8null, ttrait_int, &NdbDictionary::Table::getPrimaryKey >(env, obj, p0);
#else
    return gcall_fr< ttrait_char_cp_jutf8null, ttrait_c_m_n_n_NdbDictionary_Table_cr, ttrait_int, &NdbApiWrapper::NdbDictionary__Table__getPrimaryKey >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    equal
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/TableConst;)Z
 */
JNIEXPORT jboolean JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_equal(JNIEnv * env, jobject obj, jobject p0)
{
    TRACE("jboolean Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_equal(JNIEnv *, jobject, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Table_ct, ttrait_bool, ttrait_c_m_n_n_NdbDictionary_Table_cr, &NdbDictionary::Table::equal >(env, obj, p0);
#else
    return gcall_fr< ttrait_bool, ttrait_c_m_n_n_NdbDictionary_Table_cr, ttrait_c_m_n_n_NdbDictionary_Table_cr, &NdbApiWrapper::NdbDictionary__Table__equal >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    getFrmData
 * Signature: ()Ljava/nio/ByteBuffer;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getFrmData(JNIEnv * env, jobject obj)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getFrmData(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Table_ct, ttrait_void_1cp_bb, &NdbDictionary::Table::getFrmData >(env, obj);
#else
    return gcall_fr< ttrait_void_1cp_bb, ttrait_c_m_n_n_NdbDictionary_Table_cr, &NdbApiWrapper::NdbDictionary__Table__getFrmData >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    getFrmLength
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getFrmLength(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getFrmLength(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Table_ct, ttrait_Uint32, &NdbDictionary::Table::getFrmLength >(env, obj);
#else
    return gcall_fr< ttrait_Uint32, ttrait_c_m_n_n_NdbDictionary_Table_cr, &NdbApiWrapper::NdbDictionary__Table__getFrmLength >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    getFragmentData
 * Signature: ()Ljava/nio/ByteBuffer;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getFragmentData(JNIEnv * env, jobject obj)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getFragmentData(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Table_ct, ttrait_Uint32_0cp_bb, &NdbDictionary::Table::getFragmentData >(env, obj);
#else
    return gcall_fr< ttrait_Uint32_0cp_bb, ttrait_c_m_n_n_NdbDictionary_Table_cr, &NdbApiWrapper::NdbDictionary__Table__getFragmentData >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    getFragmentDataLen
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getFragmentDataLen(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getFragmentDataLen(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Table_ct, ttrait_Uint32, &NdbDictionary::Table::getFragmentDataLen >(env, obj);
#else
    return gcall_fr< ttrait_Uint32, ttrait_c_m_n_n_NdbDictionary_Table_cr, &NdbApiWrapper::NdbDictionary__Table__getFragmentDataLen >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    getRangeListData
 * Signature: ()Ljava/nio/ByteBuffer;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getRangeListData(JNIEnv * env, jobject obj)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getRangeListData(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Table_ct, ttrait_Int32_0cp_bb, &NdbDictionary::Table::getRangeListData >(env, obj);
#else
    return gcall_fr< ttrait_Int32_0cp_bb, ttrait_c_m_n_n_NdbDictionary_Table_cr, &NdbApiWrapper::NdbDictionary__Table__getRangeListData >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    getRangeListDataLen
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getRangeListDataLen(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getRangeListDataLen(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Table_ct, ttrait_Uint32, &NdbDictionary::Table::getRangeListDataLen >(env, obj);
#else
    return gcall_fr< ttrait_Uint32, ttrait_c_m_n_n_NdbDictionary_Table_cr, &NdbApiWrapper::NdbDictionary__Table__getRangeListDataLen >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    getDefaultRecord
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbRecordConst;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getDefaultRecord(JNIEnv * env, jobject obj)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getDefaultRecord(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Table_ct, ttrait_c_m_n_n_NdbRecord_cp, &NdbDictionary::Table::getDefaultRecord >(env, obj);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbRecord_cp, ttrait_c_m_n_n_NdbDictionary_Table_cr, &NdbApiWrapper::NdbDictionary__Table__getDefaultRecord >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    getLinearFlag
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getLinearFlag(JNIEnv * env, jobject obj)
{
    TRACE("jboolean Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getLinearFlag(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Table_ct, ttrait_bool, &NdbDictionary::Table::getLinearFlag >(env, obj);
#else
    return gcall_fr< ttrait_bool, ttrait_c_m_n_n_NdbDictionary_Table_cr, &NdbApiWrapper::NdbDictionary__Table__getLinearFlag >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    getFragmentCount
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getFragmentCount(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getFragmentCount(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Table_ct, ttrait_Uint32, &NdbDictionary::Table::getFragmentCount >(env, obj);
#else
    return gcall_fr< ttrait_Uint32, ttrait_c_m_n_n_NdbDictionary_Table_cr, &NdbApiWrapper::NdbDictionary__Table__getFragmentCount >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    getTablespaceName
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getTablespaceName(JNIEnv * env, jobject obj)
{
    TRACE("jstring Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getTablespaceName(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Table_ct, ttrait_char_cp_jutf8null, &NdbDictionary::Table::getTablespaceName >(env, obj);
#else
    return gcall_fr< ttrait_char_cp_jutf8null, ttrait_c_m_n_n_NdbDictionary_Table_cr, &NdbApiWrapper::NdbDictionary__Table__getTablespaceName >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    getTablespace
 * Signature: ([I[I)Z
 */
JNIEXPORT jboolean JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getTablespace(JNIEnv * env, jobject obj, jintArray p0, jintArray p1)
{
    TRACE("jboolean Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getTablespace(JNIEnv *, jobject, jintArray, jintArray)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Table_ct, ttrait_bool, ttrait_Uint32_0p_a, ttrait_Uint32_0p_a, &NdbDictionary::Table::getTablespace >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_bool, ttrait_c_m_n_n_NdbDictionary_Table_cr, ttrait_Uint32_0p_a, ttrait_Uint32_0p_a, &NdbApiWrapper::NdbDictionary__Table__getTablespace >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    getObjectStatus
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getObjectStatus(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getObjectStatus(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Table_ct, ttrait_c_m_n_n_NdbDictionary_Object_Status_iv/*_enum_*/, &NdbDictionary::Table::getObjectStatus >(env, obj);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbDictionary_Object_Status_iv/*_enum_*/, ttrait_c_m_n_n_NdbDictionary_Table_cr, &NdbApiWrapper::NdbDictionary__Table__getObjectStatus >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    setStatusInvalid
 * Signature: ()V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_setStatusInvalid(JNIEnv * env, jobject obj)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_setStatusInvalid(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Table_ct, &NdbDictionary::Table::setStatusInvalid >(env, obj);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_Table_cr, &NdbApiWrapper::NdbDictionary__Table__setStatusInvalid >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    getObjectVersion
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getObjectVersion(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getObjectVersion(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Table_ct, ttrait_int, &NdbDictionary::Table::getObjectVersion >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Table_cr, &NdbApiWrapper::NdbDictionary__Table__getObjectVersion >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    getDefaultNoPartitionsFlag
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getDefaultNoPartitionsFlag(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getDefaultNoPartitionsFlag(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Table_ct, ttrait_Uint32, &NdbDictionary::Table::getDefaultNoPartitionsFlag >(env, obj);
#else
    return gcall_fr< ttrait_Uint32, ttrait_c_m_n_n_NdbDictionary_Table_cr, &NdbApiWrapper::NdbDictionary__Table__getDefaultNoPartitionsFlag >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    getObjectId
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getObjectId(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getObjectId(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Table_ct, ttrait_int, &NdbDictionary::Table::getObjectId >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Table_cr, &NdbApiWrapper::NdbDictionary__Table__getObjectId >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    getMaxRows
 * Signature: ()J
 */
JNIEXPORT jlong JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getMaxRows(JNIEnv * env, jobject obj)
{
    TRACE("jlong Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getMaxRows(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Table_ct, ttrait_Uint64, &NdbDictionary::Table::getMaxRows >(env, obj);
#else
    return gcall_fr< ttrait_Uint64, ttrait_c_m_n_n_NdbDictionary_Table_cr, &NdbApiWrapper::NdbDictionary__Table__getMaxRows >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    getMinRows
 * Signature: ()J
 */
JNIEXPORT jlong JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getMinRows(JNIEnv * env, jobject obj)
{
    TRACE("jlong Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getMinRows(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Table_ct, ttrait_Uint64, &NdbDictionary::Table::getMinRows >(env, obj);
#else
    return gcall_fr< ttrait_Uint64, ttrait_c_m_n_n_NdbDictionary_Table_cr, &NdbApiWrapper::NdbDictionary__Table__getMinRows >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    getSingleUserMode
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getSingleUserMode(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getSingleUserMode(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Table_ct, ttrait_c_m_n_n_NdbDictionary_Table_SingleUserMode_iv/*_enum_*/, &NdbDictionary::Table::getSingleUserMode >(env, obj);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbDictionary_Table_SingleUserMode_iv/*_enum_*/, ttrait_c_m_n_n_NdbDictionary_Table_cr, &NdbApiWrapper::NdbDictionary__Table__getSingleUserMode >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    getRowGCIIndicator
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getRowGCIIndicator(JNIEnv * env, jobject obj)
{
    TRACE("jboolean Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getRowGCIIndicator(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Table_ct, ttrait_bool, &NdbDictionary::Table::getRowGCIIndicator >(env, obj);
#else
    return gcall_fr< ttrait_bool, ttrait_c_m_n_n_NdbDictionary_Table_cr, &NdbApiWrapper::NdbDictionary__Table__getRowGCIIndicator >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    getRowChecksumIndicator
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getRowChecksumIndicator(JNIEnv * env, jobject obj)
{
    TRACE("jboolean Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getRowChecksumIndicator(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Table_ct, ttrait_bool, &NdbDictionary::Table::getRowChecksumIndicator >(env, obj);
#else
    return gcall_fr< ttrait_bool, ttrait_c_m_n_n_NdbDictionary_Table_cr, &NdbApiWrapper::NdbDictionary__Table__getRowChecksumIndicator >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    getPartitionId
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getPartitionId(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getPartitionId(JNIEnv *, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Table_ct, ttrait_Uint32, ttrait_Uint32, &NdbDictionary::Table::getPartitionId >(env, obj, p0);
#else
    return gcall_fr< ttrait_Uint32, ttrait_c_m_n_n_NdbDictionary_Table_cr, ttrait_Uint32, &NdbApiWrapper::NdbDictionary__Table__getPartitionId >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    create
 * Signature: (Ljava/lang/String;)Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/Table;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_create__Ljava_lang_String_2(JNIEnv * env, jclass cls, jstring p0)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_create__Ljava_lang_String_2(JNIEnv *, jclass, jstring)");
    return gcreate< ttrait_c_m_n_n_NdbDictionary_Table_r, ttrait_char_cp_jutf8null >(env, cls, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    create
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/TableConst;)Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/Table;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_create__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024TableConst_2(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_create__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024TableConst_2(JNIEnv *, jclass, jobject)");
    return gcreate< ttrait_c_m_n_n_NdbDictionary_Table_r, ttrait_c_m_n_n_NdbDictionary_Table_cr >(env, cls, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    delete
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/Table;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_delete(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_delete(JNIEnv *, jclass, jobject)");
    gdelete< ttrait_c_m_n_n_NdbDictionary_Table_r >(env, cls, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    getColumnM
 * Signature: (I)Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/Column;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getColumnM__I(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getColumnM__I(JNIEnv *, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_CONST_OVERLOADED_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Table_t, ttrait_c_m_n_n_NdbDictionary_Column_p, ttrait_int, &NdbDictionary::Table::getColumn >(env, obj, p0); // call of overloaded const/non-const method
#else
    return gcall_fr< ttrait_c_m_n_n_NdbDictionary_Column_p, ttrait_c_m_n_n_NdbDictionary_Table_r, ttrait_int, &NdbApiWrapper::NdbDictionary__Table__getColumn__2 >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_CONST_OVERLOADED_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    getColumnM
 * Signature: (Ljava/lang/String;)Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/Column;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getColumnM__Ljava_lang_String_2(JNIEnv * env, jobject obj, jstring p0)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_getColumnM__Ljava_lang_String_2(JNIEnv *, jobject, jstring)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_CONST_OVERLOADED_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Table_t, ttrait_c_m_n_n_NdbDictionary_Column_p, ttrait_char_cp_jutf8null, &NdbDictionary::Table::getColumn >(env, obj, p0); // call of overloaded const/non-const method
#else
    return gcall_fr< ttrait_c_m_n_n_NdbDictionary_Column_p, ttrait_c_m_n_n_NdbDictionary_Table_r, ttrait_char_cp_jutf8null, &NdbApiWrapper::NdbDictionary__Table__getColumn__3 >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_CONST_OVERLOADED_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    setName
 * Signature: (Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_setName(JNIEnv * env, jobject obj, jstring p0)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_setName(JNIEnv *, jobject, jstring)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Table_t, ttrait_int, ttrait_char_cp_jutf8null, &NdbDictionary::Table::setName >(env, obj, p0);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Table_r, ttrait_char_cp_jutf8null, &NdbApiWrapper::NdbDictionary__Table__setName >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    addColumn
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/ColumnConst;)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_addColumn(JNIEnv * env, jobject obj, jobject p0)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_addColumn(JNIEnv *, jobject, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Table_t, ttrait_int, ttrait_c_m_n_n_NdbDictionary_Column_cr, &NdbDictionary::Table::addColumn >(env, obj, p0);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Table_r, ttrait_c_m_n_n_NdbDictionary_Column_cr, &NdbApiWrapper::NdbDictionary__Table__addColumn >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    setLogging
 * Signature: (Z)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_setLogging(JNIEnv * env, jobject obj, jboolean p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_setLogging(JNIEnv *, jobject, jboolean)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Table_t, ttrait_bool, &NdbDictionary::Table::setLogging >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_Table_r, ttrait_bool, &NdbApiWrapper::NdbDictionary__Table__setLogging >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    setLinearFlag
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_setLinearFlag(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_setLinearFlag(JNIEnv *, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Table_t, ttrait_Uint32, &NdbDictionary::Table::setLinearFlag >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_Table_r, ttrait_Uint32, &NdbApiWrapper::NdbDictionary__Table__setLinearFlag >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    setFragmentCount
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_setFragmentCount(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_setFragmentCount(JNIEnv *, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Table_t, ttrait_Uint32, &NdbDictionary::Table::setFragmentCount >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_Table_r, ttrait_Uint32, &NdbApiWrapper::NdbDictionary__Table__setFragmentCount >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    setFragmentType
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_setFragmentType(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_setFragmentType(JNIEnv *, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Table_t, ttrait_c_m_n_n_NdbDictionary_Object_FragmentType_iv/*_enum_*/, &NdbDictionary::Table::setFragmentType >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_Table_r, ttrait_c_m_n_n_NdbDictionary_Object_FragmentType_iv/*_enum_*/, &NdbApiWrapper::NdbDictionary__Table__setFragmentType >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    setKValue
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_setKValue(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_setKValue(JNIEnv *, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Table_t, ttrait_int, &NdbDictionary::Table::setKValue >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_Table_r, ttrait_int, &NdbApiWrapper::NdbDictionary__Table__setKValue >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    setMinLoadFactor
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_setMinLoadFactor(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_setMinLoadFactor(JNIEnv *, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Table_t, ttrait_int, &NdbDictionary::Table::setMinLoadFactor >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_Table_r, ttrait_int, &NdbApiWrapper::NdbDictionary__Table__setMinLoadFactor >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    setMaxLoadFactor
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_setMaxLoadFactor(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_setMaxLoadFactor(JNIEnv *, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Table_t, ttrait_int, &NdbDictionary::Table::setMaxLoadFactor >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_Table_r, ttrait_int, &NdbApiWrapper::NdbDictionary__Table__setMaxLoadFactor >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    setTablespaceName
 * Signature: (Ljava/lang/String;)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_setTablespaceName(JNIEnv * env, jobject obj, jstring p0)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_setTablespaceName(JNIEnv *, jobject, jstring)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Table_t, ttrait_int, ttrait_char_cp_jutf8null, &NdbDictionary::Table::setTablespaceName >(env, obj, p0);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Table_r, ttrait_char_cp_jutf8null, &NdbApiWrapper::NdbDictionary__Table__setTablespaceName >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    setTablespace
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/TablespaceConst;)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_setTablespace(JNIEnv * env, jobject obj, jobject p0)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_setTablespace(JNIEnv *, jobject, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Table_t, ttrait_int, ttrait_c_m_n_n_NdbDictionary_Tablespace_cr, &NdbDictionary::Table::setTablespace >(env, obj, p0);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Table_r, ttrait_c_m_n_n_NdbDictionary_Tablespace_cr, &NdbApiWrapper::NdbDictionary__Table__setTablespace >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    setDefaultNoPartitionsFlag
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_setDefaultNoPartitionsFlag(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_setDefaultNoPartitionsFlag(JNIEnv *, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Table_t, ttrait_Uint32, &NdbDictionary::Table::setDefaultNoPartitionsFlag >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_Table_r, ttrait_Uint32, &NdbApiWrapper::NdbDictionary__Table__setDefaultNoPartitionsFlag >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    setFrm
 * Signature: (Ljava/nio/ByteBuffer;I)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_setFrm(JNIEnv * env, jobject obj, jobject p0, jint p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_setFrm(JNIEnv *, jobject, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Table_t, ttrait_int, ttrait_void_1cp_bb, ttrait_Uint32, &NdbDictionary::Table::setFrm >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Table_r, ttrait_void_1cp_bb, ttrait_Uint32, &NdbApiWrapper::NdbDictionary__Table__setFrm >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    setFragmentData
 * Signature: (Ljava/nio/ByteBuffer;I)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_setFragmentData(JNIEnv * env, jobject obj, jobject p0, jint p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_setFragmentData(JNIEnv *, jobject, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Table_t, ttrait_int, ttrait_Uint32_0cp_bb, ttrait_Uint32, &NdbDictionary::Table::setFragmentData >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Table_r, ttrait_Uint32_0cp_bb, ttrait_Uint32, &NdbApiWrapper::NdbDictionary__Table__setFragmentData >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    setRangeListData
 * Signature: (Ljava/nio/ByteBuffer;I)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_setRangeListData(JNIEnv * env, jobject obj, jobject p0, jint p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_setRangeListData(JNIEnv *, jobject, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Table_t, ttrait_int, ttrait_Int32_0cp_bb, ttrait_Uint32, &NdbDictionary::Table::setRangeListData >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Table_r, ttrait_Int32_0cp_bb, ttrait_Uint32, &NdbApiWrapper::NdbDictionary__Table__setRangeListData >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    setMaxRows
 * Signature: (J)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_setMaxRows(JNIEnv * env, jobject obj, jlong p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_setMaxRows(JNIEnv *, jobject, jlong)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Table_t, ttrait_Uint64, &NdbDictionary::Table::setMaxRows >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_Table_r, ttrait_Uint64, &NdbApiWrapper::NdbDictionary__Table__setMaxRows >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    setMinRows
 * Signature: (J)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_setMinRows(JNIEnv * env, jobject obj, jlong p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_setMinRows(JNIEnv *, jobject, jlong)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Table_t, ttrait_Uint64, &NdbDictionary::Table::setMinRows >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_Table_r, ttrait_Uint64, &NdbApiWrapper::NdbDictionary__Table__setMinRows >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    setSingleUserMode
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_setSingleUserMode(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_setSingleUserMode(JNIEnv *, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Table_t, ttrait_c_m_n_n_NdbDictionary_Table_SingleUserMode_iv/*_enum_*/, &NdbDictionary::Table::setSingleUserMode >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_Table_r, ttrait_c_m_n_n_NdbDictionary_Table_SingleUserMode_iv/*_enum_*/, &NdbApiWrapper::NdbDictionary__Table__setSingleUserMode >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    setRowGCIIndicator
 * Signature: (Z)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_setRowGCIIndicator(JNIEnv * env, jobject obj, jboolean p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_setRowGCIIndicator(JNIEnv *, jobject, jboolean)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Table_t, ttrait_bool, &NdbDictionary::Table::setRowGCIIndicator >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_Table_r, ttrait_bool, &NdbApiWrapper::NdbDictionary__Table__setRowGCIIndicator >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    setRowChecksumIndicator
 * Signature: (Z)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_setRowChecksumIndicator(JNIEnv * env, jobject obj, jboolean p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_setRowChecksumIndicator(JNIEnv *, jobject, jboolean)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Table_t, ttrait_bool, &NdbDictionary::Table::setRowChecksumIndicator >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_Table_r, ttrait_bool, &NdbApiWrapper::NdbDictionary__Table__setRowChecksumIndicator >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    aggregate
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbError;)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_aggregate(JNIEnv * env, jobject obj, jobject p0)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_aggregate(JNIEnv *, jobject, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Table_t, ttrait_int, ttrait_c_m_n_n_NdbError_r, &NdbDictionary::Table::aggregate >(env, obj, p0);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Table_r, ttrait_c_m_n_n_NdbError_r, &NdbApiWrapper::NdbDictionary__Table__aggregate >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Table
 * Method:    validate
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbError;)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_validate(JNIEnv * env, jobject obj, jobject p0)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Table_validate(JNIEnv *, jobject, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Table_t, ttrait_int, ttrait_c_m_n_n_NdbError_r, &NdbDictionary::Table::validate >(env, obj, p0);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Table_r, ttrait_c_m_n_n_NdbError_r, &NdbApiWrapper::NdbDictionary__Table__validate >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

// ---------------------------------------------------------------------------

//#include "com_mysql_ndbjtie_ndbapi_NdbDictionary_Tablespace.h"

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Tablespace
 * Method:    getName
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Tablespace_getName(JNIEnv * env, jobject obj)
{
    TRACE("jstring Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Tablespace_getName(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Tablespace_ct, ttrait_char_cp_jutf8null, &NdbDictionary::Tablespace::getName >(env, obj);
#else
    return gcall_fr< ttrait_char_cp_jutf8null, ttrait_c_m_n_n_NdbDictionary_Tablespace_cr, &NdbApiWrapper::NdbDictionary__Tablespace__getName >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Tablespace
 * Method:    getExtentSize
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Tablespace_getExtentSize(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Tablespace_getExtentSize(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Tablespace_ct, ttrait_Uint32, &NdbDictionary::Tablespace::getExtentSize >(env, obj);
#else
    return gcall_fr< ttrait_Uint32, ttrait_c_m_n_n_NdbDictionary_Tablespace_cr, &NdbApiWrapper::NdbDictionary__Tablespace__getExtentSize >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Tablespace
 * Method:    getAutoGrowSpecification
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/AutoGrowSpecificationConst;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Tablespace_getAutoGrowSpecification(JNIEnv * env, jobject obj)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Tablespace_getAutoGrowSpecification(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Tablespace_ct, ttrait_c_m_n_n_NdbDictionary_AutoGrowSpecification_cr, &NdbDictionary::Tablespace::getAutoGrowSpecification >(env, obj);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbDictionary_AutoGrowSpecification_cr, ttrait_c_m_n_n_NdbDictionary_Tablespace_cr, &NdbApiWrapper::NdbDictionary__Tablespace__getAutoGrowSpecification >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Tablespace
 * Method:    getDefaultLogfileGroup
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Tablespace_getDefaultLogfileGroup(JNIEnv * env, jobject obj)
{
    TRACE("jstring Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Tablespace_getDefaultLogfileGroup(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Tablespace_ct, ttrait_char_cp_jutf8null, &NdbDictionary::Tablespace::getDefaultLogfileGroup >(env, obj);
#else
    return gcall_fr< ttrait_char_cp_jutf8null, ttrait_c_m_n_n_NdbDictionary_Tablespace_cr, &NdbApiWrapper::NdbDictionary__Tablespace__getDefaultLogfileGroup >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Tablespace
 * Method:    getDefaultLogfileGroupId
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Tablespace_getDefaultLogfileGroupId(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Tablespace_getDefaultLogfileGroupId(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Tablespace_ct, ttrait_Uint32, &NdbDictionary::Tablespace::getDefaultLogfileGroupId >(env, obj);
#else
    return gcall_fr< ttrait_Uint32, ttrait_c_m_n_n_NdbDictionary_Tablespace_cr, &NdbApiWrapper::NdbDictionary__Tablespace__getDefaultLogfileGroupId >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Tablespace
 * Method:    getObjectStatus
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Tablespace_getObjectStatus(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Tablespace_getObjectStatus(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Tablespace_ct, ttrait_c_m_n_n_NdbDictionary_Object_Status_iv/*_enum_*/, &NdbDictionary::Tablespace::getObjectStatus >(env, obj);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbDictionary_Object_Status_iv/*_enum_*/, ttrait_c_m_n_n_NdbDictionary_Tablespace_cr, &NdbApiWrapper::NdbDictionary__Tablespace__getObjectStatus >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Tablespace
 * Method:    getObjectVersion
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Tablespace_getObjectVersion(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Tablespace_getObjectVersion(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Tablespace_ct, ttrait_int, &NdbDictionary::Tablespace::getObjectVersion >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Tablespace_cr, &NdbApiWrapper::NdbDictionary__Tablespace__getObjectVersion >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Tablespace
 * Method:    getObjectId
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Tablespace_getObjectId(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Tablespace_getObjectId(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Tablespace_ct, ttrait_int, &NdbDictionary::Tablespace::getObjectId >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Tablespace_cr, &NdbApiWrapper::NdbDictionary__Tablespace__getObjectId >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Tablespace
 * Method:    create
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/Tablespace;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Tablespace_create__(JNIEnv * env, jclass cls)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Tablespace_create__(JNIEnv *, jclass)");
    return gcreate< ttrait_c_m_n_n_NdbDictionary_Tablespace_r >(env, cls);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Tablespace
 * Method:    create
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/TablespaceConst;)Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/Tablespace;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Tablespace_create__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024TablespaceConst_2(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Tablespace_create__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024TablespaceConst_2(JNIEnv *, jclass, jobject)");
    return gcreate< ttrait_c_m_n_n_NdbDictionary_Tablespace_r, ttrait_c_m_n_n_NdbDictionary_Tablespace_cr >(env, cls, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Tablespace
 * Method:    delete
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/Tablespace;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Tablespace_delete(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Tablespace_delete(JNIEnv *, jclass, jobject)");
    gdelete< ttrait_c_m_n_n_NdbDictionary_Tablespace_r >(env, cls, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Tablespace
 * Method:    setName
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Tablespace_setName(JNIEnv * env, jobject obj, jstring p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Tablespace_setName(JNIEnv *, jobject, jstring)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Tablespace_t, ttrait_char_cp_jutf8null, &NdbDictionary::Tablespace::setName >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_Tablespace_r, ttrait_char_cp_jutf8null, &NdbApiWrapper::NdbDictionary__Tablespace__setName >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Tablespace
 * Method:    setExtentSize
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Tablespace_setExtentSize(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Tablespace_setExtentSize(JNIEnv *, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Tablespace_t, ttrait_Uint32, &NdbDictionary::Tablespace::setExtentSize >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_Tablespace_r, ttrait_Uint32, &NdbApiWrapper::NdbDictionary__Tablespace__setExtentSize >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Tablespace
 * Method:    setAutoGrowSpecification
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/AutoGrowSpecificationConst;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Tablespace_setAutoGrowSpecification(JNIEnv * env, jobject obj, jobject p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Tablespace_setAutoGrowSpecification(JNIEnv *, jobject, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Tablespace_t, ttrait_c_m_n_n_NdbDictionary_AutoGrowSpecification_cr, &NdbDictionary::Tablespace::setAutoGrowSpecification >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_Tablespace_r, ttrait_c_m_n_n_NdbDictionary_AutoGrowSpecification_cr, &NdbApiWrapper::NdbDictionary__Tablespace__setAutoGrowSpecification >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Tablespace
 * Method:    setDefaultLogfileGroup
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Tablespace_setDefaultLogfileGroup__Ljava_lang_String_2(JNIEnv * env, jobject obj, jstring p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Tablespace_setDefaultLogfileGroup__Ljava_lang_String_2(JNIEnv *, jobject, jstring)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Tablespace_t, ttrait_char_cp_jutf8null, &NdbDictionary::Tablespace::setDefaultLogfileGroup >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_Tablespace_r, ttrait_char_cp_jutf8null, &NdbApiWrapper::NdbDictionary__Tablespace__setDefaultLogfileGroup__0 >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Tablespace
 * Method:    setDefaultLogfileGroup
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/LogfileGroupConst;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Tablespace_setDefaultLogfileGroup__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024LogfileGroupConst_2(JNIEnv * env, jobject obj, jobject p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Tablespace_setDefaultLogfileGroup__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024LogfileGroupConst_2(JNIEnv *, jobject, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Tablespace_t, ttrait_c_m_n_n_NdbDictionary_LogfileGroup_cr, &NdbDictionary::Tablespace::setDefaultLogfileGroup >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_Tablespace_r, ttrait_c_m_n_n_NdbDictionary_LogfileGroup_cr, &NdbApiWrapper::NdbDictionary__Tablespace__setDefaultLogfileGroup__1 >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

// ---------------------------------------------------------------------------

//#include "com_mysql_ndbjtie_ndbapi_NdbDictionary_Undofile.h"

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Undofile
 * Method:    getPath
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Undofile_getPath(JNIEnv * env, jobject obj)
{
    TRACE("jstring Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Undofile_getPath(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Undofile_ct, ttrait_char_cp_jutf8null, &NdbDictionary::Undofile::getPath >(env, obj);
#else
    return gcall_fr< ttrait_char_cp_jutf8null, ttrait_c_m_n_n_NdbDictionary_Undofile_cr, &NdbApiWrapper::NdbDictionary__Undofile__getPath >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Undofile
 * Method:    getSize
 * Signature: ()J
 */
JNIEXPORT jlong JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Undofile_getSize(JNIEnv * env, jobject obj)
{
    TRACE("jlong Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Undofile_getSize(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Undofile_ct, ttrait_Uint64, &NdbDictionary::Undofile::getSize >(env, obj);
#else
    return gcall_fr< ttrait_Uint64, ttrait_c_m_n_n_NdbDictionary_Undofile_cr, &NdbApiWrapper::NdbDictionary__Undofile__getSize >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Undofile
 * Method:    getLogfileGroup
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Undofile_getLogfileGroup(JNIEnv * env, jobject obj)
{
    TRACE("jstring Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Undofile_getLogfileGroup(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Undofile_ct, ttrait_char_cp_jutf8null, &NdbDictionary::Undofile::getLogfileGroup >(env, obj);
#else
    return gcall_fr< ttrait_char_cp_jutf8null, ttrait_c_m_n_n_NdbDictionary_Undofile_cr, &NdbApiWrapper::NdbDictionary__Undofile__getLogfileGroup >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Undofile
 * Method:    getLogfileGroupId
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/ObjectId;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Undofile_getLogfileGroupId(JNIEnv * env, jobject obj, jobject p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Undofile_getLogfileGroupId(JNIEnv *, jobject, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Undofile_ct, ttrait_c_m_n_n_NdbDictionary_ObjectId_p, &NdbDictionary::Undofile::getLogfileGroupId >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_Undofile_cr, ttrait_c_m_n_n_NdbDictionary_ObjectId_p, &NdbApiWrapper::NdbDictionary__Undofile__getLogfileGroupId >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Undofile
 * Method:    getObjectStatus
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Undofile_getObjectStatus(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Undofile_getObjectStatus(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Undofile_ct, ttrait_c_m_n_n_NdbDictionary_Object_Status_iv/*_enum_*/, &NdbDictionary::Undofile::getObjectStatus >(env, obj);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbDictionary_Object_Status_iv/*_enum_*/, ttrait_c_m_n_n_NdbDictionary_Undofile_cr, &NdbApiWrapper::NdbDictionary__Undofile__getObjectStatus >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Undofile
 * Method:    getObjectVersion
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Undofile_getObjectVersion(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Undofile_getObjectVersion(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Undofile_ct, ttrait_int, &NdbDictionary::Undofile::getObjectVersion >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Undofile_cr, &NdbApiWrapper::NdbDictionary__Undofile__getObjectVersion >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Undofile
 * Method:    getObjectId
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Undofile_getObjectId(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Undofile_getObjectId(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Undofile_ct, ttrait_int, &NdbDictionary::Undofile::getObjectId >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbDictionary_Undofile_cr, &NdbApiWrapper::NdbDictionary__Undofile__getObjectId >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Undofile
 * Method:    create
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/Undofile;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Undofile_create__(JNIEnv * env, jclass cls)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Undofile_create__(JNIEnv *, jclass)");
    return gcreate< ttrait_c_m_n_n_NdbDictionary_Undofile_r >(env, cls);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Undofile
 * Method:    create
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/UndofileConst;)Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/Undofile;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Undofile_create__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024UndofileConst_2(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Undofile_create__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024UndofileConst_2(JNIEnv *, jclass, jobject)");
    return gcreate< ttrait_c_m_n_n_NdbDictionary_Undofile_r, ttrait_c_m_n_n_NdbDictionary_Undofile_cr >(env, cls, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Undofile
 * Method:    delete
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/Undofile;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Undofile_delete(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Undofile_delete(JNIEnv *, jclass, jobject)");
    gdelete< ttrait_c_m_n_n_NdbDictionary_Undofile_r >(env, cls, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Undofile
 * Method:    setPath
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Undofile_setPath(JNIEnv * env, jobject obj, jstring p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Undofile_setPath(JNIEnv *, jobject, jstring)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Undofile_t, ttrait_char_cp_jutf8null, &NdbDictionary::Undofile::setPath >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_Undofile_r, ttrait_char_cp_jutf8null, &NdbApiWrapper::NdbDictionary__Undofile__setPath >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Undofile
 * Method:    setSize
 * Signature: (J)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Undofile_setSize(JNIEnv * env, jobject obj, jlong p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Undofile_setSize(JNIEnv *, jobject, jlong)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Undofile_t, ttrait_Uint64, &NdbDictionary::Undofile::setSize >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_Undofile_r, ttrait_Uint64, &NdbApiWrapper::NdbDictionary__Undofile__setSize >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Undofile
 * Method:    setLogfileGroup
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Undofile_setLogfileGroup__Ljava_lang_String_2(JNIEnv * env, jobject obj, jstring p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Undofile_setLogfileGroup__Ljava_lang_String_2(JNIEnv *, jobject, jstring)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Undofile_t, ttrait_char_cp_jutf8null, &NdbDictionary::Undofile::setLogfileGroup >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_Undofile_r, ttrait_char_cp_jutf8null, &NdbApiWrapper::NdbDictionary__Undofile__setLogfileGroup__0 >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Undofile
 * Method:    setLogfileGroup
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/LogfileGroupConst;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Undofile_setLogfileGroup__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024LogfileGroupConst_2(JNIEnv * env, jobject obj, jobject p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Undofile_setLogfileGroup__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024LogfileGroupConst_2(JNIEnv *, jobject, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Undofile_t, ttrait_c_m_n_n_NdbDictionary_LogfileGroup_cr, &NdbDictionary::Undofile::setLogfileGroup >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_NdbDictionary_Undofile_r, ttrait_c_m_n_n_NdbDictionary_LogfileGroup_cr, &NdbApiWrapper::NdbDictionary__Undofile__setLogfileGroup__1 >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

// ---------------------------------------------------------------------------

//#include "com_mysql_ndbjtie_ndbapi_NdbError.h"

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbError
 * Method:    status
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbError_status__(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbError_status__(JNIEnv *, jobject)");
    return gget< ttrait_c_m_n_n_NdbError_t, ttrait_c_m_n_n_NdbError_Status_iv/*_enum_*/, &NdbError::status >(env, obj);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbError
 * Method:    classification
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbError_classification__(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbError_classification__(JNIEnv *, jobject)");
    return gget< ttrait_c_m_n_n_NdbError_t, ttrait_c_m_n_n_NdbError_Classification_iv/*_enum_*/, &NdbError::classification >(env, obj);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbError
 * Method:    code
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbError_code__(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbError_code__(JNIEnv *, jobject)");
    return gget< ttrait_c_m_n_n_NdbError_t, ttrait_int, &NdbError::code >(env, obj);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbError
 * Method:    mysql_code
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbError_mysql_1code__(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbError_mysql_1code__(JNIEnv *, jobject)");
    return gget< ttrait_c_m_n_n_NdbError_t, ttrait_int, &NdbError::mysql_code >(env, obj);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbError
 * Method:    message
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbError_message__(JNIEnv * env, jobject obj)
{
    TRACE("jstring Java_com_mysql_ndbjtie_ndbapi_NdbError_message__(JNIEnv *, jobject)");
    return gget< ttrait_c_m_n_n_NdbError_t, ttrait_char_cp_jutf8null, &NdbError::message >(env, obj);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbError
 * Method:    status
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbError_status__I(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbError_status__I(JNIEnv *, jobject, jint)");
    gset< ttrait_c_m_n_n_NdbError_t, ttrait_c_m_n_n_NdbError_Status_iv/*_enum_*/, &NdbError::status >(env, obj, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbError
 * Method:    classification
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbError_classification__I(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbError_classification__I(JNIEnv *, jobject, jint)");
    gset< ttrait_c_m_n_n_NdbError_t, ttrait_c_m_n_n_NdbError_Classification_iv/*_enum_*/, &NdbError::classification >(env, obj, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbError
 * Method:    code
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbError_code__I(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbError_code__I(JNIEnv *, jobject, jint)");
    gset< ttrait_c_m_n_n_NdbError_t, ttrait_int, &NdbError::code >(env, obj, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbError
 * Method:    mysql_code
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbError_mysql_1code__I(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbError_mysql_1code__I(JNIEnv *, jobject, jint)");
    gset< ttrait_c_m_n_n_NdbError_t, ttrait_int, &NdbError::mysql_code >(env, obj, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbError
 * Method:    message
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbError_message__Ljava_lang_String_2(JNIEnv * env, jobject obj, jstring p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbError_message__Ljava_lang_String_2(JNIEnv *, jobject, jstring)");
    gset< ttrait_c_m_n_n_NdbError_t, ttrait_char_cp_jutf8null, &NdbError::message >(env, obj, p0);
}

// ---------------------------------------------------------------------------

//#include "com_mysql_ndbjtie_ndbapi_NdbEventOperation.h"

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbEventOperation
 * Method:    isOverrun
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbEventOperation_isOverrun(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbEventOperation_isOverrun(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbEventOperation_ct, ttrait_int, &NdbEventOperation::isOverrun >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbEventOperation_cr, &NdbApiWrapper::NdbEventOperation__isOverrun >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbEventOperation
 * Method:    isConsistent
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbEventOperation_isConsistent(JNIEnv * env, jobject obj)
{
    TRACE("jboolean Java_com_mysql_ndbjtie_ndbapi_NdbEventOperation_isConsistent(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbEventOperation_ct, ttrait_bool, &NdbEventOperation::isConsistent >(env, obj);
#else
    return gcall_fr< ttrait_bool, ttrait_c_m_n_n_NdbEventOperation_cr, &NdbApiWrapper::NdbEventOperation__isConsistent >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbEventOperation
 * Method:    getEventType
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbEventOperation_getEventType(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbEventOperation_getEventType(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbEventOperation_ct, ttrait_c_m_n_n_NdbDictionary_Event_TableEvent_iv/*_enum_*/, &NdbEventOperation::getEventType >(env, obj);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbDictionary_Event_TableEvent_iv/*_enum_*/, ttrait_c_m_n_n_NdbEventOperation_cr, &NdbApiWrapper::NdbEventOperation__getEventType >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbEventOperation
 * Method:    tableNameChanged
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbEventOperation_tableNameChanged(JNIEnv * env, jobject obj)
{
    TRACE("jboolean Java_com_mysql_ndbjtie_ndbapi_NdbEventOperation_tableNameChanged(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbEventOperation_ct, ttrait_bool, &NdbEventOperation::tableNameChanged >(env, obj);
#else
    return gcall_fr< ttrait_bool, ttrait_c_m_n_n_NdbEventOperation_cr, &NdbApiWrapper::NdbEventOperation__tableNameChanged >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbEventOperation
 * Method:    tableFrmChanged
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbEventOperation_tableFrmChanged(JNIEnv * env, jobject obj)
{
    TRACE("jboolean Java_com_mysql_ndbjtie_ndbapi_NdbEventOperation_tableFrmChanged(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbEventOperation_ct, ttrait_bool, &NdbEventOperation::tableFrmChanged >(env, obj);
#else
    return gcall_fr< ttrait_bool, ttrait_c_m_n_n_NdbEventOperation_cr, &NdbApiWrapper::NdbEventOperation__tableFrmChanged >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbEventOperation
 * Method:    tableFragmentationChanged
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbEventOperation_tableFragmentationChanged(JNIEnv * env, jobject obj)
{
    TRACE("jboolean Java_com_mysql_ndbjtie_ndbapi_NdbEventOperation_tableFragmentationChanged(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbEventOperation_ct, ttrait_bool, &NdbEventOperation::tableFragmentationChanged >(env, obj);
#else
    return gcall_fr< ttrait_bool, ttrait_c_m_n_n_NdbEventOperation_cr, &NdbApiWrapper::NdbEventOperation__tableFragmentationChanged >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbEventOperation
 * Method:    tableRangeListChanged
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbEventOperation_tableRangeListChanged(JNIEnv * env, jobject obj)
{
    TRACE("jboolean Java_com_mysql_ndbjtie_ndbapi_NdbEventOperation_tableRangeListChanged(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbEventOperation_ct, ttrait_bool, &NdbEventOperation::tableRangeListChanged >(env, obj);
#else
    return gcall_fr< ttrait_bool, ttrait_c_m_n_n_NdbEventOperation_cr, &NdbApiWrapper::NdbEventOperation__tableRangeListChanged >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbEventOperation
 * Method:    getGCI
 * Signature: ()J
 */
JNIEXPORT jlong JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbEventOperation_getGCI(JNIEnv * env, jobject obj)
{
    TRACE("jlong Java_com_mysql_ndbjtie_ndbapi_NdbEventOperation_getGCI(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbEventOperation_ct, ttrait_Uint64, &NdbEventOperation::getGCI >(env, obj);
#else
    return gcall_fr< ttrait_Uint64, ttrait_c_m_n_n_NdbEventOperation_cr, &NdbApiWrapper::NdbEventOperation__getGCI >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbEventOperation
 * Method:    getAnyValue
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbEventOperation_getAnyValue(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbEventOperation_getAnyValue(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbEventOperation_ct, ttrait_Uint32, &NdbEventOperation::getAnyValue >(env, obj);
#else
    return gcall_fr< ttrait_Uint32, ttrait_c_m_n_n_NdbEventOperation_cr, &NdbApiWrapper::NdbEventOperation__getAnyValue >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbEventOperation
 * Method:    getLatestGCI
 * Signature: ()J
 */
JNIEXPORT jlong JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbEventOperation_getLatestGCI(JNIEnv * env, jobject obj)
{
    TRACE("jlong Java_com_mysql_ndbjtie_ndbapi_NdbEventOperation_getLatestGCI(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbEventOperation_ct, ttrait_Uint64, &NdbEventOperation::getLatestGCI >(env, obj);
#else
    return gcall_fr< ttrait_Uint64, ttrait_c_m_n_n_NdbEventOperation_cr, &NdbApiWrapper::NdbEventOperation__getLatestGCI >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbEventOperation
 * Method:    getNdbError
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbErrorConst;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbEventOperation_getNdbError(JNIEnv * env, jobject obj)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbEventOperation_getNdbError(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbEventOperation_ct, ttrait_c_m_n_n_NdbError_cr, &NdbEventOperation::getNdbError >(env, obj);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbError_cr, ttrait_c_m_n_n_NdbEventOperation_cr, &NdbApiWrapper::NdbEventOperation__getNdbError >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbEventOperation
 * Method:    getState
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbEventOperation_getState(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbEventOperation_getState(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbEventOperation_t, ttrait_c_m_n_n_NdbEventOperation_State_iv/*_enum_*/, &NdbEventOperation::getState >(env, obj);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbEventOperation_State_iv/*_enum_*/, ttrait_c_m_n_n_NdbEventOperation_r, &NdbApiWrapper::NdbEventOperation__getState >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbEventOperation
 * Method:    mergeEvents
 * Signature: (Z)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbEventOperation_mergeEvents(JNIEnv * env, jobject obj, jboolean p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbEventOperation_mergeEvents(JNIEnv *, jobject, jboolean)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbEventOperation_t, ttrait_bool, &NdbEventOperation::mergeEvents >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_NdbEventOperation_r, ttrait_bool, &NdbApiWrapper::NdbEventOperation__mergeEvents >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbEventOperation
 * Method:    execute
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbEventOperation_execute(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbEventOperation_execute(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbEventOperation_t, ttrait_int, &NdbEventOperation::execute >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbEventOperation_r, &NdbApiWrapper::NdbEventOperation__execute >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbEventOperation
 * Method:    getValue
 * Signature: (Ljava/lang/String;Ljava/nio/ByteBuffer;)Lcom/mysql/ndbjtie/ndbapi/NdbRecAttr;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbEventOperation_getValue(JNIEnv * env, jobject obj, jstring p0, jobject p1)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbEventOperation_getValue(JNIEnv *, jobject, jstring, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbEventOperation_t, ttrait_c_m_n_n_NdbRecAttr_p, ttrait_char_cp_jutf8null, ttrait_char_1p_bb, &NdbEventOperation::getValue >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbRecAttr_p, ttrait_c_m_n_n_NdbEventOperation_r, ttrait_char_cp_jutf8null, ttrait_char_1p_bb, &NdbApiWrapper::NdbEventOperation__getValue >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbEventOperation
 * Method:    getPreValue
 * Signature: (Ljava/lang/String;Ljava/nio/ByteBuffer;)Lcom/mysql/ndbjtie/ndbapi/NdbRecAttr;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbEventOperation_getPreValue(JNIEnv * env, jobject obj, jstring p0, jobject p1)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbEventOperation_getPreValue(JNIEnv *, jobject, jstring, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbEventOperation_t, ttrait_c_m_n_n_NdbRecAttr_p, ttrait_char_cp_jutf8null, ttrait_char_1p_bb, &NdbEventOperation::getPreValue >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbRecAttr_p, ttrait_c_m_n_n_NdbEventOperation_r, ttrait_char_cp_jutf8null, ttrait_char_1p_bb, &NdbApiWrapper::NdbEventOperation__getPreValue >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbEventOperation
 * Method:    getBlobHandle
 * Signature: (Ljava/lang/String;)Lcom/mysql/ndbjtie/ndbapi/NdbBlob;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbEventOperation_getBlobHandle(JNIEnv * env, jobject obj, jstring p0)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbEventOperation_getBlobHandle(JNIEnv *, jobject, jstring)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbEventOperation_t, ttrait_c_m_n_n_NdbBlob_p, ttrait_char_cp_jutf8null, &NdbEventOperation::getBlobHandle >(env, obj, p0);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbBlob_p, ttrait_c_m_n_n_NdbEventOperation_r, ttrait_char_cp_jutf8null, &NdbApiWrapper::NdbEventOperation__getBlobHandle >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbEventOperation
 * Method:    getPreBlobHandle
 * Signature: (Ljava/lang/String;)Lcom/mysql/ndbjtie/ndbapi/NdbBlob;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbEventOperation_getPreBlobHandle(JNIEnv * env, jobject obj, jstring p0)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbEventOperation_getPreBlobHandle(JNIEnv *, jobject, jstring)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbEventOperation_t, ttrait_c_m_n_n_NdbBlob_p, ttrait_char_cp_jutf8null, &NdbEventOperation::getPreBlobHandle >(env, obj, p0);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbBlob_p, ttrait_c_m_n_n_NdbEventOperation_r, ttrait_char_cp_jutf8null, &NdbApiWrapper::NdbEventOperation__getPreBlobHandle >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

// ---------------------------------------------------------------------------

//#include "com_mysql_ndbjtie_ndbapi_NdbIndexOperation.h"

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbIndexOperation
 * Method:    getIndex
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/IndexConst;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbIndexOperation_getIndex(JNIEnv * env, jobject obj)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbIndexOperation_getIndex(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbIndexOperation_ct, ttrait_c_m_n_n_NdbDictionary_Index_cp, &NdbIndexOperation::getIndex >(env, obj);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbDictionary_Index_cp, ttrait_c_m_n_n_NdbIndexOperation_cr, &NdbApiWrapper::NdbIndexOperation__getIndex >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbIndexOperation
 * Method:    insertTuple
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbIndexOperation_insertTuple(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbIndexOperation_insertTuple(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbIndexOperation_t, ttrait_int, &NdbIndexOperation::insertTuple >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbIndexOperation_r, &NdbApiWrapper::NdbIndexOperation__insertTuple >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbIndexOperation
 * Method:    readTuple
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbIndexOperation_readTuple(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbIndexOperation_readTuple(JNIEnv *, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbIndexOperation_t, ttrait_int, ttrait_c_m_n_n_NdbOperation_LockMode_iv/*_enum_*/, &NdbIndexOperation::readTuple >(env, obj, p0);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbIndexOperation_r, ttrait_c_m_n_n_NdbOperation_LockMode_iv/*_enum_*/, &NdbApiWrapper::NdbIndexOperation__readTuple >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbIndexOperation
 * Method:    updateTuple
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbIndexOperation_updateTuple(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbIndexOperation_updateTuple(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbIndexOperation_t, ttrait_int, &NdbIndexOperation::updateTuple >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbIndexOperation_r, &NdbApiWrapper::NdbIndexOperation__updateTuple >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbIndexOperation
 * Method:    deleteTuple
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbIndexOperation_deleteTuple(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbIndexOperation_deleteTuple(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbIndexOperation_t, ttrait_int, &NdbIndexOperation::deleteTuple >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbIndexOperation_r, &NdbApiWrapper::NdbIndexOperation__deleteTuple >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

// ---------------------------------------------------------------------------

//#include "com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation.h"

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation
 * Method:    getSorted
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_getSorted(JNIEnv * env, jobject obj)
{
    TRACE("jboolean Java_com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_getSorted(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbIndexScanOperation_ct, ttrait_bool, &NdbIndexScanOperation::getSorted >(env, obj);
#else
    return gcall_fr< ttrait_bool, ttrait_c_m_n_n_NdbIndexScanOperation_cr, &NdbApiWrapper::NdbIndexScanOperation__getSorted >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation
 * Method:    getDescending
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_getDescending(JNIEnv * env, jobject obj)
{
    TRACE("jboolean Java_com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_getDescending(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbIndexScanOperation_ct, ttrait_bool, &NdbIndexScanOperation::getDescending >(env, obj);
#else
    return gcall_fr< ttrait_bool, ttrait_c_m_n_n_NdbIndexScanOperation_cr, &NdbApiWrapper::NdbIndexScanOperation__getDescending >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation
 * Method:    readTuples
 * Signature: (IIII)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_readTuples(JNIEnv * env, jobject obj, jint p0, jint p1, jint p2, jint p3)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_readTuples(JNIEnv *, jobject, jint, jint, jint, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbIndexScanOperation_t, ttrait_int, ttrait_c_m_n_n_NdbOperation_LockMode_iv/*_enum_*/, ttrait_Uint32, ttrait_Uint32, ttrait_Uint32, &NdbIndexScanOperation::readTuples >(env, obj, p0, p1, p2, p3);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbIndexScanOperation_r, ttrait_c_m_n_n_NdbOperation_LockMode_iv/*_enum_*/, ttrait_Uint32, ttrait_Uint32, ttrait_Uint32, &NdbApiWrapper::NdbIndexScanOperation__readTuples >(env, NULL, obj, p0, p1, p2, p3);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation
 * Method:    setBound
 * Signature: (Ljava/lang/String;ILjava/nio/ByteBuffer;)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_setBound__Ljava_lang_String_2ILjava_nio_ByteBuffer_2(JNIEnv * env, jobject obj, jstring p0, jint p1, jobject p2)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_setBound__Ljava_lang_String_2ILjava_nio_ByteBuffer_2(JNIEnv *, jobject, jstring, jint, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbIndexScanOperation_t, ttrait_int, ttrait_char_cp_jutf8null, ttrait_int, ttrait_void_1cp_bb, &NdbIndexScanOperation::setBound >(env, obj, p0, p1, p2);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbIndexScanOperation_r, ttrait_char_cp_jutf8null, ttrait_int, ttrait_void_1cp_bb, &NdbApiWrapper::NdbIndexScanOperation__setBound__0 >(env, NULL, obj, p0, p1, p2);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation
 * Method:    setBound
 * Signature: (IILjava/nio/ByteBuffer;)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_setBound__IILjava_nio_ByteBuffer_2(JNIEnv * env, jobject obj, jint p0, jint p1, jobject p2)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_setBound__IILjava_nio_ByteBuffer_2(JNIEnv *, jobject, jint, jint, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbIndexScanOperation_t, ttrait_int, ttrait_Uint32, ttrait_int, ttrait_void_1cp_bb, &NdbIndexScanOperation::setBound >(env, obj, p0, p1, p2);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbIndexScanOperation_r, ttrait_Uint32, ttrait_int, ttrait_void_1cp_bb, &NdbApiWrapper::NdbIndexScanOperation__setBound__1 >(env, NULL, obj, p0, p1, p2);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation
 * Method:    end_of_bound
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_end_1of_1bound(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_end_1of_1bound(JNIEnv *, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbIndexScanOperation_t, ttrait_int, ttrait_Uint32, &NdbIndexScanOperation::end_of_bound >(env, obj, p0);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbIndexScanOperation_r, ttrait_Uint32, &NdbApiWrapper::NdbIndexScanOperation__end_of_bound >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation
 * Method:    get_range_no
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_get_1range_1no(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_get_1range_1no(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbIndexScanOperation_t, ttrait_int, &NdbIndexScanOperation::get_range_no >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbIndexScanOperation_r, &NdbApiWrapper::NdbIndexScanOperation__get_range_no >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation
 * Method:    setBound
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbRecordConst;Lcom/mysql/ndbjtie/ndbapi/NdbIndexScanOperation/IndexBoundConst;)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_setBound__Lcom_mysql_ndbjtie_ndbapi_NdbRecordConst_2Lcom_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_00024IndexBoundConst_2(JNIEnv * env, jobject obj, jobject p0, jobject p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_setBound__Lcom_mysql_ndbjtie_ndbapi_NdbRecordConst_2Lcom_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_00024IndexBoundConst_2(JNIEnv *, jobject, jobject, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbIndexScanOperation_t, ttrait_int, ttrait_c_m_n_n_NdbRecord_cp, ttrait_c_m_n_n_NdbIndexScanOperation_IndexBound_cr, &NdbIndexScanOperation::setBound >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbIndexScanOperation_r, ttrait_c_m_n_n_NdbRecord_cp, ttrait_c_m_n_n_NdbIndexScanOperation_IndexBound_cr, &NdbApiWrapper::NdbIndexScanOperation__setBound__2 >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

// ---------------------------------------------------------------------------

//#include "com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_IndexBound.h"

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_IndexBound
 * Method:    low_key
 * Signature: ()Ljava/nio/ByteBuffer;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_00024IndexBound_low_1key__(JNIEnv * env, jobject obj)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_00024IndexBound_low_1key__(JNIEnv *, jobject)");
    return gget< ttrait_c_m_n_n_NdbIndexScanOperation_IndexBound_t, ttrait_char_0cp_bb, &NdbIndexScanOperation::IndexBound::low_key >(env, obj);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_IndexBound
 * Method:    low_key_count
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_00024IndexBound_low_1key_1count__(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_00024IndexBound_low_1key_1count__(JNIEnv *, jobject)");
    return gget< ttrait_c_m_n_n_NdbIndexScanOperation_IndexBound_t, ttrait_Uint32, &NdbIndexScanOperation::IndexBound::low_key_count >(env, obj);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_IndexBound
 * Method:    low_inclusive
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_00024IndexBound_low_1inclusive__(JNIEnv * env, jobject obj)
{
    TRACE("jboolean Java_com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_00024IndexBound_low_1inclusive__(JNIEnv *, jobject)");
    return gget< ttrait_c_m_n_n_NdbIndexScanOperation_IndexBound_t, ttrait_bool, &NdbIndexScanOperation::IndexBound::low_inclusive >(env, obj);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_IndexBound
 * Method:    high_key
 * Signature: ()Ljava/nio/ByteBuffer;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_00024IndexBound_high_1key__(JNIEnv * env, jobject obj)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_00024IndexBound_high_1key__(JNIEnv *, jobject)");
    return gget< ttrait_c_m_n_n_NdbIndexScanOperation_IndexBound_t, ttrait_char_0cp_bb, &NdbIndexScanOperation::IndexBound::high_key >(env, obj);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_IndexBound
 * Method:    high_key_count
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_00024IndexBound_high_1key_1count__(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_00024IndexBound_high_1key_1count__(JNIEnv *, jobject)");
    return gget< ttrait_c_m_n_n_NdbIndexScanOperation_IndexBound_t, ttrait_Uint32, &NdbIndexScanOperation::IndexBound::high_key_count >(env, obj);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_IndexBound
 * Method:    high_inclusive
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_00024IndexBound_high_1inclusive__(JNIEnv * env, jobject obj)
{
    TRACE("jboolean Java_com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_00024IndexBound_high_1inclusive__(JNIEnv *, jobject)");
    return gget< ttrait_c_m_n_n_NdbIndexScanOperation_IndexBound_t, ttrait_bool, &NdbIndexScanOperation::IndexBound::high_inclusive >(env, obj);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_IndexBound
 * Method:    range_no
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_00024IndexBound_range_1no__(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_00024IndexBound_range_1no__(JNIEnv *, jobject)");
    return gget< ttrait_c_m_n_n_NdbIndexScanOperation_IndexBound_t, ttrait_Uint32, &NdbIndexScanOperation::IndexBound::range_no >(env, obj);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_IndexBound
 * Method:    low_key
 * Signature: (Ljava/nio/ByteBuffer;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_00024IndexBound_low_1key__Ljava_nio_ByteBuffer_2(JNIEnv * env, jobject obj, jobject p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_00024IndexBound_low_1key__Ljava_nio_ByteBuffer_2(JNIEnv *, jobject, jobject)");
    gset< ttrait_c_m_n_n_NdbIndexScanOperation_IndexBound_t, ttrait_char_0cp_bb, &NdbIndexScanOperation::IndexBound::low_key >(env, obj, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_IndexBound
 * Method:    low_key_count
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_00024IndexBound_low_1key_1count__I(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_00024IndexBound_low_1key_1count__I(JNIEnv *, jobject, jint)");
    gset< ttrait_c_m_n_n_NdbIndexScanOperation_IndexBound_t, ttrait_Uint32, &NdbIndexScanOperation::IndexBound::low_key_count >(env, obj, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_IndexBound
 * Method:    low_inclusive
 * Signature: (Z)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_00024IndexBound_low_1inclusive__Z(JNIEnv * env, jobject obj, jboolean p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_00024IndexBound_low_1inclusive__Z(JNIEnv *, jobject, jboolean)");
    gset< ttrait_c_m_n_n_NdbIndexScanOperation_IndexBound_t, ttrait_bool, &NdbIndexScanOperation::IndexBound::low_inclusive >(env, obj, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_IndexBound
 * Method:    high_key
 * Signature: (Ljava/nio/ByteBuffer;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_00024IndexBound_high_1key__Ljava_nio_ByteBuffer_2(JNIEnv * env, jobject obj, jobject p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_00024IndexBound_high_1key__Ljava_nio_ByteBuffer_2(JNIEnv *, jobject, jobject)");
    gset< ttrait_c_m_n_n_NdbIndexScanOperation_IndexBound_t, ttrait_char_0cp_bb, &NdbIndexScanOperation::IndexBound::high_key >(env, obj, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_IndexBound
 * Method:    high_key_count
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_00024IndexBound_high_1key_1count__I(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_00024IndexBound_high_1key_1count__I(JNIEnv *, jobject, jint)");
    gset< ttrait_c_m_n_n_NdbIndexScanOperation_IndexBound_t, ttrait_Uint32, &NdbIndexScanOperation::IndexBound::high_key_count >(env, obj, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_IndexBound
 * Method:    high_inclusive
 * Signature: (Z)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_00024IndexBound_high_1inclusive__Z(JNIEnv * env, jobject obj, jboolean p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_00024IndexBound_high_1inclusive__Z(JNIEnv *, jobject, jboolean)");
    gset< ttrait_c_m_n_n_NdbIndexScanOperation_IndexBound_t, ttrait_bool, &NdbIndexScanOperation::IndexBound::high_inclusive >(env, obj, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_IndexBound
 * Method:    range_no
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_00024IndexBound_range_1no__I(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_00024IndexBound_range_1no__I(JNIEnv *, jobject, jint)");
    gset< ttrait_c_m_n_n_NdbIndexScanOperation_IndexBound_t, ttrait_Uint32, &NdbIndexScanOperation::IndexBound::range_no >(env, obj, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_IndexBound
 * Method:    create
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbIndexScanOperation/IndexBound;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_00024IndexBound_create(JNIEnv * env, jclass cls)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_00024IndexBound_create(JNIEnv *, jclass)");
    return gcreate< ttrait_c_m_n_n_NdbIndexScanOperation_IndexBound_r >(env, cls);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_IndexBound
 * Method:    delete
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbIndexScanOperation/IndexBound;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_00024IndexBound_delete(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_00024IndexBound_delete(JNIEnv *, jclass, jobject)");
    gdelete< ttrait_c_m_n_n_NdbIndexScanOperation_IndexBound_r >(env, cls, p0);
}

// ---------------------------------------------------------------------------

//#include "com_mysql_ndbjtie_ndbapi_NdbInterpretedCode.h"

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbInterpretedCode
 * Method:    getTable
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/TableConst;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_getTable(JNIEnv * env, jobject obj)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_getTable(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbInterpretedCode_ct, ttrait_c_m_n_n_NdbDictionary_Table_cp, &NdbInterpretedCode::getTable >(env, obj);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbDictionary_Table_cp, ttrait_c_m_n_n_NdbInterpretedCode_cr, &NdbApiWrapper::NdbInterpretedCode__getTable >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbInterpretedCode
 * Method:    getNdbError
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbErrorConst;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_getNdbError(JNIEnv * env, jobject obj)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_getNdbError(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbInterpretedCode_ct, ttrait_c_m_n_n_NdbError_cr, &NdbInterpretedCode::getNdbError >(env, obj);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbError_cr, ttrait_c_m_n_n_NdbInterpretedCode_cr, &NdbApiWrapper::NdbInterpretedCode__getNdbError >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbInterpretedCode
 * Method:    getWordsUsed
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_getWordsUsed(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_getWordsUsed(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbInterpretedCode_ct, ttrait_Uint32, &NdbInterpretedCode::getWordsUsed >(env, obj);
#else
    return gcall_fr< ttrait_Uint32, ttrait_c_m_n_n_NdbInterpretedCode_cr, &NdbApiWrapper::NdbInterpretedCode__getWordsUsed >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbInterpretedCode
 * Method:    create
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/TableConst;[II)Lcom/mysql/ndbjtie/ndbapi/NdbInterpretedCode;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_create(JNIEnv * env, jclass cls, jobject p0, jintArray p1, jint p2)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_create(JNIEnv *, jclass, jobject, jintArray, jint)");
    return gcreate< ttrait_c_m_n_n_NdbInterpretedCode_r, ttrait_c_m_n_n_NdbDictionary_Table_cp, ttrait_Uint32_0p_bb, ttrait_Uint32 >(env, cls, p0, p1, p2);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbInterpretedCode
 * Method:    delete
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbInterpretedCode;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_delete(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_delete(JNIEnv *, jclass, jobject)");
    gdelete< ttrait_c_m_n_n_NdbInterpretedCode_r >(env, cls, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbInterpretedCode
 * Method:    load_const_null
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_load_1const_1null(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_load_1const_1null(JNIEnv *, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbInterpretedCode_t, ttrait_int, ttrait_Uint32, &NdbInterpretedCode::load_const_null >(env, obj, p0);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbInterpretedCode_r, ttrait_Uint32, &NdbApiWrapper::NdbInterpretedCode__load_const_null >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbInterpretedCode
 * Method:    load_const_u16
 * Signature: (II)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_load_1const_1u16(JNIEnv * env, jobject obj, jint p0, jint p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_load_1const_1u16(JNIEnv *, jobject, jint, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbInterpretedCode_t, ttrait_int, ttrait_Uint32, ttrait_Uint32, &NdbInterpretedCode::load_const_u16 >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbInterpretedCode_r, ttrait_Uint32, ttrait_Uint32, &NdbApiWrapper::NdbInterpretedCode__load_const_u16 >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbInterpretedCode
 * Method:    load_const_u32
 * Signature: (II)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_load_1const_1u32(JNIEnv * env, jobject obj, jint p0, jint p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_load_1const_1u32(JNIEnv *, jobject, jint, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbInterpretedCode_t, ttrait_int, ttrait_Uint32, ttrait_Uint32, &NdbInterpretedCode::load_const_u32 >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbInterpretedCode_r, ttrait_Uint32, ttrait_Uint32, &NdbApiWrapper::NdbInterpretedCode__load_const_u32 >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbInterpretedCode
 * Method:    load_const_u64
 * Signature: (IJ)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_load_1const_1u64(JNIEnv * env, jobject obj, jint p0, jlong p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_load_1const_1u64(JNIEnv *, jobject, jint, jlong)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbInterpretedCode_t, ttrait_int, ttrait_Uint32, ttrait_Uint64, &NdbInterpretedCode::load_const_u64 >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbInterpretedCode_r, ttrait_Uint32, ttrait_Uint64, &NdbApiWrapper::NdbInterpretedCode__load_const_u64 >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbInterpretedCode
 * Method:    read_attr
 * Signature: (II)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_read_1attr__II(JNIEnv * env, jobject obj, jint p0, jint p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_read_1attr__II(JNIEnv *, jobject, jint, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbInterpretedCode_t, ttrait_int, ttrait_Uint32, ttrait_Uint32, &NdbInterpretedCode::read_attr >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbInterpretedCode_r, ttrait_Uint32, ttrait_Uint32, &NdbApiWrapper::NdbInterpretedCode__read_attr__0 >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbInterpretedCode
 * Method:    read_attr
 * Signature: (ILcom/mysql/ndbjtie/ndbapi/NdbDictionary/ColumnConst;)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_read_1attr__ILcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024ColumnConst_2(JNIEnv * env, jobject obj, jint p0, jobject p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_read_1attr__ILcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024ColumnConst_2(JNIEnv *, jobject, jint, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbInterpretedCode_t, ttrait_int, ttrait_Uint32, ttrait_c_m_n_n_NdbDictionary_Column_cp, &NdbInterpretedCode::read_attr >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbInterpretedCode_r, ttrait_Uint32, ttrait_c_m_n_n_NdbDictionary_Column_cp, &NdbApiWrapper::NdbInterpretedCode__read_attr__1 >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbInterpretedCode
 * Method:    write_attr
 * Signature: (II)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_write_1attr__II(JNIEnv * env, jobject obj, jint p0, jint p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_write_1attr__II(JNIEnv *, jobject, jint, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbInterpretedCode_t, ttrait_int, ttrait_Uint32, ttrait_Uint32, &NdbInterpretedCode::write_attr >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbInterpretedCode_r, ttrait_Uint32, ttrait_Uint32, &NdbApiWrapper::NdbInterpretedCode__write_attr__0 >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbInterpretedCode
 * Method:    write_attr
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/ColumnConst;I)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_write_1attr__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024ColumnConst_2I(JNIEnv * env, jobject obj, jobject p0, jint p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_write_1attr__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024ColumnConst_2I(JNIEnv *, jobject, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbInterpretedCode_t, ttrait_int, ttrait_c_m_n_n_NdbDictionary_Column_cp, ttrait_Uint32, &NdbInterpretedCode::write_attr >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbInterpretedCode_r, ttrait_c_m_n_n_NdbDictionary_Column_cp, ttrait_Uint32, &NdbApiWrapper::NdbInterpretedCode__write_attr__1 >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbInterpretedCode
 * Method:    add_reg
 * Signature: (III)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_add_1reg(JNIEnv * env, jobject obj, jint p0, jint p1, jint p2)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_add_1reg(JNIEnv *, jobject, jint, jint, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbInterpretedCode_t, ttrait_int, ttrait_Uint32, ttrait_Uint32, ttrait_Uint32, &NdbInterpretedCode::add_reg >(env, obj, p0, p1, p2);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbInterpretedCode_r, ttrait_Uint32, ttrait_Uint32, ttrait_Uint32, &NdbApiWrapper::NdbInterpretedCode__add_reg >(env, NULL, obj, p0, p1, p2);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbInterpretedCode
 * Method:    sub_reg
 * Signature: (III)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_sub_1reg(JNIEnv * env, jobject obj, jint p0, jint p1, jint p2)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_sub_1reg(JNIEnv *, jobject, jint, jint, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbInterpretedCode_t, ttrait_int, ttrait_Uint32, ttrait_Uint32, ttrait_Uint32, &NdbInterpretedCode::sub_reg >(env, obj, p0, p1, p2);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbInterpretedCode_r, ttrait_Uint32, ttrait_Uint32, ttrait_Uint32, &NdbApiWrapper::NdbInterpretedCode__sub_reg >(env, NULL, obj, p0, p1, p2);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbInterpretedCode
 * Method:    def_label
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_def_1label(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_def_1label(JNIEnv *, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbInterpretedCode_t, ttrait_int, ttrait_int, &NdbInterpretedCode::def_label >(env, obj, p0);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbInterpretedCode_r, ttrait_int, &NdbApiWrapper::NdbInterpretedCode__def_label >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbInterpretedCode
 * Method:    branch_label
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_branch_1label(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_branch_1label(JNIEnv *, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbInterpretedCode_t, ttrait_int, ttrait_Uint32, &NdbInterpretedCode::branch_label >(env, obj, p0);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbInterpretedCode_r, ttrait_Uint32, &NdbApiWrapper::NdbInterpretedCode__branch_label >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbInterpretedCode
 * Method:    branch_ge
 * Signature: (III)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_branch_1ge(JNIEnv * env, jobject obj, jint p0, jint p1, jint p2)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_branch_1ge(JNIEnv *, jobject, jint, jint, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbInterpretedCode_t, ttrait_int, ttrait_Uint32, ttrait_Uint32, ttrait_Uint32, &NdbInterpretedCode::branch_ge >(env, obj, p0, p1, p2);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbInterpretedCode_r, ttrait_Uint32, ttrait_Uint32, ttrait_Uint32, &NdbApiWrapper::NdbInterpretedCode__branch_ge >(env, NULL, obj, p0, p1, p2);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbInterpretedCode
 * Method:    branch_gt
 * Signature: (III)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_branch_1gt(JNIEnv * env, jobject obj, jint p0, jint p1, jint p2)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_branch_1gt(JNIEnv *, jobject, jint, jint, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbInterpretedCode_t, ttrait_int, ttrait_Uint32, ttrait_Uint32, ttrait_Uint32, &NdbInterpretedCode::branch_gt >(env, obj, p0, p1, p2);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbInterpretedCode_r, ttrait_Uint32, ttrait_Uint32, ttrait_Uint32, &NdbApiWrapper::NdbInterpretedCode__branch_gt >(env, NULL, obj, p0, p1, p2);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbInterpretedCode
 * Method:    branch_le
 * Signature: (III)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_branch_1le(JNIEnv * env, jobject obj, jint p0, jint p1, jint p2)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_branch_1le(JNIEnv *, jobject, jint, jint, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbInterpretedCode_t, ttrait_int, ttrait_Uint32, ttrait_Uint32, ttrait_Uint32, &NdbInterpretedCode::branch_le >(env, obj, p0, p1, p2);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbInterpretedCode_r, ttrait_Uint32, ttrait_Uint32, ttrait_Uint32, &NdbApiWrapper::NdbInterpretedCode__branch_le >(env, NULL, obj, p0, p1, p2);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbInterpretedCode
 * Method:    branch_lt
 * Signature: (III)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_branch_1lt(JNIEnv * env, jobject obj, jint p0, jint p1, jint p2)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_branch_1lt(JNIEnv *, jobject, jint, jint, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbInterpretedCode_t, ttrait_int, ttrait_Uint32, ttrait_Uint32, ttrait_Uint32, &NdbInterpretedCode::branch_lt >(env, obj, p0, p1, p2);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbInterpretedCode_r, ttrait_Uint32, ttrait_Uint32, ttrait_Uint32, &NdbApiWrapper::NdbInterpretedCode__branch_lt >(env, NULL, obj, p0, p1, p2);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbInterpretedCode
 * Method:    branch_eq
 * Signature: (III)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_branch_1eq(JNIEnv * env, jobject obj, jint p0, jint p1, jint p2)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_branch_1eq(JNIEnv *, jobject, jint, jint, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbInterpretedCode_t, ttrait_int, ttrait_Uint32, ttrait_Uint32, ttrait_Uint32, &NdbInterpretedCode::branch_eq >(env, obj, p0, p1, p2);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbInterpretedCode_r, ttrait_Uint32, ttrait_Uint32, ttrait_Uint32, &NdbApiWrapper::NdbInterpretedCode__branch_eq >(env, NULL, obj, p0, p1, p2);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbInterpretedCode
 * Method:    branch_ne
 * Signature: (III)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_branch_1ne(JNIEnv * env, jobject obj, jint p0, jint p1, jint p2)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_branch_1ne(JNIEnv *, jobject, jint, jint, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbInterpretedCode_t, ttrait_int, ttrait_Uint32, ttrait_Uint32, ttrait_Uint32, &NdbInterpretedCode::branch_ne >(env, obj, p0, p1, p2);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbInterpretedCode_r, ttrait_Uint32, ttrait_Uint32, ttrait_Uint32, &NdbApiWrapper::NdbInterpretedCode__branch_ne >(env, NULL, obj, p0, p1, p2);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbInterpretedCode
 * Method:    branch_ne_null
 * Signature: (II)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_branch_1ne_1null(JNIEnv * env, jobject obj, jint p0, jint p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_branch_1ne_1null(JNIEnv *, jobject, jint, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbInterpretedCode_t, ttrait_int, ttrait_Uint32, ttrait_Uint32, &NdbInterpretedCode::branch_ne_null >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbInterpretedCode_r, ttrait_Uint32, ttrait_Uint32, &NdbApiWrapper::NdbInterpretedCode__branch_ne_null >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbInterpretedCode
 * Method:    branch_eq_null
 * Signature: (II)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_branch_1eq_1null(JNIEnv * env, jobject obj, jint p0, jint p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_branch_1eq_1null(JNIEnv *, jobject, jint, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbInterpretedCode_t, ttrait_int, ttrait_Uint32, ttrait_Uint32, &NdbInterpretedCode::branch_eq_null >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbInterpretedCode_r, ttrait_Uint32, ttrait_Uint32, &NdbApiWrapper::NdbInterpretedCode__branch_eq_null >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbInterpretedCode
 * Method:    branch_col_eq
 * Signature: (Ljava/nio/ByteBuffer;III)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_branch_1col_1eq(JNIEnv * env, jobject obj, jobject p0, jint p1, jint p2, jint p3)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_branch_1col_1eq(JNIEnv *, jobject, jobject, jint, jint, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbInterpretedCode_t, ttrait_int, ttrait_void_1cp_bb, ttrait_Uint32, ttrait_Uint32, ttrait_Uint32, &NdbInterpretedCode::branch_col_eq >(env, obj, p0, p1, p2, p3);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbInterpretedCode_r, ttrait_void_1cp_bb, ttrait_Uint32, ttrait_Uint32, ttrait_Uint32, &NdbApiWrapper::NdbInterpretedCode__branch_col_eq >(env, NULL, obj, p0, p1, p2, p3);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbInterpretedCode
 * Method:    branch_col_ne
 * Signature: (Ljava/nio/ByteBuffer;III)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_branch_1col_1ne(JNIEnv * env, jobject obj, jobject p0, jint p1, jint p2, jint p3)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_branch_1col_1ne(JNIEnv *, jobject, jobject, jint, jint, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbInterpretedCode_t, ttrait_int, ttrait_void_1cp_bb, ttrait_Uint32, ttrait_Uint32, ttrait_Uint32, &NdbInterpretedCode::branch_col_ne >(env, obj, p0, p1, p2, p3);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbInterpretedCode_r, ttrait_void_1cp_bb, ttrait_Uint32, ttrait_Uint32, ttrait_Uint32, &NdbApiWrapper::NdbInterpretedCode__branch_col_ne >(env, NULL, obj, p0, p1, p2, p3);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbInterpretedCode
 * Method:    branch_col_lt
 * Signature: (Ljava/nio/ByteBuffer;III)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_branch_1col_1lt(JNIEnv * env, jobject obj, jobject p0, jint p1, jint p2, jint p3)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_branch_1col_1lt(JNIEnv *, jobject, jobject, jint, jint, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbInterpretedCode_t, ttrait_int, ttrait_void_1cp_bb, ttrait_Uint32, ttrait_Uint32, ttrait_Uint32, &NdbInterpretedCode::branch_col_lt >(env, obj, p0, p1, p2, p3);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbInterpretedCode_r, ttrait_void_1cp_bb, ttrait_Uint32, ttrait_Uint32, ttrait_Uint32, &NdbApiWrapper::NdbInterpretedCode__branch_col_lt >(env, NULL, obj, p0, p1, p2, p3);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbInterpretedCode
 * Method:    branch_col_le
 * Signature: (Ljava/nio/ByteBuffer;III)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_branch_1col_1le(JNIEnv * env, jobject obj, jobject p0, jint p1, jint p2, jint p3)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_branch_1col_1le(JNIEnv *, jobject, jobject, jint, jint, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbInterpretedCode_t, ttrait_int, ttrait_void_1cp_bb, ttrait_Uint32, ttrait_Uint32, ttrait_Uint32, &NdbInterpretedCode::branch_col_le >(env, obj, p0, p1, p2, p3);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbInterpretedCode_r, ttrait_void_1cp_bb, ttrait_Uint32, ttrait_Uint32, ttrait_Uint32, &NdbApiWrapper::NdbInterpretedCode__branch_col_le >(env, NULL, obj, p0, p1, p2, p3);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbInterpretedCode
 * Method:    branch_col_gt
 * Signature: (Ljava/nio/ByteBuffer;III)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_branch_1col_1gt(JNIEnv * env, jobject obj, jobject p0, jint p1, jint p2, jint p3)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_branch_1col_1gt(JNIEnv *, jobject, jobject, jint, jint, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbInterpretedCode_t, ttrait_int, ttrait_void_1cp_bb, ttrait_Uint32, ttrait_Uint32, ttrait_Uint32, &NdbInterpretedCode::branch_col_gt >(env, obj, p0, p1, p2, p3);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbInterpretedCode_r, ttrait_void_1cp_bb, ttrait_Uint32, ttrait_Uint32, ttrait_Uint32, &NdbApiWrapper::NdbInterpretedCode__branch_col_gt >(env, NULL, obj, p0, p1, p2, p3);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbInterpretedCode
 * Method:    branch_col_ge
 * Signature: (Ljava/nio/ByteBuffer;III)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_branch_1col_1ge(JNIEnv * env, jobject obj, jobject p0, jint p1, jint p2, jint p3)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_branch_1col_1ge(JNIEnv *, jobject, jobject, jint, jint, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbInterpretedCode_t, ttrait_int, ttrait_void_1cp_bb, ttrait_Uint32, ttrait_Uint32, ttrait_Uint32, &NdbInterpretedCode::branch_col_ge >(env, obj, p0, p1, p2, p3);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbInterpretedCode_r, ttrait_void_1cp_bb, ttrait_Uint32, ttrait_Uint32, ttrait_Uint32, &NdbApiWrapper::NdbInterpretedCode__branch_col_ge >(env, NULL, obj, p0, p1, p2, p3);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbInterpretedCode
 * Method:    branch_col_eq_null
 * Signature: (II)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_branch_1col_1eq_1null(JNIEnv * env, jobject obj, jint p0, jint p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_branch_1col_1eq_1null(JNIEnv *, jobject, jint, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbInterpretedCode_t, ttrait_int, ttrait_Uint32, ttrait_Uint32, &NdbInterpretedCode::branch_col_eq_null >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbInterpretedCode_r, ttrait_Uint32, ttrait_Uint32, &NdbApiWrapper::NdbInterpretedCode__branch_col_eq_null >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbInterpretedCode
 * Method:    branch_col_ne_null
 * Signature: (II)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_branch_1col_1ne_1null(JNIEnv * env, jobject obj, jint p0, jint p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_branch_1col_1ne_1null(JNIEnv *, jobject, jint, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbInterpretedCode_t, ttrait_int, ttrait_Uint32, ttrait_Uint32, &NdbInterpretedCode::branch_col_ne_null >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbInterpretedCode_r, ttrait_Uint32, ttrait_Uint32, &NdbApiWrapper::NdbInterpretedCode__branch_col_ne_null >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbInterpretedCode
 * Method:    branch_col_like
 * Signature: (Ljava/nio/ByteBuffer;III)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_branch_1col_1like(JNIEnv * env, jobject obj, jobject p0, jint p1, jint p2, jint p3)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_branch_1col_1like(JNIEnv *, jobject, jobject, jint, jint, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbInterpretedCode_t, ttrait_int, ttrait_void_1cp_bb, ttrait_Uint32, ttrait_Uint32, ttrait_Uint32, &NdbInterpretedCode::branch_col_like >(env, obj, p0, p1, p2, p3);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbInterpretedCode_r, ttrait_void_1cp_bb, ttrait_Uint32, ttrait_Uint32, ttrait_Uint32, &NdbApiWrapper::NdbInterpretedCode__branch_col_like >(env, NULL, obj, p0, p1, p2, p3);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbInterpretedCode
 * Method:    branch_col_notlike
 * Signature: (Ljava/nio/ByteBuffer;III)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_branch_1col_1notlike(JNIEnv * env, jobject obj, jobject p0, jint p1, jint p2, jint p3)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_branch_1col_1notlike(JNIEnv *, jobject, jobject, jint, jint, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbInterpretedCode_t, ttrait_int, ttrait_void_1cp_bb, ttrait_Uint32, ttrait_Uint32, ttrait_Uint32, &NdbInterpretedCode::branch_col_notlike >(env, obj, p0, p1, p2, p3);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbInterpretedCode_r, ttrait_void_1cp_bb, ttrait_Uint32, ttrait_Uint32, ttrait_Uint32, &NdbApiWrapper::NdbInterpretedCode__branch_col_notlike >(env, NULL, obj, p0, p1, p2, p3);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbInterpretedCode
 * Method:    interpret_exit_ok
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_interpret_1exit_1ok(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_interpret_1exit_1ok(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbInterpretedCode_t, ttrait_int, &NdbInterpretedCode::interpret_exit_ok >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbInterpretedCode_r, &NdbApiWrapper::NdbInterpretedCode__interpret_exit_ok >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbInterpretedCode
 * Method:    interpret_exit_nok
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_interpret_1exit_1nok__I(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_interpret_1exit_1nok__I(JNIEnv *, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbInterpretedCode_t, ttrait_int, ttrait_Uint32, &NdbInterpretedCode::interpret_exit_nok >(env, obj, p0);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbInterpretedCode_r, ttrait_Uint32, &NdbApiWrapper::NdbInterpretedCode__interpret_exit_nok__0 >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbInterpretedCode
 * Method:    interpret_exit_nok
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_interpret_1exit_1nok__(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_interpret_1exit_1nok__(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbInterpretedCode_t, ttrait_int, &NdbInterpretedCode::interpret_exit_nok >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbInterpretedCode_r, &NdbApiWrapper::NdbInterpretedCode__interpret_exit_nok__1 >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbInterpretedCode
 * Method:    interpret_exit_last_row
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_interpret_1exit_1last_1row(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_interpret_1exit_1last_1row(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbInterpretedCode_t, ttrait_int, &NdbInterpretedCode::interpret_exit_last_row >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbInterpretedCode_r, &NdbApiWrapper::NdbInterpretedCode__interpret_exit_last_row >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbInterpretedCode
 * Method:    add_val
 * Signature: (II)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_add_1val__II(JNIEnv * env, jobject obj, jint p0, jint p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_add_1val__II(JNIEnv *, jobject, jint, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbInterpretedCode_t, ttrait_int, ttrait_Uint32, ttrait_Uint32, &NdbInterpretedCode::add_val >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbInterpretedCode_r, ttrait_Uint32, ttrait_Uint32, &NdbApiWrapper::NdbInterpretedCode__add_val__0 >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbInterpretedCode
 * Method:    add_val
 * Signature: (IJ)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_add_1val__IJ(JNIEnv * env, jobject obj, jint p0, jlong p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_add_1val__IJ(JNIEnv *, jobject, jint, jlong)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbInterpretedCode_t, ttrait_int, ttrait_Uint32, ttrait_Uint64, &NdbInterpretedCode::add_val >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbInterpretedCode_r, ttrait_Uint32, ttrait_Uint64, &NdbApiWrapper::NdbInterpretedCode__add_val__1 >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbInterpretedCode
 * Method:    sub_val
 * Signature: (II)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_sub_1val__II(JNIEnv * env, jobject obj, jint p0, jint p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_sub_1val__II(JNIEnv *, jobject, jint, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbInterpretedCode_t, ttrait_int, ttrait_Uint32, ttrait_Uint32, &NdbInterpretedCode::sub_val >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbInterpretedCode_r, ttrait_Uint32, ttrait_Uint32, &NdbApiWrapper::NdbInterpretedCode__sub_val__0 >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbInterpretedCode
 * Method:    sub_val
 * Signature: (IJ)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_sub_1val__IJ(JNIEnv * env, jobject obj, jint p0, jlong p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_sub_1val__IJ(JNIEnv *, jobject, jint, jlong)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbInterpretedCode_t, ttrait_int, ttrait_Uint32, ttrait_Uint64, &NdbInterpretedCode::sub_val >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbInterpretedCode_r, ttrait_Uint32, ttrait_Uint64, &NdbApiWrapper::NdbInterpretedCode__sub_val__1 >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbInterpretedCode
 * Method:    def_sub
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_def_1sub(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_def_1sub(JNIEnv *, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbInterpretedCode_t, ttrait_int, ttrait_Uint32, &NdbInterpretedCode::def_sub >(env, obj, p0);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbInterpretedCode_r, ttrait_Uint32, &NdbApiWrapper::NdbInterpretedCode__def_sub >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbInterpretedCode
 * Method:    call_sub
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_call_1sub(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_call_1sub(JNIEnv *, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbInterpretedCode_t, ttrait_int, ttrait_Uint32, &NdbInterpretedCode::call_sub >(env, obj, p0);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbInterpretedCode_r, ttrait_Uint32, &NdbApiWrapper::NdbInterpretedCode__call_sub >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbInterpretedCode
 * Method:    ret_sub
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_ret_1sub(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_ret_1sub(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbInterpretedCode_t, ttrait_int, &NdbInterpretedCode::ret_sub >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbInterpretedCode_r, &NdbApiWrapper::NdbInterpretedCode__ret_sub >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbInterpretedCode
 * Method:    finalise
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_finalise(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_finalise(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbInterpretedCode_t, ttrait_int, &NdbInterpretedCode::finalise >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbInterpretedCode_r, &NdbApiWrapper::NdbInterpretedCode__finalise >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

// ---------------------------------------------------------------------------

//#include "com_mysql_ndbjtie_ndbapi_NdbOperation.h"

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation
 * Method:    getBlobHandle
 * Signature: (Ljava/lang/String;)Lcom/mysql/ndbjtie/ndbapi/NdbBlob;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_getBlobHandle__Ljava_lang_String_2(JNIEnv * env, jobject obj, jstring p0)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbOperation_getBlobHandle__Ljava_lang_String_2(JNIEnv *, jobject, jstring)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_CONST_OVERLOADED_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_ct, ttrait_c_m_n_n_NdbBlob_p, ttrait_char_cp_jutf8null, &NdbOperation::getBlobHandle >(env, obj, p0); // call of overloaded const/non-const method
#else
    return gcall_fr< ttrait_c_m_n_n_NdbBlob_p, ttrait_c_m_n_n_NdbOperation_cr, ttrait_char_cp_jutf8null, &NdbApiWrapper::NdbOperation__getBlobHandle__0 >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_CONST_OVERLOADED_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation
 * Method:    getBlobHandle
 * Signature: (I)Lcom/mysql/ndbjtie/ndbapi/NdbBlob;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_getBlobHandle__I(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbOperation_getBlobHandle__I(JNIEnv *, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_CONST_OVERLOADED_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_ct, ttrait_c_m_n_n_NdbBlob_p, ttrait_Uint32, &NdbOperation::getBlobHandle >(env, obj, p0); // call of overloaded const/non-const method
#else
    return gcall_fr< ttrait_c_m_n_n_NdbBlob_p, ttrait_c_m_n_n_NdbOperation_cr, ttrait_Uint32, &NdbApiWrapper::NdbOperation__getBlobHandle__1 >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_CONST_OVERLOADED_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation
 * Method:    getNdbError
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbErrorConst;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_getNdbError(JNIEnv * env, jobject obj)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbOperation_getNdbError(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_ct, ttrait_c_m_n_n_NdbError_cr, &NdbOperation::getNdbError >(env, obj);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbError_cr, ttrait_c_m_n_n_NdbOperation_cr, &NdbApiWrapper::NdbOperation__getNdbError >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation
 * Method:    getNdbErrorLine
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_getNdbErrorLine(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbOperation_getNdbErrorLine(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_ct, ttrait_int, &NdbOperation::getNdbErrorLine >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbOperation_cr, &NdbApiWrapper::NdbOperation__getNdbErrorLine >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation
 * Method:    getTableName
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_getTableName(JNIEnv * env, jobject obj)
{
    TRACE("jstring Java_com_mysql_ndbjtie_ndbapi_NdbOperation_getTableName(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_ct, ttrait_char_cp_jutf8null, &NdbOperation::getTableName >(env, obj);
#else
    return gcall_fr< ttrait_char_cp_jutf8null, ttrait_c_m_n_n_NdbOperation_cr, &NdbApiWrapper::NdbOperation__getTableName >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation
 * Method:    getTable
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/TableConst;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_getTable(JNIEnv * env, jobject obj)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbOperation_getTable(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_ct, ttrait_c_m_n_n_NdbDictionary_Table_cp, &NdbOperation::getTable >(env, obj);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbDictionary_Table_cp, ttrait_c_m_n_n_NdbOperation_cr, &NdbApiWrapper::NdbOperation__getTable >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation
 * Method:    getType
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_getType(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbOperation_getType(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_ct, ttrait_c_m_n_n_NdbOperation_Type_iv/*_enum_*/, &NdbOperation::getType >(env, obj); // MMM const return
#else
    return gcall_fr< ttrait_c_m_n_n_NdbOperation_Type_iv/*_enum_*/, ttrait_c_m_n_n_NdbOperation_cr, &NdbApiWrapper::NdbOperation__getType >(env, NULL, obj); // MMM const return
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation
 * Method:    getLockMode
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_getLockMode(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbOperation_getLockMode(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_ct, ttrait_c_m_n_n_NdbOperation_LockMode_iv/*_enum_*/, &NdbOperation::getLockMode >(env, obj);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbOperation_LockMode_iv/*_enum_*/, ttrait_c_m_n_n_NdbOperation_cr, &NdbApiWrapper::NdbOperation__getLockMode >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation
 * Method:    getAbortOption
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_getAbortOption(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbOperation_getAbortOption(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_ct, ttrait_c_m_n_n_NdbOperation_AbortOption_iv/*_enum_*/, &NdbOperation::getAbortOption >(env, obj);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbOperation_AbortOption_iv/*_enum_*/, ttrait_c_m_n_n_NdbOperation_cr, &NdbApiWrapper::NdbOperation__getAbortOption >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation
 * Method:    getNdbTransaction
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbTransaction;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_getNdbTransaction(JNIEnv * env, jobject obj)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbOperation_getNdbTransaction(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_ct, ttrait_c_m_n_n_NdbTransaction_p, &NdbOperation::getNdbTransaction >(env, obj);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbTransaction_p, ttrait_c_m_n_n_NdbOperation_cr, &NdbApiWrapper::NdbOperation__getNdbTransaction >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation
 * Method:    getLockHandle
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbLockHandle;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_getLockHandle(JNIEnv * env, jobject obj)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbOperation_getLockHandle(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_CONST_OVERLOADED_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_ct, ttrait_c_m_n_n_NdbLockHandle_cp, &NdbOperation::getLockHandle >(env, obj); // call of overloaded const/non-const method
#else
    return gcall_fr< ttrait_c_m_n_n_NdbLockHandle_cp, ttrait_c_m_n_n_NdbOperation_cr, &NdbApiWrapper::NdbOperation__getLockHandle__0 >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_CONST_OVERLOADED_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation
 * Method:    getLockHandleM
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbLockHandle;
 */
JNIEXPORT jobject JNICALL Java_com_mysql_ndbjtie_ndbapi_NdbOperation_getLockHandleM(JNIEnv * env, jobject obj)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbOperation_getLockHandleM(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_CONST_OVERLOADED_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_t, ttrait_c_m_n_n_NdbLockHandle_cp, &NdbOperation::getLockHandle >(env, obj); // call of overloaded const/non-const method
#else
    return gcall_fr< ttrait_c_m_n_n_NdbLockHandle_cp, ttrait_c_m_n_n_NdbOperation_r, &NdbApiWrapper::NdbOperation__getLockHandle__1 >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_CONST_OVERLOADED_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation
 * Method:    insertTuple
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_insertTuple(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbOperation_insertTuple(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_t, ttrait_int, &NdbOperation::insertTuple >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbOperation_r, &NdbApiWrapper::NdbOperation__insertTuple >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation
 * Method:    updateTuple
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_updateTuple(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbOperation_updateTuple(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_t, ttrait_int, &NdbOperation::updateTuple >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbOperation_r, &NdbApiWrapper::NdbOperation__updateTuple >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation
 * Method:    writeTuple
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_writeTuple(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbOperation_writeTuple(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_t, ttrait_int, &NdbOperation::writeTuple >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbOperation_r, &NdbApiWrapper::NdbOperation__writeTuple >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation
 * Method:    deleteTuple
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_deleteTuple(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbOperation_deleteTuple(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_t, ttrait_int, &NdbOperation::deleteTuple >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbOperation_r, &NdbApiWrapper::NdbOperation__deleteTuple >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation
 * Method:    readTuple
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_readTuple(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbOperation_readTuple(JNIEnv *, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_t, ttrait_int, ttrait_c_m_n_n_NdbOperation_LockMode_iv/*_enum_*/, &NdbOperation::readTuple >(env, obj, p0);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbOperation_r, ttrait_c_m_n_n_NdbOperation_LockMode_iv/*_enum_*/, &NdbApiWrapper::NdbOperation__readTuple >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation
 * Method:    equal
 * Signature: (Ljava/lang/String;Ljava/nio/ByteBuffer;)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_equal__Ljava_lang_String_2Ljava_nio_ByteBuffer_2(JNIEnv * env, jobject obj, jstring p0, jobject p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbOperation_equal__Ljava_lang_String_2Ljava_nio_ByteBuffer_2(JNIEnv *, jobject, jstring, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_t, ttrait_int, ttrait_char_cp_jutf8null, ttrait_char_1cp_bb, &NdbOperation::equal >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbOperation_r, ttrait_char_cp_jutf8null, ttrait_char_1cp_bb, &NdbApiWrapper::NdbOperation__equal__0 >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation
 * Method:    equal
 * Signature: (Ljava/lang/String;I)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_equal__Ljava_lang_String_2I(JNIEnv * env, jobject obj, jstring p0, jint p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbOperation_equal__Ljava_lang_String_2I(JNIEnv *, jobject, jstring, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_t, ttrait_int, ttrait_char_cp_jutf8null, ttrait_Int32, &NdbOperation::equal >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbOperation_r, ttrait_char_cp_jutf8null, ttrait_Int32, &NdbApiWrapper::NdbOperation__equal__1 >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation
 * Method:    equal
 * Signature: (Ljava/lang/String;J)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_equal__Ljava_lang_String_2J(JNIEnv * env, jobject obj, jstring p0, jlong p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbOperation_equal__Ljava_lang_String_2J(JNIEnv *, jobject, jstring, jlong)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_t, ttrait_int, ttrait_char_cp_jutf8null, ttrait_Int64, &NdbOperation::equal >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbOperation_r, ttrait_char_cp_jutf8null, ttrait_Int64, &NdbApiWrapper::NdbOperation__equal__2 >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation
 * Method:    equal
 * Signature: (ILjava/nio/ByteBuffer;)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_equal__ILjava_nio_ByteBuffer_2(JNIEnv * env, jobject obj, jint p0, jobject p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbOperation_equal__ILjava_nio_ByteBuffer_2(JNIEnv *, jobject, jint, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_t, ttrait_int, ttrait_Uint32, ttrait_char_1cp_bb, &NdbOperation::equal >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbOperation_r, ttrait_Uint32, ttrait_char_1cp_bb, &NdbApiWrapper::NdbOperation__equal__3 >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation
 * Method:    equal
 * Signature: (II)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_equal__II(JNIEnv * env, jobject obj, jint p0, jint p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbOperation_equal__II(JNIEnv *, jobject, jint, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_t, ttrait_int, ttrait_Uint32, ttrait_Int32, &NdbOperation::equal >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbOperation_r, ttrait_Uint32, ttrait_Int32, &NdbApiWrapper::NdbOperation__equal__4 >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation
 * Method:    equal
 * Signature: (IJ)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_equal__IJ(JNIEnv * env, jobject obj, jint p0, jlong p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbOperation_equal__IJ(JNIEnv *, jobject, jint, jlong)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_t, ttrait_int, ttrait_Uint32, ttrait_Int64, &NdbOperation::equal >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbOperation_r, ttrait_Uint32, ttrait_Int64, &NdbApiWrapper::NdbOperation__equal__5 >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation
 * Method:    getValue
 * Signature: (Ljava/lang/String;Ljava/nio/ByteBuffer;)Lcom/mysql/ndbjtie/ndbapi/NdbRecAttr;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_getValue__Ljava_lang_String_2Ljava_nio_ByteBuffer_2(JNIEnv * env, jobject obj, jstring p0, jobject p1)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbOperation_getValue__Ljava_lang_String_2Ljava_nio_ByteBuffer_2(JNIEnv *, jobject, jstring, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_t, ttrait_c_m_n_n_NdbRecAttr_p, ttrait_char_cp_jutf8null, ttrait_char_1p_bb, &NdbOperation::getValue >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbRecAttr_p, ttrait_c_m_n_n_NdbOperation_r, ttrait_char_cp_jutf8null, ttrait_char_1p_bb, &NdbApiWrapper::NdbOperation__getValue__0 >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation
 * Method:    getValue
 * Signature: (ILjava/nio/ByteBuffer;)Lcom/mysql/ndbjtie/ndbapi/NdbRecAttr;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_getValue__ILjava_nio_ByteBuffer_2(JNIEnv * env, jobject obj, jint p0, jobject p1)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbOperation_getValue__ILjava_nio_ByteBuffer_2(JNIEnv *, jobject, jint, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_t, ttrait_c_m_n_n_NdbRecAttr_p, ttrait_Uint32, ttrait_char_1p_bb, &NdbOperation::getValue >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbRecAttr_p, ttrait_c_m_n_n_NdbOperation_r, ttrait_Uint32, ttrait_char_1p_bb, &NdbApiWrapper::NdbOperation__getValue__1 >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation
 * Method:    getValue
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/ColumnConst;Ljava/nio/ByteBuffer;)Lcom/mysql/ndbjtie/ndbapi/NdbRecAttr;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_getValue__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024ColumnConst_2Ljava_nio_ByteBuffer_2(JNIEnv * env, jobject obj, jobject p0, jobject p1)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbOperation_getValue__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024ColumnConst_2Ljava_nio_ByteBuffer_2(JNIEnv *, jobject, jobject, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_t, ttrait_c_m_n_n_NdbRecAttr_p, ttrait_c_m_n_n_NdbDictionary_Column_cp, ttrait_char_1p_bb, &NdbOperation::getValue >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbRecAttr_p, ttrait_c_m_n_n_NdbOperation_r, ttrait_c_m_n_n_NdbDictionary_Column_cp, ttrait_char_1p_bb, &NdbApiWrapper::NdbOperation__getValue__2 >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation
 * Method:    setValue
 * Signature: (Ljava/lang/String;Ljava/nio/ByteBuffer;)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_setValue__Ljava_lang_String_2Ljava_nio_ByteBuffer_2(JNIEnv * env, jobject obj, jstring p0, jobject p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbOperation_setValue__Ljava_lang_String_2Ljava_nio_ByteBuffer_2(JNIEnv *, jobject, jstring, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_t, ttrait_int, ttrait_char_cp_jutf8null, ttrait_char_1cp_bb, &NdbOperation::setValue >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbOperation_r, ttrait_char_cp_jutf8null, ttrait_char_1cp_bb, &NdbApiWrapper::NdbOperation__setValue__0 >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation
 * Method:    setValue
 * Signature: (Ljava/lang/String;I)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_setValue__Ljava_lang_String_2I(JNIEnv * env, jobject obj, jstring p0, jint p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbOperation_setValue__Ljava_lang_String_2I(JNIEnv *, jobject, jstring, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_t, ttrait_int, ttrait_char_cp_jutf8null, ttrait_Int32, &NdbOperation::setValue >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbOperation_r, ttrait_char_cp_jutf8null, ttrait_Int32, &NdbApiWrapper::NdbOperation__setValue__1 >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation
 * Method:    setValue
 * Signature: (Ljava/lang/String;J)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_setValue__Ljava_lang_String_2J(JNIEnv * env, jobject obj, jstring p0, jlong p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbOperation_setValue__Ljava_lang_String_2J(JNIEnv *, jobject, jstring, jlong)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_t, ttrait_int, ttrait_char_cp_jutf8null, ttrait_Int64, &NdbOperation::setValue >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbOperation_r, ttrait_char_cp_jutf8null, ttrait_Int64, &NdbApiWrapper::NdbOperation__setValue__2 >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation
 * Method:    setValue
 * Signature: (Ljava/lang/String;F)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_setValue__Ljava_lang_String_2F(JNIEnv * env, jobject obj, jstring p0, jfloat p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbOperation_setValue__Ljava_lang_String_2F(JNIEnv *, jobject, jstring, jfloat)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_t, ttrait_int, ttrait_char_cp_jutf8null, ttrait_float, &NdbOperation::setValue >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbOperation_r, ttrait_char_cp_jutf8null, ttrait_float, &NdbApiWrapper::NdbOperation__setValue__3 >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation
 * Method:    setValue
 * Signature: (Ljava/lang/String;D)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_setValue__Ljava_lang_String_2D(JNIEnv * env, jobject obj, jstring p0, jdouble p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbOperation_setValue__Ljava_lang_String_2D(JNIEnv *, jobject, jstring, jdouble)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_t, ttrait_int, ttrait_char_cp_jutf8null, ttrait_double, &NdbOperation::setValue >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbOperation_r, ttrait_char_cp_jutf8null, ttrait_double, &NdbApiWrapper::NdbOperation__setValue__4 >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation
 * Method:    setValue
 * Signature: (ILjava/nio/ByteBuffer;)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_setValue__ILjava_nio_ByteBuffer_2(JNIEnv * env, jobject obj, jint p0, jobject p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbOperation_setValue__ILjava_nio_ByteBuffer_2(JNIEnv *, jobject, jint, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_t, ttrait_int, ttrait_Uint32, ttrait_char_1cp_bb, &NdbOperation::setValue >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbOperation_r, ttrait_Uint32, ttrait_char_1cp_bb, &NdbApiWrapper::NdbOperation__setValue__5 >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation
 * Method:    setValue
 * Signature: (II)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_setValue__II(JNIEnv * env, jobject obj, jint p0, jint p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbOperation_setValue__II(JNIEnv *, jobject, jint, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_t, ttrait_int, ttrait_Uint32, ttrait_Int32, &NdbOperation::setValue >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbOperation_r, ttrait_Uint32, ttrait_Int32, &NdbApiWrapper::NdbOperation__setValue__6 >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation
 * Method:    setValue
 * Signature: (IJ)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_setValue__IJ(JNIEnv * env, jobject obj, jint p0, jlong p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbOperation_setValue__IJ(JNIEnv *, jobject, jint, jlong)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_t, ttrait_int, ttrait_Uint32, ttrait_Int64, &NdbOperation::setValue >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbOperation_r, ttrait_Uint32, ttrait_Int64, &NdbApiWrapper::NdbOperation__setValue__7 >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation
 * Method:    setValue
 * Signature: (IF)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_setValue__IF(JNIEnv * env, jobject obj, jint p0, jfloat p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbOperation_setValue__IF(JNIEnv *, jobject, jint, jfloat)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_t, ttrait_int, ttrait_Uint32, ttrait_float, &NdbOperation::setValue >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbOperation_r, ttrait_Uint32, ttrait_float, &NdbApiWrapper::NdbOperation__setValue__8 >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation
 * Method:    setValue
 * Signature: (ID)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_setValue__ID(JNIEnv * env, jobject obj, jint p0, jdouble p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbOperation_setValue__ID(JNIEnv *, jobject, jint, jdouble)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_t, ttrait_int, ttrait_Uint32, ttrait_double, &NdbOperation::setValue >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbOperation_r, ttrait_Uint32, ttrait_double, &NdbApiWrapper::NdbOperation__setValue__9 >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation
 * Method:    getBlobHandleM
 * Signature: (Ljava/lang/String;)Lcom/mysql/ndbjtie/ndbapi/NdbBlob;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_getBlobHandleM__Ljava_lang_String_2(JNIEnv * env, jobject obj, jstring p0)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbOperation_getBlobHandleM__Ljava_lang_String_2(JNIEnv *, jobject, jstring)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_CONST_OVERLOADED_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_t, ttrait_c_m_n_n_NdbBlob_p, ttrait_char_cp_jutf8null, &NdbOperation::getBlobHandle >(env, obj, p0); // call of overloaded const/non-const method
#else
    return gcall_fr< ttrait_c_m_n_n_NdbBlob_p, ttrait_c_m_n_n_NdbOperation_r, ttrait_char_cp_jutf8null, &NdbApiWrapper::NdbOperation__getBlobHandle__2 >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_CONST_OVERLOADED_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation
 * Method:    getBlobHandleM
 * Signature: (I)Lcom/mysql/ndbjtie/ndbapi/NdbBlob;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_getBlobHandleM__I(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbOperation_getBlobHandleM__I(JNIEnv *, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_CONST_OVERLOADED_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_t, ttrait_c_m_n_n_NdbBlob_p, ttrait_Uint32, &NdbOperation::getBlobHandle >(env, obj, p0); // call of overloaded const/non-const method
#else
    return gcall_fr< ttrait_c_m_n_n_NdbBlob_p, ttrait_c_m_n_n_NdbOperation_r, ttrait_Uint32, &NdbApiWrapper::NdbOperation__getBlobHandle__3 >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_CONST_OVERLOADED_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation
 * Method:    setAbortOption
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_setAbortOption(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbOperation_setAbortOption(JNIEnv *, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_t, ttrait_int, ttrait_c_m_n_n_NdbOperation_AbortOption_iv/*_enum_*/, &NdbOperation::setAbortOption >(env, obj, p0);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbOperation_r, ttrait_c_m_n_n_NdbOperation_AbortOption_iv/*_enum_*/, &NdbApiWrapper::NdbOperation__setAbortOption >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

// ---------------------------------------------------------------------------

//#include "com_mysql_ndbjtie_ndbapi_NdbOperation_GetValueSpec.h"

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation_GetValueSpec
 * Method:    column
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/ColumnConst;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024GetValueSpec_column__(JNIEnv * env, jobject obj)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024GetValueSpec_column__(JNIEnv *, jobject)");
    return gget< ttrait_c_m_n_n_NdbOperation_GetValueSpec_t, ttrait_c_m_n_n_NdbDictionary_Column_cp, &NdbOperation::GetValueSpec::column >(env, obj);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation_GetValueSpec
 * Method:    recAttr
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbRecAttr;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024GetValueSpec_recAttr__(JNIEnv * env, jobject obj)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024GetValueSpec_recAttr__(JNIEnv *, jobject)");
    return gget< ttrait_c_m_n_n_NdbOperation_GetValueSpec_t, ttrait_c_m_n_n_NdbRecAttr_p, &NdbOperation::GetValueSpec::recAttr >(env, obj);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation_GetValueSpec
 * Method:    column
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/ColumnConst;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024GetValueSpec_column__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024ColumnConst_2(JNIEnv * env, jobject obj, jobject p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024GetValueSpec_column__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024ColumnConst_2(JNIEnv *, jobject, jobject)");
    gset< ttrait_c_m_n_n_NdbOperation_GetValueSpec_t, ttrait_c_m_n_n_NdbDictionary_Column_cp, &NdbOperation::GetValueSpec::column >(env, obj, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation_GetValueSpec
 * Method:    recAttr
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbRecAttr;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024GetValueSpec_recAttr__Lcom_mysql_ndbjtie_ndbapi_NdbRecAttr_2(JNIEnv * env, jobject obj, jobject p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024GetValueSpec_recAttr__Lcom_mysql_ndbjtie_ndbapi_NdbRecAttr_2(JNIEnv *, jobject, jobject)");
    gset< ttrait_c_m_n_n_NdbOperation_GetValueSpec_t, ttrait_c_m_n_n_NdbRecAttr_p, &NdbOperation::GetValueSpec::recAttr >(env, obj, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation_GetValueSpec
 * Method:    create
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbOperation/GetValueSpec;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024GetValueSpec_create(JNIEnv * env, jclass cls)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024GetValueSpec_create(JNIEnv *, jclass)");
    return gcreate< ttrait_c_m_n_n_NdbOperation_GetValueSpec_r >(env, cls);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation_GetValueSpec
 * Method:    delete
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbOperation/GetValueSpec;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024GetValueSpec_delete(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024GetValueSpec_delete(JNIEnv *, jclass, jobject)");
    gdelete< ttrait_c_m_n_n_NdbOperation_GetValueSpec_r >(env, cls, p0);
}

// ---------------------------------------------------------------------------

//#include "com_mysql_ndbjtie_ndbapi_NdbOperation_GetValueSpecArray.h"

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation_GetValueSpecArray
 * Method:    create
 * Signature: (I)Lcom/mysql/ndbjtie/ndbapi/NdbOperation/GetValueSpecArray;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024GetValueSpecArray_create(JNIEnv * env, jclass cls, jint p0)
{
    TRACE("Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024GetValueSpecArray_create(JNIEnv *, jclass, jint)");
    return gcreateArray< ttrait_c_m_n_n_NdbOperation_GetValueSpecArray_r, ttrait_int32 >(env, cls, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation_GetValueSpecArray
 * Method:    delete
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbOperation/GetValueSpecArray;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024GetValueSpecArray_delete(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024GetValueSpecArray_delete(JNIEnv *, jclass, jobject)");
    gdeleteArray< ttrait_c_m_n_n_NdbOperation_GetValueSpecArray_r >(env, cls, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation_GetValueSpecArray
 * Method:    at
 * Signature: (I)Lcom/mysql/ndbjtie/ndbapi/NdbOperation/GetValueSpec;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024GetValueSpecArray_at(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024GetValueSpecArray_at(JNIEnv *, jobject, jint)");
    return gat< ttrait_c_m_n_n_NdbOperation_GetValueSpec_r, ttrait_c_m_n_n_NdbOperation_GetValueSpecArray_r, ttrait_int32 >(env, NULL, obj, p0);
}

// ---------------------------------------------------------------------------

//#include "com_mysql_ndbjtie_ndbapi_NdbOperation_OperationOptions.h"

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation_OperationOptions
 * Method:    size
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024OperationOptions_size(JNIEnv * env, jclass cls)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024OperationOptions_size(JNIEnv *, jclass)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_fr< ttrait_Uint32, &NdbOperation::OperationOptions::size >(env, cls);
#else
    return gcall_fr< ttrait_Uint32, &NdbApiWrapper::NdbOperation__OperationOptions__size >(env, cls);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation_OperationOptions
 * Method:    optionsPresent
 * Signature: ()J
 */
JNIEXPORT jlong JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024OperationOptions_optionsPresent__(JNIEnv * env, jobject obj)
{
    TRACE("jlong Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024OperationOptions_optionsPresent__(JNIEnv *, jobject)");
    return gget< ttrait_c_m_n_n_NdbOperation_OperationOptions_t, ttrait_Uint64, &NdbOperation::OperationOptions::optionsPresent >(env, obj);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation_OperationOptions
 * Method:    abortOption
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024OperationOptions_abortOption__(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024OperationOptions_abortOption__(JNIEnv *, jobject)");
    return gget< ttrait_c_m_n_n_NdbOperation_OperationOptions_t, ttrait_c_m_n_n_NdbOperation_AbortOption_iv/*_enum_*/, &NdbOperation::OperationOptions::abortOption >(env, obj);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation_OperationOptions
 * Method:    extraGetValues
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbOperation/GetValueSpecArray;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024OperationOptions_extraGetValues__(JNIEnv * env, jobject obj)
{
    TRACE("Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024OperationOptions_extraGetValues__(JNIEnv *, jobject)");
    return gget< ttrait_c_m_n_n_NdbOperation_OperationOptions_t, ttrait_c_m_n_n_NdbOperation_GetValueSpecArray_p, &NdbOperation::OperationOptions::extraGetValues >(env, obj);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation_OperationOptions
 * Method:    numExtraGetValues
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024OperationOptions_numExtraGetValues__(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024OperationOptions_numExtraGetValues__(JNIEnv *, jobject)");
    return gget< ttrait_c_m_n_n_NdbOperation_OperationOptions_t, ttrait_Uint32, &NdbOperation::OperationOptions::numExtraGetValues >(env, obj);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation_OperationOptions
 * Method:    extraSetValues
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbOperation/SetValueSpecConstArray;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024OperationOptions_extraSetValues__(JNIEnv * env, jobject obj)
{
    TRACE("Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024OperationOptions_extraSetValues__(JNIEnv *, jobject)");
    return gget< ttrait_c_m_n_n_NdbOperation_OperationOptions_t, ttrait_c_m_n_n_NdbOperation_SetValueSpecArray_cp, &NdbOperation::OperationOptions::extraSetValues >(env, obj);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation_OperationOptions
 * Method:    numExtraSetValues
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024OperationOptions_numExtraSetValues__(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024OperationOptions_numExtraSetValues__(JNIEnv *, jobject)");
    return gget< ttrait_c_m_n_n_NdbOperation_OperationOptions_t, ttrait_Uint32, &NdbOperation::OperationOptions::numExtraSetValues >(env, obj);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation_OperationOptions
 * Method:    partitionId
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024OperationOptions_partitionId__(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024OperationOptions_partitionId__(JNIEnv *, jobject)");
    return gget< ttrait_c_m_n_n_NdbOperation_OperationOptions_t, ttrait_Uint32, &NdbOperation::OperationOptions::partitionId >(env, obj);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation_OperationOptions
 * Method:    interpretedCode
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbInterpretedCodeConst;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024OperationOptions_interpretedCode__(JNIEnv * env, jobject obj)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024OperationOptions_interpretedCode__(JNIEnv *, jobject)");
    return gget< ttrait_c_m_n_n_NdbOperation_OperationOptions_t, ttrait_c_m_n_n_NdbInterpretedCode_cp, &NdbOperation::OperationOptions::interpretedCode >(env, obj);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation_OperationOptions
 * Method:    anyValue
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024OperationOptions_anyValue__(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024OperationOptions_anyValue__(JNIEnv *, jobject)");
    return gget< ttrait_c_m_n_n_NdbOperation_OperationOptions_t, ttrait_Uint32, &NdbOperation::OperationOptions::anyValue >(env, obj);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation_OperationOptions
 * Method:    optionsPresent
 * Signature: (J)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024OperationOptions_optionsPresent__J(JNIEnv * env, jobject obj, jlong p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024OperationOptions_optionsPresent__J(JNIEnv *, jobject, jlong)");
    gset< ttrait_c_m_n_n_NdbOperation_OperationOptions_t, ttrait_Uint64, &NdbOperation::OperationOptions::optionsPresent >(env, obj, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation_OperationOptions
 * Method:    abortOption
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024OperationOptions_abortOption__I(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024OperationOptions_abortOption__I(JNIEnv *, jobject, jint)");
    gset< ttrait_c_m_n_n_NdbOperation_OperationOptions_t, ttrait_c_m_n_n_NdbOperation_AbortOption_iv/*_enum_*/, &NdbOperation::OperationOptions::abortOption >(env, obj, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation_OperationOptions
 * Method:    extraGetValues
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbOperation/GetValueSpecArray;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024OperationOptions_extraGetValues__Lcom_mysql_ndbjtie_ndbapi_NdbOperation_00024GetValueSpecArray_2(JNIEnv * env, jobject obj, jobject p0)
{
    TRACE("Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024OperationOptions_extraGetValues__Lcom_mysql_ndbjtie_ndbapi_NdbOperation_00024GetValueSpecArray_2(JNIEnv *, jobject, jobject)");
    gset< ttrait_c_m_n_n_NdbOperation_OperationOptions_t, ttrait_c_m_n_n_NdbOperation_GetValueSpecArray_p, &NdbOperation::OperationOptions::extraGetValues >(env, obj, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation_OperationOptions
 * Method:    numExtraGetValues
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024OperationOptions_numExtraGetValues__I(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024OperationOptions_numExtraGetValues__I(JNIEnv *, jobject, jint)");
    gset< ttrait_c_m_n_n_NdbOperation_OperationOptions_t, ttrait_Uint32, &NdbOperation::OperationOptions::numExtraGetValues >(env, obj, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation_OperationOptions
 * Method:    extraSetValues
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbOperation/SetValueSpecConstArray;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024OperationOptions_extraSetValues__Lcom_mysql_ndbjtie_ndbapi_NdbOperation_00024SetValueSpecConstArray_2(JNIEnv * env, jobject obj, jobject p0)
{
    TRACE("Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024OperationOptions_extraSetValues__Lcom_mysql_ndbjtie_ndbapi_NdbOperation_00024SetValueSpecConstArray_2(JNIEnv *, jobject, jobject)");
    gset< ttrait_c_m_n_n_NdbOperation_OperationOptions_t, ttrait_c_m_n_n_NdbOperation_SetValueSpecArray_cp, &NdbOperation::OperationOptions::extraSetValues >(env, obj, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation_OperationOptions
 * Method:    numExtraSetValues
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024OperationOptions_numExtraSetValues__I(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024OperationOptions_numExtraSetValues__I(JNIEnv *, jobject, jint)");
    gset< ttrait_c_m_n_n_NdbOperation_OperationOptions_t, ttrait_Uint32, &NdbOperation::OperationOptions::numExtraSetValues >(env, obj, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation_OperationOptions
 * Method:    partitionId
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024OperationOptions_partitionId__I(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024OperationOptions_partitionId__I(JNIEnv *, jobject, jint)");
    gset< ttrait_c_m_n_n_NdbOperation_OperationOptions_t, ttrait_Uint32, &NdbOperation::OperationOptions::partitionId >(env, obj, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation_OperationOptions
 * Method:    interpretedCode
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbInterpretedCodeConst;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024OperationOptions_interpretedCode__Lcom_mysql_ndbjtie_ndbapi_NdbInterpretedCodeConst_2(JNIEnv * env, jobject obj, jobject p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024OperationOptions_interpretedCode__Lcom_mysql_ndbjtie_ndbapi_NdbInterpretedCodeConst_2(JNIEnv *, jobject, jobject)");
    gset< ttrait_c_m_n_n_NdbOperation_OperationOptions_t, ttrait_c_m_n_n_NdbInterpretedCode_cp, &NdbOperation::OperationOptions::interpretedCode >(env, obj, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation_OperationOptions
 * Method:    anyValue
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024OperationOptions_anyValue__I(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024OperationOptions_anyValue__I(JNIEnv *, jobject, jint)");
    gset< ttrait_c_m_n_n_NdbOperation_OperationOptions_t, ttrait_Uint32, &NdbOperation::OperationOptions::anyValue >(env, obj, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation_OperationOptions
 * Method:    create
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbOperation/OperationOptions;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024OperationOptions_create(JNIEnv * env, jclass cls)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024OperationOptions_create(JNIEnv *, jclass)");
    return gcreate< ttrait_c_m_n_n_NdbOperation_OperationOptions_r >(env, cls);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation_OperationOptions
 * Method:    delete
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbOperation/OperationOptions;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024OperationOptions_delete(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024OperationOptions_delete(JNIEnv *, jclass, jobject)");
    gdelete< ttrait_c_m_n_n_NdbOperation_OperationOptions_r >(env, cls, p0);
}

// ---------------------------------------------------------------------------

//#include "com_mysql_ndbjtie_ndbapi_NdbOperation_SetValueSpec.h"

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation_SetValueSpec
 * Method:    column
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/ColumnConst;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024SetValueSpec_column__(JNIEnv * env, jobject obj)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024SetValueSpec_column__(JNIEnv *, jobject)");
    return gget< ttrait_c_m_n_n_NdbOperation_SetValueSpec_t, ttrait_c_m_n_n_NdbDictionary_Column_cp, &NdbOperation::SetValueSpec::column >(env, obj);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation_SetValueSpec
 * Method:    column
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/ColumnConst;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024SetValueSpec_column__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024ColumnConst_2(JNIEnv * env, jobject obj, jobject p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024SetValueSpec_column__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_00024ColumnConst_2(JNIEnv *, jobject, jobject)");
    gset< ttrait_c_m_n_n_NdbOperation_SetValueSpec_t, ttrait_c_m_n_n_NdbDictionary_Column_cp, &NdbOperation::SetValueSpec::column >(env, obj, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation_SetValueSpec
 * Method:    create
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbOperation/SetValueSpec;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024SetValueSpec_create(JNIEnv * env, jclass cls)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024SetValueSpec_create(JNIEnv *, jclass)");
    return gcreate< ttrait_c_m_n_n_NdbOperation_SetValueSpec_r >(env, cls);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation_SetValueSpec
 * Method:    delete
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbOperation/SetValueSpec;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024SetValueSpec_delete(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024SetValueSpec_delete(JNIEnv *, jclass, jobject)");
    gdelete< ttrait_c_m_n_n_NdbOperation_SetValueSpec_r >(env, cls, p0);
}

// ---------------------------------------------------------------------------

//#include "com_mysql_ndbjtie_ndbapi_NdbOperation_SetValueSpecArray.h"

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation_SetValueSpecArray
 * Method:    create
 * Signature: (I)Lcom/mysql/ndbjtie/ndbapi/NdbOperation/SetValueSpecArray;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024SetValueSpecArray_create(JNIEnv * env, jclass cls, jint p0)
{
    TRACE("Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024SetValueSpecArray_create(JNIEnv *, jclass, jint)");
    return gcreateArray< ttrait_c_m_n_n_NdbOperation_SetValueSpecArray_r, ttrait_int32 >(env, cls, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation_SetValueSpecArray
 * Method:    delete
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbOperation/SetValueSpecArray;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024SetValueSpecArray_delete(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024SetValueSpecArray_delete(JNIEnv *, jclass, jobject)");
    gdeleteArray< ttrait_c_m_n_n_NdbOperation_SetValueSpecArray_r >(env, cls, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation_SetValueSpecArray
 * Method:    at
 * Signature: (I)Lcom/mysql/ndbjtie/ndbapi/NdbOperation/SetValueSpec;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024SetValueSpecArray_at(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("Java_com_mysql_ndbjtie_ndbapi_NdbOperation_00024SetValueSpecArray_at(JNIEnv *, jobject, jint)");
    return gat< ttrait_c_m_n_n_NdbOperation_SetValueSpec_r, ttrait_c_m_n_n_NdbOperation_SetValueSpecArray_r, ttrait_int32 >(env, NULL, obj, p0);
}

// ---------------------------------------------------------------------------

//#include "com_mysql_ndbjtie_ndbapi_NdbRecAttr.h"

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbRecAttr
 * Method:    getColumn
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/ColumnConst;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbRecAttr_getColumn(JNIEnv * env, jobject obj)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbRecAttr_getColumn(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbRecAttr_ct, ttrait_c_m_n_n_NdbDictionary_Column_cp, &NdbRecAttr::getColumn >(env, obj);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbDictionary_Column_cp, ttrait_c_m_n_n_NdbRecAttr_cr, &NdbApiWrapper::NdbRecAttr__getColumn >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbRecAttr
 * Method:    getType
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbRecAttr_getType(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbRecAttr_getType(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbRecAttr_ct, ttrait_c_m_n_n_NdbDictionary_Column_Type_iv/*_enum_*/, &NdbRecAttr::getType >(env, obj);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbDictionary_Column_Type_iv/*_enum_*/, ttrait_c_m_n_n_NdbRecAttr_cr, &NdbApiWrapper::NdbRecAttr__getType >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbRecAttr
 * Method:    get_size_in_bytes
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbRecAttr_get_1size_1in_1bytes(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbRecAttr_get_1size_1in_1bytes(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbRecAttr_ct, ttrait_Uint32, &NdbRecAttr::get_size_in_bytes >(env, obj);
#else
    return gcall_fr< ttrait_Uint32, ttrait_c_m_n_n_NdbRecAttr_cr, &NdbApiWrapper::NdbRecAttr__get_size_in_bytes >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbRecAttr
 * Method:    isNULL
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbRecAttr_isNULL(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbRecAttr_isNULL(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbRecAttr_ct, ttrait_int, &NdbRecAttr::isNULL >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbRecAttr_cr, &NdbApiWrapper::NdbRecAttr__isNULL >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbRecAttr
 * Method:    int64_value
 * Signature: ()J
 */
JNIEXPORT jlong JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbRecAttr_int64_1value(JNIEnv * env, jobject obj)
{
    TRACE("jlong Java_com_mysql_ndbjtie_ndbapi_NdbRecAttr_Int64_1value(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbRecAttr_ct, ttrait_Int64, &NdbRecAttr::int64_value >(env, obj);
#else
    return gcall_fr< ttrait_Int64, ttrait_c_m_n_n_NdbRecAttr_cr, &NdbApiWrapper::NdbRecAttr__int64_value >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbRecAttr
 * Method:    int32_value
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbRecAttr_int32_1value(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbRecAttr_int32_1value(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbRecAttr_ct, ttrait_Int32, &NdbRecAttr::int32_value >(env, obj);
#else
    return gcall_fr< ttrait_Int32, ttrait_c_m_n_n_NdbRecAttr_cr, &NdbApiWrapper::NdbRecAttr__int32_value >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbRecAttr
 * Method:    medium_value
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbRecAttr_medium_1value(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbRecAttr_medium_1value(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbRecAttr_ct, ttrait_Int32, &NdbRecAttr::medium_value >(env, obj);
#else
    return gcall_fr< ttrait_Int32, ttrait_c_m_n_n_NdbRecAttr_cr, &NdbApiWrapper::NdbRecAttr__medium_value >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbRecAttr
 * Method:    short_value
 * Signature: ()S
 */
JNIEXPORT jshort JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbRecAttr_short_1value(JNIEnv * env, jobject obj)
{
    TRACE("jshort Java_com_mysql_ndbjtie_ndbapi_NdbRecAttr_short_1value(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbRecAttr_ct, ttrait_short, &NdbRecAttr::short_value >(env, obj);
#else
    return gcall_fr< ttrait_short, ttrait_c_m_n_n_NdbRecAttr_cr, &NdbApiWrapper::NdbRecAttr__short_value >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbRecAttr
 * Method:    char_value
 * Signature: ()B
 */
JNIEXPORT jbyte JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbRecAttr_char_1value(JNIEnv * env, jobject obj)
{
    TRACE("jchar Java_com_mysql_ndbjtie_ndbapi_NdbRecAttr_char_1value(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbRecAttr_ct, ttrait_char, &NdbRecAttr::char_value >(env, obj);
#else
    return gcall_fr< ttrait_char, ttrait_c_m_n_n_NdbRecAttr_cr, &NdbApiWrapper::NdbRecAttr__char_value >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbRecAttr
 * Method:    int8_value
 * Signature: ()B
 */
JNIEXPORT jbyte JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbRecAttr_int8_1value(JNIEnv * env, jobject obj)
{
    TRACE("jbyte Java_com_mysql_ndbjtie_ndbapi_NdbRecAttr_int8_1value(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbRecAttr_ct, ttrait_Int8, &NdbRecAttr::int8_value >(env, obj);
#else
    return gcall_fr< ttrait_Int8, ttrait_c_m_n_n_NdbRecAttr_cr, &NdbApiWrapper::NdbRecAttr__int8_value >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbRecAttr
 * Method:    u_64_value
 * Signature: ()J
 */
JNIEXPORT jlong JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbRecAttr_u_164_1value(JNIEnv * env, jobject obj)
{
    TRACE("jlong Java_com_mysql_ndbjtie_ndbapi_NdbRecAttr_u_164_1value(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbRecAttr_ct, ttrait_Uint64, &NdbRecAttr::u_64_value >(env, obj);
#else
    return gcall_fr< ttrait_Uint64, ttrait_c_m_n_n_NdbRecAttr_cr, &NdbApiWrapper::NdbRecAttr__u_64_value >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbRecAttr
 * Method:    u_32_value
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbRecAttr_u_132_1value(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbRecAttr_u_132_1value(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbRecAttr_ct, ttrait_Uint32, &NdbRecAttr::u_32_value >(env, obj);
#else
    return gcall_fr< ttrait_Uint32, ttrait_c_m_n_n_NdbRecAttr_cr, &NdbApiWrapper::NdbRecAttr__u_32_value >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbRecAttr
 * Method:    u_medium_value
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbRecAttr_u_1medium_1value(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbRecAttr_u_1medium_1value(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbRecAttr_ct, ttrait_Uint32, &NdbRecAttr::u_medium_value >(env, obj);
#else
    return gcall_fr< ttrait_Uint32, ttrait_c_m_n_n_NdbRecAttr_cr, &NdbApiWrapper::NdbRecAttr__u_medium_value >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbRecAttr
 * Method:    u_short_value
 * Signature: ()S
 */
JNIEXPORT jshort JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbRecAttr_u_1short_1value(JNIEnv * env, jobject obj)
{
    TRACE("jshort Java_com_mysql_ndbjtie_ndbapi_NdbRecAttr_u_1short_1value(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbRecAttr_ct, ttrait_Uint16, &NdbRecAttr::u_short_value >(env, obj);
#else
    return gcall_fr< ttrait_Uint16, ttrait_c_m_n_n_NdbRecAttr_cr, &NdbApiWrapper::NdbRecAttr__u_short_value >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbRecAttr
 * Method:    u_char_value
 * Signature: ()B
 */
JNIEXPORT jbyte JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbRecAttr_u_1char_1value(JNIEnv * env, jobject obj)
{
    TRACE("jbyte Java_com_mysql_ndbjtie_ndbapi_NdbRecAttr_u_1char_1value(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbRecAttr_ct, ttrait_Uint8, &NdbRecAttr::u_char_value >(env, obj);
#else
    return gcall_fr< ttrait_Uint8, ttrait_c_m_n_n_NdbRecAttr_cr, &NdbApiWrapper::NdbRecAttr__u_char_value >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbRecAttr
 * Method:    u_8_value
 * Signature: ()B
 */
JNIEXPORT jbyte JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbRecAttr_u_18_1value(JNIEnv * env, jobject obj)
{
    TRACE("jbyte Java_com_mysql_ndbjtie_ndbapi_NdbRecAttr_u_18_1value(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbRecAttr_ct, ttrait_Uint8, &NdbRecAttr::u_8_value >(env, obj);
#else
    return gcall_fr< ttrait_Uint8, ttrait_c_m_n_n_NdbRecAttr_cr, &NdbApiWrapper::NdbRecAttr__u_8_value >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbRecAttr
 * Method:    float_value
 * Signature: ()F
 */
JNIEXPORT jfloat JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbRecAttr_float_1value(JNIEnv * env, jobject obj)
{
    TRACE("jfloat Java_com_mysql_ndbjtie_ndbapi_NdbRecAttr_float_1value(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbRecAttr_ct, ttrait_float, &NdbRecAttr::float_value >(env, obj);
#else
    return gcall_fr< ttrait_float, ttrait_c_m_n_n_NdbRecAttr_cr, &NdbApiWrapper::NdbRecAttr__float_value >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbRecAttr
 * Method:    double_value
 * Signature: ()D
 */
JNIEXPORT jdouble JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbRecAttr_double_1value(JNIEnv * env, jobject obj)
{
    TRACE("jdouble Java_com_mysql_ndbjtie_ndbapi_NdbRecAttr_double_1value(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbRecAttr_ct, ttrait_double, &NdbRecAttr::double_value >(env, obj);
#else
    return gcall_fr< ttrait_double, ttrait_c_m_n_n_NdbRecAttr_cr, &NdbApiWrapper::NdbRecAttr__double_value >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbRecAttr
 * Method:    cloneNative
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbRecAttr;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbRecAttr_cloneNative(JNIEnv * env, jobject obj)
{
    TRACE("Java_com_mysql_ndbjtie_ndbapi_NdbRecAttr_cloneNative(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbRecAttr_ct, ttrait_c_m_n_n_NdbRecAttr_p, &NdbRecAttr::clone >(env, obj);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbRecAttr_p, ttrait_c_m_n_n_NdbRecAttr_cr, &NdbApiWrapper::NdbRecAttr__clone >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbRecAttr
 * Method:    delete
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbRecAttr;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbRecAttr_delete(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbRecAttr_delete(JNIEnv *, jclass, jobject)");
    gdelete< ttrait_c_m_n_n_NdbRecAttr_r >(env, cls, p0);
}

// ---------------------------------------------------------------------------

//#include "com_mysql_ndbjtie_ndbapi_NdbScanFilter.h"

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanFilter
 * Method:    getNdbError
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbErrorConst;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanFilter_getNdbError(JNIEnv * env, jobject obj)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbScanFilter_getNdbError(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbScanFilter_ct, ttrait_c_m_n_n_NdbError_cr, &NdbScanFilter::getNdbError >(env, obj);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbError_cr, ttrait_c_m_n_n_NdbScanFilter_cr, &NdbApiWrapper::NdbScanFilter__getNdbError >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanFilter
 * Method:    getInterpretedCode
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbInterpretedCodeConst;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanFilter_getInterpretedCode(JNIEnv * env, jobject obj)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbScanFilter_getInterpretedCode(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbScanFilter_ct, ttrait_c_m_n_n_NdbInterpretedCode_cp, &NdbScanFilter::getInterpretedCode >(env, obj);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbInterpretedCode_cp, ttrait_c_m_n_n_NdbScanFilter_cr, &NdbApiWrapper::NdbScanFilter__getInterpretedCode >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanFilter
 * Method:    getNdbOperation
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbOperation;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanFilter_getNdbOperation(JNIEnv * env, jobject obj)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbScanFilter_getNdbOperation(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbScanFilter_ct, ttrait_c_m_n_n_NdbOperation_p, &NdbScanFilter::getNdbOperation >(env, obj);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbOperation_p, ttrait_c_m_n_n_NdbScanFilter_cr, &NdbApiWrapper::NdbScanFilter__getNdbOperation >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanFilter
 * Method:    create
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbInterpretedCode;)Lcom/mysql/ndbjtie/ndbapi/NdbScanFilter;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanFilter_create__Lcom_mysql_ndbjtie_ndbapi_NdbInterpretedCode_2(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbScanFilter_create__Lcom_mysql_ndbjtie_ndbapi_NdbInterpretedCode_2(JNIEnv *, jclass, jobject)");
    return gcreate< ttrait_c_m_n_n_NdbScanFilter_r, ttrait_c_m_n_n_NdbInterpretedCode_p >(env, cls, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanFilter
 * Method:    create
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbOperation;)Lcom/mysql/ndbjtie/ndbapi/NdbScanFilter;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanFilter_create__Lcom_mysql_ndbjtie_ndbapi_NdbOperation_2(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbScanFilter_create__Lcom_mysql_ndbjtie_ndbapi_NdbOperation_2(JNIEnv *, jclass, jobject)");
    return gcreate< ttrait_c_m_n_n_NdbScanFilter_r, ttrait_c_m_n_n_NdbOperation_p >(env, cls, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanFilter
 * Method:    delete
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbScanFilter;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanFilter_delete(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbScanFilter_delete(JNIEnv *, jclass, jobject)");
    gdelete< ttrait_c_m_n_n_NdbScanFilter_r >(env, cls, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanFilter
 * Method:    begin
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanFilter_begin(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbScanFilter_begin(JNIEnv *, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbScanFilter_t, ttrait_int, ttrait_c_m_n_n_NdbScanFilter_Group_iv/*_enum_*/, &NdbScanFilter::begin >(env, obj, p0);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbScanFilter_r, ttrait_c_m_n_n_NdbScanFilter_Group_iv/*_enum_*/, &NdbApiWrapper::NdbScanFilter__begin >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanFilter
 * Method:    end
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanFilter_end(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbScanFilter_end(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbScanFilter_t, ttrait_int, &NdbScanFilter::end >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbScanFilter_r, &NdbApiWrapper::NdbScanFilter__end >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanFilter
 * Method:    istrue
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanFilter_istrue(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbScanFilter_istrue(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbScanFilter_t, ttrait_int, &NdbScanFilter::istrue >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbScanFilter_r, &NdbApiWrapper::NdbScanFilter__istrue >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanFilter
 * Method:    isfalse
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanFilter_isfalse(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbScanFilter_isfalse(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbScanFilter_t, ttrait_int, &NdbScanFilter::isfalse >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbScanFilter_r, &NdbApiWrapper::NdbScanFilter__isfalse >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanFilter
 * Method:    cmp
 * Signature: (IILjava/nio/ByteBuffer;I)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanFilter_cmp(JNIEnv * env, jobject obj, jint p0, jint p1, jobject p2, jint p3)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbScanFilter_cmp(JNIEnv *, jobject, jint, jint, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbScanFilter_t, ttrait_int, ttrait_c_m_n_n_NdbScanFilter_BinaryCondition_iv/*_enum_*/, ttrait_int, ttrait_void_1cp_bb, ttrait_Uint32, &NdbScanFilter::cmp >(env, obj, p0, p1, p2, p3);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbScanFilter_r, ttrait_c_m_n_n_NdbScanFilter_BinaryCondition_iv/*_enum_*/, ttrait_int, ttrait_void_1cp_bb, ttrait_Uint32, &NdbApiWrapper::NdbScanFilter__cmp >(env, NULL, obj, p0, p1, p2, p3);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanFilter
 * Method:    eq
 * Signature: (II)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanFilter_eq__II(JNIEnv * env, jobject obj, jint p0, jint p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbScanFilter_eq__II(JNIEnv *, jobject, jint, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbScanFilter_t, ttrait_int, ttrait_int, ttrait_Uint32, &NdbScanFilter::eq >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbScanFilter_r, ttrait_int, ttrait_Uint32, &NdbApiWrapper::NdbScanFilter__eq__0 >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanFilter
 * Method:    ne
 * Signature: (II)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanFilter_ne__II(JNIEnv * env, jobject obj, jint p0, jint p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbScanFilter_ne__II(JNIEnv *, jobject, jint, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbScanFilter_t, ttrait_int, ttrait_int, ttrait_Uint32, &NdbScanFilter::ne >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbScanFilter_r, ttrait_int, ttrait_Uint32, &NdbApiWrapper::NdbScanFilter__ne__0 >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanFilter
 * Method:    lt
 * Signature: (II)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanFilter_lt__II(JNIEnv * env, jobject obj, jint p0, jint p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbScanFilter_lt__II(JNIEnv *, jobject, jint, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbScanFilter_t, ttrait_int, ttrait_int, ttrait_Uint32, &NdbScanFilter::lt >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbScanFilter_r, ttrait_int, ttrait_Uint32, &NdbApiWrapper::NdbScanFilter__lt__0 >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanFilter
 * Method:    le
 * Signature: (II)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanFilter_le__II(JNIEnv * env, jobject obj, jint p0, jint p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbScanFilter_le__II(JNIEnv *, jobject, jint, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbScanFilter_t, ttrait_int, ttrait_int, ttrait_Uint32, &NdbScanFilter::le >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbScanFilter_r, ttrait_int, ttrait_Uint32, &NdbApiWrapper::NdbScanFilter__le__0 >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanFilter
 * Method:    gt
 * Signature: (II)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanFilter_gt__II(JNIEnv * env, jobject obj, jint p0, jint p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbScanFilter_gt__II(JNIEnv *, jobject, jint, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbScanFilter_t, ttrait_int, ttrait_int, ttrait_Uint32, &NdbScanFilter::gt >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbScanFilter_r, ttrait_int, ttrait_Uint32, &NdbApiWrapper::NdbScanFilter__gt__0 >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanFilter
 * Method:    ge
 * Signature: (II)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanFilter_ge__II(JNIEnv * env, jobject obj, jint p0, jint p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbScanFilter_ge__II(JNIEnv *, jobject, jint, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbScanFilter_t, ttrait_int, ttrait_int, ttrait_Uint32, &NdbScanFilter::ge >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbScanFilter_r, ttrait_int, ttrait_Uint32, &NdbApiWrapper::NdbScanFilter__ge__0 >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanFilter
 * Method:    eq
 * Signature: (IJ)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanFilter_eq__IJ(JNIEnv * env, jobject obj, jint p0, jlong p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbScanFilter_eq__IJ(JNIEnv *, jobject, jint, jlong)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbScanFilter_t, ttrait_int, ttrait_int, ttrait_Uint64, &NdbScanFilter::eq >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbScanFilter_r, ttrait_int, ttrait_Uint64, &NdbApiWrapper::NdbScanFilter__eq__1 >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanFilter
 * Method:    ne
 * Signature: (IJ)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanFilter_ne__IJ(JNIEnv * env, jobject obj, jint p0, jlong p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbScanFilter_ne__IJ(JNIEnv *, jobject, jint, jlong)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbScanFilter_t, ttrait_int, ttrait_int, ttrait_Uint64, &NdbScanFilter::ne >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbScanFilter_r, ttrait_int, ttrait_Uint64, &NdbApiWrapper::NdbScanFilter__ne__1 >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanFilter
 * Method:    lt
 * Signature: (IJ)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanFilter_lt__IJ(JNIEnv * env, jobject obj, jint p0, jlong p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbScanFilter_lt__IJ(JNIEnv *, jobject, jint, jlong)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbScanFilter_t, ttrait_int, ttrait_int, ttrait_Uint64, &NdbScanFilter::lt >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbScanFilter_r, ttrait_int, ttrait_Uint64, &NdbApiWrapper::NdbScanFilter__lt__1 >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanFilter
 * Method:    le
 * Signature: (IJ)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanFilter_le__IJ(JNIEnv * env, jobject obj, jint p0, jlong p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbScanFilter_le__IJ(JNIEnv *, jobject, jint, jlong)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbScanFilter_t, ttrait_int, ttrait_int, ttrait_Uint64, &NdbScanFilter::le >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbScanFilter_r, ttrait_int, ttrait_Uint64, &NdbApiWrapper::NdbScanFilter__le__1 >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanFilter
 * Method:    gt
 * Signature: (IJ)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanFilter_gt__IJ(JNIEnv * env, jobject obj, jint p0, jlong p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbScanFilter_gt__IJ(JNIEnv *, jobject, jint, jlong)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbScanFilter_t, ttrait_int, ttrait_int, ttrait_Uint64, &NdbScanFilter::gt >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbScanFilter_r, ttrait_int, ttrait_Uint64, &NdbApiWrapper::NdbScanFilter__gt__1 >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanFilter
 * Method:    ge
 * Signature: (IJ)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanFilter_ge__IJ(JNIEnv * env, jobject obj, jint p0, jlong p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbScanFilter_ge__IJ(JNIEnv *, jobject, jint, jlong)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbScanFilter_t, ttrait_int, ttrait_int, ttrait_Uint64, &NdbScanFilter::ge >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbScanFilter_r, ttrait_int, ttrait_Uint64, &NdbApiWrapper::NdbScanFilter__ge__1 >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanFilter
 * Method:    isnull
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanFilter_isnull(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbScanFilter_isnull(JNIEnv *, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbScanFilter_t, ttrait_int, ttrait_int, &NdbScanFilter::isnull >(env, obj, p0);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbScanFilter_r, ttrait_int, &NdbApiWrapper::NdbScanFilter__isnull >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanFilter
 * Method:    isnotnull
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanFilter_isnotnull(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbScanFilter_isnotnull(JNIEnv *, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbScanFilter_t, ttrait_int, ttrait_int, &NdbScanFilter::isnotnull >(env, obj, p0);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbScanFilter_r, ttrait_int, &NdbApiWrapper::NdbScanFilter__isnotnull >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

// ---------------------------------------------------------------------------

//#include "com_mysql_ndbjtie_ndbapi_NdbScanOperation.h"

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanOperation
 * Method:    getNdbTransaction
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbTransaction;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_getNdbTransaction(JNIEnv * env, jobject obj)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_getNdbTransaction(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbScanOperation_ct, ttrait_c_m_n_n_NdbTransaction_p, &NdbScanOperation::getNdbTransaction >(env, obj);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbTransaction_p, ttrait_c_m_n_n_NdbScanOperation_cr, &NdbApiWrapper::NdbScanOperation__getNdbTransaction >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanOperation
 * Method:    readTuples
 * Signature: (IIII)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_readTuples(JNIEnv * env, jobject obj, jint p0, jint p1, jint p2, jint p3)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_readTuples(JNIEnv *, jobject, jint, jint, jint, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbScanOperation_t, ttrait_int, ttrait_c_m_n_n_NdbOperation_LockMode_iv/*_enum_*/, ttrait_Uint32, ttrait_Uint32, ttrait_Uint32, &NdbScanOperation::readTuples >(env, obj, p0, p1, p2, p3);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbScanOperation_r, ttrait_c_m_n_n_NdbOperation_LockMode_iv/*_enum_*/, ttrait_Uint32, ttrait_Uint32, ttrait_Uint32, &NdbApiWrapper::NdbScanOperation__readTuples >(env, NULL, obj, p0, p1, p2, p3);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanOperation
 * Method:    nextResult
 * Signature: (ZZ)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_nextResult(JNIEnv * env, jobject obj, jboolean p0, jboolean p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_nextResult(JNIEnv *, jobject, jboolean, jboolean)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbScanOperation_t, ttrait_int, ttrait_bool, ttrait_bool, &NdbScanOperation::nextResult >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbScanOperation_r, ttrait_bool, ttrait_bool, &NdbApiWrapper::NdbScanOperation__nextResult >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanOperation
 * Method:    nextResultCopyOut
 * Signature: (Ljava/nio/ByteBuffer;ZZ)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_nextResultCopyOut(JNIEnv * env, jobject obj, jobject p0, jboolean p1, jboolean p2)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_nextResultCopyOut(JNIEnv *, jobject, jobject, jboolean, jboolean)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbScanOperation_t, ttrait_int, ttrait_char_1p_bb, ttrait_bool, ttrait_bool, &NdbScanOperation::nextResultCopyOut >(env, obj, p0, p1, p2);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbScanOperation_r, ttrait_char_1p_bb, ttrait_bool, ttrait_bool, &NdbApiWrapper::NdbScanOperation__nextResultCopyOut >(env, NULL, obj, p0, p1, p2);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanOperation
 * Method:    close
 * Signature: (ZZ)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_close(JNIEnv * env, jobject obj, jboolean p0, jboolean p1)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_close(JNIEnv *, jobject, jboolean, jboolean)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbScanOperation_t, ttrait_bool, ttrait_bool, &NdbScanOperation::close >(env, obj, p0, p1);
#else
    gcall_fv< ttrait_c_m_n_n_NdbScanOperation_r, ttrait_bool, ttrait_bool, &NdbApiWrapper::NdbScanOperation__close >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanOperation
 * Method:    lockCurrentTuple
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbOperation;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_lockCurrentTuple__(JNIEnv * env, jobject obj)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_lockCurrentTuple__(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbScanOperation_t, ttrait_c_m_n_n_NdbOperation_p, &NdbScanOperation::lockCurrentTuple >(env, obj);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbOperation_p, ttrait_c_m_n_n_NdbScanOperation_r, &NdbApiWrapper::NdbScanOperation__lockCurrentTuple__0 >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanOperation
 * Method:    lockCurrentTuple
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbTransaction;)Lcom/mysql/ndbjtie/ndbapi/NdbOperation;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_lockCurrentTuple__Lcom_mysql_ndbjtie_ndbapi_NdbTransaction_2(JNIEnv * env, jobject obj, jobject p0)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_lockCurrentTuple__Lcom_mysql_ndbjtie_ndbapi_NdbTransaction_2(JNIEnv *, jobject, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbScanOperation_t, ttrait_c_m_n_n_NdbOperation_p, ttrait_c_m_n_n_NdbTransaction_p, &NdbScanOperation::lockCurrentTuple >(env, obj, p0);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbOperation_p, ttrait_c_m_n_n_NdbScanOperation_r, ttrait_c_m_n_n_NdbTransaction_p, &NdbApiWrapper::NdbScanOperation__lockCurrentTuple__1 >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanOperation
 * Method:    updateCurrentTuple
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbOperation;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_updateCurrentTuple__(JNIEnv * env, jobject obj)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_updateCurrentTuple__(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbScanOperation_t, ttrait_c_m_n_n_NdbOperation_p, &NdbScanOperation::updateCurrentTuple >(env, obj);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbOperation_p, ttrait_c_m_n_n_NdbScanOperation_r, &NdbApiWrapper::NdbScanOperation__updateCurrentTuple__0 >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanOperation
 * Method:    updateCurrentTuple
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbTransaction;)Lcom/mysql/ndbjtie/ndbapi/NdbOperation;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_updateCurrentTuple__Lcom_mysql_ndbjtie_ndbapi_NdbTransaction_2(JNIEnv * env, jobject obj, jobject p0)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_updateCurrentTuple__Lcom_mysql_ndbjtie_ndbapi_NdbTransaction_2(JNIEnv *, jobject, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbScanOperation_t, ttrait_c_m_n_n_NdbOperation_p, ttrait_c_m_n_n_NdbTransaction_p, &NdbScanOperation::updateCurrentTuple >(env, obj, p0);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbOperation_p, ttrait_c_m_n_n_NdbScanOperation_r, ttrait_c_m_n_n_NdbTransaction_p, &NdbApiWrapper::NdbScanOperation__updateCurrentTuple__1 >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanOperation
 * Method:    deleteCurrentTuple
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_deleteCurrentTuple__(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_deleteCurrentTuple__(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbScanOperation_t, ttrait_int, &NdbScanOperation::deleteCurrentTuple >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbScanOperation_r, &NdbApiWrapper::NdbScanOperation__deleteCurrentTuple__0 >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanOperation
 * Method:    deleteCurrentTuple
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbTransaction;)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_deleteCurrentTuple__Lcom_mysql_ndbjtie_ndbapi_NdbTransaction_2(JNIEnv * env, jobject obj, jobject p0)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_deleteCurrentTuple__Lcom_mysql_ndbjtie_ndbapi_NdbTransaction_2(JNIEnv *, jobject, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbScanOperation_t, ttrait_int, ttrait_c_m_n_n_NdbTransaction_p, &NdbScanOperation::deleteCurrentTuple >(env, obj, p0);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbScanOperation_r, ttrait_c_m_n_n_NdbTransaction_p, &NdbApiWrapper::NdbScanOperation__deleteCurrentTuple__1 >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanOperation
 * Method:    lockCurrentTuple
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbTransaction;Lcom/mysql/ndbjtie/ndbapi/NdbRecordConst;Ljava/nio/ByteBuffer;[BLcom/mysql/ndbjtie/ndbapi/NdbOperation/OperationOptionsConst;I)Lcom/mysql/ndbjtie/ndbapi/NdbOperationConst;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_lockCurrentTuple__Lcom_mysql_ndbjtie_ndbapi_NdbTransaction_2Lcom_mysql_ndbjtie_ndbapi_NdbRecordConst_2Ljava_nio_ByteBuffer_2_3BLcom_mysql_ndbjtie_ndbapi_NdbOperation_00024OperationOptionsConst_2I(JNIEnv * env, jobject obj, jobject p0, jobject p1, jobject p2, jbyteArray p3, jobject p4, jint p5)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_lockCurrentTuple__Lcom_mysql_ndbjtie_ndbapi_NdbTransaction_2Lcom_mysql_ndbjtie_ndbapi_NdbRecordConst_2Ljava_nio_ByteBuffer_2_3BLcom_mysql_ndbjtie_ndbapi_NdbOperation_00024OperationOptionsConst_2I(JNIEnv *, jobject, jobject, jobject, jobject, jbyteArray, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbScanOperation_t, ttrait_c_m_n_n_NdbOperation_cp, ttrait_c_m_n_n_NdbTransaction_p, ttrait_c_m_n_n_NdbRecord_cp, ttrait_char_1p_bb, ttrait_Uint8_0cp_a, ttrait_c_m_n_n_NdbOperation_OperationOptions_cp, ttrait_Uint32, &NdbScanOperation::lockCurrentTuple >(env, obj, p0, p1, p2, p3, p4, p5);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbOperation_cp, ttrait_c_m_n_n_NdbScanOperation_r, ttrait_c_m_n_n_NdbTransaction_p, ttrait_c_m_n_n_NdbRecord_cp, ttrait_char_1p_bb, ttrait_Uint8_0cp_a, ttrait_c_m_n_n_NdbOperation_OperationOptions_cp, ttrait_Uint32, &NdbApiWrapper::NdbScanOperation__lockCurrentTuple >(env, NULL, obj, p0, p1, p2, p3, p4, p5);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanOperation
 * Method:    updateCurrentTuple
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbTransaction;Lcom/mysql/ndbjtie/ndbapi/NdbRecordConst;Ljava/lang/String;[BLcom/mysql/ndbjtie/ndbapi/NdbOperation/OperationOptionsConst;I)Lcom/mysql/ndbjtie/ndbapi/NdbOperationConst;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_updateCurrentTuple__Lcom_mysql_ndbjtie_ndbapi_NdbTransaction_2Lcom_mysql_ndbjtie_ndbapi_NdbRecordConst_2Ljava_lang_String_2_3BLcom_mysql_ndbjtie_ndbapi_NdbOperation_00024OperationOptionsConst_2I(JNIEnv * env, jobject obj, jobject p0, jobject p1, jstring p2, jbyteArray p3, jobject p4, jint p5)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_updateCurrentTuple__Lcom_mysql_ndbjtie_ndbapi_NdbTransaction_2Lcom_mysql_ndbjtie_ndbapi_NdbRecordConst_2Ljava_lang_String_2_3BLcom_mysql_ndbjtie_ndbapi_NdbOperation_00024OperationOptionsConst_2I(JNIEnv *, jobject, jobject, jobject, jstring, jbyteArray, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbScanOperation_t, ttrait_c_m_n_n_NdbOperation_cp, ttrait_c_m_n_n_NdbTransaction_p, ttrait_c_m_n_n_NdbRecord_cp, ttrait_char_cp_jutf8null, ttrait_Uint8_0cp_a, ttrait_c_m_n_n_NdbOperation_OperationOptions_cp, ttrait_Uint32, &NdbScanOperation::updateCurrentTuple >(env, obj, p0, p1, p2, p3, p4, p5);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbOperation_cp, ttrait_c_m_n_n_NdbScanOperation_r, ttrait_c_m_n_n_NdbTransaction_p, ttrait_c_m_n_n_NdbRecord_cp, ttrait_char_cp_jutf8null, ttrait_Uint8_0cp_a, ttrait_c_m_n_n_NdbOperation_OperationOptions_cp, ttrait_Uint32, &NdbApiWrapper::NdbScanOperation__updateCurrentTuple >(env, NULL, obj, p0, p1, p2, p3, p4, p5);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanOperation
 * Method:    deleteCurrentTuple
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbTransaction;Lcom/mysql/ndbjtie/ndbapi/NdbRecordConst;Ljava/nio/ByteBuffer;[BLcom/mysql/ndbjtie/ndbapi/NdbOperation/OperationOptionsConst;I)Lcom/mysql/ndbjtie/ndbapi/NdbOperationConst;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_deleteCurrentTuple__Lcom_mysql_ndbjtie_ndbapi_NdbTransaction_2Lcom_mysql_ndbjtie_ndbapi_NdbRecordConst_2Ljava_nio_ByteBuffer_2_3BLcom_mysql_ndbjtie_ndbapi_NdbOperation_00024OperationOptionsConst_2I(JNIEnv * env, jobject obj, jobject p0, jobject p1, jobject p2, jbyteArray p3, jobject p4, jint p5)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_deleteCurrentTuple__Lcom_mysql_ndbjtie_ndbapi_NdbTransaction_2Lcom_mysql_ndbjtie_ndbapi_NdbRecordConst_2Ljava_nio_ByteBuffer_2_3BLcom_mysql_ndbjtie_ndbapi_NdbOperation_00024OperationOptionsConst_2I(JNIEnv *, jobject, jobject, jobject, jobject, jbyteArray, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbScanOperation_t, ttrait_c_m_n_n_NdbOperation_cp, ttrait_c_m_n_n_NdbTransaction_p, ttrait_c_m_n_n_NdbRecord_cp, ttrait_char_1p_bb, ttrait_Uint8_0cp_a, ttrait_c_m_n_n_NdbOperation_OperationOptions_cp, ttrait_Uint32, &NdbScanOperation::deleteCurrentTuple >(env, obj, p0, p1, p2, p3, p4, p5);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbOperation_cp, ttrait_c_m_n_n_NdbScanOperation_r, ttrait_c_m_n_n_NdbTransaction_p, ttrait_c_m_n_n_NdbRecord_cp, ttrait_char_1p_bb, ttrait_Uint8_0cp_a, ttrait_c_m_n_n_NdbOperation_OperationOptions_cp, ttrait_Uint32, &NdbApiWrapper::NdbScanOperation__deleteCurrentTuple >(env, NULL, obj, p0, p1, p2, p3, p4, p5);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

// ---------------------------------------------------------------------------

//#include "com_mysql_ndbjtie_ndbapi_NdbScanOperation_ScanOptions.h"

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanOperation_ScanOptions
 * Method:    size
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_00024ScanOptions_size(JNIEnv * env, jclass cls)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_00024ScanOptions_size(JNIEnv *, jclass)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_fr< ttrait_Uint32, &NdbScanOperation::ScanOptions::size >(env, cls);
#else
    return gcall_fr< ttrait_Uint32, &NdbApiWrapper::NdbScanOperation__ScanOptions__size >(env, cls);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanOperation_ScanOptions
 * Method:    optionsPresent
 * Signature: ()J
 */
JNIEXPORT jlong JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_00024ScanOptions_optionsPresent__(JNIEnv * env, jobject obj)
{
    TRACE("jlong Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_00024ScanOptions_optionsPresent__(JNIEnv *, jobject)");
    return gget< ttrait_c_m_n_n_NdbScanOperation_ScanOptions_t, ttrait_Uint64, &NdbScanOperation::ScanOptions::optionsPresent >(env, obj);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanOperation_ScanOptions
 * Method:    scan_flags
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_00024ScanOptions_scan_1flags__(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_00024ScanOptions_scan_1flags__(JNIEnv *, jobject)");
    return gget< ttrait_c_m_n_n_NdbScanOperation_ScanOptions_t, ttrait_Uint32, &NdbScanOperation::ScanOptions::scan_flags >(env, obj);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanOperation_ScanOptions
 * Method:    parallel
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_00024ScanOptions_parallel__(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_00024ScanOptions_parallel__(JNIEnv *, jobject)");
    return gget< ttrait_c_m_n_n_NdbScanOperation_ScanOptions_t, ttrait_Uint32, &NdbScanOperation::ScanOptions::parallel >(env, obj);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanOperation_ScanOptions
 * Method:    batch
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_00024ScanOptions_batch__(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_00024ScanOptions_batch__(JNIEnv *, jobject)");
    return gget< ttrait_c_m_n_n_NdbScanOperation_ScanOptions_t, ttrait_Uint32, &NdbScanOperation::ScanOptions::batch >(env, obj);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanOperation_ScanOptions
 * Method:    extraGetValues
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbOperation/GetValueSpecArray;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_00024ScanOptions_extraGetValues__(JNIEnv * env, jobject obj)
{
    TRACE("Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_00024ScanOptions_extraGetValues__(JNIEnv *, jobject)");
    return gget< ttrait_c_m_n_n_NdbScanOperation_ScanOptions_t, ttrait_c_m_n_n_NdbOperation_GetValueSpecArray_p, &NdbScanOperation::ScanOptions::extraGetValues >(env, obj);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanOperation_ScanOptions
 * Method:    numExtraGetValues
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_00024ScanOptions_numExtraGetValues__(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_00024ScanOptions_numExtraGetValues__(JNIEnv *, jobject)");
    return gget< ttrait_c_m_n_n_NdbScanOperation_ScanOptions_t, ttrait_Uint32, &NdbScanOperation::ScanOptions::numExtraGetValues >(env, obj);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanOperation_ScanOptions
 * Method:    partitionId
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_00024ScanOptions_partitionId__(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_00024ScanOptions_partitionId__(JNIEnv *, jobject)");
    return gget< ttrait_c_m_n_n_NdbScanOperation_ScanOptions_t, ttrait_Uint32, &NdbScanOperation::ScanOptions::partitionId >(env, obj);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanOperation_ScanOptions
 * Method:    interpretedCode
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbInterpretedCodeConst;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_00024ScanOptions_interpretedCode__(JNIEnv * env, jobject obj)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_00024ScanOptions_interpretedCode__(JNIEnv *, jobject)");
    return gget< ttrait_c_m_n_n_NdbScanOperation_ScanOptions_t, ttrait_c_m_n_n_NdbInterpretedCode_cp, &NdbScanOperation::ScanOptions::interpretedCode >(env, obj);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanOperation_ScanOptions
 * Method:    optionsPresent
 * Signature: (J)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_00024ScanOptions_optionsPresent__J(JNIEnv * env, jobject obj, jlong p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_00024ScanOptions_optionsPresent__J(JNIEnv *, jobject, jlong)");
    gset< ttrait_c_m_n_n_NdbScanOperation_ScanOptions_t, ttrait_Uint64, &NdbScanOperation::ScanOptions::optionsPresent >(env, obj, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanOperation_ScanOptions
 * Method:    scan_flags
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_00024ScanOptions_scan_1flags__I(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_00024ScanOptions_scan_1flags__I(JNIEnv *, jobject, jint)");
    gset< ttrait_c_m_n_n_NdbScanOperation_ScanOptions_t, ttrait_Uint32, &NdbScanOperation::ScanOptions::scan_flags >(env, obj, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanOperation_ScanOptions
 * Method:    parallel
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_00024ScanOptions_parallel__I(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_00024ScanOptions_parallel__I(JNIEnv *, jobject, jint)");
    gset< ttrait_c_m_n_n_NdbScanOperation_ScanOptions_t, ttrait_Uint32, &NdbScanOperation::ScanOptions::parallel >(env, obj, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanOperation_ScanOptions
 * Method:    batch
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_00024ScanOptions_batch__I(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_00024ScanOptions_batch__I(JNIEnv *, jobject, jint)");
    gset< ttrait_c_m_n_n_NdbScanOperation_ScanOptions_t, ttrait_Uint32, &NdbScanOperation::ScanOptions::batch >(env, obj, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanOperation_ScanOptions
 * Method:    extraGetValues
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbOperation/GetValueSpecArray;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_00024ScanOptions_extraGetValues__Lcom_mysql_ndbjtie_ndbapi_NdbOperation_00024GetValueSpecArray_2(JNIEnv * env, jobject obj, jobject p0)
{
    TRACE("Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_00024ScanOptions_extraGetValues__Lcom_mysql_ndbjtie_ndbapi_NdbOperation_00024GetValueSpecArray_2(JNIEnv *, jobject, jobject)");
    gset< ttrait_c_m_n_n_NdbScanOperation_ScanOptions_t, ttrait_c_m_n_n_NdbOperation_GetValueSpecArray_p, &NdbScanOperation::ScanOptions::extraGetValues >(env, obj, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanOperation_ScanOptions
 * Method:    numExtraGetValues
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_00024ScanOptions_numExtraGetValues__I(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_00024ScanOptions_numExtraGetValues__I(JNIEnv *, jobject, jint)");
    gset< ttrait_c_m_n_n_NdbScanOperation_ScanOptions_t, ttrait_Uint32, &NdbScanOperation::ScanOptions::numExtraGetValues >(env, obj, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanOperation_ScanOptions
 * Method:    partitionId
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_00024ScanOptions_partitionId__I(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_00024ScanOptions_partitionId__I(JNIEnv *, jobject, jint)");
    gset< ttrait_c_m_n_n_NdbScanOperation_ScanOptions_t, ttrait_Uint32, &NdbScanOperation::ScanOptions::partitionId >(env, obj, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanOperation_ScanOptions
 * Method:    interpretedCode
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbInterpretedCodeConst;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_00024ScanOptions_interpretedCode__Lcom_mysql_ndbjtie_ndbapi_NdbInterpretedCodeConst_2(JNIEnv * env, jobject obj, jobject p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_00024ScanOptions_interpretedCode__Lcom_mysql_ndbjtie_ndbapi_NdbInterpretedCodeConst_2(JNIEnv *, jobject, jobject)");
    gset< ttrait_c_m_n_n_NdbScanOperation_ScanOptions_t, ttrait_c_m_n_n_NdbInterpretedCode_cp, &NdbScanOperation::ScanOptions::interpretedCode >(env, obj, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanOperation_ScanOptions
 * Method:    create
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbScanOperation/ScanOptions;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_00024ScanOptions_create(JNIEnv * env, jclass cls)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_00024ScanOptions_create(JNIEnv *, jclass)");
    return gcreate< ttrait_c_m_n_n_NdbScanOperation_ScanOptions_r >(env, cls);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbScanOperation_ScanOptions
 * Method:    delete
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbScanOperation/ScanOptions;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_00024ScanOptions_delete(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbScanOperation_00024ScanOptions_delete(JNIEnv *, jclass, jobject)");
    gdelete< ttrait_c_m_n_n_NdbScanOperation_ScanOptions_r >(env, cls, p0);
}

// ---------------------------------------------------------------------------

//#include "com_mysql_ndbjtie_ndbapi_NdbTransaction.h"

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbTransaction
 * Method:    getNdbError
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbErrorConst;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbTransaction_getNdbError(JNIEnv * env, jobject obj)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbTransaction_getNdbError(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbTransaction_ct, ttrait_c_m_n_n_NdbError_cr, &NdbTransaction::getNdbError >(env, obj);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbError_cr, ttrait_c_m_n_n_NdbTransaction_cr, &NdbApiWrapper::NdbTransaction__getNdbError >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbTransaction
 * Method:    getNdbErrorOperation
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbOperationConst;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbTransaction_getNdbErrorOperation(JNIEnv * env, jobject obj)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbTransaction_getNdbErrorOperation(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_CONST_OVERLOADED_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbTransaction_ct, ttrait_c_m_n_n_NdbOperation_cp, &NdbTransaction::getNdbErrorOperation >(env, obj); // call of overloaded const/non-const method
#else
    return gcall_fr< ttrait_c_m_n_n_NdbOperation_cp, ttrait_c_m_n_n_NdbTransaction_cr, &NdbApiWrapper::NdbTransaction__getNdbErrorOperation__0 >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_CONST_OVERLOADED_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbTransaction
 * Method:    getNextCompletedOperation
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbOperationConst;)Lcom/mysql/ndbjtie/ndbapi/NdbOperationConst;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbTransaction_getNextCompletedOperation(JNIEnv * env, jobject obj, jobject p0)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbTransaction_getNextCompletedOperation(JNIEnv *, jobject, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbTransaction_ct, ttrait_c_m_n_n_NdbOperation_cp, ttrait_c_m_n_n_NdbOperation_cp, &NdbTransaction::getNextCompletedOperation >(env, obj, p0);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbOperation_cp, ttrait_c_m_n_n_NdbTransaction_cr, ttrait_c_m_n_n_NdbOperation_cp, &NdbApiWrapper::NdbTransaction__getNextCompletedOperation >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbTransaction
 * Method:    getNdbOperation
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/TableConst;)Lcom/mysql/ndbjtie/ndbapi/NdbOperation;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbTransaction_getNdbOperation(JNIEnv * env, jobject obj, jobject p0)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbTransaction_getNdbOperation(JNIEnv *, jobject, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbTransaction_t, ttrait_c_m_n_n_NdbOperation_p, ttrait_c_m_n_n_NdbDictionary_Table_cp, &NdbTransaction::getNdbOperation >(env, obj, p0);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbOperation_p, ttrait_c_m_n_n_NdbTransaction_r, ttrait_c_m_n_n_NdbDictionary_Table_cp, &NdbApiWrapper::NdbTransaction__getNdbOperation >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbTransaction
 * Method:    getNdbScanOperation
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/TableConst;)Lcom/mysql/ndbjtie/ndbapi/NdbScanOperation;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbTransaction_getNdbScanOperation(JNIEnv * env, jobject obj, jobject p0)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbTransaction_getNdbScanOperation(JNIEnv *, jobject, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbTransaction_t, ttrait_c_m_n_n_NdbScanOperation_p, ttrait_c_m_n_n_NdbDictionary_Table_cp, &NdbTransaction::getNdbScanOperation >(env, obj, p0);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbScanOperation_p, ttrait_c_m_n_n_NdbTransaction_r, ttrait_c_m_n_n_NdbDictionary_Table_cp, &NdbApiWrapper::NdbTransaction__getNdbScanOperation >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbTransaction
 * Method:    getNdbIndexScanOperation
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/IndexConst;)Lcom/mysql/ndbjtie/ndbapi/NdbIndexScanOperation;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbTransaction_getNdbIndexScanOperation(JNIEnv * env, jobject obj, jobject p0)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbTransaction_getNdbIndexScanOperation(JNIEnv *, jobject, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbTransaction_t, ttrait_c_m_n_n_NdbIndexScanOperation_p, ttrait_c_m_n_n_NdbDictionary_Index_cp, &NdbTransaction::getNdbIndexScanOperation >(env, obj, p0);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbIndexScanOperation_p, ttrait_c_m_n_n_NdbTransaction_r, ttrait_c_m_n_n_NdbDictionary_Index_cp, &NdbApiWrapper::NdbTransaction__getNdbIndexScanOperation >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbTransaction
 * Method:    getNdbIndexOperation
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/IndexConst;)Lcom/mysql/ndbjtie/ndbapi/NdbIndexOperation;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbTransaction_getNdbIndexOperation(JNIEnv * env, jobject obj, jobject p0)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbTransaction_getNdbIndexOperation(JNIEnv *, jobject, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbTransaction_t, ttrait_c_m_n_n_NdbIndexOperation_p, ttrait_c_m_n_n_NdbDictionary_Index_cp, &NdbTransaction::getNdbIndexOperation >(env, obj, p0);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbIndexOperation_p, ttrait_c_m_n_n_NdbTransaction_r, ttrait_c_m_n_n_NdbDictionary_Index_cp, &NdbApiWrapper::NdbTransaction__getNdbIndexOperation >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbTransaction
 * Method:    execute
 * Signature: (III)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbTransaction_execute(JNIEnv * env, jobject obj, jint p0, jint p1, jint p2)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbTransaction_execute(JNIEnv *, jobject, jint, jint, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbTransaction_t, ttrait_int, ttrait_c_m_n_n_NdbTransaction_ExecType_iv/*_enum_*/, ttrait_c_m_n_n_NdbOperation_AbortOption_iv/*_enum_*/, ttrait_int, &NdbTransaction::execute >(env, obj, p0, p1, p2);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbTransaction_r, ttrait_c_m_n_n_NdbTransaction_ExecType_iv/*_enum_*/, ttrait_c_m_n_n_NdbOperation_AbortOption_iv/*_enum_*/, ttrait_int, &NdbApiWrapper::NdbTransaction__execute >(env, NULL, obj, p0, p1, p2);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbTransaction
 * Method:    refresh
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbTransaction_refresh(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbTransaction_refresh(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbTransaction_t, ttrait_int, &NdbTransaction::refresh >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbTransaction_r, &NdbApiWrapper::NdbTransaction__refresh >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbTransaction
 * Method:    close
 * Signature: ()V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbTransaction_close(JNIEnv * env, jobject obj)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbTransaction_close(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_NdbTransaction_t, &NdbTransaction::close >(env, obj);
#else
    gcall_fv< ttrait_c_m_n_n_NdbTransaction_r, &NdbApiWrapper::NdbTransaction__close >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbTransaction
 * Method:    getGCI
 * Signature: ([J)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbTransaction_getGCI___3J(JNIEnv * env, jobject obj, jlongArray p0)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbTransaction_getGCI___3J(JNIEnv *, jobject, jlongArray)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbTransaction_t, ttrait_int, ttrait_Uint64_0p_a, &NdbTransaction::getGCI >(env, obj, p0);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbTransaction_r, ttrait_Uint64_0p_a, &NdbApiWrapper::NdbTransaction__getGCI >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbTransaction
 * Method:    getTransactionId
 * Signature: ()J
 */
JNIEXPORT jlong JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbTransaction_getTransactionId(JNIEnv * env, jobject obj)
{
    TRACE("jlong Java_com_mysql_ndbjtie_ndbapi_NdbTransaction_getTransactionId(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbTransaction_t, ttrait_Uint64, &NdbTransaction::getTransactionId >(env, obj);
#else
    return gcall_fr< ttrait_Uint64, ttrait_c_m_n_n_NdbTransaction_r, &NdbApiWrapper::NdbTransaction__getTransactionId >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbTransaction
 * Method:    commitStatus
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbTransaction_commitStatus(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbTransaction_commitStatus(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbTransaction_t, ttrait_c_m_n_n_NdbTransaction_CommitStatusType_iv/*_enum_*/, &NdbTransaction::commitStatus >(env, obj);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbTransaction_CommitStatusType_iv/*_enum_*/, ttrait_c_m_n_n_NdbTransaction_r, &NdbApiWrapper::NdbTransaction__commitStatus >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbTransaction
 * Method:    getNdbErrorLine
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbTransaction_getNdbErrorLine(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbTransaction_getNdbErrorLine(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbTransaction_t, ttrait_int, &NdbTransaction::getNdbErrorLine >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbTransaction_r, &NdbApiWrapper::NdbTransaction__getNdbErrorLine >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbTransaction
 * Method:    readTuple
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbRecordConst;Ljava/nio/ByteBuffer;Lcom/mysql/ndbjtie/ndbapi/NdbRecordConst;Ljava/nio/ByteBuffer;I[BLcom/mysql/ndbjtie/ndbapi/NdbOperation/OperationOptionsConst;I)Lcom/mysql/ndbjtie/ndbapi/NdbOperationConst;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbTransaction_readTuple(JNIEnv * env, jobject obj, jobject p0, jobject p1, jobject p2, jobject p3, jint p4, jbyteArray p5, jobject p6, jint p7)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbTransaction_readTuple(JNIEnv *, jobject, jobject, jobject, jobject, jobject, jint, jbyteArray, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbTransaction_t, ttrait_c_m_n_n_NdbOperation_cp, ttrait_c_m_n_n_NdbRecord_cp, ttrait_char_1cp_bb, ttrait_c_m_n_n_NdbRecord_cp, ttrait_char_1p_bb, ttrait_c_m_n_n_NdbOperation_LockMode_iv/*_enum_*/, ttrait_Uint8_0cp_a, ttrait_c_m_n_n_NdbOperation_OperationOptions_cp, ttrait_Uint32, &NdbTransaction::readTuple >(env, obj, p0, p1, p2, p3, p4, p5, p6, p7);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbOperation_cp, ttrait_c_m_n_n_NdbTransaction_r, ttrait_c_m_n_n_NdbRecord_cp, ttrait_char_1cp_bb, ttrait_c_m_n_n_NdbRecord_cp, ttrait_char_1p_bb, ttrait_c_m_n_n_NdbOperation_LockMode_iv/*_enum_*/, ttrait_Uint8_0cp_a, ttrait_c_m_n_n_NdbOperation_OperationOptions_cp, ttrait_Uint32, &NdbApiWrapper::NdbTransaction__readTuple >(env, NULL, obj, p0, p1, p2, p3, p4, p5, p6, p7);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbTransaction
 * Method:    insertTuple
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbRecordConst;Ljava/nio/ByteBuffer;Lcom/mysql/ndbjtie/ndbapi/NdbRecordConst;Ljava/nio/ByteBuffer;[BLcom/mysql/ndbjtie/ndbapi/NdbOperation/OperationOptionsConst;I)Lcom/mysql/ndbjtie/ndbapi/NdbOperationConst;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbTransaction_insertTuple__Lcom_mysql_ndbjtie_ndbapi_NdbRecordConst_2Ljava_nio_ByteBuffer_2Lcom_mysql_ndbjtie_ndbapi_NdbRecordConst_2Ljava_nio_ByteBuffer_2_3BLcom_mysql_ndbjtie_ndbapi_NdbOperation_00024OperationOptionsConst_2I(JNIEnv * env, jobject obj, jobject p0, jobject p1, jobject p2, jobject p3, jbyteArray p4, jobject p5, jint p6)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbTransaction_insertTuple__Lcom_mysql_ndbjtie_ndbapi_NdbRecordConst_2Ljava_nio_ByteBuffer_2Lcom_mysql_ndbjtie_ndbapi_NdbRecordConst_2Ljava_nio_ByteBuffer_2_3BLcom_mysql_ndbjtie_ndbapi_NdbOperation_00024OperationOptionsConst_2I(JNIEnv *, jobject, jobject, jobject, jobject, jobject, jbyteArray, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbTransaction_t, ttrait_c_m_n_n_NdbOperation_cp, ttrait_c_m_n_n_NdbRecord_cp, ttrait_char_1cp_bb, ttrait_c_m_n_n_NdbRecord_cp, ttrait_char_1cp_bb, ttrait_Uint8_0cp_a, ttrait_c_m_n_n_NdbOperation_OperationOptions_cp, ttrait_Uint32, &NdbTransaction::insertTuple >(env, obj, p0, p1, p2, p3, p4, p5, p6);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbOperation_cp, ttrait_c_m_n_n_NdbTransaction_r, ttrait_c_m_n_n_NdbRecord_cp, ttrait_char_1cp_bb, ttrait_c_m_n_n_NdbRecord_cp, ttrait_char_1cp_bb, ttrait_Uint8_0cp_a, ttrait_c_m_n_n_NdbOperation_OperationOptions_cp, ttrait_Uint32, &NdbApiWrapper::NdbTransaction__insertTuple__0 >(env, NULL, obj, p0, p1, p2, p3, p4, p5, p6);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbTransaction
 * Method:    insertTuple
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbRecordConst;Ljava/nio/ByteBuffer;[BLcom/mysql/ndbjtie/ndbapi/NdbOperation/OperationOptionsConst;I)Lcom/mysql/ndbjtie/ndbapi/NdbOperationConst;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbTransaction_insertTuple__Lcom_mysql_ndbjtie_ndbapi_NdbRecordConst_2Ljava_nio_ByteBuffer_2_3BLcom_mysql_ndbjtie_ndbapi_NdbOperation_00024OperationOptionsConst_2I(JNIEnv * env, jobject obj, jobject p0, jobject p1, jbyteArray p2, jobject p3, jint p4)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbTransaction_insertTuple__Lcom_mysql_ndbjtie_ndbapi_NdbRecordConst_2Ljava_nio_ByteBuffer_2_3BLcom_mysql_ndbjtie_ndbapi_NdbOperation_00024OperationOptionsConst_2I(JNIEnv *, jobject, jobject, jobject, jbyteArray, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbTransaction_t, ttrait_c_m_n_n_NdbOperation_cp, ttrait_c_m_n_n_NdbRecord_cp, ttrait_char_1cp_bb, ttrait_Uint8_0cp_a, ttrait_c_m_n_n_NdbOperation_OperationOptions_cp, ttrait_Uint32, &NdbTransaction::insertTuple >(env, obj, p0, p1, p2, p3, p4);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbOperation_cp, ttrait_c_m_n_n_NdbTransaction_r, ttrait_c_m_n_n_NdbRecord_cp, ttrait_char_1cp_bb, ttrait_Uint8_0cp_a, ttrait_c_m_n_n_NdbOperation_OperationOptions_cp, ttrait_Uint32, &NdbApiWrapper::NdbTransaction__insertTuple__1 >(env, NULL, obj, p0, p1, p2, p3, p4);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbTransaction
 * Method:    updateTuple
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbRecordConst;Ljava/nio/ByteBuffer;Lcom/mysql/ndbjtie/ndbapi/NdbRecordConst;Ljava/nio/ByteBuffer;[BLcom/mysql/ndbjtie/ndbapi/NdbOperation/OperationOptionsConst;I)Lcom/mysql/ndbjtie/ndbapi/NdbOperationConst;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbTransaction_updateTuple(JNIEnv * env, jobject obj, jobject p0, jobject p1, jobject p2, jobject p3, jbyteArray p4, jobject p5, jint p6)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbTransaction_updateTuple(JNIEnv *, jobject, jobject, jobject, jobject, jobject, jbyteArray, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbTransaction_t, ttrait_c_m_n_n_NdbOperation_cp, ttrait_c_m_n_n_NdbRecord_cp, ttrait_char_1cp_bb, ttrait_c_m_n_n_NdbRecord_cp, ttrait_char_1cp_bb, ttrait_Uint8_0cp_a, ttrait_c_m_n_n_NdbOperation_OperationOptions_cp, ttrait_Uint32, &NdbTransaction::updateTuple >(env, obj, p0, p1, p2, p3, p4, p5, p6);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbOperation_cp, ttrait_c_m_n_n_NdbTransaction_r, ttrait_c_m_n_n_NdbRecord_cp, ttrait_char_1cp_bb, ttrait_c_m_n_n_NdbRecord_cp, ttrait_char_1cp_bb, ttrait_Uint8_0cp_a, ttrait_c_m_n_n_NdbOperation_OperationOptions_cp, ttrait_Uint32, &NdbApiWrapper::NdbTransaction__updateTuple >(env, NULL, obj, p0, p1, p2, p3, p4, p5, p6);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbTransaction
 * Method:    writeTuple
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbRecordConst;Ljava/nio/ByteBuffer;Lcom/mysql/ndbjtie/ndbapi/NdbRecordConst;Ljava/nio/ByteBuffer;[BLcom/mysql/ndbjtie/ndbapi/NdbOperation/OperationOptionsConst;I)Lcom/mysql/ndbjtie/ndbapi/NdbOperationConst;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbTransaction_writeTuple(JNIEnv * env, jobject obj, jobject p0, jobject p1, jobject p2, jobject p3, jbyteArray p4, jobject p5, jint p6)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbTransaction_writeTuple(JNIEnv *, jobject, jobject, jobject, jobject, jobject, jbyteArray, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbTransaction_t, ttrait_c_m_n_n_NdbOperation_cp, ttrait_c_m_n_n_NdbRecord_cp, ttrait_char_1cp_bb, ttrait_c_m_n_n_NdbRecord_cp, ttrait_char_1cp_bb, ttrait_Uint8_0cp_a, ttrait_c_m_n_n_NdbOperation_OperationOptions_cp, ttrait_Uint32, &NdbTransaction::writeTuple >(env, obj, p0, p1, p2, p3, p4, p5, p6);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbOperation_cp, ttrait_c_m_n_n_NdbTransaction_r, ttrait_c_m_n_n_NdbRecord_cp, ttrait_char_1cp_bb, ttrait_c_m_n_n_NdbRecord_cp, ttrait_char_1cp_bb, ttrait_Uint8_0cp_a, ttrait_c_m_n_n_NdbOperation_OperationOptions_cp, ttrait_Uint32, &NdbApiWrapper::NdbTransaction__writeTuple >(env, NULL, obj, p0, p1, p2, p3, p4, p5, p6);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbTransaction
 * Method:    deleteTuple
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbRecordConst;Ljava/nio/ByteBuffer;Lcom/mysql/ndbjtie/ndbapi/NdbRecordConst;Ljava/nio/ByteBuffer;[BLcom/mysql/ndbjtie/ndbapi/NdbOperation/OperationOptionsConst;I)Lcom/mysql/ndbjtie/ndbapi/NdbOperationConst;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbTransaction_deleteTuple(JNIEnv * env, jobject obj, jobject p0, jobject p1, jobject p2, jobject p3, jbyteArray p4, jobject p5, jint p6)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbTransaction_deleteTuple(JNIEnv *, jobject, jobject, jobject, jobject, jobject, jbyteArray, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbTransaction_t, ttrait_c_m_n_n_NdbOperation_cp, ttrait_c_m_n_n_NdbRecord_cp, ttrait_char_1cp_bb, ttrait_c_m_n_n_NdbRecord_cp, ttrait_char_1p_bb, ttrait_Uint8_0cp_a, ttrait_c_m_n_n_NdbOperation_OperationOptions_cp, ttrait_Uint32, &NdbTransaction::deleteTuple >(env, obj, p0, p1, p2, p3, p4, p5, p6);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbOperation_cp, ttrait_c_m_n_n_NdbTransaction_r, ttrait_c_m_n_n_NdbRecord_cp, ttrait_char_1cp_bb, ttrait_c_m_n_n_NdbRecord_cp, ttrait_char_1p_bb, ttrait_Uint8_0cp_a, ttrait_c_m_n_n_NdbOperation_OperationOptions_cp, ttrait_Uint32, &NdbApiWrapper::NdbTransaction__deleteTuple >(env, NULL, obj, p0, p1, p2, p3, p4, p5, p6);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbTransaction
 * Method:    scanTable
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbRecordConst;I[BLcom/mysql/ndbjtie/ndbapi/NdbScanOperation/ScanOptionsConst;I)Lcom/mysql/ndbjtie/ndbapi/NdbScanOperation;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbTransaction_scanTable(JNIEnv * env, jobject obj, jobject p0, jint p1, jbyteArray p2, jobject p3, jint p4)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbTransaction_scanTable(JNIEnv *, jobject, jobject, jint, jbyteArray, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbTransaction_t, ttrait_c_m_n_n_NdbScanOperation_p, ttrait_c_m_n_n_NdbRecord_cp, ttrait_c_m_n_n_NdbOperation_LockMode_iv/*_enum_*/, ttrait_Uint8_0cp_a, ttrait_c_m_n_n_NdbScanOperation_ScanOptions_cp, ttrait_Uint32, &NdbTransaction::scanTable >(env, obj, p0, p1, p2, p3, p4);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbScanOperation_p, ttrait_c_m_n_n_NdbTransaction_r, ttrait_c_m_n_n_NdbRecord_cp, ttrait_c_m_n_n_NdbOperation_LockMode_iv/*_enum_*/, ttrait_Uint8_0cp_a, ttrait_c_m_n_n_NdbScanOperation_ScanOptions_cp, ttrait_Uint32, &NdbApiWrapper::NdbTransaction__scanTable >(env, NULL, obj, p0, p1, p2, p3, p4);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbTransaction
 * Method:    scanIndex
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbRecordConst;Lcom/mysql/ndbjtie/ndbapi/NdbRecordConst;I[BLcom/mysql/ndbjtie/ndbapi/NdbIndexScanOperation/IndexBoundConst;Lcom/mysql/ndbjtie/ndbapi/NdbScanOperation/ScanOptionsConst;I)Lcom/mysql/ndbjtie/ndbapi/NdbIndexScanOperation;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbTransaction_scanIndex(JNIEnv * env, jobject obj, jobject p0, jobject p1, jint p2, jbyteArray p3, jobject p4, jobject p5, jint p6)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbTransaction_scanIndex(JNIEnv *, jobject, jobject, jobject, jint, jbyteArray, jobject, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbTransaction_t, ttrait_c_m_n_n_NdbIndexScanOperation_p, ttrait_c_m_n_n_NdbRecord_cp, ttrait_c_m_n_n_NdbRecord_cp, ttrait_c_m_n_n_NdbOperation_LockMode_iv/*_enum_*/, ttrait_Uint8_0cp_a, ttrait_c_m_n_n_NdbIndexScanOperation_IndexBound_cp, ttrait_c_m_n_n_NdbScanOperation_ScanOptions_cp, ttrait_Uint32, &NdbTransaction::scanIndex >(env, obj, p0, p1, p2, p3, p4, p5, p6);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbIndexScanOperation_p, ttrait_c_m_n_n_NdbTransaction_r, ttrait_c_m_n_n_NdbRecord_cp, ttrait_c_m_n_n_NdbRecord_cp, ttrait_c_m_n_n_NdbOperation_LockMode_iv/*_enum_*/, ttrait_Uint8_0cp_a, ttrait_c_m_n_n_NdbIndexScanOperation_IndexBound_cp, ttrait_c_m_n_n_NdbScanOperation_ScanOptions_cp, ttrait_Uint32, &NdbApiWrapper::NdbTransaction__scanIndex >(env, NULL, obj, p0, p1, p2, p3, p4, p5, p6);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbTransaction
 * Method:    unlock
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbLockHandleConst;I)Lcom/mysql/ndbjtie/ndbapi/NdbOperationConst;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbTransaction_unlock(JNIEnv * env, jobject obj, jobject p0, jint p1)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbTransaction_unlock(JNIEnv *, jobject, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbTransaction_t, ttrait_c_m_n_n_NdbOperation_cp, ttrait_c_m_n_n_NdbLockHandle_cp, ttrait_c_m_n_n_NdbOperation_AbortOption_iv/*_enum_*/, &NdbTransaction::unlock >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_c_m_n_n_NdbOperation_cp, ttrait_c_m_n_n_NdbTransaction_r, ttrait_c_m_n_n_NdbLockHandle_cp, ttrait_c_m_n_n_NdbOperation_AbortOption_iv/*_enum_*/, &NdbApiWrapper::NdbTransaction__unlock >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbTransaction
 * Method:    releaseLockHandle
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbLockHandleConst;)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbTransaction_releaseLockHandle(JNIEnv * env, jobject obj, jobject p0)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbTransaction_releaseLockHandle(JNIEnv *, jobject, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_NdbTransaction_t, ttrait_int, ttrait_c_m_n_n_NdbLockHandle_cp, &NdbTransaction::releaseLockHandle >(env, obj, p0);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_NdbTransaction_r, ttrait_c_m_n_n_NdbLockHandle_cp, &NdbApiWrapper::NdbTransaction__releaseLockHandle >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

// ---------------------------------------------------------------------------

//#include "com_mysql_ndbjtie_ndbapi_Ndb_Key_part_ptr.h"

/*
 * Class:     com_mysql_ndbjtie_ndbapi_Ndb_Key_part_ptr
 * Method:    len
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_Ndb_00024Key_1part_1ptr_len__(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_Ndb_00024Key_1part_1ptr_len__(JNIEnv *, jobject)");
    return gget< ttrait_c_m_n_n_Ndb_Key_part_ptr_t, ttrait_uint, &Ndb::Key_part_ptr::len >(env, obj);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_Ndb_Key_part_ptr
 * Method:    ptr
 * Signature: (Ljava/nio/ByteBuffer;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_Ndb_00024Key_1part_1ptr_ptr__Ljava_nio_ByteBuffer_2(JNIEnv * env, jobject obj, jobject p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_Ndb_00024Key_1part_1ptr_ptr__Ljava_nio_ByteBuffer_2(JNIEnv *, jobject, jobject)");
    gset< ttrait_c_m_n_n_Ndb_Key_part_ptr_t, ttrait_void_1cp_bb, &Ndb::Key_part_ptr::ptr >(env, obj, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_Ndb_Key_part_ptr
 * Method:    len
 * Signature: (I)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_Ndb_00024Key_1part_1ptr_len__I(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_Ndb_00024Key_1part_1ptr_len__I(JNIEnv *, jobject, jint)");
    gset< ttrait_c_m_n_n_Ndb_Key_part_ptr_t, ttrait_uint, &Ndb::Key_part_ptr::len >(env, obj, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_Ndb_Key_part_ptr
 * Method:    create
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/Ndb/Key_part_ptr;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_Ndb_00024Key_1part_1ptr_create(JNIEnv * env, jclass cls)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_Ndb_00024Key_1part_1ptr_create(JNIEnv *, jclass)");
    return gcreate< ttrait_c_m_n_n_Ndb_Key_part_ptr_r >(env, cls);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_Ndb_Key_part_ptr
 * Method:    delete
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/Ndb/Key/part/ptr;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_Ndb_00024Key_1part_1ptr_delete(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_Ndb_00024Key_1part_1ptr_delete(JNIEnv *, jclass, jobject)");
    gdelete< ttrait_c_m_n_n_Ndb_Key_part_ptr_r >(env, cls, p0);
}

// ---------------------------------------------------------------------------

//#include "com_mysql_ndbjtie_ndbapi_Ndb_Key_part_ptrArray.h"

/*
 * Class:     com_mysql_ndbjtie_ndbapi_Ndb_Key_part_ptrArray
 * Method:    create
 * Signature: (I)Lcom/mysql/ndbjtie/ndbapi/Ndb/Key_part_ptrArray;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_Ndb_00024Key_1part_1ptrArray_create(JNIEnv * env, jclass cls, jint p0)
{
    TRACE("Java_com_mysql_ndbjtie_ndbapi_Ndb_00024Key_1part_1ptrArray_create(JNIEnv *, jclass, jint)");
    return gcreateArray< ttrait_c_m_n_n_Ndb_Key_part_ptrArray_r, ttrait_int32 >(env, cls, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_Ndb_Key_part_ptrArray
 * Method:    delete
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/Ndb/Key_part_ptrArray;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_Ndb_00024Key_1part_1ptrArray_delete(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("Java_com_mysql_ndbjtie_ndbapi_Ndb_00024Key_1part_1ptrArray_delete(JNIEnv *, jclass, jobject)");
    gdeleteArray< ttrait_c_m_n_n_Ndb_Key_part_ptrArray_r >(env, cls, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_Ndb_Key_part_ptrArray
 * Method:    at
 * Signature: (I)Lcom/mysql/ndbjtie/ndbapi/Ndb/Key_part_ptr;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_Ndb_00024Key_1part_1ptrArray_at(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("Java_com_mysql_ndbjtie_ndbapi_Ndb_00024Key_1part_1ptrArray_at(JNIEnv *, jobject, jint)");
    return gat< ttrait_c_m_n_n_Ndb_Key_part_ptr_r, ttrait_c_m_n_n_Ndb_Key_part_ptrArray_r, ttrait_int32 >(env, NULL, obj, p0);
}

// ---------------------------------------------------------------------------

//#include "com_mysql_ndbjtie_ndbapi_Ndb_PartitionSpec.h"

/*
 * Class:     com_mysql_ndbjtie_ndbapi_Ndb_PartitionSpec
 * Method:    size
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_Ndb_00024PartitionSpec_size(JNIEnv * env, jclass cls)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_Ndb_00024PartitionSpec_size(JNIEnv *, jclass)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_fr< ttrait_Uint32, &Ndb::PartitionSpec::size >(env, cls);
#else
    return gcall_fr< ttrait_Uint32, &NdbApiWrapper::Ndb__PartitionSpec__size >(env, cls);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_Ndb_PartitionSpec
 * Method:    type
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_Ndb_00024PartitionSpec_type(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_Ndb_00024PartitionSpec_type(JNIEnv *, jobject)");
    return gget< ttrait_c_m_n_n_Ndb_PartitionSpec_t, ttrait_Uint32, &Ndb::PartitionSpec::type >(env, obj);
}

// ---------------------------------------------------------------------------

//#include "com_mysql_ndbjtie_ndbapi_Ndb_cluster_connection.h"

/*
 * Class:     com_mysql_ndbjtie_ndbapi_Ndb_cluster_connection
 * Method:    get_latest_error
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_Ndb_1cluster_1connection_get_1latest_1error(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_Ndb_1cluster_1connection_get_1latest_1error(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_Ndb_cluster_connection_ct, ttrait_int, &Ndb_cluster_connection::get_latest_error >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_Ndb_cluster_connection_cr, &NdbApiWrapper::Ndb_cluster_connection__get_latest_error >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_Ndb_cluster_connection
 * Method:    get_latest_error_msg
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL
Java_com_mysql_ndbjtie_ndbapi_Ndb_1cluster_1connection_get_1latest_1error_1msg(JNIEnv * env, jobject obj)
{
    TRACE("jstring Java_com_mysql_ndbjtie_ndbapi_Ndb_1cluster_1connection_get_1latest_1error_1msg(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_Ndb_cluster_connection_ct, ttrait_char_cp_jutf8null, &Ndb_cluster_connection::get_latest_error_msg >(env, obj);
#else
    return gcall_fr< ttrait_char_cp_jutf8null, ttrait_c_m_n_n_Ndb_cluster_connection_cr, &NdbApiWrapper::Ndb_cluster_connection__get_latest_error_msg >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_Ndb_cluster_connection
 * Method:    create
 * Signature: (Ljava/lang/String;)Lcom/mysql/ndbjtie/ndbapi/Ndb_cluster_connection;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_Ndb_1cluster_1connection_create__Ljava_lang_String_2(JNIEnv * env, jclass cls, jstring p0)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_Ndb_1cluster_1connection_create__Ljava_lang_String_2(JNIEnv *, jclass, jstring)");
    return gcreate< ttrait_c_m_n_n_Ndb_cluster_connection_r, ttrait_char_cp_jutf8null >(env, cls, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_Ndb_cluster_connection
 * Method:    create
 * Signature: (Ljava/lang/String;I)Lcom/mysql/ndbjtie/ndbapi/Ndb_cluster_connection;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_Ndb_1cluster_1connection_create__Ljava_lang_String_2I
  (JNIEnv * env, jclass cls, jstring p0, jint p1)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_Ndb_1cluster_1connection_create__Ljava_lang_String_2I(JNIEnv *, jclass, jstring, jint)");
    return gcreate< ttrait_c_m_n_n_Ndb_cluster_connection_r, ttrait_char_cp_jutf8null, ttrait_int >(env, cls, p0, p1);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_Ndb_cluster_connection
 * Method:    delete
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/Ndb/cluster/connection;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_Ndb_1cluster_1connection_delete(JNIEnv * env, jclass cls, jobject p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_Ndb_1cluster_1connection_delete(JNIEnv *, jclass, jobject)");
    gdelete< ttrait_c_m_n_n_Ndb_cluster_connection_r >(env, cls, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_Ndb_cluster_connection
 * Method:    set_name
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_Ndb_1cluster_1connection_set_1name(JNIEnv * env, jobject obj, jstring p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_Ndb_1cluster_1connection_set_1name(JNIEnv *, jobject, jstring)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_Ndb_cluster_connection_t, ttrait_char_cp_jutf8null, &Ndb_cluster_connection::set_name >(env, obj, p0);
#else
    gcall_fv< ttrait_c_m_n_n_Ndb_cluster_connection_r, ttrait_char_cp_jutf8null, &NdbApiWrapper::Ndb_cluster_connection__set_name >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_Ndb_cluster_connection
 * Method:    set_service_uri
 * Signature:
*/
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_Ndb_1cluster_1connection_set_1service_1uri(JNIEnv * env, jobject obj, jstring p0, jstring p1, jint p2, jstring p3)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_Ndb_1cluster_1connection_set_1service_1uri(JNIEnv *, jobject, jstring, jstring, jint, jstring)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_Ndb_cluster_connection_t, ttrait_char_cp_jutf8null, ttrait_char_cp_jutf8null, ttrait_int, ttrait_char_cp_jutf8null, &Ndb_cluster_connection::set_service_uri >(env, obj, p0, p1, p2, p3);
#else
    gcall_fv< ttrait_c_m_n_n_Ndb_cluster_connection_r, ttrait_char_cp_jutf8null, ttrait_char_cp_jutf8null, ttrait_int, ttrait_char_cp_jutf8null, &NdbApiWrapper::Ndb_cluster_connection__set_service_uri >(env, NULL, obj, p0, p1, p2, p3);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_Ndb_cluster_connection
 * Method:    set_timeout
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_Ndb_1cluster_1connection_set_1timeout(JNIEnv * env, jobject obj, jint p0)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_Ndb_1cluster_1connection_set_1timeout(JNIEnv *, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_Ndb_cluster_connection_t, ttrait_int, ttrait_int, &Ndb_cluster_connection::set_timeout >(env, obj, p0);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_Ndb_cluster_connection_r, ttrait_int, &NdbApiWrapper::Ndb_cluster_connection__set_timeout >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_Ndb_cluster_connection
 * Method:    connect
 * Signature: (III)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_Ndb_1cluster_1connection_connect(JNIEnv * env, jobject obj, jint p0, jint p1, jint p2)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_Ndb_1cluster_1connection_connect(JNIEnv *, jobject, jint, jint, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_Ndb_cluster_connection_t, ttrait_int, ttrait_int, ttrait_int, ttrait_int, &Ndb_cluster_connection::connect >(env, obj, p0, p1, p2);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_Ndb_cluster_connection_r, ttrait_int, ttrait_int, ttrait_int, &NdbApiWrapper::Ndb_cluster_connection__connect >(env, NULL, obj, p0, p1, p2);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_Ndb_cluster_connection
 * Method:    wait_until_ready
 * Signature: (II)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_Ndb_1cluster_1connection_wait_1until_1ready(JNIEnv * env, jobject obj, jint p0, jint p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_Ndb_1cluster_1connection_wait_1until_1ready(JNIEnv *, jobject, jint, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_Ndb_cluster_connection_t, ttrait_int, ttrait_int, ttrait_int, &Ndb_cluster_connection::wait_until_ready >(env, obj, p0, p1);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_Ndb_cluster_connection_r, ttrait_int, ttrait_int, &NdbApiWrapper::Ndb_cluster_connection__wait_until_ready >(env, NULL, obj, p0, p1);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_Ndb_cluster_connection
 * Method:    lock_ndb_objects
 * Signature: ()V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_Ndb_1cluster_1connection_lock_1ndb_1objects(JNIEnv * env, jobject obj)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_Ndb_1cluster_1connection_lock_1ndb_1objects(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_Ndb_cluster_connection_t, &Ndb_cluster_connection::lock_ndb_objects >(env, obj);
#else
    gcall_fv< ttrait_c_m_n_n_Ndb_cluster_connection_r, &NdbApiWrapper::Ndb_cluster_connection__lock_ndb_objects >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_Ndb_cluster_connection
 * Method:    unlock_ndb_objects
 * Signature: ()V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_Ndb_1cluster_1connection_unlock_1ndb_1objects(JNIEnv * env, jobject obj)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_Ndb_1cluster_1connection_unlock_1ndb_1objects(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    gcall_mfv< ttrait_c_m_n_n_Ndb_cluster_connection_t, &Ndb_cluster_connection::unlock_ndb_objects >(env, obj);
#else
    gcall_fv< ttrait_c_m_n_n_Ndb_cluster_connection_r, &NdbApiWrapper::Ndb_cluster_connection__unlock_ndb_objects >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_Ndb_cluster_connection
 * Method:    set_recv_thread_activation_threshold
 * Signature: (I)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_Ndb_1cluster_1connection_set_1recv_1thread_1activation_1threshold (JNIEnv * env, jobject obj, jint p0)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_Ndb_1cluster_1connection_set_1recv_1thread_1activation_1threshold(JNIEnv *, jobject, jint)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_Ndb_cluster_connection_t, ttrait_int, ttrait_Uint32, &Ndb_cluster_connection::set_recv_thread_activation_threshold >(env, obj, p0);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_Ndb_cluster_connection_r, ttrait_int, &NdbApiWrapper::Ndb_cluster_connection__set_recv_thread_activation_threshold >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_Ndb_cluster_connection
 * Method:    get_recv_thread_activation_threshold
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_Ndb_1cluster_1connection_get_1recv_1thread_1activation_1threshold (JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_Ndb_1cluster_1connection_get_1recv_1thread_1activation_1threshold(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_Ndb_cluster_connection_t, ttrait_int, &Ndb_cluster_connection::get_recv_thread_activation_threshold >(env, obj);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_Ndb_cluster_connection_r, &NdbApiWrapper::Ndb_cluster_connection__get_recv_thread_activation_threshold >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_Ndb_cluster_connection
 * Method:    set_recv_thread_cpu
 * Signature: (S)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_Ndb_1cluster_1connection_set_1recv_1thread_1cpu (JNIEnv * env, jobject obj, jshort p0)
{
    TRACE("Java_com_mysql_ndbjtie_ndbapi_Ndb_1cluster_1connection_set_1recv_1thread_1cpu (JNIEnv *, jobject, jshort)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_Ndb_cluster_connection_t, ttrait_int, ttrait_Uint16, &Ndb_cluster_connection::set_recv_thread_cpu >(env, obj, p0);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_Ndb_cluster_connection_r, ttrait_short, &NdbApiWrapper::Ndb_cluster_connection__set_recv_thread_cpu >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_Ndb_cluster_connection
 * Method:    unset_recv_thread_cpu
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_Ndb_1cluster_1connection_unset_1recv_1thread_1cpu (JNIEnv * env, jobject obj)
{
    TRACE("Java_com_mysql_ndbjtie_ndbapi_Ndb_1cluster_1connection_unset_1recv_1thread_1cpu (JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_Ndb_cluster_connection_t, ttrait_int, ttrait_Uint32, &Ndb_cluster_connection::unset_recv_thread_cpu >(env, obj, 0);
#else
    return gcall_fr< ttrait_int, ttrait_c_m_n_n_Ndb_cluster_connection_r, &NdbApiWrapper::Ndb_cluster_connection__unset_recv_thread_cpu >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_Ndb_cluster_connection
 * Method:    get_next_ndb_object
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbConst;)Lcom/mysql/ndbjtie/ndbapi/NdbConst;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_Ndb_1cluster_1connection_get_1next_1ndb_1object(JNIEnv * env, jobject obj, jobject p0)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_Ndb_1cluster_1connection_get_1next_1ndb_1object(JNIEnv *, jobject, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_Ndb_cluster_connection_t, ttrait_c_m_n_n_Ndb_cp, ttrait_c_m_n_n_Ndb_cp, &Ndb_cluster_connection::get_next_ndb_object >(env, obj, p0);
#else
    return gcall_fr< ttrait_c_m_n_n_Ndb_cp, ttrait_c_m_n_n_Ndb_cluster_connection_r, ttrait_c_m_n_n_Ndb_cp, &NdbApiWrapper::Ndb_cluster_connection__get_next_ndb_object >(env, NULL, obj, p0);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_Ndb_cluster_connection
 * Method:    get_system_name
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL
Java_com_mysql_ndbjtie_ndbapi_Ndb_1cluster_1connection_get_1system_1name(JNIEnv * env, jobject obj)
{
    TRACE("jstring Java_com_mysql_ndbjtie_ndbapi_Ndb_1cluster_1connection_get_1system_1name(JNIEnv *, jobject)");
#ifndef NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
    return gcall_mfr< ttrait_c_m_n_n_Ndb_cluster_connection_ct, ttrait_char_cp_jutf8null, &Ndb_cluster_connection::get_system_name >(env, obj);
#else
    return gcall_fr< ttrait_char_cp_jutf8null, ttrait_c_m_n_n_Ndb_cluster_connection_cr, &NdbApiWrapper::Ndb_cluster_connection__get_system_name >(env, NULL, obj);
#endif // NDBJTIE_USE_WRAPPED_VARIANT_FOR_FUNCTION
}

} // extern "C"

#endif // ndbapi_jtie_hpp
