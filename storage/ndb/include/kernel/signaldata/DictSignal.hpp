/* Copyright (c) 2007, 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#ifndef DICT_SIGNAL_HPP
#define DICT_SIGNAL_HPP

#include <Bitmask.hpp>

#define JAM_FILE_ID 187


struct DictSignal
{
  // DICT transaction and operation REQs include Uint32 requestInfo
  // implementation signals have only requestType
  // requestInfo format should be as follows

  // byte 0: requestType (usually enum)

  static Uint32
  getRequestType(const Uint32& info) {
    return BitmaskImpl::getField(1, &info, 0, 8);
  }

  static void
  setRequestType(Uint32& info, Uint32 val) {
    assert(val < (1 << 8));
    BitmaskImpl::setField(1, &info, 0, 8, val);
  }

  // byte 1: extra case-dependent usage within DICT

  static Uint32
  getRequestExtra(const Uint32& info) {
    return BitmaskImpl::getField(1, &info, 8, 8);
  }

  static void
  setRequestExtra(Uint32& info, Uint32 val) {
    assert(val < (1 << 8));
    BitmaskImpl::setField(1, &info, 8, 8, val);
  }

  static void
  addRequestExtra(Uint32& dst_info, const Uint32& src_info) {
    Uint32 val = getRequestExtra(src_info);
    setRequestExtra(dst_info, val);
  }

  // byte 2: global flags: passed everywhere
  // byte 3: local flags: consumed by current op

private:

  // flag bits are defined relative to entire requestInfo word
  enum { RequestFlagsMask = 0xffff0000 };
  enum { RequestFlagsGlobalMask = 0x00ff0000 };

public:

  enum RequestFlags {
    // global

    /*
     * This node is transaction coordinator and the only participant.
     * Used by node doing NR to activate each index.
     */
    RF_LOCAL_TRANS = (1 << 16),

    /*
     * Activate index but do not build it.  On SR, the build is done
     * in a later start phase (for non-logged index).  On NR, the build
     * on this node takes place automatically during data copy.
     */
    RF_NO_BUILD = (1 << 17)

  };

  static void
  addRequestFlags(Uint32& dst_info, const Uint32& src_info) {
    dst_info |= src_info & RequestFlagsMask;
  }

  static void
  addRequestFlagsGlobal(Uint32& dst_info, const Uint32& src_info) {
    dst_info |= src_info & RequestFlagsGlobalMask;
  }

  static const char*
  getRequestFlagsText(const Uint32& info) {
    static char buf[100];
    buf[0] = buf[1] = 0;
    if (info & RF_LOCAL_TRANS)
      strcat(buf, " LOCAL_TRANS");
    if (info & RF_NO_BUILD)
      strcat(buf, " NO_BUILD");
    return &buf[1];
  }

  static const char*
  getRequestInfoText(const Uint32& info) {
    static char buf[100];
    sprintf(buf, "type: %u extra: %u flags: %s",
        getRequestType(info), (info >> 8) & 0xff, getRequestFlagsText(info));
    return buf;
  }

  // these match Dbdict.hpp

  static const char*
  getTransModeName(Uint32 val) {
    static const char* name[] = {
      "Undef", "Normal", "Rollback", "Abort"
    };
    Uint32 size = sizeof(name)/sizeof(name[0]);
    return val < size ? name[val] : "?";
  }

  static const char*
  getTransPhaseName(Uint32 val) {
    static const char* name[] = {
      "Undef", "Begin", "Parse", "Prepare", "Commit", "Complete", "End"
    };
    Uint32 size = sizeof(name)/sizeof(name[0]);
    return val < size ? name[val] : "?";
  }

  static const char*
  getTransStateName(Uint32 val) {
    static const char* name[] = {
      "Undef", "Ok", "Error", "NodeFail", "NeedTrans", "NoTrans", "NeedOp", "NoOp"
    };
    Uint32 size = sizeof(name)/sizeof(name[0]);
    return val < size ? name[val] : "?";
  }
};


#undef JAM_FILE_ID

#endif
