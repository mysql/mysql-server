/*
   Copyright (c) 2004, 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef READ_CONFIG_HPP
#define READ_CONFIG_HPP

#define JAM_FILE_ID 75


/**
 */
class ReadConfigReq {
public:
  STATIC_CONST( SignalLength = 3 );
  
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 noOfParameters; // 0 Means read all relevant for block
  Uint32 parameters[1];  // see mgmapi_config_parameters.h
};

class ReadConfigConf {
public:
  STATIC_CONST( SignalLength = 2 );

  Uint32 senderRef;
  Uint32 senderData;
};


#undef JAM_FILE_ID

#endif
