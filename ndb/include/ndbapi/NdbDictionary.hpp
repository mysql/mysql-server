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

/*****************************************************************************
 * Name:          NdbDictionary.hpp
 * Include:
 * Link:
 * Author:        Jonas Oreland
 * Date:          2003-05-14
 * Version:       0.1
 * Description:   Data dictionary support
 * Documentation:
 * Adjust:  2003-05-14  Jonas Oreland   First version.
 ****************************************************************************/

#ifndef NdbDictionary_H
#define NdbDictionary_H

#include <ndb_types.h>

class Ndb;

/**
 * @class NdbDictionary
 * @brief Data dictionary class
 * 
 * This class supports all schema data definition and enquiry such as:
 * -# Creating tables (Dictionary::createTable) and table columns
 * -# Dropping tables (Dictionary::dropTable)
 * -# Creating secondary indexes (Dictionary::createIndex)
 * -# Dropping secondary indexes (Dictionary::dropIndex)
 * -# Enquiries about tables
 *    (Dictionary::getTable, Table::getNoOfColumns, 
 *    Table::getPrimaryKey, and Table::getNoOfPrimaryKeys)
 * -# Enquiries about indexes
 *    (Dictionary::getIndex, Index::getNoOfColumns, 
 *    and Index::getColumn)
 *
 * NdbDictionary has several help (inner) classes:
 * -# NdbDictionary::Table for creating tables
 * -# NdbDictionary::Column for creating table columns
 * -# NdbDictionary::Index for creating secondary indexes
 * 
 * See @ref ndbapi_example4.cpp for details of usage.
 */
class NdbDictionary {
public:
  /**
   * @class Object
   * @brief Meta information about a database object (a table, index, etc)
   */
  class Object {
  public:
    /**
     * Status of object
     */
    enum Status {
      New,                    ///< The object only exist in memory and 
                              ///< has not been created in the NDB Kernel
      Changed,                ///< The object has been modified in memory 
                              ///< and has to be commited in NDB Kernel for 
                              ///< changes to take effect
      Retrieved               ///< The object exist and has been read 
                              ///< into main memory from NDB Kernel
    };

    /**
     * Get status of object
     */
    virtual Status getObjectStatus() const = 0;

    /**
     * Get version of object
     */
    virtual int getObjectVersion() const = 0;

    /**
     * Object type
     */
    enum Type {
      TypeUndefined = 0,      ///< Undefined
      SystemTable = 1,        ///< System table
      UserTable = 2,          ///< User table (may be temporary)
      UniqueHashIndex = 3,    ///< Unique un-ordered hash index
      HashIndex = 4,          ///< Non-unique un-ordered hash index
      UniqueOrderedIndex = 5, ///< Unique ordered index
      OrderedIndex = 6,       ///< Non-unique ordered index
      HashIndexTrigger = 7,   ///< Index maintenance, internal
      IndexTrigger = 8,       ///< Index maintenance, internal
      SubscriptionTrigger = 9,///< Backup or replication, internal
      ReadOnlyConstraint = 10 ///< Trigger, internal
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
      StateBroken = 9         ///< Broken, should be dropped and re-created
    };

    /**
     * Object store
     */
    enum Store {
      StoreUndefined = 0,     ///< Undefined
      StoreTemporary = 1,     ///< Object or data deleted on system restart
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
      FragAllSmall = 2,       ///< One fragment per node group
      FragAllMedium = 3,      ///< Default value. Two fragments per node group.
      FragAllLarge = 4        ///< Eight fragments per node group.
    };
  };

  class Table; // forward declaration
  
