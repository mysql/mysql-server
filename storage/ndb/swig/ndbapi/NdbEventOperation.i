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

class NdbEventOperation {

private:

  NdbEventOperation(Ndb *theNdb, const char* eventName);
  ~NdbEventOperation();

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


  %ndbnoexception;

  /**
   * Get the latest error
   *
   * @return   Error object.
   */
  const struct NdbError & getNdbError() const;

  bool isConsistent() const;

  /**
   * Check if table name has changed, for event TE_ALTER
   */
  const bool tableNameChanged() const;

  /**
   * Check if table frm has changed, for event TE_ALTER
   */
  const bool tableFrmChanged() const;

  /**
   * Check if table fragmentation has changed, for event TE_ALTER
   */
  const bool tableFragmentationChanged() const;

  /**
   * Check if table range partition list name has changed, for event TE_ALTER
   */
  const bool tableRangeListChanged() const;

  /**
   * Retrieve the GCI of the latest retrieved event
   *
   * @return GCI number
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
   * Retrieve current state of the NdbEventOperation object
   */
  State getState();


  /**
   * See NdbDictionary::Event.  Default is false.
   */
  void mergeEvents(bool flag);

  /**
   * Query for occured event type.
   *
   * @note Only valid after Ndb::nextEvent() has been called and
   * returned a not NULL value
   *
   * @return type of event
   */
  NdbDictEvent::TableEvent getEventType() const;


  /* methods that return an object */
  %ndbexception("NdbApiException") {
    $action
      if (result == NULL) {
        NdbError err = arg1->getNdbError();
        NDB_exception(NdbApiException,err.message);
      }
  }

  /**
   * See getValue().
   */
  NdbBlob* getBlobHandle(const char *anAttrName);
  NdbBlob* getPreBlobHandle(const char *anAttrName);



  /* methods that return an object */
  %ndbexception("NdbApiException") {
    $action
      if (result == -1) {
        NdbError err = arg1->getNdbError();
        NDB_exception(NdbApiException,err.message);
      }
  }

  /**
   * Activates the NdbEventOperation to start receiving events. The
   * changed attribute values may be retrieved after Ndb::nextEvent()
   * has returned not NULL. The getValue() methods must be called
   * prior to execute().
   *
   * @return 0 if successful otherwise -1.
   */
  int execute();

  int isOverrun() const;

  %ndbnoexception;

};

typedef void (* NdbEventCallback)(NdbEventOperation*, Ndb*, void*);

%extend NdbEventOperation {

  %ndbexception("NdbApiException") {
    $action
      if (result==NULL) {
        NdbError err = arg1->getNdbError();
        NDB_exception(NdbApiException,err.message);
      }
  }

  NdbRecAttr* getValue(const char* anAttrName) {
    return self->getValue(anAttrName,NULL);
  }


  NdbRecAttr* getPreValue(const char *anAttrName) {
    return self->getPreValue(anAttrName, NULL);
  }

  %ndbnoexception;

};
