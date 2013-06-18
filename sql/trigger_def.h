/*
   Copyright (c) 2013, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef TRIGGER_DEF_H_INCLUDED
#define TRIGGER_DEF_H_INCLUDED

///////////////////////////////////////////////////////////////////////////

/**
  @file

  @brief
  This file defines all base public constants related to triggers in MySQL.
*/

///////////////////////////////////////////////////////////////////////////

/**
  Constants to enumerate possible event types on which triggers can be fired.
*/
enum enum_trigger_event_type
{
  TRG_EVENT_INSERT= 0,
  TRG_EVENT_UPDATE= 1,
  TRG_EVENT_DELETE= 2,
  TRG_EVENT_MAX
};

/**
  Constants to enumerate possible timings when triggers can be fired.
*/
enum enum_trigger_action_time_type
{
  TRG_ACTION_BEFORE= 0,
  TRG_ACTION_AFTER= 1,
  TRG_ACTION_MAX
};

/**
  Possible trigger ordering clause values:
    - TRG_ORDER_NONE     -- trigger ordering clause is not specified
    - TRG_ORDER_FOLLOWS  -- FOLLOWS clause
    - TRG_ORDER_PRECEDES -- PRECEDES clause
*/
enum enum_trigger_order_type
{
  TRG_ORDER_NONE= 0,
  TRG_ORDER_FOLLOWS= 1,
  TRG_ORDER_PRECEDES= 2
};

/**
  Enum constants to designate NEW and OLD trigger pseudo-variables.
*/
enum enum_trigger_variable_type
{
  TRG_OLD_ROW,
  TRG_NEW_ROW
};

///////////////////////////////////////////////////////////////////////////

/*
  The following constants are defined in trigger_loader.cc.
  They would be private to Trigger_loader if we didn't have handler.
*/

extern const char * const TRG_EXT;
extern const char * const TRN_EXT;

///////////////////////////////////////////////////////////////////////////

#endif // TRIGGER_DEF_H_INCLUDED