  /**
   * @class Column
   * @brief Represents an column in an NDB Cluster table
   *
   * Each column has a type. The type of a column is determind by a number 
   * of type specifiers.
   * The type specifiers are:
   * - Builtin type
   * - Array length or max length
   * - Precision and scale
   */
  class Column {
  public:
    /**
     * The builtin column types
     */
    enum Type {
      Undefined=0,///< Undefined 
      Tinyint,       ///< 8 bit. 1 byte signed integer, can be used in array
      Tinyunsigned,  ///< 8 bit. 1 byte unsigned integer, can be used in array
      Smallint,      ///< 16 bit. 2 byte signed integer, can be used in array
      Smallunsigned, ///< 16 bit. 2 byte unsigned integer, can be used in array
      Mediumint,     ///< 24 bit. 3 byte signed integer, can be used in array
      Mediumunsigned,///< 24 bit. 3 byte unsigned integer, can be used in array
      Int,           ///< 32 bit. 4 byte signed integer, can be used in array
      Unsigned,      ///< 32 bit. 4 byte unsigned integer, can be used in array
      Bigint,        ///< 64 bit. 8 byte signed integer, can be used in array
      Bigunsigned,   ///< 64 Bit. 8 byte signed integer, can be used in array
      Float,         ///< 32-bit float. 4 bytes float, can be used in array
      Double,        ///< 64-bit float. 8 byte float, can be used in array
      Decimal,       ///< Precision, Scale are applicable
      Char,          ///< Len. A fixed array of 1-byte chars
      Varchar,       ///< Max len
      Binary,        ///< Len
      Varbinary,     ///< Max len
      Datetime,    ///< Precision down to 1 sec (sizeof(Datetime) == 8 bytes )
      Timespec,    ///< Precision down to 1 nsec(sizeof(Datetime) == 12 bytes )
      Blob,        ///< Binary large object (see NdbBlob)
      Text         ///< Text blob
    };

    /** 
     * @name General 
     * @{
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
    void setName(const char * name);

    /**
     * Get name of column
     * @return  Name of the column
     */
    const char* getName() const;

    /**
     * Set whether column is nullable or not
     */
    void setNullable(bool);

    /**
     * Get if the column is nullable or not
     */
    bool getNullable() const;
    
    /**
     * Set that column is part of primary key
     */
    void setPrimaryKey(bool);

    /**
     * Check if column is part of primary key
     */
    bool getPrimaryKey() const;

    /**
     *  Get number of column (horizontal position within table)
     */
    int getColumnNo() const;

    /**
     * Check if column is equal to some other column
     * @param  column  Column to compare with
     * @return  true if column is equal to some other column otherwise false.
     */
    bool equal(const Column& column) const;

    /** @} *******************************************************************/
    /** 
     * @name Type Specifiers
     * @{
     */

    /**
     * Set type of column
     * @param  type  Type of column
     */
    void setType(Type type);

    /**
     * Get type of column
     */
    Type getType() const;

    /**
     * Set precision of column.
     * @note Only applicable for builtin type Decimal
     */
    void setPrecision(int);

    /**
     * Get precision of column.
     * @note Only applicable for builtin type Decimal
     */
    int getPrecision() const;

    /**
     * Set scale of column.
     * @note Only applicable for builtin type Decimal
     */
    void setScale(int);

    /**
     * Get scale of column.
     * @note Only applicable for builtin type Decimal
     */
    int getScale() const;

    /**
     * Set length for column
     * Array length for column or max length for variable length arrays.
     */
    void setLength(int length);

    /**
     * Get length for column
     * Array length for column or max length for variable length arrays.
     */
    int getLength() const;

    /**
     * For blob, set or get "inline size" i.e. number of initial bytes
     * to store in table's blob attribute.  This part is normally in
     * main memory and can be indexed and interpreted.
     */
    void setInlineSize(int size) { setPrecision(size); }
    int getInlineSize() const { return getPrecision(); }

    /**
     * For blob, set or get "part size" i.e. number of bytes to store in
     * each tuple of the "blob table".  Can be set to zero to omit parts
     * and to allow only inline bytes ("tinyblob").
     */
    void setPartSize(int size) { setScale(size); }
    int getPartSize() const { return getScale(); }

    /**
     * For blob, set or get "stripe size" i.e. number of consecutive
     * <em>parts</em> to store in each node group.
     */
    void setStripeSize(int size) { setLength(size); }
    int getStripeSize() const { return getLength(); }

    /**
     * Get size of element
     */
    int getSize() const;

    /** 
     * Set distribution key
     *
     * A <em>distribution key</em> is a set of attributes which are used
     * to distribute the tuples onto the NDB nodes.
     * The distribution key uses the NDB Cluster hashing function.
     *
     * An example where this is useful is TPC-C where it might be
     * good to use the warehouse id and district id as the distribution key. 
     * This would place all data for a specific district and warehouse 
     * in the same database node.
     *
     * Locally in the fragments the full primary key 
     * will still be used with the hashing algorithm.
     *
     * @param  enable  If set to true, then the column will be part of 
     *                 the distribution key.
     */
    void setDistributionKey(bool enable);

