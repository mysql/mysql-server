/*
   Copyright (c) 2003, 2014, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NdbDictionary_H
#define NdbDictionary_H

#include <ndb_types.h>

class Ndb;
struct charset_info_st;
typedef struct charset_info_st CHARSET_INFO;

/* Forward declaration only. */
class NdbRecord;

/**
 * @class NdbDictionary
 * @brief Data dictionary class
 * 
 * The preferred and supported way to create and drop tables and indexes
 * in ndb is through the 
 * MySQL Server (see MySQL reference Manual, section MySQL Cluster).
 *
 * Tables and indexes that are created directly through the 
 * NdbDictionary class
 * can not be viewed from the MySQL Server.
 * Dropping indexes directly via the NdbApi will cause inconsistencies
 * if they were originally created from a MySQL Cluster.
 * 
 * This class supports schema data enquiries such as:
 * -# Enquiries about tables
 *    (Dictionary::getTable, Table::getNoOfColumns, 
 *    Table::getPrimaryKey, and Table::getNoOfPrimaryKeys)
 * -# Enquiries about indexes
 *    (Dictionary::getIndex, Index::getNoOfColumns, 
 *    and Index::getColumn)
 *
 * This class supports schema data definition such as:
 * -# Creating tables (Dictionary::createTable) and table columns
 * -# Dropping tables (Dictionary::dropTable)
 * -# Creating secondary indexes (Dictionary::createIndex)
 * -# Dropping secondary indexes (Dictionary::dropIndex)
 *
 * NdbDictionary has several help (inner) classes to support this:
 * -# NdbDictionary::Dictionary the dictionary handling dictionary objects
 * -# NdbDictionary::Table for creating tables
 * -# NdbDictionary::Column for creating table columns
 * -# NdbDictionary::Index for creating secondary indexes
 *
 * See @ref ndbapi_simple_index.cpp for details of usage.
 */
class NdbDictionary {
public:
  NdbDictionary() {}                          /* Remove gcc warning */
  /**
   * @class Object
   * @brief Meta information about a database object (a table, index, etc)
   */
  class Object {
  public:
    Object() {}                               /* Remove gcc warning */
    virtual ~Object() {}                      /* Remove gcc warning */
    /**
     * Status of object
     */
    enum Status {
      New,                    ///< The object only exist in memory and 
                              ///< has not been created in the NDB Kernel
      Changed,                ///< The object has been modified in memory 
                              ///< and has to be commited in NDB Kernel for 
                              ///< changes to take effect
      Retrieved,              ///< The object exist and has been read 
                              ///< into main memory from NDB Kernel
      Invalid,                ///< The object has been invalidated
                              ///< and should not be used
      Altered                 ///< Table has been altered in NDB kernel
                              ///< but is still valid for usage
    };

    /**
     * Get status of object
     */
    virtual Status getObjectStatus() const = 0;

    /**
     * Get version of object
     */
    virtual int getObjectVersion() const = 0;
    
    virtual int getObjectId() const = 0;
    
    /**
     * Object type
     */
    enum Type {
      TypeUndefined = 0,      ///< Undefined
      SystemTable = 1,        ///< System table
      UserTable = 2,          ///< User table (may be temporary)
      UniqueHashIndex = 3,    ///< Unique un-ordered hash index
      OrderedIndex = 6,       ///< Non-unique ordered index
      HashIndexTrigger = 7,   ///< Index maintenance, internal
      IndexTrigger = 8,       ///< Index maintenance, internal
      SubscriptionTrigger = 9,///< Backup or replication, internal
      ReadOnlyConstraint = 10,///< Trigger, internal
      TableEvent = 11,        ///< Table event
      Tablespace = 20,        ///< Tablespace
      LogfileGroup = 21,      ///< Logfile group
      Datafile = 22,          ///< Datafile
      Undofile = 23,          ///< Undofile
      ReorgTrigger = 19,
      HashMap = 24,
      ForeignKey = 25,
      FKParentTrigger = 26,
      FKChildTrigger = 27
    };

    /**
     * Object state
     */
    enum State {
      StateUndefined = 0,     ///< Undefined
      StateOffline = 1,       ///< Offline, not usable
      StateBuilding = 2,      ///< Building, not yet usable
      StateDropping = 3,      ///< Offlining or dropping, not usable
      StateOnline = 4,        ///< Online, usable
      StateBackup = 5,        ///< Online, being backuped, usable
      StateBroken = 9         ///< Broken, should be dropped and re-created
    };

    /**
     * Object store
     */
    enum Store {
      StoreUndefined = 0,     ///< Undefined
      StoreNotLogged = 1,     ///< Object or data deleted on system restart
      StorePermanent = 2      ///< Permanent. logged to disk
    };

    /**
     * Type of fragmentation.
     *
     * This parameter specifies how data in the table or index will
     * be distributed among the db nodes in the cluster.<br>
     * The bigger the table the more number of fragments should be used.
     * Note that all replicas count as same "fragment".<br>
     * For a table, default is FragAllMedium.  For a unique hash index,
     * default is taken from underlying table and cannot currently
     * be changed.
     */
    enum FragmentType { 
      FragUndefined = 0,      ///< Fragmentation type undefined or default
      FragSingle = 1,         ///< Only one fragment
      FragAllSmall = 2,       ///< One fragment per node, default
      FragAllMedium = 3,      ///< two fragments per node
      FragAllLarge = 4,       ///< Four fragments per node.
      DistrKeyHash = 5,
      DistrKeyLin = 6,
      UserDefined = 7,
      HashMapPartition = 9
    };
  private:
    Object&operator=(const Object&);
  };

  class Dictionary; // Forward declaration
  
  class ObjectId : public Object 
  {
  public:
    ObjectId();
    virtual ~ObjectId();
    
    /**
     * Get status of object
     */
    virtual Status getObjectStatus() const;
    
    /**
     * Get version of object
     */
    virtual int getObjectVersion() const;
    
    virtual int getObjectId() const;
    
  private:
    friend class NdbDictObjectImpl;
    class NdbDictObjectImpl & m_impl;

    ObjectId(const ObjectId&); // Not impl.
    ObjectId&operator=(const ObjectId&);
  };
  
  class Table; // forward declaration
  class Tablespace; // forward declaration
  class HashMap; // Forward

  /**
   * @class Column
   * @brief Represents a column in an NDB Cluster table
   *
   * Each column has a type. The type of a column is determined by a number 
   * of type specifiers.
   * The type specifiers are:
   * - Builtin type
   * - Array length or max length
   * - Precision and scale (not used yet)
   * - Character set for string types
   * - Inline and part sizes for blobs
   *
   * Types in general correspond to MySQL types and their variants.
   * Data formats are same as in MySQL.  NDB API provides no support for
   * constructing such formats.  NDB kernel checks them however.
   */
  class Column {
  public:
    /**
     * The builtin column types
     */
    enum Type {
      Undefined = NDB_TYPE_UNDEFINED,   ///< Undefined 
      Tinyint = NDB_TYPE_TINYINT,       ///< 8 bit. 1 byte signed integer, can be used in array
      Tinyunsigned = NDB_TYPE_TINYUNSIGNED,  ///< 8 bit. 1 byte unsigned integer, can be used in array
      Smallint = NDB_TYPE_SMALLINT,      ///< 16 bit. 2 byte signed integer, can be used in array
      Smallunsigned = NDB_TYPE_SMALLUNSIGNED, ///< 16 bit. 2 byte unsigned integer, can be used in array
      Mediumint = NDB_TYPE_MEDIUMINT,     ///< 24 bit. 3 byte signed integer, can be used in array
      Mediumunsigned = NDB_TYPE_MEDIUMUNSIGNED,///< 24 bit. 3 byte unsigned integer, can be used in array
      Int = NDB_TYPE_INT,           ///< 32 bit. 4 byte signed integer, can be used in array
      Unsigned = NDB_TYPE_UNSIGNED,      ///< 32 bit. 4 byte unsigned integer, can be used in array
      Bigint = NDB_TYPE_BIGINT,        ///< 64 bit. 8 byte signed integer, can be used in array
      Bigunsigned = NDB_TYPE_BIGUNSIGNED,   ///< 64 Bit. 8 byte signed integer, can be used in array
      Float = NDB_TYPE_FLOAT,         ///< 32-bit float. 4 bytes float, can be used in array
      Double = NDB_TYPE_DOUBLE,        ///< 64-bit float. 8 byte float, can be used in array
      Olddecimal = NDB_TYPE_OLDDECIMAL,    ///< MySQL < 5.0 signed decimal,  Precision, Scale
      Olddecimalunsigned = NDB_TYPE_OLDDECIMALUNSIGNED,
      Decimal = NDB_TYPE_DECIMAL,    ///< MySQL >= 5.0 signed decimal,  Precision, Scale
      Decimalunsigned = NDB_TYPE_DECIMALUNSIGNED,
      Char = NDB_TYPE_CHAR,          ///< Len. A fixed array of 1-byte chars
      Varchar = NDB_TYPE_VARCHAR,       ///< Length bytes: 1, Max: 255
      Binary = NDB_TYPE_BINARY,        ///< Len
      Varbinary = NDB_TYPE_VARBINARY,     ///< Length bytes: 1, Max: 255
      Datetime = NDB_TYPE_DATETIME,    ///< Precision down to 1 sec (sizeof(Datetime) == 8 bytes )
      Date = NDB_TYPE_DATE,            ///< Precision down to 1 day(sizeof(Date) == 4 bytes )
      Blob = NDB_TYPE_BLOB,        ///< Binary large object (see NdbBlob)
      Text = NDB_TYPE_TEXT,         ///< Text blob
      Bit = NDB_TYPE_BIT,          ///< Bit, length specifies no of bits
      Longvarchar = NDB_TYPE_LONGVARCHAR,  ///< Length bytes: 2, little-endian
      Longvarbinary = NDB_TYPE_LONGVARBINARY, ///< Length bytes: 2, little-endian
      Time = NDB_TYPE_TIME,        ///< Time without date
      Year = NDB_TYPE_YEAR,   ///< Year 1901-2155 (1 byte)
      Timestamp = NDB_TYPE_TIMESTAMP, ///< Unix time
      /**
       * Time types in MySQL 5.6 add microsecond fraction.
       * One should use setPrecision(x) to set number of fractional
       * digits (x = 0-6, default 0).  Data formats are as in MySQL
       * and must use correct byte length.  NDB does not check data
       * itself since any values can be compared as binary strings.
       */
      Time2 = NDB_TYPE_TIME2, ///< 3 bytes + 0-3 fraction
      Datetime2 = NDB_TYPE_DATETIME2, ///< 5 bytes plus 0-3 fraction
      Timestamp2 = NDB_TYPE_TIMESTAMP2 ///< 4 bytes + 0-3 fraction
    };

