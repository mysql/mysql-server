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
 * ndbapi_wrapper.hpp
 */

#ifndef ndbapi_wrapper_hpp
#define ndbapi_wrapper_hpp

// API to implement against
#include "NdbApi.hpp"

struct NdbApiWrapper {

    // ----------------------------------------------------------------------
    // const overloaded member functions
    // ----------------------------------------------------------------------

    static NdbBlob * getBlobHandle0(const NdbOperation & obj, const char * anAttrName) {
        return obj.getBlobHandle(anAttrName);
    }
    
    static NdbBlob * getBlobHandle1(const NdbOperation & obj, Uint32 anAttrId) {
        return obj.getBlobHandle(anAttrId);
    }    

    static NdbBlob * getBlobHandle2(NdbOperation & obj, const char * anAttrName) {
        return obj.getBlobHandle(anAttrName);
    }
    
    static NdbBlob * getBlobHandle3(NdbOperation & obj, Uint32 anAttrId) {
        return obj.getBlobHandle(anAttrId);
    }
    
    static int listIndexes(const NdbDictionary::Dictionary & obj, NdbDictionary::Dictionary::List & list, const char * tableName) {
        return obj.listIndexes(list, tableName);
    }
    
    static int listEvents(const NdbDictionary::Dictionary & obj, NdbDictionary::Dictionary::List & list) {
        return obj.listEvents(list);
    }
    
    static int listObjects(const NdbDictionary::Dictionary & obj, NdbDictionary::Dictionary::List & list, NdbDictionary::Object::Type type = NdbDictionary::Object::TypeUndefined) {
        return obj.listObjects(list, type);
    }

    static int getNdbErrorLine(const NdbOperation & obj) {
        return obj.getNdbErrorLine();
    }

    static const NdbLockHandle * getLockHandle0(const NdbOperation & obj) {
        return obj.getLockHandle();
    }
    
    static const NdbLockHandle * getLockHandle1(NdbOperation & obj) {
        return obj.getLockHandle();
    }

    // ----------------------------------------------------------------------
    // overloaded non-member functions
    // ----------------------------------------------------------------------

    static int getBlobTableName(char * btname, Ndb * anNdb, const char * tableName, const char * columnName) {
        return NdbBlob::getBlobTableName(btname, anNdb, tableName, columnName);
    }
    
    static int getBlobEventName(char * bename, Ndb * anNdb, const char * eventName, const char * columnName) {
        return NdbBlob::getBlobEventName(bename, anNdb, eventName, columnName);
    }

    static const char * getValuePtr(const NdbRecord * record, const char * row, Uint32 attrId) {
        return NdbDictionary::getValuePtr(record, row, attrId);
    }

