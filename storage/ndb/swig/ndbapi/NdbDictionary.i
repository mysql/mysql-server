/* -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
 *  vim:expandtab:shiftwidth=2:tabstop=2:smarttab:
 *
 *  Copyright (C) 2008 MySQL, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
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

class NdbDictObject {
public:

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



  enum Type {
    TypeUndefined = 0,      ///< Undefined
    SystemTable = 1,        ///< System table
    UserTable = 2,          ///< User table (may be temporary)
    UniqueHashIndex = 3,    ///< Unique un-ordered hash index
    OrderedIndex = 6,       ///< Non-unique ordered index
    HashIndexTrigger = 7,   ///< NdbDictionary::Index maintenance, internal
    IndexTrigger = 8,       ///< NdbDictionary::Index maintenance, internal
    SubscriptionTrigger = 9,///< Backup or replication, internal
    ReadOnlyConstraint = 10 ///< Trigger, internal
  };

  enum State {
    StateUndefined = 0,     ///< Undefined
    StateOffline = 1,       ///< Offline, not usable
    StateBuilding = 2,      ///< Building, not yet usable
    StateDropping = 3,      ///< Offlining or dropping, not usable
    StateOnline = 4,        ///< Online, usable
    StateBackup = 5,        ///< Online, being backuped, usable
    StateBroken = 9         ///< Broken, should be dropped and re-created
  };

  enum Store {
    StoreUndefined = 0,     ///< Undefined
    StoreNotLogged = 1,     ///< Object or data deleted on system restart
    StorePermanent = 2      ///< Permanent. logged to disk
  };

  enum FragmentType {
    FragUndefined = 0,      ///< Fragmentation type undefined or default
    FragSingle = 1,         ///< Only one fragment
    FragAllSmall = 2,       ///< One fragment per node, default
    FragAllMedium = 3,      ///< two fragments per node
    FragAllLarge = 4,       ///< Four fragments per node.
    DistrKeyHash = 5,
    DistrKeyLin = 6,
    UserDefined = 7
  };

  virtual NdbDictObject::Status getObjectStatus() const = 0;

  virtual int getObjectVersion() const = 0;

  virtual int getObjectId() const = 0;

};


class NdbDictTable; // forward declaration

class NdbDictColumn {
public:
  enum Type {
    Undefined = NDB_TYPE_UNDEFINED,
    Tinyint = NDB_TYPE_TINYINT,
    Tinyunsigned = NDB_TYPE_TINYUNSIGNED,
    Smallint = NDB_TYPE_SMALLINT,
    Smallunsigned = NDB_TYPE_SMALLUNSIGNED,
    Mediumint = NDB_TYPE_MEDIUMINT,
    Mediumunsigned = NDB_TYPE_MEDIUMUNSIGNED,
    Int = NDB_TYPE_INT,
    Unsigned = NDB_TYPE_UNSIGNED,
    Bigint = NDB_TYPE_BIGINT,
    Bigunsigned = NDB_TYPE_BIGUNSIGNED,
    Float = NDB_TYPE_FLOAT,
    Double = NDB_TYPE_DOUBLE,
    Olddecimal = NDB_TYPE_OLDDECIMAL,
    Olddecimalunsigned = NDB_TYPE_OLDDECIMALUNSIGNED,
    Decimal = NDB_TYPE_DECIMAL,
    Decimalunsigned = NDB_TYPE_DECIMALUNSIGNED,
    Char = NDB_TYPE_CHAR,
    Varchar = NDB_TYPE_VARCHAR,
    Binary = NDB_TYPE_BINARY,
    Varbinary = NDB_TYPE_VARBINARY,
    Datetime = NDB_TYPE_DATETIME,
    Date = NDB_TYPE_DATE,
    Blob = NDB_TYPE_BLOB,
    Text = NDB_TYPE_TEXT,
    Bit = NDB_TYPE_BIT,
    Longvarchar = NDB_TYPE_LONGVARCHAR,
    Longvarbinary = NDB_TYPE_LONGVARBINARY,
    Time = NDB_TYPE_TIME,
    Year = NDB_TYPE_YEAR,
    Timestamp = NDB_TYPE_TIMESTAMP
  };
  const char* getName() const;
  bool getNullable() const;
  bool getPrimaryKey() const;
  int getColumnNo() const;
  bool equal(const NdbDictColumn& column) const;
  Type getType() const;
  int getPrecision() const;
  int getScale() const;
  int getLength() const;
//    CHARSET_INFO* getCharset() const;
  int getInlineSize() const;
  int getPartSize() const;
  int getStripeSize() const;
  int getSize() const;
  bool getPartitionKey() const;
  NdbDictColumn(const char * name = "");
  NdbDictColumn(const NdbDictColumn& column);
  ~NdbDictColumn();
  void setName(const char * name);
  void setNullable(bool);
  void setPrimaryKey(bool);
  void setType(Type type);
  void setPrecision(int);
  void setScale(int);
  void setLength(int length);
// TODO: CHARSET_INFO
//    void setCharset(CHARSET_INFO* cs);
  void setInlineSize(int size);
  void setPartSize(int size);
  void setStripeSize(int size);
  void setPartitionKey(bool enable);
  const NdbDictTable * getBlobTable() const;

  void setAutoIncrement(bool);
  bool getAutoIncrement() const;
  void setAutoIncrementInitialValue(Uint64 val);
  void setDefaultValue(const char*);
  const char* getDefaultValue() const;

#if defined(MYSQL_50)
  // The problem here is actually a Python issue, but this clears it
  // for the moment
  // <FIXME>
  static const NdbDictColumn * FRAGMENT;
  static const NdbDictColumn * FRAGMENT_MEMORY;
  static const NdbDictColumn * ROW_COUNT;
  static const NdbDictColumn * COMMIT_COUNT;
  static const NdbDictColumn * ROW_SIZE;
  static const NdbDictColumn * RANGE_NO;
#endif

  int getSizeInBytes() const;
};

%extend NdbDictColumn {
public:
  const char * getCharsetName() {
    const CHARSET_INFO * csinfo = self->getCharset();
    return csinfo->csname;
  }
  Uint32 getCharsetNumber() {
    const CHARSET_INFO * csinfo = self->getCharset();
    return csinfo->number;
  }
}

typedef NdbDictColumn Attribute;

class NdbDictTable : public NdbDictObject {
public:
  const char * getName() const;
  int getTableId() const;
  NdbDictColumn* getColumn(const int attributeId);
  NdbDictColumn* getColumn(const char * name);
  bool getLogging() const;
  NdbDictObject::FragmentType getFragmentType() const;
  int getKValue() const;
  int getMinLoadFactor() const;
  int getMaxLoadFactor() const;
  int getNoOfColumns() const;
  int getNoOfPrimaryKeys() const;
  const char* getPrimaryKey(int no) const;
  bool equal(const NdbDictTable&) const;
/* There is really no need to expose these */
//  const void* getFrmData() const;
//  Uint32 getFrmLength() const;
//  void setFrm(const void* data, Uint32 len);
  NdbDictTable(const char * name = "");
  NdbDictTable(const NdbDictTable& table);
  virtual ~Table();
  //NdbDictTable& operator=(const NdbDictTable& table);
  void setName(const char * name);
  void addColumn(const NdbDictColumn &);
  void setLogging(bool);
  void setFragmentType(NdbDictObject::FragmentType);
  void setKValue(int kValue);
  void setMinLoadFactor(int);
  void setMaxLoadFactor(int);
  virtual NdbDictObject::Status getObjectStatus() const;
  virtual int getObjectVersion() const;