    /*
     * Array type specifies internal attribute format.
     *
     * - ArrayTypeFixed is stored as fixed number of bytes.  This type
     *   is fastest to access but can waste space.
     *
     * - ArrayTypeVar is stored as variable number of bytes with a fixed
     *   overhead of 2 bytes.
     *
     * Default is ArrayTypeVar for Var* types and ArrayTypeFixed for
     * others.  The default is normally ok.
     */
    enum ArrayType {
      ArrayTypeFixed = NDB_ARRAYTYPE_FIXED,          // 0 length bytes
      ArrayTypeShortVar = NDB_ARRAYTYPE_SHORT_VAR,   // 1 length bytes
      ArrayTypeMediumVar = NDB_ARRAYTYPE_MEDIUM_VAR // 2 length bytes
    };

    /*
     * Storage type specifies whether attribute is stored in memory or
     * on disk.  Default is memory.  Disk attributes are potentially
     * much slower to access and cannot be indexed in version 5.1.
     */
    enum StorageType {
      StorageTypeMemory = NDB_STORAGETYPE_MEMORY,
      StorageTypeDisk = NDB_STORAGETYPE_DISK,
      StorageTypeDefault = NDB_STORAGETYPE_DEFAULT
    };

    /** 
     * @name General 
     * @{
     */
    
    /**
     * Get name of column
     * @return  Name of the column
     */
    const char* getName() const;

    /**
     * Get if the column is nullable or not
     */
    bool getNullable() const;
    
    /**
     * Check if column is part of primary key
     */
    bool getPrimaryKey() const;

    /**
     *  Get number of column (horizontal position within table)
     */
    int getColumnNo() const;

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
    int getAttrId() const;
#endif

    /**
     * Check if column is equal to some other column
     * @param  column  Column to compare with
     * @return  true if column is equal to some other column otherwise false.
     */
    bool equal(const Column& column) const;


    /** @} *******************************************************************/
    /** 
     * @name Get Type Specifiers
     * @{
     */

    /**
     * Get type of column
     */
    Type getType() const;

    /**
     * Get precision of column.
     * @note Only applicable for decimal types
     * @note Also applicable for Time2 etc in mysql 5.6
     */
    int getPrecision() const;

    /**
     * Get scale of column.
     * @note Only applicable for decimal types
     */
    int getScale() const;

    /**
     * Get length for column
     * Array length for column or max length for variable length arrays.
     */
    int getLength() const;

    /**
     * For Char or Varchar or Text, get MySQL CHARSET_INFO.  This
     * specifies both character set and collation.  See get_charset()
     * etc in MySQL.  (The cs is not "const" in MySQL).
     */
    CHARSET_INFO* getCharset() const;

    /**
     * Returns mysql's internal number for the column's character set.
     */
    int getCharsetNumber() const;

    /**
     * For blob, get "inline size" i.e. number of initial bytes
     * to store in table's blob attribute.
     */
    int getInlineSize() const;

    /**
     * For blob, get "part size" i.e. number of bytes to store in
     * each tuple of the "blob table".  Can be set to zero to omit parts
     * and to allow only inline bytes ("tinyblob").
     */
    int getPartSize() const;

    /**
     * For blob, set or get "stripe size" i.e. number of consecutive
     * <em>parts</em> to store in each node group.
     */
    int getStripeSize() const;

    /**
     * Get size of element
     */
    int getSize() const;

    /** 
     * Check if column is part of partition key
     *
     * A <em>partition key</em> is a set of attributes which are used
     * to distribute the tuples onto the NDB nodes.
     * The partition key uses the NDB Cluster hashing function.
     *
     * An example where this is useful is TPC-C where it might be
     * good to use the warehouse id and district id as the partition key. 
     * This would place all data for a specific district and warehouse 
     * in the same database node.
     *
     * Locally in the fragments the full primary key 
     * will still be used with the hashing algorithm.
     *
     * @return  true then the column is part of 
     *                 the partition key.
     */
    bool getPartitionKey() const;
#ifndef DOXYGEN_SHOULD_SKIP_DEPRECATED
    inline bool getDistributionKey() const { return getPartitionKey(); };
#endif

    ArrayType getArrayType() const;
    StorageType getStorageType() const;

    /**
     * Get if the column is dynamic (NULL values not stored)
     */
    bool getDynamic() const;

    /**
     * Determine if the column is defined relative to an Index
     * This affects the meaning of the attrId, column no and primary key,
     */
    bool getIndexSourced() const;

    /** @} *******************************************************************/


    /** 
     * @name Column creation
     * @{
     *
     * These operations should normally not be performed in an NbdApi program
     * as results will not be visable in the MySQL Server
     * 
     */

    /**
     * Constructor
     * @param   name   Name of column
     */
    Column(const char * name = "");
    /**
     * Copy constructor
     * @param  column  Column to be copied
     */
    Column(const Column& column); 
    ~Column();

    /**
     * Set name of column
     * @param  name  Name of the column
     */
    int setName(const char * name);

    /**
     * Set whether column is nullable or not
     */
    void setNullable(bool);

    /**
     * Set that column is part of primary key
     */
    void setPrimaryKey(bool);

    /**
     * Set type of column
     * @param  type  Type of column
     *
     * @note setType resets <em>all</em> column attributes
     *       to (type dependent) defaults and should be the first
     *       method to call.  Default type is Unsigned.
     */
    void setType(Type type);

    /**
     * Set precision of column.
     * @note Only applicable for decimal types
     * @note Also applicable for Time2 etc in mysql 5.6
     */
    void setPrecision(int);

    /**
     * Set scale of column.
     * @note Only applicable for decimal types
     */
    void setScale(int);

    /**
     * Set length for column
     * Array length for column or max length for variable length arrays.
     */
    void setLength(int length);

    /**
     * For Char or Varchar or Text, get MySQL CHARSET_INFO.  This
     * specifies both character set and collation.  See get_charset()
     * etc in MySQL.  (The cs is not "const" in MySQL).
     */
    void setCharset(CHARSET_INFO* cs);

    /**
     * For blob, set "inline size" i.e. number of initial bytes
     * to store in table's blob attribute.  This part is normally in
     * main memory.  It can not currently be indexed.
     */
    void setInlineSize(int size);

    /**
     * For blob, set "part size" i.e. number of bytes to store in
     * each tuple of the "blob table".  Can be set to zero to omit parts
     * and to allow only inline bytes ("tinyblob").
     */
    void setPartSize(int size);

    /**
     * For blob, set "stripe size" i.e. number of consecutive
     * <em>parts</em> to store in a fragment, before moving to
     * another (random) fragment.
     *
     * Striping may improve performance for large blobs
     * since blob part operations are done in parallel.
     * Optimal stripe size depends on the transport e.g. tcp/ip.
     *
     * Example: Given part size 2048 bytes, set stripe size 8.
     * This assigns i/o in 16k chunks to each fragment.
     *
     * Blobs V1 required non-zero stripe size.  Blobs V2
     * (created in version >= 5.1.x) have following behaviour:
     *
     * Default stripe size is zero, which means no striping and
     * also that blob part data is stored in the same node group
     * as the primary table row.  This is done by giving blob parts
     * table same partition key as the primary table.
     */
    void setStripeSize(int size);

    /** 
     * Set partition key
     * @see getPartitionKey
     *
     * @param  enable  If set to true, then the column will be part of 
     *                 the partition key.
     */
    void setPartitionKey(bool enable);
#ifndef DOXYGEN_SHOULD_SKIP_DEPRECATED
    inline void setDistributionKey(bool enable)
    { setPartitionKey(enable); };
#endif

    void setArrayType(ArrayType type);
    void setStorageType(StorageType type);

    /**
     * Set whether column is dynamic.
     */
    void setDynamic(bool);

    /** @} *******************************************************************/

#ifndef DOXYGEN_SHOULD_SKIP_DEPRECATED
    int setDefaultValue(const char*);
#endif
    /* setDefaultValue
     * Set buf to NULL for no default value, or null default value for
     * NULLABLE column, otherwise set buf to pointer to default value.
     * The len parameter is the number of significant bytes of default
     * value supplied, which is the type size for fixed size types.
     * For variable length types, the leading 1 or 2 bytes pointed to 
     * by buf also contain length information as normal for the type.
     */
    int setDefaultValue(const void* buf, unsigned int len);

    /* getDefaultValue
     * Get the default value data for this column.
     * Optional int len* will be updated with the significant length 
     * of the default value, or set to 0 for NULL or no default.
     */
    const void* getDefaultValue(unsigned int* len = 0) const;

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
    const Table * getBlobTable() const;

    void setAutoIncrement(bool);
    bool getAutoIncrement() const;
    void setAutoIncrementInitialValue(Uint64 val);

