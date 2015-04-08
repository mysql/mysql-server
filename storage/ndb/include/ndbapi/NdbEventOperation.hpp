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

#ifndef NdbEventOperation_H
#define NdbEventOperation_H

class NdbGlobalEventBuffer;
class NdbEventOperationImpl;

/**
 * @class NdbEventOperation
 * @brief Class of operations for getting change events from database.  
 *
 * Brief description on how to work with events:
 *
 * - An event, represented by an NdbDictionary::Event, i created in the 
 *   Database through
 *   NdbDictionary::Dictionary::createEvent() (note that this can be done 
 *   by any application or thread and not necessarily by the "listener")
 * - To listen to events, an NdbEventOperation object is instantiated by 
 *   Ndb::createEventOperation()
 * - execute() starts the event flow. Use Ndb::pollEvents() to wait
 *   for an event to occur.  Use Ndb::nextEvent() to iterate
 *   through the events that have occured.
 * - The instance is removed by Ndb::dropEventOperation()
 *
 * For more info see:
 * @ref ndbapi_event.cpp
 *
 * Known limitations:
 *
 * - Maximum number of active NdbEventOperations are now set at compile time.
 * Today 100.  This will become a configuration parameter later.
 * - Maximum number of NdbEventOperations tied to same event are maximum 16
 * per process.
 *
 * Known issues:
 *
 * - When several NdbEventOperation's are tied to the same event in the same
 * process they will share the circular buffer. The BufferLength will then
 * be the same for all and decided by the first NdbEventOperation 
 * instantiation. Just make sure to instantiate the "largest" one first.
 * - Today all events INSERT/DELETE/UPDATE and all changed attributes are
 * sent to the API, even if only specific attributes have been specified.
 * These are however hidden from the user and only relevant data is shown
 * after  Ndb::nextEvent().
 * - "False" exits from Ndb::pollEvents() may occur and thus
 * the subsequent Ndb::nextEvent() will return NULL,
 * since there was no available data. Just do Ndb::pollEvents() again.
 * - Event code does not check table schema version. Make sure to drop events
 * after table is dropped. Will be fixed in later
 * versions.
 * - If a node failure has occured not all events will be received
 * anymore. Drop NdbEventOperation and Create again after nodes are up
 * again. Will be fixed in later versions.
 *
 * Test status:
 *
 * - Tests have been run on 1-node and 2-node systems
 *
 * Useful API programs:
 *
 * - ndb_select_all -d sys 'NDB$EVENTS_0'
 * shows contents in the system table containing created events.
 *
 * @note this is an inteface to viewing events that is subject to change
 */
class NdbEventOperation {
public:
  /**
   * State of the NdbEventOperation object
   */
  enum State {
    EO_CREATED,   ///< Created but execute() not called
    EO_EXECUTING, ///< execute() called
    EO_DROPPED,   ///< Waiting to be deleted, Object unusable.
    EO_ERROR      ///< An error has occurred. Object unusable.
  };
  /**
   * Retrieve current state of the NdbEventOperation object
   */
  State getState();
  /**
   * See NdbDictionary::Event.  Default is false.
   */
  void mergeEvents(bool flag);

  /**
   * Activates the NdbEventOperation to start receiving events. The
   * changed attribute values may be retrieved after Ndb::nextEvent() 
   * has returned not NULL. The getValue() methods must be called
   * prior to execute().
   *
   * @return 0 if successful otherwise -1.
   */
  int execute();

  /**
   * Defines a retrieval operation of an attribute value.
   * The NDB API allocate memory for the NdbRecAttr object that
   * will hold the returned attribute value. 
   *
   * @note Note that it is the applications responsibility
   *       to allocate enough memory for aValue (if non-NULL).
   *       The buffer aValue supplied by the application must be
   *       aligned appropriately.  The buffer is used directly
   *       (avoiding a copy penalty) only if it is aligned on a
   *       4-byte boundary and the attribute size in bytes
   *       (i.e. NdbRecAttr::attrSize() times NdbRecAttr::arraySize() is
   *       a multiple of 4).
   *
   * @note There are two versions, getValue() and
   *       getPreValue() for retrieving the current and
   *       previous value repectively.
   *
   * @note This method does not fetch the attribute value from 
   *       the database!  The NdbRecAttr object returned by this method 
   *       is <em>not</em> readable/printable before the 
   *       execute() has been made and
   *       Ndb::nextEvent() has returned not NULL.
   *       If a specific attribute has not changed the corresponding 
   *       NdbRecAttr will be in state UNDEFINED.  This is checked by 
   *       NdbRecAttr::isNULL() which then returns -1.
   *
   * @param anAttrName  Attribute name 
   * @param aValue      If this is non-NULL, then the attribute value 
   *                    will be returned in this parameter.<br>
   *                    If NULL, then the attribute value will only 
   *                    be stored in the returned NdbRecAttr object.
   * @return            An NdbRecAttr object to hold the value of 
   *                    the attribute, or a NULL pointer 
   *                    (indicating error).
   */
  NdbRecAttr *getValue(const char *anAttrName, char *aValue = 0);
  /**
   * See getValue().
   */
  NdbRecAttr *getPreValue(const char *anAttrName, char *aValue = 0);

