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

/** Log event specific data for for corresponding NDB_LE_ log event */

/**
 * The NdbLogevent
 */
%rename ndb_logevent NdbLogEvent;

class ndb_logevent {

  /** NdbLogEventHandle (to be used for comparing only)
   *  set in ndb_logevent_get_next()
   */
  void *handle;

  /** Which event */
  enum Ndb_logevent_type type;

  /** Time when log event was registred at the management server */
  unsigned time;

  /** Category of log event */
  enum ndb_mgm_event_category category;

  /** Severity of log event */
  enum ndb_mgm_event_severity severity;

  /** Level (0-15) of log event */
  unsigned level;

  /** Node ID of the node that reported the log event */
  unsigned source_nodeid;

};

%extend ndb_logevent {

public:

  Ndb_logevent_type getEventType() {
    return $self->type;
  }

  ndb_mgm_event_category getEventCategory() {
    return $self->category;
  }

  ndb_mgm_event_severity getEventSeverity() {
    return $self->severity;
  }

  Uint32 getSourceNodeId() {
    return $self->source_nodeid;
  }

  Uint32 getEventTime() {
    return $self->time;
  }

  Uint32 getEventLevel() {
    return $self->level;
  }

};



