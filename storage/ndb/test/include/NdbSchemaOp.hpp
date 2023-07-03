/*
   Copyright (c) 2003, 2022, Oracle and/or its affiliates.

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

#ifndef NdbSchemaOp_H
#define NdbSchemaOp_H

#include <NdbDictionary.hpp>

#ifndef DOXYGEN_SHOULD_SKIP_DEPRECATED

  /**
   * Type of attribute
   *
   * NOTE! AttrType is deprecated, use NdbDictionary::Column::Type instead!
   */
  enum AttrType { 
    Signed,           ///< Attributes of this type can be read with:
                      ///< NdbRecAttr::int64_value, 
                      ///< NdbRecAttr::int32_value,
                      ///< NdbRecAttr::short_value, 
                      ///< NdbRecAttr::char_value
    UnSigned,         ///< Attributes of this type can be read with:
                      ///< NdbRecAttr::u_64_value, 
                      ///< NdbRecAttr::u_32_value,
                      ///< NdbRecAttr::u_short_value, 
                      ///< NdbRecAttr::u_char_value
    Float,            ///< Attributes of this type can be read with:
                      ///< NdbRecAttr::float_value and 
                      ///< NdbRecAttr::double_value
    String,           ///< Attributes of this type can be read with:
                      ///< NdbRecAttr::aRef, 
                      ///< NdbRecAttr::getAttributeObject
    NoAttrTypeDef     ///< Used for debugging only
  };


  /**
   * @deprecated
   */
  enum NullAttributeType { 
    NoNullTypeDefined = -1,
    NotNullAttribute, 
    NullAttribute,
    AttributeDefined 
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
   * Only for legacy createAttribute().
   */
  enum StorageMode { 
    MMBased = NDB_STORAGETYPE_MEMORY,
    DiskBased = NDB_STORAGETYPE_DISK
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
    SystemTable = 1,              ///< Internal.Table cannot be updated by user
    UserTable = 2,                  ///< Normal application table
    UniqueHashIndex = 3,          ///< Unique un-ordered hash index
    HashIndex = 4,                ///< Non-unique un-ordered hash index
    UniqueOrderedIndex = 5,       ///< Unique ordered index
    OrderedIndex = 6              ///< Non-unique ordered index
  };


class NdbSchemaCon;
class Ndb;
  

/** 
 * @class NdbSchemaOp
 * @brief Represents various operations for use in schema transactions
 *
 * This class is used for schema operations, e.g. creating tables and
 * attributes.
 *
 * The NdbSchemaOp object is created using NdbSchemaCon::getNdbSchemaOp.
 * 
 * @note  This class is deprecated and is now replaced with the class
 *        NdbDictionary.
 */
class NdbSchemaOp 
{
  friend class Ndb;
  friend class NdbSchemaCon;

public:

  
  /**
   * Create a new table in the database.
   * 
   * @note The NdbSchemaCon should be closed with 
   *       Ndb::closeSchemaTransaction, even if this method fails.
   *
   * @param  aTableName   Table name.  Should not be NULL.
   * @param  aTableSize	  (Performance parameter.)
   *                      Initial size of the data part of the table
   *                      expressed in kByte. 
   *                      The database handles
   * 			  bad parameter setting but at a certain 
   *                      loss in performance.
   *                      The size given here is
   * 			  the initial size allocated for the table 
   *                      storage (the data part).
   * 			  When calculating the data storage one should 
   *                      add the size of all attributes (each attribute
   *                      consumes at least 4 bytes) and also an overhead
   *                      of 12 byte. 
   *                      Variable size attributes (not supported yet)
   * 			  will have a size of 12 bytes plus the actual 
   *                      data storage parts where there is an 
   *                      additional overhead based on the size of the
   * 			  variable part.
   *                      <br>
   * 	                  An example table with 5 attributes: 
   *                      one 64 bit attribute, one 32 bit attribute, 
   *                      two 16 bit attributes and one array of 64 8 bits. 
   *                      This table will consume 
   *                        12 (overhead) + 8 + 4 + 2*4 (4 is minimum) + 64 = 
   *                        96 bytes per record.
   * 	                  Additionally an overhead of about 2 % as page 
   *                      headers and waste should be allocated. 
   *                      Thus, 1 million records should consume 96 MBytes
   * 	                  plus the overhead 2 MByte and rounded up to 
   *                      100 000 kBytes.
   *                      <br><em>
   *                      This parameter is currently not used.
   *                      </em>
   * @param  aTupleKey	  Indicates if the table has a primary key or not.
   * 			  <br>
   *                        <b>TupleKey</b> means that a <em>primary key</em> 
   *                        consisting of one to four attributes
   * 			    (at most one of variable size) 
   *                        uniquely identifies each record in the created
   *                        table.
   * 			    <br>
   *                        <b>TupleId</b> means that a <em>tuple identity</em>
   *                        is used.  The tuple identity is 
   *                        a unique key identifying each record of the 
   *                        created table.
   *                        The tuple identity is a (non-stored)
   *                        64 bit attribute named <b>NDB$TID</b>.
   * 			    <br>
   *                        When inserting a record (tuple), the method 
   *                        NdbOperation::setTupleId 
   *                        will generate a unique tuple identity
   *                        and return it to the user. 
   *                        <br>
   *                        When reading, updating or deleting a record
   *                        in a table with <b>TupleId</b>,
   *                        NdbOperation::equal("NDB$TID", value_Uint64)
   *                        can be used to identify the record.
   *                        <br>
   *                        Legal values: TupleKey or TupleId.
   * @param aNrOfPages	  (Performance parameter.)
   *                      Specifies the initial size of the index storage. 
   *                      When calculating the index storage,
   * 			  each key has approximately 14 byte of 
   *                      overhead plus the size of the key. 
   *                      Each key attribute takes up at least 4 bytes 
   *                      of storage. 
   *                      Thus a mixed key consisting of a 
   *                      64 bit attribute, a 32 bit attribute
   * 			  and a 16 bit attribute will 
   *                      consume approx. 30 bytes per key.
   * 			  Thus, the if initial size is to be 1 million rows,
   *                      then aNrOfPages should be set to 
   *                      30 M / 8k = 2670 pages.
   *                      <br><em>
   *                      This parameter is currently not used.
   *                       </em>
   * @param aFragmentType Type of fragmentation.<br>
   *                      <b>All</b> (default) means that the 
   *                      table fragments are automatically 
   *                      distributed on all nodes in the system.<br>
   *                      <b>DistributionGroup</b> and 
   *                      <b>DistributionKey</b> are 
   *                      also supported. For further details about
   *                      these types see the documentation of 
   *                      Ndb::startTransaction.
   * @param aKValue	  (Hash parameter.)
   *                      Only allowed value is 6.
   *                      Later implementations might add flexibility
   * 			  in this parameter.
   * @param aMinLoadFactor  (Hash parameter.)
   *                        This value specifies the load factor when 
   *                        starting to shrink the hash table. 
   *                        It must be smaller than aMaxLoadFactor.
   *                        Both these factors are given in percentage.
   * @param aMaxLoadFactor  (Hash parameter.)
   *                        This value specifies the load factor when 
   *                        starting to split the containers in the local
   *                        hash tables. 100 is the maximum which will
   * 		     	    optimize memory usage (this is the figure 
   *                        used for the above calculations).
   * 		       	    A lower figure will store less information in 
   *                        each container and thus
   * 		            find the key faster but consume more memory.
   * @param aMemoryType	    Currently only 1 is allowed which specifies 
   *                        storage of table in main memory. 
   *                        Later 2 will be added where the table is stored
   * 	       		    completely on disk 
   *                        and 3 where the index is in main memory but
   * 		            data is on disk. 
   *                        If 1 is chosen an individual attribute can
   * 		            still be specified as a disk attribute.
   * @param aStoredTable    If set to false it indicates that the table is 
   *                        a temporary table and should not be logged 
   *                        to disk.
   *                        In case of a system restart the table will still
   * 	      	   	    be defined and exist but will be empty. 
   *                        Thus no checkpointing and
   * 		       	    no logging is performed on the table.
   * 			    The default value is true and indicates a 
   *                        normal table with full checkpointing and 
   *                        logging activated.
   * @return                Returns 0 when successful and returns -1 otherwise.
   */
  int		createTable(	const char* aTableName, 
				Uint32 aTableSize = 8, 
				KeyType aTupleKey = TupleKey,
				int aNrOfPages = 2, 
				FragmentType aFragmentType = All, 
				int aKValue = 6,
				int aMinLoadFactor = 78,
				int aMaxLoadFactor = 80,
				int aMemoryType = 1,
				bool aStoredTable = true);

  /**
   * Add a new attribute to a database table.
   *
   * Attributes can only be added to a table in the same transaction
   * as the transaction creating the table.
   *
   * @note The NdbSchemaCon transaction should be closed with 
   *       Ndb::closeSchemaTransaction, even if this method fails.
   *
   * Example creating an unsigned int attribute belonging to the primary key
   * of the table it is created in:
   * @code 
   *   MySchemaOp->createAttribute("Attr1",   // Attribute name
   *                               TupleKey,  // Belongs to primary key
   *                               32,        // 32 bits
   *                               1,         // Not an array attribute
   *                               UnSigned,  // Unsigned type
   *                              );
   * @endcode 
   * 
   * Example creating a string attribute belonging to the primary key
   * of the table it is created in:
   * @code
   *   MySchemaOp->createAttribute("Attr1",       // Attribute name
   *                               TupleKey,      // Belongs to primary key
   *                               8,             // Each character is 8 bits
   *                               12,            // Max 12 chars in string
   *                               String,        // Attribute if of type string
   *                              );
   * @endcode
   *
   * A <em>distribution key</em> is a set of attributes which are used
   * to distribute the tuples onto the NDB nodes.
   * A <em>distribution group</em> is a part (currently 16 bits) 
   * of an attribute used to distribute the tuples onto the NDB nodes.
   * The distribution key uses the NDB Cluster hashing function,
   * while the distribution group uses a simpler function.
   *
   * @param  aAttrName   Attribute name.  Should not be NULL.
   * @param  aTupleKey	 This parameter specifies whether the 
   *                     attribute is part of the primary key or not.
   *                     Floats are not allowed in the primary key.
   *                     <br>
   *                     Legal values: NoKey, TupleKey
   * @param  aAttrSize	 Specifies the size of the elements of the 
   *                     attribute.  (An attribute can consist
   *                     of an array of elements.)
   *                     <br>
   *                     Legal values: 8, 16, 32, 64 and 128 bits.
   * @param  aArraySize	 Size of array.
   *                     <br>
   *                     Legal values:
   *                     0 = variable-sized array, 
   *                     1 = no array, and
   *                     2- = fixed size array.
   *                     <br>
   *                     <em>
   *                     Variable-sized array attributes are 
   *                     not yet supported.
   *                     </em>
   *                     <br>
   *                     There is no upper limit of the array size
   *                     for a single attribute. 
   * @param  aAttrType   The attribute type.
   * 			 This is only of interest if calculations are 
   *                     made within NDB.
   *                     <br>
   *                     Legal values: UnSigned, Signed, Float, String
   * @param aStorageMode    Main memory based or disk based attribute.<br>
   *                     Legal values: MMBased, DiskBased
   *                     <br>
   *                     <em>
   *                     Disk-based attributes are not yet supported.
   *                     </em>
   * @param nullable     Set to true if NULL is a correct value for
   *                     the attribute.
   *                     <br>
   *                     Legal values: true, false
   * @param aStType      Obsolete since wl-2066
   * @param aDistributionKey    Sometimes it is preferable to use a subset
   *                            of the primary key as the distribution key. 
   *                            An example is TPC-C where it might be
   *                            good to use the warehouse id and district id 
   *                            as the distribution key. 
   *                            <br>
   *                            Locally in the fragments the full primary key 
   *                            will still be used with the hashing algorithm.
   *                            Set to 1 if this attribute is part of the 
   *                            distribution key.
   *                            All distribution key attributes must be 
   *                            defined before
   *                            any other attributes are defined.
   * @param aDistributionGroup    In other applications it is desirable to use 
   *                              only a part of an attribute to create the 
   *                              distribution key.
   *                              This is applicable for some telecom
   *                              applications.
   *                              <br>
   *                              In these situations one must provide how many 
   *                              bits of the attribute that is to
   *                              be used as the distribution hash value.
   *                              <br>
   *                              This provides some control to the
   *                              application of the distribution. 
   *                              It still needs to be part of a primary key
   *                              the attribute and must be defined as the 
   *                              first attribute.
   * @param  aDistributionGroupNoOfBits
   *                              Number of bits to use of the 
   *                              distribution group attribute in the
   *                              distribution hash value.
   *                              <br>
   *                              Currently, only 16 bits is supported. It will
   *                              always be the last 16 bits in the attribute
   *                              which is used for the distribution group.
   * @param aAutoIncrement        Set to autoincrement attribute.
   * @param aDefaultValue         Set a default value of attribute.
   *
   * @return Returns 0 when successful and returns -1 otherwise.
   ****************************************************************************/
  int createAttribute(const char* aAttrName,
		      KeyType aTupleKey = NoKey,
		      int aAttrSize = 32,
		      int aArraySize = 1,
		      AttrType aAttrType = UnSigned,
		      StorageMode aStorageMode = MMBased,
		      bool nullable = false,
		      int aStType= 0, // obsolete
		      int aDistributionKey = 0,
		      int aDistributionGroup = 0,
		      int aDistributionGroupNoOfBits = 16,
                      bool aAutoIncrement = false,
                      const char* aDefaultValue = 0);

  /**
   * @deprecated do not use!
   */
  int createAttribute(const char* aAttrName,
		      KeyType aTupleKey,
		      int aAttrSize,
		      int aArraySize,
		      AttrType aAttrType,
		      StorageMode aStorageMode,
		      NullAttributeType aNullAttr,
		      int aStType, // obsolete
		      int aDistributionKey = 0,
		      int aDistributionGroup = 0,
		      int aDistributionGroupNoOfBits = 16){
    return createAttribute(aAttrName,
			   aTupleKey,
			   aAttrSize,
			   aArraySize,
			   aAttrType,
			   aStorageMode,
			   aNullAttr == NullAttribute,
			   aStType,
			   aDistributionKey,
			   aDistributionGroup,
			   aDistributionGroupNoOfBits);
  }

  const NdbError & getNdbError() const;

protected:

/*****************************************************************************
 *   These are the methods used to create and delete the NdbOperation objects.
 ****************************************************************************/
  			NdbSchemaOp(Ndb* aNdb);

  			~NdbSchemaOp();     

/******************************************************************************
 *	These methods are service routines used by the other NDBAPI classes.
 *****************************************************************************/

  void			release();	// Release all memory connected
  					      // to the operations object.	    

/****************************************************************************
 *	The methods below is the execution part of the NdbSchemaOp class. 
 *****************************************************************************/

  int sendRec();	
  int sendSignals(Uint32 aNodeId, bool HaveMutex);

  int init(NdbSchemaCon* aSchemaCon);

  /**************************************************************************
   * These are the private variables that are defined in the operation 
   * objects.
   **************************************************************************/
  Ndb*			theNdb;		// Point back to the Ndb object.      
  NdbSchemaCon* 	theSchemaCon;	// Point back to the connection object.
  

  class NdbDictionary::Table * m_currentTable;
};


/**
 * Get old attribute type from new type
 * 
 * NOTE! attrType is deprecated, use getType instead!
 *
 * @return Type of attribute: { Signed, UnSigned, Float,a String }
 */
inline 
AttrType 
convertColumnTypeToAttrType(NdbDictionary::Column::Type _type)
{      
  
  switch(_type){
  case NdbDictionary::Column::Bigint:
  case NdbDictionary::Column::Int:
    return Signed;
  case NdbDictionary::Column::Bigunsigned:
  case NdbDictionary::Column::Unsigned:
    return UnSigned;
  case NdbDictionary::Column::Float:
  case NdbDictionary::Column::Olddecimal:
  case NdbDictionary::Column::Olddecimalunsigned:
  case NdbDictionary::Column::Decimal:
  case NdbDictionary::Column::Decimalunsigned:
  case NdbDictionary::Column::Double:
    return Float;
  case NdbDictionary::Column::Char:
  case NdbDictionary::Column::Varchar:
  case NdbDictionary::Column::Binary:
  case NdbDictionary::Column::Varbinary:
    return String;
  default:
    return NoAttrTypeDef;
  }
}
#endif

#endif