    /**
     * Check if column is part of distribution key
     * @see setDistributionKey
     */
    bool getDistributionKey() const;
    /** @} *******************************************************************/
    
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
    void setTupleKey(bool);
    bool getTupleKey() const;
    
    void setDistributionGroup(bool, int bits = 16);
    bool getDistributionGroup() const;
    int getDistributionGroupBits() const;
    
    void setIndexOnlyStorage(bool);
    bool getIndexOnlyStorage() const;

    const Table * getBlobTable() const;

    /** 
     * @name ODBC Specific methods 
     * @{
     */
    void setAutoIncrement(bool);         
    bool getAutoIncrement() const;
    void setAutoIncrementInitialValue(Uint64 val);
    void setDefaultValue(const char*);   
    const char* getDefaultValue() const;
    /** @} *******************************************************************/

    static const Column * FRAGMENT;
    static const Column * ROW_COUNT;
    static const Column * COMMIT_COUNT;
#endif
    
  private:
    friend class NdbRecAttr;
    friend class NdbColumnImpl;
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
    /** 
     * @name General
     * @{
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
    Table& operator=(const Table&);

    /**
     * Name of table
     * @param  name  Name of table
     */
    void setName(const char * name);

    /**
     * Get table name
     */
    const char * getName() const;

    /**
     * Get table id
     */ 
    int getTableId() const;
    
    /**
     * Add a column definition to a table
     * @note creates a copy
     */
    void addColumn(const Column &);
    
    /**
     * Get column definition via name.
     * @return null if none existing name
     */
    const Column* getColumn(const char * name) const;
    
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
    void setLogging(bool); 

    /**
     * @see NdbDictionary::Table::setLogging.
     */
    bool getLogging() const;
   
    /**
     * Set fragmentation type
     */
    void setFragmentType(FragmentType);

    /**
     * Get fragmentation type
     */
    FragmentType getFragmentType() const;
    
    /**
     * Set KValue (Hash parameter.)
     * Only allowed value is 6.
     * Later implementations might add flexibility in this parameter.
     */
    void setKValue(int kValue);
    
    /**
     * Get KValue (Hash parameter.)
     * Only allowed value is 6.
     * Later implementations might add flexibility in this parameter.
     */
    int getKValue() const;

    /**
     * Set MinLoadFactor  (Hash parameter.)
     * This value specifies the load factor when starting to shrink 
     * the hash table. 
     * It must be smaller than MaxLoadFactor.
     * Both these factors are given in percentage.
     */
    void setMinLoadFactor(int);

    /**
     * Get MinLoadFactor  (Hash parameter.)
     * This value specifies the load factor when starting to shrink 
     * the hash table. 
     * It must be smaller than MaxLoadFactor.
     * Both these factors are given in percentage.
     */
    int getMinLoadFactor() const;

    /**
     * Set MaxLoadFactor  (Hash parameter.)
     * This value specifies the load factor when starting to split 
     * the containers in the local hash tables. 
     * 100 is the maximum which will optimize memory usage.
     * A lower figure will store less information in each container and thus
     * find the key faster but consume more memory.
     */
    void setMaxLoadFactor(int);

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
     * Set frm file to store with this table
     */ 
    void setFrm(const void* data, Uint32 len);

    /**
     * Set table object type
     */
    void setObjectType(Object::Type type);

    /**
     * Get table object type
     */
    Object::Type getObjectType() const;

    /**
     * Get object status
     */
    virtual Object::Status getObjectStatus() const;

    /**
     * Get object version
     */
    virtual int getObjectVersion() const;

    /** @} *******************************************************************/

#ifndef DOXYGEN_SHOULD_SKIP_DEPRECATED
    void setStoredTable(bool x) { setLogging(x); }
    bool getStoredTable() const { return getLogging(); }

    int getRowSizeInBytes() const ;
    int createTableInDb(Ndb*, bool existingEqualIsOk = true) const ;
#endif

  private:
    friend class NdbTableImpl;
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
     *  Constructor
     *  @param  name  Name of index
     */
    Index(const char * name = "");
    virtual ~Index();

    /**
     * Set the name of an index
     */
    void setName(const char * name);
    
    /**
     * Get the name of an index
     */
    const char * getName() const;
    
    /**
     * Define the name of the table to be indexed
     */
    void setTable(const char * name);

    /**
     * Get the name of the table being indexed
     */
    const char * getTable() const;
    
    /**
     * Get the number of columns in the index
     */
    unsigned getNoOfColumns() const;

