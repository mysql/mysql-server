/*
 Copyright (c) 2010, 2024, Oracle and/or its affiliates.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2.0,
 as published by the Free Software Foundation.

 This program is designed to work with certain software (including
 but not limited to OpenSSL) that is licensed under separate terms,
 as designated in a particular file or component or in included license
 documentation.  The authors of MySQL hereby grant you an additional
 permission to link the program and your derivative works with the
 separately licensed software that they have either included with
 the program or referenced in the documentation.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/
/*
 * ndbjtie_lib.cpp
 */

// libraries
#include "helpers.hpp"

// global jtie library definitions
#include "jtie.hpp"
#include "jtie_lib.hpp"

// ndb client library initialization
#include "CharsetMap.hpp"
#include "ndb_init.h"

// global ndb client library definitions
#include <mutex>
#include "mgmapi_jtie.hpp"
#include "mysql_utils_jtie.hpp"
#include "ndbapi_jtie.hpp"

// ---------------------------------------------------------------------------
// API Global Symbol Definitions & Template Instantiations
// ---------------------------------------------------------------------------

JTIE_INSTANTIATE_PEER_CLASS_MAPPING(c_m_n_m_CharsetMap,
                                    "com/mysql/ndbjtie/mysql/CharsetMap")

// ---------------------------------------------------------------------------

JTIE_INSTANTIATE_PEER_CLASS_MAPPING(c_m_n_n_Ndb, "com/mysql/ndbjtie/ndbapi/Ndb")
JTIE_INSTANTIATE_PEER_CLASS_MAPPING(c_m_n_n_NdbBlob,
                                    "com/mysql/ndbjtie/ndbapi/NdbBlob")
JTIE_INSTANTIATE_PEER_CLASS_MAPPING(c_m_n_n_NdbDictionary,
                                    "com/mysql/ndbjtie/ndbapi/NdbDictionary")
JTIE_INSTANTIATE_PEER_CLASS_MAPPING(
    c_m_n_n_NdbDictionary_AutoGrowSpecification,
    "com/mysql/ndbjtie/ndbapi/NdbDictionary$AutoGrowSpecification")
JTIE_INSTANTIATE_PEER_CLASS_MAPPING(
    c_m_n_n_NdbDictionary_Column,
    "com/mysql/ndbjtie/ndbapi/NdbDictionary$Column")
JTIE_INSTANTIATE_PEER_CLASS_MAPPING(
    c_m_n_n_NdbDictionary_Datafile,
    "com/mysql/ndbjtie/ndbapi/NdbDictionary$Datafile")
JTIE_INSTANTIATE_PEER_CLASS_MAPPING(
    c_m_n_n_NdbDictionary_Dictionary,
    "com/mysql/ndbjtie/ndbapi/NdbDictionary$Dictionary")
JTIE_INSTANTIATE_PEER_CLASS_MAPPING(
    c_m_n_n_NdbDictionary_DictionaryConst_List,
    "com/mysql/ndbjtie/ndbapi/NdbDictionary$DictionaryConst$List")
JTIE_INSTANTIATE_PEER_CLASS_MAPPING(
    c_m_n_n_NdbDictionary_DictionaryConst_ListConst_Element,
    "com/mysql/ndbjtie/ndbapi/NdbDictionary$DictionaryConst$ListConst$Element")
JTIE_INSTANTIATE_PEER_CLASS_MAPPING(
    c_m_n_n_NdbDictionary_DictionaryConst_ListConst_ElementArray,
    "com/mysql/ndbjtie/ndbapi/"
    "NdbDictionary$DictionaryConst$ListConst$ElementArray")
JTIE_INSTANTIATE_PEER_CLASS_MAPPING(
    c_m_n_n_NdbDictionary_Event, "com/mysql/ndbjtie/ndbapi/NdbDictionary$Event")
JTIE_INSTANTIATE_PEER_CLASS_MAPPING(
    c_m_n_n_NdbDictionary_Index, "com/mysql/ndbjtie/ndbapi/NdbDictionary$Index")
JTIE_INSTANTIATE_PEER_CLASS_MAPPING(
    c_m_n_n_NdbDictionary_LogfileGroup,
    "com/mysql/ndbjtie/ndbapi/NdbDictionary$LogfileGroup")
JTIE_INSTANTIATE_PEER_CLASS_MAPPING(
    c_m_n_n_NdbDictionary_Object,
    "com/mysql/ndbjtie/ndbapi/NdbDictionary$Object")
