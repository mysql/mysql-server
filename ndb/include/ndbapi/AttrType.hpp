/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/**
 * @file AttrType.hpp
 */

#ifndef AttrType_H
#define AttrType_H

/**
 * Max number of Ndb objects in different threads.  
 * (Ndb objects should not be shared by different threads.)
 */
const unsigned MAX_NO_THREADS = 4711;

/**
 * Max number of attributes in a table.
 */
const unsigned MAXNROFATTRIBUTES = 128;

/**
 * Max number of tuple keys for a table in NDB Cluster.
 * 
 * A <em>tuple key</em> of a table is an attribute 
 * which is either part of the 
 * <em>primary key</em> or the <em>tuple id</em> of a table.
 */
const unsigned MAXNROFTUPLEKEY = 16;

/** 
 * Max number of words in a tuple key attribute.
 *
 * Tuple keys can not have values larger than
 * 4092 bytes (i.e. 1023 words).
 */
const unsigned MAXTUPLEKEYLENOFATTERIBUTEINWORD = 1023;

/**
 * Max number of ErrorCode in NDB Cluster range 0 - 1999.
 */
const unsigned MAXNDBCLUSTERERROR = 1999;	

/**
 * Max number of theErrorCode NDB API range 4000 - 4999.
 */
const unsigned MAXNROFERRORCODE	= 5000;

/**
 * <i>Missing explanation</i>
 */
enum ReturnType { 
  ReturnSuccess,                ///< <i>Missing explanation</i>
  ReturnFailure                 ///< <i>Missing explanation</i>
};

/**
 * 
 */
enum SendStatusType {     
  NotInit,                      ///< <i>Missing explanation</i>
  InitState,                    ///< <i>Missing explanation</i>
  sendOperations,               ///< <i>Missing explanation</i>
  sendCompleted,                ///< <i>Missing explanation</i>
  sendCOMMITstate,              ///< <i>Missing explanation</i>
  sendABORT,                    ///< <i>Missing explanation</i>
  sendABORTfail,                ///< <i>Missing explanation</i>
  sendTC_ROLLBACK,              ///< <i>Missing explanation</i>
  sendTC_COMMIT,                ///< <i>Missing explanation</i>
  sendTC_OP                     ///< <i>Missing explanation</i>
};

/**
 * <i>Missing explanation</i>
 */
enum ListState { 
  NotInList,                    ///< <i>Missing explanation</i>
  InPreparedList,               ///< <i>Missing explanation</i>
  InSendList,                   ///< <i>Missing explanation</i>
  InCompletedList               ///< <i>Missing explanation</i>
};

/**
 * Commit status of the transaction
 */
enum CommitStatusType { 
  NotStarted,                   ///< Transaction not yet started
  Started,                      ///< <i>Missing explanation</i>
  Committed,                    ///< Transaction has been committed
  Aborted,                      ///< Transaction has been aborted
  NeedAbort                     ///< <i>Missing explanation</i>
};

/**
 * Commit type of transaction
 */
enum AbortOption {      
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  CommitIfFailFree = 0,         
  CommitAsMuchAsPossible = 2,   ///< Commit transaction with as many 
  TryCommit = 0,                ///< <i>Missing explanation</i>
#endif
  AbortOnError = 0,             ///< Abort transaction on failed operation
  IgnoreError = 2               ///< Transaction continues on failed operation
};

typedef AbortOption CommitType;

/**
 * <i>Missing explanation</i>
 */
enum InitType {
  NotConstructed,               ///< <i>Missing explanation</i>
  NotInitialised,               ///< <i>Missing explanation</i>
  StartingInit,                 ///< <i>Missing explanation</i>
  Initialised,                  ///< <i>Missing explanation</i>
  InitConfigError               ///< <i>Missing explanation</i>
};

/**
 * Type of attribute
 */
enum AttrType { 
  Signed,                       ///< Attributes of this type can be read with:
                                ///< NdbRecAttr::int64_value, 
                                ///< NdbRecAttr::int32_value,
                                ///< NdbRecAttr::short_value, 
                                ///< NdbRecAttr::char_value
  UnSigned,                     ///< Attributes of this type can be read with:
                                ///< NdbRecAttr::u_64_value, 
                                ///< NdbRecAttr::u_32_value,
                                ///< NdbRecAttr::u_short_value, 
                                ///< NdbRecAttr::u_char_value
  Float,                        ///< Attributes of this type can be read with:
                                ///< NdbRecAttr::float_value and 
                                ///< NdbRecAttr::double_value
  String,                       ///< Attributes of this type can be read with:
                                ///< NdbRecAttr::aRef, 
                                ///< NdbRecAttr::getAttributeObject
  NoAttrTypeDef                 ///< Used for debugging only
};

/**
 * Execution type of transaction
 */
enum ExecType { 
  NoExecTypeDef = -1,           ///< Erroneous type (Used for debugging only)
  Prepare,                      ///< <i>Missing explanation</i>
  NoCommit,                     ///< Execute the transaction as far as it has 
                                ///< been defined, but do not yet commit it
  Commit,                       ///< Execute and try to commit the transaction
  Rollback                      ///< Rollback transaction
};

/**
 * Indicates whether the attribute is part of a primary key or not
 */
enum KeyType { 
  Undefined = -1,               ///< Used for debugging only
  NoKey,                        ///< Attribute is not part of primary key 
                                ///< or tuple identity
  TupleKey,                     ///< Attribute is part of primary key
  TupleId                       ///< Attribute is part of tuple identity 
  ///< (This type of attribute is created 
                                ///< internally, and should not be 
                                ///< manually created.)
};

