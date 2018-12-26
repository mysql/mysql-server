/*
   Copyright (c) 2018, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include <signaldata/RedoStateRep.hpp>

bool
printREDO_STATE_REP(FILE *output,
                    const Uint32 *theData,
                    Uint32 len,
                    Uint32 recB)
{
  RedoStateRep* sig = (RedoStateRep*)theData;
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
  switch (sig->redoState)
  {
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
  fprintf(output, " receiverInfo: %s, redoState: %s\n",
          receiver_info_str, redo_state_str);
  return true;
}
