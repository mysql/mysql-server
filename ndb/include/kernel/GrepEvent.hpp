/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef GREP_EVENT_H
#define GREP_EVENT_H

class GrepEvent {
public:
  enum Subscription  {
    GrepSS_CreateSubIdConf = 1,
    GrepSS_SubCreateConf = 2,
    GrepSS_SubStartMetaConf = 3,
    GrepSS_SubStartDataConf = 4,
    GrepSS_SubSyncDataConf = 5,
    GrepSS_SubSyncMetaConf = 6,
    GrepSS_SubRemoveConf = 7,
    
    GrepPS_CreateSubIdConf = 8,
    GrepPS_SubCreateConf = 9,
    GrepPS_SubStartMetaConf = 10,
    GrepPS_SubStartDataConf = 11,
    GrepPS_SubSyncMetaConf = 12,
    GrepPS_SubSyncDataConf = 13,
    GrepPS_SubRemoveConf = 14,

    GrepPS_CreateSubIdRef = 15,
    GrepPS_SubCreateRef = 16,
    GrepPS_SubStartMetaRef = 17,
    GrepPS_SubStartDataRef = 18,
    GrepPS_SubSyncMetaRef = 19,
    GrepPS_SubSyncDataRef = 20,
    GrepPS_SubRemoveRef = 21,

    GrepSS_CreateSubIdRef = 22,
    GrepSS_SubCreateRef = 23,
    GrepSS_SubStartMetaRef = 24,
    GrepSS_SubStartDataRef = 25,
    GrepSS_SubSyncMetaRef = 26,
    GrepSS_SubSyncDataRef = 27,
    GrepSS_SubRemoveRef = 28,

    Rep_Disconnect = 29

  };
};
#endif