// TODO: This is not definied in libndbclient.so but is in the .hpp file
//  void setObjectType(NdbDictObject::Type type);
//  NdbDictObject::Type getObjectType() const;
  void setMaxRows(Uint64 maxRows);
  Uint64 getMaxRows() const;
  void setMinRows(Uint64 minRows);
  Uint64 getMinRows() const;
  int getRowSizeInBytes() const ;
  int createTableInDb(Ndb*, bool existingEqualIsOk = true) const ;
  int getReplicaCount() const ;
};

class NdbDictIndex : public NdbDictObject {
public:

  enum Type {
    Undefined = 0,          ///< Undefined object type (initial value)
    UniqueHashIndex = 3,    ///< Unique un-ordered hash index
    ///< (only one currently supported)
    OrderedIndex = 6        ///< Non-unique ordered index
  };

  %ndbexception("NdbApiException") {
    $action
      if (result==0) {
        NDB_exception(NdbApiException,"NdbDictionary::Index Error");
      }
  }

  const char * getName() const;
  const char * getTable() const;

  %ndbexception("NdbApiException") {
    $action
      if (result==NULL) {
        NDB_exception(NdbApiException,"NdbDictionary::Index Error");
      }
  }

  const NdbDictColumn * getColumn(unsigned no) const ;

  %ndbnoexception;

  virtual int getObjectVersion() const;

  Type getType() const;
  virtual NdbDictObject::Status getObjectStatus() const;

  bool getLogging() const;
  unsigned getNoOfColumns() const;
  NdbDictIndex(const char * name = "");
  virtual ~NdbDictIndex();

  void setName(const char * name);
  void setTable(const char * name);
  void addColumn(const NdbDictColumn & c);
  void addColumnName(const char * name);
  void addColumnNames(unsigned noOfNames, const char ** names);
  void setType(Type type);
  void setLogging(bool enable);
};

