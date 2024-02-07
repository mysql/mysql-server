/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

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

#include <RefConvert.hpp>
#include <signaldata/FireTrigOrd.hpp>

static const char *trigEvent(Uint32 i) {
  switch (i) {
    case TriggerEvent::TE_INSERT:
      return "insert";
      break;
    case TriggerEvent::TE_UPDATE:
      return "update";
      break;
    case TriggerEvent::TE_DELETE:
      return "delete";
      break;
  }
  return "UNKNOWN";
}

bool printFIRE_TRIG_ORD(FILE *output, const Uint32 *theData, Uint32 len,
                        Uint16 /*receiverBlockNo*/) {
  const FireTrigOrd *const sig = (const FireTrigOrd *)theData;

  fprintf(output, " TriggerId: %d TriggerEvent: %s\n", sig->getTriggerId(),
          trigEvent(sig->getTriggerEvent()));
  fprintf(output, " UserRef: (%d, %d, %d) User data: %x\n",
          refToNode(sig->getUserRef()), refToInstance(sig->getUserRef()),
          refToMain(sig->getUserRef()), sig->getConnectionPtr());
  fprintf(output, " Signal: PK=%d BEFORE=%d AFTER=%d\n",
          sig->getNoOfPrimaryKeyWords(), sig->getNoOfBeforeValueWords(),
          sig->getNoOfAfterValueWords());
  fprintf(output, " fragId: %u ", sig->fragId);

  /* Variants, see DbtupTrigger.cpp */
  if (len == FireTrigOrd::SignalWithGCILength) {
    fprintf(output, "gci_hi: %u\n", sig->m_gci_hi);
  } else if (len == FireTrigOrd::SignalLength) {
    fprintf(output, " Triggertype: %s\n",
            TriggerInfo::triggerTypeName(sig->m_triggerType));
    fprintf(output, " transId: (H\'%.8x, H\'%.8x)\n", sig->m_transId1,
            sig->m_transId2);
  } else if (len == FireTrigOrd::SignalLengthSuma) {
    fprintf(output, " transId: (H\'%.8x, H\'%.8x)\n", sig->m_transId1,
            sig->m_transId2);
    fprintf(output, " gci: %u/%u Hash: %u Any: %u\n", sig->m_gci_hi,
            sig->m_gci_lo, sig->m_hashValue, sig->m_any_value);
  } else {
    fprintf(output, " Unexpected length\n");
    if (len > 8) {
      fprintf(output, " -- Variable data -- \n");

      Uint32 remain = len - 8;
      const Uint32 *data = &theData[8];
      while (remain >= 7) {
        fprintf(output,
                " H\'%.8x H\'%.8x H\'%.8x H\'%.8x H\'%.8x H\'%.8x H\'%.8x\n",
                data[0], data[1], data[2], data[3], data[4], data[5], data[6]);
        remain -= 7;
        data += 7;
      }
      if (remain > 0) {
        for (Uint32 i = 0; i < remain; i++) {
          fprintf(output, " H\'%.8x", data[i]);
        }
        fprintf(output, "\n");
      }
    }
  }

  return true;
}