JTIE_INSTANTIATE_PEER_CLASS_MAPPING(
    c_m_n_n_NdbDictionary_ObjectId,
    "com/mysql/ndbjtie/ndbapi/NdbDictionary$ObjectId")
JTIE_INSTANTIATE_PEER_CLASS_MAPPING(
    c_m_n_n_NdbDictionary_OptimizeIndexHandle,
    "com/mysql/ndbjtie/ndbapi/NdbDictionary$OptimizeIndexHandle")
JTIE_INSTANTIATE_PEER_CLASS_MAPPING(
    c_m_n_n_NdbDictionary_OptimizeTableHandle,
    "com/mysql/ndbjtie/ndbapi/NdbDictionary$OptimizeTableHandle")
JTIE_INSTANTIATE_PEER_CLASS_MAPPING(
    c_m_n_n_NdbDictionary_RecordSpecification,
    "com/mysql/ndbjtie/ndbapi/NdbDictionary$RecordSpecification")
JTIE_INSTANTIATE_PEER_CLASS_MAPPING(
    c_m_n_n_NdbDictionary_RecordSpecificationArray,
    "com/mysql/ndbjtie/ndbapi/NdbDictionary$RecordSpecificationArray")
JTIE_INSTANTIATE_PEER_CLASS_MAPPING(
    c_m_n_n_NdbDictionary_Table, "com/mysql/ndbjtie/ndbapi/NdbDictionary$Table")
JTIE_INSTANTIATE_PEER_CLASS_MAPPING(
    c_m_n_n_NdbDictionary_Tablespace,
    "com/mysql/ndbjtie/ndbapi/NdbDictionary$Tablespace")
JTIE_INSTANTIATE_PEER_CLASS_MAPPING(
    c_m_n_n_NdbDictionary_Undofile,
    "com/mysql/ndbjtie/ndbapi/NdbDictionary$Undofile")
JTIE_INSTANTIATE_PEER_CLASS_MAPPING(c_m_n_n_NdbError,
                                    "com/mysql/ndbjtie/ndbapi/NdbError")
JTIE_INSTANTIATE_PEER_CLASS_MAPPING(
    c_m_n_n_NdbEventOperation, "com/mysql/ndbjtie/ndbapi/NdbEventOperation")
JTIE_INSTANTIATE_PEER_CLASS_MAPPING(
    c_m_n_n_NdbIndexOperation, "com/mysql/ndbjtie/ndbapi/NdbIndexOperation")
JTIE_INSTANTIATE_PEER_CLASS_MAPPING(
    c_m_n_n_NdbIndexScanOperation,
    "com/mysql/ndbjtie/ndbapi/NdbIndexScanOperation")
JTIE_INSTANTIATE_PEER_CLASS_MAPPING(
    c_m_n_n_NdbIndexScanOperation_IndexBound,
    "com/mysql/ndbjtie/ndbapi/NdbIndexScanOperation$IndexBound")
JTIE_INSTANTIATE_PEER_CLASS_MAPPING(
    c_m_n_n_NdbInterpretedCode, "com/mysql/ndbjtie/ndbapi/NdbInterpretedCode")
JTIE_INSTANTIATE_PEER_CLASS_MAPPING(c_m_n_n_NdbLockHandle,
                                    "com/mysql/ndbjtie/ndbapi/NdbLockHandle")
JTIE_INSTANTIATE_PEER_CLASS_MAPPING(c_m_n_n_NdbOperation,
                                    "com/mysql/ndbjtie/ndbapi/NdbOperation")
JTIE_INSTANTIATE_PEER_CLASS_MAPPING(
    c_m_n_n_NdbOperation_GetValueSpec,
    "com/mysql/ndbjtie/ndbapi/NdbOperation$GetValueSpec")
JTIE_INSTANTIATE_PEER_CLASS_MAPPING(
    c_m_n_n_NdbOperation_GetValueSpecArray,
    "com/mysql/ndbjtie/ndbapi/NdbOperation$GetValueSpecArray")
JTIE_INSTANTIATE_PEER_CLASS_MAPPING(
    c_m_n_n_NdbOperation_OperationOptions,
    "com/mysql/ndbjtie/ndbapi/NdbOperation$OperationOptions")
JTIE_INSTANTIATE_PEER_CLASS_MAPPING(
    c_m_n_n_NdbOperation_SetValueSpec,
    "com/mysql/ndbjtie/ndbapi/NdbOperation$SetValueSpec")