    // ----------------------------------------------------------------------
    // overloaded member functions
    // ----------------------------------------------------------------------

#if 0 // XXXXX ToDo: need to define wrappers for these functions

/*
 * Class:     com_mysql_ndbjtie_ndbapi_Ndb
 * Method:    getNdbError
 * Signature: ()Lcom/mysql/ndbjtie/ndbapi/NdbErrorConst;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_Ndb_getNdbError__(JNIEnv * env, jobject obj)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_Ndb_getNdbError__(JNIEnv *, jobject)");
    return gcall_mfr< ttrait_c_m_n_n_Ndb_ct, ttrait_c_m_n_n_NdbError_cr, &Ndb::getNdbError >(env, obj);
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
    return gcall_mfr< ttrait_c_m_n_n_Ndb_t, ttrait_c_m_n_n_NdbTransaction_p, ttrait_c_m_n_n_NdbDictionary_Table_cp, ttrait_char_0cp_bb, ttrait_Uint32, &Ndb::startTransaction >(env, obj, p0, p1, p2);
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
    return gcall_mfr< ttrait_c_m_n_n_Ndb_t, ttrait_c_m_n_n_NdbTransaction_p, ttrait_c_m_n_n_NdbDictionary_Table_cp, ttrait_Uint32, &Ndb::startTransaction >(env, obj, p0, p1);
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
    return gcall_mfr< ttrait_c_m_n_n_Ndb_t, ttrait_c_m_n_n_NdbError_cr, ttrait_int, &Ndb::getNdbError >(env, obj, p0);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Datafile_t, ttrait_int, ttrait_utf8cstring, &NdbDictionary::Datafile::setTablespace >(env, obj, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Datafile
 * Method:    setTablespace
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/TablespaceConst;)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Datafile_setTablespace__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_TablespaceConst_2(JNIEnv * env, jobject obj, jobject p0)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Datafile_setTablespace__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_TablespaceConst_2(JNIEnv *, jobject, jobject)");
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Datafile_t, ttrait_int, ttrait_c_m_n_n_NdbDictionary_Tablespace_cr, &NdbDictionary::Datafile::setTablespace >(env, obj, p0);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Dictionary_ct, ttrait_c_m_n_n_NdbDictionary_Table_cp, ttrait_utf8cstring, &NdbDictionary::Dictionary::getTable >(env, obj, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Dictionary
 * Method:    dropTable
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/Table;)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_dropTable__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_Table_2(JNIEnv * env, jobject obj, jobject p0)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Dictionary_dropTable__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_Table_2(JNIEnv *, jobject, jobject)");
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Dictionary_t, ttrait_int, ttrait_c_m_n_n_NdbDictionary_Table_r, &NdbDictionary::Dictionary::dropTable >(env, obj, p0);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Dictionary_t, ttrait_int, ttrait_utf8cstring, &NdbDictionary::Dictionary::dropTable >(env, obj, p0);
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
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Dictionary_t, ttrait_utf8cstring, &NdbDictionary::Dictionary::removeCachedTable >(env, obj, p0);
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
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Dictionary_t, ttrait_utf8cstring, ttrait_utf8cstring, &NdbDictionary::Dictionary::removeCachedIndex >(env, obj, p0, p1);
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
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Event_t, ttrait_c_m_n_n_NdbDictionary_Table_cr, &NdbDictionary::Event::setTable >(env, obj, p0);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Event_t, ttrait_int, ttrait_utf8cstring, &NdbDictionary::Event::setTable >(env, obj, p0);
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
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Event_t, ttrait_uint, &NdbDictionary::Event::addEventColumn >(env, obj, p0);
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
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Event_t, ttrait_utf8cstring, &NdbDictionary::Event::addEventColumn >(env, obj, p0);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Table_ct, ttrait_c_m_n_n_NdbDictionary_Column_cp, ttrait_utf8cstring, &NdbDictionary::Table::getColumn >(env, obj, p0); // call of overloaded const/non-const method
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
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Table_ct, ttrait_c_m_n_n_NdbDictionary_Column_cp, ttrait_int, &NdbDictionary::Table::getColumn >(env, obj, p0); // call of overloaded const/non-const method
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
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Table_t, ttrait_c_m_n_n_NdbDictionary_Column_p, ttrait_int, &NdbDictionary::Table::getColumn >(env, obj, p0); // call of overloaded const/non-const method
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
    return gcall_mfr< ttrait_c_m_n_n_NdbDictionary_Table_t, ttrait_c_m_n_n_NdbDictionary_Column_p, ttrait_utf8cstring, &NdbDictionary::Table::getColumn >(env, obj, p0); // call of overloaded const/non-const method
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
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Tablespace_t, ttrait_utf8cstring, &NdbDictionary::Tablespace::setDefaultLogfileGroup >(env, obj, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Tablespace
 * Method:    setDefaultLogfileGroup
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/LogfileGroupConst;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Tablespace_setDefaultLogfileGroup__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_LogfileGroupConst_2(JNIEnv * env, jobject obj, jobject p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Tablespace_setDefaultLogfileGroup__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_LogfileGroupConst_2(JNIEnv *, jobject, jobject)");
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Tablespace_t, ttrait_c_m_n_n_NdbDictionary_LogfileGroup_cr, &NdbDictionary::Tablespace::setDefaultLogfileGroup >(env, obj, p0);
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
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Undofile_t, ttrait_utf8cstring, &NdbDictionary::Undofile::setLogfileGroup >(env, obj, p0);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbDictionary_Undofile
 * Method:    setLogfileGroup
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/LogfileGroupConst;)V
 */
JNIEXPORT void JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Undofile_setLogfileGroup__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_LogfileGroupConst_2(JNIEnv * env, jobject obj, jobject p0)
{
    TRACE("void Java_com_mysql_ndbjtie_ndbapi_NdbDictionary_00024Undofile_setLogfileGroup__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_LogfileGroupConst_2(JNIEnv *, jobject, jobject)");
    gcall_mfv< ttrait_c_m_n_n_NdbDictionary_Undofile_t, ttrait_c_m_n_n_NdbDictionary_LogfileGroup_cr, &NdbDictionary::Undofile::setLogfileGroup >(env, obj, p0);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbIndexOperation_t, ttrait_int, ttrait_c_m_n_n_NdbOperation_LockMode_iv/*_enum_*/, &NdbIndexOperation::readTuple >(env, obj, p0);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbIndexScanOperation_t, ttrait_int, ttrait_c_m_n_n_NdbOperation_LockMode_iv/*_enum_*/, ttrait_Uint32, ttrait_Uint32, ttrait_Uint32, &NdbIndexScanOperation::readTuples >(env, obj, p0, p1, p2, p3);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbIndexScanOperation_t, ttrait_int, ttrait_utf8cstring, ttrait_int, ttrait_void_1cp_bb, &NdbIndexScanOperation::setBound >(env, obj, p0, p1, p2);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbIndexScanOperation_t, ttrait_int, ttrait_Uint32, ttrait_int, ttrait_void_1cp_bb, &NdbIndexScanOperation::setBound >(env, obj, p0, p1, p2);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation
 * Method:    setBound
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbRecordConst;Lcom/mysql/ndbjtie/ndbapi/NdbIndexScanOperation/IndexBoundConst;)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_setBound__Lcom_mysql_ndbjtie_ndbapi_NdbRecordConst_2Lcom_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_IndexBoundConst_2(JNIEnv * env, jobject obj, jobject p0, jobject p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_setBound__Lcom_mysql_ndbjtie_ndbapi_NdbRecordConst_2Lcom_mysql_ndbjtie_ndbapi_NdbIndexScanOperation_IndexBoundConst_2(JNIEnv *, jobject, jobject, jobject)");
    return gcall_mfr< ttrait_c_m_n_n_NdbIndexScanOperation_t, ttrait_int, ttrait_c_m_n_n_NdbRecord_cp, ttrait_c_m_n_n_NdbIndexScanOperation_IndexBound_cr, &NdbIndexScanOperation::setBound >(env, obj, p0, p1);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbInterpretedCode_t, ttrait_int, ttrait_Uint32, ttrait_Uint32, &NdbInterpretedCode::read_attr >(env, obj, p0, p1);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbInterpretedCode
 * Method:    read_attr
 * Signature: (ILcom/mysql/ndbjtie/ndbapi/NdbDictionary/ColumnConst;)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_read_1attr__ILcom_mysql_ndbjtie_ndbapi_NdbDictionary_ColumnConst_2(JNIEnv * env, jobject obj, jint p0, jobject p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_read_1attr__ILcom_mysql_ndbjtie_ndbapi_NdbDictionary_ColumnConst_2(JNIEnv *, jobject, jint, jobject)");
    return gcall_mfr< ttrait_c_m_n_n_NdbInterpretedCode_t, ttrait_int, ttrait_Uint32, ttrait_c_m_n_n_NdbDictionary_Column_cp, &NdbInterpretedCode::read_attr >(env, obj, p0, p1);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbInterpretedCode_t, ttrait_int, ttrait_Uint32, ttrait_Uint32, &NdbInterpretedCode::write_attr >(env, obj, p0, p1);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbInterpretedCode
 * Method:    write_attr
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/ColumnConst;I)I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_write_1attr__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_ColumnConst_2I(JNIEnv * env, jobject obj, jobject p0, jint p1)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbInterpretedCode_write_1attr__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_ColumnConst_2I(JNIEnv *, jobject, jobject, jint)");
    return gcall_mfr< ttrait_c_m_n_n_NdbInterpretedCode_t, ttrait_int, ttrait_c_m_n_n_NdbDictionary_Column_cp, ttrait_Uint32, &NdbInterpretedCode::write_attr >(env, obj, p0, p1);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbInterpretedCode_t, ttrait_int, ttrait_Uint32, &NdbInterpretedCode::interpret_exit_nok >(env, obj, p0);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbInterpretedCode_t, ttrait_int, &NdbInterpretedCode::interpret_exit_nok >(env, obj);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbInterpretedCode_t, ttrait_int, ttrait_Uint32, ttrait_Uint32, &NdbInterpretedCode::add_val >(env, obj, p0, p1);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbInterpretedCode_t, ttrait_int, ttrait_Uint32, ttrait_Uint64, &NdbInterpretedCode::add_val >(env, obj, p0, p1);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbInterpretedCode_t, ttrait_int, ttrait_Uint32, ttrait_Uint32, &NdbInterpretedCode::sub_val >(env, obj, p0, p1);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbInterpretedCode_t, ttrait_int, ttrait_Uint32, ttrait_Uint64, &NdbInterpretedCode::sub_val >(env, obj, p0, p1);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_ct, ttrait_int, &NdbOperation::getNdbErrorLine >(env, obj);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_t, ttrait_int, ttrait_c_m_n_n_NdbOperation_LockMode_iv/*_enum_*/, &NdbOperation::readTuple >(env, obj, p0);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_t, ttrait_int, ttrait_utf8cstring, ttrait_char_1cp_bb, &NdbOperation::equal >(env, obj, p0, p1);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_t, ttrait_int, ttrait_utf8cstring, ttrait_Int32, &NdbOperation::equal >(env, obj, p0, p1);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_t, ttrait_int, ttrait_utf8cstring, ttrait_Int64, &NdbOperation::equal >(env, obj, p0, p1);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_t, ttrait_int, ttrait_Uint32, ttrait_char_1cp_bb, &NdbOperation::equal >(env, obj, p0, p1);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_t, ttrait_int, ttrait_Uint32, ttrait_Int32, &NdbOperation::equal >(env, obj, p0, p1);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_t, ttrait_int, ttrait_Uint32, ttrait_Int64, &NdbOperation::equal >(env, obj, p0, p1);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_t, ttrait_c_m_n_n_NdbRecAttr_p, ttrait_utf8cstring, ttrait_char_1p_bb, &NdbOperation::getValue >(env, obj, p0, p1);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_t, ttrait_c_m_n_n_NdbRecAttr_p, ttrait_Uint32, ttrait_char_1p_bb, &NdbOperation::getValue >(env, obj, p0, p1);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbOperation
 * Method:    getValue
 * Signature: (Lcom/mysql/ndbjtie/ndbapi/NdbDictionary/ColumnConst;Ljava/nio/ByteBuffer;)Lcom/mysql/ndbjtie/ndbapi/NdbRecAttr;
 */
JNIEXPORT jobject JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbOperation_getValue__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_ColumnConst_2Ljava_nio_ByteBuffer_2(JNIEnv * env, jobject obj, jobject p0, jobject p1)
{
    TRACE("jobject Java_com_mysql_ndbjtie_ndbapi_NdbOperation_getValue__Lcom_mysql_ndbjtie_ndbapi_NdbDictionary_ColumnConst_2Ljava_nio_ByteBuffer_2(JNIEnv *, jobject, jobject, jobject)");
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_t, ttrait_c_m_n_n_NdbRecAttr_p, ttrait_c_m_n_n_NdbDictionary_Column_cp, ttrait_char_1p_bb, &NdbOperation::getValue >(env, obj, p0, p1);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_t, ttrait_int, ttrait_utf8cstring, ttrait_char_1cp_bb, &NdbOperation::setValue >(env, obj, p0, p1);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_t, ttrait_int, ttrait_utf8cstring, ttrait_Int32, &NdbOperation::setValue >(env, obj, p0, p1);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_t, ttrait_int, ttrait_utf8cstring, ttrait_Int64, &NdbOperation::setValue >(env, obj, p0, p1);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_t, ttrait_int, ttrait_utf8cstring, ttrait_float, &NdbOperation::setValue >(env, obj, p0, p1);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_t, ttrait_int, ttrait_utf8cstring, ttrait_double, &NdbOperation::setValue >(env, obj, p0, p1);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_t, ttrait_int, ttrait_Uint32, ttrait_char_1cp_bb, &NdbOperation::setValue >(env, obj, p0, p1);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_t, ttrait_int, ttrait_Uint32, ttrait_Int32, &NdbOperation::setValue >(env, obj, p0, p1);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_t, ttrait_int, ttrait_Uint32, ttrait_Int64, &NdbOperation::setValue >(env, obj, p0, p1);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_t, ttrait_int, ttrait_Uint32, ttrait_float, &NdbOperation::setValue >(env, obj, p0, p1);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbOperation_t, ttrait_int, ttrait_Uint32, ttrait_double, &NdbOperation::setValue >(env, obj, p0, p1);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbScanFilter_t, ttrait_int, ttrait_int, ttrait_Uint32, &NdbScanFilter::eq >(env, obj, p0, p1);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbScanFilter_t, ttrait_int, ttrait_int, ttrait_Uint32, &NdbScanFilter::ne >(env, obj, p0, p1);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbScanFilter_t, ttrait_int, ttrait_int, ttrait_Uint32, &NdbScanFilter::lt >(env, obj, p0, p1);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbScanFilter_t, ttrait_int, ttrait_int, ttrait_Uint32, &NdbScanFilter::le >(env, obj, p0, p1);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbScanFilter_t, ttrait_int, ttrait_int, ttrait_Uint32, &NdbScanFilter::gt >(env, obj, p0, p1);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbScanFilter_t, ttrait_int, ttrait_int, ttrait_Uint32, &NdbScanFilter::ge >(env, obj, p0, p1);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbScanFilter_t, ttrait_int, ttrait_int, ttrait_Uint64, &NdbScanFilter::eq >(env, obj, p0, p1);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbScanFilter_t, ttrait_int, ttrait_int, ttrait_Uint64, &NdbScanFilter::ne >(env, obj, p0, p1);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbScanFilter_t, ttrait_int, ttrait_int, ttrait_Uint64, &NdbScanFilter::lt >(env, obj, p0, p1);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbScanFilter_t, ttrait_int, ttrait_int, ttrait_Uint64, &NdbScanFilter::le >(env, obj, p0, p1);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbScanFilter_t, ttrait_int, ttrait_int, ttrait_Uint64, &NdbScanFilter::gt >(env, obj, p0, p1);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbScanFilter_t, ttrait_int, ttrait_int, ttrait_Uint64, &NdbScanFilter::ge >(env, obj, p0, p1);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbScanOperation_t, ttrait_int, ttrait_c_m_n_n_NdbOperation_LockMode_iv/*_enum_*/, ttrait_Uint32, ttrait_Uint32, ttrait_Uint32, &NdbScanOperation::readTuples >(env, obj, p0, p1, p2, p3);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbScanOperation_t, ttrait_int, ttrait_bool, ttrait_bool, &NdbScanOperation::nextResult >(env, obj, p0, p1);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbScanOperation_t, ttrait_c_m_n_n_NdbOperation_p, &NdbScanOperation::lockCurrentTuple >(env, obj);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbScanOperation_t, ttrait_c_m_n_n_NdbOperation_p, ttrait_c_m_n_n_NdbTransaction_p, &NdbScanOperation::lockCurrentTuple >(env, obj, p0);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbScanOperation_t, ttrait_c_m_n_n_NdbOperation_p, &NdbScanOperation::updateCurrentTuple >(env, obj);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbScanOperation_t, ttrait_c_m_n_n_NdbOperation_p, ttrait_c_m_n_n_NdbTransaction_p, &NdbScanOperation::updateCurrentTuple >(env, obj, p0);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbScanOperation_t, ttrait_int, &NdbScanOperation::deleteCurrentTuple >(env, obj);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbScanOperation_t, ttrait_int, ttrait_c_m_n_n_NdbTransaction_p, &NdbScanOperation::deleteCurrentTuple >(env, obj, p0);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbTransaction_ct, ttrait_c_m_n_n_NdbOperation_cp, &NdbTransaction::getNdbErrorOperation >(env, obj); // call of overloaded const/non-const method
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
    return gcall_mfr< ttrait_c_m_n_n_NdbTransaction_t, ttrait_c_m_n_n_NdbOperation_p, ttrait_c_m_n_n_NdbDictionary_Table_cp, &NdbTransaction::getNdbOperation >(env, obj, p0);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbTransaction_t, ttrait_c_m_n_n_NdbScanOperation_p, ttrait_c_m_n_n_NdbDictionary_Table_cp, &NdbTransaction::getNdbScanOperation >(env, obj, p0);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbTransaction_t, ttrait_c_m_n_n_NdbIndexScanOperation_p, ttrait_c_m_n_n_NdbDictionary_Index_cp, &NdbTransaction::getNdbIndexScanOperation >(env, obj, p0);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbTransaction_t, ttrait_c_m_n_n_NdbIndexOperation_p, ttrait_c_m_n_n_NdbDictionary_Index_cp, &NdbTransaction::getNdbIndexOperation >(env, obj, p0);
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
    return gcall_mfr< ttrait_c_m_n_n_NdbTransaction_t, ttrait_int, ttrait_c_m_n_n_NdbTransaction_ExecType_iv/*_enum_*/, ttrait_c_m_n_n_NdbOperation_AbortOption_iv/*_enum_*/, ttrait_int, &NdbTransaction::execute >(env, obj, p0, p1, p2);
}

/*
 * Class:     com_mysql_ndbjtie_ndbapi_NdbTransaction
 * Method:    getGCI
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_com_mysql_ndbjtie_ndbapi_NdbTransaction_getGCI__(JNIEnv * env, jobject obj)
{
    TRACE("jint Java_com_mysql_ndbjtie_ndbapi_NdbTransaction_getGCI__(JNIEnv *, jobject)");
    return gcall_mfr< ttrait_c_m_n_n_NdbTransaction_t, ttrait_int, &NdbTransaction::getGCI >(env, obj);
}

#endif // XXXXX ToDo: need to define wrappers for these functions

};

#endif // ndbapi_wrapper_hpp