    static const Column * FRAGMENT;
    static const Column * FRAGMENT_FIXED_MEMORY;
    static const Column * FRAGMENT_VARSIZED_MEMORY;
    static const Column * ROW_COUNT;
    static const Column * COMMIT_COUNT;
    static const Column * ROW_SIZE;
    static const Column * RANGE_NO;
    static const Column * DISK_REF;
    static const Column * RECORDS_IN_RANGE;
    static const Column * ROWID;
    static const Column * ROW_GCI;
    static const Column * ROW_GCI64;
    static const Column * ROW_AUTHOR;
    static const Column * ANY_VALUE;
    static const Column * COPY_ROWID;
    static const Column * LOCK_REF;
    static const Column * OP_ID;
    static const Column * OPTIMIZE;
    static const Column * FRAGMENT_EXTENT_SPACE;
    static const Column * FRAGMENT_FREE_EXTENT_SPACE;

    int getSizeInBytes() const;

    int getBlobVersion() const; // NDB_BLOB_V1 or NDB_BLOB_V2
    void setBlobVersion(int blobVersion); // default NDB_BLOB_V2

    /**
     * 0 = yes
     * -1 = no
     */
    int isBindable(const Column&) const;
#endif
    
  private:
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
    friend class NdbRecAttr;
    friend class NdbColumnImpl;
#endif
    class NdbColumnImpl & m_impl;
    Column(NdbColumnImpl&);
    Column& operator=(const Column&);
  };

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  /**
   * ???
   */
  typedef Column Attribute;
#endif
  
  /**
   * @brief Represents a table in NDB Cluster
   *
   * <em>TableSize</em><br>
   * When calculating the data storage one should add the size of all 
   * attributes (each attributeconsumes at least 4 bytes) and also an overhead
   * of 12 byte. Variable size attributes (not supported yet) will have a 
   * size of 12 bytes plus the actual data storage parts where there is an 
   * additional overhead based on the size of the variable part.<br>
   * An example table with 5 attributes: 
   * one 64 bit attribute, one 32 bit attribute, 
   * two 16 bit attributes and one array of 64 8 bits. 
   * This table will consume 
   * 12 (overhead) + 8 + 4 + 2*4 (4 is minimum) + 64 = 96 bytes per record.
   * Additionally an overhead of about 2 % as page headers and waste should 
   * be allocated. Thus, 1 million records should consume 96 MBytes
   * plus the overhead 2 MByte and rounded up to 100 000 kBytes.<br>
   *
   */
  class Table : public Object {
  public:
    /*
     * Single user mode specifies access rights to table during single user mode
     */
    enum SingleUserMode {
      SingleUserModeLocked    = NDB_SUM_LOCKED,
      SingleUserModeReadOnly  = NDB_SUM_READONLY,
      SingleUserModeReadWrite = NDB_SUM_READ_WRITE
    };

    /** 
     * @name General
     * @{
     */

    /**
     * Get table name
     */
    const char * getName() const;

    /**
     * Get table id
     */ 
    int getTableId() const;
    
    /**
     * Get column definition via name.
     * @return null if none existing name
     */
    const Column* getColumn(const char * name) const;
    
    /**
     * Get column definition via index in table.
     * @return null if none existing name
     */
    Column* getColumn(const int attributeId);

    /**
     * Get column definition via name.
     * @return null if none existing name
     */
    Column* getColumn(const char * name);
    
    /**
     * Get column definition via index in table.
     * @return null if none existing name
     */
    const Column* getColumn(const int attributeId) const;
    
    /** @} *******************************************************************/
    /**
     * @name Storage
     * @{
     */

    /**
     * If set to false, then the table is a temporary 
     * table and is not logged to disk.
     *
     * In case of a system restart the table will still
     * be defined and exist but will be empty. 
     * Thus no checkpointing and no logging is performed on the table.
     *
     * The default value is true and indicates a normal table 
     * with full checkpointing and logging activated.
     */
    bool getLogging() const;

    /**
     * Get fragmentation type
     */
    FragmentType getFragmentType() const;
    
    /**
     * Get KValue (Hash parameter.)
     * Only allowed value is 6.
     * Later implementations might add flexibility in this parameter.
     */
    int getKValue() const;

    /**
     * Get MinLoadFactor  (Hash parameter.)
     * This value specifies the load factor when starting to shrink 
     * the hash table. 
     * It must be smaller than MaxLoadFactor.
     * Both these factors are given in percentage.
     */
    int getMinLoadFactor() const;

    /**
     * Get MaxLoadFactor  (Hash parameter.)
     * This value specifies the load factor when starting to split 
     * the containers in the local hash tables. 
     * 100 is the maximum which will optimize memory usage.
     * A lower figure will store less information in each container and thus
     * find the key faster but consume more memory.
     */
    int getMaxLoadFactor() const;

    /** @} *******************************************************************/
    /** 
     * @name Other
     * @{
     */
    
    /**
     * Get number of columns in the table
     */
    int getNoOfColumns() const;
    
    /**
     * Get number of auto_increment columns in the table
     */
    int getNoOfAutoIncrementColumns() const;
    
    /**
     * Get number of primary keys in the table
     */
    int getNoOfPrimaryKeys() const;

    /**
     * Get name of primary key 
     */
    const char* getPrimaryKey(int no) const;
    
    /**
     * Check if table is equal to some other table
     */
    bool equal(const Table&) const;

    /**
     * Get frm file stored with this table
     */
    const void* getFrmData() const;
    Uint32 getFrmLength() const;

    /**
     * Get default NdbRecord object for this table
     * This NdbRecord object becomes invalid at the same time as
     * the table object - when the ndb_cluster_connection is closed.
     */
    const NdbRecord* getDefaultRecord() const;

    /** @} *******************************************************************/

    /** 
     * @name Table creation
     * @{
     *
     * These methods should normally not be used in an application as
     * the result is not accessible from the MySQL Server
     *
     */

    /**
     * Constructor
     * @param  name   Name of table
     */
    Table(const char * name = "");

    /** 
     * Copy constructor 
     * @param  table  Table to be copied
     */
    Table(const Table& table); 
    virtual ~Table();
    
    /**
     * Assignment operator, deep copy
     * @param  table  Table to be copied
     */
    Table& operator=(const Table& table);

    /**
     * Name of table
     * @param  name  Name of table
     */
    int setName(const char * name);

    /**
     * Add a column definition to a table
     * @note creates a copy
     */
    int addColumn(const Column &);
    
    /**
     * @see NdbDictionary::Table::getLogging.
     */
    void setLogging(bool); 
  
    /**
     * Set/Get Linear Hash Flag
     */ 
    void setLinearFlag(Uint32 flag);
    bool getLinearFlag() const;

    /**
     * Set fragment count
     */
    void setFragmentCount(Uint32);

    /**
     * Get fragment count
     */
    Uint32 getFragmentCount() const;

    /**
     * Set fragmentation type
     */
    void setFragmentType(FragmentType);

    /**
     * Set KValue (Hash parameter.)
     * Only allowed value is 6.
     * Later implementations might add flexibility in this parameter.
     */
    void setKValue(int kValue);
    
    /**
     * Set MinLoadFactor  (Hash parameter.)
     * This value specifies the load factor when starting to shrink 
     * the hash table. 
     * It must be smaller than MaxLoadFactor.
     * Both these factors are given in percentage.
     */
    void setMinLoadFactor(int);

    /**
     * Set MaxLoadFactor  (Hash parameter.)
     * This value specifies the load factor when starting to split 
     * the containers in the local hash tables. 
     * 100 is the maximum which will optimize memory usage.
     * A lower figure will store less information in each container and thus
     * find the key faster but consume more memory.
     */
    void setMaxLoadFactor(int);

    int setTablespaceName(const char * name);
    const char * getTablespaceName() const;
    int setTablespace(const class Tablespace &);
    bool getTablespace(Uint32 *id= 0, Uint32 *version= 0) const;

    bool getHashMap(Uint32* id = 0, Uint32* version = 0) const;
    int setHashMap(const class HashMap &);

    /**
     * Get table object type
     */
    Object::Type getObjectType() const;

    /**
     * Get object status
     */
    virtual Object::Status getObjectStatus() const;
    void setStatusInvalid() const;

    /**
     * Get object version
     */
    virtual int getObjectVersion() const;

    /**
     * Set/Get indicator if default number of partitions is used in table.
     */
    void setDefaultNoPartitionsFlag(Uint32 indicator);
    Uint32 getDefaultNoPartitionsFlag() const;
   
    /**
     * Get object id
     */
    virtual int getObjectId() const;

    /**
     * Set frm file to store with this table
     */ 
    int setFrm(const void* data, Uint32 len);

    /**
     * Set fragmentation
     *   One Uint32 per fragment, containing nodegroup of fragment
     *   nodegroups[0] - correspondce to fragment 0
     *
     * Note: This calls also modifies <em>setFragmentCount</em>
     *
     */
    int setFragmentData(const Uint32 * nodegroups, Uint32 cnt);

    /**
     * Get Fragment Data (array of node groups)
     */
    const Uint32 *getFragmentData() const;
    Uint32 getFragmentDataLen() const;

    /**
     * Set array of information mapping range values and list values
     * to fragments.
     *
     * For range, this is a sorted list of range values
     * For list, this is a list of pairs { value, partition }
     */
    int setRangeListData(const Int32* data, Uint32 cnt);

    /**
     * Get Range or List Array (value, partition)
     */
    const Int32 *getRangeListData() const;
    Uint32 getRangeListDataLen() const;

    /**
     * Get list of nodes storing given fragment, primary
     * is normally entry 0
     * Returns : 0 for error, > 0 for fragment count
     * If fragment count is > arraySize param, only arraySize
     * entries are written.
     */
    Uint32 getFragmentNodes(Uint32 fragmentId, 
                            Uint32* nodeIdArrayPtr,
                            Uint32 arraySize) const;

    /**
     * Set table object type
     */
    void setObjectType(Object::Type type);

    /**
     * Set/Get Maximum number of rows in table (only used to calculate
     * number of partitions).
     */
    void setMaxRows(Uint64 maxRows);
    Uint64 getMaxRows() const;

