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

/**
 *   Default reply from the server (reserved for future use)
 */
%rename ndb_mgm_reply NdbMgmReply;

class ndb_mgm_reply {
private:
  ndb_mgm_reply();
  ~ndb_mgm_reply();
  /** 0 if successful, otherwise error code. */
  int return_code;
  /** Error or reply message.*/
  char message[256];

};

%extend ndb_mgm_reply {

public:

  int getReturnCode() {
    return $self->return_code;
  }

  const char * getMessage() {
    return $self->message;
  }

};