    /**
     * Get the number of columns in the index
     * Depricated, use getNoOfColumns instead.
     */
    int getNoOfIndexColumns() const;

    /**
     * Get a specific column in the index
     */
    const NdbDictionary::Column * getColumn(unsigned no) const ;

    /**
     * Get a specific column name in the index
     * Depricated, use getColumn instead.
     */
    const char * getIndexColumn(int no) const ;

    /**
     * Add a column to the index definition
     * Note that the order of columns will be in
     * the order they are added (only matters for ordered indexes).
     */
    void addColumn(const Column & c);

    /**
     * Add a column name to the index definition
     * Note that the order of indexes will be in
     * the order they are added (only matters for ordered indexes).
     */
    void addColumnName(const char * name);

    /**
     * Add a column name to the index definition
     * Note that the order of indexes will be in
     * the order they are added (only matters for ordered indexes).
     * Depricated, use addColumnName instead.
     */
    void addIndexColumn(const char * name);

    /**
     * Add several column names to the index definition
     * Note that the order of indexes will be in
     * the order they are added (only matters for ordered indexes).
     */
    void addColumnNames(unsigned noOfNames, const char ** names);

    /**
     * Add several column names to the index definition
     * Note that the order of indexes will be in
     * the order they are added (only matters for ordered indexes).
     * Depricated, use addColumnNames instead.
     */
    void addIndexColumns(int noOfNames, const char ** names);

    /**
     * Represents type of index
     */
    enum Type {
      Undefined = 0,          ///< Undefined object type (initial value)
      UniqueHashIndex = 3,    ///< Unique un-ordered hash index 
                              ///< (only one currently supported)
      HashIndex = 4,          ///< Non-unique un-ordered hash index
      UniqueOrderedIndex = 5, ///< Unique ordered index
      OrderedIndex = 6        ///< Non-unique ordered index
    };

    /**
     * Set index type of the index
     */
    void setType(Type type);

    /**
     * Get index type of the index
     */
    Type getType() const;
    
    /**
     * Enable/Disable index storage on disk
     *
     * @param enable  If enable is set to true, then logging becomes enabled
     *
     * @see NdbDictionary::Table::setLogging
     *
     * @note Non-logged indexes are rebuilt at system restart.
     * @note Ordered index does not currently support logging.
     */
    void setLogging(bool enable); 

    /**
     * Check if index is set to be stored on disk
     *
     * @see NdbDictionary::Index::setLogging
     */
    bool getLogging() const;

#ifndef DOXYGEN_SHOULD_SKIP_DEPRECATED
    void setStoredIndex(bool x) { setLogging(x); }
    bool getStoredIndex() const { return getLogging(); }
#endif
    
    /**
     * Get object status
     */
    virtual Object::Status getObjectStatus() const;

    /**
     * Get object version
     */
    virtual int getObjectVersion() const;

  private:
    friend class NdbIndexImpl;

    class NdbIndexImpl & m_impl;
    Index(NdbIndexImpl&);
  };

  /**
   * @brief Represents an Event in NDB Cluster
   *
   */
  class Event : public Object  {
  public:
    enum TableEvent { TE_INSERT=1, TE_DELETE=2, TE_UPDATE=4, TE_ALL=7 };
    enum EventDurability { 
      ED_UNDEFINED = 0,
#if 0 // not supported
      ED_SESSION = 1, 
      // Only this API can use it
      // and it's deleted after api has disconnected or ndb has restarted
      
      ED_TEMPORARY = 2,
      // All API's can use it,
      // But's its removed when ndb is restarted
#endif      
      ED_PERMANENT = 3
      // All API's can use it,
      // It's still defined after a restart
    };
    
    Event(const char *name);
    virtual ~Event();
    void setName(const char *);
    void setTable(const char *);
    void addTableEvent(const TableEvent);
    void setDurability(const EventDurability);
    void addColumn(const Column &c);
    void addEventColumn(unsigned attrId);
    void addEventColumn(const char * columnName);
    void addEventColumns(int n, const char ** columnNames);

    /**
     * Get object status
     */
    virtual Object::Status getObjectStatus() const;

    /**
     * Get object version
     */
    virtual int getObjectVersion() const;

    void print();