    /**
     * Set/Get Minimum number of rows in table (only used to calculate
     * number of partitions).
     */
    void setMinRows(Uint64 minRows);
    Uint64 getMinRows() const;

    /**
     * Set/Get SingleUserMode
     */
    void setSingleUserMode(enum SingleUserMode);
    enum SingleUserMode getSingleUserMode() const;


    /** @} *******************************************************************/

    /**
     * 
     */
    void setRowGCIIndicator(bool value);
    bool getRowGCIIndicator() const;

    void setRowChecksumIndicator(bool value);
    bool getRowChecksumIndicator() const;
 
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
    const char *getMysqlName() const;

    void setStoredTable(bool x) { setLogging(x); }
    bool getStoredTable() const { return getLogging(); }

    int getRowSizeInBytes() const ;
    int createTableInDb(Ndb*, bool existingEqualIsOk = true) const ;

    int getReplicaCount() const ;

    bool getTemporary() const;
    void setTemporary(bool); 

    /**
     * Only table with varpart do support online add column
     *   Add property so that table wo/ varsize column(s) still
     *   allocates varpart-ref, so that later online add column is possible
     */
    bool getForceVarPart() const;
    void setForceVarPart(bool);

    /**
     * Check if any of column in bitmaps are disk columns
     *   returns bitmap of different columns
     *     bit 0 = atleast 1 pk column is set
     *     bit 1 = atleast 1 disk column set
     *     bit 2 = atleast 1 non disk column set
     *   passing NULL pointer will equal to bitmap with all columns set
     */
    int checkColumns(const Uint32* bitmap, unsigned len_in_bytes) const;

    /**
     * Set tableId,tableVersion on a table...
     *   this is a "work-around" since createIndex can't (currently)
     *   accept an ObjectId instead of table-object in createIndex
     *   this as way way too much stuff is pushed into NdbDictInterface
     */
    void assignObjId(const ObjectId &);

    /**
     * set/get table-storage-method
     */
    void setStorageType(Column::StorageType);
    Column::StorageType getStorageType() const;

    /**
     * Get/set extra GCI bits (max 31)
     */
    void setExtraRowGciBits(Uint32);
    Uint32 getExtraRowGciBits() const;

    /**
     * Get/set extra row author bits (max 31)
     */
    void setExtraRowAuthorBits(Uint32);
    Uint32 getExtraRowAuthorBits() const;
#endif

    // these 2 are not de-doxygenated

    /**
     * This method is not needed in normal usage.
     *
     * Compute aggregate data on table being defined.  Required for
     * aggregate methods such as getNoOfPrimaryKeys() to work before
     * table has been created and retrieved via getTable().
     *
     * May adjust some column flags.  If no PK is so far marked as
     * distribution key then all PK's will be marked.
     *
     * Returns 0 on success.  Returns -1 and sets error if an
     * inconsistency is detected.
     */
    int aggregate(struct NdbError& error);

    /**
     * This method is not needed in normal usage.
     *
     * Validate new table definition before create.  Does aggregate()
     * and additional checks.  There may still be errors which are
     * detected only by NDB kernel at create table.
     *
     * Create table and retrieve table do validate() automatically.
     *
     * Returns 0 on success.  Returns -1 and sets error if an
     * inconsistency is detected.
     */
    int validate(struct NdbError& error);

    /**
     * Return partitionId given a hashvalue
     *   Note, if table is not retreived (e.i using getTable) result
     *   will most likely be wrong
     */
    Uint32 getPartitionId(Uint32 hashvalue) const ;

    /*
     * Return TRUE if any of the columns in the table have a 
     * non NULL default value defined
     */ 
    bool hasDefaultValues() const;

  private:
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
    friend class Ndb;
    friend class NdbDictionaryImpl;
    friend class NdbTableImpl;
    friend class NdbEventOperationImpl;
#endif
    class NdbTableImpl & m_impl;
    Table(NdbTableImpl&);
  };
  
  /**
   * @class Index
   * @brief Represents an index in an NDB Cluster
   */
  class Index : public Object {
  public:
    
    /** 
     * @name Getting Index properties
     * @{
     */

    /**
     * Get the name of an index
     */
    const char * getName() const;
    
    /**
     * Get the name of the underlying table being indexed
     */
    const char * getTable() const;

    /**
     * Get the number of columns in the index
     */
    unsigned getNoOfColumns() const;

#ifndef DOXYGEN_SHOULD_SKIP_DEPRECATED
    /**
     * Get the number of columns in the index
     * Deprecated, use getNoOfColumns instead.
     */
    int getNoOfIndexColumns() const;
#endif

    /**
     * Get a specific column in the index
     */
    const Column * getColumn(unsigned no) const ;

#ifndef DOXYGEN_SHOULD_SKIP_DEPRECATED
    /**
     * Get a specific column name in the index
     * Deprecated, use getColumn instead.
     */
    const char * getIndexColumn(int no) const ;
#endif

    /**
     * Represents type of index
     */
    enum Type {
      Undefined = 0,          ///< Undefined object type (initial value)
      UniqueHashIndex = 3,    ///< Unique un-ordered hash index 
                              ///< (only one currently supported)
      OrderedIndex = 6        ///< Non-unique ordered index
    };

    /**
     * Get index type of the index
     */
    Type getType() const;
    
    /**
     * Check if index is set to be stored on disk
     *
     * @return if true then logging is enabled
     *
     * @note Non-logged indexes are rebuilt at system restart.
     * @note Ordered index does not currently support logging.
     */
    bool getLogging() const;

    /**
     * Get object status
     */
    virtual Object::Status getObjectStatus() const;

    /**
     * Get object version
     */
    virtual int getObjectVersion() const;

    /**
     * Get object id
     */
    virtual int getObjectId() const;

    /**
     * Get default NdbRecord object for this index
     * This NdbRecord object becomes invalid at the same time as
     * the index object does - when the ndb_cluster_connection 
     * is closed.
     */
    const NdbRecord* getDefaultRecord() const;

    /** @} *******************************************************************/

    /** 
     * @name Index creation
     * @{
     *
     * These methods should normally not be used in an application as
     * the result will not be visible from the MySQL Server
     *
     */

    /**
     *  Constructor
     *  @param  name  Name of index
     */
    Index(const char * name = "");
    virtual ~Index();

    /**
     * Set the name of an index
     */
    int setName(const char * name);

    /**
     * Define the name of the table to be indexed
     */
    int setTable(const char * name);

    /**
     * Add a column to the index definition
     * Note that the order of columns will be in
     * the order they are added (only matters for ordered indexes).
     */
    int addColumn(const Column & c);

    /**
     * Add a column name to the index definition
     * Note that the order of indexes will be in
     * the order they are added (only matters for ordered indexes).
     */
    int addColumnName(const char * name);

#ifndef DOXYGEN_SHOULD_SKIP_DEPRECATED
    /**
     * Add a column name to the index definition
     * Note that the order of indexes will be in
     * the order they are added (only matters for ordered indexes).
     * Deprecated, use addColumnName instead.
     */
    int addIndexColumn(const char * name);
#endif

    /**
     * Add several column names to the index definition
     * Note that the order of indexes will be in
     * the order they are added (only matters for ordered indexes).
     */
    int  addColumnNames(unsigned noOfNames, const char ** names);

#ifndef DOXYGEN_SHOULD_SKIP_DEPRECATED
    /**
     * Add several column names to the index definition
     * Note that the order of indexes will be in
     * the order they are added (only matters for ordered indexes).
     * Deprecated, use addColumnNames instead.
     */
    int addIndexColumns(int noOfNames, const char ** names);
#endif

    /**
     * Set index type of the index
     */
    void setType(Type type);

    /**
     * Enable/Disable index storage on disk
     *
     * @param enable  If enable is set to true, then logging becomes enabled
     *
     * @see NdbDictionary::Index::getLogging
     */
    void setLogging(bool enable); 

#ifndef DOXYGEN_SHOULD_SKIP_DEPRECATED
    void setStoredIndex(bool x) { setLogging(x); }
    bool getStoredIndex() const { return getLogging(); }

    bool getTemporary() const;
    void setTemporary(bool); 
#endif
    
    /** @} *******************************************************************/

  private:
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
    friend class NdbIndexImpl;
    friend class NdbIndexStat;
#endif
    class NdbIndexImpl & m_impl;
    Index(NdbIndexImpl&);
  };

  /**
   * @brief Represents a Table Optimization Handle
   * Passed as argument to optimizeTable
   */
  class OptimizeTableHandle {
  public:
    /**
     * Supported operations for OptimizeTableHandle
     */
    OptimizeTableHandle();
    ~OptimizeTableHandle();
    /**
     * Optimize one more batch of records
     * @return 1 for more records left to optimize,
     *         0 when completed
     *         -1 encountered some error 
     */
    int next();
    /**
     * Close the handle object
     * @return 0 when completed
     *         -1 encountered some error      
     */
    int close();
  private:
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
    friend class NdbOptimizeTableHandleImpl;
    friend class NdbOptimizeIndexHandleImpl;
    friend class NdbDicitionaryImpl;
#endif
    class NdbOptimizeTableHandleImpl & m_impl;
    OptimizeTableHandle(NdbOptimizeTableHandleImpl &);
  };

  /**
   * @brief Represents a Index Optimization Handle
   * passed as argument to optimizeIndex
   */
  class OptimizeIndexHandle {
  public:
    /**
     * Supported operations for OptimizeIndexHandle
     */
    OptimizeIndexHandle();
    ~OptimizeIndexHandle();
    /**
     * Optimize one more batch of records
     * @return 1 for more records left to optimize,
     *         0 when completed
     *         -1 encountered some error 
     */
    int next();
    /**
     * Close the handle object
     * @return 0 when completed
     *         -1 encountered some error      
     */
    int close();
  private:
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
    friend class NdbOptimizeIndexHandleImpl;
    friend class NdbDicitionaryImpl;
#endif
    class NdbOptimizeIndexHandleImpl & m_impl;
    OptimizeIndexHandle(NdbOptimizeIndexHandleImpl &);
  };

