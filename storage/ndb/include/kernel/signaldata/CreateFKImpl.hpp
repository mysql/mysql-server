/*
   Copyright (c) 2011, 2012, Oracle and/or its affiliates. All rights reserved.

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


#ifndef CREATE_FK_IMPL_HPP
#define CREATE_FK_IMPL_HPP

#include "SignalData.hpp"

#define JAM_FILE_ID 208


struct CreateFKImplReq
{
  /**
   * Sender(s) / Reciver(s)
   */
  friend class Dbdict;

  /**
   * For printing
   */
  friend bool printCREATE_FK_IMPL_REQ(FILE*, const Uint32*, Uint32, Uint16);

  STATIC_CONST( SignalLength = 10 );
  STATIC_CONST( PARENT_COLUMNS = 0); // section no
  STATIC_CONST( CHILD_COLUMNS = 1); // section no

  enum {
    RT_PARSE    = 0x1,
    RT_PREPARE  = 0x2,
    RT_ABORT    = 0x3,
    RT_COMMIT   = 0x4,
    RT_COMPLETE = 0x5
  };

  enum Bits
  {
    FK_PARENT_UI          =   1,  // Parent index is an unique index
    FK_PARENT_OI          =   2,  // Parent index is an ordered index
    FK_CHILD_UI           =   4,  // Child index is an unique index
    FK_CHILD_OI           =   8,  // Child index is an ordered index
    FK_UPDATE_RESTRICT    =  16,  // On update restrict
    FK_UPDATE_CASCADE     =  32,  // On update cascade
    FK_UPDATE_SET_NULL    =  64,  // On update set null
    FK_UPDATE_SET_DEFAULT =  128, // On update set default
    FK_DELETE_RESTRICT    =  256, // On delete restrict
    FK_DELETE_CASCADE     =  512, // On delete cascade
    FK_DELETE_SET_NULL    =  1024,// On delete set null
    FK_DELETE_SET_DEFAULT =  2048,// On delete set default

    FK_UPDATE_MASK = (FK_UPDATE_RESTRICT |
                      FK_UPDATE_CASCADE  |
                      FK_UPDATE_SET_NULL |
                      FK_UPDATE_SET_DEFAULT),

    FK_DELETE_MASK = (FK_DELETE_RESTRICT |
                      FK_DELETE_CASCADE  |
                      FK_DELETE_SET_NULL |
                      FK_DELETE_SET_DEFAULT),

    FK_ACTION_MASK = (FK_UPDATE_MASK | FK_DELETE_MASK),

    FK_UPDATE_ACTION = (FK_UPDATE_CASCADE  |
                        FK_UPDATE_SET_NULL |
                        FK_UPDATE_SET_DEFAULT),

    FK_DELETE_ACTION = (FK_DELETE_CASCADE  |
                        FK_DELETE_SET_NULL |
                        FK_DELETE_SET_DEFAULT),

    FK_ON_ACTION = (FK_UPDATE_ACTION | FK_DELETE_ACTION),

    FK_END = 0 // marker
  };

  Uint32 senderData;
  Uint32 senderRef;
  Uint32 requestType;
  Uint32 fkId;
  Uint32 fkVersion;
  Uint32 bits;
  Uint32 parentTableId;
  Uint32 parentIndexId;
  Uint32 childTableId;
  Uint32 childIndexId;
};

struct CreateFKImplRef
{
  /**
   * Sender(s)
   */
  friend class Dbdict;

  /**
   * For printing
   */
  friend bool printCREATE_FK_IMPL_REF(FILE*, const Uint32*, Uint32, Uint16);

  STATIC_CONST( SignalLength = 3 );

  Uint32 senderData;
  Uint32 senderRef;
  Uint32 errorCode;

  enum ErrCode
  {
    ObjectAlreadyExist = 21030,
    NoMoreObjectRecords = 21031,
    InvalidFormat = 21032
  };
};

struct CreateFKImplConf
{
  /**
   * Sender(s)
   */
  friend class Dbdict;

  /**
   * For printing
   */
  friend bool printCREATE_FK_IMPL_CONF(FILE*, const Uint32*, Uint32, Uint16);

  STATIC_CONST( SignalLength = 4 );

  Uint32 senderData;
  Uint32 senderRef;
};


#undef JAM_FILE_ID

#endif
