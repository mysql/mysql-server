/*
   Copyright (c) 2016, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_PROCESSINFO_REP_H
#define NDB_PROCESSINFO_REP_H

#include "SignalData.hpp"

class ProcessInfoRep {
  friend class ClusterMgr;     // Sender
  friend class Qmgr;           // Receiver
  friend class ProcessInfo;    // Stored format
  friend bool printPROCESSINFO_REP(FILE *, const Uint32 *, Uint32, Uint16);

public:
  static constexpr Uint32 SignalLength = 20;
  static constexpr Uint32 PathSectionNum = 0;
  static constexpr Uint32 HostSectionNum = 1;

private:
  Uint8 process_name[48];
  Uint8 uri_scheme[16];
  Uint32 node_id;
  Uint32 process_id;
  Uint32 angel_process_id;
  Uint32 application_port;
};

// path and host sections of service URI are sent as separate sections

#endif