  /**
   * @brief Represents an Event in NDB Cluster
   *
   */
  class Event : public Object  {
  public:
    /**
     * Specifies the type of database operations an Event listens to
     */
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
    /** TableEvent must match 1 << TriggerEvent */
#endif
    enum TableEvent { 
      TE_INSERT      =1<<0, ///< Insert event on table
      TE_DELETE      =1<<1, ///< Delete event on table
      TE_UPDATE      =1<<2, ///< Update event on table
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
      TE_SCAN        =1<<3, ///< Scan event on table
      TE_FIRST_NON_DATA_EVENT =1<<4,
#endif
      TE_DROP        =1<<4, ///< Drop of table
      TE_ALTER       =1<<5, ///< Alter of table
      TE_CREATE      =1<<6, ///< Create of table
      TE_GCP_COMPLETE=1<<7, ///< GCP is complete
      TE_CLUSTER_FAILURE=1<<8, ///< Cluster is unavailable
      TE_STOP        =1<<9, ///< Stop of event operation
      TE_NODE_FAILURE=1<<10, ///< Node failed
      TE_SUBSCRIBE   =1<<11, ///< Node subscribes
      TE_UNSUBSCRIBE =1<<12, ///< Node unsubscribes
      TE_EMPTY         =1<<15, ///< Empty epoch from data nodes
      TE_INCONSISTENT  =1<<21, ///< MISSING_DATA (buffer overflow) at data node
      TE_OUT_OF_MEMORY =1<<22, ///< Buffer overflow in event buffer
      TE_ALL=0xFFFF         ///< Any/all event on table (not relevant when 
                            ///< events are received)
    };
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
    enum _TableEvent { 
      _TE_INSERT=0,
      _TE_DELETE=1,
      _TE_UPDATE=2,
      _TE_SCAN=3,
      _TE_FIRST_NON_DATA_EVENT=4,
      _TE_DROP=4,
      _TE_ALTER=5,
      _TE_CREATE=6,
      _TE_GCP_COMPLETE=7,
      _TE_CLUSTER_FAILURE=8,
      _TE_STOP=9,
      _TE_NODE_FAILURE=10,
      _TE_SUBSCRIBE=11,
      _TE_UNSUBSCRIBE=12,
      _TE_NUL=13, // internal (e.g. INS o DEL within same GCI)
      _TE_ACTIVE=14, // internal (node becomes active)
      _TE_EMPTY=15,
      _TE_INCONSISTENT=21,
      _TE_OUT_OF_MEMORY=22
    };
#endif
    /**
     *  Specifies the durability of an event
     * (future version may supply other types)
     */
    enum EventDurability { 
      ED_UNDEFINED
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
      = 0
#endif
#if 0 // not supported
      ,ED_SESSION = 1, 
      // Only this API can use it
      // and it's deleted after api has disconnected or ndb has restarted
      
      ED_TEMPORARY = 2
      // All API's can use it,
      // But's its removed when ndb is restarted
#endif
      ,ED_PERMANENT    ///< All API's can use it.
                       ///< It's still defined after a cluster system restart
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
      = 3
#endif
    };

    /**
     * Specifies reporting options for table events
     */
    enum EventReport {
      ER_UPDATED = 0,
      ER_ALL = 1, // except not-updated blob inlines
      ER_SUBSCRIBE = 2,
      ER_DDL = 4
    };

    /**
     *  Constructor
     *  @param  name  Name of event
     */
    Event(const char *name);
    /**
     *  Constructor
     *  @param  name  Name of event
     *  @param  table Reference retrieved from NdbDictionary
     */
    Event(const char *name, const NdbDictionary::Table& table);
    virtual ~Event();
    /**
     * Set unique identifier for the event
     */
    int setName(const char *name);
    /**
     * Get unique identifier for the event
     */
    const char *getName() const;
    /**
     * Get table that the event is defined on
     *
     * @return pointer to table or NULL if no table has been defined
     */
    const NdbDictionary::Table * getTable() const;
    /**
     * Define table on which events should be detected
     *
     * @note calling this method will default to detection
     *       of events on all columns. Calling subsequent
     *       addEventColumn calls will override this.
     *
     * @param table reference retrieved from NdbDictionary
     */
    void setTable(const NdbDictionary::Table& table);
    /**
     * Set table for which events should be detected
     *
     * @note preferred way is using setTable(const NdbDictionary::Table&)
     *       or constructor with table object parameter
     */
    int setTable(const NdbDictionary::Table *table);
    int setTable(const char *tableName);
    /**
     * Get table name for events
     *
     * @return table name
     */
    const char* getTableName() const;
    /**
     * Add type of event that should be detected
     */
    void addTableEvent(const TableEvent te);
    /**
     * Check if a specific table event will be detected
     */
    bool getTableEvent(const TableEvent te) const;
    /**
     * Set durability of the event
     */
    void setDurability(EventDurability);
    /**
     * Get durability of the event
     */
    EventDurability getDurability() const;
    /**
     * Set report option of the event
     */
    void setReport(EventReport);
    /**
     * Get report option of the event
     */
    EventReport getReport() const;
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
    void addColumn(const Column &c);
#endif
    /**
     * Add a column on which events should be detected
     *
     * @param attrId Column id
     *
     * @note errors will mot be detected until createEvent() is called
     */
    void addEventColumn(unsigned attrId);
    /**
     * Add a column on which events should be detected
     *
     * @param columnName Column name
     *
     * @note errors will not be detected until createEvent() is called
     */
    void addEventColumn(const char * columnName);
    /**
     * Add several columns on which events should be detected
     *
     * @param n Number of columns
     * @param columnNames Column names
     *
     * @note errors will mot be detected until 
     *       NdbDictionary::Dictionary::createEvent() is called
     */
    void addEventColumns(int n, const char ** columnNames);

    /**
     * Get no of columns defined in an Event
     *
     * @return Number of columns, -1 on error
     */
    int getNoOfEventColumns() const;

    /**
     * Get a specific column in the event
     */
    const Column * getEventColumn(unsigned no) const;

    /**
     * The merge events flag is false by default.  Setting it true
     * implies that events are merged in following ways:
     *
     * - for given NdbEventOperation associated with this event,
     *   events on same PK within same GCI are merged into single event
     *
     * - a blob table event is created for each blob attribute
     *   and blob events are handled as part of main table events
     *
     * - blob post/pre data from the blob part events can be read
     *   via NdbBlob methods as a single value
     *
     * NOTE: Currently this flag is not inherited by NdbEventOperation
     * and must be set on NdbEventOperation explicitly.
     */
    void mergeEvents(bool flag);

    /**
     * Get object status
     */
    virtual Object::Status getObjectStatus() const;

    /**
     * Get object version
     */
    virtual int getObjectVersion() const;

    /**
     * Get object id
     */
    virtual int getObjectId() const;

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
    void print();
#endif

  private:
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
    friend class NdbEventImpl;
    friend class NdbEventOperationImpl;
#endif
    class NdbEventImpl & m_impl;
    Event(NdbEventImpl&);
  };

  /* Flags for createRecord(). */
  enum NdbRecordFlags {
    /*
      Use special mysqld varchar format in index keys, used only from
      inside mysqld.
    */
    RecMysqldShrinkVarchar= 0x1,
    /* Use the mysqld record format for bitfields, only used inside mysqld. */
    RecMysqldBitfield= 0x2,
    /* Use the column specific flags from RecordSpecification. */
    RecPerColumnFlags= 0x4
  };
  struct RecordSpecification {
    /*
      Size of the RecordSpecification structure.
    */
    static inline Uint32 size()
    {
        return sizeof(RecordSpecification);
    }

    /*
      Column described by this entry (the column maximum size defines field
      size in row).
      Note that even when creating an NdbRecord for an index, the column
      pointers must be to columns obtained from the underlying table, not
      from the index itself.
      Note that pseudo columns cannot be used as part of a RecordSpecification.
      To read pesudo column values, use the extra get value and set value 
      APIs.
    */
    const Column *column;
    /*
      Offset of data from start of a row.
      
      For reading blobs, the blob handle (NdbBlob *) will be written into the
      result row when the operation is created, not the actual blob data. 
      So at least sizeof(NdbBlob *) must be available in the row.  Other 
      operations do not write the blob handle into the row.
      In any case, a blob handle can always be obtained with a call to 
      NdbOperation/NdbScanOperation::getBlobHandle().
    */
    Uint32 offset;
    /*
      Offset from start of row of byte containing NULL bit.
      Not used for columns that are not NULLable.
    */
    Uint32 nullbit_byte_offset;
    /* NULL bit, 0-7. Not used for columns that are not NULLable. */
    Uint32 nullbit_bit_in_byte;
    /*
      Column specific flags
      Used only when RecPerColumnFlags is enabled
    */
    enum ColumnFlags
    {
      /*
        Skip reading/writing overflow bits in bitmap
        Used for MySQLD char(0) column
        Used only with RecMysqldBitfield flag
      */
      BitColMapsNullBitOnly= 0x1
    };
    Uint32 column_flags;
  };

  /*
    First version of RecordSpecification
    Maintained here for backward compatibility reasons.
  */
  struct RecordSpecification_v1 {
    const Column *column;
    Uint32 offset;
    Uint32 nullbit_byte_offset;
    Uint32 nullbit_bit_in_byte;
  };

  /* Types of NdbRecord object */
  enum RecordType {
    TableAccess,
    IndexAccess
  };
  
  /*
    Return the type of the passed NdbRecord object
  */
  static RecordType getRecordType(const NdbRecord* record);
  
  /*
    Return the name of the table object that the NdbRecord
    refers to.
    This method returns Null if the NdbRecord object is not a 
    TableAccess NdbRecord.
  */
  static const char* getRecordTableName(const NdbRecord* record);
  
