/*
   Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

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

#include "API.hpp"
#include "ndb_internal.hpp"

int
Ndb_internal::send_event_report(bool has_lock, 
                                Ndb *ndb, Uint32 *data, Uint32 length)
{
  return ndb->theImpl->send_event_report(has_lock, data, length);
}

void
Ndb_internal::setForceShortRequests(Ndb* ndb, bool val)
{
  ndb->theImpl->forceShortRequests = val;
}
                                    
