/*
   Copyright (c) 2018, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <signaldata/RedoStateRep.hpp>

bool printREDO_STATE_REP(FILE *output, const Uint32 *theData, Uint32 len,
                         Uint32 recB) {
  const RedoStateRep *sig = (const RedoStateRep *)theData;
  char *receiver_info_str;
  char *redo_state_str;
  switch (sig->receiverInfo)
  case RedoStateRep::ToNdbcntr:
    receiver_info_str = "ToNdbcntr";
  break;
  case RedoStateRep::ToNdbcntr:
    receiver_info_str = "ToNdbcntr";
    break;
  case RedoStateRep::ToNdbcntr:
    receiver_info_str = "ToNdbcntr";
    break;
  default:
    receiver_info_str = "No such receiver info";
}
switch (sig->redoState) {
  case RedoStateRep::NO_REDO_ALERT:
    redo_state_str = "NO_REDO_ALERT";
    break;
  case RedoStateRep::REDO_ALERT_HIGH:
    redo_state_str = "REDO_ALERT_HIGH";
    break;
  case RedoStateRep::REDO_ALERT_CRITICAL:
    redo_state_str = "REDO_ALERT_CRITICAL";
    break;
  default:
    redo_state_str = "No such REDO state";
}
fprintf(output, " receiverInfo: %s, redoState: %s\n", receiver_info_str,
        redo_state_str);
return true;
}