  /*
    Return the name of the index object that the NdbRecord
    refers to.
    This method returns Null if the NdbRecord object is not an
    IndexAccess NdbRecord
  */
  static const char* getRecordIndexName(const NdbRecord* record);
  
  /*
    Get the first Attribute Id specified in the NdbRecord object.
    Returns false if no Attribute Ids are specified.
  */
  static bool getFirstAttrId(const NdbRecord* record, Uint32& firstAttrId);

  /* Get the next Attribute Id specified in the NdbRecord object
     after the attribute Id passed in.
     Returns false if there are no more attribute Ids
  */
  static bool getNextAttrId(const NdbRecord* record, Uint32& attrId);

  /* Get offset of the given attribute id's storage from the start
     of the NdbRecord row.
     Returns false if the attribute id is not present
  */
  static bool getOffset(const NdbRecord* record, Uint32 attrId, Uint32& offset);
  
  /* Get offset of the given attribute id's null bit from the start
     of the NdbRecord row.
     Returns false if the attribute is not present or if the
     attribute is not nullable
  */
  static bool getNullBitOffset(const NdbRecord* record, 
                               Uint32 attrId, 
                               Uint32& nullbit_byte_offset,
                               Uint32& nullbit_bit_in_byte);

  /*
    Return pointer to beginning of storage of data specified by
    attrId.
    This method looks up the offset of the column which is stored in
    the NdbRecord object, and returns the value of row + offset.
    There are row-const and non-row-const versions.
    
    @param record : Pointer to NdbRecord object describing the row format
    @param row : Pointer to the start of row data
    @param attrId : Attribute id of column
    @return : Pointer to start of the attribute in the row.  Null if the
    attribute is not part of the NdbRecord definition
  */
  static const char* getValuePtr(const NdbRecord* record,
                                 const char* row,
                                 Uint32 attrId);
  
  static char* getValuePtr(const NdbRecord* record,
                           char* row,
                           Uint32 attrId);
  
  /*
    Return a bool indicating whether the null bit for the given
    column is set to true or false.
    The location of the null bit in relation to the row pointer is
    obtained from the passed NdbRecord object.
    If the column is not nullable, false will be returned.
    If the column is not part of the NdbRecord definition, false will
    be returned.
    
    @param record : Pointer to NdbRecord object describing the row format
    @param row : Pointer to the start of row data
    @param attrId : Attibute id of column
    @return : true if attrId exists in NdbRecord, is nullable, and null bit
    in row is set, false otherwise.
  */
  static bool isNull(const NdbRecord* record,
                     const char* row,
                     Uint32 attrId);
  
  /*
    Set the null bit for the given column to the supplied value.
    The offset for the null bit is obtained from the passed 
    NdbRecord object.
    
    If the attrId is not part of the NdbRecord, or is not nullable
    then an error will be returned.
    
    @param record : Pointer to NdbRecord object describing the row format
    @param row : Pointer to the start of row data
    @param atrId : Attribute id of the column
    @param value : Value to set null bit to
    @returns : 0 in success, -1 if the attrId is not part of the record,
    or is not nullable
  */
  static int setNull(const NdbRecord* record,
                     char* row,
                     Uint32 attrId,
                     bool value);
  
  /*
    Return the number of bytes needed to store one row of data
    laid out as described by the passed NdbRecord structure.
  */
  static Uint32 getRecordRowLength(const NdbRecord* record);
  
  /*
    Return an empty column presence bitmask.
    This bitmask can be used with any NdbRecord to specify that
    no NdbRecord columns are to be included in the operation.
  */
  static const unsigned char* getEmptyBitmask();
  
  struct AutoGrowSpecification {
    Uint32 min_free;
    Uint64 max_size;
    Uint64 file_size;
    const char * filename_pattern;
  };

  /**
   * @class LogfileGroup
   */
  class LogfileGroup : public Object {
  public:
    LogfileGroup();
    LogfileGroup(const LogfileGroup&);
    virtual ~LogfileGroup();

    void setName(const char * name);
    const char* getName() const;

    void setUndoBufferSize(Uint32 sz);
    Uint32 getUndoBufferSize() const;
    
    void setAutoGrowSpecification(const AutoGrowSpecification&);
    const AutoGrowSpecification& getAutoGrowSpecification() const;

    Uint64 getUndoFreeWords() const;

    /**
     * Get object status
     */
    virtual Object::Status getObjectStatus() const;

    /**
     * Get object version
     */
    virtual int getObjectVersion() const;

    /**
     * Get object id
     */
    virtual int getObjectId() const;

  private:
    friend class NdbDictionaryImpl;
    friend class NdbLogfileGroupImpl;
    class NdbLogfileGroupImpl & m_impl;
    LogfileGroup(NdbLogfileGroupImpl&);
  };

  /**
   * @class Tablespace
   */
  class Tablespace : public Object {
  public:
    Tablespace();
    Tablespace(const Tablespace&);
    virtual ~Tablespace();

    void setName(const char * name);
    const char* getName() const;

    void setExtentSize(Uint32 sz);
    Uint32 getExtentSize() const;

    void setAutoGrowSpecification(const AutoGrowSpecification&);
    const AutoGrowSpecification& getAutoGrowSpecification() const;

    void setDefaultLogfileGroup(const char * name);
    void setDefaultLogfileGroup(const class LogfileGroup&);

    const char * getDefaultLogfileGroup() const;
    Uint32 getDefaultLogfileGroupId() const;
    
    /**
     * Get object status
     */
    virtual Object::Status getObjectStatus() const;

    /**
     * Get object version
     */
    virtual int getObjectVersion() const;

    /**
     * Get object id
     */
    virtual int getObjectId() const;

  private:
    friend class NdbTablespaceImpl;
    class NdbTablespaceImpl & m_impl;
    Tablespace(NdbTablespaceImpl&);
  };

  class Datafile : public Object {
  public:
    Datafile();
    Datafile(const Datafile&);
    virtual ~Datafile();

    void setPath(const char * name);
    const char* getPath() const;
  
    void setSize(Uint64);
    Uint64 getSize() const;
    Uint64 getFree() const;
    
    int setTablespace(const char * name);
    int setTablespace(const class Tablespace &);
    const char * getTablespace() const;
    void getTablespaceId(ObjectId * dst) const;

    void setNode(Uint32 nodeId);
    Uint32 getNode() const;

    Uint32 getFileNo() const;

    /**
     * Get object status
     */
    virtual Object::Status getObjectStatus() const;

    /**
     * Get object version
     */
    virtual int getObjectVersion() const;

    /**
     * Get object id
     */
    virtual int getObjectId() const;

  private:
    friend class NdbDatafileImpl;
    class NdbDatafileImpl & m_impl;
    Datafile(NdbDatafileImpl&);
  };

  class Undofile : public Object {
  public:
    Undofile();
    Undofile(const Undofile&);
    virtual ~Undofile();

    void setPath(const char * path);
    const char* getPath() const;
  
    void setSize(Uint64);
    Uint64 getSize() const;

    void setLogfileGroup(const char * name);
    void setLogfileGroup(const class LogfileGroup &);
    const char * getLogfileGroup() const;
    void getLogfileGroupId(ObjectId * dst) const;

    void setNode(Uint32 nodeId);
    Uint32 getNode() const;

    Uint32 getFileNo() const;

    /**
     * Get object status
     */
    virtual Object::Status getObjectStatus() const;

    /**
     * Get object version
     */
    virtual int getObjectVersion() const;

    /**
     * Get object id
     */
    virtual int getObjectId() const;

  private:
    friend class NdbUndofileImpl;
    class NdbUndofileImpl & m_impl;
    Undofile(NdbUndofileImpl&);
  };

  /**
   * @class HashMap
   * @brief Represents a HashMap in an NDB Cluster
   *
   */
  class HashMap : public Object {
  public:
    HashMap();
    HashMap(const HashMap&);
    virtual ~HashMap();

    void setName(const char *);
    const char * getName() const;

    void setMap(const Uint32* values, Uint32 len);
    Uint32 getMapLen() const;
    int getMapValues(Uint32* dst, Uint32 len) const;

    /**
     * equal
     *   compares *values* only
     */
    bool equal(const HashMap&) const;

    /**
     * Get object status
     */
    virtual Object::Status getObjectStatus() const;

    /**
     * Get object version
     */
    virtual int getObjectVersion() const;

    /**
     * Get object id
     */
    virtual int getObjectId() const;

  private:
    friend class NdbHashMapImpl;
    class NdbHashMapImpl & m_impl;
    HashMap(NdbHashMapImpl&);
  };

  /**
   * @class ForeignKey
   * @brief Represents a foreign key in an NDB Cluster
   *
   */
  class ForeignKey : public Object {
  public:
    ForeignKey();
    ForeignKey(const ForeignKey&);
    virtual ~ForeignKey();

    enum FkAction
    {
      NoAction = NDB_FK_NO_ACTION, // deferred check
      Restrict = NDB_FK_RESTRICT,
      Cascade = NDB_FK_CASCADE,
      SetNull = NDB_FK_SET_NULL,
      SetDefault = NDB_FK_SET_DEFAULT
    };

    const char * getName() const;
    const char * getParentTable() const;
    const char * getChildTable() const;
    unsigned getParentColumnCount() const;
    unsigned getChildColumnCount() const;
    int getParentColumnNo(unsigned no) const;
    int getChildColumnNo(unsigned no) const;

    /**
     * return 0 if child referes to parent PK
     */
    const char * getParentIndex() const;

    /**
     * return 0 if child references are resolved using child PK
     */
    const char * getChildIndex() const;

    FkAction getOnUpdateAction() const;
    FkAction getOnDeleteAction() const;

    /**
     *
     */
    void setName(const char *);

    /**
     * specify parent/child table
     * optionally an index
     * and columns in parent/child table (optionally)
     *
     * if index is not specified primary key is used
     *
     * if columns is not specified, index order is used
     *
     * if columns and index is specified, and index is ordered index
     *   column order must match given column order
     *
     */
    void setParent(const Table&, const Index * index = 0,
                   const Column * cols[] = 0);
    void setChild(const Table&, const Index * index = 0,
                  const Column * cols[] = 0);