class NdbDictDictionary {
public:
#if 0
// Protect swig from this nested class until we redefine it
  /**
   * @class List
   * @brief Structure for retrieving lists of object names
   */
  struct List {
    /**
     * @struct  Element
     * @brief   NdbDictObject to be stored in an NdbDictDictList
     */
    struct Element {
      unsigned id;            ///< Id of object
      NdbDictObject::Type type;      ///< Type of object
      NdbDictObject::State state;    ///< State of object
      NdbDictObject::Store store;    ///< How object is stored
      char * database;        ///< In what database the object resides
      char * schema;          ///< What schema the object is defined in
      char * name;            ///< Name of object
      Element() :
        id(0),
        type(NdbDictObject::TypeUndefined),
        state(NdbDictObject::StateUndefined),
        store(NdbDictObject::StoreUndefined),
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

  int listObjects(List & list,
                  NdbDictObject::Type type = NdbDictObject::TypeUndefined);
  int listObjects(List & list,
                  NdbDictObject::Type type = NdbDictObject::TypeUndefined) const;
  int listIndexes(List & list, const char * tableName);
  int listIndexes(List & list, const char * tableName) const;

#endif


  %ndbexception("NdbApiException") {
    $action
      if (result==NULL) {
        NdbError err = arg1->getNdbError();
        NDB_exception(NdbApiException,err.message);
      }
  }

  const NdbDictTable * getTable(const char * name) const;
  const NdbDictIndex * getIndex(const char * indexName,
                                const char * tableName) const;
  const NdbDictEvent * getEvent(const char * eventName);

  %ndbexception("NdbApiException") {
    $action
      if (result==-1) {
        NdbError err = arg1->getNdbError();
        NDB_exception(NdbApiException,err.message);
      }
  }

  int createEvent(const NdbDictEvent &event);
  int dropEvent(const char * eventName);
  int createTable(const NdbDictTable &table);
  int dropTable(NdbDictTable & table);
  int dropTable(const char * name);
  int createIndex(const NdbDictIndex &index);
  int dropIndex(const char * indexName,
                const char * tableName);
#if defined(MYSQL_50)
  int dropIndex(const NdbDictIndex &);
#endif

  %ndbnoexception;

  const struct NdbError & getNdbError() const;

  void invalidateTable(const char * name);
  void removeCachedTable(const char * table);
  void removeCachedIndex(const char * index, const char * table);
  void invalidateIndex(const char * indexName,
                       const char * tableName);
private:
  NdbDictDictionary();
  ~NdbDictDictionary();
};


class NdbDictEvent : public NdbDictObject {

public:
  enum TableEvent {
    TE_INSERT      =1<<0, ///< Insert event on table
    TE_DELETE      =1<<1, ///< Delete event on table
    TE_UPDATE      =1<<2, ///< Update event on table
    TE_DROP        =1<<4, ///< Drop of table
    TE_ALTER       =1<<5, ///< Alter of table
    TE_CREATE      =1<<6, ///< Create of table
    TE_GCP_COMPLETE=1<<7, ///< GCP is complete
    TE_CLUSTER_FAILURE=1<<8, ///< Cluster is unavailable
    TE_STOP        =1<<9, ///< Stop of event operation
    TE_NODE_FAILURE=1<<10, ///< Node failed
    TE_SUBSCRIBE   =1<<11, ///< Node subscribes
    TE_UNSUBSCRIBE =1<<12, ///< Node unsubscribes
    TE_ALL=0xFFFF         ///< Any/all event on table (not relevant when
    ///< events are received)
  };

  enum EventDurability {
    ED_UNDEFINED = 0,
    ED_PERMANENT = 3,
  };

  /**
   * Specifies reporting options for table events
   */
  enum EventReport {
    ER_UPDATED = 0,
    ER_ALL = 1, // except not-updated blob inlines
    ER_SUBSCRIBE = 2
  };

  /**
   *  Constructor
   *  @param  name  Name of event
   */
  NdbDictEvent(const char *name);
  /**
   *  Constructor
   *  @param  name  Name of event
   *  @param  table Reference retrieved from NdbDictionary
   */
  NdbDictEvent(const char *name, const NdbDictTable& table);
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
  const NdbDictTable * getTable() const;
  /**
   * Define table on which events should be detected
   *
   * @note calling this method will default to detection
   *       of events on all columns. Calling subsequent
   *       addEventColumn calls will override this.
   *
   * @param table reference retrieved from NdbDictionary
   */
  void setTable(const NdbDictTable& table);
  /**
   * Set table for which events should be detected
   *
   * @note preferred way is using setTable(const NdbDictionary::Table&)
   *       or constructor with table object parameter
   */
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
  void addEventColumns(unsigned noOfNames, const char ** names);

  /**
   * Get no of columns defined in an Event
   *
   * @return Number of columns, -1 on error
   */
  int getNoOfEventColumns() const;

  /**
   * Get a specific column in the event
   */
  const NdbDictColumn * getEventColumn(unsigned no) const;

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
  virtual NdbDictObject::Status getObjectStatus() const;

  /**
   * Get object version
   */
  virtual int getObjectVersion() const;

  /**
   * Get object id
   */
  virtual int getObjectId() const;
private:
  Event(NdbEventImpl&);
};
