/*
   Copyright (c) 2011, 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NDB_STATE_DESC_H
#define NDB_STATE_DESC_H

#define JAM_FILE_ID 216


struct ndbkernel_state_desc
{
  unsigned value;
  const char * name;
  const char * friendly_name;
  const char * description;
};

extern struct ndbkernel_state_desc g_dbtc_apiconnect_state_desc[];
extern struct ndbkernel_state_desc g_dblqh_tcconnect_state_desc[];


#undef JAM_FILE_ID

#endif