    void setOnUpdateAction(FkAction);
    void setOnDeleteAction(FkAction);

    /**
     * Get object status
     */
    virtual Object::Status getObjectStatus() const;

    /**
     * Get object id
     */
    virtual int getObjectId() const;

    /**
     * Get object version
     */
    virtual int getObjectVersion() const;

  private:
    friend class NdbForeignKeyImpl;
    class NdbForeignKeyImpl & m_impl;
    ForeignKey(NdbForeignKeyImpl&);
  };

  /**
   * @class Dictionary
   * @brief Dictionary for defining and retreiving meta data
   */
  class Dictionary {
  public:
    /**
     * @class List
     * @brief Structure for retrieving lists of object names
     */
    struct List {
      /**
       * @struct  Element
       * @brief   Object to be stored in an NdbDictionary::Dictionary::List
       */
      struct Element {
	unsigned id;            ///< Id of object
        Object::Type type;      ///< Type of object
        Object::State state;    ///< State of object
        Object::Store store;    ///< How object is logged
        Uint32 temp;            ///< Temporary status of object
	char * database;        ///< In what database the object resides 
	char * schema;          ///< What schema the object is defined in
	char * name;            ///< Name of object
        Element() :
          id(0),
          type(Object::TypeUndefined),
          state(Object::StateUndefined),
          store(Object::StoreUndefined),
          temp(NDB_TEMP_TAB_PERMANENT),
	  database(0),
	  schema(0),
          name(0) {
        }
      };
      unsigned count;           ///< Number of elements in list
      Element * elements;       ///< Pointer to array of elements
      List() : count(0), elements(0) {}
      ~List() {
        if (elements != 0) {
          for (unsigned i = 0; i < count; i++) {
            delete[] elements[i].database;
            delete[] elements[i].schema;
            delete[] elements[i].name;
            elements[i].name = 0;
          }
          delete[] elements;
          count = 0;
          elements = 0;
        }
      }
    };

    /** 
     * @name General
     * @{
     */

    /**
     * Fetch list of all objects, optionally restricted to given type.
     *
     * @param list   List of objects returned in the dictionary
     * @param type   Restrict returned list to only contain objects of
     *               this type
     *
     * @return       -1 if error.
     *
     */
#ifndef DOXYGEN_SHOULD_SKIP_DEPRECATED
    int listObjects(List & list, Object::Type type = Object::TypeUndefined);
#endif
    int listObjects(List & list,
		    Object::Type type = Object::TypeUndefined) const;
    int listObjects(List & list,
                    Object::Type type,
                    bool fullyQualified) const;

    /**
     * Get the latest error
     *
     * @return   Error object.
     */			     
    const struct NdbError & getNdbError() const;

    /**
     * Get warning flags.  The value is valid only if the operation did
     * not return an error and can return warnings.  The flags are
     * specific to the operation.
     */
    int getWarningFlags() const;

    /** @} *******************************************************************/

    /** 
     * @name Retrieving references to Tables and Indexes
     * @{
     */

    /**
     * Get table with given name, NULL if undefined
     * @param name   Name of table to get
     * @return table if successful otherwise NULL.
     */
    const Table * getTable(const char * name) const;

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
    /**
     * Given main table, get blob table.
     */
    const Table * getBlobTable(const Table *, const char * col_name);
    const Table * getBlobTable(const Table *, Uint32 col_no);

    /*
     * Save a table definition in dictionary cache
     * @param table Object to put into cache
     */
    void putTable(const Table * table);
#endif

    /**
     * Get index with given name, NULL if undefined
     * @param indexName  Name of index to get.
     * @param tableName  Name of table that index belongs to.
     * @return  index if successful, otherwise 0.
     */
    const Index * getIndex(const char * indexName,
			   const char * tableName) const;
    const Index * getIndex(const char * indexName,
                           const Table& base) const;

    /**
     * Fetch list of indexes of given table.
     * @param list  Reference to list where to store the listed indexes
     * @param tableName  Name of table that index belongs to.
     * @return  0 if successful, otherwise -1
     */
#ifndef DOXYGEN_SHOULD_SKIP_DEPRECATED
    int listIndexes(List & list, const char * tableName);
#endif
    int listIndexes(List & list, const char * tableName) const;

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
    /**
     * Fetch list of indexes of given table.
     * @param list  Reference to list where to store the listed indexes
     * @param table  Reference to table that index belongs to.
     * @return  0 if successful, otherwise -1
     */
    int listIndexes(List & list, const Table &table) const;

    /**
     * Fetch list of objects that table depend on
     * @param list  Reference to list where to store the listed objects
     * @param table  Reference to table that objects belongs to.
     * @return  0 if successful, otherwise -1
     */
    int listDependentObjects(List & list, const Table &table) const;
#endif

    /** @} *******************************************************************/
    /** 
     * @name Events
     * @{
     */
    
    /**
     * Create event given defined Event instance
     * @param event Event to create
     * @return 0 if successful otherwise -1.
     */
    int createEvent(const Event &event);

    /**
     * Drop event with given name
     * @param eventName  Name of event to drop.
     * @return 0 if successful otherwise -1.
     */
    int dropEvent(const char * eventName, int force= 0);
    
    /**
     * Get event with given name.
     * @param eventName  Name of event to get.
     * @return an Event if successful, otherwise NULL.
     */
    const Event * getEvent(const char * eventName);

    /**
     * List defined events
     * @param list   List of events returned in the dictionary
     * @return 0 if successful otherwise -1.
     */
#ifndef DOXYGEN_SHOULD_SKIP_DEPRECATED
    int listEvents(List & list);
#endif
    int listEvents(List & list) const;

    /** @} *******************************************************************/

    /** 
     * @name Table creation
     * @{
     *
     * These methods should normally not be used in an application as
     * the result will not be visible from the MySQL Server
     */

    /**
     * Create defined table given defined Table instance
     * @param table Table to create
     * @return 0 if successful otherwise -1.
     */
    int createTable(const Table &table);

    /**
     * Create defined table given defined Table instance
     *   return ObjectId
     * @param table Table to create
     * @return 0 if successful otherwise -1.
     */
    int createTable(const Table &table, ObjectId * objid);

    /**
     * Start table optimization given defined table object
     * @param t Object of table to optimize
     * @param Pre-allocated OptimizeTableHandle
     * @return 0 if successful otherwise -1.
     */
    int
    optimizeTable(const Table &t, OptimizeTableHandle &h);

    /**
     * Start index optimization given defined index object
     * @param ind Object of index to optimize
     * @param Pre-allocated OptimizeIndexHandle
     * @return 0 if successful otherwise -1.
     */
    int
    optimizeIndex(const Index &ind, OptimizeIndexHandle &h);

    /**
     * Drop table given retrieved Table instance
     * @param table Table to drop
     * @return 0 if successful otherwise -1.
     *
     * @note dropTable() drops indexes and foreign keys
     * where the table is child or parent
     */
    int dropTable(Table & table);

    /**
     * Drop table given table name
     * @param name   Name of table to drop 
     * @return 0 if successful otherwise -1.
     */
    int dropTable(const char * name);
    
    /**
     * Check if alter of table given defined
     * Table instance to new definition is supported
     * @param f Table to alter
     * @param t New definition of table
     * @return  TRUE supported      <br>
     *          FALSE not supported <br>
     */
    bool supportedAlterTable(const Table & f, const Table & t);

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
    /**
     * Alter defined table given defined Table instance
     * @param f Table to alter
     * @param t New definition of table
     * @return  -2 (incompatible version) <br>
     *          -1 general error          <br>
     *           0 success                 
     */
    int alterTable(const Table & f, const Table & t);

    /**
     * Invalidate cached table object
     * @param name  Name of table to invalidate
     */
    void invalidateTable(const char * name);
#endif

    /**
     * Remove table from local cache
     */
    void removeCachedTable(const char * table);
    /**
     * Remove index from local cache
     */
    void removeCachedIndex(const char * index, const char * table);

    
    /** @} *******************************************************************/
    /** 
     * @name Index creation
     * @{
     *
     * These methods should normally not be used in an application as
     * the result will not be visible from the MySQL Server
     *
     */
    
    /**
     * Create index given defined Index instance
     * @param index Index to create
     * @return 0 if successful otherwise -1.
     */
    int createIndex(const Index &index, bool offline = false);
    int createIndex(const Index &index, const Table &table, bool offline = false);

    /**
     * Drop index with given name
     * @param indexName  Name of index to drop.
     * @param tableName  Name of table that index belongs to.
     * @return 0 if successful otherwise -1.
     */
    int dropIndex(const char * indexName,
		  const char * tableName);

    /*
     * Force update of ordered index stats.  Scans an assigned fragment
     * in the kernel and updates result in stats tables.  This one-time
     * update is independent of IndexStatAuto settings.  Common use case
     * is mysql "analyze table".
     */
    int updateIndexStat(const Index&, const Table&);

    /*
     * Force update of ordered index stats where index is given by id.
     */
    int updateIndexStat(Uint32 indexId, Uint32 indexVersion, Uint32 tableId);

    /*
     * Delete ordered index stats.  If IndexStatAutoUpdate is set, also
     * stops automatic updates, until another forced update is done.
     */
    int deleteIndexStat(const Index&, const Table&);

    /*
     * Delete ordered index stats where index is given by id.
     */
    int deleteIndexStat(Uint32 indexId, Uint32 indexVersion, Uint32 tableId);
    
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
    void removeCachedTable(const Table *table);
    void removeCachedIndex(const Index *index);
    void invalidateTable(const Table *table);
    /**
     * Invalidate cached index object
     */
    void invalidateIndex(const char * indexName,
                         const char * tableName);
    void invalidateIndex(const Index *index);
    /**
     * Force gcp and wait for gcp complete
     */
    int forceGCPWait();
    int forceGCPWait(int type);