JTIE_INSTANTIATE_PEER_CLASS_MAPPING(
    c_m_n_n_NdbOperation_SetValueSpecArray,
    "com/mysql/ndbjtie/ndbapi/NdbOperation$SetValueSpecArray")
JTIE_INSTANTIATE_PEER_CLASS_MAPPING(c_m_n_n_NdbRecAttr,
                                    "com/mysql/ndbjtie/ndbapi/NdbRecAttr")
JTIE_INSTANTIATE_PEER_CLASS_MAPPING(c_m_n_n_NdbRecord,
                                    "com/mysql/ndbjtie/ndbapi/NdbRecord")
JTIE_INSTANTIATE_PEER_CLASS_MAPPING(c_m_n_n_NdbScanFilter,
                                    "com/mysql/ndbjtie/ndbapi/NdbScanFilter")
JTIE_INSTANTIATE_PEER_CLASS_MAPPING(c_m_n_n_NdbScanOperation,
                                    "com/mysql/ndbjtie/ndbapi/NdbScanOperation")
JTIE_INSTANTIATE_PEER_CLASS_MAPPING(
    c_m_n_n_NdbScanOperation_ScanOptions,
    "com/mysql/ndbjtie/ndbapi/NdbScanOperation$ScanOptions")
JTIE_INSTANTIATE_PEER_CLASS_MAPPING(c_m_n_n_NdbTransaction,
                                    "com/mysql/ndbjtie/ndbapi/NdbTransaction")
JTIE_INSTANTIATE_PEER_CLASS_MAPPING(c_m_n_n_Ndb_Key_part_ptr,
                                    "com/mysql/ndbjtie/ndbapi/Ndb$Key_part_ptr")
JTIE_INSTANTIATE_PEER_CLASS_MAPPING(
    c_m_n_n_Ndb_Key_part_ptrArray,
    "com/mysql/ndbjtie/ndbapi/Ndb$Key_part_ptrArray")
JTIE_INSTANTIATE_PEER_CLASS_MAPPING(
    c_m_n_n_Ndb_cluster_connection,
    "com/mysql/ndbjtie/ndbapi/Ndb_cluster_connection")

// ---------------------------------------------------------------------------

JTIE_INSTANTIATE_JINT_ENUM_TYPE_MAPPING(NdbBlob::State)
JTIE_INSTANTIATE_JINT_ENUM_TYPE_MAPPING(NdbDictionary::Object::Status)
JTIE_INSTANTIATE_JINT_ENUM_TYPE_MAPPING(NdbDictionary::Object::Type)
JTIE_INSTANTIATE_JINT_ENUM_TYPE_MAPPING(NdbDictionary::Object::State)
JTIE_INSTANTIATE_JINT_ENUM_TYPE_MAPPING(NdbDictionary::Object::Store)
JTIE_INSTANTIATE_JINT_ENUM_TYPE_MAPPING(NdbDictionary::Object::FragmentType)
JTIE_INSTANTIATE_JINT_ENUM_TYPE_MAPPING(NdbDictionary::Column::Type)
JTIE_INSTANTIATE_JINT_ENUM_TYPE_MAPPING(NdbDictionary::Column::ArrayType)
JTIE_INSTANTIATE_JINT_ENUM_TYPE_MAPPING(NdbDictionary::Column::StorageType)
JTIE_INSTANTIATE_JINT_ENUM_TYPE_MAPPING(NdbDictionary::Table::SingleUserMode)
JTIE_INSTANTIATE_JINT_ENUM_TYPE_MAPPING(NdbDictionary::Index::Type)
JTIE_INSTANTIATE_JINT_ENUM_TYPE_MAPPING(NdbDictionary::Event::TableEvent)
JTIE_INSTANTIATE_JINT_ENUM_TYPE_MAPPING(NdbDictionary::Event::EventDurability)
JTIE_INSTANTIATE_JINT_ENUM_TYPE_MAPPING(NdbDictionary::Event::EventReport)
JTIE_INSTANTIATE_JINT_ENUM_TYPE_MAPPING(NdbDictionary::NdbRecordFlags)
JTIE_INSTANTIATE_JINT_ENUM_TYPE_MAPPING(NdbDictionary::RecordType)
JTIE_INSTANTIATE_JINT_ENUM_TYPE_MAPPING(NdbError::Status)
JTIE_INSTANTIATE_JINT_ENUM_TYPE_MAPPING(NdbError::Classification)
JTIE_INSTANTIATE_JINT_ENUM_TYPE_MAPPING(NdbEventOperation::State)
JTIE_INSTANTIATE_JINT_ENUM_TYPE_MAPPING(NdbIndexScanOperation::BoundType)
JTIE_INSTANTIATE_JINT_ENUM_TYPE_MAPPING(NdbOperation::Type)
JTIE_INSTANTIATE_JINT_ENUM_TYPE_MAPPING(NdbOperation::LockMode)
JTIE_INSTANTIATE_JINT_ENUM_TYPE_MAPPING(NdbOperation::AbortOption)
JTIE_INSTANTIATE_JINT_ENUM_TYPE_MAPPING(NdbOperation::OperationOptions::Flags)
JTIE_INSTANTIATE_JINT_ENUM_TYPE_MAPPING(NdbScanFilter::Group)
JTIE_INSTANTIATE_JINT_ENUM_TYPE_MAPPING(NdbScanFilter::BinaryCondition)
JTIE_INSTANTIATE_JINT_ENUM_TYPE_MAPPING(NdbScanFilter::Error)
JTIE_INSTANTIATE_JINT_ENUM_TYPE_MAPPING(NdbScanOperation::ScanFlag)
JTIE_INSTANTIATE_JINT_ENUM_TYPE_MAPPING(NdbScanOperation::ScanOptions::Type)
JTIE_INSTANTIATE_JINT_ENUM_TYPE_MAPPING(NdbTransaction::ExecType)
JTIE_INSTANTIATE_JINT_ENUM_TYPE_MAPPING(NdbTransaction::CommitStatusType)