  private:
    friend class NdbEventImpl;
    friend class NdbEventOperationImpl;
    class NdbEventImpl & m_impl;
    Event(NdbEventImpl&);
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
        Object::Store store;    ///< How object is stored
	char * database;        ///< In what database the object resides 
	char * schema;          ///< What schema the object is defined in
	char * name;            ///< Name of object
        Element() :
          id(0),
          type(Object::TypeUndefined),
          state(Object::StateUndefined),
          store(Object::StoreUndefined),
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
     */
    int listObjects(List & list, Object::Type type = Object::TypeUndefined);

    /**
     * Get the latest error
     *
     * @return   Error object.
     */			     
    const struct NdbError & getNdbError() const;

    /** @} *******************************************************************/
    /** 
     * @name Tables
     * @{
     */

    /**
     * Create defined table given defined Table instance
     * @param Table Table to create
     * @return 0 if successful otherwise -1.
     */
    int createTable(const Table &);

    /**
     * Drop table given retrieved Table instance
     * @param Table Table to drop
     * @return 0 if successful otherwise -1.
     */
    int dropTable(Table &);

    /**
     * Drop table given table name
     * @param name   Name of table to drop 
     * @return 0 if successful otherwise -1.
     */
    int dropTable(const char * name);
    
    /**
     * Alter defined table given defined Table instance
     * @param Table Table to alter
     * @return  -2 (incompatible version) <br>
     *          -1 general error          <br>
     *           0 success                 
     */
    int alterTable(const Table &);

    /**
     * Get table with given name, NULL if undefined
     * @param name   Name of table to get
     * @return table if successful otherwise NULL.
     */
    const Table * getTable(const char * name);

    /**
     * Get table with given name for alteration.
     * @param name   Name of table to alter
     * @return table if successful. NULL if undefined
     */
    Table getTableForAlteration(const char * name);

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
    /**
     * Invalidate cached table object
     * @param name  Name of table to invalidate
     */
    void invalidateTable(const char * name);
#endif

    /**
     * Remove table/index from local cache
     */
    void removeCachedTable(const char * table);
    void removeCachedIndex(const char * index, const char * table);

    
    /** @} *******************************************************************/
    /** 
     * @name Indexes
     * @{
     */
    
    /**
     * Create index given defined Index instance
     * @param Index to create
     * @return 0 if successful otherwise -1.
     */
    int createIndex(const Index &);

    /**
     * Drop index with given name
     * @param indexName  Name of index to drop.
     * @param tableName  Name of table that index belongs to.
     * @return 0 if successful otherwise -1.
     */
    int dropIndex(const char * indexName,
		  const char * tableName);
    
    /**
     * Get index with given name, NULL if undefined
     * @param indexName  Name of index to get.
     * @param tableName  Name of table that index belongs to.
     * @return  index if successful, otherwise 0.
     */
    const Index * getIndex(const char * indexName,
			   const char * tableName);

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
    /**
     * Invalidate cached index object
     */
    void invalidateIndex(const char * indexName,
                         const char * tableName);
#endif

    /**
     * Fetch list of indexes of given table.
     * @param list  Reference to list where to store the listed indexes
     * @param tableName  Name of table that index belongs to.
     * @return  0 if successful, otherwise -1
     */
    int listIndexes(List & list, const char * tableName);

    /** @} *******************************************************************/
    /** 
     * @name Events
     * @{
     */
    
    /**
     * Create event given defined Event instance
     * @param Event to create
     * @return 0 if successful otherwise -1.
     */
    int createEvent(const Event &);

    /**
     * Drop event with given name
     * @param eventName  Name of event to drop.
     * @return 0 if successful otherwise -1.
     */
    int dropEvent(const char * eventName);
    
    /**
     * Get event with given name.
     * @param eventName  Name of event to get.
     * @return an Event if successful, otherwise NULL.
     */
    const Event * getEvent(const char * eventName);

    /** @} *******************************************************************/
    
  protected:
    Dictionary(Ndb & ndb);
    ~Dictionary();
    
  private:
    friend class NdbDictionaryImpl;
    friend class UtilTransactions;
    friend class NdbBlob;
    class NdbDictionaryImpl & m_impl;
    Dictionary(NdbDictionaryImpl&);
    const Table * getIndexTable(const char * indexName, 
				const char * tableName);
  public:
    const Table * getTable(const char * name, void **data);
    void set_local_table_data_size(unsigned sz);
  };
};

class NdbOut& operator <<(class NdbOut& out, const NdbDictionary::Column& col);

#endif
