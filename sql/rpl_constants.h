/* Copyright (c) 2007 MySQL AB, 2008 Sun Microsystems, Inc.
   Use is subject to license terms.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef RPL_CONSTANTS_H
#define RPL_CONSTANTS_H

/**
   Enumeration of the incidents that can occur for the server.
 */
enum Incident {
  /** No incident */
  INCIDENT_NONE = 0,

  /** There are possibly lost events in the replication stream */
  INCIDENT_LOST_EVENTS = 1,

  /** Shall be last event of the enumeration */
  INCIDENT_COUNT
};

#endif /* RPL_CONSTANTS_H */