/**
 * Indicate whether the attribute should be stored on disk or not
 */
enum StorageMode { 
  MMBased = 0,                  ///< Main memory 
  DiskBased = 1,                ///< Disk (Not yet supported.)
  NoStorageTypeDef              ///< Used for debugging only
};

/**
 * Where attribute is stored.
 *
 * This is used to indicate whether a primary key
 * should only be stored in the index storage and not in the data storage
 * or if it should be stored in both places.
 * The first alternative makes the attribute take less space, 
 * but makes it impossible to scan using attribute.
 *
 * @note  Use NormalStorageAttribute for most cases.
 *        (IndexStorageAttribute should only be used on primary key 
 *        attributes and only if you do not want to scan using the attribute.)
 */
enum StorageAttributeType { 
  NoStorageAttributeTypeDefined = -1,  ///< <i>Missing explanation</i>
  IndexStorageAttribute,               ///< Attribute is only stored in 
                                       ///< index storage (ACC)
  NormalStorageAttribute               ///< Attribute values are stored 
                                       ///< both in the index (ACC) and 
                                       ///< in the data storage (TUP)
};
	
/**
 * <i>Missing explanation</i>
 */
enum OperationStatus{ 
  Init,                         ///< <i>Missing explanation</i>
  OperationDefined,             ///< <i>Missing explanation</i>
  TupleKeyDefined,              ///< <i>Missing explanation</i>
  GetValue,                     ///< <i>Missing explanation</i>
  SetValue,                     ///< <i>Missing explanation</i>
  ExecInterpretedValue,         ///< <i>Missing explanation</i>
  SetValueInterpreted,          ///< <i>Missing explanation</i>
  FinalGetValue,                ///< <i>Missing explanation</i>
  SubroutineExec,               ///< <i>Missing explanation</i>
  SubroutineEnd,                ///< <i>Missing explanation</i>
  SetBound,                     ///< Setting bounds in range scan
  WaitResponse,                 ///< <i>Missing explanation</i>
  WaitCommitResponse,           ///< <i>Missing explanation</i>
  Finished,                     ///< <i>Missing explanation</i>
  ReceiveFinished               ///< <i>Missing explanation</i>
};

/**
 * Type of operation
 */
enum OperationType { 
  ReadRequest = 0,              ///< Read operation
  UpdateRequest = 1,            ///< Update Operation
  InsertRequest = 2,            ///< Insert Operation
  DeleteRequest = 3,            ///< Delete Operation
  WriteRequest = 4,             ///< Write Operation
  ReadExclusive = 5,            ///< Read exclusive
  OpenScanRequest,              ///< Scan Operation
  OpenRangeScanRequest,         ///< Range scan operation
  NotDefined2,                  ///< <i>Missing explanation</i>
  NotDefined                    ///< <i>Missing explanation</i>
};

/**
 * <i>Missing explanation</i>
 */
enum ConStatusType { 
  NotConnected,                 ///< <i>Missing explanation</i>
  Connecting,                   ///< <i>Missing explanation</i>
  Connected,                    ///< <i>Missing explanation</i>
  DisConnecting,                ///< <i>Missing explanation</i>
  ConnectFailure                ///< <i>Missing explanation</i>
};

/**
 * <i>Missing explanation</i>
 */
enum CompletionStatus { 
  NotCompleted,                 ///< <i>Missing explanation</i>
  CompletedSuccess,             ///< <i>Missing explanation</i>
  CompletedFailure,             ///< <i>Missing explanation</i>
  DefinitionFailure             ///< <i>Missing explanation</i>
};

/**
 *  Type of fragmentation used for a table
 */
enum FragmentType { 
  Default = 0,                  ///<  (All is default!)
  Single = 1,                   ///< Only one fragment
  All = 2,                      ///< Default value.  One fragment per node group
  DistributionGroup = 3,        ///< Distribution Group used for fragmentation.
                                ///< One fragment per node group
  DistributionKey = 4,          ///< Distribution Key used for fragmentation.
                                ///< One fragment per node group.
  AllLarge = 5,                 ///< Sixten fragments per node group.
  DGroupLarge = 6,              ///< Distribution Group used for fragmentation.
                                ///< Sixten fragments per node group
  DKeyLarge = 7                 ///< Distribution Key used for fragmentation.
                                ///< Sixten fragments per node group
};

/**
 *  Type of table or index.
 */
enum TableType {
  UndefTableType = 0,
  SystemTable = 1,              ///< Internal.  Table cannot be updated by user
  UserTable = 2,                ///< Normal application table
  UniqueHashIndex = 3,          ///< Unique un-ordered hash index
  HashIndex = 4,                ///< Non-unique un-ordered hash index
  UniqueOrderedIndex = 5,       ///< Unique ordered index
  OrderedIndex = 6              ///< Non-unique ordered index
};

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
/**
 * Different types of tampering with the NDB Cluster.
 * <b>Only for debugging purposes only.</b>
 */
enum TamperType	{ 
  LockGlbChp = 1,               ///< Lock GCP
  UnlockGlbChp,                 ///< Unlock GCP
  CrashNode,                    ///< Crash an NDB node
  ReadRestartGCI,               ///< Request the restart GCI id from NDB Cluster
  InsertError                   ///< Execute an error in NDB Cluster 
                                ///< (may crash system)
};
#endif

#ifndef DOXYGEN_SHOULD_SKIP_DEPRECATED
/**
 * @deprecated
 */
enum NullAttributeType { 
  NoNullTypeDefined = -1,
  NotNullAttribute, 
  NullAttribute,
  AttributeDefined 
};
#endif

#endif