    /**
     * Get restart gci
     */
    int getRestartGCI(Uint32 * gci);
#endif

    /** @} *******************************************************************/

    /** @} *******************************************************************/
    /** 
     * @name Disk data objects
     * @{
     */
    
    /*
     * The four "create" operations can return warning flags defined
     * below.  See getWarningFlags().
     */
    enum {
      WarnUndobufferRoundUp = 0x1,  // rounded up to kernel page size
      WarnUndofileRoundDown = 0x2,  // rounded down to kernel page size
      WarnExtentRoundUp = 0x4,      // rounded up to kernel page size
      WarnDatafileRoundDown = 0x8,  // rounded down to kernel page size
      WarnDatafileRoundUp = 0x10    // rounded up to extent size
    };

    int createLogfileGroup(const LogfileGroup &, ObjectId* = 0);
    int dropLogfileGroup(const LogfileGroup&);
    LogfileGroup getLogfileGroup(const char * name);

    int createTablespace(const Tablespace &, ObjectId* = 0);
    int dropTablespace(const Tablespace&);
    Tablespace getTablespace(const char * name);
    Tablespace getTablespace(Uint32 tablespaceId);

    int createDatafile(const Datafile &, bool overwrite_existing = false, ObjectId* = 0);
    int dropDatafile(const Datafile&);
    Datafile getDatafile(Uint32 node, const char * path);
    
    int createUndofile(const Undofile &, bool overwrite_existing = false, ObjectId * = 0);
    int dropUndofile(const Undofile&);
    Undofile getUndofile(Uint32 node, const char * path);
    

    /** @} *******************************************************************/
    /**
     * @name HashMap
     * @{
     */

    /**
     * Create a HashMap in database
     */
    int createHashMap(const HashMap&, ObjectId* = 0);

    /**
     * Get a HashMap by name
     */
    int getHashMap(HashMap& dst, const char* name);

    /**
     * Get a HashMap for a table
     */
    int getHashMap(HashMap& dst, const Table* table);

    /**
     * Get default HashMap
     */
    int getDefaultHashMap(HashMap& dst, Uint32 fragments);
    int getDefaultHashMap(HashMap& dst, Uint32 buckets, Uint32 fragments);


    /**
     * Init a default HashMap
     */
    int initDefaultHashMap(HashMap& dst, Uint32 fragments);
    int initDefaultHashMap(HashMap& dst, Uint32 buckets, Uint32 fragments);

    /**
     * create (or retreive) a HashMap suitable for alter
     * NOTE: Requires a started schema transaction
     */
    int prepareHashMap(const Table& oldTable, Table& newTable);
    int prepareHashMap(const Table& oldTable, Table& newTable, Uint32 buckets);

    /** @} *******************************************************************/

    /** @} *******************************************************************/
    /**
     * @name ForeignKey
     * @{
     */

    enum CreateFKFlags
    {
      /**
       * CreateFK_NoVerify
       * - don't verify FK as part of Create.
       * - @NOTE: This allows creation of inconsistent FK
       */
      CreateFK_NoVerify = 1
    };

    /**
     * Create a ForeignKey in database
     */
    int createForeignKey(const ForeignKey&, ObjectId* = 0, int flags = 0);

    /**
     * Get a ForeignKey by name
     */
    int getForeignKey(ForeignKey& dst, const char* name);

    /**
     * Drop a ForeignKey
     */
    int dropForeignKey(const ForeignKey&);

    /** @} *******************************************************************/

    /**
     * @name Schema transactions
     *
     * Metadata operations are create, alter, and drop of objects of
     * various types.  An operation may create additional sub-operations
     * in the kernel.
     *
     * By default, each user operation is executed separately.  That is,
     * a schema transaction is started implicitly, the operation and its
     * suboperations are executed, and the transaction is closed.
     *
     * The Ndb object and its associated Dictionary support one schema
     * transaction at a time.
     *
     * Using begin and end transaction explicitly it is possible to
     * execute a set of user defined operations atomically i.e. either
     * all operations succeed or all are aborted (rolled back).
     *
     * The steps are 1) beginSchemaTrans 2) submit operations such as
     * createTable 3) endSchemaTrans.
     *
     * Each operation is sent to the kernel which parses and saves it.
     * Parse failure does rollback to previous user operation before
     * returning.  The user can continue or abort entire transaction.
     *
     * After all operations have been submitted, endSchemaTrans with
     * flags 0 (the default) processes and commits them.  On error
     * return the transaction is already aborted.
     *
     * If the user exits before calling endSchemaTrans, the kernel
     * aborts the transaction.  If the user exits before the call to
     * endSchemaTrans returns, the kernel continues with the request.
     * Completion status is reported in cluster log.
     */

    //@{
    /**
     * Begin schema transaction.  Returns error if a transaction is
     * already active or if the kernel metadata is locked.
     *
     * @return 0 on success, -1 on error
     */
    int beginSchemaTrans();

    /**
     * End schema transaction, with commit or with abort.  Combines
     * execute and close which do not exist separately.  May be called
     * and succeeds even if no transaction is active.
     *
     * @note Like any method, may overwrite current error code.
     *       First save error code from any failed operation.
     *
     * @param flags
     *        Bitmask of options.
     *        Default 0 commits the transaction.
     *        Including option 1 aborts the transaction.
     *        See SchemaTransFlag for others.
     * @return 0 on success, -1 on error
     */
    int endSchemaTrans(Uint32 flags = 0);

    /**
     * Flags for endSchemaTrans, or-ed together.
     */
    enum SchemaTransFlag {
      // abort transaction
      SchemaTransAbort = 1,
      // do not wait for reply, status is reported in cluster log
      SchemaTransBackground = 2
    };

    /**
     * Check if a schema transaction exists currently.
     */
    bool hasSchemaTrans() const;
    //@}

  protected:
    Dictionary(Ndb & ndb);
    ~Dictionary();
    
  private:
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
    friend class NdbDictionaryImpl;
    friend class UtilTransactions;
    friend class NdbBlob;
#endif
    class NdbDictionaryImpl & m_impl;
    Dictionary(NdbDictionaryImpl&);
    const Table * getIndexTable(const char * indexName,
                                const char * tableName) const;
  public:
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
    const Table * getTable(const char * name, void **data) const;
    void set_local_table_data_size(unsigned sz);

    const Index * getIndexGlobal(const char * indexName,
                                 const Table &ndbtab) const;
    const Index * getIndexGlobal(const char * indexName,
                                 const char * tableName) const;
    const Table * getTableGlobal(const char * tableName) const;
    int alterTableGlobal(const Table &f, const Table &t);
    int dropTableGlobal(const Table &ndbtab);
    /* Flags for second variant of dropTableGlobal */
    enum {
      /*
       * Drop any referring foreign keys on child tables.
       * Named after oracle "drop table .. cascade constraints".
       */
      DropTableCascadeConstraints = 0x1

      /*
       * Drop any referring foreign keys within same DB
       *   used when dropping database
       */
      ,DropTableCascadeConstraintsDropDB = 0x2
    };
    int dropTableGlobal(const Table &ndbtab, int flags);
    int dropIndexGlobal(const Index &index);
    int removeIndexGlobal(const Index &ndbidx, int invalidate) const;
    int removeTableGlobal(const Table &ndbtab, int invalidate) const;
    void invalidateDbGlobal(const char * dbname);
#endif

    /*
      Create an NdbRecord for use in table operations.
    */
    NdbRecord *createRecord(const Table *table,
                            const RecordSpecification *recSpec,
                            Uint32 length,
                            Uint32 elemSize,
                            Uint32 flags= 0);

    /*
      Create an NdbRecord for use in index operations.
    */
    NdbRecord *createRecord(const Index *index,
                            const Table *table,
                            const RecordSpecification *recSpec,
                            Uint32 length,
                            Uint32 elemSize,
                            Uint32 flags= 0);
    /*
      Create an NdbRecord for use in index operations.
      This variant assumes that the index is for a table in 
      the current database and schema
    */
    NdbRecord *createRecord(const Index *index,
                            const RecordSpecification *recSpec,
                            Uint32 length,
                            Uint32 elemSize,
                            Uint32 flags= 0);

    /*
      Free an NdbRecord object created earlier with
      createRecord
    */
    void releaseRecord(NdbRecord *rec);

    /*
      Methods to print objects more verbose than possible from
      object itself.
     */
    void print(class NdbOut& out, NdbDictionary::Index const& idx);
    void print(class NdbOut& out, NdbDictionary::Table const& tab);
  }; // class Dictionary

  class NdbDataPrintFormat
  {
  public:
    NdbDataPrintFormat();
    virtual ~NdbDataPrintFormat();
    const char *lines_terminated_by;
    const char *fields_terminated_by;
    const char *start_array_enclosure;
    const char *end_array_enclosure;
    const char *fields_enclosed_by;
    const char *fields_optionally_enclosed_by;
    const char *hex_prefix;
    const char *null_string;
    int hex_format;
  };

  static 
  class NdbOut& printFormattedValue(class NdbOut& out, 
                                    const NdbDataPrintFormat& format,
                                    const NdbDictionary::Column* c,
                                    const void* val);
  
}; // class NdbDictionary

class NdbOut& operator <<(class NdbOut& out, const NdbDictionary::Column& col);
class NdbOut& operator <<(class NdbOut& out, const NdbDictionary::Index& idx);
class NdbOut& operator <<(class NdbOut& out, const NdbDictionary::Index::Type type);
class NdbOut& operator <<(class NdbOut& out, const NdbDictionary::Object::FragmentType fragtype);
class NdbOut& operator <<(class NdbOut& out, const NdbDictionary::Object::Status status);
class NdbOut& operator <<(class NdbOut& out, const NdbDictionary::Object::Type type);
class NdbOut& operator <<(class NdbOut& out, const NdbDictionary::Table& tab);

#endif
