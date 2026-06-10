/* Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef OPT_TRACKER_INCLUDED
#define OPT_TRACKER_INCLUDED

/**
  Tracks the Group Replication feature as available,
  is installed but not running.
*/
void track_group_replication_available();

/**
  Tracks the Group Replication feature as unavailable,
  is not installed.
*/
void track_group_replication_unavailable();

/**
  Tracks the Group Replication feature, including the usage data.
  It only updates usage data if the feature is enabled.

  @param enabled  true:  tracks as enabled
                  false: tracks as disabled
*/
void track_group_replication_enabled(bool enabled);

/**
@brief A status variable counter for usage tracking by the option_tracker
component
*/
extern unsigned long long opt_option_tracker_usage_group_replication_plugin;

#endif /* OPT_TRACKER_INCLUDED */