  /**
   * These methods replace getValue/getPreValue for blobs.  Each
   * method creates a blob handle NdbBlob.  The handle supports only
   * read operations.  See NdbBlob.
   */
  NdbBlob* getBlobHandle(const char *anAttrName);
  NdbBlob* getPreBlobHandle(const char *anAttrName);

  int isOverrun() const;

  /**
   * In the current implementation a nodefailiure may cause loss of events,
   * in which case isConsistent() will return false
   */
  bool isConsistent() const;

  /**
   * Query for occured event type.
   *
   * @note Only valid after Ndb::nextEvent2() has been called and
   * returned a non-NULL value
   *
   * @return type of event, including the exceptional event data types:
   * TE_EMPTY, TE_INCONSISTENT, TE_OUT_OF_MEMORY
   */
  NdbDictionary::Event::TableEvent getEventType2() const;

  /**
   * Query for occured event type. This is a backward compatibility
   * wrapper for getEventType2(). Since it is called after nextEvent()
   * returned a non-NULL event operation after filtering exceptional epoch
   * event data, it should not see the exceptional event data types:
   * TE_EMPTY, TE_INCONSISTENT, TE_OUT_OF_MEMORY
   *
   * @note Only valid after Ndb::nextEvent() has been called and
   * returned a non-NULL value
   *
   * @return type of event
   */
  NdbDictionary::Event::TableEvent getEventType() const;

  /**
   * Check if table name has changed, for event TE_ALTER
   */
  bool tableNameChanged() const;

  /**
   * Check if table frm has changed, for event TE_ALTER
   */
  bool tableFrmChanged() const;

  /**
   * Check if table fragmentation has changed, for event TE_ALTER
   */
  bool tableFragmentationChanged() const;

  /**
   * Check if table range partition list name has changed, for event TE_ALTER
   */
  bool tableRangeListChanged() const;

  /**
   * Retrieve the epoch of the latest retrieved event data
   *
   * @return epoch
   */
  Uint64 getEpoch() const;

  /**
   * Retrieve the GCI of the latest retrieved event
   *
   * @return GCI number
   *
   * This is a wrapper to getEpoch() for backward compatibility.
   */
  Uint64 getGCI() const;

  /**
   * Retrieve the AnyValue of the latest retrieved event
   *
   * @return AnyValue
   */
  Uint32 getAnyValue() const;

  /**
   * Retrieve the complete GCI in the cluster (not necessarily
   * associated with an event)
   *
   * @return GCI number
   */
  Uint64 getLatestGCI() const;

  /**
   * Retrieve the TransId of the latest retrieved event
   *
   * Only valid for data events.  If the kernel does not
   * support transaction ids with events, the max Uint64
   * value is returned.
   *
   * @return TransId
   */
  Uint64 getTransId() const;

  /**
   * Get the latest error
   *
   * @return   Error object.
   */			     
  const struct NdbError & getNdbError() const;

  /**
   * Set allow empty updates
   *
   * To support monitoring of pseudo columns we need to
   * explicitely allow for receiving events with no updates
   * to user defined columns.
   * Normally update events with no changes to monitored columns
   * are filtered out by NdbApi. By calling setAllowEmptyUpdate(true),
   * these are passed to the user.
   */
  void setAllowEmptyUpdate(bool allow);

  /**
   * Get allow empty updates value
   *
   * @return current value (with initial value being false)
   */
  bool getAllowEmptyUpdate();

  /**
   * Check whether the consumed event data marks an empty epoch
   */
  bool isEmptyEpoch();

  /**
   * Check whether the consumed event data marks an error epoch
   * and get the error.
   */
  bool isErrorEpoch(NdbDictionary::Event::TableEvent *error_type = 0);


#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  /** these are subject to change at any time */
  const NdbDictionary::Table* getTable() const;
  const NdbDictionary::Event *getEvent() const;
  const NdbRecAttr *getFirstPkAttr() const;
  const NdbRecAttr *getFirstPkPreAttr() const;
  const NdbRecAttr *getFirstDataAttr() const;
  const NdbRecAttr *getFirstDataPreAttr() const;

//  bool validateTable(NdbDictionary::Table &table) const;

  void setCustomData(void * data);
  void * getCustomData() const;

  void clearError();
  int hasError() const;

  int getReqNodeId() const;
  int getNdbdNodeId() const;
#endif

#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  /*
   *
   */
  void print();
#endif

private:
#ifndef DOXYGEN_SHOULD_SKIP_INTERNAL
  friend class NdbEventOperationImpl;
  friend class NdbEventBuffer;
#endif
  NdbEventOperation(Ndb *theNdb, const char* eventName);
  ~NdbEventOperation();
  class NdbEventOperationImpl &m_impl;
  NdbEventOperation(NdbEventOperationImpl& impl);

  NdbEventOperation(const NdbEventOperation&); // Not impl.
  NdbEventOperation&operator=(const NdbEventOperation&);
};

typedef void (* NdbEventCallback)(NdbEventOperation*, Ndb*, void*);
#endif