class JTie_NdbInit {
 public:
  int initOnLoad() {
    m_mutex.lock();
    if (!is_init) {
      VERBOSE("initializing the NDBAPI resources ...");
      int stat = ndb_init();
      if (stat != 0) {
        PRINT_ERROR_CODE("ndb_init() returned: ", stat);
        ndb_end(0);
        m_mutex.unlock();
        return 1;
      }
      VERBOSE("... initialized the NDBAPI resources");
      VERBOSE("initializing the MySQL Utilities resources ...");
      CharsetMap::init();
      VERBOSE("... initialized the MySQL Utilities resources");
      is_init = true;
    }
    m_mutex.unlock();
    return 0;
  }

  ~JTie_NdbInit() { uninitOnUnLoad(); }

 private:
  static bool is_init;
  static std::mutex m_mutex;

  void uninitOnUnLoad() {
    m_mutex.lock();
    if (is_init) {
      VERBOSE("releasing the MySQL Utilities resources ...");
      CharsetMap::unload();
      VERBOSE("... released the MySQL Utilities resources");

      VERBOSE("releasing NDBAPI resources ...");
      ndb_end(0);
      VERBOSE("... released NDBAPI resources");
      is_init = false;
    }
    m_mutex.unlock();
  }
};

bool JTie_NdbInit::is_init = false;
std::mutex JTie_NdbInit::m_mutex;
static JTie_NdbInit ndbInitHelper;

// ---------------------------------------------------------------------------
// Library Load and Unload Handlers
// ---------------------------------------------------------------------------

// initialize the NDB interface and JTie resources
// called when the native library is loaded; returns the JNI version
// needed by the native library or JNI_ERR.
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *jvm, void *reserved) {
  TRACE("jint JNI_OnLoad(JavaVM *, void *)");
  VERBOSE("loading the NDB JTie library ...");

  const jint required_jni_version = JTie_OnLoad(jvm, reserved);
  if (required_jni_version == JNI_ERR) {
    PRINT_ERROR("JTie_OnLoad() returned: JNI_ERR");
    return JNI_ERR;
  }

  if (ndbInitHelper.initOnLoad()) {
    return JNI_ERR;
  }

  VERBOSE("... loaded the NDB JTie library");
  return required_jni_version;
}

// called when the class loader containing the native library is garbage
// collected; called in an unknown context (such as from a finalizer):
// be conservative, and refrain from arbitrary Java call-backs.
JNIEXPORT void JNICALL JNI_OnUnload(JavaVM *jvm, void *reserved) {
  TRACE("void JNI_OnUnload(JavaVM *, void *)");
  VERBOSE("unloading the NDB JTie library...");

  JTie_OnUnload(jvm, reserved);

  VERBOSE("... unloaded the NDB JTie library");
}

// ---------------------------------------------------------------------------

/*
  Dummy function with constant signature to be used by parent library
  to make sure that the linker includes the functions from this module
*/
extern "C" void _ndbjtie_exports(void) { return; }
