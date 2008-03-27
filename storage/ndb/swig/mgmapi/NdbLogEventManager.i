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

%{
#include "NdbEventListener.hpp"
  %}


%feature("director") NdbLogEventTypeListener;
%feature("director") NdbLogEventCategoryListener;

class NdbLogEventListener {
public:
  virtual ~NdbLogEventListener();
//  virtual void le_handleEvent(const ndb_logevent & theEvent);
};
class NdbLogEventCategoryListener {
public:
  virtual ~NdbLogEventCategoryListener();
//  virtual void le_handleEvent(const ndb_logevent & theEvent);
  virtual ndb_mgm_event_category getEventCategory();
};
class NdbLogEventTypeListener {
public:
  virtual ~NdbLogEventTypeListener();
//  virtual void le_handleEvent(const ndb_logevent & theEvent);
  virtual Ndb_logevent_type getEventType();
};

%typemap(newfree) (BaseEventWrapper *) "free($1);";
%newobject NdbLogEventManager::getLogEvent;

class NdbLogEventManager {
  NdbLogEventManager();
  ndb_logevent_handle * handle;
  NdbLogEventManager(ndb_logevent_handle * theHandle);
public:
  // Returns -1 on error, 0 otherwise
  ~NdbLogEventManager();
  bool unregisterListener(NdbLogEventTypeListener * listener);
  bool unregisterListener(NdbLogEventCategoryListener * listener);

  %ndbexception("NdbMgmException") {
    $action
      if (result < 0) {
        NDB_exception(NdbMgmException,"Must deregister handler before adding a new one");
      }
  }
  int registerListener(NdbLogEventTypeListener * listener);
  int registerListener(NdbLogEventCategoryListener * listener);


  %ndbexception("NdbMgmException") {
    $action
      if (result->ret < 0) {
        NDB_exception(NdbMgmException,arg1->getMgmError());
      }
  }
  BaseEventWrapper * getLogEvent(unsigned timeout_in_milliseconds);

  %ndbexception("NdbMgmException") {
    $action
      if (result < 0) {
        NDB_exception(NdbMgmException,arg1->getMgmError());
      }
  }
  int pollEvents(unsigned timeout_in_milliseconds);
  %ndbnoexception;
};


